#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/apex32-uefi-entry.XXXXXX")"
trap 'rm -rf "$TEST_ROOT"' EXIT

FIRMWARE_STATE="${TEST_ROOT}/firmware"
ESP_MOUNT="${TEST_ROOT}/esp"
MANAGER_STATE="${TEST_ROOT}/manager"
mkdir -p "$FIRMWARE_STATE" "$ESP_MOUNT/EFI/APEX32"
printf '0006,0002,0003\n' > "$FIRMWARE_STATE/order"
: > "$ESP_MOUNT/EFI/APEX32/Apex32BootManager.efi"

export APEX32_ALLOW_NON_ROOT=1
export APEX32_SKIP_EFI_CHECK=1
export APEX32_TEST_FIRMWARE_STATE="$FIRMWARE_STATE"
export APEX32_EFIBOOTMGR="$PROJECT_ROOT/Tests/UefiToolsStub/efibootmgr"
export APEX32_FINDMNT="$PROJECT_ROOT/Tests/UefiToolsStub/findmnt"
export APEX32_LSBLK="$PROJECT_ROOT/Tests/UefiToolsStub/lsblk"
export APEX32_ESP_MOUNT="$ESP_MOUNT"
export APEX32_STATE_DIR="$MANAGER_STATE"

MANAGER="$PROJECT_ROOT/Tools/manage-uefi-entry.sh"

"$MANAGER" register >/dev/null
[[ "$(<"$FIRMWARE_STATE/order")" == "0006,0002,0003,0007" ]]
[[ "$(<"$MANAGER_STATE/bootorder.before-apex32")" == "0006,0002,0003" ]]

"$MANAGER" test-next >/dev/null
[[ "$(<"$FIRMWARE_STATE/next")" == "0007" ]]
[[ "$(<"$FIRMWARE_STATE/order")" == "0006,0002,0003,0007" ]]
[[ "$(<"$FIRMWARE_STATE/create-count")" == "1" ]]

"$MANAGER" activate >/dev/null
[[ "$(<"$FIRMWARE_STATE/order")" == "0007,0006,0002,0003" ]]

STATUS_OUTPUT="$("$MANAGER" status)"
grep -q '^APEX32 entry: Boot0007$' <<< "$STATUS_OUTPUT"
grep -q '^BootCurrent: Boot0006$' <<< "$STATUS_OUTPUT"
grep -q '^Primary: yes$' <<< "$STATUS_OUTPUT"
grep -q '^rEFInd recovery: Boot0006$' <<< "$STATUS_OUTPUT"

"$MANAGER" rollback >/dev/null
[[ "$(<"$FIRMWARE_STATE/order")" == "0006,0002,0003" ]]
[[ ! -e "$FIRMWARE_STATE/next" ]]

: > "$FIRMWARE_STATE/apex32-duplicate"
DUPLICATE_STATUS="$("$MANAGER" status)"
grep -q '^APEX32 entries: Boot0007, Boot0008$' <<< "$DUPLICATE_STATUS"
grep -q '^APEX32 warning: duplicate firmware entries require cleanup$' \
  <<< "$DUPLICATE_STATUS"
if "$MANAGER" register >/dev/null 2>&1; then
  printf 'FAIL: duplicate registration was not rejected\n' >&2
  exit 1
fi

printf 'PASS: UEFI entry lifecycle, verbose parsing, duplicate guard, and rollback\n'
