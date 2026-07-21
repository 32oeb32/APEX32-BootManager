#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCREENSHOT_BASE="${APEX32_QEMU_SCREENSHOT:-}"

run_handoff() {
  local target="$1"
  local screenshot=""

  if [[ -n "${SCREENSHOT_BASE}" ]]; then
    if [[ "${SCREENSHOT_BASE}" == *.ppm ]]; then
      screenshot="${SCREENSHOT_BASE%.ppm}-${target}.ppm"
    else
      screenshot="${SCREENSHOT_BASE}-${target}.ppm"
    fi
  fi

  if [[ -n "${screenshot}" ]]; then
    APEX32_OVMF_TEST_MODE="handoff-${target}" \
    APEX32_QEMU_SCREENSHOT="${screenshot}" \
      "${PROJECT_ROOT}/Tools/test-qemu-ovmf.sh"
  else
    APEX32_OVMF_TEST_MODE="handoff-${target}" \
      "${PROJECT_ROOT}/Tools/test-qemu-ovmf.sh"
  fi
}

run_handoff linux
run_handoff windows
