#include "Boot/BootDiscovery.hpp"

extern "C" {
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
}

namespace apex32 {

namespace {

constexpr CHAR16 kConfigurationPath[] = {
    '\\', 'E', 'F', 'I', '\\', 'A', 'P', 'E', 'X', '3', '2', '\\',
    'a', 'p', 'e', 'x', '3', '2', '.', 'c', 'f', 'g', 0,
};

constexpr UINTN kConfigurationBufferCapacity = 16384U;

[[nodiscard]] BOOLEAN FileExists(
    EFI_FILE_PROTOCOL* Root,
    const CHAR16* Path) noexcept {
  if ((Root == nullptr) || (Root->Open == nullptr) || (Path == nullptr)) {
    return FALSE;
  }

  EFI_FILE_PROTOCOL* File = nullptr;
  const EFI_STATUS Status = Root->Open(
      Root,
      &File,
      const_cast<CHAR16*>(Path),
      EFI_FILE_MODE_READ,
      0);
  if (EFI_ERROR(Status) || (File == nullptr)) {
    return FALSE;
  }

  if (File->Close != nullptr) {
    (void)File->Close(File);
  }
  return TRUE;
}

}  // namespace

BootConfiguration BootDiscovery::LoadSameEsp(
    const EFI_HANDLE ParentImageHandle) noexcept {
  BootConfiguration Configuration{};
  Configuration.Status = EFI_UNSUPPORTED;

  if ((ParentImageHandle == nullptr) || (gBS == nullptr) ||
      (gBS->HandleProtocol == nullptr)) {
    return Configuration;
  }

  EFI_LOADED_IMAGE_PROTOCOL* ParentImage = nullptr;
  EFI_STATUS Status = gBS->HandleProtocol(
      ParentImageHandle,
      &gEfiLoadedImageProtocolGuid,
      reinterpret_cast<VOID**>(&ParentImage));
  if (EFI_ERROR(Status) || (ParentImage == nullptr) ||
      (ParentImage->DeviceHandle == nullptr)) {
    Configuration.Status = EFI_ERROR(Status) ? Status : EFI_UNSUPPORTED;
    return Configuration;
  }

  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem = nullptr;
  Status = gBS->HandleProtocol(
      ParentImage->DeviceHandle,
      &gEfiSimpleFileSystemProtocolGuid,
      reinterpret_cast<VOID**>(&FileSystem));
  if (EFI_ERROR(Status) || (FileSystem == nullptr) ||
      (FileSystem->OpenVolume == nullptr)) {
    Configuration.Status = EFI_ERROR(Status) ? Status : EFI_UNSUPPORTED;
    return Configuration;
  }

  EFI_FILE_PROTOCOL* Root = nullptr;
  Status = FileSystem->OpenVolume(FileSystem, &Root);
  if (EFI_ERROR(Status) || (Root == nullptr) || (Root->Open == nullptr)) {
    if ((Root != nullptr) && (Root->Close != nullptr)) {
      (void)Root->Close(Root);
    }
    Configuration.Status = EFI_ERROR(Status) ? Status : EFI_UNSUPPORTED;
    return Configuration;
  }

  EFI_FILE_PROTOCOL* File = nullptr;
  Status = Root->Open(
      Root,
      &File,
      const_cast<CHAR16*>(kConfigurationPath),
      EFI_FILE_MODE_READ,
      0);
  if (EFI_ERROR(Status) || (File == nullptr) || (File->Read == nullptr)) {
    if ((File != nullptr) && (File->Close != nullptr)) {
      (void)File->Close(File);
    }
    if (Root->Close != nullptr) {
      (void)Root->Close(Root);
    }
    Configuration.Status = EFI_ERROR(Status) ? Status : EFI_UNSUPPORTED;
    return Configuration;
  }

  CHAR8 Buffer[kConfigurationBufferCapacity]{};
  UINTN BufferSize = sizeof(Buffer) - 1U;
  Status = File->Read(File, &BufferSize, Buffer);
  if (File->Close != nullptr) {
    (void)File->Close(File);
  }
  if (EFI_ERROR(Status) || (BufferSize >= sizeof(Buffer))) {
    if (Root->Close != nullptr) {
      (void)Root->Close(Root);
    }
    Configuration.Status = EFI_ERROR(Status) ? Status : EFI_BAD_BUFFER_SIZE;
    return Configuration;
  }
  Buffer[BufferSize] = '\0';

  Status = BootConfig::ParseAscii(Buffer, BufferSize, &Configuration);
  if (EFI_ERROR(Status)) {
    if (Root->Close != nullptr) {
      (void)Root->Close(Root);
    }
    Configuration.Status = Status;
    return Configuration;
  }

  for (UINTN Index = 0; Index < Configuration.Count; ++Index) {
    Configuration.Entries[Index].Available = FileExists(
        Root,
        Configuration.Entries[Index].LoaderPath);
  }
  if (Root->Close != nullptr) {
    (void)Root->Close(Root);
  }

  Configuration.Status = EFI_SUCCESS;
  return Configuration;
}

}  // namespace apex32
