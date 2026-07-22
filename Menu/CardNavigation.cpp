#include "Menu/CardNavigation.hpp"

#include "Menu/CardLayout.hpp"

namespace apex32 {

UINTN CardNavigation::Apply(
    const UINTN FocusedIndex,
    const UINTN EntryCount,
    const CardNavigationAction Action) noexcept {
  if ((EntryCount == 0U) || (FocusedIndex >= EntryCount)) {
    return 0U;
  }

  switch (Action) {
    case CardNavigationAction::Previous:
      return (FocusedIndex == 0U) ? (EntryCount - 1U)
                                  : (FocusedIndex - 1U);
    case CardNavigationAction::Next:
      return (FocusedIndex == (EntryCount - 1U)) ? 0U
                                                 : (FocusedIndex + 1U);
    case CardNavigationAction::First:
      return 0U;
    case CardNavigationAction::Last:
      return EntryCount - 1U;
    case CardNavigationAction::PreviousPage:
      return (FocusedIndex > kCardsPerPage)
                 ? (FocusedIndex - kCardsPerPage)
                 : 0U;
    case CardNavigationAction::NextPage: {
      const UINTN Remaining = (EntryCount - 1U) - FocusedIndex;
      return FocusedIndex +
             ((Remaining > kCardsPerPage) ? kCardsPerPage : Remaining);
    }
  }
  return FocusedIndex;
}

}  // namespace apex32
