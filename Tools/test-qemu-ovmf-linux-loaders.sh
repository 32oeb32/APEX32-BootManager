#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GRUB_MKSTANDALONE="${GRUB_MKSTANDALONE:-grub-mkstandalone}"
HANDOFF_TARGET="${APEX32_OVMF_LINUX_HANDOFF:-${PROJECT_ROOT}/Build/DEBUG_GCC/X64/OvmfLinuxHandoffTarget.efi}"
SCREENSHOT_BASE="${APEX32_QEMU_SCREENSHOT:-}"

command -v "${GRUB_MKSTANDALONE}" >/dev/null || {
  echo "error: ${GRUB_MKSTANDALONE} is required (install grub-efi-amd64-bin)" >&2
  exit 2
}
[[ -f "${HANDOFF_TARGET}" ]] || {
  echo "error: Linux handoff target not found at ${HANDOFF_TARGET}" >&2
  echo "error: rebuild with Tools/build-edk2.sh first" >&2
  exit 2
}

find_shim() {
  local candidate

  if [[ -n "${APEX32_OVMF_SHIM:-}" ]]; then
    [[ -f "${APEX32_OVMF_SHIM}" ]] || {
      echo "error: APEX32_OVMF_SHIM is not a file: ${APEX32_OVMF_SHIM}" >&2
      return 1
    }
    printf '%s\n' "${APEX32_OVMF_SHIM}"
    return 0
  fi

  for candidate in \
    /usr/lib/shim/shimx64.efi.signed.latest \
    /usr/lib/shim/shimx64.efi.signed \
    /usr/share/shim-signed/shimx64.efi.signed; do
    if [[ -f "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  echo "error: a distribution shimx64 EFI image was not found" >&2
  echo "error: install shim-signed or set APEX32_OVMF_SHIM" >&2
  return 1
}

screenshot_for() {
  local loader="$1"

  if [[ -z "${SCREENSHOT_BASE}" ]]; then
    return 0
  fi
  if [[ "${SCREENSHOT_BASE}" == *.ppm ]]; then
    printf '%s-%s.ppm\n' "${SCREENSHOT_BASE%.ppm}" "${loader}"
  else
    printf '%s-%s.ppm\n' "${SCREENSHOT_BASE}" "${loader}"
  fi
}

SANDBOX="$(mktemp -d "${TMPDIR:-/tmp}/apex32-real-loaders.XXXXXX")"
cleanup() {
  rm -rf "${SANDBOX}"
}
trap cleanup EXIT

GRUB_CONFIG="${SANDBOX}/grub.cfg"
GRUB_IMAGE="${SANDBOX}/grubx64.efi"
printf '%s\n' \
  'set timeout=0' \
  'search --no-floppy --file --set=root /EFI/APEX32/OvmfLinuxHandoffTarget.efi' \
  'chainloader /EFI/APEX32/OvmfLinuxHandoffTarget.efi' \
  'boot' \
  >"${GRUB_CONFIG}"
"${GRUB_MKSTANDALONE}" \
  --format=x86_64-efi \
  --output="${GRUB_IMAGE}" \
  "boot/grub/grub.cfg=${GRUB_CONFIG}"

GRUB_OVERLAY="${SANDBOX}/grub-overlay"
mkdir -p "${GRUB_OVERLAY}/EFI/APEX32"
install -m 0644 \
  "${HANDOFF_TARGET}" \
  "${GRUB_OVERLAY}/EFI/APEX32/OvmfLinuxHandoffTarget.efi"

GRUB_SCREENSHOT="$(screenshot_for grub)"
if [[ -n "${GRUB_SCREENSHOT}" ]]; then
  APEX32_OVMF_TEST_MODE=handoff-linux \
  APEX32_OVMF_LINUX_HANDOFF="${GRUB_IMAGE}" \
  APEX32_OVMF_ESP_OVERLAY="${GRUB_OVERLAY}" \
  APEX32_QEMU_SCREENSHOT="${GRUB_SCREENSHOT}" \
    "${PROJECT_ROOT}/Tools/test-qemu-ovmf.sh"
else
  APEX32_OVMF_TEST_MODE=handoff-linux \
  APEX32_OVMF_LINUX_HANDOFF="${GRUB_IMAGE}" \
  APEX32_OVMF_ESP_OVERLAY="${GRUB_OVERLAY}" \
    "${PROJECT_ROOT}/Tools/test-qemu-ovmf.sh"
fi
echo "PASS: APEX32 launched real standalone GRUB and GRUB chainloaded the test payload"

SHIM_IMAGE="$(find_shim)"
SHIM_OVERLAY="${SANDBOX}/shim-overlay"
mkdir -p \
  "${SHIM_OVERLAY}/EFI/APEX32" \
  "${SHIM_OVERLAY}/EFI/kali"
install -m 0644 "${GRUB_IMAGE}" "${SHIM_OVERLAY}/EFI/kali/grubx64.efi"
install -m 0644 \
  "${HANDOFF_TARGET}" \
  "${SHIM_OVERLAY}/EFI/APEX32/OvmfLinuxHandoffTarget.efi"
printf '%s\n' \
  'APEX32CFG|1' \
  'ENTRY|LINUX SHIM|\EFI\kali\shimx64.efi|linux' \
  >"${SHIM_OVERLAY}/EFI/APEX32/apex32.cfg"

SHIM_SCREENSHOT="$(screenshot_for shim)"
if [[ -n "${SHIM_SCREENSHOT}" ]]; then
  APEX32_OVMF_TEST_MODE=handoff-linux \
  APEX32_OVMF_LINUX_HANDOFF="${SHIM_IMAGE}" \
  APEX32_OVMF_LINUX_LOADER_PATH=EFI/kali/shimx64.efi \
  APEX32_OVMF_ESP_OVERLAY="${SHIM_OVERLAY}" \
  APEX32_QEMU_SCREENSHOT="${SHIM_SCREENSHOT}" \
    "${PROJECT_ROOT}/Tools/test-qemu-ovmf.sh"
else
  APEX32_OVMF_TEST_MODE=handoff-linux \
  APEX32_OVMF_LINUX_HANDOFF="${SHIM_IMAGE}" \
  APEX32_OVMF_LINUX_LOADER_PATH=EFI/kali/shimx64.efi \
  APEX32_OVMF_ESP_OVERLAY="${SHIM_OVERLAY}" \
    "${PROJECT_ROOT}/Tools/test-qemu-ovmf.sh"
fi
echo "PASS: APEX32 launched real distribution shim and shim reached GRUB's test payload"
