#include "UefiCompat.hpp"

#include "Animation/IntroAnimation.hpp"
#include "Menu/WorkspaceMenu.hpp"
#include "Renderer/GopRenderer.hpp"

namespace {

constexpr CHAR8 kFailurePrefix[] =
    "APEX32 Boot Manager: initialization failed, EFI status 0x";
constexpr CHAR8 kHexDigits[] = "0123456789ABCDEF";

void SetCursorVisibility(
    EFI_SYSTEM_TABLE* SystemTable,
    const BOOLEAN Visible) noexcept {
  if ((SystemTable != nullptr) && (SystemTable->ConOut != nullptr) &&
      (SystemTable->ConOut->EnableCursor != nullptr)) {
    SystemTable->ConOut->EnableCursor(SystemTable->ConOut, Visible);
  }
}

void ReportFailure(
    EFI_SYSTEM_TABLE* SystemTable,
    const EFI_STATUS Status) noexcept {
  if ((SystemTable != nullptr) && (SystemTable->ConOut != nullptr) &&
      (SystemTable->ConOut->OutputString != nullptr)) {
    CHAR16 FailureMessage[96]{};
    UINTN Offset = 0U;
    for (UINTN Index = 0U;
         (kFailurePrefix[Index] != '\0') &&
         ((Offset + 1U) < (sizeof(FailureMessage) / sizeof(FailureMessage[0])));
         ++Index) {
      FailureMessage[Offset++] = static_cast<CHAR16>(kFailurePrefix[Index]);
    }
    for (UINTN Nibble = 0U;
         (Nibble < (sizeof(EFI_STATUS) * 2U)) &&
         ((Offset + 1U) < (sizeof(FailureMessage) / sizeof(FailureMessage[0])));
         ++Nibble) {
      const UINTN Shift =
          ((sizeof(EFI_STATUS) * 2U) - Nibble - 1U) * 4U;
      FailureMessage[Offset++] = static_cast<CHAR16>(
          kHexDigits[(Status >> Shift) & 0xFU]);
    }
    if ((Offset + 2U) <
        (sizeof(FailureMessage) / sizeof(FailureMessage[0]))) {
      FailureMessage[Offset++] = static_cast<CHAR16>('\r');
      FailureMessage[Offset++] = static_cast<CHAR16>('\n');
    }
    FailureMessage[Offset] = 0U;

    SystemTable->ConOut->OutputString(
        SystemTable->ConOut,
        FailureMessage);
  }
}

}  // namespace

extern "C" EFI_STATUS EFIAPI UefiMain(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE* SystemTable) {
  SetCursorVisibility(SystemTable, FALSE);

  apex32::GopRenderer Renderer;
  EFI_STATUS Status = Renderer.Initialize();
  if (!EFI_ERROR(Status)) {
    Status = Renderer.EnableLogicalCanvas();
  }
  if (!EFI_ERROR(Status)) {
    Status = apex32::IntroAnimation::Play(Renderer);
  }
  if (!EFI_ERROR(Status)) {
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL* Input =
        (SystemTable != nullptr) ? SystemTable->ConIn : nullptr;
    Status = apex32::WorkspaceMenu::Run(Renderer, Input, ImageHandle);
  }

  Renderer.Shutdown();
  SetCursorVisibility(SystemTable, TRUE);

  if (EFI_ERROR(Status)) {
    ReportFailure(SystemTable, Status);
  }

  return Status;
}
