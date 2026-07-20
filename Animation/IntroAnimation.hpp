#pragma once

#include "UefiCompat.hpp"

namespace apex32 {

class GopRenderer;

class IntroAnimation final {
 public:
  [[nodiscard]] static EFI_STATUS Play(GopRenderer& Renderer) noexcept;

 private:
  [[nodiscard]] static EFI_STATUS RenderFrame(
      GopRenderer& Renderer,
      UINT8 RevealProgress,
      UINT8 SceneIntensity) noexcept;
};

}  // namespace apex32
