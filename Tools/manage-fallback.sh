#!/usr/bin/env bash

set -euo pipefail

COMMAND="${1:-status}"
ESP_MOUNT="${APEX32_ESP_MOUNT:-/boot/efi}"
SOURCE="${APEX32_FALLBACK_SOURCE:-${ESP_MOUNT}/EFI/APEX32/Apex32BootManager.efi}"
TARGET="${APEX32_FALLBACK_TARGET:-${ESP_MOUNT}/EFI/BOOT/BOOTX64.EFI}"
STATE_DIR="${APEX32_FALLBACK_STATE_DIR:-/var/lib/apex32/fallback}"
BACKUP="${STATE_DIR}/BOOTX64.EFI.before-apex32"
CREATED_MARKER="${STATE_DIR}/target-created-by-apex32"

die() {
  printf 'APEX32 fallback: %s\n' "$*" >&2
  exit 1
}

require_mutation_access() {
  if [[ "${APEX32_ALLOW_NON_ROOT:-0}" != "1" ]] && [[ "${EUID}" -ne 0 ]]; then
    die "run this action with sudo"
  fi
}

show_status() {
  printf 'Source: %s\n' "$SOURCE"
  printf 'Fallback: %s\n' "$TARGET"
  printf 'Source present: %s\n' "$([[ -f "$SOURCE" ]] && printf yes || printf no)"
  printf 'Fallback present: %s\n' "$([[ -f "$TARGET" ]] && printf yes || printf no)"
  printf 'APEX32 installed: %s\n' \
    "$([[ -f "$SOURCE" && -f "$TARGET" ]] && cmp -s "$SOURCE" "$TARGET" && printf yes || printf no)"
  printf 'Original backup: %s\n' "$([[ -f "$BACKUP" ]] && printf present || printf none)"
}

install_fallback() {
  [[ -f "$SOURCE" ]] || die "source loader not found: $SOURCE"
  install -d -m 0755 "$(dirname "$TARGET")"
  install -d -m 0700 "$STATE_DIR"

  if [[ -f "$TARGET" ]] && [[ ! -f "$BACKUP" ]] &&
      [[ ! -f "$CREATED_MARKER" ]]; then
    install -m 0600 "$TARGET" "$BACKUP"
  elif [[ ! -e "$TARGET" ]] && [[ ! -f "$BACKUP" ]] &&
      [[ ! -f "$CREATED_MARKER" ]]; then
    : > "$CREATED_MARKER"
    chmod 0600 "$CREATED_MARKER"
  fi

  install -m 0644 "$SOURCE" "$TARGET"
  sync
  cmp -s "$SOURCE" "$TARGET" || die "installed fallback does not match APEX32"
  printf 'Installed APEX32 as %s.\n' "$TARGET"
}

restore_fallback() {
  if [[ -f "$BACKUP" ]]; then
    install -m 0644 "$BACKUP" "$TARGET"
    sync
    printf 'Restored the original fallback loader.\n'
  elif [[ -f "$CREATED_MARKER" ]]; then
    rm -f "$TARGET"
    sync
    printf 'Removed the APEX32 fallback; no previous fallback existed.\n'
  else
    die "no fallback recovery state exists"
  fi
}

usage() {
  cat <<'EOF'
Usage: manage-fallback.sh COMMAND

Commands:
  status    Compare APEX32 with the removable-media fallback path.
  install   Back up the current fallback once, then install APEX32.
  restore   Restore the original fallback or remove the created copy.
EOF
}

case "$COMMAND" in
  status)
    show_status
    ;;
  install)
    require_mutation_access
    install_fallback
    ;;
  restore)
    require_mutation_access
    restore_fallback
    ;;
  help|-h|--help)
    usage
    ;;
  *)
    usage >&2
    die "unknown command: $COMMAND"
    ;;
esac
