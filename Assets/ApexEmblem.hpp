#pragma once

#include "Renderer/Color.hpp"
#include "UefiCompat.hpp"

namespace apex32 {

class GopRenderer;

namespace apexemblem {

// Draws the original APEX32 shield-and-aperture mark. It is procedural so the
// firmware carries no filesystem image dependency or third-party artwork.
void Draw(
    GopRenderer& Renderer,
    UINTN CenterX,
    UINTN Top,
    UINTN Size,
    RgbColor Color) noexcept;

}  // namespace apexemblem
}  // namespace apex32
