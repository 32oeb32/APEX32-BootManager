#pragma once

#include "UefiCompat.hpp"

namespace apex32::oslogos {

struct MonochromeLogo final {
  const UINT8* Data;
  UINTN Width;
  UINTN Height;
  UINTN BytesPerRow;
};

extern const MonochromeLogo kKali;
extern const MonochromeLogo kBlackArch;

}  // namespace apex32::oslogos
