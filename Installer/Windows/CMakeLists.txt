cmake_minimum_required(VERSION 3.24)
project(Apex32WindowsInstaller VERSION 0.11.0 LANGUAGES CXX RC)

if(NOT WIN32)
  message(FATAL_ERROR "the Windows installer must be built on Windows")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets)

option(
  APEX32_ENABLE_HARDWARE_INSTALL
  "Package verified firmware and enable native Windows UEFI transactions"
  OFF)
set(
  APEX32_FIRMWARE
  ""
  CACHE FILEPATH
  "Path to the verified Apex32BootManager.efi artifact")

set(apex32_firmware_sha256 "")
if(APEX32_ENABLE_HARDWARE_INSTALL)
  if(NOT APEX32_FIRMWARE OR NOT EXISTS "${APEX32_FIRMWARE}")
    message(FATAL_ERROR
      "hardware installation requires an existing APEX32_FIRMWARE file")
  endif()
  file(SIZE "${APEX32_FIRMWARE}" apex32_firmware_size)
  file(READ "${APEX32_FIRMWARE}" apex32_firmware_magic LIMIT 2 HEX)
  string(TOLOWER "${apex32_firmware_magic}" apex32_firmware_magic)
  if(apex32_firmware_size LESS 4096 OR
     apex32_firmware_size GREATER 67108864 OR
     NOT apex32_firmware_magic STREQUAL "4d5a")
    message(FATAL_ERROR "APEX32_FIRMWARE is not a bounded PE/COFF image")
  endif()
  file(SHA256 "${APEX32_FIRMWARE}" apex32_firmware_sha256)
endif()

add_library(
  apex32-windows-transaction
  STATIC
  Transaction/WindowsTransaction.cpp
  Transaction/WindowsTransaction.hpp
  Transaction/WindowsFirmwareStore.cpp
  Transaction/WindowsFirmwareStore.hpp)
target_link_libraries(
  apex32-windows-transaction
  PUBLIC Qt6::Core advapi32)
target_include_directories(
  apex32-windows-transaction
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(
  apex32-windows-transaction
  PRIVATE
    NOMINMAX
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00)

add_executable(
  apex32-installer
  WIN32
  Gui/main.cpp
  apex32-installer.rc)
target_compile_definitions(
  apex32-installer
  PRIVATE
    NOMINMAX
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    APEX32_ENABLE_HARDWARE_INSTALL=$<BOOL:${APEX32_ENABLE_HARDWARE_INSTALL}>
    APEX32_PACKAGED_FIRMWARE_SHA256="${apex32_firmware_sha256}")
target_link_libraries(
  apex32-installer
  PRIVATE
    Qt6::Core
    Qt6::Widgets
    apex32-windows-transaction
    advapi32
    shell32)

enable_testing()
add_executable(
  apex32-windows-transaction-test
  "${CMAKE_CURRENT_SOURCE_DIR}/../../Tests/WindowsTransactionTest.cpp")
target_include_directories(
  apex32-windows-transaction-test
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_link_libraries(
  apex32-windows-transaction-test
  PRIVATE apex32-windows-transaction Qt6::Core)
add_test(
  NAME windows-transaction-lifecycle
  COMMAND $<TARGET_FILE:apex32-windows-transaction-test>)

add_executable(
  apex32-windows-native-firmware-test
  "${CMAKE_CURRENT_SOURCE_DIR}/../../Tests/WindowsNativeFirmwareStoreTest.cpp")
target_include_directories(
  apex32-windows-native-firmware-test
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_link_libraries(
  apex32-windows-native-firmware-test
  PRIVATE apex32-windows-transaction Qt6::Core)
add_test(
  NAME windows-native-firmware-store
  COMMAND $<TARGET_FILE:apex32-windows-native-firmware-test>)

if(MSVC)
  target_compile_options(
    apex32-installer PRIVATE /W4 /permissive-)
  target_compile_options(
    apex32-windows-transaction PRIVATE /W4 /permissive-)
  target_compile_options(
    apex32-windows-transaction-test PRIVATE /W4 /permissive-)
  target_compile_options(
    apex32-windows-native-firmware-test PRIVATE /W4 /permissive-)
else()
  target_compile_options(
    apex32-installer PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(
    apex32-windows-transaction PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(
    apex32-windows-transaction-test PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(
    apex32-windows-native-firmware-test PRIVATE -Wall -Wextra -Wpedantic)
endif()

get_target_property(qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(qt_bin_dir "${qmake_executable}" DIRECTORY)
find_program(
  windeployqt_executable
  NAMES windeployqt
  HINTS "${qt_bin_dir}"
  REQUIRED)

set(deploy_dir "${CMAKE_CURRENT_BINARY_DIR}/deploy")
add_custom_command(
  TARGET apex32-installer
  POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E rm -rf "${deploy_dir}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${deploy_dir}"
  COMMAND
    "${windeployqt_executable}"
    --no-translations
    --no-system-d3d-compiler
    --no-opengl-sw
    --dir "${deploy_dir}"
    "$<TARGET_FILE:apex32-installer>"
  VERBATIM)

install(TARGETS apex32-installer RUNTIME DESTINATION bin)
install(DIRECTORY "${deploy_dir}/" DESTINATION bin)
if(APEX32_ENABLE_HARDWARE_INSTALL)
  install(
    FILES "${APEX32_FIRMWARE}"
    DESTINATION share/apex32
    RENAME Apex32BootManager.efi)
endif()
install(
  FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../DISCLAIMER.md"
  DESTINATION share/apex32)

set(CPACK_GENERATOR "NSIS")
set(CPACK_PACKAGE_NAME "APEX32 Community Installer")
set(CPACK_PACKAGE_VENDOR "APEX32 Secure")
set(CPACK_PACKAGE_CONTACT "32oeb32@users.noreply.github.com")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://apex32-secure.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Graphical APEX32 UEFI discovery, installation, and recovery for Windows")
set(CPACK_PACKAGE_VERSION "0.11.0")
set(CPACK_PACKAGE_FILE_NAME "APEX32-Community-Setup")
set(CPACK_NSIS_DISPLAY_NAME "APEX32 Community Installer")
set(CPACK_NSIS_PACKAGE_NAME "APEX32 Community Installer")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_MUI_FINISHPAGE_RUN "bin/apex32-installer.exe")
set(CPACK_NSIS_MENU_LINKS
    "bin/apex32-installer.exe" "APEX32 Community Installer")
include(CPack)
