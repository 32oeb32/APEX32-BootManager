#include <Uefi.h>

#include <Guid/GlobalVariable.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/LoadedImage.h>

#define APEX32_TEST_BOOT_NUMBER  0x7A32

STATIC CONST CHAR16  mBootVariableName[] = L"Boot7A32";
STATIC CONST CHAR16  mDescription[]      = L"APEX32 OVMF DEFAULT TEST";
STATIC CONST CHAR16  mApex32Path[]       =
  L"\\EFI\\APEX32\\Apex32BootManager.efi";

STATIC
EFI_STATUS
WriteApex32BootOption (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS                 Status;
  EFI_LOADED_IMAGE_PROTOCOL  *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL   *FilePath;
  UINTN                      FilePathSize;
  UINTN                      DescriptionSize;
  UINTN                      LoadOptionSize;
  UINT8                      *LoadOption;
  UINT8                      *Cursor;
  UINT32                     Attributes;
  UINT16                     FilePathSize16;

  LoadedImage = NULL;
  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  FilePath = FileDevicePath (LoadedImage->DeviceHandle, (CHAR16 *)mApex32Path);
  if (FilePath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  FilePathSize = GetDevicePathSize (FilePath);
  if ((FilePathSize == 0) || (FilePathSize > 0xFFFFU)) {
    FreePool (FilePath);
    return EFI_BAD_BUFFER_SIZE;
  }

  DescriptionSize = sizeof (mDescription);
  LoadOptionSize   = sizeof (UINT32) + sizeof (UINT16) +
                     DescriptionSize + FilePathSize;
  LoadOption = AllocateZeroPool (LoadOptionSize);
  if (LoadOption == NULL) {
    FreePool (FilePath);
    return EFI_OUT_OF_RESOURCES;
  }

  Attributes     = LOAD_OPTION_ACTIVE;
  FilePathSize16 = (UINT16)FilePathSize;
  Cursor         = LoadOption;
  CopyMem (Cursor, &Attributes, sizeof (Attributes));
  Cursor += sizeof (Attributes);
  CopyMem (Cursor, &FilePathSize16, sizeof (FilePathSize16));
  Cursor += sizeof (FilePathSize16);
  CopyMem (Cursor, mDescription, DescriptionSize);
  Cursor += DescriptionSize;
  CopyMem (Cursor, FilePath, FilePathSize);

  Status = gRT->SetVariable (
                  (CHAR16 *)mBootVariableName,
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_NON_VOLATILE |
                  EFI_VARIABLE_BOOTSERVICE_ACCESS |
                  EFI_VARIABLE_RUNTIME_ACCESS,
                  LoadOptionSize,
                  LoadOption
                  );

  FreePool (LoadOption);
  FreePool (FilePath);
  return Status;
}

STATIC
EFI_STATUS
PromoteApex32BootOption (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       ExistingSize;
  UINT16      *ExistingOrder;
  UINT16      *NewOrder;
  UINTN       ExistingCount;
  UINTN       NewCount;
  UINTN       Index;
  UINTN       VerifySize;
  UINT16      *VerifyOrder;

  ExistingSize  = 0;
  ExistingOrder = NULL;
  Status = gRT->GetVariable (
                  L"BootOrder",
                  &gEfiGlobalVariableGuid,
                  NULL,
                  &ExistingSize,
                  NULL
                  );
  if (Status == EFI_NOT_FOUND) {
    ExistingSize = 0;
  } else if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  if ((ExistingSize % sizeof (UINT16)) != 0) {
    return EFI_COMPROMISED_DATA;
  }

  if (ExistingSize > 0) {
    ExistingOrder = AllocatePool (ExistingSize);
    if (ExistingOrder == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    Status = gRT->GetVariable (
                    L"BootOrder",
                    &gEfiGlobalVariableGuid,
                    NULL,
                    &ExistingSize,
                    ExistingOrder
                    );
    if (EFI_ERROR (Status)) {
      FreePool (ExistingOrder);
      return Status;
    }
  }

  ExistingCount = ExistingSize / sizeof (UINT16);
  NewOrder = AllocateZeroPool (ExistingSize + sizeof (UINT16));
  if (NewOrder == NULL) {
    if (ExistingOrder != NULL) {
      FreePool (ExistingOrder);
    }

    return EFI_OUT_OF_RESOURCES;
  }

  NewOrder[0] = APEX32_TEST_BOOT_NUMBER;
  NewCount    = 1;
  for (Index = 0; Index < ExistingCount; ++Index) {
    if (ExistingOrder[Index] != APEX32_TEST_BOOT_NUMBER) {
      NewOrder[NewCount++] = ExistingOrder[Index];
    }
  }

  Status = gRT->SetVariable (
                  L"BootOrder",
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_NON_VOLATILE |
                  EFI_VARIABLE_BOOTSERVICE_ACCESS |
                  EFI_VARIABLE_RUNTIME_ACCESS,
                  NewCount * sizeof (UINT16),
                  NewOrder
                  );

  FreePool (NewOrder);
  if (ExistingOrder != NULL) {
    FreePool (ExistingOrder);
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }

  VerifySize  = 0;
  VerifyOrder = NULL;
  Status = gRT->GetVariable (
                  L"BootOrder",
                  &gEfiGlobalVariableGuid,
                  NULL,
                  &VerifySize,
                  NULL
                  );
  if ((Status != EFI_BUFFER_TOO_SMALL) || (VerifySize < sizeof (UINT16))) {
    return EFI_ERROR (Status) ? Status : EFI_DEVICE_ERROR;
  }

  VerifyOrder = AllocatePool (VerifySize);
  if (VerifyOrder == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gRT->GetVariable (
                  L"BootOrder",
                  &gEfiGlobalVariableGuid,
                  NULL,
                  &VerifySize,
                  VerifyOrder
                  );
  if (!EFI_ERROR (Status) &&
      (VerifyOrder[0] != APEX32_TEST_BOOT_NUMBER)) {
    Status = EFI_DEVICE_ERROR;
  }

  FreePool (VerifyOrder);
  return Status;
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  (VOID)SystemTable;

  Status = WriteApex32BootOption (ImageHandle);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PromoteApex32BootOption ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
  CpuDeadLoop ();
  return EFI_DEVICE_ERROR;
}
