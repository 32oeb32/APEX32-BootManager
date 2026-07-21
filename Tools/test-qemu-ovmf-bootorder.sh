#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export APEX32_OVMF_TEST_MODE=bootorder
exec "${PROJECT_ROOT}/Tools/test-qemu-ovmf.sh" "$@"

