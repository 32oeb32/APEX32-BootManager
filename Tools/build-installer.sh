#!/usr/bin/env bash

set -euo pipefail

if [[ ${EUID} -eq 0 ]]; then
  echo "FAIL: build the installer as the regular user, never with sudo" >&2
  exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/Installer/Linux/build"

cmake \
  -S "${repo_root}/Installer/Linux" \
  -B "${build_dir}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DAPEX32_ENABLE_HARDWARE_INSTALL=OFF
cmake --build "${build_dir}" --parallel

echo "built: ${build_dir}/apex32-installer"
echo "built: ${build_dir}/apex32-installer-helper"
"${build_dir}/apex32-installer" --capabilities
