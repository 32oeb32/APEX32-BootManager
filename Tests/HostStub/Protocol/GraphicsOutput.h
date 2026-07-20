#pragma once

#include <Uefi.h>

struct EFI_GRAPHICS_OUTPUT_BLT_PIXEL {
  UINT8 Blue;
  UINT8 Green;
  UINT8 Red;
  UINT8 Reserved;
};

enum EFI_GRAPHICS_OUTPUT_BLT_OPERATION {
  EfiBltVideoFill,
  EfiBltVideoToBltBuffer,
  EfiBltBufferToVideo,
  EfiBltVideoToVideo,
  EfiGraphicsOutputBltOperationMax,
};

struct EFI_GRAPHICS_OUTPUT_MODE_INFORMATION {
  UINTN Version;
  UINTN HorizontalResolution;
  UINTN VerticalResolution;
};

struct EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE {
  UINTN MaxMode;
  UINTN Mode;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* Info;
};

struct EFI_GRAPHICS_OUTPUT_PROTOCOL;
using EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT = EFI_STATUS(EFIAPI*)(
    EFI_GRAPHICS_OUTPUT_PROTOCOL*,
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL*,
    EFI_GRAPHICS_OUTPUT_BLT_OPERATION,
    UINTN,
    UINTN,
    UINTN,
    UINTN,
    UINTN,
    UINTN,
    UINTN);

struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
  VOID* QueryMode;
  VOID* SetMode;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT Blt;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* Mode;
};

extern UINTN gEfiGraphicsOutputProtocolGuid;

