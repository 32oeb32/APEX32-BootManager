#pragma once

#include <Protocol/DevicePath.h>

EFI_DEVICE_PATH_PROTOCOL* EFIAPI FileDevicePath(
    EFI_HANDLE Device,
    const CHAR16* FileName);
