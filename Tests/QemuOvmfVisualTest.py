#!/usr/bin/env python3

"""Capture an OVMF framebuffer and verify the APEX32 gateway palette."""

from __future__ import annotations

import argparse
import json
import socket
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class ScreenMetrics:
    width: int
    height: int
    dark_pixels: int
    cyan_pixels: int
    red_pixels: int
    center_accent_pixels: int

    @property
    def total_pixels(self) -> int:
        return self.width * self.height

    @property
    def dark_ratio(self) -> float:
        return self.dark_pixels / self.total_pixels


def parse_ppm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    offset = 0

    def next_token() -> bytes:
        nonlocal offset
        while offset < len(data):
            if data[offset] == ord("#"):
                newline = data.find(b"\n", offset)
                if newline < 0:
                    raise ValueError("unterminated PPM comment")
                offset = newline + 1
            elif chr(data[offset]).isspace():
                offset += 1
            else:
                break

        start = offset
        while offset < len(data) and not chr(data[offset]).isspace():
            offset += 1
        if start == offset:
            raise ValueError("truncated PPM header")
        return data[start:offset]

    if next_token() != b"P6":
        raise ValueError("QEMU screenshot is not a binary PPM")
    width = int(next_token())
    height = int(next_token())
    maximum = int(next_token())
    if width <= 0 or height <= 0 or maximum != 255:
        raise ValueError("unsupported PPM dimensions or channel depth")

    if offset >= len(data) or not chr(data[offset]).isspace():
        raise ValueError("missing PPM pixel-data separator")
    if data[offset : offset + 2] == b"\r\n":
        offset += 2
    else:
        offset += 1

    expected = width * height * 3
    pixels = data[offset : offset + expected]
    if len(pixels) != expected:
        raise ValueError("truncated PPM pixel data")
    return width, height, pixels


def analyze_screen(width: int, height: int, pixels: bytes) -> ScreenMetrics:
    dark = 0
    cyan = 0
    red = 0
    center_accent = 0
    left = width // 5
    right = width - left
    top = height // 10
    bottom = height - top
    for pixel_index, offset in enumerate(range(0, len(pixels), 3)):
        r, g, b = pixels[offset : offset + 3]
        if max(r, g, b) <= 56:
            dark += 1
        is_cyan = g >= 110 and b >= 125 and g > r * 1.30 and b > r * 1.35
        is_red = r >= 125 and r > g * 1.45 and r > b * 1.25
        if is_cyan:
            cyan += 1
        if is_red:
            red += 1
        x = pixel_index % width
        y = pixel_index // width
        if left <= x < right and top <= y < bottom and (is_cyan or is_red):
            center_accent += 1
    return ScreenMetrics(width, height, dark, cyan, red, center_accent)


