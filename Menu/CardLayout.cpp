#include "Menu/CardLayout.hpp"

#include "Renderer/LogicalCanvas.hpp"

namespace apex32 {

CardPageLayout CardLayout::Calculate(
    const UINTN TotalEntries,
    const UINTN FocusedIndex) noexcept {
  CardPageLayout Layout{};
  if ((TotalEntries > 0U) && (FocusedIndex >= TotalEntries)) {
    return Layout;
  }

  Layout.Valid = TRUE;
  if (TotalEntries == 0U) {
    return Layout;
  }

  Layout.VisibleStart = (FocusedIndex / kCardsPerPage) * kCardsPerPage;
  const UINTN Remaining = TotalEntries - Layout.VisibleStart;
  Layout.VisibleCount =
      (Remaining > kCardsPerPage) ? kCardsPerPage : Remaining;

  constexpr UINTN kCardWidth = 800U;
  constexpr UINTN kWideCardHeight = 460U;
  constexpr UINTN kCompactCardHeight = 220U;
  constexpr UINTN kColumnGap = 80U;
  constexpr UINTN kRowGap = 40U;
  constexpr UINTN kWideY = 280U;
  constexpr UINTN kCompactY = 260U;
  constexpr UINTN kTwoCardWidth = (2U * kCardWidth) + kColumnGap;
  constexpr UINTN kLeftX =
      (kReferenceCanvasWidth - kTwoCardWidth) / 2U;
  constexpr UINTN kCenteredX =
      (kReferenceCanvasWidth - kCardWidth) / 2U;

  Layout.Compact = (Layout.VisibleCount > 2U) ? TRUE : FALSE;
  if (Layout.VisibleCount == 1U) {
    Layout.Cards[0] = {
        kCenteredX, kWideY, kCardWidth, kWideCardHeight};
    return Layout;
  }
  if (Layout.VisibleCount == 2U) {
    Layout.Cards[0] = {
        kLeftX, kWideY, kCardWidth, kWideCardHeight};
    Layout.Cards[1] = {
        kLeftX + kCardWidth + kColumnGap,
        kWideY,
        kCardWidth,
        kWideCardHeight};
    return Layout;
  }

  for (UINTN Index = 0U; Index < Layout.VisibleCount; ++Index) {
    const UINTN Row = Index / 2U;
    const UINTN Column = Index % 2U;
    UINTN X = kLeftX + (Column * (kCardWidth + kColumnGap));
    if ((Layout.VisibleCount == 3U) && (Index == 2U)) {
      X = kCenteredX;
    }
    Layout.Cards[Index] = {
        X,
        kCompactY + (Row * (kCompactCardHeight + kRowGap)),
        kCardWidth,
        kCompactCardHeight};
  }
  return Layout;
}

}  // namespace apex32
