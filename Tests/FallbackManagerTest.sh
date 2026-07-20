#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/apex32-fallback.XXXXXX")"
trap 'rm -rf "$TEST_ROOT"' EXIT

ESP_MOUNT="${TEST_ROOT}/esp"
STATE_DIR="${TEST_ROOT}/state"
SOURCE="${ESP_MOUNT}/EFI/APEX32/Apex32BootManager.efi"
TARGET="${ESP_MOUNT}/EFI/BOOT/BOOTX64.EFI"
mkdir -p "$(dirname "$SOURCE")" "$(dirname "$TARGET")"
printf 'apex32-v007\n' > "$SOURCE"
printf 'original-fallback\n' > "$TARGET"

export APEX32_ALLOW_NON_ROOT=1
export APEX32_ESP_MOUNT="$ESP_MOUNT"
export APEX32_FALLBACK_STATE_DIR="$STATE_DIR"

MANAGER="$PROJECT_ROOT/Tools/manage-fallback.sh"

"$MANAGER" install >/dev/null
cmp -s "$SOURCE" "$TARGET"
grep -q '^original-fallback$' "$STATE_DIR/BOOTX64.EFI.before-apex32"

"$MANAGER" install >/dev/null
grep -q '^original-fallback$' "$STATE_DIR/BOOTX64.EFI.before-apex32"

STATUS_OUTPUT="$("$MANAGER" status)"
grep -q '^APEX32 installed: yes$' <<< "$STATUS_OUTPUT"
grep -q '^Original backup: present$' <<< "$STATUS_OUTPUT"

"$MANAGER" restore >/dev/null
grep -q '^original-fallback$' "$TARGET"

printf 'PASS: fallback install, immutable backup, status, and restore\n'
