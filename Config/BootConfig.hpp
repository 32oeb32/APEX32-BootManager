#pragma once

#include "UefiCompat.hpp"

namespace apex32 {

inline constexpr UINTN kMaximumBootEntries = 32U;
inline constexpr UINTN kBootEntryNameCapacity = 40U;
inline constexpr UINTN kBootEntryPathCapacity = 160U;
inline constexpr UINTN kBootEntryDevicePathCapacity = 512U;

enum class OsIcon : UINT8 {
  Generic,
  Linux,
  Windows,
  Kali,
  BlackArch,
  Ubuntu,
  Fedora,
  Arch,
  Debian,
  Mint,
  OpenSuse,
  PopOs,
  OpenCore,
  Recovery,
  Usb,
  Network,
};

enum class BootEntrySource : UINT8 {
  Configuration,
  Firmware,
};

struct BootEntry final {
  CHAR8 Name[kBootEntryNameCapacity];
  CHAR16 LoaderPath[kBootEntryPathCapacity];
  OsIcon Icon;
  BOOLEAN Available;
  BootEntrySource Source;
  UINT16 FirmwareBootNumber;
  UINT16 DevicePathSize;
  alignas(8) UINT8 DevicePath[kBootEntryDevicePathCapacity];
};

struct BootConfiguration final {
  BootEntry Entries[kMaximumBootEntries];
  UINTN Count;
  UINTN FirmwareCount;
  UINTN ConfigCount;
  BOOLEAN Loaded;
  EFI_STATUS Status;
  EFI_STATUS FirmwareStatus;
  EFI_STATUS ConfigStatus;
};

class BootConfig final {
 public:
  [[nodiscard]] static EFI_STATUS ParseAscii(
      const CHAR8* Buffer,
      UINTN BufferSize,
      BootConfiguration* Configuration) noexcept;
};

}  // namespace apex32
