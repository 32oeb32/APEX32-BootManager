#include "Animation/IntroAnimation.hpp"

extern "C" {
#include <Library/UefiBootServicesTableLib.h>
}

#include "Fonts/Font5x7.hpp"
#include "Renderer/GopRenderer.hpp"
#include "Themes/DefaultTheme.hpp"

namespace apex32 {

namespace {

constexpr CHAR8 kPrimaryTitle[] = "APEX32";
constexpr CHAR8 kSecondaryTitle[] = "S E C U R E";
constexpr UINTN kRevealSteps = 32;
constexpr UINTN kExitSteps = 24;
constexpr UINTN kFrameDurationMicroseconds = 25'000;
constexpr UINTN kHoldDurationMicroseconds = 350'000;

[[nodiscard]] UINTN ResponsiveUnit(const GopRenderer& Renderer) noexcept {
  const UINTN WidthScale = Renderer.Width() / 96U;
  const UINTN HeightScale = Renderer.Height() / 54U;
  const UINTN Scale = (WidthScale < HeightScale) ? WidthScale : HeightScale;
  return (Scale == 0) ? 1U : Scale;
}

[[nodiscard]] UINT8 ProgressAfter(
    const UINT8 Progress,
    const UINT8 Start) noexcept {
  if (Progress <= Start) {
    return 0;
  }

  return static_cast<UINT8>(
      ((static_cast<UINTN>(Progress) - Start) * 255U) / (255U - Start));
}

[[nodiscard]] UINTN ScaleDimension(
    const UINTN Value,
    const UINT8 Intensity) noexcept {
  return ((Value / 255U) * Intensity) +
         (((Value % 255U) * Intensity) / 255U);
}

[[nodiscard]] RgbColor AnimatedColor(
    const RgbColor Color,
    const UINT8 ElementIntensity,
    const UINT8 SceneIntensity) noexcept {
  return ScaleColor(
      ScaleColor(Color, ElementIntensity),
      SceneIntensity);
}

void DrawGrid(
    GopRenderer& Renderer,
    const UINTN Unit,
    const UINT8 Intensity) noexcept {
  const UINTN Step = 4U * Unit;
  const RgbColor Color = AnimatedColor(
      theme::kGrid,
      150,
      Intensity);

  for (UINTN X = 0; X < Renderer.Width(); X += Step) {
    Renderer.FillRectangle(X, 0, 1, Renderer.Height(), Color);
  }

  for (UINTN Y = 0; Y < Renderer.Height(); Y += Step) {
    Renderer.FillRectangle(0, Y, Renderer.Width(), 1, Color);
  }
}

void DrawCornerFrame(
    GopRenderer& Renderer,
    const UINTN X,
    const UINTN Y,
    const UINTN Width,
    const UINTN Height,
    const UINTN CornerLength,
    const UINTN Thickness,
    const RgbColor Color) noexcept {
  Renderer.FillRectangle(X, Y, CornerLength, Thickness, Color);
  Renderer.FillRectangle(X, Y, Thickness, CornerLength, Color);

  Renderer.FillRectangle(
      X + Width - CornerLength,
      Y,
      CornerLength,
      Thickness,
      Color);
  Renderer.FillRectangle(
      X + Width - Thickness,
      Y,
      Thickness,
      CornerLength,
      Color);

  Renderer.FillRectangle(
      X,
      Y + Height - Thickness,
      CornerLength,
      Thickness,
      Color);
  Renderer.FillRectangle(
      X,
      Y + Height - CornerLength,
      Thickness,
      CornerLength,
      Color);

  Renderer.FillRectangle(
      X + Width - CornerLength,
      Y + Height - Thickness,
      CornerLength,
      Thickness,
      Color);
  Renderer.FillRectangle(
      X + Width - Thickness,
      Y + Height - CornerLength,
      Thickness,
      CornerLength,
      Color);
}

void DrawGlowLine(
    GopRenderer& Renderer,
    const UINTN X,
    const UINTN Y,
    const UINTN Width,
    const UINTN Thickness,
    const RgbColor Glow,
    const RgbColor Core) noexcept {
  if (Width == 0) {
    return;
  }

  const UINTN GlowThickness = 3U * Thickness;
  const UINTN GlowY = (Y >= Thickness) ? (Y - Thickness) : 0;
  Renderer.FillRectangle(X, GlowY, Width, GlowThickness, Glow);
  Renderer.FillRectangle(X, Y, Width, Thickness, Core);
}

[[nodiscard]] EFI_STATUS Stall(const UINTN Microseconds) noexcept {
  if (gBS == nullptr) {
    return EFI_NOT_READY;
  }

  return gBS->Stall(Microseconds);
}

}  // namespace

EFI_STATUS IntroAnimation::Play(GopRenderer& Renderer) noexcept {
  for (UINTN Step = 0; Step <= kRevealSteps; ++Step) {
    const UINT8 Progress =
        static_cast<UINT8>((Step * 255U) / kRevealSteps);
    EFI_STATUS Status = RenderFrame(Renderer, Progress, Progress);
    if (EFI_ERROR(Status)) {
      return Status;
    }

    Status = Stall(kFrameDurationMicroseconds);
    if (EFI_ERROR(Status)) {
      return Status;
    }
  }

  EFI_STATUS Status = Stall(kHoldDurationMicroseconds);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  for (UINTN Step = kExitSteps; Step > 0; --Step) {
    const UINT8 Intensity =
        static_cast<UINT8>(((Step - 1U) * 255U) / kExitSteps);
    Status = RenderFrame(Renderer, 255, Intensity);
    if (EFI_ERROR(Status)) {
      return Status;
    }

    Status = Stall(kFrameDurationMicroseconds);
    if (EFI_ERROR(Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS IntroAnimation::RenderFrame(
    GopRenderer& Renderer,
    const UINT8 RevealProgress,
    const UINT8 SceneIntensity) noexcept {
  const UINTN Unit = ResponsiveUnit(Renderer);
  const UINTN TitleScale = Unit;
  const UINTN SubtitleScale = (Unit >= 3U) ? (Unit / 3U) : 1U;
  const UINTN FrameWidth = 58U * Unit;
  const UINTN FrameHeight = 20U * Unit;
  const UINTN FrameX =
      (Renderer.Width() > FrameWidth)
          ? (Renderer.Width() - FrameWidth) / 2U
          : 0;
  const UINTN FrameY =
      (Renderer.Height() > FrameHeight)
          ? (Renderer.Height() - FrameHeight) / 2U
          : 0;
  const UINTN PrimaryWidth = Renderer.MeasureText(kPrimaryTitle, TitleScale);
  const UINTN SecondaryWidth =
      Renderer.MeasureText(kSecondaryTitle, SubtitleScale);
  const UINTN PrimaryHeight = font5x7::kGlyphHeight * TitleScale;
  const UINTN SecondaryHeight = font5x7::kGlyphHeight * SubtitleScale;
  const UINTN TextGap = 2U * Unit;
  const UINTN TextHeight = PrimaryHeight + TextGap + SecondaryHeight;

  const UINTN PrimaryX =
      (Renderer.Width() > PrimaryWidth)
          ? (Renderer.Width() - PrimaryWidth) / 2U
          : 0;
  const UINTN SecondaryX =
      (Renderer.Width() > SecondaryWidth)
          ? (Renderer.Width() - SecondaryWidth) / 2U
          : 0;
  const UINTN StartY =
      (Renderer.Height() > TextHeight)
          ? (Renderer.Height() - TextHeight) / 2U
          : 0;

  const UINT8 FrameIntensity = ProgressAfter(RevealProgress, 18);
  const UINT8 TitleIntensity = ProgressAfter(RevealProgress, 42);
  const UINT8 SubtitleIntensity = ProgressAfter(RevealProgress, 118);
  const UINT8 AccentIntensity = ProgressAfter(RevealProgress, 74);
  const UINTN Thickness = (Unit >= 5U) ? (Unit / 5U) : 1U;
  const UINTN CornerLength = ScaleDimension(7U * Unit, FrameIntensity);
  const UINTN AccentMaximumWidth = FrameWidth - (12U * Unit);
  const UINTN AccentWidth =
      ScaleDimension(AccentMaximumWidth, AccentIntensity);
  const UINTN AccentX = FrameX + ((FrameWidth - AccentWidth) / 2U);

  Renderer.BeginFrame(AnimatedColor(
      theme::kBackground,
      RevealProgress,
      SceneIntensity));
  DrawGrid(Renderer, Unit, SceneIntensity);

  DrawCornerFrame(
      Renderer,
      FrameX,
      FrameY,
      FrameWidth,
      FrameHeight,
      CornerLength,
      Thickness,
      AnimatedColor(
          theme::kCyan,
          FrameIntensity,
          SceneIntensity));

  DrawGlowLine(
      Renderer,
      AccentX,
      FrameY + (3U * Unit),
      AccentWidth,
      Thickness,
      AnimatedColor(
          theme::kRedGlow,
          AccentIntensity,
          SceneIntensity),
      AnimatedColor(
          theme::kRed,
          AccentIntensity,
          SceneIntensity));
  DrawGlowLine(
      Renderer,
      AccentX,
      FrameY + FrameHeight - (3U * Unit),
      AccentWidth,
      Thickness,
      AnimatedColor(
          theme::kCyanGlow,
          AccentIntensity,
          SceneIntensity),
      AnimatedColor(
          theme::kCyanCore,
          AccentIntensity,
          SceneIntensity));

  Renderer.DrawText(
      kPrimaryTitle,
      PrimaryX,
      StartY,
      TitleScale,
      AnimatedColor(
          theme::kPrimaryText,
          TitleIntensity,
          SceneIntensity));
  Renderer.DrawText(
      kSecondaryTitle,
      SecondaryX,
      StartY + PrimaryHeight + TextGap,
      SubtitleScale,
      AnimatedColor(
          theme::kSecondaryText,
          SubtitleIntensity,
          SceneIntensity));

  if (AccentWidth > 0) {
    const UINTN NodeSize = 2U * Thickness;
    const UINTN NodeWidth =
        (AccentWidth < NodeSize) ? AccentWidth : NodeSize;
    const UINTN NodeX = AccentX + AccentWidth - NodeWidth;
    Renderer.FillRectangle(
        NodeX,
        FrameY + (3U * Unit) - (Thickness / 2U),
        NodeWidth,
        NodeSize,
        AnimatedColor(
            theme::kCyanCore,
            AccentIntensity,
            SceneIntensity));
  }

  return Renderer.Present();
}

}  // namespace apex32
