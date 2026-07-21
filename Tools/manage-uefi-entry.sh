#!/usr/bin/env bash

set -euo pipefail

COMMAND="${1:-status}"
LABEL="${APEX32_BOOT_LABEL:-APEX32 Secure Gateway}"
LOADER_PATH="${APEX32_LOADER_PATH:-\EFI\APEX32\Apex32BootManager.efi}"
ESP_MOUNT="${APEX32_ESP_MOUNT:-/boot/efi}"
EFI_RELATIVE_PATH="${APEX32_EFI_RELATIVE_PATH:-EFI/APEX32/Apex32BootManager.efi}"
STATE_DIR="${APEX32_STATE_DIR:-/var/lib/apex32}"
ORDER_BACKUP="${STATE_DIR}/bootorder.before-apex32"

EFIBOOTMGR="${APEX32_EFIBOOTMGR:-efibootmgr}"
FINDMNT="${APEX32_FINDMNT:-findmnt}"
LSBLK="${APEX32_LSBLK:-lsblk}"

die() {
  printf 'APEX32: %s\n' "$*" >&2
  exit 1
}

need_command() {
  command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

require_mutation_access() {
  if [[ "${APEX32_ALLOW_NON_ROOT:-0}" != "1" ]] && [[ "${EUID}" -ne 0 ]]; then
    die "run this action with sudo"
  fi

  if [[ "${APEX32_SKIP_EFI_CHECK:-0}" != "1" ]] &&
      [[ ! -d /sys/firmware/efi/efivars ]]; then
    die "UEFI variables are unavailable; boot Linux in UEFI mode first"
  fi
}

firmware_state() {
  "$EFIBOOTMGR"
}

boot_order() {
  firmware_state | awk -F': ' '
    !found && /^BootOrder:/ {
      gsub(/[[:space:]]/, "", $2)
      print toupper($2)
      found = 1
    }
  '
}

boot_next() {
  firmware_state | awk -F': ' '
    !found && /^BootNext:/ {
      gsub(/[[:space:]]/, "", $2)
      print toupper($2)
      found = 1
    }
  '
}

boot_current() {
  firmware_state | awk -F': ' '
    !found && /^BootCurrent:/ {
      gsub(/[[:space:]]/, "", $2)
      print toupper($2)
      found = 1
    }
  '
}

entry_ids() {
  firmware_state | awk -v wanted="$LABEL" '
    /^Boot[0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]/ {
      id = substr($1, 5, 4)
      name = $0
      sub(/^Boot[0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]\*?[[:space:]]*/, "", name)
      split(name, fields, "\t")
      if (fields[1] == wanted) {
        print toupper(id)
      }
    }
  '
}

entry_id() {
  local ids
  ids="$(entry_ids)"
  [[ -n "$ids" ]] || return 0
  printf '%s\n' "${ids%%$'\n'*}"
}

refind_id() {
  firmware_state | awk '
    !found && /^Boot[0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]/ {
      name = tolower($0)
      if (name ~ /refind/) {
        print toupper(substr($1, 5, 4))
        found = 1
      }
    }
  '
}

without_entry() {
  local order="$1"
  local remove_id="${2^^}"
  local result=""
  local item

  IFS=',' read -r -a items <<< "$order"
  for item in "${items[@]}"; do
    item="${item^^}"
    if [[ -n "$item" ]] && [[ "$item" != "$remove_id" ]]; then
      result="${result:+${result},}${item}"
    fi
  done

  printf '%s\n' "$result"
}

save_original_order() {
  local order="$1"
  [[ -n "$order" ]] || die "firmware BootOrder is empty"

  if [[ ! -f "$ORDER_BACKUP" ]]; then
    install -d -m 0700 "$STATE_DIR"
    umask 077
    printf '%s\n' "$order" > "$ORDER_BACKUP"
  fi
}

esp_device() {
  "$FINDMNT" -no SOURCE "$ESP_MOUNT" | head -n 1
}

register_entry() {
  local ids existing count
  ids="$(entry_ids)"
  existing="$(head -n 1 <<< "$ids")"
  count="$(wc -w <<< "$ids" | tr -d '[:space:]')"
  if [[ "$count" -gt 1 ]]; then
    die "multiple APEX32 entries detected (${ids//$'\n'/, }); remove duplicates before continuing"
  fi
  if [[ -n "$existing" ]]; then
    printf 'APEX32 entry already registered as Boot%s.\n' "$existing"
    return
  fi

  [[ -f "${ESP_MOUNT}/${EFI_RELATIVE_PATH}" ]] ||
    die "loader not found: ${ESP_MOUNT}/${EFI_RELATIVE_PATH}"

  local source disk_name disk part original new_id remaining new_order
  source="$(esp_device)"
  [[ -n "$source" ]] || die "cannot resolve the ESP mounted at $ESP_MOUNT"

  disk_name="$("$LSBLK" -no PKNAME "$source" | head -n 1 | tr -d '[:space:]')"
  part="$("$LSBLK" -no PARTN "$source" | head -n 1 | tr -d '[:space:]')"
  [[ -n "$disk_name" ]] || die "cannot resolve the parent disk for $source"
  [[ -n "$part" ]] || die "cannot resolve the partition number for $source"
  disk="/dev/${disk_name#/dev/}"

  original="$(boot_order)"
  save_original_order "$original"

  "$EFIBOOTMGR" \
    --create \
    --disk "$disk" \
    --part "$part" \
    --label "$LABEL" \
    --loader "$LOADER_PATH"

  new_id="$(entry_id)"
  [[ -n "$new_id" ]] || die "firmware did not expose the new APEX32 entry"

  remaining="$(without_entry "$original" "$new_id")"
  new_order="${remaining:+${remaining},}${new_id}"
  "$EFIBOOTMGR" --bootorder "$new_order"
  printf 'Registered Boot%s and kept it behind the existing recovery order.\n' "$new_id"
}

show_status() {
  local ids id order next current first recovery primary count
  ids="$(entry_ids)"
  id="$(entry_id)"
  count="$(wc -w <<< "$ids" | tr -d '[:space:]')"
  order="$(boot_order)"
  next="$(boot_next)"
  current="$(boot_current)"
  recovery="$(refind_id)"
  first="${order%%,*}"

  printf 'APEX32 loader: %s\n' "${ESP_MOUNT}/${EFI_RELATIVE_PATH}"
  if [[ "$count" -gt 1 ]]; then
    printf 'APEX32 entries: Boot%s\n' "${ids//$'\n'/, Boot}"
    printf 'APEX32 warning: duplicate firmware entries require cleanup\n'
  elif [[ -n "$id" ]]; then
    printf 'APEX32 entry: Boot%s\n' "$id"
  else
    printf 'APEX32 entry: not registered\n'
  fi
  printf 'BootOrder: %s\n' "${order:-unavailable}"
  if [[ -n "$current" ]]; then
    printf 'BootCurrent: Boot%s\n' "$current"
  else
    printf 'BootCurrent: unavailable\n'
  fi
  if [[ -n "$next" ]]; then
    printf 'BootNext: Boot%s\n' "$next"
  else
    printf 'BootNext: not set\n'
  fi
  primary=no
  while IFS= read -r id; do
    [[ -n "$id" ]] || continue
    if [[ "$first" == "$id" ]]; then
      primary=yes
    fi
  done <<< "$ids"
  printf 'Primary: %s\n' "$primary"
  if [[ -n "$recovery" ]]; then
    printf 'rEFInd recovery: Boot%s\n' "$recovery"
  else
    printf 'rEFInd recovery: not detected\n'
  fi
  if [[ -r "$ORDER_BACKUP" ]]; then
    printf 'Saved order: %s\n' "$(head -n 1 "$ORDER_BACKUP")"
  elif [[ -e "$ORDER_BACKUP" ]]; then
    printf 'Saved order: present (run status with sudo to read it)\n'
  else
    printf 'Saved order: none\n'
  fi
}

test_next_boot() {
  register_entry
  local id
  id="$(entry_id)"
  [[ -n "$id" ]] || die "APEX32 firmware entry is unavailable"
  "$EFIBOOTMGR" --bootnext "$id"
  printf 'Next boot is a one-time direct firmware test of Boot%s.\n' "$id"
  printf 'The persistent BootOrder has not been promoted.\n'
}

activate_primary() {
  register_entry
  local id order remaining promoted
  id="$(entry_id)"
  order="$(boot_order)"
  [[ -n "$id" ]] || die "APEX32 firmware entry is unavailable"
  save_original_order "$order"
  remaining="$(without_entry "$order" "$id")"
  promoted="${id}${remaining:+,${remaining}}"
  "$EFIBOOTMGR" --bootorder "$promoted"
  printf 'APEX32 Boot%s is now first in BootOrder.\n' "$id"
  printf 'rEFInd remains installed as an independent recovery entry.\n'
}

restore_order() {
  [[ -f "$ORDER_BACKUP" ]] || die "no saved pre-APEX32 BootOrder exists"
  local saved id next
  saved="$(head -n 1 "$ORDER_BACKUP" | tr -d '[:space:]')"
  [[ -n "$saved" ]] || die "the saved BootOrder is empty"

  id="$(entry_id)"
  next="$(boot_next)"
  if [[ -n "$id" ]] && [[ "$next" == "$id" ]]; then
    "$EFIBOOTMGR" --delete-bootnext
  fi

  "$EFIBOOTMGR" --bootorder "$saved"
  printf 'Restored the exact saved BootOrder: %s\n' "$saved"
}

usage() {
  cat <<'EOF'
Usage: manage-uefi-entry.sh COMMAND

Commands:
  status     Show the APEX32 entry, BootOrder, BootNext, and recovery entry.
  register   Create the APEX32 entry without making it primary.
  test-next  Set a one-time direct firmware boot into APEX32.
  activate   Put APEX32 first while retaining every existing boot entry.
  rollback   Restore the exact BootOrder saved before APEX32 registration.
EOF
}

need_command "$EFIBOOTMGR"

case "$COMMAND" in
  status)
    show_status
    ;;
  register)
    require_mutation_access
    need_command "$FINDMNT"
    need_command "$LSBLK"
    register_entry
    ;;
  test-next)
    require_mutation_access
    need_command "$FINDMNT"
    need_command "$LSBLK"
    test_next_boot
    ;;
  activate)
    require_mutation_access
    need_command "$FINDMNT"
    need_command "$LSBLK"
    activate_primary
    ;;
  rollback)
    require_mutation_access
    restore_order
    ;;
  help|-h|--help)
    usage
    ;;
  *)
    usage >&2
    die "unknown command: $COMMAND"
    ;;
esac
