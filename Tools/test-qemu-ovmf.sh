#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QEMU_BIN="${QEMU_BIN:-qemu-system-x86_64}"
FIRMWARE="${APEX32_FIRMWARE:-${PROJECT_ROOT}/Build/DEBUG_GCC/X64/Apex32BootManager.efi}"

find_ovmf_pair() {
  local code_candidate
  local vars_candidate

  if [[ -n "${OVMF_CODE:-}" || -n "${OVMF_VARS:-}" ]]; then
    if [[ -z "${OVMF_CODE:-}" || -z "${OVMF_VARS:-}" ]]; then
      echo "error: set both OVMF_CODE and OVMF_VARS" >&2
      return 1
    fi
    printf '%s\n%s\n' "${OVMF_CODE}" "${OVMF_VARS}"
    return 0
  fi

  while IFS='|' read -r code_candidate vars_candidate; do
    if [[ -f "${code_candidate}" && -f "${vars_candidate}" ]]; then
      printf '%s\n%s\n' "${code_candidate}" "${vars_candidate}"
      return 0
    fi
  done <<'EOF'
/usr/share/OVMF/OVMF_CODE_4M.fd|/usr/share/OVMF/OVMF_VARS_4M.fd
/usr/share/OVMF/OVMF_CODE.fd|/usr/share/OVMF/OVMF_VARS.fd
/usr/share/edk2/ovmf/OVMF_CODE.fd|/usr/share/edk2/ovmf/OVMF_VARS.fd
/usr/share/edk2/x64/OVMF_CODE.fd|/usr/share/edk2/x64/OVMF_VARS.fd
EOF

  echo "error: OVMF firmware was not found; set OVMF_CODE and OVMF_VARS" >&2
  return 1
}

command -v "${QEMU_BIN}" >/dev/null || {
  echo "error: ${QEMU_BIN} is required" >&2
  exit 2
}
command -v python3 >/dev/null || {
  echo "error: python3 is required" >&2
  exit 2
}
[[ -f "${FIRMWARE}" ]] || {
  echo "error: firmware not found at ${FIRMWARE}" >&2
  echo "error: build it with Tools/build-edk2.sh first" >&2
  exit 2
}

OVMF_OUTPUT="$(find_ovmf_pair)"
mapfile -t OVMF_PATHS <<<"${OVMF_OUTPUT}"
if [[ "${#OVMF_PATHS[@]}" -ne 2 ]]; then
  echo "error: could not resolve a matching OVMF code/variables pair" >&2
  exit 2
fi
OVMF_CODE_PATH="${OVMF_PATHS[0]}"
OVMF_VARS_PATH="${OVMF_PATHS[1]}"

SANDBOX="$(mktemp -d "${TMPDIR:-/tmp}/apex32-qemu-ovmf.XXXXXX")"
ESP_ROOT="${SANDBOX}/esp"
QMP_SOCKET="${SANDBOX}/qmp.sock"
SCREENSHOT="${APEX32_QEMU_SCREENSHOT:-${SANDBOX}/apex32-menu.ppm}"
SERIAL_LOG="${SANDBOX}/serial.log"
QEMU_PID=""

cleanup() {
  if [[ -n "${QEMU_PID}" ]] && kill -0 "${QEMU_PID}" 2>/dev/null; then
    kill "${QEMU_PID}" 2>/dev/null || true
    wait "${QEMU_PID}" 2>/dev/null || true
  fi
  if [[ "${APEX32_KEEP_QEMU_SANDBOX:-0}" == "1" ]]; then
    echo "QEMU sandbox retained at ${SANDBOX}"
  else
    rm -rf "${SANDBOX}"
  fi
}
trap cleanup EXIT

mkdir -p \
  "${ESP_ROOT}/EFI/BOOT" \
  "${ESP_ROOT}/EFI/APEX32" \
  "${ESP_ROOT}/EFI/BlackArch_Linux" \
  "${ESP_ROOT}/EFI/kali" \
  "${ESP_ROOT}/EFI/Microsoft/Boot" \
  "$(dirname "${SCREENSHOT}")"
install -m 0644 "${FIRMWARE}" "${ESP_ROOT}/EFI/BOOT/BOOTX64.EFI"
install -m 0644 "${FIRMWARE}" "${ESP_ROOT}/EFI/APEX32/Apex32BootManager.efi"
install -m 0644 /dev/null "${ESP_ROOT}/EFI/BlackArch_Linux/grubx64.efi"
install -m 0644 /dev/null "${ESP_ROOT}/EFI/kali/grubx64.efi"
install -m 0644 /dev/null "${ESP_ROOT}/EFI/Microsoft/Boot/bootmgfw.efi"
install -m 0644 "${OVMF_VARS_PATH}" "${SANDBOX}/OVMF_VARS.fd"
install -m 0644 \
  "${PROJECT_ROOT}/Config/apex32.cfg.example" \
  "${ESP_ROOT}/EFI/APEX32/apex32.cfg"

"${QEMU_BIN}" \
  -machine q35,accel=tcg \
  -cpu max \
  -m 256 \
  -drive "if=pflash,format=raw,unit=0,readonly=on,file=${OVMF_CODE_PATH}" \
  -drive "if=pflash,format=raw,unit=1,file=${SANDBOX}/OVMF_VARS.fd" \
  -drive "if=none,id=apex32esp,format=raw,file=fat:rw:${ESP_ROOT}" \
  -device "virtio-blk-pci,drive=apex32esp,bootindex=1" \
  -boot order=c,menu=off,strict=on \
  -vga std \
  -display none \
  -monitor none \
  -serial "file:${SERIAL_LOG}" \
  -net none \
  -no-reboot \
  -qmp "unix:${QMP_SOCKET},server=on,wait=off" \
  >/dev/null 2>&1 &
QEMU_PID=$!

python3 "${PROJECT_ROOT}/Tests/QemuOvmfVisualTest.py" \
  --socket "${QMP_SOCKET}" \
  --screenshot "${SCREENSHOT}" \
  --wait-seconds "${APEX32_QEMU_WAIT_SECONDS:-45}"

wait "${QEMU_PID}" 2>/dev/null || true
QEMU_PID=""
