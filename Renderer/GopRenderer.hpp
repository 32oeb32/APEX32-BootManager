#pragma once

#include "UefiCompat.hpp"

extern "C" {
#include <Protocol/GraphicsOutput.h>
}

#include "Renderer/Color.hpp"
#include "Renderer/LogicalCanvas.hpp"

namespace apex32 {

enum class GradientDirection : UINT8 {
  Horizontal,
  Vertical,
};

enum class TextAlignment : UINT8 {
  Left,
  Center,
  Right,
};

class GopRenderer final {
 public:
  GopRenderer() noexcept;

  GopRenderer(const GopRenderer&) = delete;
  GopRenderer& operator=(const GopRenderer&) = delete;

  [[nodiscard]] EFI_STATUS Initialize() noexcept;
  void Shutdown() noexcept;
  [[nodiscard]] EFI_STATUS EnableLogicalCanvas() noexcept;

  [[nodiscard]] UINTN Width() const noexcept;
  [[nodiscard]] UINTN Height() const noexcept;
  [[nodiscard]] UINTN PhysicalWidth() const noexcept;
  [[nodiscard]] UINTN PhysicalHeight() const noexcept;
  [[nodiscard]] UINTN PixelsPerScanLine() const noexcept;
  [[nodiscard]] const LogicalViewport& Viewport() const noexcept;

  void Clear(RgbColor Background) noexcept;
  void BeginFrame(RgbColor Background) noexcept;
  void PutPixel(UINTN X, UINTN Y, RgbColor Color) noexcept;
  void BlendPixel(UINTN X, UINTN Y, RgbaColor Color) noexcept;
  void FillRectangle(
      UINTN X,
      UINTN Y,
      UINTN Width,
      UINTN Height,
      RgbColor Color) noexcept;
  void FillRectangleAlpha(
      UINTN X,
      UINTN Y,
      UINTN Width,
      UINTN Height,
      RgbaColor Color) noexcept;
  void DrawRectangle(
      UINTN X,
      UINTN Y,
      UINTN Width,
      UINTN Height,
      UINTN Thickness,
      RgbColor Color) noexcept;
  void FillGradient(
      UINTN X,
      UINTN Y,
      UINTN Width,
      UINTN Height,
      RgbColor Start,
      RgbColor End,
      GradientDirection Direction) noexcept;
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
  void DrawTextAligned(
      const CHAR8* Text,
      UINTN AnchorX,
      UINTN Y,
      UINTN Scale,
      RgbColor Color,
      TextAlignment Alignment) noexcept;
  [[nodiscard]] UINTN MeasureText(const CHAR8* Text, UINTN Scale) const noexcept;
  [[nodiscard]] EFI_STATUS Present() noexcept;

 private:
  void PutPhysicalPixel(UINTN X, UINTN Y, RgbColor Color) noexcept;
  void BlendPhysicalPixel(UINTN X, UINTN Y, RgbaColor Color) noexcept;
  void FillPhysicalRectangle(
      UINTN X,
      UINTN Y,
      UINTN Width,
      UINTN Height,
      RgbColor Color) noexcept;
  void DrawPhysicalLine(
      UINTN StartX,
      UINTN StartY,
      UINTN EndX,
      UINTN EndY,
      UINTN Thickness,
      RgbColor Color) noexcept;

  EFI_GRAPHICS_OUTPUT_PROTOCOL* GraphicsOutput_;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL* BackBuffer_;
  UINTN Width_;
  UINTN Height_;
  UINTN PixelsPerScanLine_;
  UINTN PixelCount_;
  UINTN BufferSize_;
  LogicalViewport Viewport_;
  BOOLEAN LogicalCanvasEnabled_;
};

}  // namespace apex32
