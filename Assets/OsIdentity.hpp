#pragma once

#include "Config/BootConfig.hpp"
#include "Renderer/Color.hpp"
#include "UefiCompat.hpp"

namespace apex32 {

class GopRenderer;

namespace osidentity {

struct Descriptor final {
  OsIcon Icon;
  const CHAR8* Token;
  const CHAR8* Badge;
  RgbColor Accent;
};

[[nodiscard]] OsIcon ParseToken(
    const CHAR8* Text,
    UINTN Length) noexcept;

[[nodiscard]] const Descriptor& Resolve(const BootEntry& Entry) noexcept;

void Draw(
    GopRenderer& Renderer,
    const BootEntry& Entry,
    UINTN CenterX,
    UINTN Top,
    UINTN Size,
    RgbColor Color) noexcept;

}  // namespace osidentity
}  // namespace apex32
