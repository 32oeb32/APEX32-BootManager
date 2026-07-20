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

}  // namespace

GopRenderer::GopRenderer() noexcept
    : GraphicsOutput_(nullptr),
      BackBuffer_(nullptr),
      Width_(0),
      Height_(0),
      PixelCount_(0),
      BufferSize_(0) {}

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

  if (Width > (MAX_UINTN / Height)) {
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

  GraphicsOutput_ = GraphicsOutput;
  BackBuffer_ = BackBuffer;
  Width_ = Width;
  Height_ = Height;
  PixelCount_ = PixelCount;
  BufferSize_ = BufferSize;
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
  PixelCount_ = 0;
  BufferSize_ = 0;
}

UINTN GopRenderer::Width() const noexcept {
  return Width_;
}

UINTN GopRenderer::Height() const noexcept {
  return Height_;
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

void GopRenderer::DrawText(
    const CHAR8* Text,
    const UINTN X,
    const UINTN Y,
    const UINTN Scale,
    const RgbColor Color) noexcept {
  if ((Text == nullptr) || (Scale == 0) || (BackBuffer_ == nullptr)) {
    return;
  }

  UINTN CursorX = X;
  for (UINTN CharacterIndex = 0; Text[CharacterIndex] != '\0';
       ++CharacterIndex) {
    const UINT8* Glyph = font5x7::FindGlyph(Text[CharacterIndex]);
    if (Glyph != nullptr) {
      for (UINTN Row = 0; Row < font5x7::kGlyphHeight; ++Row) {
        for (UINTN Column = 0; Column < font5x7::kGlyphWidth; ++Column) {
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

    CursorX += (font5x7::kGlyphWidth + font5x7::kGlyphSpacing) * Scale;
  }
}

UINTN GopRenderer::MeasureText(
    const CHAR8* Text,
    const UINTN Scale) const noexcept {
  if ((Text == nullptr) || (Scale == 0)) {
    return 0;
  }

  UINTN CharacterCount = 0;
  while (Text[CharacterCount] != '\0') {
    ++CharacterCount;
  }

  if (CharacterCount == 0) {
    return 0;
  }

  const UINTN CellWidth =
      (font5x7::kGlyphWidth + font5x7::kGlyphSpacing) * Scale;
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
    FillRectangle(
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
      (BytesPerRow < ((SourceWidth + 7U) / 8U)) || (Scale == 0) ||
      (BackBuffer_ == nullptr)) {
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
