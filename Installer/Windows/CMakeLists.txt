cmake_minimum_required(VERSION 3.24)
project(Apex32WindowsInstaller VERSION 0.11.0 LANGUAGES CXX RC)

if(NOT WIN32)
  message(FATAL_ERROR "the Windows installer must be built on Windows")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets)

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
    WIN32_LEAN_AND_MEAN)
target_link_libraries(
  apex32-installer
  PRIVATE
    Qt6::Core
    Qt6::Widgets
    advapi32
    shell32)

if(MSVC)
  target_compile_options(apex32-installer PRIVATE /W4 /permissive-)
else()
  target_compile_options(apex32-installer PRIVATE -Wall -Wextra -Wpedantic)
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
    "Graphical APEX32 UEFI discovery preview for Windows")
set(CPACK_PACKAGE_VERSION "0.11.0")
set(CPACK_PACKAGE_FILE_NAME "APEX32-Community-Setup")
set(CPACK_NSIS_DISPLAY_NAME "APEX32 Community Installer")
set(CPACK_NSIS_PACKAGE_NAME "APEX32 Community Installer")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_MUI_FINISHPAGE_RUN "bin/apex32-installer.exe")
set(CPACK_NSIS_MENU_LINKS
    "bin/apex32-installer.exe" "APEX32 Community Installer")
include(CPack)
