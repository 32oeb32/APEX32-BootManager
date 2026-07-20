#!/usr/bin/env bash

set -euo pipefail

if [[ ${EUID} -eq 0 ]]; then
  echo "FAIL: build the installer as the regular user, never with sudo" >&2
  exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/Installer/Linux/build"
firmware="${repo_root}/Build/DEBUG_GCC/X64/Apex32BootManager.efi"

if [[ ! -f "${firmware}" ]]; then
  echo "FAIL: build the Community EFI firmware first: ${firmware}" >&2
  exit 2
fi

cmake \
  -S "${repo_root}/Installer/Linux" \
  -B "${build_dir}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DAPEX32_FIRMWARE="${firmware}"
cmake --build "${build_dir}" --parallel

echo "built: ${build_dir}/apex32-installer"
echo "built: ${build_dir}/apex32-installer-helper"