def looks_like_apex32(metrics: ScreenMetrics) -> bool:
    total = metrics.total_pixels
    return (
        metrics.width >= 640
        and metrics.height >= 480
        and metrics.dark_ratio >= 0.40
        and metrics.cyan_pixels >= max(300, total // 1200)
        and metrics.red_pixels >= 60
        and metrics.center_accent_pixels >= max(120, total // 5000)
    )


def handoff_signature_ratio(pixels: bytes, target: str) -> float:
    matching = 0
    for offset in range(0, len(pixels), 3):
        r, g, b = pixels[offset : offset + 3]
        if target == "linux":
            is_match = r <= 60 and g >= 160 and 80 <= b <= 175
        elif target == "windows":
            is_match = r <= 45 and 80 <= g <= 165 and b >= 170
        else:
            raise ValueError(f"unsupported handoff target: {target}")
        if is_match:
            matching += 1
    return matching / (len(pixels) // 3)


def frame_difference_ratio(before: bytes, after: bytes) -> float:
    if len(before) != len(after) or not before:
        raise ValueError("frame comparison requires equal non-empty buffers")
    changed = 0
    for offset in range(0, len(before), 3):
        if before[offset : offset + 3] != after[offset : offset + 3]:
            changed += 1
    return changed / (len(before) // 3)


class QmpClient:
    def __init__(self, socket_path: Path) -> None:
        self._socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._socket.settimeout(2.0)
        self._socket.connect(str(socket_path))
        self._stream = self._socket.makefile("rwb", buffering=0)
        greeting = self._read_message()
        if "QMP" not in greeting:
            raise RuntimeError("QEMU did not send a QMP greeting")
        self.execute("qmp_capabilities")

    def _read_message(self) -> dict[str, object]:
        while True:
            line = self._stream.readline()
            if not line:
                raise RuntimeError("QMP connection closed unexpectedly")
            message = json.loads(line)
            if "event" not in message:
                return message

    def execute(self, command: str, arguments: dict[str, object] | None = None) -> None:
        request: dict[str, object] = {"execute": command}
        if arguments is not None:
            request["arguments"] = arguments
        self._stream.write(json.dumps(request).encode("utf-8") + b"\r\n")
        response = self._read_message()
        if "error" in response:
            raise RuntimeError(f"QMP {command} failed: {response['error']}")
        if "return" not in response:
            raise RuntimeError(f"QMP {command} returned an invalid response")

    def send_key(self, key: str) -> None:
        self.execute(
            "send-key",
            {
                "keys": [{"type": "qcode", "data": key}],
                "hold-time": 100,
            },
        )

    def close(self) -> None:
        self._stream.close()
        self._socket.close()


def run_self_test() -> int:
    # Match the high-resolution OVMF regression frame from CI. The tiny red
    # APEX32 eyebrow remains one-pixel scaled at this mode, so its area does
    # not grow in proportion to the framebuffer.
    width, height = 1280, 800
    pixels = bytearray((2, 8, 12) * (width * height))
    center_start = ((height // 3) * width) + (width // 3)
    for index in range(0, 1000):
        offset = (center_start + index) * 3
        pixels[offset : offset + 3] = bytes((33, 212, 234))
    for index in range(1000, 1093):
        offset = (center_start + index) * 3
        pixels[offset : offset + 3] = bytes((239, 77, 50))
    with tempfile.TemporaryDirectory(prefix="apex32-ppm-self-test-") as directory:
        ppm = Path(directory) / "framebuffer.ppm"
        ppm.write_bytes(
            f"P6\n# analyzer fixture\n{width} {height}\n255\n".encode("ascii")
            + bytes(pixels)
        )
        parsed_width, parsed_height, parsed_pixels = parse_ppm(ppm)

    metrics = analyze_screen(parsed_width, parsed_height, parsed_pixels)
    if (
        parsed_width != width
        or parsed_height != height
        or metrics.cyan_pixels != 1000
        or metrics.red_pixels != 93
        or not looks_like_apex32(metrics)
    ):
        print("FAIL: visual analyzer self-test produced incorrect counts", file=sys.stderr)
        return 1
    if handoff_signature_ratio(bytes((0x16, 0xC7, 0x84)) * 100, "linux") != 1.0:
        print("FAIL: Linux handoff signature self-test did not match", file=sys.stderr)
        return 1
    if handoff_signature_ratio(bytes((0x00, 0x78, 0xD4)) * 100, "windows") != 1.0:
        print("FAIL: Windows handoff signature self-test did not match", file=sys.stderr)
        return 1
    baseline = bytes((0x10, 0x10, 0x10)) * 100
    changed = bytearray(baseline)
    changed[0:30] = bytes((0x21, 0xD4, 0xEA)) * 10
    if abs(frame_difference_ratio(baseline, bytes(changed)) - 0.10) > 1e-9:
        print("FAIL: focus-frame difference self-test did not match", file=sys.stderr)
        return 1
    print("PASS: QEMU framebuffer analyzer self-test")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--socket", type=Path)
    parser.add_argument("--screenshot", type=Path)
    parser.add_argument("--wait-seconds", type=float, default=45.0)
    parser.add_argument("--handoff-target", choices=("linux", "windows"))
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        return run_self_test()
    if args.socket is None or args.screenshot is None:
        parser.error("--socket and --screenshot are required")

    deadline = time.monotonic() + args.wait_seconds
    client: QmpClient | None = None
    last_error = "QMP socket did not become ready"
    last_metrics: ScreenMetrics | None = None
    first_matching_frame: float | None = None
    first_handoff_frame: float | None = None
    handoff_started = False
    last_signature_ratio = 0.0

    try:
        while time.monotonic() < deadline and client is None:
            try:
                client = QmpClient(args.socket)
            except (FileNotFoundError, ConnectionRefusedError, socket.timeout, RuntimeError) as error:
                last_error = str(error)
                time.sleep(0.25)

        if client is None:
            print(f"FAIL: {last_error}", file=sys.stderr)
            return 1

        while time.monotonic() < deadline:
            try:
                client.execute("screendump", {"filename": str(args.screenshot)})
                width, height, pixels = parse_ppm(args.screenshot)
                if handoff_started:
                    last_signature_ratio = handoff_signature_ratio(
                        pixels, args.handoff_target
                    )
                    now = time.monotonic()
                    if last_signature_ratio >= 0.90:
                        if first_handoff_frame is None:
                            first_handoff_frame = now
                        elif now - first_handoff_frame >= 0.75:
                            print(
                                "PASS: APEX32 completed a real UEFI handoff to the "
                                f"{args.handoff_target} test payload "
                                f"({width}x{height}, signature={last_signature_ratio:.1%})"
                            )
                            return 0
                    else:
                        first_handoff_frame = None
                    time.sleep(0.50)
                    continue

                last_metrics = analyze_screen(width, height, pixels)
                if looks_like_apex32(last_metrics):
                    now = time.monotonic()
                    if first_matching_frame is None:
                        first_matching_frame = now
                    elif now - first_matching_frame >= 2.25:
                        if args.handoff_target is None:
                            print(
                                "PASS: APEX32 reached a stable OVMF framebuffer "
                                f"({width}x{height}, dark={last_metrics.dark_ratio:.1%}, "
                                f"cyan={last_metrics.cyan_pixels}, red={last_metrics.red_pixels}, "
                                f"center={last_metrics.center_accent_pixels})"
                            )
                            return 0

                        if args.handoff_target == "windows":
                            previous_pixels = pixels
                            for transition in range(2):
                                client.send_key("right")
                                time.sleep(0.35)
                                client.execute(
                                    "screendump", {"filename": str(args.screenshot)}
                                )
                                moved_width, moved_height, moved_pixels = parse_ppm(
                                    args.screenshot
                                )
                                moved_metrics = analyze_screen(
                                    moved_width, moved_height, moved_pixels
                                )
                                difference = frame_difference_ratio(
                                    previous_pixels, moved_pixels
                                )
                                if not looks_like_apex32(moved_metrics):
                                    print(
                                        "FAIL: focus transition left the APEX32 "
                                        "gateway frame",
                                        file=sys.stderr,
                                    )
                                    return 1
                                if difference < 0.0005:
                                    print(
                                        "FAIL: focus transition did not visibly "
                                        "update the frame",
                                        file=sys.stderr,
                                    )
                                    return 1
                                print(
                                    "INFO: validated animated focus transition "
                                    f"{transition + 1} (changed={difference:.2%})"
                                )
                                previous_pixels = moved_pixels
                        client.send_key("ret")
                        handoff_started = True
                        first_handoff_frame = None
                        print(
                            f"INFO: selected the {args.handoff_target} card and sent Enter"
                        )
                else:
                    first_matching_frame = None
            except (OSError, ValueError, RuntimeError) as error:
                last_error = str(error)
            time.sleep(0.50)

        if handoff_started:
            print(
                "FAIL: APEX32 did not transfer control to the "
                f"{args.handoff_target} test payload "
                f"(last signature={last_signature_ratio:.1%})",
                file=sys.stderr,
            )
        elif last_metrics is not None:
            print(
                "FAIL: framebuffer never matched the APEX32 gateway palette "
                f"({last_metrics.width}x{last_metrics.height}, "
                f"dark={last_metrics.dark_ratio:.1%}, "
                f"cyan={last_metrics.cyan_pixels}, red={last_metrics.red_pixels}, "
                f"center={last_metrics.center_accent_pixels})",
                file=sys.stderr,
            )
        else:
            print(f"FAIL: no usable framebuffer captured: {last_error}", file=sys.stderr)
        return 1
    finally:
        if client is not None:
            try:
                client.execute("quit")
            except (OSError, RuntimeError):
                pass
            client.close()


if __name__ == "__main__":
    raise SystemExit(main())
