#pragma once

#include "Config/BootConfig.hpp"
#include "UefiCompat.hpp"

namespace apex32 {

class BootDiscovery final {
 public:
  [[nodiscard]] static BootConfiguration LoadSameEsp(
      EFI_HANDLE ParentImageHandle) noexcept;
};

}  // namespace apex32
