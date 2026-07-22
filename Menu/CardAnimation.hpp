#pragma once

#include "UefiCompat.hpp"

namespace apex32 {

inline constexpr UINTN kCardTransitionSteps = 6U;
inline constexpr UINTN kCardTransitionFrameMicroseconds = 16000U;

class CardAnimation final {
 public:
  [[nodiscard]] static UINT8 Ease(
      UINTN Step,
      UINTN TotalSteps) noexcept;

  [[nodiscard]] static UINT8 FocusIntensity(
      UINTN CardIndex,
      UINTN PreviousFocusedIndex,
      UINTN FocusedIndex,
      UINT8 Progress) noexcept;
};

}  // namespace apex32
