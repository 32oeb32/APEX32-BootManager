#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_BINARY="${TMPDIR:-/tmp}/apex32-host-smoke"

cd "${PROJECT_ROOT}"

g++ \
  -std=c++20 \
  -fshort-wchar \
  -fno-exceptions \
  -fno-rtti \
  -fno-threadsafe-statics \
  -fno-use-cxa-atexit \
  -Wall \
  -Wextra \
  -Werror \
  -Wconversion \
  -fsanitize=address,undefined \
  -fno-omit-frame-pointer \
  -ITests/HostStub \
  -I. \
  Animation/IntroAnimation.cpp \
  Assets/OsLogos.cpp \
  Assets/OsIdentity.cpp \
  Assets/ApexEmblem.cpp \
  Boot/BootDiscovery.cpp \
  Boot/EfiLoader.cpp \
  BootManager/Main.cpp \
  Config/BootConfig.cpp \
  Fonts/Font5x7.cpp \
  Menu/WorkspaceMenu.cpp \
  Menu/CardLayout.cpp \
  Renderer/GopRenderer.cpp \
  Renderer/LogicalCanvas.cpp \
  Tests/HostSmoke.cpp \
  -o "${TEST_BINARY}"

ASAN_OPTIONS="detect_leaks=0${ASAN_OPTIONS:+:${ASAN_OPTIONS}}" \
  "${TEST_BINARY}"
rm -f "${TEST_BINARY}"

"${PROJECT_ROOT}/Tests/UefiEntryManagerTest.sh"
"${PROJECT_ROOT}/Tests/FallbackManagerTest.sh"
python3 "${PROJECT_ROOT}/Tests/QemuOvmfVisualTest.py" --self-test
"${PROJECT_ROOT}/Tests/OneStepInstallTest.sh"
"${PROJECT_ROOT}/Tests/WindowsPackageStaticTest.sh"
