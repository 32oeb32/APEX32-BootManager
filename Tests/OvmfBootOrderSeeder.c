#include <Uefi.h>

#include <Guid/GlobalVariable.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/LoadedImage.h>

#define APEX32_TEST_BOOT_NUMBER   0x7A32
#define APEX32_TEST_LINUX_NUMBER  0x7A33
#define APEX32_TEST_WINDOWS_NUMBER  0x7A34
#define APEX32_TEST_UNKNOWN_NUMBER  0x7A35

STATIC CONST CHAR16  mApex32VariableName[] = L"Boot7A32";
STATIC CONST CHAR16  mLinuxVariableName[]  = L"Boot7A33";
STATIC CONST CHAR16  mWindowsVariableName[] = L"Boot7A34";
STATIC CONST CHAR16  mUnknownVariableName[] = L"Boot7A35";
STATIC CONST CHAR16  mApex32Description[] = L"APEX32 OVMF DEFAULT TEST";
STATIC CONST CHAR16  mLinuxDescription[]  = L"Kali Linux Native Entry";
STATIC CONST CHAR16  mWindowsDescription[] = L"Windows Boot Manager";
STATIC CONST CHAR16  mUnknownDescription[] = L"FutureOS Experimental Loader";
STATIC CONST CHAR16  mApex32Path[] =
  L"\\EFI\\APEX32\\Apex32BootManager.efi";
STATIC CONST CHAR16  mLinuxPath[] = L"\\EFI\\kali\\grubx64.efi";
STATIC CONST CHAR16  mWindowsPath[] =
  L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";
STATIC CONST CHAR16  mUnknownPath[] = L"\\EFI\\vendor\\bootx64.efi";

STATIC
EFI_STATUS
WriteBootOption (
  IN EFI_HANDLE    ImageHandle,
  IN CONST CHAR16  *VariableName,
  IN CONST CHAR16  *Description,
  IN CONST CHAR16  *LoaderPath
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

  if ((VariableName == NULL) || (Description == NULL) || (LoaderPath == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  FilePath = FileDevicePath (LoadedImage->DeviceHandle, (CHAR16 *)LoaderPath);
  if (FilePath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  FilePathSize = GetDevicePathSize (FilePath);
  if ((FilePathSize == 0) || (FilePathSize > 0xFFFFU)) {
    FreePool (FilePath);
    return EFI_BAD_BUFFER_SIZE;
  }

  DescriptionSize = StrSize (Description);
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
  CopyMem (Cursor, Description, DescriptionSize);
  Cursor += DescriptionSize;
  CopyMem (Cursor, FilePath, FilePathSize);

  Status = gRT->SetVariable (
                  (CHAR16 *)VariableName,
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
  STATIC CONST UINT16  DesiredOrder[] = {
    APEX32_TEST_BOOT_NUMBER,
    APEX32_TEST_LINUX_NUMBER,
    APEX32_TEST_WINDOWS_NUMBER,
    APEX32_TEST_UNKNOWN_NUMBER
  };

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
  NewOrder = AllocateZeroPool (ExistingSize + sizeof (DesiredOrder));
  if (NewOrder == NULL) {
    if (ExistingOrder != NULL) {
      FreePool (ExistingOrder);
    }

    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (NewOrder, DesiredOrder, sizeof (DesiredOrder));
  NewCount = sizeof (DesiredOrder) / sizeof (DesiredOrder[0]);
  for (Index = 0; Index < ExistingCount; ++Index) {
    if ((ExistingOrder[Index] != APEX32_TEST_BOOT_NUMBER) &&
        (ExistingOrder[Index] != APEX32_TEST_LINUX_NUMBER) &&
        (ExistingOrder[Index] != APEX32_TEST_WINDOWS_NUMBER) &&
        (ExistingOrder[Index] != APEX32_TEST_UNKNOWN_NUMBER)) {
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

  Status = WriteBootOption (
             ImageHandle,
             mApex32VariableName,
             mApex32Description,
             mApex32Path
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = WriteBootOption (
             ImageHandle,
             mLinuxVariableName,
             mLinuxDescription,
             mLinuxPath
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = WriteBootOption (
             ImageHandle,
             mWindowsVariableName,
             mWindowsDescription,
             mWindowsPath
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = WriteBootOption (
             ImageHandle,
             mUnknownVariableName,
             mUnknownDescription,
             mUnknownPath
             );
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
