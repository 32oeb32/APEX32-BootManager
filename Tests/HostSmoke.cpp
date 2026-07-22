#include "UefiCompat.hpp"

extern "C" {
#include <Protocol/GraphicsOutput.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
}

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Boot/BootDiscovery.hpp"
#include "Boot/EfiLoader.hpp"
#include "Boot/FirmwareBootDiscovery.hpp"
#include "Assets/OsIdentity.hpp"
#include "Menu/CardLayout.hpp"
#include "Renderer/Color.hpp"
#include "Renderer/GopRenderer.hpp"
#include "Renderer/LogicalCanvas.hpp"

extern "C" EFI_STATUS EFIAPI UefiMain(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE* SystemTable);

extern "C" {
UINTN gEfiGraphicsOutputProtocolGuid = 0;
EFI_GUID gEfiLoadedImageProtocolGuid{};
EFI_GUID gEfiSimpleFileSystemProtocolGuid{};
EFI_GUID gEfiGlobalVariableGuid{
    0x8BE4DF61U,
    0x93CAU,
    0x11D2U,
    {0xAAU, 0x0DU, 0x00U, 0xE0U, 0x98U, 0x03U, 0x2BU, 0x8CU},
};
EFI_BOOT_SERVICES* gBS = nullptr;
EFI_RUNTIME_SERVICES* gRT = nullptr;
}

namespace {

struct SmokeState {
  UINTN FrameCount;
  UINTN StallCount;
  UINTN FirstFrameColoredPixels;
  UINTN LastFrameColoredPixels;
  UINTN MaximumColoredPixels;
  UINTN CursorChangeCount;
  BOOLEAN CursorVisible;
  UINTN FailureMessageCount;
  UINTN WaitForEventCount;
  UINTN ReadKeyCount;
  UINTN HandleProtocolCount;
  UINTN LoadedImageProtocolCount;
  UINTN SimpleFileSystemProtocolCount;
  UINTN OpenVolumeCount;
  UINTN ProbeOpenCount;
  UINTN ProbeCloseCount;
  UINTN ConfigReadCount;
  UINTN FileDevicePathCount;
  UINTN LoadImageCount;
  UINTN StartImageCount;
  UINTN UnloadImageCount;
  BOOLEAN KaliPathMatched;
  BOOLEAN BlackArchPathMatched;
  BOOLEAN KaliProbeMatched;
  BOOLEAN BlackArchProbeMatched;
  BOOLEAN ConfigProbeMatched;
  BOOLEAN SimulateBlackArchMissing;
  UINT8 PendingTarget;
};

SmokeState State{};
EFI_GRAPHICS_OUTPUT_BLT_PIXEL CapturedFrame[64]{};
UINTN CapturedWidth = 0U;
UINTN CapturedHeight = 0U;

EFI_INPUT_KEY KeySequence[] = {
    {SCAN_F2, 0},
    {SCAN_F2, 0},
    {0, static_cast<CHAR16>('\r')},
    {SCAN_RIGHT, 0},
    {0, static_cast<CHAR16>('\r')},
    {SCAN_LEFT, 0},
    {SCAN_ESC, 0},
};
UINTN NextKeyIndex = 0;

UINT8 RuntimeBootOrder[] = {0x05U, 0x00U, 0x06U, 0x00U, 0x07U, 0x00U};
BOOLEAN RuntimeBootCurrentAvailable = FALSE;
UINT8 RuntimeBootCurrent[] = {0x00U, 0x00U};
alignas(8) UINT8 RuntimeOption5[1024]{};
alignas(8) UINT8 RuntimeOption6[1024]{};
alignas(8) UINT8 RuntimeOption7[1024]{};
alignas(8) UINT8 RuntimeOption8[1024]{};
UINTN RuntimeOption5Size = 0U;
UINTN RuntimeOption6Size = 0U;
UINTN RuntimeOption7Size = 0U;
UINTN RuntimeOption8Size = 0U;

EFI_HANDLE ParentImageHandle = reinterpret_cast<EFI_HANDLE>(0x1000);
EFI_HANDLE EspDeviceHandle = reinterpret_cast<EFI_HANDLE>(0x2000);
EFI_HANDLE KaliImageHandle = reinterpret_cast<EFI_HANDLE>(0x3000);
EFI_HANDLE BlackArchImageHandle = reinterpret_cast<EFI_HANDLE>(0x4000);
EFI_LOADED_IMAGE_PROTOCOL ParentImage{EspDeviceHandle};

EFI_STATUS EFIAPI CloseProbeFile(EFI_FILE_PROTOCOL*) {
  ++State.ProbeCloseCount;
  return EFI_SUCCESS;
}

EFI_FILE_PROTOCOL KaliProbeFile{
    0, nullptr, CloseProbeFile, nullptr, nullptr, nullptr};
EFI_FILE_PROTOCOL BlackArchProbeFile{
    0, nullptr, CloseProbeFile, nullptr, nullptr, nullptr};

EFI_STATUS EFIAPI ReadConfigFile(
    EFI_FILE_PROTOCOL*,
    UINTN* BufferSize,
    VOID* Buffer) {
  constexpr CHAR8 kConfiguration[] =
      "APEX32CFG|1\n"
      "ENTRY|KALI LINUX|\\EFI\\kali\\grubx64.efi|kali\n"
      "ENTRY|BLACKARCH LINUX|\\EFI\\BlackArch_Linux\\grubx64.efi|blackarch\n";
  constexpr UINTN kConfigurationSize = sizeof(kConfiguration) - 1U;
  ++State.ConfigReadCount;
  if ((BufferSize == nullptr) || (Buffer == nullptr) ||
      (*BufferSize < kConfigurationSize)) {
    return EFI_BAD_BUFFER_SIZE;
  }
  std::memcpy(Buffer, kConfiguration, kConfigurationSize);
  *BufferSize = kConfigurationSize;
  return EFI_SUCCESS;
}

EFI_FILE_PROTOCOL ConfigFile{
    0,
    nullptr,
    CloseProbeFile,
    nullptr,
    ReadConfigFile,
    nullptr,
};

EFI_STATUS EFIAPI OpenProbeFile(
    EFI_FILE_PROTOCOL*,
    EFI_FILE_PROTOCOL** NewHandle,
    CHAR16* FileName,
    const UINT64 OpenMode,
    const UINT64 Attributes);

EFI_FILE_PROTOCOL EspRoot{
    0, OpenProbeFile, CloseProbeFile, nullptr, nullptr, nullptr};

EFI_STATUS EFIAPI OpenVolume(
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL*,
    EFI_FILE_PROTOCOL** Root) {
  ++State.OpenVolumeCount;
  if (Root == nullptr) {
    return EFI_UNSUPPORTED;
  }
  *Root = &EspRoot;
  return EFI_SUCCESS;
}

EFI_SIMPLE_FILE_SYSTEM_PROTOCOL SimpleFileSystem{0, OpenVolume};

EFI_GRAPHICS_OUTPUT_MODE_INFORMATION ModeInfo{
    0,
    1920,
    1080,
    0,
    {},
    1920,
};

EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE Mode{
    1,
    0,
    &ModeInfo,
};

EFI_STATUS EFIAPI PresentFrame(
    EFI_GRAPHICS_OUTPUT_PROTOCOL*,
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL* Buffer,
    const EFI_GRAPHICS_OUTPUT_BLT_OPERATION Operation,
    UINTN,
    UINTN,
    UINTN,
    UINTN,
    const UINTN Width,
    const UINTN Height,
    const UINTN Delta) {
  if ((Buffer == nullptr) || (Operation != EfiBltBufferToVideo) ||
      (Width != ModeInfo.HorizontalResolution) ||
      (Height != ModeInfo.VerticalResolution) ||
      (Delta != Width * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL))) {
    return EFI_UNSUPPORTED;
  }

