cmake_minimum_required(VERSION 3.24)
project(Apex32CommunityInstaller VERSION 0.11.0 LANGUAGES CXX)

include(GNUInstallDirs)
set(
  CMAKE_INSTALL_DOCDIR
  "${CMAKE_INSTALL_DATAROOTDIR}/doc/apex32-boot-manager")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets)

option(
  APEX32_ENABLE_HARDWARE_INSTALL
  "Enable the unfinished ESP and UEFI boot-order installation path"
  OFF)
option(
  APEX32_BUILD_DEB_PACKAGE
  "Generate the self-contained Debian beta package"
  OFF)

set(
  APEX32_FIRMWARE
  ""
  CACHE FILEPATH
  "Path to the built Apex32BootManager.efi installed by the package")

add_executable(apex32-installer Gui/main.cpp)
target_link_libraries(apex32-installer PRIVATE Qt6::Core Qt6::Widgets)
target_compile_options(apex32-installer PRIVATE -Wall -Wextra -Wpedantic)

add_executable(apex32-installer-helper PrivilegedHelper/main.cpp)
target_link_libraries(apex32-installer-helper PRIVATE Qt6::Core)
target_compile_options(
  apex32-installer-helper PRIVATE -Wall -Wextra -Wpedantic)

add_executable(
  apex32-installer-transaction-test
  EXCLUDE_FROM_ALL
  PrivilegedHelper/main.cpp)
target_link_libraries(apex32-installer-transaction-test PRIVATE Qt6::Core)
target_compile_options(
  apex32-installer-transaction-test PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(
  apex32-installer-transaction-test
  PRIVATE
    APEX32_ENABLE_HARDWARE_INSTALL=1
    APEX32_TRANSACTION_TEST=1)

if(APEX32_ENABLE_HARDWARE_INSTALL)
  if(NOT APEX32_FIRMWARE OR NOT EXISTS "${APEX32_FIRMWARE}")
    message(
      FATAL_ERROR
      "APEX32_ENABLE_HARDWARE_INSTALL=ON requires an existing "
      "APEX32_FIRMWARE file")
  endif()
  target_compile_definitions(
    apex32-installer PRIVATE APEX32_ENABLE_HARDWARE_INSTALL=1)
  target_compile_definitions(
    apex32-installer-helper PRIVATE APEX32_ENABLE_HARDWARE_INSTALL=1)
  message(WARNING "Building the experimental hardware-install path")
else()
  target_compile_definitions(
    apex32-installer PRIVATE APEX32_ENABLE_HARDWARE_INSTALL=0)
  target_compile_definitions(
    apex32-installer-helper PRIVATE APEX32_ENABLE_HARDWARE_INSTALL=0)
  message(STATUS "Building scan-only installer; hardware installation is disabled")
endif()

if(APEX32_BUILD_DEB_PACKAGE AND NOT APEX32_ENABLE_HARDWARE_INSTALL)
  message(
    FATAL_ERROR
    "APEX32_BUILD_DEB_PACKAGE=ON requires "
    "APEX32_ENABLE_HARDWARE_INSTALL=ON")
endif()

install(
  TARGETS apex32-installer
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(
  TARGETS apex32-installer-helper
  RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/apex32")
install(
  FILES org.apex32secure.installer.policy
  DESTINATION "${CMAKE_INSTALL_DATADIR}/polkit-1/actions")
install(
  FILES apex32-installer.desktop
  DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(
  FILES apex32-installer.svg
  DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
install(
  FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../DISCLAIMER.md"
  DESTINATION "${CMAKE_INSTALL_DOCDIR}")

if(APEX32_ENABLE_HARDWARE_INSTALL)
  install(
    FILES "${APEX32_FIRMWARE}"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/apex32"
    RENAME Apex32BootManager.efi)
endif()

if(APEX32_BUILD_DEB_PACKAGE)
  if(NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr")
    message(FATAL_ERROR "the Debian package must use CMAKE_INSTALL_PREFIX=/usr")
  endif()
  if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    message(FATAL_ERROR "the v0.11 beta package supports x86_64 hosts only")
  endif()

  file(SIZE "${APEX32_FIRMWARE}" APEX32_FIRMWARE_SIZE)
  file(READ "${APEX32_FIRMWARE}" APEX32_FIRMWARE_MAGIC LIMIT 2 HEX)
  string(TOUPPER "${APEX32_FIRMWARE_MAGIC}" APEX32_FIRMWARE_MAGIC)
  if(APEX32_FIRMWARE_SIZE LESS 4096 OR
     NOT APEX32_FIRMWARE_MAGIC STREQUAL "4D5A")
    message(FATAL_ERROR "the packaged firmware does not look like a PE/COFF image")
  endif()

  set(CPACK_GENERATOR "DEB")
  set(CPACK_PACKAGE_NAME "apex32-boot-manager")
  set(CPACK_PACKAGE_VENDOR "APEX32 Secure")
  set(CPACK_PACKAGE_CONTACT "32oeb32@users.noreply.github.com")
  set(CPACK_PACKAGE_HOMEPAGE_URL "https://apex32-secure.com")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
      "Graphical x86_64 UEFI boot manager and zero-terminal installer")
  set(CPACK_PACKAGE_VERSION "0.11.0")
  set(CPACK_PACKAGE_FILE_NAME "apex32-boot-manager_0.11.0~beta1_amd64")
  set(CPACK_DEBIAN_PACKAGE_VERSION "0.11.0~beta1")
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "APEX32 Secure contributors")
  set(CPACK_DEBIAN_PACKAGE_SECTION "admin")
  set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "efibootmgr, pkexec, util-linux")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://apex32-secure.com")
  include(CPack)
endif()
