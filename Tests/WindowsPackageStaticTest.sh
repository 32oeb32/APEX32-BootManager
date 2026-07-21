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
grep -q 'CPACK_NSIS_MUI_FINISHPAGE_RUN "bin/apex32-installer.exe"' \
  "${windows_root}/CMakeLists.txt"
grep -q -- '-B Installer/Windows/build/package' \
  "${project_root}/.github/workflows/windows-package.yml"
grep -q 'requestedExecutionLevel level="asInvoker"' \
  "${windows_root}/apex32-installer.manifest"
grep -q 'ShellExecuteW' "${windows_root}/Gui/main.cpp"
grep -q 'L"runas"' "${windows_root}/Gui/main.cpp"
grep -q 'mountvol.exe' "${windows_root}/Gui/main.cpp"
grep -q 'INSTALL|0' "${windows_root}/Gui/main.cpp"
grep -q 'RESTORE|0' "${windows_root}/Gui/main.cpp"
! grep -q 'INSTALL|1' "${windows_root}/Gui/main.cpp"
capability_probe_line="$(grep -n 'std::ofstream File' \
  "${windows_root}/Gui/main.cpp" | cut -d: -f1)"
gui_start_line="$(grep -n 'QApplication Application' \
  "${windows_root}/Gui/main.cpp" | cut -d: -f1)"
[[ -n "${capability_probe_line}" && -n "${gui_start_line}" ]]
((capability_probe_line < gui_start_line)) || {
  echo "FAIL: Windows capability probe initializes the GUI first" >&2
  exit 1
}

echo "PASS: Windows package is one-launch, UAC-aware, read-only, and fail-closed for installation"
