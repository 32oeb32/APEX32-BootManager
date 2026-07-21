cmake_minimum_required(VERSION 3.24)
project(Apex32CommunityInstaller VERSION 0.10.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets)

option(
  APEX32_ENABLE_HARDWARE_INSTALL
  "Enable the unfinished ESP and UEFI boot-order installation path"
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

install(TARGETS apex32-installer RUNTIME DESTINATION bin)
install(
  TARGETS apex32-installer-helper
  RUNTIME DESTINATION libexec/apex32)
install(
  FILES org.apex32secure.installer.policy
  DESTINATION share/polkit-1/actions)
install(
  FILES apex32-installer.desktop
  DESTINATION share/applications)

if(APEX32_ENABLE_HARDWARE_INSTALL)
  install(
    FILES "${APEX32_FIRMWARE}"
    DESTINATION share/apex32
    RENAME Apex32BootManager.efi)
endif()
