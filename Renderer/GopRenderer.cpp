#include "Renderer/GopRenderer.hpp"

extern "C" {
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
}

#include "Fonts/Font5x7.hpp"

namespace apex32 {

namespace {

[[nodiscard]] EFI_GRAPHICS_OUTPUT_BLT_PIXEL ToBltPixel(
    const RgbColor Color) noexcept {
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel;
  Pixel.Blue = Color.Blue;
  Pixel.Green = Color.Green;
  Pixel.Red = Color.Red;
  Pixel.Reserved = 0;
  return Pixel;
}

[[nodiscard]] RgbColor FromBltPixel(
    const EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel) noexcept {
  return {Pixel.Red, Pixel.Green, Pixel.Blue};
}

[[nodiscard]] UINT8 InterpolateChannel(
    const UINT8 Start,
    const UINT8 End,
    const UINTN Position,
    const UINTN Maximum) noexcept {
  if ((Maximum == 0U) || (Position >= Maximum)) {
    return End;
  }
  if (Start <= End) {
    const UINTN Difference = static_cast<UINTN>(End - Start);
    return static_cast<UINT8>(
        static_cast<UINTN>(Start) + ((Difference * Position) / Maximum));
  }
  const UINTN Difference = static_cast<UINTN>(Start - End);
  return static_cast<UINT8>(
      static_cast<UINTN>(Start) - ((Difference * Position) / Maximum));
}

[[nodiscard]] RgbColor InterpolateColor(
    const RgbColor Start,
    const RgbColor End,
    const UINTN Position,
    const UINTN Maximum) noexcept {
  return {
      InterpolateChannel(Start.Red, End.Red, Position, Maximum),
      InterpolateChannel(Start.Green, End.Green, Position, Maximum),
      InterpolateChannel(Start.Blue, End.Blue, Position, Maximum),
  };
}

}  // namespace

GopRenderer::GopRenderer() noexcept
    : GraphicsOutput_(nullptr),
      BackBuffer_(nullptr),
      Width_(0),
      Height_(0),
      PixelsPerScanLine_(0),
      PixelCount_(0),
      BufferSize_(0),
      Viewport_{},
      LogicalCanvasEnabled_(FALSE) {}

EFI_STATUS GopRenderer::Initialize() noexcept {
  Shutdown();

  if (gBS == nullptr) {
    return EFI_NOT_READY;
  }

  EFI_GRAPHICS_OUTPUT_PROTOCOL* GraphicsOutput = nullptr;
  const EFI_STATUS LocateStatus = gBS->LocateProtocol(
      &gEfiGraphicsOutputProtocolGuid,
      nullptr,
      reinterpret_cast<VOID**>(&GraphicsOutput));

  if (EFI_ERROR(LocateStatus)) {
    return LocateStatus;
  }

  if ((GraphicsOutput == nullptr) || (GraphicsOutput->Mode == nullptr) ||
      (GraphicsOutput->Mode->Info == nullptr) ||
      (GraphicsOutput->Blt == nullptr)) {
    return EFI_UNSUPPORTED;
  }

  const UINTN Width = GraphicsOutput->Mode->Info->HorizontalResolution;
  const UINTN Height = GraphicsOutput->Mode->Info->VerticalResolution;
  if ((Width == 0) || (Height == 0)) {
    return EFI_UNSUPPORTED;
  }

  if ((Width > (MAX_UINTN / Height)) ||
      (Width > (MAX_UINTN / 255U)) ||
      (Height > (MAX_UINTN / 255U))) {
    return EFI_BAD_BUFFER_SIZE;
  }

  const UINTN PixelCount = Width * Height;
  if (PixelCount > (MAX_UINTN / sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL))) {
    return EFI_BAD_BUFFER_SIZE;
  }

  const UINTN BufferSize =
      PixelCount * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
  auto* BackBuffer = static_cast<EFI_GRAPHICS_OUTPUT_BLT_PIXEL*>(
      AllocateZeroPool(BufferSize));
  if (BackBuffer == nullptr) {
    return EFI_OUT_OF_RESOURCES;
  }

