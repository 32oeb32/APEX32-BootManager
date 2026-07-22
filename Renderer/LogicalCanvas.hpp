#pragma once

#include "UefiCompat.hpp"

namespace apex32 {

inline constexpr UINTN kReferenceCanvasWidth = 1920U;
inline constexpr UINTN kReferenceCanvasHeight = 1080U;

struct LogicalViewport final {
  UINTN PhysicalWidth;
  UINTN PhysicalHeight;
  UINTN X;
  UINTN Y;
  UINTN Width;
  UINTN Height;
  BOOLEAN Valid;
};

class LogicalCanvas final {
 public:
  [[nodiscard]] static EFI_STATUS CreateViewport(
      UINTN PhysicalWidth,
      UINTN PhysicalHeight,
      LogicalViewport* Viewport) noexcept;

  [[nodiscard]] static BOOLEAN MapPoint(
      const LogicalViewport& Viewport,
      UINTN LogicalX,
      UINTN LogicalY,
      UINTN* PhysicalX,
      UINTN* PhysicalY) noexcept;

  [[nodiscard]] static BOOLEAN MapRectangle(
      const LogicalViewport& Viewport,
      UINTN LogicalX,
      UINTN LogicalY,
      UINTN LogicalWidth,
      UINTN LogicalHeight,
      UINTN* PhysicalX,
      UINTN* PhysicalY,
      UINTN* PhysicalWidth,
      UINTN* PhysicalHeight) noexcept;

  [[nodiscard]] static UINTN ScaleLength(
      const LogicalViewport& Viewport,
      UINTN LogicalLength) noexcept;
};

}  // namespace apex32
