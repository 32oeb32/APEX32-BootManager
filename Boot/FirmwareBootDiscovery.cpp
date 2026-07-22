#include "Boot/FirmwareBootDiscovery.hpp"

extern "C" {
#include <Guid/GlobalVariable.h>
#include <Library/UefiRuntimeServicesTableLib.h>
}

namespace apex32 {

namespace {

constexpr UINT32 kLoadOptionActive = 0x00000001U;
constexpr UINT8 kMediaDevicePath = 0x04U;
constexpr UINT8 kFilePathDevicePath = 0x04U;
constexpr UINT8 kEndDevicePath = 0x7FU;
constexpr UINT8 kEndEntireDevicePath = 0xFFU;
constexpr UINT8 kEndInstanceDevicePath = 0x01U;
constexpr UINTN kNodeHeaderSize = 4U;
constexpr UINTN kBootOrderCapacity = 512U;
constexpr UINTN kSeenBootNumberCapacity = kBootOrderCapacity / 2U;
constexpr UINTN kLoadOptionCapacity = 4096U;
constexpr UINTN kVariableNameCapacity = 512U;
constexpr UINTN kMaximumVariablesVisited = 4096U;

[[nodiscard]] UINT16 ReadUint16(const UINT8* Buffer) noexcept {
  return static_cast<UINT16>(
      static_cast<UINT16>(Buffer[0]) |
      static_cast<UINT16>(static_cast<UINT16>(Buffer[1]) << 8U));
}

[[nodiscard]] UINT32 ReadUint32(const UINT8* Buffer) noexcept {
  return static_cast<UINT32>(Buffer[0]) |
         (static_cast<UINT32>(Buffer[1]) << 8U) |
         (static_cast<UINT32>(Buffer[2]) << 16U) |
         (static_cast<UINT32>(Buffer[3]) << 24U);
}

[[nodiscard]] CHAR8 UpperAscii(const CHAR8 Character) noexcept {
  return ((Character >= 'a') && (Character <= 'z'))
             ? static_cast<CHAR8>(Character - ('a' - 'A'))
             : Character;
}

[[nodiscard]] BOOLEAN ContainsAscii(
    const CHAR8* Text,
    const CHAR8* Needle) noexcept {
  if ((Text == nullptr) || (Needle == nullptr) || (Needle[0] == '\0')) {
    return FALSE;
  }
  for (UINTN Start = 0U; Text[Start] != '\0'; ++Start) {
    UINTN Offset = 0U;
    while ((Needle[Offset] != '\0') && (Text[Start + Offset] != '\0') &&
           (UpperAscii(Text[Start + Offset]) == UpperAscii(Needle[Offset]))) {
      ++Offset;
    }
    if (Needle[Offset] == '\0') {
      return TRUE;
    }
  }
  return FALSE;
}

[[nodiscard]] BOOLEAN PathContainsAscii(
    const CHAR16* Text,
    const CHAR8* Needle) noexcept {
  if ((Text == nullptr) || (Needle == nullptr) || (Needle[0] == '\0')) {
    return FALSE;
  }
  for (UINTN Start = 0U; Text[Start] != 0U; ++Start) {
    UINTN Offset = 0U;
    while ((Needle[Offset] != '\0') && (Text[Start + Offset] != 0U) &&
           (Text[Start + Offset] <= 0x7FU) &&
           (UpperAscii(static_cast<CHAR8>(Text[Start + Offset])) ==
            UpperAscii(Needle[Offset]))) {
      ++Offset;
    }
    if (Needle[Offset] == '\0') {
      return TRUE;
    }
  }
  return FALSE;
}

[[nodiscard]] BOOLEAN IsApex32Entry(const BootEntry& Entry) noexcept {
  return (ContainsAscii(Entry.Name, "APEX32") ||
          PathContainsAscii(Entry.LoaderPath, "\\EFI\\APEX32\\"))
             ? TRUE
             : FALSE;
}

[[nodiscard]] BOOLEAN GuidEquals(
    const EFI_GUID& Left,
    const EFI_GUID& Right) noexcept {
  if ((Left.Data1 != Right.Data1) || (Left.Data2 != Right.Data2) ||
      (Left.Data3 != Right.Data3)) {
    return FALSE;
  }
  for (UINTN Index = 0U; Index < 8U; ++Index) {
    if (Left.Data4[Index] != Right.Data4[Index]) {
      return FALSE;
    }
  }
  return TRUE;
}

[[nodiscard]] CHAR16 HexDigit(const UINT16 Value) noexcept {
  return static_cast<CHAR16>(
      (Value < 10U) ? (static_cast<UINT16>('0') + Value)
                    : (static_cast<UINT16>('A') + (Value - 10U)));
}

void FormatBootVariableName(
    const UINT16 BootNumber,
    CHAR16* Name) noexcept {
  Name[0] = 'B';
  Name[1] = 'o';
  Name[2] = 'o';
  Name[3] = 't';
  Name[4] = HexDigit(static_cast<UINT16>((BootNumber >> 12U) & 0xFU));
  Name[5] = HexDigit(static_cast<UINT16>((BootNumber >> 8U) & 0xFU));
  Name[6] = HexDigit(static_cast<UINT16>((BootNumber >> 4U) & 0xFU));
  Name[7] = HexDigit(static_cast<UINT16>(BootNumber & 0xFU));
  Name[8] = 0;
}

[[nodiscard]] BOOLEAN ParseHexDigit(
    const CHAR16 Character,
    UINT16* Value) noexcept {
  if (Value == nullptr) {
    return FALSE;
  }
  if ((Character >= '0') && (Character <= '9')) {
    *Value = static_cast<UINT16>(Character - '0');
    return TRUE;
  }
  if ((Character >= 'A') && (Character <= 'F')) {
    *Value = static_cast<UINT16>(10U + Character - 'A');
    return TRUE;
  }
  if ((Character >= 'a') && (Character <= 'f')) {
    *Value = static_cast<UINT16>(10U + Character - 'a');
    return TRUE;
  }
  return FALSE;
}

[[nodiscard]] BOOLEAN ParseBootVariableName(
    const CHAR16* Name,
    UINT16* BootNumber) noexcept {
  if ((Name == nullptr) || (BootNumber == nullptr) ||
      (Name[0] != 'B') || (Name[1] != 'o') || (Name[2] != 'o') ||
      (Name[3] != 't') || (Name[8] != 0)) {
    return FALSE;
  }
  UINT16 Result = 0U;
  for (UINTN Index = 4U; Index < 8U; ++Index) {
    UINT16 Digit = 0U;
    if (!ParseHexDigit(Name[Index], &Digit)) {
      return FALSE;
    }
    Result = static_cast<UINT16>((Result << 4U) | Digit);
  }
  *BootNumber = Result;
  return TRUE;
}

void FormatFallbackName(
    const UINT16 BootNumber,
    CHAR8* Name) noexcept {
  constexpr CHAR8 kPrefix[] = "BOOT ";
  for (UINTN Index = 0U; Index < (sizeof(kPrefix) - 1U); ++Index) {
    Name[Index] = kPrefix[Index];
  }
  for (UINTN Index = 0U; Index < 4U; ++Index) {
    const UINT16 Shift = static_cast<UINT16>(12U - (4U * Index));
    Name[5U + Index] = static_cast<CHAR8>(
        HexDigit(static_cast<UINT16>((BootNumber >> Shift) & 0xFU)));
  }
  Name[9] = '\0';
}

[[nodiscard]] BOOLEAN CopyDescription(
    const UINT8* Buffer,
    const UINTN Start,
    const UINTN End,
    const UINT16 BootNumber,
    CHAR8* Name) noexcept {
  if ((Buffer == nullptr) || (Name == nullptr) || (End < Start) ||
      (((End - Start) & 1U) != 0U)) {
    return FALSE;
  }
  UINTN Output = 0U;
  for (UINTN Offset = Start;
       (Offset < End) && (Output + 1U < kBootEntryNameCapacity);
       Offset += 2U) {
    const UINT16 Character = ReadUint16(Buffer + Offset);
    if (Character == 0U) {
      break;
    }
    Name[Output++] = ((Character >= 0x20U) && (Character <= 0x7EU))
                         ? static_cast<CHAR8>(Character)
                         : '?';
  }
  if (Output == 0U) {
    FormatFallbackName(BootNumber, Name);
  } else {
    Name[Output] = '\0';
  }
  return TRUE;
}

[[nodiscard]] EFI_STATUS ValidateAndExtractDevicePath(
    const UINT8* Buffer,
    const UINTN BufferSize,
    CHAR16* LoaderPath) noexcept {
  if ((Buffer == nullptr) || (LoaderPath == nullptr) ||
      (BufferSize < kNodeHeaderSize)) {
    return EFI_LOAD_ERROR;
  }
  LoaderPath[0] = 0;
  BOOLEAN EndEntireSeen = FALSE;
  UINTN Offset = 0U;
  while (Offset < BufferSize) {
    if ((BufferSize - Offset) < kNodeHeaderSize) {
      return EFI_LOAD_ERROR;
    }
    const UINT8 Type = Buffer[Offset];
    const UINT8 SubType = Buffer[Offset + 1U];
    const UINTN NodeLength = ReadUint16(Buffer + Offset + 2U);
    if ((NodeLength < kNodeHeaderSize) || (NodeLength > (BufferSize - Offset))) {
      return EFI_LOAD_ERROR;
    }

    if ((Type == kMediaDevicePath) && (SubType == kFilePathDevicePath) &&
        (LoaderPath[0] == 0U)) {
      if (((NodeLength - kNodeHeaderSize) & 1U) != 0U) {
        return EFI_LOAD_ERROR;
      }
      const UINTN CharacterCapacity = (NodeLength - kNodeHeaderSize) / 2U;
      UINTN Output = 0U;
      BOOLEAN TerminatorSeen = FALSE;
      for (UINTN CharacterIndex = 0U;
           CharacterIndex < CharacterCapacity;
           ++CharacterIndex) {
        const UINT16 Character = ReadUint16(
            Buffer + Offset + kNodeHeaderSize + (CharacterIndex * 2U));
        if (Character == 0U) {
          TerminatorSeen = TRUE;
          break;
        }
        if (Output + 1U < kBootEntryPathCapacity) {
          LoaderPath[Output++] = static_cast<CHAR16>(Character);
        }
      }
      LoaderPath[Output] = 0;
      if (!TerminatorSeen) {
        return EFI_LOAD_ERROR;
      }
    }

    Offset += NodeLength;
    if (Type == kEndDevicePath) {
      if ((NodeLength != kNodeHeaderSize) ||
          ((SubType != kEndEntireDevicePath) &&
           (SubType != kEndInstanceDevicePath))) {
        return EFI_LOAD_ERROR;
      }
      if (SubType == kEndEntireDevicePath) {
        EndEntireSeen = TRUE;
        if (Offset != BufferSize) {
          return EFI_LOAD_ERROR;
        }
      }
    }
  }
  return EndEntireSeen ? EFI_SUCCESS : EFI_LOAD_ERROR;
}

[[nodiscard]] BOOLEAN AlreadySeen(
    const UINT16* Seen,
    const UINTN SeenCount,
    const UINT16 BootNumber) noexcept {
  for (UINTN Index = 0U; Index < SeenCount; ++Index) {
    if (Seen[Index] == BootNumber) {
      return TRUE;
    }
  }
  return FALSE;
}

void TryAppendBootOption(
    const UINT16 BootNumber,
    UINT16* Seen,
    UINTN* SeenCount,
    BootConfiguration* Configuration,
    EFI_STATUS* LastError) noexcept {
  if ((Seen == nullptr) || (SeenCount == nullptr) ||
      (Configuration == nullptr) || (LastError == nullptr) ||
      (*SeenCount >= kSeenBootNumberCapacity) ||
      (Configuration->Count >= kMaximumBootEntries) ||
      AlreadySeen(Seen, *SeenCount, BootNumber)) {
    return;
  }
  Seen[*SeenCount] = BootNumber;
  ++(*SeenCount);

  CHAR16 VariableName[9]{};
  FormatBootVariableName(BootNumber, VariableName);
  alignas(8) UINT8 Buffer[kLoadOptionCapacity]{};
  UINTN BufferSize = sizeof(Buffer);
  UINT32 Attributes = 0U;
  const EFI_STATUS Status = gRT->GetVariable(
      VariableName,
      &gEfiGlobalVariableGuid,
      &Attributes,
      &BufferSize,
      Buffer);
  if (EFI_ERROR(Status)) {
    *LastError = Status;
    return;
  }

  BootEntry Entry{};
  const EFI_STATUS ParseStatus = FirmwareBootDiscovery::ParseLoadOption(
      BootNumber, Buffer, BufferSize, &Entry);
  if (EFI_ERROR(ParseStatus)) {
    if (ParseStatus != EFI_NOT_READY) {
      *LastError = ParseStatus;
    }
    return;
  }
  if (IsApex32Entry(Entry)) {
    return;
  }
  Configuration->Entries[Configuration->Count] = Entry;
  ++Configuration->Count;
  ++Configuration->FirmwareCount;
}

}  // namespace

EFI_STATUS FirmwareBootDiscovery::ParseLoadOption(
    const UINT16 BootNumber,
    const UINT8* Buffer,
    const UINTN BufferSize,
    BootEntry* Entry) noexcept {
  if ((Buffer == nullptr) || (Entry == nullptr) || (BufferSize < 8U)) {
    return EFI_INVALID_PARAMETER;
  }
  *Entry = {};
  const UINT32 Attributes = ReadUint32(Buffer);
  if ((Attributes & kLoadOptionActive) == 0U) {
    return EFI_NOT_READY;
  }
  const UINTN DevicePathSize = ReadUint16(Buffer + 4U);
  if ((DevicePathSize < kNodeHeaderSize) ||
      (DevicePathSize > kBootEntryDevicePathCapacity)) {
    return EFI_BAD_BUFFER_SIZE;
  }

  UINTN DescriptionEnd = 6U;
  BOOLEAN TerminatorSeen = FALSE;
  while ((DescriptionEnd + 1U) < BufferSize) {
    if (ReadUint16(Buffer + DescriptionEnd) == 0U) {
      TerminatorSeen = TRUE;
      break;
    }
    DescriptionEnd += 2U;
  }
  if (!TerminatorSeen || (DescriptionEnd > (MAX_UINTN - 2U))) {
    return EFI_LOAD_ERROR;
  }
  const UINTN DevicePathStart = DescriptionEnd + 2U;
  if ((DevicePathStart > BufferSize) ||
      (DevicePathSize > (BufferSize - DevicePathStart))) {
    return EFI_BAD_BUFFER_SIZE;
  }
  if (!CopyDescription(
          Buffer, 6U, DescriptionEnd, BootNumber, Entry->Name)) {
    return EFI_LOAD_ERROR;
  }
  const EFI_STATUS PathStatus = ValidateAndExtractDevicePath(
      Buffer + DevicePathStart, DevicePathSize, Entry->LoaderPath);
  if (EFI_ERROR(PathStatus)) {
    return PathStatus;
  }
  for (UINTN Index = 0U; Index < DevicePathSize; ++Index) {
    Entry->DevicePath[Index] = Buffer[DevicePathStart + Index];
  }
  Entry->Icon = OsIcon::Generic;
  Entry->Available = TRUE;
  Entry->Source = BootEntrySource::Firmware;
  Entry->FirmwareBootNumber = BootNumber;
  Entry->DevicePathSize = static_cast<UINT16>(DevicePathSize);
  return EFI_SUCCESS;
}

EFI_STATUS FirmwareBootDiscovery::Discover(
    BootConfiguration* Configuration) noexcept {
  if (Configuration == nullptr) {
    return EFI_INVALID_PARAMETER;
  }
  *Configuration = {};
  Configuration->FirmwareStatus = EFI_UNSUPPORTED;
  if ((gRT == nullptr) || (gRT->GetVariable == nullptr) ||
      (gRT->GetNextVariableName == nullptr)) {
    return EFI_UNSUPPORTED;
  }

  UINT16 Seen[kSeenBootNumberCapacity]{};
  UINTN SeenCount = 0U;
  EFI_STATUS LastError = EFI_NOT_FOUND;
  CHAR16 BootOrderName[] = {
      'B', 'o', 'o', 't', 'O', 'r', 'd', 'e', 'r', 0,
  };
  alignas(8) UINT8 BootOrder[kBootOrderCapacity]{};
  UINTN BootOrderSize = sizeof(BootOrder);
  UINT32 Attributes = 0U;
  const EFI_STATUS OrderStatus = gRT->GetVariable(
      BootOrderName,
      &gEfiGlobalVariableGuid,
      &Attributes,
      &BootOrderSize,
      BootOrder);
  if (!EFI_ERROR(OrderStatus) && ((BootOrderSize & 1U) == 0U)) {
    for (UINTN Offset = 0U;
         (Offset < BootOrderSize) &&
         (Configuration->Count < kMaximumBootEntries);
         Offset += 2U) {
      TryAppendBootOption(
          ReadUint16(BootOrder + Offset),
          Seen,
          &SeenCount,
          Configuration,
          &LastError);
    }
  } else if (OrderStatus != EFI_NOT_FOUND) {
    LastError = EFI_ERROR(OrderStatus) ? OrderStatus : EFI_LOAD_ERROR;
  }

  CHAR16 VariableName[kVariableNameCapacity]{};
  EFI_GUID VendorGuid{};
  for (UINTN Visited = 0U;
       (Visited < kMaximumVariablesVisited) &&
       (Configuration->Count < kMaximumBootEntries);
       ++Visited) {
    UINTN VariableNameSize = sizeof(VariableName);
    const EFI_STATUS Status = gRT->GetNextVariableName(
        &VariableNameSize, VariableName, &VendorGuid);
    if (Status == EFI_NOT_FOUND) {
      break;
    }
    if (EFI_ERROR(Status)) {
      LastError = Status;
      break;
    }
    UINT16 BootNumber = 0U;
    if (GuidEquals(VendorGuid, gEfiGlobalVariableGuid) &&
        ParseBootVariableName(VariableName, &BootNumber)) {
      TryAppendBootOption(
          BootNumber,
          Seen,
          &SeenCount,
          Configuration,
          &LastError);
    }
  }

  Configuration->Loaded = TRUE;
  Configuration->FirmwareStatus =
      (Configuration->FirmwareCount > 0U) ? EFI_SUCCESS : LastError;
  Configuration->Status = Configuration->FirmwareStatus;
  return Configuration->FirmwareStatus;
}

}  // namespace apex32