  LogicalViewport Viewport{};
  const EFI_STATUS ViewportStatus =
      LogicalCanvas::CreateViewport(Width, Height, &Viewport);
  if (EFI_ERROR(ViewportStatus)) {
    FreePool(BackBuffer);
    return ViewportStatus;
  }

  GraphicsOutput_ = GraphicsOutput;
  BackBuffer_ = BackBuffer;
  Width_ = Width;
  Height_ = Height;
  PixelsPerScanLine_ =
      (GraphicsOutput->Mode->Info->PixelsPerScanLine >= Width)
          ? GraphicsOutput->Mode->Info->PixelsPerScanLine
          : Width;
  PixelCount_ = PixelCount;
  BufferSize_ = BufferSize;
  Viewport_ = Viewport;
  LogicalCanvasEnabled_ = FALSE;
  return EFI_SUCCESS;
}

void GopRenderer::Shutdown() noexcept {
  if (BackBuffer_ != nullptr) {
    FreePool(BackBuffer_);
  }

  GraphicsOutput_ = nullptr;
  BackBuffer_ = nullptr;
  Width_ = 0;
  Height_ = 0;
  PixelsPerScanLine_ = 0;
  PixelCount_ = 0;
  BufferSize_ = 0;
  Viewport_ = {};
  LogicalCanvasEnabled_ = FALSE;
}

EFI_STATUS GopRenderer::EnableLogicalCanvas() noexcept {
  if ((BackBuffer_ == nullptr) || !Viewport_.Valid) {
    return EFI_NOT_READY;
  }
  LogicalCanvasEnabled_ = TRUE;
  return EFI_SUCCESS;
}

UINTN GopRenderer::Width() const noexcept {
  return LogicalCanvasEnabled_ ? kReferenceCanvasWidth : Width_;
}

UINTN GopRenderer::Height() const noexcept {
  return LogicalCanvasEnabled_ ? kReferenceCanvasHeight : Height_;
}

UINTN GopRenderer::PhysicalWidth() const noexcept {
  return Width_;
}

UINTN GopRenderer::PhysicalHeight() const noexcept {
  return Height_;
}

UINTN GopRenderer::PixelsPerScanLine() const noexcept {
  return PixelsPerScanLine_;
}

const LogicalViewport& GopRenderer::Viewport() const noexcept {
  return Viewport_;
}

void GopRenderer::Clear(const RgbColor Background) noexcept {
  BeginFrame(Background);
}

void GopRenderer::BeginFrame(const RgbColor Background) noexcept {
  if (BackBuffer_ == nullptr) {
    return;
  }

  if ((Background.Red == 0) && (Background.Green == 0) &&
      (Background.Blue == 0)) {
    SetMem(BackBuffer_, BufferSize_, 0);
    return;
  }

  const EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel = ToBltPixel(Background);
  for (UINTN Index = 0; Index < PixelCount_; ++Index) {
    BackBuffer_[Index] = Pixel;
  }
}

void GopRenderer::PutPixel(
    const UINTN X,
    const UINTN Y,
    const RgbColor Color) noexcept {
  if (LogicalCanvasEnabled_) {
    UINTN PhysicalX = 0U;
    UINTN PhysicalY = 0U;
    if (!LogicalCanvas::MapPoint(
            Viewport_, X, Y, &PhysicalX, &PhysicalY) ||
        (PhysicalX >= (Viewport_.X + Viewport_.Width)) ||
        (PhysicalY >= (Viewport_.Y + Viewport_.Height))) {
      return;
    }
    PutPhysicalPixel(PhysicalX, PhysicalY, Color);
    return;
  }
  PutPhysicalPixel(X, Y, Color);
}

void GopRenderer::PutPhysicalPixel(
    const UINTN X,
    const UINTN Y,
    const RgbColor Color) noexcept {
  if ((BackBuffer_ == nullptr) || (X >= Width_) || (Y >= Height_)) {
    return;
  }
  BackBuffer_[(Y * Width_) + X] = ToBltPixel(Color);
}

