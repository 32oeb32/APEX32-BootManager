#pragma once

#include "UefiCompat.hpp"

extern "C" {
#include <Protocol/GraphicsOutput.h>
}

#include "Renderer/Color.hpp"

namespace apex32 {

class GopRenderer final {
 public:
  GopRenderer() noexcept;

  GopRenderer(const GopRenderer&) = delete;
  GopRenderer& operator=(const GopRenderer&) = delete;

  [[nodiscard]] EFI_STATUS Initialize() noexcept;
  void Shutdown() noexcept;

  [[nodiscard]] UINTN Width() const noexcept;
  [[nodiscard]] UINTN Height() const noexcept;

  void BeginFrame(RgbColor Background) noexcept;
  void FillRectangle(
      UINTN X,
      UINTN Y,
      UINTN Width,
      UINTN Height,
      RgbColor Color) noexcept;
  void DrawLine(
      UINTN StartX,
      UINTN StartY,
      UINTN EndX,
      UINTN EndY,
      UINTN Thickness,
      RgbColor Color) noexcept;
  void DrawMonochromeBitmap(
      const UINT8* Data,
      UINTN SourceWidth,
      UINTN SourceHeight,
      UINTN BytesPerRow,
      UINTN X,
      UINTN Y,
      UINTN Scale,
      RgbColor Color) noexcept;
  void DrawText(
      const CHAR8* Text,
      UINTN X,
      UINTN Y,
      UINTN Scale,
      RgbColor Color) noexcept;
  [[nodiscard]] UINTN MeasureText(const CHAR8* Text, UINTN Scale) const noexcept;
  [[nodiscard]] EFI_STATUS Present() noexcept;

 private:
  EFI_GRAPHICS_OUTPUT_PROTOCOL* GraphicsOutput_;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL* BackBuffer_;
  UINTN Width_;
  UINTN Height_;
  UINTN PixelCount_;
  UINTN BufferSize_;
};

}  // namespace apex32
