#pragma once

#include "Config/BootConfig.hpp"
#include "UefiCompat.hpp"

namespace apex32 {

// Read-only parser and enumerator for UEFI Boot#### load options. The class
// never creates, deletes, or reorders firmware variables.
class FirmwareBootDiscovery final {
 public:
  [[nodiscard]] static EFI_STATUS Discover(
      BootConfiguration* Configuration) noexcept;

  [[nodiscard]] static EFI_STATUS ParseLoadOption(
      UINT16 BootNumber,
      const UINT8* Buffer,
      UINTN BufferSize,
      BootEntry* Entry) noexcept;
};

}  // namespace apex32
