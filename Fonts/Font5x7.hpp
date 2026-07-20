#pragma once

#include "UefiCompat.hpp"

namespace apex32::font5x7 {

inline constexpr UINTN kGlyphWidth = 5;
inline constexpr UINTN kGlyphHeight = 7;
inline constexpr UINTN kGlyphSpacing = 1;

[[nodiscard]] const UINT8* FindGlyph(CHAR8 Character) noexcept;

}  // namespace apex32::font5x7
