#pragma once

#include "UefiCompat.hpp"

namespace apex32 {

inline constexpr UINTN kCardsPerPage = 4U;

struct CardRectangle final {
  UINTN X;
  UINTN Y;
  UINTN Width;
  UINTN Height;
};

struct CardPageLayout final {
  CardRectangle Cards[kCardsPerPage];
  UINTN VisibleStart;
  UINTN VisibleCount;
  BOOLEAN Compact;
  BOOLEAN Valid;
};

class CardLayout final {
 public:
  [[nodiscard]] static CardPageLayout Calculate(
      UINTN TotalEntries,
      UINTN FocusedIndex) noexcept;
};

}  // namespace apex32
