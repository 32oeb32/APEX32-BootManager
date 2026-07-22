#pragma once

#include "Config/BootConfig.hpp"
#include "UefiCompat.hpp"

namespace apex32 {

class GopRenderer;

class WorkspaceMenu final {
 public:
  [[nodiscard]] static EFI_STATUS Run(
      GopRenderer& Renderer,
      EFI_SIMPLE_TEXT_INPUT_PROTOCOL* Input,
      EFI_HANDLE ImageHandle) noexcept;

 private:
  [[nodiscard]] static EFI_STATUS Render(
      GopRenderer& Renderer,
      const BootConfiguration& Configuration,
      UINTN FocusedIndex,
      const CHAR8* Notice,
      BOOLEAN NoticeIsError) noexcept;

  [[nodiscard]] static EFI_STATUS RenderFrame(
      GopRenderer& Renderer,
      const BootConfiguration& Configuration,
      UINTN PreviousFocusedIndex,
      UINTN FocusedIndex,
      UINT8 FocusProgress,
      const CHAR8* Notice,
      BOOLEAN NoticeIsError) noexcept;

  [[nodiscard]] static EFI_STATUS AnimateFocus(
      GopRenderer& Renderer,
      const BootConfiguration& Configuration,
      UINTN PreviousFocusedIndex,
      UINTN FocusedIndex) noexcept;

  [[nodiscard]] static EFI_STATUS RenderDiagnostics(
      GopRenderer& Renderer,
      const BootConfiguration& Configuration,
      UINTN FocusedIndex) noexcept;
};

}  // namespace apex32
