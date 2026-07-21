## @file
# APEX32 Boot Manager platform build description.
#
##

[Defines]
  PLATFORM_NAME           = Apex32BootManager
  PLATFORM_GUID           = E2777D03-C4B4-47C7-84A2-BD9E419CFB65
  PLATFORM_VERSION        = 0.10.0
  DSC_SPECIFICATION       = 0x00010006
  OUTPUT_DIRECTORY        = Build/Apex32BootManager
  SUPPORTED_ARCHITECTURES = X64
  BUILD_TARGETS           = DEBUG|RELEASE
  SKUID_IDENTIFIER        = DEFAULT

!include MdePkg/MdeLibs.dsc.inc

[LibraryClasses.common]
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf

[Components]
  APEX32-BootManager/Apex32BootManager.inf
  APEX32-BootManager/Tests/OvmfBootOrderSeeder.inf
  APEX32-BootManager/Tests/OvmfLinuxHandoffTarget.inf
  APEX32-BootManager/Tests/OvmfWindowsHandoffTarget.inf

[BuildOptions]
  GCC:*_*_X64_CXX_FLAGS = -std=c++20 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit
