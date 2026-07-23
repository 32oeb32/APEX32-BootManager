# Graphics foundation

The firmware renders through `Renderer/GopRenderer` into a canonical
`EFI_GRAPHICS_OUTPUT_BLT_PIXEL` back buffer. A frame is submitted with one GOP
`Blt()` operation. Scene code never writes directly to the hardware framebuffer
and never depends on its byte layout.

## GOP initialization and formats

Initialization requires a valid GOP mode, non-zero dimensions, a usable
`Blt()` function, and dimensions whose pixel and byte counts fit in `UINTN`.
Failure is returned to `UefiMain`, which reports a console error and exits. The
renderer records the physical resolution and `PixelsPerScanLine` for
diagnostics.

Because conversion is delegated to the firmware GOP implementation, the
canonical renderer supports GOP modes backed by `PixelBlueGreenRedReserved8BitPerColor`,
`PixelRedGreenBlueReserved8BitPerColor`, and valid `PixelBitMask` modes when
their firmware exposes `Blt()`. `PixelBltOnly` modes are also usable. There is
no unsafe raw-framebuffer fallback.

## Logical canvas

Scenes draw on a 1920×1080 logical canvas. `LogicalCanvas` creates the largest
centered 16:9 viewport that fits the physical GOP mode:

- 800×600 becomes 800×450 at `(0, 75)`;
- 1024×768 becomes 1024×576 at `(0, 96)`;
- 1280×720 and 1920×1080 use their complete displays; and
- ultrawide modes receive centered horizontal letterboxing.

The framebuffer is cleared before every frame, so letterbox areas have a
deterministic background. Coordinate and rectangle multiplication is checked
before use. Mapping rejects invalid dimensions, overflow, and rectangles
outside the reference canvas.

## Drawing primitives

The renderer provides clipped pixels, solid and alpha rectangles, borders,
lines, horizontal and vertical gradients, embedded monochrome images, scaled
5×7 text, text measurement, left/center/right alignment, and frame
presentation. Per-pixel operations do not allocate memory. The procedural
APEX32 shield/aperture emblem remains an original, embedded project asset
available to later scenes, but the startup animation uses the previously
approved APEX32 Secure text-and-frame sequence and does not insert the emblem.

OS identity is separate from drawing mechanics. `Assets/OsIdentity` owns the
token registry, accent colors, and marks. `Menu/CardLayout` owns bounded
one-to-four-card page geometry. This keeps future scenes and menu behavior out
of the renderer.

`Menu/CardAnimation` adds a separate bounded integer easing layer. It
cross-fades focus presentation only after explicit keyboard navigation; it
does not add countdown or autoboot policy to the renderer.

## Validation

Run the sanitized host tests with:

```bash
./Tools/test-host.sh
```

They cover clipping, alpha blending, gradients, text geometry/alignment,
unsupported GOP state, overflow guards, the required resolutions, ultrawide
letterboxing, identity fallback, and card pagination.

After building with the pinned EDK II revision, boot the real PE/COFF image in
the disposable OVMF environment:

```bash
./Tools/test-qemu-ovmf.sh
```

The analyzer requires a stable, non-black APEX32 frame, the expected accent
palette, and accent pixels inside the central scene region. It does not compare
an exact screenshot.

## Current limits and host safety

This milestone does not select a different GOP mode and does not support legacy
BIOS or non-x86_64 firmware. Complete native `Boot####` device paths are
retained only for configuration-free recovery discovery. Installed systems use
the scanner-selected same-ESP paths until cross-ESP handoff passes a dedicated
physical-hardware matrix.

**PR #4 does not install APEX32, mount or write the host EFI System Partition,
or modify host UEFI variables.** Its real-firmware checks use a temporary
virtual ESP and private OVMF variable store.
