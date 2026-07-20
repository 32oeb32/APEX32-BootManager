#pragma once

#include <Protocol/DevicePath.h>

struct EFI_LOADED_IMAGE_PROTOCOL {
  EFI_HANDLE DeviceHandle;
};

extern EFI_GUID gEfiLoadedImageProtocolGuid;
