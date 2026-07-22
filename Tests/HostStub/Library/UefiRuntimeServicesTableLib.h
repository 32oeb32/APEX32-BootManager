#pragma once

#include <Uefi.h>

struct EFI_RUNTIME_SERVICES {
  EFI_STATUS(EFIAPI* GetVariable)(
      CHAR16*, EFI_GUID*, UINT32*, UINTN*, VOID*);
  EFI_STATUS(EFIAPI* GetNextVariableName)(
      UINTN*, CHAR16*, EFI_GUID*);
};

extern EFI_RUNTIME_SERVICES* gRT;
