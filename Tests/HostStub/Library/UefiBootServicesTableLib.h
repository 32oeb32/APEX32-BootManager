#pragma once

#include <Uefi.h>

struct EFI_BOOT_SERVICES {
  EFI_STATUS(EFIAPI* Stall)(UINTN);
  EFI_STATUS(EFIAPI* LocateProtocol)(VOID*, VOID*, VOID**);
  EFI_STATUS(EFIAPI* WaitForEvent)(UINTN, EFI_EVENT*, UINTN*);
  EFI_STATUS(EFIAPI* HandleProtocol)(EFI_HANDLE, EFI_GUID*, VOID**);
  EFI_STATUS(EFIAPI* LoadImage)(
      BOOLEAN,
      EFI_HANDLE,
      struct EFI_DEVICE_PATH_PROTOCOL*,
      VOID*,
      UINTN,
      EFI_HANDLE*);
  EFI_STATUS(EFIAPI* StartImage)(EFI_HANDLE, UINTN*, CHAR16**);
  EFI_STATUS(EFIAPI* UnloadImage)(EFI_HANDLE);
};

extern EFI_BOOT_SERVICES* gBS;
