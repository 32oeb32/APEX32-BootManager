cmake_minimum_required(VERSION 3.24)
project(Apex32CommunityInstaller VERSION 0.10.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets)

add_executable(apex32-installer Gui/main.cpp)
target_link_libraries(apex32-installer PRIVATE Qt6::Core Qt6::Widgets)
target_compile_options(apex32-installer PRIVATE -Wall -Wextra -Wpedantic)

add_executable(apex32-installer-helper PrivilegedHelper/main.cpp)
target_link_libraries(apex32-installer-helper PRIVATE Qt6::Core)
target_compile_options(
  apex32-installer-helper PRIVATE -Wall -Wextra -Wpedantic)

set(
  APEX32_FIRMWARE
  ""
  CACHE FILEPATH
  "Path to the built Apex32BootManager.efi installed by the package")

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

if(APEX32_FIRMWARE)
  install(
    FILES "${APEX32_FIRMWARE}"
    DESTINATION share/apex32
    RENAME Apex32BootManager.efi)
else()
  message(WARNING "APEX32_FIRMWARE is unset; the installer package will not contain firmware")
endif()