  CapturedWidth = Width;
  CapturedHeight = Height;
  if ((Width <= 8U) && (Height <= 8U)) {
    for (UINTN Index = 0U; Index < (Width * Height); ++Index) {
      CapturedFrame[Index] = Buffer[Index];
    }
  }

  UINTN ColoredPixels = 0;
  for (UINTN Index = 0; Index < (Width * Height); ++Index) {
    if ((Buffer[Index].Red != 0) || (Buffer[Index].Green != 0) ||
        (Buffer[Index].Blue != 0)) {
      ++ColoredPixels;
    }
  }

  ++State.FrameCount;
  if (State.FrameCount == 1) {
    State.FirstFrameColoredPixels = ColoredPixels;
  }

  const char* PreviewPath = nullptr;
  if (State.FrameCount == 58) {
    PreviewPath = std::getenv("APEX32_PREVIEW");
  } else if (State.FrameCount == 59) {
    PreviewPath = std::getenv("APEX32_DIAGNOSTICS_PREVIEW");
  } else if (State.FrameCount == 61) {
    PreviewPath = std::getenv("APEX32_BOOT_PREVIEW");
  } else if (State.FrameCount == 62) {
    PreviewPath = std::getenv("APEX32_ERROR_PREVIEW");
  } else if (State.FrameCount == 64) {
    PreviewPath = std::getenv("APEX32_BLACKARCH_BOOT_PREVIEW");
  } else if (State.FrameCount == 65) {
    PreviewPath = std::getenv("APEX32_BLACKARCH_ERROR_PREVIEW");
  }
  if ((PreviewPath != nullptr) && (PreviewPath[0] != '\0')) {
    std::FILE* Preview = std::fopen(PreviewPath, "wb");
    if (Preview != nullptr) {
      std::fprintf(Preview, "P6\n%zu %zu\n255\n",
                   static_cast<std::size_t>(Width),
                   static_cast<std::size_t>(Height));
      for (UINTN Index = 0; Index < (Width * Height); ++Index) {
        std::fputc(Buffer[Index].Red, Preview);
        std::fputc(Buffer[Index].Green, Preview);
        std::fputc(Buffer[Index].Blue, Preview);
      }
      std::fclose(Preview);
    }
  }

  State.LastFrameColoredPixels = ColoredPixels;
  if (ColoredPixels > State.MaximumColoredPixels) {
    State.MaximumColoredPixels = ColoredPixels;
  }

  return EFI_SUCCESS;
}

EFI_GRAPHICS_OUTPUT_PROTOCOL GraphicsOutput{
    nullptr,
    nullptr,
    PresentFrame,
    &Mode,
};

