#pragma once

#include "UefiCompat.hpp"

namespace apex32 {

struct RgbColor {
  UINT8 Red;
  UINT8 Green;
  UINT8 Blue;
};

[[nodiscard]] constexpr RgbColor ScaleColor(
    const RgbColor Color,
    const UINT8 Intensity) noexcept {
  return {
      static_cast<UINT8>((static_cast<UINTN>(Color.Red) * Intensity) / 255U),
      static_cast<UINT8>((static_cast<UINTN>(Color.Green) * Intensity) / 255U),
      static_cast<UINT8>((static_cast<UINTN>(Color.Blue) * Intensity) / 255U),
  };
}

}  // namespace apex32
