#include "Menu/CardAnimation.hpp"

namespace apex32 {

UINT8 CardAnimation::Ease(
    const UINTN Step,
    const UINTN TotalSteps) noexcept {
  if (TotalSteps == 0U) {
    return 255U;
  }
  if (Step >= TotalSteps) {
    return 255U;
  }

  // Smoothstep over an integer 0..255 domain. The bounded intermediate
  // values stay below 16.6 million and are safe on every supported X64 build.
  UINTN Linear = 0U;
  UINTN Remainder = 0U;
  const UINTN DistanceToWrap = TotalSteps - Step;
  for (UINTN Part = 0U; Part < 255U; ++Part) {
    if (Remainder >= DistanceToWrap) {
      Remainder -= DistanceToWrap;
      ++Linear;
    } else {
      Remainder += Step;
    }
  }
  constexpr UINTN kScaleSquared = 255U * 255U;
  const UINTN Curvature = (3U * 255U) - (2U * Linear);
  const UINTN Smoothed =
      ((Linear * Linear * Curvature) + (kScaleSquared / 2U)) /
      kScaleSquared;
  return static_cast<UINT8>((Smoothed > 255U) ? 255U : Smoothed);
}

UINT8 CardAnimation::FocusIntensity(
    const UINTN CardIndex,
    const UINTN PreviousFocusedIndex,
    const UINTN FocusedIndex,
    const UINT8 Progress) noexcept {
  if (PreviousFocusedIndex == FocusedIndex) {
    return (CardIndex == FocusedIndex) ? 255U : 0U;
  }
  if (CardIndex == FocusedIndex) {
    return Progress;
  }
  if (CardIndex == PreviousFocusedIndex) {
    return static_cast<UINT8>(255U - Progress);
  }
  return 0U;
}

}  // namespace apex32
