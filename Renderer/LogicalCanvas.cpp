#include "Renderer/LogicalCanvas.hpp"

namespace apex32 {

namespace {

[[nodiscard]] BOOLEAN SafeMultiply(
    const UINTN Left,
    const UINTN Right,
    UINTN* Product) noexcept {
  if (Product == nullptr) {
    return FALSE;
  }
  if ((Left != 0U) && (Right > (MAX_UINTN / Left))) {
    return FALSE;
  }
  *Product = Left * Right;
  return TRUE;
}

[[nodiscard]] BOOLEAN ScaleCoordinate(
    const UINTN Coordinate,
    const UINTN PhysicalExtent,
    const UINTN LogicalExtent,
    UINTN* Result) noexcept {
  if ((Result == nullptr) || (LogicalExtent == 0U) ||
      (Coordinate > LogicalExtent)) {
    return FALSE;
  }

  UINTN Product = 0U;
  if (!SafeMultiply(Coordinate, PhysicalExtent, &Product)) {
    return FALSE;
  }
  *Result = Product / LogicalExtent;
  return TRUE;
}

}  // namespace

EFI_STATUS LogicalCanvas::CreateViewport(
    const UINTN PhysicalWidth,
    const UINTN PhysicalHeight,
    LogicalViewport* Viewport) noexcept {
  if (Viewport == nullptr) {
    return EFI_UNSUPPORTED;
  }
  *Viewport = {};
  if ((PhysicalWidth == 0U) || (PhysicalHeight == 0U)) {
    return EFI_UNSUPPORTED;
  }

  UINTN WidthProduct = 0U;
  UINTN HeightProduct = 0U;
  if (!SafeMultiply(
          PhysicalWidth, kReferenceCanvasHeight, &WidthProduct) ||
      !SafeMultiply(
          PhysicalHeight, kReferenceCanvasWidth, &HeightProduct)) {
    return EFI_BAD_BUFFER_SIZE;
  }

  UINTN ViewportWidth = 0U;
  UINTN ViewportHeight = 0U;
  if (WidthProduct <= HeightProduct) {
    ViewportWidth = PhysicalWidth;
    ViewportHeight = WidthProduct / kReferenceCanvasWidth;
  } else {
    ViewportHeight = PhysicalHeight;
    ViewportWidth = HeightProduct / kReferenceCanvasHeight;
  }

  if ((ViewportWidth == 0U) || (ViewportHeight == 0U) ||
      (ViewportWidth > PhysicalWidth) ||
      (ViewportHeight > PhysicalHeight)) {
    return EFI_UNSUPPORTED;
  }

  Viewport->PhysicalWidth = PhysicalWidth;
  Viewport->PhysicalHeight = PhysicalHeight;
  Viewport->X = (PhysicalWidth - ViewportWidth) / 2U;
  Viewport->Y = (PhysicalHeight - ViewportHeight) / 2U;
  Viewport->Width = ViewportWidth;
  Viewport->Height = ViewportHeight;
  Viewport->Valid = TRUE;
  return EFI_SUCCESS;
}

BOOLEAN LogicalCanvas::MapPoint(
    const LogicalViewport& Viewport,
    const UINTN LogicalX,
    const UINTN LogicalY,
    UINTN* PhysicalX,
    UINTN* PhysicalY) noexcept {
  if (!Viewport.Valid || (PhysicalX == nullptr) || (PhysicalY == nullptr)) {
    return FALSE;
  }

  UINTN ScaledX = 0U;
  UINTN ScaledY = 0U;
  if (!ScaleCoordinate(
          LogicalX, Viewport.Width, kReferenceCanvasWidth, &ScaledX) ||
      !ScaleCoordinate(
          LogicalY, Viewport.Height, kReferenceCanvasHeight, &ScaledY) ||
      (ScaledX > (MAX_UINTN - Viewport.X)) ||
      (ScaledY > (MAX_UINTN - Viewport.Y))) {
    return FALSE;
  }

  *PhysicalX = Viewport.X + ScaledX;
  *PhysicalY = Viewport.Y + ScaledY;
  return TRUE;
}

BOOLEAN LogicalCanvas::MapRectangle(
    const LogicalViewport& Viewport,
    const UINTN LogicalX,
    const UINTN LogicalY,
    const UINTN LogicalWidth,
    const UINTN LogicalHeight,
    UINTN* PhysicalX,
    UINTN* PhysicalY,
    UINTN* PhysicalWidth,
    UINTN* PhysicalHeight) noexcept {
  if (!Viewport.Valid || (LogicalWidth == 0U) || (LogicalHeight == 0U) ||
      (PhysicalX == nullptr) || (PhysicalY == nullptr) ||
      (PhysicalWidth == nullptr) || (PhysicalHeight == nullptr) ||
      (LogicalX > kReferenceCanvasWidth) ||
      (LogicalY > kReferenceCanvasHeight) ||
      (LogicalWidth > (kReferenceCanvasWidth - LogicalX)) ||
      (LogicalHeight > (kReferenceCanvasHeight - LogicalY))) {
    return FALSE;
  }

  UINTN StartX = 0U;
  UINTN StartY = 0U;
  UINTN EndX = 0U;
  UINTN EndY = 0U;
  if (!MapPoint(Viewport, LogicalX, LogicalY, &StartX, &StartY) ||
      !MapPoint(
          Viewport,
          LogicalX + LogicalWidth,
          LogicalY + LogicalHeight,
          &EndX,
          &EndY) ||
      (EndX <= StartX) || (EndY <= StartY)) {
    return FALSE;
  }

  *PhysicalX = StartX;
  *PhysicalY = StartY;
  *PhysicalWidth = EndX - StartX;
  *PhysicalHeight = EndY - StartY;
  return TRUE;
}

UINTN LogicalCanvas::ScaleLength(
    const LogicalViewport& Viewport,
    const UINTN LogicalLength) noexcept {
  if (!Viewport.Valid || (LogicalLength == 0U)) {
    return 0U;
  }
  UINTN Result = 0U;
  if (!ScaleCoordinate(
          (LogicalLength > kReferenceCanvasWidth)
              ? kReferenceCanvasWidth
              : LogicalLength,
          Viewport.Width,
          kReferenceCanvasWidth,
          &Result)) {
    return 0U;
  }
  return (Result == 0U) ? 1U : Result;
}

}  // namespace apex32
