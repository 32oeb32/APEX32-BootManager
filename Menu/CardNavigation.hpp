#pragma once

#include "UefiCompat.hpp"

namespace apex32 {

enum class CardNavigationAction : UINT8 {
  Previous,
  Next,
  First,
  Last,
  PreviousPage,
  NextPage,
};

class CardNavigation final {
 public:
  [[nodiscard]] static UINTN Apply(
      UINTN FocusedIndex,
      UINTN EntryCount,
      CardNavigationAction Action) noexcept;
};

}  // namespace apex32
