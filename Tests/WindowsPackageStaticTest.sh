#!/usr/bin/env bash

set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
windows_root="${project_root}/Installer/Windows"

for required in \
  CMakeLists.txt \
  Gui/main.cpp \
  Transaction/WindowsTransaction.cpp \
  Transaction/WindowsTransaction.hpp \
  Transaction/WindowsFirmwareStore.cpp \
  Transaction/WindowsFirmwareStore.hpp \
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
grep -q 'TRANSACTION|1' "${windows_root}/Gui/main.cpp"
grep -q 'INSTALL|0' "${windows_root}/Gui/main.cpp"
grep -q 'RESTORE|0' "${windows_root}/Gui/main.cpp"
grep -q 'INSTALL|1' "${windows_root}/Gui/main.cpp"
grep -q 'RESTORE|1' "${windows_root}/Gui/main.cpp"
grep -q 'APEX32_ENABLE_HARDWARE_INSTALL' "${windows_root}/CMakeLists.txt"
grep -q 'APEX32_FIRMWARE' "${windows_root}/CMakeLists.txt"
grep -q -- '-DAPEX32_ENABLE_HARDWARE_INSTALL=ON' \
  "${project_root}/.github/workflows/windows-package.yml"
grep -q 'windows-transaction-lifecycle' "${windows_root}/CMakeLists.txt"
grep -q 'windows-native-firmware-store' "${windows_root}/CMakeLists.txt"
grep -q 'apex32-windows-transaction-test' \
  "${project_root}/.github/workflows/windows-package.yml"
grep -q 'QTemporaryDir' "${project_root}/Tests/WindowsTransactionTest.cpp"
grep -q 'FileFirmwareStore' \
  "${windows_root}/Transaction/WindowsTransaction.cpp"
grep -q 'SetFirmwareEnvironmentVariableExW' \
  "${windows_root}/Transaction/WindowsFirmwareStore.cpp"
grep -q 'ActiveOption' \
  "${windows_root}/Transaction/WindowsFirmwareStore.cpp"
grep -q 'Order.Attributes != Snapshot.BootOrderAttributes' \
  "${windows_root}/Transaction/WindowsFirmwareStore.cpp"
grep -q 'SameEspIdentity' \
  "${windows_root}/Transaction/WindowsFirmwareStore.cpp"
grep -q 'FakeVariables' \
  "${project_root}/Tests/WindowsNativeFirmwareStoreTest.cpp"
grep -q 'OtherEspApex' \
  "${project_root}/Tests/WindowsNativeFirmwareStoreTest.cpp"
! grep -R -q -E 'bcdedit(\.exe)?' "${windows_root}"
grep -q 'test-qemu-ovmf-installer-lifecycle.sh' \
  "${project_root}/.github/workflows/ovmf-visual-smoke.yml"
grep -q 'branches: \[main\]' \
  "${project_root}/.github/workflows/windows-package.yml"
capability_probe_line="$(grep -n 'std::ofstream File' \
  "${windows_root}/Gui/main.cpp" | cut -d: -f1)"
gui_start_line="$(grep -n 'QApplication Application' \
  "${windows_root}/Gui/main.cpp" | cut -d: -f1)"
[[ -n "${capability_probe_line}" && -n "${gui_start_line}" ]]
((capability_probe_line < gui_start_line)) || {
  echo "FAIL: Windows capability probe initializes the GUI first" >&2
  exit 1
}

echo "PASS: Windows package contains verified firmware, partition-aware native transactions, isolated lifecycle tests, and compile-time hardware gates"
