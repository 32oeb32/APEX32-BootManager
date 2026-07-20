#pragma once

#include "Renderer/Color.hpp"

namespace apex32::theme {

inline constexpr RgbColor kBlack{0x00, 0x00, 0x00};
inline constexpr RgbColor kBackground{0x02, 0x08, 0x0C};
inline constexpr RgbColor kGrid{0x0A, 0x22, 0x2A};
inline constexpr RgbColor kCircuit{0x0D, 0x3C, 0x48};
inline constexpr RgbColor kPanel{0x04, 0x10, 0x16};
inline constexpr RgbColor kPanelFocused{0x06, 0x1A, 0x22};
inline constexpr RgbColor kPanelLine{0x19, 0x45, 0x50};
inline constexpr RgbColor kPanelInset{0x0B, 0x2B, 0x34};
inline constexpr RgbColor kCyanGlow{0x0B, 0x64, 0x78};
inline constexpr RgbColor kCyanDim{0x16, 0x7A, 0x8B};
inline constexpr RgbColor kCyan{0x21, 0xD4, 0xEA};
inline constexpr RgbColor kCyanCore{0xA8, 0xF7, 0xFF};
inline constexpr RgbColor kRedGlow{0x61, 0x1C, 0x16};
inline constexpr RgbColor kRed{0xEF, 0x4D, 0x32};
inline constexpr RgbColor kPrimaryText{0xD9, 0xFA, 0xFF};
inline constexpr RgbColor kSecondaryText{0x6D, 0xB4, 0xC0};

}  // namespace apex32::theme