void GopRenderer::BlendPixel(
    const UINTN X,
    const UINTN Y,
    const RgbaColor Color) noexcept {
  if (LogicalCanvasEnabled_) {
    UINTN PhysicalX = 0U;
    UINTN PhysicalY = 0U;
    if (!LogicalCanvas::MapPoint(
            Viewport_, X, Y, &PhysicalX, &PhysicalY) ||
        (PhysicalX >= (Viewport_.X + Viewport_.Width)) ||
        (PhysicalY >= (Viewport_.Y + Viewport_.Height))) {
      return;
    }
    BlendPhysicalPixel(PhysicalX, PhysicalY, Color);
    return;
  }
  BlendPhysicalPixel(X, Y, Color);
}

void GopRenderer::BlendPhysicalPixel(
    const UINTN X,
    const UINTN Y,
    const RgbaColor Color) noexcept {
  if ((BackBuffer_ == nullptr) || (X >= Width_) || (Y >= Height_) ||
      (Color.Alpha == 0U)) {
    return;
  }
  if (Color.Alpha == 255U) {
    PutPhysicalPixel(X, Y, {Color.Red, Color.Green, Color.Blue});
    return;
  }
  const UINTN Index = (Y * Width_) + X;
  BackBuffer_[Index] = ToBltPixel(
      BlendColor(FromBltPixel(BackBuffer_[Index]), Color));
}

void GopRenderer::DrawText(
    const CHAR8* Text,
    const UINTN X,
    const UINTN Y,
    const UINTN Scale,
    const RgbColor Color) noexcept {
  constexpr UINTN kCellUnits =
      font5x7::kGlyphWidth + font5x7::kGlyphSpacing;
  if ((Text == nullptr) || (Scale == 0) || (BackBuffer_ == nullptr) ||
      (Scale > (MAX_UINTN / kCellUnits)) ||
      (X >= Width()) || (Y >= Height())) {
    return;
  }

  const UINTN CellWidth = kCellUnits * Scale;
  UINTN CursorX = X;
  for (UINTN CharacterIndex = 0; Text[CharacterIndex] != '\0';
       ++CharacterIndex) {
    const UINT8* Glyph = font5x7::FindGlyph(Text[CharacterIndex]);
    if (Glyph != nullptr) {
      for (UINTN Row = 0; Row < font5x7::kGlyphHeight; ++Row) {
        if ((Row > ((MAX_UINTN - Y) / Scale)) ||
            ((Y + (Row * Scale)) >= Height())) {
          break;
        }
        for (UINTN Column = 0; Column < font5x7::kGlyphWidth; ++Column) {
          if ((Column > ((MAX_UINTN - CursorX) / Scale)) ||
              ((CursorX + (Column * Scale)) >= Width())) {
            break;
          }
          const UINT8 Mask = static_cast<UINT8>(
              1U << (font5x7::kGlyphWidth - Column - 1U));
          if ((Glyph[Row] & Mask) != 0) {
            FillRectangle(
                CursorX + (Column * Scale),
                Y + (Row * Scale),
                Scale,
                Scale,
                Color);
          }
        }
      }
    }

    if (CursorX > (MAX_UINTN - CellWidth)) {
      break;
    }
    CursorX += CellWidth;
    if (CursorX >= Width()) {
      break;
    }
  }
}

void GopRenderer::DrawTextAligned(
    const CHAR8* Text,
    const UINTN AnchorX,
    const UINTN Y,
    const UINTN Scale,
    const RgbColor Color,
    const TextAlignment Alignment) noexcept {
  const UINTN TextWidth = MeasureText(Text, Scale);
  if (TextWidth == 0U) {
    return;
  }

  UINTN X = AnchorX;
  if (Alignment == TextAlignment::Center) {
    X = (AnchorX >= (TextWidth / 2U)) ? (AnchorX - (TextWidth / 2U)) : 0U;
  } else if (Alignment == TextAlignment::Right) {
    X = (AnchorX >= TextWidth) ? (AnchorX - TextWidth) : 0U;
  }
  DrawText(Text, X, Y, Scale, Color);
}

