#include <Uefi.h>

#include <Guid/GlobalVariable.h>
#include <Library/IoLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/LoadedImage.h>

#define APEX32_LIFECYCLE_BOOT_NUMBER  0x7A40
#define APEX32_SUCCESS_EXIT_VALUE     0x2A
#define APEX32_DEBUG_EXIT_PORT        0xF4

STATIC CONST CHAR16  mLifecycleVariableName[] = L"Boot7A40";
STATIC CONST CHAR16  mLifecycleDescription[]  = L"APEX32 INSTALLER LIFECYCLE";
STATIC CONST CHAR16  mLifecyclePath[] =
  L"\\EFI\\APEX32\\Apex32BootManager.efi";

STATIC
VOID
ExitQemu (
  IN UINT8  Value
  )
{
  IoWrite8 (APEX32_DEBUG_EXIT_PORT, Value);
  CpuDeadLoop ();
}

STATIC
EFI_STATUS
ReadBootOrder (
  OUT UINT16  **Order,
  OUT UINTN   *OrderSize
  )
{
  EFI_STATUS  Status;

  *Order     = NULL;
  *OrderSize = 0;
  Status = gRT->GetVariable (
                  L"BootOrder",
                  &gEfiGlobalVariableGuid,
                  NULL,
                  OrderSize,
                  NULL
                  );
  if (Status == EFI_NOT_FOUND) {
    return EFI_SUCCESS;
  }

  if ((Status != EFI_BUFFER_TOO_SMALL) ||
      ((*OrderSize % sizeof (UINT16)) != 0)) {
    return EFI_COMPROMISED_DATA;
  }

  *Order = AllocatePool (*OrderSize);
  if (*Order == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gRT->GetVariable (
                  L"BootOrder",
                  &gEfiGlobalVariableGuid,
                  NULL,
                  OrderSize,
                  *Order
                  );
  if (EFI_ERROR (Status)) {
    FreePool (*Order);
    *Order = NULL;
  }

  return Status;
}

STATIC
EFI_STATUS
WriteLifecycleOption (
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
  UINT32                     LoadAttributes;
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

  FilePath = FileDevicePath (LoadedImage->DeviceHandle, (CHAR16 *)mLifecyclePath);
  if (FilePath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  FilePathSize = GetDevicePathSize (FilePath);
  if ((FilePathSize == 0) || (FilePathSize > 0xFFFFU)) {
    FreePool (FilePath);
    return EFI_BAD_BUFFER_SIZE;
  }

  DescriptionSize = StrSize (mLifecycleDescription);
  LoadOptionSize = sizeof (UINT32) + sizeof (UINT16) +
                   DescriptionSize + FilePathSize;
  LoadOption = AllocateZeroPool (LoadOptionSize);
  if (LoadOption == NULL) {
    FreePool (FilePath);
    return EFI_OUT_OF_RESOURCES;
  }

  LoadAttributes  = LOAD_OPTION_ACTIVE;
  FilePathSize16  = (UINT16)FilePathSize;
  Cursor          = LoadOption;
  CopyMem (Cursor, &LoadAttributes, sizeof (LoadAttributes));
  Cursor += sizeof (LoadAttributes);
  CopyMem (Cursor, &FilePathSize16, sizeof (FilePathSize16));
  Cursor += sizeof (FilePathSize16);
  CopyMem (Cursor, mLifecycleDescription, DescriptionSize);
  Cursor += DescriptionSize;
  CopyMem (Cursor, FilePath, FilePathSize);

  Status = gRT->SetVariable (
                  (CHAR16 *)mLifecycleVariableName,
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
PromoteLifecycleOption (
  IN CONST UINT16  *OriginalOrder,
  IN UINTN         OriginalOrderSize
  )
{
  EFI_STATUS  Status;
  UINT16      *NewOrder;
  UINTN       OriginalCount;
  UINTN       NewCount;
  UINTN       Index;

  OriginalCount = OriginalOrderSize / sizeof (UINT16);
  NewOrder = AllocateZeroPool (OriginalOrderSize + sizeof (UINT16));
  if (NewOrder == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  NewOrder[0] = APEX32_LIFECYCLE_BOOT_NUMBER;
  NewCount    = 1;
  for (Index = 0; Index < OriginalCount; ++Index) {
    if (OriginalOrder[Index] != APEX32_LIFECYCLE_BOOT_NUMBER) {
      NewOrder[NewCount++] = OriginalOrder[Index];
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
  return Status;
}

STATIC
EFI_STATUS
RestoreOriginalState (
  IN CONST UINT16  *OriginalOrder,
  IN UINTN         OriginalOrderSize
  )
{
  EFI_STATUS  Status;

  Status = gRT->SetVariable (
                  (CHAR16 *)mLifecycleVariableName,
                  &gEfiGlobalVariableGuid,
                  0,
                  0,
                  NULL
                  );
  if ((Status != EFI_SUCCESS) && (Status != EFI_NOT_FOUND)) {
    return Status;
  }

  Status = gRT->SetVariable (
                  L"BootOrder",
                  &gEfiGlobalVariableGuid,
                  (OriginalOrderSize == 0) ? 0 :
                    (EFI_VARIABLE_NON_VOLATILE |
                     EFI_VARIABLE_BOOTSERVICE_ACCESS |
                     EFI_VARIABLE_RUNTIME_ACCESS),
                  OriginalOrderSize,
                  (VOID *)OriginalOrder
                  );
  if ((OriginalOrderSize == 0) && (Status == EFI_NOT_FOUND)) {
    return EFI_SUCCESS;
  }
  return Status;
}

STATIC
EFI_STATUS
VerifyOriginalState (
  IN CONST UINT16  *OriginalOrder,
  IN UINTN         OriginalOrderSize
  )
{
  EFI_STATUS  Status;
  UINT16      *CurrentOrder;
  UINTN       CurrentOrderSize;
  UINTN       VariableSize;

  VariableSize = 0;
  Status = gRT->GetVariable (
                  (CHAR16 *)mLifecycleVariableName,
                  &gEfiGlobalVariableGuid,
                  NULL,
                  &VariableSize,
                  NULL
                  );
  if (Status != EFI_NOT_FOUND) {
    return EFI_COMPROMISED_DATA;
  }

  CurrentOrder     = NULL;
  CurrentOrderSize = 0;
  Status = ReadBootOrder (&CurrentOrder, &CurrentOrderSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((CurrentOrderSize != OriginalOrderSize) ||
      ((OriginalOrderSize != 0) &&
       (CompareMem (CurrentOrder, OriginalOrder, OriginalOrderSize) != 0))) {
    Status = EFI_COMPROMISED_DATA;
  }

  if (CurrentOrder != NULL) {
    FreePool (CurrentOrder);
  }

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
  UINT16      *OriginalOrder;
  UINTN       OriginalOrderSize;
  UINTN       ExistingSize;

  (VOID)SystemTable;
  OriginalOrder     = NULL;
  OriginalOrderSize = 0;
  ExistingSize      = 0;

  Status = gRT->GetVariable (
                  (CHAR16 *)mLifecycleVariableName,
                  &gEfiGlobalVariableGuid,
                  NULL,
                  &ExistingSize,
                  NULL
                  );
  if (Status != EFI_NOT_FOUND) {
    ExitQemu (0x11);
  }

  Status = ReadBootOrder (&OriginalOrder, &OriginalOrderSize);
  if (EFI_ERROR (Status)) {
    ExitQemu (0x12);
  }

  Status = WriteLifecycleOption (ImageHandle);
  if (EFI_ERROR (Status)) {
    if (OriginalOrder != NULL) {
      FreePool (OriginalOrder);
    }
    ExitQemu (0x13);
  }

  Status = PromoteLifecycleOption (OriginalOrder, OriginalOrderSize);
  if (EFI_ERROR (Status)) {
    (VOID)RestoreOriginalState (OriginalOrder, OriginalOrderSize);
    if (OriginalOrder != NULL) {
      FreePool (OriginalOrder);
    }
    ExitQemu (0x14);
  }

  {
    UINT16  *PromotedOrder;
    UINTN   PromotedOrderSize;

    PromotedOrder     = NULL;
    PromotedOrderSize = 0;
    Status = ReadBootOrder (&PromotedOrder, &PromotedOrderSize);
    if (EFI_ERROR (Status) || (PromotedOrderSize < sizeof (UINT16)) ||
        (PromotedOrder[0] != APEX32_LIFECYCLE_BOOT_NUMBER)) {
      if (PromotedOrder != NULL) {
        FreePool (PromotedOrder);
      }
      (VOID)RestoreOriginalState (OriginalOrder, OriginalOrderSize);
      if (OriginalOrder != NULL) {
        FreePool (OriginalOrder);
      }
      ExitQemu (0x15);
    }
    FreePool (PromotedOrder);
  }

  Status = RestoreOriginalState (OriginalOrder, OriginalOrderSize);
  if (!EFI_ERROR (Status)) {
    Status = VerifyOriginalState (OriginalOrder, OriginalOrderSize);
  }
  if (OriginalOrder != NULL) {
    FreePool (OriginalOrder);
  }
  if (EFI_ERROR (Status)) {
    ExitQemu (0x16);
  }

  ExitQemu (APEX32_SUCCESS_EXIT_VALUE);
  return EFI_DEVICE_ERROR;
}