EFI_STATUS EFIAPI LocateProtocol(VOID*, VOID*, VOID** Interface) {
  if (Interface == nullptr) {
    return EFI_UNSUPPORTED;
  }

  *Interface = &GraphicsOutput;
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI Stall(UINTN) {
  ++State.StallCount;
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI HandleProtocol(
    const EFI_HANDLE Handle,
    EFI_GUID* Protocol,
    VOID** Interface) {
  ++State.HandleProtocolCount;
  if (Interface == nullptr) {
    return EFI_UNSUPPORTED;
  }

  if ((Handle == ParentImageHandle) &&
      (Protocol == &gEfiLoadedImageProtocolGuid)) {
    ++State.LoadedImageProtocolCount;
    *Interface = &ParentImage;
    return EFI_SUCCESS;
  }
  if ((Handle == EspDeviceHandle) &&
      (Protocol == &gEfiSimpleFileSystemProtocolGuid)) {
    ++State.SimpleFileSystemProtocolCount;
    *Interface = &SimpleFileSystem;
    return EFI_SUCCESS;
  }

  return EFI_UNSUPPORTED;
}

[[nodiscard]] BOOLEAN MatchesPath(
    const CHAR16* Path,
    const CHAR16* Expected) {
  if ((Path == nullptr) || (Expected == nullptr)) {
    return FALSE;
  }

  UINTN Index = 0;
  while ((Path[Index] != 0) && (Expected[Index] != 0)) {
    if (Path[Index] != Expected[Index]) {
      return FALSE;
    }
    ++Index;
  }
  return (Path[Index] == Expected[Index]) ? TRUE : FALSE;
}

EFI_STATUS EFIAPI RuntimeGetVariable(
    CHAR16* VariableName,
    EFI_GUID*,
    UINT32* Attributes,
    UINTN* DataSize,
    VOID* Data) {
  constexpr CHAR16 kBootOrder[] = {
      'B', 'o', 'o', 't', 'O', 'r', 'd', 'e', 'r', 0,
  };
  constexpr CHAR16 kBootCurrent[] = {
      'B', 'o', 'o', 't', 'C', 'u', 'r', 'r', 'e', 'n', 't', 0,
  };
  constexpr CHAR16 kBoot0005[] = {
      'B', 'o', 'o', 't', '0', '0', '0', '5', 0,
  };
  constexpr CHAR16 kBoot0006[] = {
      'B', 'o', 'o', 't', '0', '0', '0', '6', 0,
  };
  constexpr CHAR16 kBoot0007[] = {
      'B', 'o', 'o', 't', '0', '0', '0', '7', 0,
  };
  constexpr CHAR16 kBoot0008[] = {
      'B', 'o', 'o', 't', '0', '0', '0', '8', 0,
  };
  if ((VariableName == nullptr) || (DataSize == nullptr)) {
    return EFI_INVALID_PARAMETER;
  }
  const UINT8* Source = nullptr;
  UINTN SourceSize = 0U;
  if (MatchesPath(VariableName, kBootOrder)) {
    Source = RuntimeBootOrder;
    SourceSize = sizeof(RuntimeBootOrder);
  } else if (MatchesPath(VariableName, kBootCurrent) &&
             RuntimeBootCurrentAvailable) {
    Source = RuntimeBootCurrent;
    SourceSize = sizeof(RuntimeBootCurrent);
  } else if (MatchesPath(VariableName, kBoot0005)) {
    Source = RuntimeOption5;
    SourceSize = RuntimeOption5Size;
  } else if (MatchesPath(VariableName, kBoot0006)) {
    Source = RuntimeOption6;
    SourceSize = RuntimeOption6Size;
  } else if (MatchesPath(VariableName, kBoot0007)) {
    Source = RuntimeOption7;
    SourceSize = RuntimeOption7Size;
  } else if (MatchesPath(VariableName, kBoot0008)) {
    Source = RuntimeOption8;
    SourceSize = RuntimeOption8Size;
  } else {
    return EFI_NOT_FOUND;
  }
  if ((*DataSize < SourceSize) || (Data == nullptr)) {
    *DataSize = SourceSize;
    return EFI_BUFFER_TOO_SMALL;
  }
  std::memcpy(Data, Source, SourceSize);
  *DataSize = SourceSize;
  if (Attributes != nullptr) {
    *Attributes = 7U;
  }
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI RuntimeGetNextVariableName(
    UINTN* VariableNameSize,
    CHAR16* VariableName,
    EFI_GUID* VendorGuid) {
  constexpr CHAR16 kNames[][9] = {
      {'B', 'o', 'o', 't', '0', '0', '0', '5', 0},
      {'B', 'o', 'o', 't', '0', '0', '0', '6', 0},
      {'B', 'o', 'o', 't', '0', '0', '0', '7', 0},
      {'B', 'o', 'o', 't', '0', '0', '0', '8', 0},
  };
  if ((VariableNameSize == nullptr) || (VariableName == nullptr) ||
      (VendorGuid == nullptr)) {
    return EFI_INVALID_PARAMETER;
  }
  UINTN Next = 0U;
  if (VariableName[0] != 0U) {
    BOOLEAN Found = FALSE;
    for (UINTN Index = 0U; Index < 4U; ++Index) {
      if (MatchesPath(VariableName, kNames[Index])) {
        Next = Index + 1U;
        Found = TRUE;
        break;
      }
    }
    if (!Found || (Next >= 4U)) {
      return EFI_NOT_FOUND;
    }
  }
  constexpr UINTN kRequiredSize = 9U * sizeof(CHAR16);
  if (*VariableNameSize < kRequiredSize) {
    *VariableNameSize = kRequiredSize;
    return EFI_BUFFER_TOO_SMALL;
  }
  for (UINTN Index = 0U; Index < 9U; ++Index) {
    VariableName[Index] = kNames[Next][Index];
  }
  *VariableNameSize = kRequiredSize;
  *VendorGuid = gEfiGlobalVariableGuid;
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI OpenProbeFile(
    EFI_FILE_PROTOCOL*,
    EFI_FILE_PROTOCOL** NewHandle,
    CHAR16* FileName,
    const UINT64 OpenMode,
    const UINT64 Attributes) {
  constexpr CHAR16 kKaliExpected[] = {
      '\\', 'E', 'F', 'I', '\\', 'k', 'a', 'l', 'i', '\\', 'g',
      'r',  'u', 'b', 'x', '6',  '4', '.', 'e', 'f', 'i', 0,
  };
  constexpr CHAR16 kBlackArchExpected[] = {
      '\\', 'E', 'F', 'I', '\\', 'B', 'l', 'a', 'c', 'k', 'A', 'r', 'c',
      'h',  '_', 'L', 'i', 'n', 'u', 'x', '\\', 'g', 'r', 'u', 'b', 'x',
      '6',  '4', '.', 'e', 'f', 'i', 0,
  };
  constexpr CHAR16 kConfigExpected[] = {
      '\\', 'E', 'F', 'I', '\\', 'A', 'P', 'E', 'X', '3', '2', '\\',
      'a', 'p', 'e', 'x', '3', '2', '.', 'c', 'f', 'g', 0,
  };

  ++State.ProbeOpenCount;
  if ((NewHandle == nullptr) || (OpenMode != EFI_FILE_MODE_READ) ||
      (Attributes != 0)) {
    return EFI_UNSUPPORTED;
  }

  if (MatchesPath(FileName, kConfigExpected) == TRUE) {
    State.ConfigProbeMatched = TRUE;
    *NewHandle = &ConfigFile;
    return EFI_SUCCESS;
  }
  if (MatchesPath(FileName, kKaliExpected) == TRUE) {
    State.KaliProbeMatched = TRUE;
    *NewHandle = &KaliProbeFile;
    return EFI_SUCCESS;
  }
  if (MatchesPath(FileName, kBlackArchExpected) == TRUE) {
    State.BlackArchProbeMatched = TRUE;
    if (State.SimulateBlackArchMissing) {
      *NewHandle = nullptr;
      return EFI_NOT_FOUND;
    }
    *NewHandle = &BlackArchProbeFile;
    return EFI_SUCCESS;
  }

  *NewHandle = nullptr;
  return EFI_NOT_FOUND;
}

EFI_STATUS EFIAPI LoadImage(
    const BOOLEAN BootPolicy,
    const EFI_HANDLE ParentHandle,
    EFI_DEVICE_PATH_PROTOCOL* DevicePath,
    VOID*,
    const UINTN SourceSize,
    EFI_HANDLE* ImageHandle) {
  ++State.LoadImageCount;
  if ((BootPolicy != TRUE) || (ParentHandle != ParentImageHandle) ||
      (DevicePath == nullptr) || (SourceSize != 0) ||
      (ImageHandle == nullptr) || (State.PendingTarget == 0U)) {
    return EFI_UNSUPPORTED;
  }

  *ImageHandle = (State.PendingTarget == 1U)
                     ? KaliImageHandle
                     : BlackArchImageHandle;
  State.PendingTarget = 0U;
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI StartImage(
    const EFI_HANDLE ImageHandle,
    UINTN* ExitDataSize,
    CHAR16** ExitData) {
  ++State.StartImageCount;
  if (((ImageHandle != KaliImageHandle) &&
       (ImageHandle != BlackArchImageHandle)) ||
      (ExitDataSize == nullptr) ||
      (ExitData == nullptr)) {
    return EFI_UNSUPPORTED;
  }

  *ExitDataSize = 0;
  *ExitData = nullptr;
  return EFI_LOAD_ERROR;
}

EFI_STATUS EFIAPI UnloadImage(const EFI_HANDLE ImageHandle) {
  ++State.UnloadImageCount;
  return ((ImageHandle == KaliImageHandle) ||
          (ImageHandle == BlackArchImageHandle))
             ? EFI_SUCCESS
             : EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI WaitForEvent(
    const UINTN NumberOfEvents,
    EFI_EVENT* Event,
    UINTN* EventIndex) {
  if ((NumberOfEvents != 1U) || (Event == nullptr) ||
      (Event[0] == nullptr) || (EventIndex == nullptr) ||
      (NextKeyIndex >= (sizeof(KeySequence) / sizeof(KeySequence[0])))) {
    return EFI_UNSUPPORTED;
  }

  ++State.WaitForEventCount;
  *EventIndex = 0;
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI ReadKeyStroke(
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL*,
    EFI_INPUT_KEY* Key) {
  if ((Key == nullptr) ||
      (NextKeyIndex >= (sizeof(KeySequence) / sizeof(KeySequence[0])))) {
    return EFI_NOT_READY;
  }

  *Key = KeySequence[NextKeyIndex];
  ++NextKeyIndex;
  ++State.ReadKeyCount;
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI SetCursor(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*,
    const BOOLEAN Visible) {
  ++State.CursorChangeCount;
  State.CursorVisible = Visible;
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI OutputString(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*,
    CHAR16*) {
  ++State.FailureMessageCount;
  return EFI_SUCCESS;
}

[[nodiscard]] bool Check(const bool Condition, const char* Message) {
  if (!Condition) {
    std::fprintf(stderr, "FAIL: %s\n", Message);
  }

  return Condition;
}

void WriteUint16(UINT8* Buffer, const UINTN Offset, const UINT16 Value) {
  Buffer[Offset] = static_cast<UINT8>(Value & 0xFFU);
  Buffer[Offset + 1U] = static_cast<UINT8>((Value >> 8U) & 0xFFU);
}

[[nodiscard]] UINTN BuildLoadOption(
    UINT8* Buffer,
    const UINTN Capacity,
    const BOOLEAN Active,
    const CHAR16* Description,
    const CHAR16* LoaderPath) {
  if ((Buffer == nullptr) || (Description == nullptr) || (Capacity < 16U)) {
    return 0U;
  }
  std::memset(Buffer, 0, Capacity);
  Buffer[0] = Active ? 1U : 0U;
  UINTN Offset = 6U;
  for (UINTN Index = 0U;; ++Index) {
    if ((Offset + 2U) > Capacity) {
      return 0U;
    }
    WriteUint16(Buffer, Offset, static_cast<UINT16>(Description[Index]));
    Offset += 2U;
    if (Description[Index] == 0U) {
      break;
    }
  }

  const UINTN DevicePathStart = Offset;
  if (LoaderPath != nullptr) {
    UINTN CharacterCount = 0U;
    while (LoaderPath[CharacterCount] != 0U) {
      ++CharacterCount;
    }
    ++CharacterCount;
    const UINTN NodeLength = 4U + (CharacterCount * 2U);
    if ((NodeLength > 0xFFFFU) ||
        (Offset > (Capacity - NodeLength - 4U))) {
      return 0U;
    }
    Buffer[Offset] = 0x04U;
    Buffer[Offset + 1U] = 0x04U;
    WriteUint16(Buffer, Offset + 2U, static_cast<UINT16>(NodeLength));
    for (UINTN Index = 0U; Index < CharacterCount; ++Index) {
      WriteUint16(
          Buffer,
          Offset + 4U + (Index * 2U),
          static_cast<UINT16>(LoaderPath[Index]));
    }
    Offset += NodeLength;
  } else {
    if (Offset > (Capacity - 8U)) {
      return 0U;
    }
    Buffer[Offset] = 0x01U;
    Buffer[Offset + 1U] = 0x01U;
    WriteUint16(Buffer, Offset + 2U, 4U);
    Offset += 4U;
  }
  Buffer[Offset] = 0x7FU;
  Buffer[Offset + 1U] = 0xFFU;
  WriteUint16(Buffer, Offset + 2U, 4U);
  Offset += 4U;
  WriteUint16(
      Buffer,
      4U,
      static_cast<UINT16>(Offset - DevicePathStart));
  return Offset;
}

[[nodiscard]] bool RunFirmwareDiscoveryTests() {
  bool Passed = true;
  constexpr CHAR16 kWindowsName[] = {
      'W', 'i', 'n', 'd', 'o', 'w', 's', ' ', 'B', 'o', 'o', 't',
      ' ', 'M', 'a', 'n', 'a', 'g', 'e', 'r', 0,
  };
  constexpr CHAR16 kWindowsPath[] = {
      '\\', 'E', 'F', 'I', '\\', 'M', 'i', 'c', 'r', 'o', 's', 'o', 'f',
      't', '\\', 'B', 'o', 'o', 't', '\\', 'b', 'o', 'o', 't', 'm', 'g',
      'f', 'w', '.', 'e', 'f', 'i', 0,
  };
  alignas(8) UINT8 Buffer[1024]{};
  const UINTN Size = BuildLoadOption(
      Buffer, sizeof(Buffer), TRUE, kWindowsName, kWindowsPath);
  apex32::BootEntry Entry{};
  Passed &= Check(
      Size > 0U &&
          apex32::FirmwareBootDiscovery::ParseLoadOption(
              0x0006U, Buffer, Size, &Entry) == EFI_SUCCESS,
      "active Boot#### load options must parse");
  Passed &= Check(
      Entry.Source == apex32::BootEntrySource::Firmware &&
          Entry.FirmwareBootNumber == 0x0006U && Entry.Available,
      "firmware entry metadata must be retained");
  Passed &= Check(
      MatchesPath(Entry.LoaderPath, kWindowsPath) == TRUE,
      "file-path nodes must be extracted for identity and diagnostics");
  Passed &= Check(
      apex32::osidentity::Resolve(Entry).Icon == apex32::OsIcon::Windows,
      "firmware descriptions must drive the pluggable identity registry");

  alignas(8) UINT8 Inactive[1024]{};
  const UINTN InactiveSize = BuildLoadOption(
      Inactive, sizeof(Inactive), FALSE, kWindowsName, kWindowsPath);
  Passed &= Check(
      apex32::FirmwareBootDiscovery::ParseLoadOption(
          0x0006U, Inactive, InactiveSize, &Entry) == EFI_NOT_READY,
      "inactive firmware options must not become boot cards");
  Passed &= Check(
      apex32::FirmwareBootDiscovery::ParseLoadOption(
          0x0006U, Buffer, Size - 1U, &Entry) == EFI_BAD_BUFFER_SIZE,
      "truncated load options must fail closed");

  constexpr CHAR16 kEmptyName[] = {0};
  alignas(8) UINT8 Generic[64]{};
  const UINTN GenericSize = BuildLoadOption(
      Generic, sizeof(Generic), TRUE, kEmptyName, nullptr);
  Passed &= Check(
      apex32::FirmwareBootDiscovery::ParseLoadOption(
          0x0007U, Generic, GenericSize, &Entry) == EFI_SUCCESS &&
          std::strcmp(Entry.Name, "BOOT 0007") == 0 &&
          Entry.LoaderPath[0] == 0U,
      "unknown device paths must receive a generic bootable identity");

  constexpr CHAR16 kFirmwareUiName[] = {
      'U', 'E', 'F', 'I', ' ', 'F', 'i', 'r', 'm', 'w', 'a', 'r', 'e',
      ' ', 'S', 'e', 't', 'u', 'p', 0,
  };
  alignas(8) UINT8 FirmwareUi[128]{};
  const UINTN FirmwareUiSize = BuildLoadOption(
      FirmwareUi,
      sizeof(FirmwareUi),
      TRUE,
      kFirmwareUiName,
      nullptr);
  const UINTN FirmwareUiPathStart = 6U + sizeof(kFirmwareUiName);
  FirmwareUi[FirmwareUiPathStart] = 0x04U;
  FirmwareUi[FirmwareUiPathStart + 1U] = 0x06U;
  Passed &= Check(
      apex32::FirmwareBootDiscovery::ParseLoadOption(
          0x0009U, FirmwareUi, FirmwareUiSize, &Entry) == EFI_NOT_READY,
      "firmware-volume maintenance applications must not become OS cards");
  FirmwareUi[FirmwareUiPathStart + 1U] = 0x07U;
  Passed &= Check(
      apex32::FirmwareBootDiscovery::ParseLoadOption(
          0x0009U, FirmwareUi, FirmwareUiSize, &Entry) == EFI_NOT_READY,
      "firmware-volume container options must not become OS cards");

  constexpr CHAR16 kApexName[] = {
      'A', 'P', 'E', 'X', '3', '2', ' ', 'S', 'e', 'c', 'u', 'r', 'e',
      ' ', 'G', 'a', 't', 'e', 'w', 'a', 'y', 0,
  };
  constexpr CHAR16 kApexPath[] = {
      '\\', 'E', 'F', 'I', '\\', 'A', 'P', 'E', 'X', '3', '2', '\\',
      'A', 'p', 'e', 'x', '3', '2', 'B', 'o', 'o', 't', 'M', 'a', 'n',
      'a', 'g', 'e', 'r', '.', 'e', 'f', 'i', 0,
  };
  constexpr CHAR16 kInactiveName[] = {
      'D', 'i', 's', 'a', 'b', 'l', 'e', 'd', ' ', 'T', 'o', 'o', 'l', 0,
  };
  constexpr CHAR16 kKaliNativeName[] = {
      'K', 'a', 'l', 'i', ' ', 'L', 'i', 'n', 'u', 'x', 0,
  };
  constexpr CHAR16 kKaliNativePath[] = {
      '\\', 'E', 'F', 'I', '\\', 'k', 'a', 'l', 'i', '\\', 'g', 'r',
      'u', 'b', 'x', '6', '4', '.', 'e', 'f', 'i', 0,
  };
  RuntimeOption5Size = BuildLoadOption(
      RuntimeOption5,
      sizeof(RuntimeOption5),
      TRUE,
      kApexName,
      kApexPath);
  RuntimeOption6Size = BuildLoadOption(
      RuntimeOption6,
      sizeof(RuntimeOption6),
      TRUE,
      kWindowsName,
      kWindowsPath);
  RuntimeOption7Size = BuildLoadOption(
      RuntimeOption7,
      sizeof(RuntimeOption7),
      FALSE,
      kInactiveName,
      nullptr);
  RuntimeOption8Size = BuildLoadOption(
      RuntimeOption8,
      sizeof(RuntimeOption8),
      TRUE,
      kKaliNativeName,
      kKaliNativePath);
  EFI_RUNTIME_SERVICES RuntimeServices{
      RuntimeGetVariable,
      RuntimeGetNextVariableName,
  };
  gRT = &RuntimeServices;
  apex32::BootConfiguration FirmwareConfiguration{};
  const EFI_STATUS DiscoveryStatus =
      apex32::FirmwareBootDiscovery::Discover(&FirmwareConfiguration);
  gRT = nullptr;
  Passed &= Check(
      DiscoveryStatus == EFI_SUCCESS &&
          FirmwareConfiguration.FirmwareCount == 2U &&
          FirmwareConfiguration.Count == 2U,
      "BootOrder and unlisted Boot#### variables must merge into one bounded set");
  Passed &= Check(
      std::strcmp(FirmwareConfiguration.Entries[0].Name,
                  "Windows Boot Manager") == 0 &&
          std::strcmp(FirmwareConfiguration.Entries[1].Name, "Kali Linux") == 0,
      "firmware order must be preserved before extra active entries");
  Passed &= Check(
      FirmwareConfiguration.Entries[0].FirmwareBootNumber == 0x0006U &&
          FirmwareConfiguration.Entries[1].FirmwareBootNumber == 0x0008U,
      "APEX32 self entries and inactive options must be excluded");

  RuntimeBootCurrent[0] = 0x06U;
  RuntimeBootCurrent[1] = 0x00U;
  RuntimeBootCurrentAvailable = TRUE;
  apex32::BootConfiguration CurrentFilteredConfiguration{};
  gRT = &RuntimeServices;
  const EFI_STATUS CurrentFilteredStatus =
      apex32::FirmwareBootDiscovery::Discover(&CurrentFilteredConfiguration);
  gRT = nullptr;
  RuntimeBootCurrentAvailable = FALSE;
  Passed &= Check(
      CurrentFilteredStatus == EFI_SUCCESS &&
          CurrentFilteredConfiguration.FirmwareCount == 1U &&
          CurrentFilteredConfiguration.Count == 1U &&
          CurrentFilteredConfiguration.Entries[0].FirmwareBootNumber ==
              0x0008U,
      "BootCurrent must be excluded to prevent recursive self-launch");

  gRT = &RuntimeServices;
  const apex32::BootConfiguration MergedConfiguration =
      apex32::BootDiscovery::Discover(ParentImageHandle);
  gRT = nullptr;
  Passed &= Check(
      MergedConfiguration.Status == EFI_SUCCESS &&
          MergedConfiguration.FirmwareCount == 2U &&
          MergedConfiguration.ConfigCount == 1U &&
          MergedConfiguration.Count == 3U,
      "native entries and configuration fallback must merge without duplicates");
  Passed &= Check(
      std::strcmp(MergedConfiguration.Entries[2].Name,
                  "BLACKARCH LINUX") == 0,
      "non-duplicate configuration entries must remain available");

  apex32::BootEntry NativeEntry{};
  Passed &= Check(
      apex32::FirmwareBootDiscovery::ParseLoadOption(
          0x0006U, Buffer, Size, &NativeEntry) == EFI_SUCCESS,
      "native handoff fixture must parse before launch");
  State.PendingTarget = 1U;
  const apex32::EfiLaunchResult LaunchResult = apex32::EfiLoader::Launch(
      ParentImageHandle, NativeEntry);
  Passed &= Check(
      LaunchResult.Stage == apex32::EfiLaunchStage::StartImage &&
          State.LoadImageCount == 1U && State.StartImageCount == 1U,
      "native Boot#### device paths must reach LoadImage and StartImage");
  return Passed;
}

}  // namespace

extern "C" EFI_DEVICE_PATH_PROTOCOL* EFIAPI FileDevicePath(
    const EFI_HANDLE Device,
    const CHAR16* FileName) {
  constexpr CHAR16 kKaliExpected[] = {
      '\\', 'E', 'F', 'I', '\\', 'k', 'a', 'l', 'i', '\\', 'g',
      'r',  'u', 'b', 'x', '6',  '4', '.', 'e', 'f', 'i', 0,
  };
  constexpr CHAR16 kBlackArchExpected[] = {
      '\\', 'E', 'F', 'I', '\\', 'B', 'l', 'a', 'c', 'k', 'A', 'r', 'c',
      'h',  '_', 'L', 'i', 'n', 'u', 'x', '\\', 'g', 'r', 'u', 'b', 'x',
      '6',  '4', '.', 'e', 'f', 'i', 0,
  };
  ++State.FileDevicePathCount;
  if ((Device == EspDeviceHandle) &&
      (MatchesPath(FileName, kKaliExpected) == TRUE)) {
    State.KaliPathMatched = TRUE;
    State.PendingTarget = 1U;
  } else if ((Device == EspDeviceHandle) &&
             (MatchesPath(FileName, kBlackArchExpected) == TRUE)) {
    State.BlackArchPathMatched = TRUE;
    State.PendingTarget = 2U;
  } else {
    State.PendingTarget = 0U;
  }
  return static_cast<EFI_DEVICE_PATH_PROTOCOL*>(
      std::calloc(1, sizeof(EFI_DEVICE_PATH_PROTOCOL)));
}

extern "C" VOID* AllocateZeroPool(const UINTN AllocationSize) {
  return std::calloc(1, AllocationSize);
}

extern "C" VOID FreePool(VOID* Buffer) {
  std::free(Buffer);
}

extern "C" VOID* SetMem(
    VOID* Buffer,
    const UINTN Length,
    const UINT8 Value) {
  return std::memset(Buffer, Value, Length);
}

[[nodiscard]] bool RunGraphicsFoundationTests() {
  bool Passed = true;

  apex32::LogicalViewport Viewport{};
  Passed &= Check(
      apex32::LogicalCanvas::CreateViewport(1920U, 1080U, &Viewport) ==
          EFI_SUCCESS &&
          Viewport.Valid && (Viewport.X == 0U) && (Viewport.Y == 0U) &&
          (Viewport.Width == 1920U) && (Viewport.Height == 1080U),
      "1920x1080 must map to the complete logical canvas");
  Passed &= Check(
      apex32::LogicalCanvas::CreateViewport(800U, 600U, &Viewport) ==
          EFI_SUCCESS &&
          (Viewport.X == 0U) && (Viewport.Y == 75U) &&
          (Viewport.Width == 800U) && (Viewport.Height == 450U),
      "800x600 must use centered vertical letterboxing");
  Passed &= Check(
      apex32::LogicalCanvas::CreateViewport(1024U, 768U, &Viewport) ==
          EFI_SUCCESS &&
          (Viewport.X == 0U) && (Viewport.Y == 96U) &&
          (Viewport.Width == 1024U) && (Viewport.Height == 576U),
      "1024x768 must preserve 16:9 content with vertical letterboxing");
  Passed &= Check(
      apex32::LogicalCanvas::CreateViewport(1280U, 720U, &Viewport) ==
          EFI_SUCCESS &&
          (Viewport.X == 0U) && (Viewport.Y == 0U) &&
          (Viewport.Width == 1280U) && (Viewport.Height == 720U),
      "1280x720 must use the complete physical canvas");
  Passed &= Check(
      apex32::LogicalCanvas::CreateViewport(800U, 600U, &Viewport) ==
          EFI_SUCCESS,
      "800x600 viewport must remain available for mapping checks");

  UINTN X = 0U;
  UINTN Y = 0U;
  UINTN Width = 0U;
  UINTN Height = 0U;
  Passed &= Check(
      apex32::LogicalCanvas::MapRectangle(
          Viewport,
          0U,
          0U,
          apex32::kReferenceCanvasWidth,
          apex32::kReferenceCanvasHeight,
          &X,
          &Y,
          &Width,
          &Height) &&
          (X == 0U) && (Y == 75U) && (Width == 800U) &&
          (Height == 450U),
      "logical full-screen rectangle must map exactly to the viewport");
  Passed &= Check(
      !apex32::LogicalCanvas::MapRectangle(
          Viewport,
          apex32::kReferenceCanvasWidth,
          0U,
          1U,
          1U,
          &X,
          &Y,
          &Width,
          &Height),
      "logical rectangles outside the reference canvas must be rejected");
  Passed &= Check(
      apex32::LogicalCanvas::CreateViewport(2560U, 1080U, &Viewport) ==
          EFI_SUCCESS &&
          (Viewport.X == 320U) && (Viewport.Y == 0U) &&
          (Viewport.Width == 1920U) && (Viewport.Height == 1080U),
      "ultrawide modes must use centered horizontal letterboxing");
  Passed &= Check(
      EFI_ERROR(apex32::LogicalCanvas::CreateViewport(0U, 1080U, &Viewport)),
      "zero-width framebuffers must be rejected");
  Passed &= Check(
      apex32::LogicalCanvas::CreateViewport(MAX_UINTN, 2U, &Viewport) ==
          EFI_BAD_BUFFER_SIZE,
      "overflowing viewport calculations must fail closed");

  const apex32::CardPageLayout OneCard =
      apex32::CardLayout::Calculate(1U, 0U);
  Passed &= Check(
      OneCard.Valid && !OneCard.Compact &&
          (OneCard.VisibleStart == 0U) && (OneCard.VisibleCount == 1U) &&
          (OneCard.Cards[0].X == 560U),
      "one-card layout must center the selected operating system");
  const apex32::CardPageLayout ThreeCards =
      apex32::CardLayout::Calculate(3U, 2U);
  Passed &= Check(
      ThreeCards.Valid && ThreeCards.Compact &&
          (ThreeCards.VisibleCount == 3U) &&
          (ThreeCards.Cards[2].X == 560U) &&
          (ThreeCards.Cards[2].Y == 520U),
      "three-card layout must center the final compact card");
  const apex32::CardPageLayout LaterPage =
      apex32::CardLayout::Calculate(9U, 5U);
  Passed &= Check(
      LaterPage.Valid && (LaterPage.VisibleStart == 4U) &&
          (LaterPage.VisibleCount == 4U),
      "many-entry layout must page in bounded groups of four");
  Passed &= Check(
      !apex32::CardLayout::Calculate(8U, 8U).Valid,
      "card layout must reject an out-of-range focused index");

  apex32::BootEntry Fedora{};
  std::strcpy(Fedora.Name, "FEDORA LINUX");
  Fedora.Icon = apex32::OsIcon::Linux;
  Passed &= Check(
      apex32::osidentity::Resolve(Fedora).Icon == apex32::OsIcon::Fedora,
      "generic Linux cards must resolve known identities from their names");
  apex32::BootEntry Unknown{};
  std::strcpy(Unknown.Name, "MYSTERY EFI LOADER");
  Unknown.Icon = apex32::OsIcon::Generic;
  Passed &= Check(
      apex32::osidentity::Resolve(Unknown).Icon == apex32::OsIcon::Generic,
      "unknown EFI loaders must retain the premium generic identity");
  Passed &= Check(
      (apex32::osidentity::ParseToken("ubuntu", 6U) ==
       apex32::OsIcon::Ubuntu) &&
          (apex32::osidentity::ParseToken("popos", 5U) ==
           apex32::OsIcon::PopOs) &&
          (apex32::osidentity::ParseToken("network", 7U) ==
           apex32::OsIcon::Network),
      "the pluggable registry must parse extended OS icon tokens");

  constexpr apex32::RgbColor kBackground{10U, 20U, 30U};
  constexpr apex32::RgbaColor kHalfWhite{255U, 255U, 255U, 128U};
  constexpr apex32::RgbColor kBlended =
      apex32::BlendColor(kBackground, kHalfWhite);
  static_assert(
      (kBlended.Red == 133U) && (kBlended.Green == 138U) &&
          (kBlended.Blue == 143U),
      "alpha blending must use stable rounded integer arithmetic");

  ModeInfo.HorizontalResolution = 8U;
  ModeInfo.VerticalResolution = 6U;
  ModeInfo.PixelsPerScanLine = 10U;
  apex32::GopRenderer Renderer;
  Passed &= Check(
      Renderer.Initialize() == EFI_SUCCESS,
      "renderer must initialize a bounded canonical GOP back buffer");
  Passed &= Check(
      (Renderer.Width() == 8U) && (Renderer.Height() == 6U) &&
          (Renderer.PixelsPerScanLine() == 10U),
      "renderer must retain resolution and GOP scan-line metadata");

  Renderer.Clear(kBackground);
  Renderer.PutPixel(MAX_UINTN, MAX_UINTN, {255U, 255U, 255U});
  Renderer.FillRectangle(7U, 5U, MAX_UINTN, MAX_UINTN, {255U, 0U, 0U});
  Renderer.BlendPixel(0U, 0U, kHalfWhite);
  Renderer.DrawRectangle(2U, 1U, 4U, 4U, 1U, {0U, 255U, 0U});
  Renderer.FillGradient(
      2U,
      0U,
      4U,
      1U,
      {0U, 0U, 255U},
      {255U, 0U, 0U},
      apex32::GradientDirection::Horizontal);
  Passed &= Check(
      Renderer.MeasureText("APEX32", 2U) == 70U,
      "text measurement must match the embedded 5x7 font geometry");
  Passed &= Check(
      Renderer.MeasureText("A", MAX_UINTN) == 0U,
      "overflowing text scales must be rejected");
  Passed &= Check(
      Renderer.Present() == EFI_SUCCESS,
      "renderer must present its canonical BLT buffer through GOP");
  Passed &= Check(
      (CapturedWidth == 8U) && (CapturedHeight == 6U),
      "test framebuffer must capture the complete rendered frame");
  Passed &= Check(
      (CapturedFrame[0].Red == 133U) &&
          (CapturedFrame[0].Green == 138U) &&
          (CapturedFrame[0].Blue == 143U),
      "alpha output must be written to the canonical GOP color channels");
  Passed &= Check(
      (CapturedFrame[2U].Blue == 255U) &&
          (CapturedFrame[5U].Red == 255U),
      "horizontal gradient endpoints must be preserved");
  Passed &= Check(
      CapturedFrame[(1U * 8U) + 2U].Green == 255U,
      "rectangle borders must render inside the requested bounds");
  Passed &= Check(
      (CapturedFrame[(2U * 8U) + 3U].Red == kBackground.Red) &&
          (CapturedFrame[(2U * 8U) + 3U].Green == kBackground.Green) &&
          (CapturedFrame[(2U * 8U) + 3U].Blue == kBackground.Blue),
      "rectangle interiors must remain unchanged");
  Passed &= Check(
      CapturedFrame[(5U * 8U) + 7U].Red == 255U,
      "oversized rectangles must clip to the final framebuffer pixel");
  Renderer.Clear({0U, 0U, 0U});
  Renderer.DrawTextAligned(
      "A", 7U, 0U, 1U, {255U, 255U, 255U},
      apex32::TextAlignment::Right);
  Passed &= Check(
      Renderer.Present() == EFI_SUCCESS &&
          (CapturedFrame[3U].Red == 255U) &&
          (CapturedFrame[4U].Red == 255U) &&
          (CapturedFrame[5U].Red == 255U) &&
          (CapturedFrame[2U].Red == 0U) &&
          (CapturedFrame[6U].Red == 0U),
      "right-aligned text must use measured embedded-font bounds");
  Renderer.Shutdown();

  ModeInfo.HorizontalResolution = 0U;
  ModeInfo.VerticalResolution = 6U;
  ModeInfo.PixelsPerScanLine = 0U;
  Passed &= Check(
      Renderer.Initialize() == EFI_UNSUPPORTED,
      "unsupported zero-dimension GOP modes must fail safely");
  ModeInfo.HorizontalResolution = MAX_UINTN;
  ModeInfo.VerticalResolution = 2U;
  ModeInfo.PixelsPerScanLine = MAX_UINTN;
  Passed &= Check(
      Renderer.Initialize() == EFI_BAD_BUFFER_SIZE,
      "overflowing GOP buffer dimensions must fail safely");

  ModeInfo.HorizontalResolution = 1920U;
  ModeInfo.VerticalResolution = 1080U;
  ModeInfo.PixelsPerScanLine = 1920U;
  State = {};
  NextKeyIndex = 0U;
  CapturedWidth = 0U;
  CapturedHeight = 0U;
  std::memset(CapturedFrame, 0, sizeof(CapturedFrame));
  return Passed;
}

int main() {
  EFI_BOOT_SERVICES BootServices{
      Stall,
      LocateProtocol,
      WaitForEvent,
      HandleProtocol,
      LoadImage,
      StartImage,
      UnloadImage,
  };
  gBS = &BootServices;

  bool Passed = RunFirmwareDiscoveryTests();
  Passed &= RunGraphicsFoundationTests();

  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL TextOutput{};
  TextOutput.OutputString = OutputString;
  TextOutput.EnableCursor = SetCursor;

  EFI_SYSTEM_TABLE SystemTable{};
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL TextInput{};
  TextInput.ReadKeyStroke = ReadKeyStroke;
  TextInput.WaitForKey = reinterpret_cast<EFI_EVENT>(1);
  SystemTable.ConIn = &TextInput;
  SystemTable.ConOut = &TextOutput;

  const EFI_STATUS Status = UefiMain(ParentImageHandle, &SystemTable);

  Passed &= Check(Status == EFI_SUCCESS, "UefiMain must succeed");
  Passed &= Check(
      State.FrameCount == 67,
      "diagnostics, both handoffs, menu states, and exit must present 67 frames");
  Passed &= Check(
      State.StallCount == 58,
      "brand sequence and hold must execute 58 timed stalls");
  Passed &= Check(
      State.FirstFrameColoredPixels == 0,
      "first frame must be black");
  Passed &= Check(
      State.MaximumColoredPixels > 0,
      "at least one frame must contain the title");
  Passed &= Check(
      State.LastFrameColoredPixels == 0,
      "Escape must clear the final frame to black");
  Passed &= Check(
      State.WaitForEventCount == 7,
      "menu must wait for all seven simulated key events");
  Passed &= Check(
      State.ReadKeyCount == 7,
      "menu must read all seven simulated keys");
  Passed &= Check(
      State.HandleProtocolCount == 4,
      "discovery and both handoffs must resolve their protocols");
  Passed &= Check(
      State.LoadedImageProtocolCount == 3,
      "discovery and both handoffs must resolve the loaded image");
  Passed &= Check(
      State.SimpleFileSystemProtocolCount == 1,
      "discovery must resolve the same-ESP filesystem once");
  Passed &= Check(
      State.OpenVolumeCount == 1,
      "discovery must open the ESP root exactly once");
  Passed &= Check(
      State.ProbeOpenCount == 3,
      "discovery must open config and probe both loader paths");
  Passed &= Check(
      State.ProbeCloseCount == 4,
      "discovery must close config, both loader files, and the ESP root");
  Passed &= Check(
      State.ConfigProbeMatched == TRUE,
      "discovery must open the canonical APEX32 configuration path");
  Passed &= Check(
      State.ConfigReadCount == 1,
      "discovery must read the configuration exactly once");
  Passed &= Check(
      State.KaliProbeMatched == TRUE,
      "discovery must probe the exact Kali path");
  Passed &= Check(
      State.BlackArchProbeMatched == TRUE,
      "discovery must probe the exact BlackArch path");
  Passed &= Check(
      State.FileDevicePathCount == 2,
      "both handoffs must construct same-ESP file paths");
  Passed &= Check(
      State.KaliPathMatched == TRUE,
      "Kali handoff must use the exact verified EFI path");
  Passed &= Check(
      State.BlackArchPathMatched == TRUE,
      "BlackArch handoff must use the exact verified EFI path");
  Passed &= Check(
      State.LoadImageCount == 2,
      "both handoffs must call LoadImage");
  Passed &= Check(
      State.StartImageCount == 2,
      "both handoffs must call StartImage");
  Passed &= Check(
      State.UnloadImageCount == 2,
      "both returned loaders must be unloaded");
  Passed &= Check(
      State.CursorChangeCount == 2,
      "cursor must be hidden and restored exactly once");
  Passed &= Check(
      State.CursorVisible == TRUE,
      "cursor must be restored before returning");
  Passed &= Check(
      State.FailureMessageCount == 0,
      "success path must not print a failure");

  State.SimulateBlackArchMissing = TRUE;
  const apex32::BootConfiguration PartialConfiguration =
      apex32::BootDiscovery::LoadSameEsp(ParentImageHandle);
  Passed &= Check(
      PartialConfiguration.Status == EFI_SUCCESS,
      "a missing loader must not fail configuration discovery");
  Passed &= Check(
      PartialConfiguration.Count == 2U,
      "both configured systems must remain visible");
  Passed &= Check(
      PartialConfiguration.Entries[0].Available == TRUE,
      "an existing loader must remain available");
  Passed &= Check(
      PartialConfiguration.Entries[1].Available == FALSE,
      "a missing BlackArch loader must be reported offline");

  constexpr CHAR8 kGenericConfiguration[] =
      "# public scanner output\n"
      "APEX32CFG|1\n"
      "ENTRY|WINDOWS BOOT MANAGER|\\EFI\\Microsoft\\Boot\\bootmgfw.efi|windows\n"
      "ENTRY|FEDORA LINUX|\\EFI\\fedora\\shimx64.efi|linux\n"
      "ENTRY|RECOVERY TOOL|\\EFI\\tools\\recovery.efi|unknown\n";
  apex32::BootConfiguration ParsedConfiguration{};
  const EFI_STATUS ParseStatus = apex32::BootConfig::ParseAscii(
      kGenericConfiguration,
      sizeof(kGenericConfiguration) - 1U,
      &ParsedConfiguration);
  Passed &= Check(
      ParseStatus == EFI_SUCCESS,
      "generic public configuration must parse");
  Passed &= Check(
      ParsedConfiguration.Count == 3U,
      "generic public configuration must retain all entries");
  Passed &= Check(
      ParsedConfiguration.Entries[0].Icon == apex32::OsIcon::Windows,
      "Windows icon identifier must be recognized");
  Passed &= Check(
      ParsedConfiguration.Entries[1].Icon == apex32::OsIcon::Linux,
      "Linux icon identifier must be recognized");
  Passed &= Check(
      ParsedConfiguration.Entries[2].Icon == apex32::OsIcon::Generic,
      "unknown icon identifier must fall back safely");

  CHAR8 ManyEntries[12288]{};
  UINTN ManyEntriesSize = 0U;
  constexpr CHAR8 kManyHeader[] = "APEX32CFG|1\n";
  std::memcpy(
      ManyEntries, kManyHeader, sizeof(kManyHeader) - 1U);
  ManyEntriesSize = sizeof(kManyHeader) - 1U;
  for (UINTN Index = 0U; Index < apex32::kMaximumBootEntries; ++Index) {
    const int Written = std::snprintf(
        ManyEntries + ManyEntriesSize,
        sizeof(ManyEntries) - ManyEntriesSize,
        "ENTRY|SYSTEM %zu|\\EFI\\vendor%zu\\bootx64.efi|generic\n",
        static_cast<std::size_t>(Index + 1U),
        static_cast<std::size_t>(Index + 1U));
    Passed &= Check(
        (Written > 0) &&
            (static_cast<UINTN>(Written) <
             (sizeof(ManyEntries) - ManyEntriesSize)),
        "many-entry test configuration must fit its bounded buffer");
    if (Written <= 0) {
      break;
    }
    ManyEntriesSize += static_cast<UINTN>(Written);
  }
  apex32::BootConfiguration ManyParsed{};
  Passed &= Check(
      (apex32::BootConfig::ParseAscii(
           ManyEntries, ManyEntriesSize, &ManyParsed) == EFI_SUCCESS) &&
          (ManyParsed.Count == apex32::kMaximumBootEntries),
      "configuration must support dozens of dynamically generated cards");
  const int ExtraWritten = std::snprintf(
      ManyEntries + ManyEntriesSize,
      sizeof(ManyEntries) - ManyEntriesSize,
      "ENTRY|OVERFLOW|\\EFI\\overflow\\bootx64.efi|generic\n");
  Passed &= Check(
      (ExtraWritten > 0) &&
          (static_cast<UINTN>(ExtraWritten) <
           (sizeof(ManyEntries) - ManyEntriesSize)),
      "overflow-entry test data must fit its host buffer");
  if (ExtraWritten > 0) {
    ManyEntriesSize += static_cast<UINTN>(ExtraWritten);
    apex32::BootConfiguration RejectedMany{};
    Passed &= Check(
        apex32::BootConfig::ParseAscii(
            ManyEntries, ManyEntriesSize, &RejectedMany) ==
            EFI_BAD_BUFFER_SIZE,
        "configuration beyond the bounded card limit must fail closed");
  }

  constexpr CHAR8 kMalformedConfiguration[] =
      "ENTRY|UNTRUSTED|\\EFI\\bad.efi|generic\n";
  apex32::BootConfiguration RejectedConfiguration{};
  Passed &= Check(
      EFI_ERROR(apex32::BootConfig::ParseAscii(
          kMalformedConfiguration,
          sizeof(kMalformedConfiguration) - 1U,
          &RejectedConfiguration)),
      "configuration without a schema header must be rejected");

  if (!Passed) {
    return EXIT_FAILURE;
  }

  std::printf(
      "PASS: %zu frames, 7 keys, dynamic config and manual boot paths clean\n",
      static_cast<std::size_t>(State.FrameCount));
  return EXIT_SUCCESS;
}
