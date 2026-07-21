#!/usr/bin/env bash

set -euo pipefail

if [[ ${EUID} -eq 0 ]]; then
  echo "FAIL: launch the demo as the regular desktop user, never with sudo" >&2
  exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
binary="${repo_root}/Installer/Linux/build/apex32-installer"
if [[ ! -x "${binary}" ]]; then
  echo "FAIL: installer is not built at Installer/Linux/build" >&2
  echo "Run ./Tools/build-installer.sh first." >&2
  exit 2
fi

sandbox="$(mktemp -d -t apex32-installer-demo-XXXXXX)"
trap 'rm -rf "${sandbox}"' EXIT
esp="${sandbox}/mock-esp"

mkdir -p \
  "${esp}/EFI/APEX32" \
  "${esp}/EFI/Microsoft/Boot" \
  "${esp}/EFI/BOOT" \
  "${esp}/EFI/kali" \
  "${esp}/EFI/ubuntu" \
  "${esp}/EFI/tools"
touch \
  "${esp}/EFI/APEX32/Apex32BootManager.efi" \
  "${esp}/EFI/Microsoft/Boot/bootmgfw.efi" \
  "${esp}/EFI/BOOT/BOOTX64.EFI" \
  "${esp}/EFI/kali/grubx64.efi" \
  "${esp}/EFI/ubuntu/grubx64.efi" \
  "${esp}/EFI/ubuntu/shimx64.efi" \
  "${esp}/EFI/tools/shellx64.efi"

echo "Launching the safe regular-user demo. Real installation is disabled."
APEX32_TEST_ESP="${esp}" "${binary}"