UINTN GopRenderer::MeasureText(
    const CHAR8* Text,
    const UINTN Scale) const noexcept {
  if ((Text == nullptr) || (Scale == 0)) {
    return 0;
  }

  constexpr UINTN kCellUnits =
      font5x7::kGlyphWidth + font5x7::kGlyphSpacing;
  if (Scale > (MAX_UINTN / kCellUnits)) {
    return 0U;
  }

  UINTN CharacterCount = 0;
  while (Text[CharacterCount] != '\0') {
    ++CharacterCount;
  }

  if (CharacterCount == 0) {
    return 0;
  }

  const UINTN CellWidth =
      kCellUnits * Scale;
  if (CharacterCount > (MAX_UINTN / CellWidth)) {
    return 0U;
  }
  return (CharacterCount * CellWidth) - (font5x7::kGlyphSpacing * Scale);
}

EFI_STATUS GopRenderer::Present() noexcept {
  if ((GraphicsOutput_ == nullptr) || (BackBuffer_ == nullptr)) {
    return EFI_NOT_READY;
  }

  return GraphicsOutput_->Blt(
      GraphicsOutput_,
      BackBuffer_,
      EfiBltBufferToVideo,
      0,
      0,
      0,
      0,
      Width_,
      Height_,
      Width_ * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
}

void GopRenderer::FillRectangle(
    const UINTN X,
    const UINTN Y,
    const UINTN RectangleWidth,
    const UINTN RectangleHeight,
    const RgbColor Color) noexcept {
  if (LogicalCanvasEnabled_) {
    if ((RectangleWidth == 0U) || (RectangleHeight == 0U) ||
        (X >= kReferenceCanvasWidth) || (Y >= kReferenceCanvasHeight)) {
      return;
    }
    const UINTN ClippedLogicalWidth =
        (RectangleWidth > (kReferenceCanvasWidth - X))
            ? (kReferenceCanvasWidth - X)
            : RectangleWidth;
    const UINTN ClippedLogicalHeight =
        (RectangleHeight > (kReferenceCanvasHeight - Y))
            ? (kReferenceCanvasHeight - Y)
            : RectangleHeight;
    UINTN PhysicalX = 0U;
    UINTN PhysicalY = 0U;
    UINTN PhysicalRectangleWidth = 0U;
    UINTN PhysicalRectangleHeight = 0U;
    if (!LogicalCanvas::MapRectangle(
            Viewport_,
            X,
            Y,
            ClippedLogicalWidth,
            ClippedLogicalHeight,
            &PhysicalX,
            &PhysicalY,
            &PhysicalRectangleWidth,
            &PhysicalRectangleHeight)) {
      return;
    }
    FillPhysicalRectangle(
        PhysicalX,
        PhysicalY,
        PhysicalRectangleWidth,
        PhysicalRectangleHeight,
        Color);
    return;
  }
  FillPhysicalRectangle(X, Y, RectangleWidth, RectangleHeight, Color);
}

void GopRenderer::FillPhysicalRectangle(
    const UINTN X,
    const UINTN Y,
    const UINTN RectangleWidth,
    const UINTN RectangleHeight,
    const RgbColor Color) noexcept {
  if ((BackBuffer_ == nullptr) || (RectangleWidth == 0) ||
      (RectangleHeight == 0) || (X >= Width_) || (Y >= Height_)) {
    return;
  }

  const UINTN ClippedWidth =
      (RectangleWidth > (Width_ - X)) ? (Width_ - X) : RectangleWidth;
  const UINTN ClippedHeight =
      (RectangleHeight > (Height_ - Y)) ? (Height_ - Y) : RectangleHeight;
  const EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel = ToBltPixel(Color);

  for (UINTN Row = 0; Row < ClippedHeight; ++Row) {
    const UINTN RowStart = ((Y + Row) * Width_) + X;
    for (UINTN Column = 0; Column < ClippedWidth; ++Column) {
      BackBuffer_[RowStart + Column] = Pixel;
    }
  }
}

void GopRenderer::FillRectangleAlpha(
    const UINTN X,
    const UINTN Y,
    const UINTN RectangleWidth,
    const UINTN RectangleHeight,
    const RgbaColor Color) noexcept {
  if ((BackBuffer_ == nullptr) || (RectangleWidth == 0U) ||
      (RectangleHeight == 0U) || (Color.Alpha == 0U)) {
    return;
  }
  if (Color.Alpha == 255U) {
    FillRectangle(
        X,
        Y,
        RectangleWidth,
        RectangleHeight,
        {Color.Red, Color.Green, Color.Blue});
    return;
  }

  UINTN PhysicalX = X;
  UINTN PhysicalY = Y;
  UINTN ClippedWidth = RectangleWidth;
  UINTN ClippedHeight = RectangleHeight;
  if (LogicalCanvasEnabled_) {
    if ((X >= kReferenceCanvasWidth) || (Y >= kReferenceCanvasHeight)) {
      return;
    }
    const UINTN LogicalWidth =
        (RectangleWidth > (kReferenceCanvasWidth - X))
            ? (kReferenceCanvasWidth - X)
            : RectangleWidth;
    const UINTN LogicalHeight =
        (RectangleHeight > (kReferenceCanvasHeight - Y))
            ? (kReferenceCanvasHeight - Y)
            : RectangleHeight;
    if (!LogicalCanvas::MapRectangle(
            Viewport_,
            X,
            Y,
            LogicalWidth,
            LogicalHeight,
            &PhysicalX,
            &PhysicalY,
            &ClippedWidth,
            &ClippedHeight)) {
      return;
    }
  } else {
    if ((X >= Width_) || (Y >= Height_)) {
      return;
    }
    ClippedWidth =
        (RectangleWidth > (Width_ - X)) ? (Width_ - X) : RectangleWidth;
    ClippedHeight =
        (RectangleHeight > (Height_ - Y)) ? (Height_ - Y) : RectangleHeight;
  }
  for (UINTN Row = 0U; Row < ClippedHeight; ++Row) {
    for (UINTN Column = 0U; Column < ClippedWidth; ++Column) {
      BlendPhysicalPixel(PhysicalX + Column, PhysicalY + Row, Color);
    }
  }
}

void GopRenderer::DrawRectangle(
    const UINTN X,
    const UINTN Y,
    const UINTN RectangleWidth,
    const UINTN RectangleHeight,
    const UINTN Thickness,
    const RgbColor Color) noexcept {
  const UINTN CoordinateWidth = Width();
  const UINTN CoordinateHeight = Height();
  if ((RectangleWidth == 0U) || (RectangleHeight == 0U) ||
      (Thickness == 0U) || (X >= CoordinateWidth) ||
      (Y >= CoordinateHeight)) {
    return;
  }
  const UINTN ClippedWidth =
      (RectangleWidth > (CoordinateWidth - X))
          ? (CoordinateWidth - X)
          : RectangleWidth;
  const UINTN ClippedHeight =
      (RectangleHeight > (CoordinateHeight - Y))
          ? (CoordinateHeight - Y)
          : RectangleHeight;
  const UINTN HorizontalThickness =
      (Thickness > ClippedHeight) ? ClippedHeight : Thickness;
  const UINTN VerticalThickness =
      (Thickness > ClippedWidth) ? ClippedWidth : Thickness;
  FillRectangle(X, Y, ClippedWidth, HorizontalThickness, Color);
  if (ClippedHeight > HorizontalThickness) {
    FillRectangle(
        X,
        Y + ClippedHeight - HorizontalThickness,
        ClippedWidth,
        HorizontalThickness,
        Color);
  }
  FillRectangle(X, Y, VerticalThickness, ClippedHeight, Color);
  if (ClippedWidth > VerticalThickness) {
    FillRectangle(
        X + ClippedWidth - VerticalThickness,
        Y,
        VerticalThickness,
        ClippedHeight,
        Color);
  }
}

void GopRenderer::FillGradient(
    const UINTN X,
    const UINTN Y,
    const UINTN RectangleWidth,
    const UINTN RectangleHeight,
    const RgbColor Start,
    const RgbColor End,
    const GradientDirection Direction) noexcept {
  if ((BackBuffer_ == nullptr) || (RectangleWidth == 0U) ||
      (RectangleHeight == 0U)) {
    return;
  }
  UINTN PhysicalX = X;
  UINTN PhysicalY = Y;
  UINTN ClippedWidth = RectangleWidth;
  UINTN ClippedHeight = RectangleHeight;
  if (LogicalCanvasEnabled_) {
    if ((X >= kReferenceCanvasWidth) || (Y >= kReferenceCanvasHeight)) {
      return;
    }
    const UINTN LogicalWidth =
        (RectangleWidth > (kReferenceCanvasWidth - X))
            ? (kReferenceCanvasWidth - X)
            : RectangleWidth;
    const UINTN LogicalHeight =
        (RectangleHeight > (kReferenceCanvasHeight - Y))
            ? (kReferenceCanvasHeight - Y)
            : RectangleHeight;
    if (!LogicalCanvas::MapRectangle(
            Viewport_,
            X,
            Y,
            LogicalWidth,
            LogicalHeight,
            &PhysicalX,
            &PhysicalY,
            &ClippedWidth,
            &ClippedHeight)) {
      return;
    }
  } else {
    if ((X >= Width_) || (Y >= Height_)) {
      return;
    }
    ClippedWidth =
        (RectangleWidth > (Width_ - X)) ? (Width_ - X) : RectangleWidth;
    ClippedHeight =
        (RectangleHeight > (Height_ - Y)) ? (Height_ - Y) : RectangleHeight;
  }
  const UINTN Maximum =
      (Direction == GradientDirection::Horizontal)
          ? ((ClippedWidth > 1U) ? (ClippedWidth - 1U) : 0U)
          : ((ClippedHeight > 1U) ? (ClippedHeight - 1U) : 0U);
  for (UINTN Row = 0U; Row < ClippedHeight; ++Row) {
    for (UINTN Column = 0U; Column < ClippedWidth; ++Column) {
      const UINTN Position =
          (Direction == GradientDirection::Horizontal) ? Column : Row;
      PutPhysicalPixel(
          PhysicalX + Column,
          PhysicalY + Row,
          InterpolateColor(Start, End, Position, Maximum));
    }
  }
}

void GopRenderer::DrawLine(
    const UINTN StartX,
    const UINTN StartY,
    const UINTN EndX,
    const UINTN EndY,
    const UINTN Thickness,
    const RgbColor Color) noexcept {
  if ((Thickness == 0) || (BackBuffer_ == nullptr)) {
    return;
  }

  if (LogicalCanvasEnabled_) {
    UINTN PhysicalStartX = 0U;
    UINTN PhysicalStartY = 0U;
    UINTN PhysicalEndX = 0U;
    UINTN PhysicalEndY = 0U;
    if (!LogicalCanvas::MapPoint(
            Viewport_, StartX, StartY, &PhysicalStartX, &PhysicalStartY) ||
        !LogicalCanvas::MapPoint(
            Viewport_, EndX, EndY, &PhysicalEndX, &PhysicalEndY)) {
      return;
    }
    const UINTN PhysicalThickness =
        LogicalCanvas::ScaleLength(Viewport_, Thickness);
    DrawPhysicalLine(
        PhysicalStartX,
        PhysicalStartY,
        PhysicalEndX,
        PhysicalEndY,
        PhysicalThickness,
        Color);
    return;
  }
  DrawPhysicalLine(StartX, StartY, EndX, EndY, Thickness, Color);
}

void GopRenderer::DrawPhysicalLine(
    const UINTN StartX,
    const UINTN StartY,
    const UINTN EndX,
    const UINTN EndY,
    const UINTN Thickness,
    const RgbColor Color) noexcept {
  if ((Thickness == 0U) || (BackBuffer_ == nullptr)) {
    return;
  }

  constexpr UINTN kMaximumIntn = MAX_UINTN >> 1U;
  if ((StartX > kMaximumIntn) || (StartY > kMaximumIntn) ||
      (EndX > kMaximumIntn) || (EndY > kMaximumIntn)) {
    return;
  }

  INTN CurrentX = static_cast<INTN>(StartX);
  INTN CurrentY = static_cast<INTN>(StartY);
  const INTN TargetX = static_cast<INTN>(EndX);
  const INTN TargetY = static_cast<INTN>(EndY);
  const INTN DeltaX =
      (TargetX >= CurrentX) ? (TargetX - CurrentX) : (CurrentX - TargetX);
  const INTN StepX = (CurrentX < TargetX) ? 1 : -1;
  const INTN AbsoluteDeltaY =
      (TargetY >= CurrentY) ? (TargetY - CurrentY) : (CurrentY - TargetY);
  const INTN DeltaY = -AbsoluteDeltaY;
  const INTN StepY = (CurrentY < TargetY) ? 1 : -1;
  INTN Error = DeltaX + DeltaY;
  const UINTN HalfThickness = Thickness / 2U;

  for (;;) {
    const UINTN PixelX = static_cast<UINTN>(CurrentX);
    const UINTN PixelY = static_cast<UINTN>(CurrentY);
    const UINTN RectangleX =
        (PixelX >= HalfThickness) ? (PixelX - HalfThickness) : 0;
    const UINTN RectangleY =
        (PixelY >= HalfThickness) ? (PixelY - HalfThickness) : 0;
    FillPhysicalRectangle(
        RectangleX,
        RectangleY,
        Thickness,
        Thickness,
        Color);

    if ((CurrentX == TargetX) && (CurrentY == TargetY)) {
      break;
    }

    const INTN DoubledError = 2 * Error;
    if (DoubledError >= DeltaY) {
      Error += DeltaY;
      CurrentX += StepX;
    }

    if (DoubledError <= DeltaX) {
      Error += DeltaX;
      CurrentY += StepY;
    }
  }
}

void GopRenderer::DrawMonochromeBitmap(
    const UINT8* Data,
    const UINTN SourceWidth,
    const UINTN SourceHeight,
    const UINTN BytesPerRow,
    const UINTN X,
    const UINTN Y,
    const UINTN Scale,
    const RgbColor Color) noexcept {
  if ((Data == nullptr) || (SourceWidth == 0) || (SourceHeight == 0) ||
      (SourceWidth > (MAX_UINTN - 7U)) ||
      (BytesPerRow < ((SourceWidth + 7U) / 8U)) || (Scale == 0) ||
      (SourceWidth > (MAX_UINTN / Scale)) ||
      (SourceHeight > (MAX_UINTN / Scale)) ||
      (SourceHeight > (MAX_UINTN / BytesPerRow)) ||
      (BackBuffer_ == nullptr) || (X >= Width()) || (Y >= Height())) {
    return;
  }

  for (UINTN Row = 0; Row < SourceHeight; ++Row) {
    UINTN Column = 0;
    while (Column < SourceWidth) {
      const UINT8 Byte = Data[(Row * BytesPerRow) + (Column / 8U)];
      const UINT8 Bit = static_cast<UINT8>(0x80U >> (Column % 8U));
      if ((Byte & Bit) == 0) {
        ++Column;
        continue;
      }

      const UINTN RunStart = Column;
      do {
        ++Column;
        if (Column >= SourceWidth) {
          break;
        }
        const UINT8 RunByte =
            Data[(Row * BytesPerRow) + (Column / 8U)];
        const UINT8 RunBit =
            static_cast<UINT8>(0x80U >> (Column % 8U));
        if ((RunByte & RunBit) == 0) {
          break;
        }
      } while (true);

      FillRectangle(
          X + (RunStart * Scale),
          Y + (Row * Scale),
          (Column - RunStart) * Scale,
          Scale,
          Color);
    }
  }
}

}  // namespace apex32
