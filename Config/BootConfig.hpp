#pragma once

#include "UefiCompat.hpp"

namespace apex32 {

inline constexpr UINTN kMaximumBootEntries = 32U;
inline constexpr UINTN kBootEntryNameCapacity = 40U;
inline constexpr UINTN kBootEntryPathCapacity = 160U;

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

struct BootEntry final {
  CHAR8 Name[kBootEntryNameCapacity];
  CHAR16 LoaderPath[kBootEntryPathCapacity];
  OsIcon Icon;
  BOOLEAN Available;
};

struct BootConfiguration final {
  BootEntry Entries[kMaximumBootEntries];
  UINTN Count;
  BOOLEAN Loaded;
  EFI_STATUS Status;
};

class BootConfig final {
 public:
  [[nodiscard]] static EFI_STATUS ParseAscii(
      const CHAR8* Buffer,
      UINTN BufferSize,
      BootConfiguration* Configuration) noexcept;
};

}  // namespace apex32
