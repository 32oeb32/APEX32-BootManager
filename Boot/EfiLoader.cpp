#include "Boot/EfiLoader.hpp"

extern "C" {
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/LoadedImage.h>
}

namespace apex32 {

EfiLaunchResult EfiLoader::LaunchFromSameEsp(
    const EFI_HANDLE ParentImageHandle,
    const CHAR16* LoaderPath) noexcept {
  if ((ParentImageHandle == nullptr) || (LoaderPath == nullptr) ||
      (gBS == nullptr) || (gBS->HandleProtocol == nullptr) ||
      (gBS->LoadImage == nullptr) || (gBS->StartImage == nullptr)) {
    return {EFI_UNSUPPORTED, EfiLaunchStage::ResolveParent};
  }

  EFI_LOADED_IMAGE_PROTOCOL* ParentImage = nullptr;
  EFI_STATUS Status = gBS->HandleProtocol(
      ParentImageHandle,
      &gEfiLoadedImageProtocolGuid,
      reinterpret_cast<VOID**>(&ParentImage));
  if (EFI_ERROR(Status) || (ParentImage == nullptr) ||
      (ParentImage->DeviceHandle == nullptr)) {
    return {
        EFI_ERROR(Status) ? Status : EFI_UNSUPPORTED,
        EfiLaunchStage::ResolveParent,
    };
  }

  EFI_DEVICE_PATH_PROTOCOL* DevicePath =
      FileDevicePath(ParentImage->DeviceHandle, LoaderPath);
  if (DevicePath == nullptr) {
    return {EFI_OUT_OF_RESOURCES, EfiLaunchStage::BuildPath};
  }

  EFI_HANDLE ChildImageHandle = nullptr;
  Status = gBS->LoadImage(
      TRUE,
      ParentImageHandle,
      DevicePath,
      nullptr,
      0,
      &ChildImageHandle);
  FreePool(DevicePath);

  if (EFI_ERROR(Status) || (ChildImageHandle == nullptr)) {
    if ((ChildImageHandle != nullptr) && (gBS->UnloadImage != nullptr)) {
      (void)gBS->UnloadImage(ChildImageHandle);
    }
    return {
        EFI_ERROR(Status) ? Status : EFI_LOAD_ERROR,
        EfiLaunchStage::LoadImage,
    };
  }

  UINTN ExitDataSize = 0;
  CHAR16* ExitData = nullptr;
  Status = gBS->StartImage(
      ChildImageHandle,
      &ExitDataSize,
      &ExitData);

  if (ExitData != nullptr) {
    FreePool(ExitData);
  }

  if (gBS->UnloadImage != nullptr) {
    (void)gBS->UnloadImage(ChildImageHandle);
  }

  return {
      Status,
      EFI_ERROR(Status) ? EfiLaunchStage::StartImage
                        : EfiLaunchStage::Returned,
  };
}

}  // namespace apex32
