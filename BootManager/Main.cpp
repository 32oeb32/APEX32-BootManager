#include "UefiCompat.hpp"

#include "Animation/IntroAnimation.hpp"
#include "Menu/WorkspaceMenu.hpp"
#include "Renderer/GopRenderer.hpp"

namespace {

constexpr CHAR8 kFailureMessage[] =
    "APEX32 Boot Manager: graphics initialization failed.\r\n";

void SetCursorVisibility(
    EFI_SYSTEM_TABLE* SystemTable,
    const BOOLEAN Visible) noexcept {
  if ((SystemTable != nullptr) && (SystemTable->ConOut != nullptr) &&
      (SystemTable->ConOut->EnableCursor != nullptr)) {
    SystemTable->ConOut->EnableCursor(SystemTable->ConOut, Visible);
  }
}

void ReportFailure(EFI_SYSTEM_TABLE* SystemTable) noexcept {
  if ((SystemTable != nullptr) && (SystemTable->ConOut != nullptr) &&
      (SystemTable->ConOut->OutputString != nullptr)) {
    CHAR16 FailureMessage[sizeof(kFailureMessage)];
    for (UINTN Index = 0; Index < sizeof(kFailureMessage); ++Index) {
      FailureMessage[Index] = static_cast<CHAR16>(kFailureMessage[Index]);
    }

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
    ReportFailure(SystemTable);
  }

  return Status;
}
