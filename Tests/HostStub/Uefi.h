#pragma once

#include <stddef.h>
#include <stdint.h>

typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef uintptr_t UINTN;
typedef intptr_t INTN;
typedef char CHAR8;
typedef wchar_t CHAR16;
typedef void VOID;
typedef UINT8 BOOLEAN;
typedef UINTN EFI_STATUS;
typedef VOID* EFI_HANDLE;
typedef VOID* EFI_EVENT;

struct EFI_GUID {
  UINT32 Data1;
  UINT16 Data2;
  UINT16 Data3;
  UINT8 Data4[8];
};

#define EFIAPI
#define TRUE static_cast<BOOLEAN>(1)
#define FALSE static_cast<BOOLEAN>(0)
#define MAX_UINTN UINTPTR_MAX
#define EFI_SUCCESS static_cast<EFI_STATUS>(0)
#define EFI_INVALID_PARAMETER (static_cast<EFI_STATUS>(1) << 63U | 2U)
#define EFI_NOT_READY (static_cast<EFI_STATUS>(1) << 63U | 6U)
#define EFI_BUFFER_TOO_SMALL (static_cast<EFI_STATUS>(1) << 63U | 5U)
#define EFI_BAD_BUFFER_SIZE (static_cast<EFI_STATUS>(1) << 63U | 4U)
#define EFI_UNSUPPORTED (static_cast<EFI_STATUS>(1) << 63U | 3U)
#define EFI_LOAD_ERROR (static_cast<EFI_STATUS>(1) << 63U | 1U)
#define EFI_OUT_OF_RESOURCES (static_cast<EFI_STATUS>(1) << 63U | 9U)
#define EFI_NOT_FOUND (static_cast<EFI_STATUS>(1) << 63U | 14U)
#define EFI_ERROR(Status) (((Status) & (static_cast<EFI_STATUS>(1) << 63U)) != 0)

#define SCAN_UP 0x0001U
#define SCAN_DOWN 0x0002U
#define SCAN_RIGHT 0x0003U
#define SCAN_LEFT 0x0004U
#define SCAN_F2 0x000CU
#define SCAN_ESC 0x0017U

struct EFI_INPUT_KEY {
  UINT16 ScanCode;
  CHAR16 UnicodeChar;
};

struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
using EFI_INPUT_READ_KEY = EFI_STATUS(EFIAPI*)(
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL*, EFI_INPUT_KEY*);

struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
  VOID* Reset;
  EFI_INPUT_READ_KEY ReadKeyStroke;
  EFI_EVENT WaitForKey;
};

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
using EFI_TEXT_ENABLE_CURSOR = EFI_STATUS(EFIAPI*)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, BOOLEAN);
using EFI_TEXT_STRING = EFI_STATUS(EFIAPI*)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*, CHAR16*);

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
  VOID* Reset;
  EFI_TEXT_STRING OutputString;
  VOID* TestString;
  VOID* QueryMode;
  VOID* SetMode;
  VOID* SetAttribute;
  VOID* ClearScreen;
  VOID* SetCursorPosition;
  EFI_TEXT_ENABLE_CURSOR EnableCursor;
};

struct EFI_SYSTEM_TABLE {
  VOID* Hdr;
  CHAR16* FirmwareVendor;
  UINTN FirmwareRevision;
  EFI_HANDLE ConsoleInHandle;
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL* ConIn;
  EFI_HANDLE ConsoleOutHandle;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* ConOut;
};
