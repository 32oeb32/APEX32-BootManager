#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QEMU_BIN="${QEMU_BIN:-qemu-system-x86_64}"
FIRMWARE="${APEX32_FIRMWARE:-${PROJECT_ROOT}/Build/DEBUG_GCC/X64/Apex32BootManager.efi}"
SEEDER="${APEX32_OVMF_SEEDER:-${PROJECT_ROOT}/Build/DEBUG_GCC/X64/OvmfBootOrderSeeder.efi}"
INSTALLER_LIFECYCLE="${APEX32_OVMF_INSTALLER_LIFECYCLE:-${PROJECT_ROOT}/Build/DEBUG_GCC/X64/OvmfInstallerLifecycle.efi}"
LINUX_HANDOFF="${APEX32_OVMF_LINUX_HANDOFF:-${PROJECT_ROOT}/Build/DEBUG_GCC/X64/OvmfLinuxHandoffTarget.efi}"
WINDOWS_HANDOFF="${APEX32_OVMF_WINDOWS_HANDOFF:-${PROJECT_ROOT}/Build/DEBUG_GCC/X64/OvmfWindowsHandoffTarget.efi}"
LINUX_LOADER_PATH="${APEX32_OVMF_LINUX_LOADER_PATH:-EFI/kali/grubx64.efi}"
ESP_OVERLAY="${APEX32_OVMF_ESP_OVERLAY:-}"
TEST_MODE="${APEX32_OVMF_TEST_MODE:-fallback}"

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
case "/${LINUX_LOADER_PATH}/" in
  *"/../"*|*"/./"*|"//"*)
    echo "error: APEX32_OVMF_LINUX_LOADER_PATH must be a relative ESP path" >&2
    exit 2
    ;;
esac
if [[ -n "${ESP_OVERLAY}" && ! -d "${ESP_OVERLAY}" ]]; then
  echo "error: APEX32_OVMF_ESP_OVERLAY is not a directory: ${ESP_OVERLAY}" >&2
  exit 2
fi

LINUX_LOADER_SOURCE=/dev/null
WINDOWS_LOADER_SOURCE=/dev/null
HANDOFF_ARGUMENTS=()
QEMU_DEBUG_EXIT_OPTIONS=()

case "${TEST_MODE}" in
  fallback)
    FALLBACK_LOADER="${FIRMWARE}"
    WAIT_SECONDS="${APEX32_QEMU_WAIT_SECONDS:-45}"
    QEMU_REBOOT_OPTIONS=(-no-reboot)
    ;;
  bootorder)
    [[ -f "${SEEDER}" ]] || {
      echo "error: OVMF boot-order seeder not found at ${SEEDER}" >&2
      echo "error: rebuild with Tools/build-edk2.sh first" >&2
      exit 2
    }
    FALLBACK_LOADER="${SEEDER}"
    WAIT_SECONDS="${APEX32_QEMU_WAIT_SECONDS:-75}"
    QEMU_REBOOT_OPTIONS=()
    ;;
  native-discovery)
    [[ -f "${SEEDER}" && -f "${LINUX_HANDOFF}" && -f "${WINDOWS_HANDOFF}" ]] || {
      echo "error: native-discovery test artifacts are missing; rebuild first" >&2
      exit 2
    }
    FALLBACK_LOADER="${SEEDER}"
    LINUX_LOADER_SOURCE="${LINUX_HANDOFF}"
    WINDOWS_LOADER_SOURCE="${WINDOWS_HANDOFF}"
    WAIT_SECONDS="${APEX32_QEMU_WAIT_SECONDS:-90}"
    QEMU_REBOOT_OPTIONS=()
    HANDOFF_ARGUMENTS=(--handoff-target linux)
    ;;
  installer-lifecycle)
    [[ -f "${INSTALLER_LIFECYCLE}" ]] || {
      echo "error: OVMF installer lifecycle artifact is missing; rebuild first" >&2
      exit 2
    }
    FALLBACK_LOADER="${INSTALLER_LIFECYCLE}"
    WAIT_SECONDS="${APEX32_QEMU_WAIT_SECONDS:-40}"
    QEMU_REBOOT_OPTIONS=(-no-reboot)
    QEMU_DEBUG_EXIT_OPTIONS=(-device isa-debug-exit,iobase=0xf4,iosize=0x04)
    ;;
  handoff-linux)
    [[ -f "${LINUX_HANDOFF}" ]] || {
      echo "error: OVMF Linux handoff target not found at ${LINUX_HANDOFF}" >&2
      echo "error: rebuild with Tools/build-edk2.sh first" >&2
      exit 2
    }
    FALLBACK_LOADER="${FIRMWARE}"
    LINUX_LOADER_SOURCE="${LINUX_HANDOFF}"
    WAIT_SECONDS="${APEX32_QEMU_WAIT_SECONDS:-60}"
    QEMU_REBOOT_OPTIONS=(-no-reboot)
    HANDOFF_ARGUMENTS=(--handoff-target linux)
    ;;
  handoff-windows)
    [[ -f "${WINDOWS_HANDOFF}" ]] || {
      echo "error: OVMF Windows handoff target not found at ${WINDOWS_HANDOFF}" >&2
      echo "error: rebuild with Tools/build-edk2.sh first" >&2
      exit 2
    }
    FALLBACK_LOADER="${FIRMWARE}"
    WINDOWS_LOADER_SOURCE="${WINDOWS_HANDOFF}"
    WAIT_SECONDS="${APEX32_QEMU_WAIT_SECONDS:-60}"
    QEMU_REBOOT_OPTIONS=(-no-reboot)
    HANDOFF_ARGUMENTS=(--handoff-target windows)
    ;;
  *)
    echo "error: APEX32_OVMF_TEST_MODE must be fallback, bootorder, native-discovery, installer-lifecycle, handoff-linux, or handoff-windows" >&2
    exit 2
    ;;
