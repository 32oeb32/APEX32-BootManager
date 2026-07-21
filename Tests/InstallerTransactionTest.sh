#!/usr/bin/env bash

set -euo pipefail

if [[ "$#" -ne 1 || ! -x "$1" ]]; then
  echo "usage: InstallerTransactionTest.sh TRANSACTION_TEST_BINARY" >&2
  exit 2
fi

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
helper="$1"
root="$(mktemp -d -t apex32-transaction-XXXXXX)"
trap 'rm -rf "${root}"' EXIT
tools="${project_root}/Tests/InstallerTransactionTools"

run_install() {
  "$helper" install "$1" "$2" "$3"
}

prepare_case() {
  local case_root="$1"
  mkdir -p "${case_root}/esp/EFI/APEX32" "${case_root}/state"
  printf '0006,0003\n' > "${case_root}/state/order"
  printf 'MZoriginal-firmware\n' > \
    "${case_root}/esp/EFI/APEX32/Apex32BootManager.efi"
  printf 'APEX32CFG|1\nENTRY|ORIGINAL|\\EFI\\old\\bootx64.efi|generic\n' > \
    "${case_root}/esp/EFI/APEX32/apex32.cfg"
}

case_one="${root}/success-reinstall"
prepare_case "$case_one"
firmware="${case_one}/new.efi"
config="${case_one}/new.cfg"
printf 'MZnew-firmware-one\n' > "$firmware"
printf 'APEX32CFG|1\nENTRY|KALI LINUX|\\EFI\\kali\\grubx64.efi|kali\n' > "$config"

export APEX32_TRANSACTION_TEST=1
export APEX32_TRANSACTION_TEST_ESP="${case_one}/esp"
export APEX32_TRANSACTION_TEST_TOOLS="$tools"
export APEX32_TRANSACTION_TEST_STATE="${case_one}/state"

run_install "${case_one}/esp" "$firmware" "$config"
cmp "$firmware" "${case_one}/esp/EFI/APEX32/Apex32BootManager.efi"
cmp "$config" "${case_one}/esp/EFI/APEX32/apex32.cfg"
grep -q '^MZoriginal-firmware$' \
  "${case_one}/esp/EFI/APEX32/Apex32BootManager.efi.before-community"
grep -q '^ENTRY|ORIGINAL|' \
  "${case_one}/esp/EFI/APEX32/apex32.cfg.before-community"
[[ "$(<"${case_one}/state/order")" == "0007,0006,0003" ]]
[[ "$(<"${case_one}/state/create-count")" == "1" ]]

printf 'MZnew-firmware-two\n' > "$firmware"
printf 'APEX32CFG|1\nENTRY|WINDOWS|\\EFI\\Microsoft\\Boot\\bootmgfw.efi|windows\n' > "$config"
run_install "${case_one}/esp" "$firmware" "$config"
cmp "$firmware" "${case_one}/esp/EFI/APEX32/Apex32BootManager.efi"
cmp "$config" "${case_one}/esp/EFI/APEX32/apex32.cfg"
[[ "$(<"${case_one}/state/create-count")" == "1" ]]
grep -q '^MZoriginal-firmware$' \
  "${case_one}/esp/EFI/APEX32/Apex32BootManager.efi.before-community"

installed_firmware_hash="$(sha256sum \
  "${case_one}/esp/EFI/APEX32/Apex32BootManager.efi" | awk '{print $1}')"
installed_config_hash="$(sha256sum \
  "${case_one}/esp/EFI/APEX32/apex32.cfg" | awk '{print $1}')"
printf 'MZmust-roll-back\n' > "$firmware"
printf 'APEX32CFG|1\nENTRY|BROKEN|\\EFI\\broken\\bootx64.efi|generic\n' > "$config"
: > "${case_one}/state/fail-next-bootorder"
if run_install "${case_one}/esp" "$firmware" "$config" >/dev/null 2>&1; then
  echo "FAIL: injected boot-order failure was accepted" >&2
  exit 3
fi
[[ "$(sha256sum "${case_one}/esp/EFI/APEX32/Apex32BootManager.efi" | awk '{print $1}')" == \
   "$installed_firmware_hash" ]]
[[ "$(sha256sum "${case_one}/esp/EFI/APEX32/apex32.cfg" | awk '{print $1}')" == \
   "$installed_config_hash" ]]
[[ "$(<"${case_one}/state/order")" == "0007,0006,0003" ]]
[[ -f "${case_one}/state/entry" ]]

case_two="${root}/new-entry-failure"
prepare_case "$case_two"
printf 'MZcandidate\n' > "${case_two}/candidate.efi"
printf 'APEX32CFG|1\nENTRY|TEST|\\EFI\\test\\bootx64.efi|generic\n' > \
  "${case_two}/candidate.cfg"
export APEX32_TRANSACTION_TEST_ESP="${case_two}/esp"
export APEX32_TRANSACTION_TEST_STATE="${case_two}/state"
: > "${case_two}/state/fail-next-bootorder"
if run_install \
  "${case_two}/esp" \
  "${case_two}/candidate.efi" \
  "${case_two}/candidate.cfg" >/dev/null 2>&1; then
  echo "FAIL: new-entry rollback scenario was accepted" >&2
  exit 3
fi
grep -q '^MZoriginal-firmware$' \
  "${case_two}/esp/EFI/APEX32/Apex32BootManager.efi"
grep -q '^ENTRY|ORIGINAL|' "${case_two}/esp/EFI/APEX32/apex32.cfg"
[[ "$(<"${case_two}/state/order")" == "0006,0003" ]]
[[ ! -e "${case_two}/state/entry" ]]
[[ ! -e "${case_two}/esp/EFI/APEX32/Apex32BootManager.efi.before-community" ]]
[[ ! -e "${case_two}/esp/EFI/APEX32/apex32.cfg.before-community" ]]

if APEX32_TRANSACTION_TEST_ESP="${case_one}/esp" \
  run_install \
    "${case_two}/esp" \
    "${case_two}/candidate.efi" \
    "${case_two}/candidate.cfg" >/dev/null 2>&1; then
  echo "FAIL: transaction test binary escaped its declared temporary ESP" >&2
  exit 4
fi

if find "$root" -type f \
  \( -name '*.pending' -o -name '*.rollback' \) -print -quit | grep -q .; then
  echo "FAIL: transaction staging files were left behind" >&2
  exit 5
fi

echo "PASS: transactional install, idempotent reinstall, and immutable backup"
echo "PASS: injected boot-order failures restored files, order, and new entry"
echo "PASS: transaction test binary was confined to its declared temporary ESP"
