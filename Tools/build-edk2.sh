#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EDK2_DIR_INPUT="${EDK2_DIR:-${PROJECT_ROOT}/../edk2}"
PACKAGE_PARENT="$(dirname "${PROJECT_ROOT}")"
TOOLCHAIN="${TOOLCHAIN:-GCC}"
TARGET="${TARGET:-DEBUG}"

source "${PROJECT_ROOT}/Tools/edk2-version"

if [[ "$(basename "${PROJECT_ROOT}")" != "APEX32-BootManager" ]]; then
  echo "error: the repository directory must be named APEX32-BootManager" >&2
  exit 2
fi

if [[ ! -f "${EDK2_DIR_INPUT}/edksetup.sh" ]]; then
  echo "error: EDK2_DIR does not point to an EDK II checkout" >&2
  exit 2
fi

EDK2_DIR="$(cd "${EDK2_DIR_INPUT}" && pwd)"

ACTUAL_EDK2_COMMIT="$(git -C "${EDK2_DIR}" rev-parse HEAD)"
if [[ "${ACTUAL_EDK2_COMMIT}" != "${EDK2_COMMIT}" ]] && \
   [[ "${ALLOW_UNPINNED_EDK2:-0}" != "1" ]]; then
  echo "error: EDK II must be ${EDK2_TAG} (${EDK2_COMMIT})" >&2
  echo "error: found ${ACTUAL_EDK2_COMMIT}" >&2
  echo "error: set ALLOW_UNPINNED_EDK2=1 only for compatibility work" >&2
  exit 2
fi

cd "${EDK2_DIR}"
export PACKAGES_PATH="${EDK2_DIR}:${PACKAGE_PARENT}${PACKAGES_PATH:+:${PACKAGES_PATH}}"

set +u
source edksetup.sh
set -u
make -C BaseTools -j"$(nproc)"
build \
  -a X64 \
  -t "${TOOLCHAIN}" \
  -b "${TARGET}" \
  -p APEX32-BootManager/Apex32BootManager.dsc

SOURCE_EFI="${EDK2_DIR}/Build/Apex32BootManager/${TARGET}_${TOOLCHAIN}/X64/Apex32BootManager.efi"
OUTPUT_DIR="${PROJECT_ROOT}/Build/${TARGET}_${TOOLCHAIN}/X64"

if [[ ! -f "${SOURCE_EFI}" ]]; then
  echo "error: build finished without the expected EFI artifact" >&2
  exit 1
fi

mkdir -p "${OUTPUT_DIR}"
install -m 0644 "${SOURCE_EFI}" "${OUTPUT_DIR}/Apex32BootManager.efi"
sha256sum "${OUTPUT_DIR}/Apex32BootManager.efi"
echo "built: ${OUTPUT_DIR}/Apex32BootManager.efi"
