#!/usr/bin/env bash

set -euo pipefail

if [[ ${EUID} -eq 0 ]]; then
  echo "FAIL: build the package as a regular user, never with sudo" >&2
  exit 1
fi

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
firmware="${APEX32_FIRMWARE:-${project_root}/Build/DEBUG_GCC/X64/Apex32BootManager.efi}"
build_dir="${APEX32_PACKAGE_BUILD_DIR:-${project_root}/Installer/Linux/package-build}"
package_dir="${build_dir}/packages"

for program in cmake cpack ninja dpkg-deb file sha256sum; do
  command -v "${program}" >/dev/null 2>&1 || {
    echo "FAIL: missing package build dependency: ${program}" >&2
    exit 2
  }
done

[[ -f "${firmware}" ]] || {
  echo "FAIL: firmware not found: ${firmware}" >&2
  echo "Build it first with Tools/build-edk2.sh." >&2
  exit 2
}

firmware_description="$(file -b "${firmware}")"
for required in "PE32+" "x86-64"; do
  if [[ "${firmware_description}" != *"${required}"* ]]; then
    echo "FAIL: firmware validation failed: ${firmware_description}" >&2
    exit 3
  fi
done
if [[ "${firmware_description}" != *"EFI application"* &&
      "${firmware_description}" != *"EFI (application)"* ]]; then
  echo "FAIL: firmware validation failed: ${firmware_description}" >&2
  exit 3
fi

cmake -E remove_directory "${build_dir}"
cmake \
  -S "${project_root}/Installer/Linux" \
  -B "${build_dir}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DAPEX32_BUILD_DEB_PACKAGE=ON \
  -DAPEX32_ENABLE_HARDWARE_INSTALL=ON \
  -DAPEX32_FIRMWARE="${firmware}"
cmake --build "${build_dir}" --parallel
cpack \
  --config "${build_dir}/CPackConfig.cmake" \
  -B "${package_dir}"

mapfile -t packages < <(
  find "${package_dir}" -maxdepth 1 -type f -name '*.deb' -print | LC_ALL=C sort
)
if [[ ${#packages[@]} -ne 1 ]]; then
  echo "FAIL: expected exactly one Debian package, found ${#packages[@]}" >&2
  exit 4
fi

package="${packages[0]}"
dpkg-deb --info "${package}" >/dev/null
sha256sum "${firmware}"
sha256sum "${package}"
echo "built: ${package}"
