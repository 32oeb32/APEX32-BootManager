#pragma once

#include "UefiCompat.hpp"

namespace apex32 {

enum class EfiLaunchStage : UINT8 {
  ResolveParent,
  BuildPath,
  LoadImage,
  StartImage,
  Returned,
};

struct EfiLaunchResult final {
  EFI_STATUS Status;
  EfiLaunchStage Stage;
};

class EfiLoader final {
 public:
  [[nodiscard]] static EfiLaunchResult LaunchFromSameEsp(
      EFI_HANDLE ParentImageHandle,
      const CHAR16* LoaderPath) noexcept;
};

}  // namespace apex32
