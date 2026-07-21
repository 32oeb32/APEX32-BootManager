#!/usr/bin/env bash

set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
windows_root="${project_root}/Installer/Windows"

for required in \
  CMakeLists.txt \
  Gui/main.cpp \
  apex32-installer.manifest \
  apex32-installer.rc \
  README.md; do
  [[ -f "${windows_root}/${required}" ]] || {
    echo "FAIL: Windows package foundation is missing: ${required}" >&2
    exit 1
  }
done

grep -q 'CPACK_GENERATOR "NSIS"' "${windows_root}/CMakeLists.txt"
grep -q 'APEX32-Community-Setup' "${windows_root}/CMakeLists.txt"
grep -q 'requestedExecutionLevel level="asInvoker"' \
  "${windows_root}/apex32-installer.manifest"
grep -q 'ShellExecuteW' "${windows_root}/Gui/main.cpp"
grep -q 'L"runas"' "${windows_root}/Gui/main.cpp"
grep -q 'mountvol.exe' "${windows_root}/Gui/main.cpp"
grep -q 'INSTALL|0' "${windows_root}/Gui/main.cpp"
grep -q 'RESTORE|0' "${windows_root}/Gui/main.cpp"
! grep -q 'INSTALL|1' "${windows_root}/Gui/main.cpp"

echo "PASS: Windows package is one-launch, UAC-aware, read-only, and fail-closed for installation"
