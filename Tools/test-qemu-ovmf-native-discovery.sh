#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

APEX32_OVMF_TEST_MODE=native-discovery \
  "${PROJECT_ROOT}/Tools/test-qemu-ovmf.sh"
