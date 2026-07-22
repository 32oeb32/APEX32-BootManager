#include "Boot/BootDiscovery.hpp"

#include "Boot/FirmwareBootDiscovery.hpp"

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

[[nodiscard]] CHAR16 UpperAscii16(const CHAR16 Character) noexcept {
  return ((Character >= 'a') && (Character <= 'z'))
             ? static_cast<CHAR16>(Character - ('a' - 'A'))
             : Character;
}

[[nodiscard]] BOOLEAN PathsEqual(
    const CHAR16* Left,
    const CHAR16* Right) noexcept {
  if ((Left == nullptr) || (Right == nullptr) ||
      (Left[0] == 0U) || (Right[0] == 0U)) {
    return FALSE;
  }
  UINTN Index = 0U;
  while ((Left[Index] != 0U) && (Right[Index] != 0U)) {
    if (UpperAscii16(Left[Index]) != UpperAscii16(Right[Index])) {
      return FALSE;
    }
    ++Index;
  }
  return ((Left[Index] == 0U) && (Right[Index] == 0U)) ? TRUE : FALSE;
}

[[nodiscard]] BOOLEAN IsDuplicate(
    const BootConfiguration& Configuration,
    const BootEntry& Candidate) noexcept {
  for (UINTN Index = 0U; Index < Configuration.Count; ++Index) {
    if (PathsEqual(
            Configuration.Entries[Index].LoaderPath,
            Candidate.LoaderPath)) {
      return TRUE;
    }
  }
  return FALSE;
}

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

BootConfiguration BootDiscovery::Discover(
    const EFI_HANDLE ParentImageHandle) noexcept {
  BootConfiguration Result{};
  const EFI_STATUS FirmwareStatus = FirmwareBootDiscovery::Discover(&Result);
  Result.FirmwareStatus = FirmwareStatus;

  const BootConfiguration Config = LoadSameEsp(ParentImageHandle);
  Result.ConfigStatus = Config.Status;
  if (!EFI_ERROR(Config.Status)) {
    for (UINTN Index = 0U;
         (Index < Config.Count) && (Result.Count < kMaximumBootEntries);
         ++Index) {
      if (IsDuplicate(Result, Config.Entries[Index])) {
        continue;
      }
      Result.Entries[Result.Count] = Config.Entries[Index];
      ++Result.Count;
      ++Result.ConfigCount;
    }
  }

  Result.Loaded = (!EFI_ERROR(FirmwareStatus) || !EFI_ERROR(Config.Status))
                      ? TRUE
                      : FALSE;
  if (Result.Loaded) {
    Result.Status = EFI_SUCCESS;
  } else {
    Result.Status = (Config.Status != EFI_NOT_FOUND)
                        ? Config.Status
                        : FirmwareStatus;
  }
  return Result;
}

BootConfiguration BootDiscovery::LoadSameEsp(
    const EFI_HANDLE ParentImageHandle) noexcept {
  BootConfiguration Configuration{};
  Configuration.Status = EFI_UNSUPPORTED;
  Configuration.ConfigStatus = EFI_UNSUPPORTED;

  if ((ParentImageHandle == nullptr) || (gBS == nullptr) ||
      (gBS->HandleProtocol == nullptr)) {
    Configuration.ConfigStatus = Configuration.Status;
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
    Configuration.ConfigStatus = Configuration.Status;
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
    Configuration.ConfigStatus = Configuration.Status;
    return Configuration;
  }

  EFI_FILE_PROTOCOL* Root = nullptr;
  Status = FileSystem->OpenVolume(FileSystem, &Root);
  if (EFI_ERROR(Status) || (Root == nullptr) || (Root->Open == nullptr)) {
    if ((Root != nullptr) && (Root->Close != nullptr)) {
      (void)Root->Close(Root);
    }
    Configuration.Status = EFI_ERROR(Status) ? Status : EFI_UNSUPPORTED;
    Configuration.ConfigStatus = Configuration.Status;
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
    Configuration.ConfigStatus = Configuration.Status;
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
    Configuration.ConfigStatus = Configuration.Status;
    return Configuration;
  }
  Buffer[BufferSize] = '\0';

  Status = BootConfig::ParseAscii(Buffer, BufferSize, &Configuration);
  if (EFI_ERROR(Status)) {
    if (Root->Close != nullptr) {
      (void)Root->Close(Root);
    }
    Configuration.Status = Status;
    Configuration.ConfigStatus = Configuration.Status;
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
  Configuration.ConfigStatus = EFI_SUCCESS;
  Configuration.ConfigCount = Configuration.Count;
  return Configuration;
}

}  // namespace apex32
