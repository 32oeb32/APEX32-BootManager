#pragma once

#include <Uefi.h>

struct EFI_DEVICE_PATH_PROTOCOL {
  UINT8 Type;
  UINT8 SubType;
  UINT8 Length[2];
};
