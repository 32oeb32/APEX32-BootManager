#!/usr/bin/env bash

set -euo pipefail

if [[ ${EUID} -eq 0 ]]; then
  echo "FAIL: run this test as the regular desktop user, never with sudo" >&2
  exit 1
fi

for program in cmake ninja pkg-config c++; do
  if ! command -v "${program}" >/dev/null 2>&1; then
    echo "FAIL: missing build dependency: ${program}" >&2
    echo "On Debian/Kali install: build-essential cmake ninja-build qt6-base-dev" >&2
    exit 2
  fi
done

if ! pkg-config --exists Qt6Core Qt6Widgets; then
  echo "FAIL: Qt 6 development files are missing" >&2
  echo "On Debian/Kali install: qt6-base-dev" >&2
  exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
sandbox="$(mktemp -d -t apex32-installer-user-XXXXXX)"
trap 'rm -rf "${sandbox}"' EXIT

esp="${sandbox}/mock-esp"
build_dir="${sandbox}/build"
firmware="${sandbox}/Apex32BootManager.efi"

mkdir -p \
  "${esp}/EFI/APEX32" \
  "${esp}/EFI/Microsoft/Boot" \
  "${esp}/EFI/kali" \
  "${esp}/EFI/ubuntu" \
  "${esp}/EFI/tools"

touch \
  "${esp}/EFI/APEX32/Apex32BootManager.efi" \
  "${esp}/EFI/Microsoft/Boot/bootmgfw.efi" \
  "${esp}/EFI/kali/grubx64.efi" \
  "${esp}/EFI/ubuntu/grubx64.efi" \
  "${esp}/EFI/ubuntu/shimx64.efi" \
  "${esp}/EFI/tools/shellx64.efi" \
  "${firmware}"

before="$(find "${esp}" -type f -printf '%P\n' | LC_ALL=C sort)"

cmake \
  -S "${repo_root}/Installer/Linux" \
  -B "${build_dir}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DAPEX32_FIRMWARE="${firmware}" \
  >/dev/null
cmake --build "${build_dir}" --parallel >/dev/null

"${build_dir}/apex32-installer" --self-test "${esp}"

set +e
helper_error="$(
  "${build_dir}/apex32-installer-helper" \
    install "${esp}" "${firmware}" "${firmware}" 2>&1
)"
helper_result=$?
set -e
if [[ ${helper_result} -eq 0 ]] ||
   [[ "${helper_error}" != *"graphical authorization prompt"* ]]; then
  echo "FAIL: privileged helper did not reject an unprivileged direct call" >&2
  exit 3
fi

set +e
scan_error="$(
  "${build_dir}/apex32-installer-helper" scan "${esp}" 2>&1
)"
scan_result=$?
set -e
if [[ ${scan_result} -eq 0 ]] ||
   [[ "${scan_error}" != *"graphical authorization prompt"* ]]; then
  echo "FAIL: scan helper did not reject an unprivileged direct call" >&2
  exit 3
fi

after="$(find "${esp}" -type f -printf '%P\n' | LC_ALL=C sort)"
if [[ "${before}" != "${after}" ]]; then
  echo "FAIL: regular-user test modified the mock ESP" >&2
  exit 4
fi

echo "PASS: helper refused unprivileged scan/install and the mock ESP was unchanged"
echo "PASS: regular-user installer test completed without sudo"