esac

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
install -m 0644 "${FALLBACK_LOADER}" "${ESP_ROOT}/EFI/BOOT/BOOTX64.EFI"
install -m 0644 "${FIRMWARE}" "${ESP_ROOT}/EFI/APEX32/Apex32BootManager.efi"
install -m 0644 /dev/null "${ESP_ROOT}/EFI/BlackArch_Linux/grubx64.efi"
install -Dm 0644 "${LINUX_LOADER_SOURCE}" "${ESP_ROOT}/${LINUX_LOADER_PATH}"
install -m 0644 "${WINDOWS_LOADER_SOURCE}" "${ESP_ROOT}/EFI/Microsoft/Boot/bootmgfw.efi"
install -m 0644 "${OVMF_VARS_PATH}" "${SANDBOX}/OVMF_VARS.fd"
install -m 0644 \
  "${PROJECT_ROOT}/Config/apex32.cfg.example" \
  "${ESP_ROOT}/EFI/APEX32/apex32.cfg"
if [[ -n "${ESP_OVERLAY}" ]]; then
  cp -a "${ESP_OVERLAY}/." "${ESP_ROOT}/"
fi

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
  "${QEMU_REBOOT_OPTIONS[@]}" \
  "${QEMU_DEBUG_EXIT_OPTIONS[@]}" \
  -qmp "unix:${QMP_SOCKET},server=on,wait=off" \
  >/dev/null 2>&1 &
QEMU_PID=$!

if [[ "${TEST_MODE}" == "installer-lifecycle" ]]; then
  for _ in $(seq 1 $((WAIT_SECONDS * 10))); do
    if ! kill -0 "${QEMU_PID}" 2>/dev/null; then
      break
    fi
    sleep 0.1
  done
  if kill -0 "${QEMU_PID}" 2>/dev/null; then
    echo "error: OVMF installer lifecycle did not finish" >&2
    exit 1
  fi
  set +e
  wait "${QEMU_PID}"
  QEMU_STATUS=$?
  set -e
  QEMU_PID=""
  if [[ "${QEMU_STATUS}" -ne 85 ]]; then
    echo "error: OVMF installer lifecycle exited with ${QEMU_STATUS}" >&2
    sed -n '1,120p' "${SERIAL_LOG}" >&2
    exit 1
  fi
  echo "PASS: OVMF created and promoted an APEX32 entry, then restored exact BootOrder and removed it"
  exit 0
fi

python3 "${PROJECT_ROOT}/Tests/QemuOvmfVisualTest.py" \
  --socket "${QMP_SOCKET}" \
  --screenshot "${SCREENSHOT}" \
  --wait-seconds "${WAIT_SECONDS}" \
  "${HANDOFF_ARGUMENTS[@]}"

if [[ "${TEST_MODE}" == "bootorder" ]]; then
  echo "PASS: OVMF rebooted through seeded Boot7A32 as first BootOrder entry"
elif [[ "${TEST_MODE}" == "native-discovery" ]]; then
  echo "PASS: APEX32 discovered Boot7A33 and launched its native device path"
fi

wait "${QEMU_PID}" 2>/dev/null || true
QEMU_PID=""
