#pragma once

#include "UefiCompat.hpp"

namespace apex32 {

struct RgbColor {
  UINT8 Red;
  UINT8 Green;
  UINT8 Blue;
};

struct RgbaColor {
  UINT8 Red;
  UINT8 Green;
  UINT8 Blue;
  UINT8 Alpha;
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

[[nodiscard]] constexpr RgbColor BlendColor(
    const RgbColor Background,
    const RgbaColor Foreground) noexcept {
  const UINTN InverseAlpha = 255U - Foreground.Alpha;
  return {
      static_cast<UINT8>(
          ((static_cast<UINTN>(Foreground.Red) * Foreground.Alpha) +
           (static_cast<UINTN>(Background.Red) * InverseAlpha) + 127U) /
          255U),
      static_cast<UINT8>(
          ((static_cast<UINTN>(Foreground.Green) * Foreground.Alpha) +
           (static_cast<UINTN>(Background.Green) * InverseAlpha) + 127U) /
          255U),
      static_cast<UINT8>(
          ((static_cast<UINTN>(Foreground.Blue) * Foreground.Alpha) +
           (static_cast<UINTN>(Background.Blue) * InverseAlpha) + 127U) /
          255U),
  };
}

}  // namespace apex32
