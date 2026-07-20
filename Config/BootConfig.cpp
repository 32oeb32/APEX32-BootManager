#include "Config/BootConfig.hpp"

namespace apex32 {

namespace {

[[nodiscard]] BOOLEAN EqualsIgnoreCase(
    const CHAR8* Left,
    const UINTN LeftLength,
    const CHAR8* Right) noexcept {
  if ((Left == nullptr) || (Right == nullptr)) {
    return FALSE;
  }

  UINTN Index = 0;
  while ((Index < LeftLength) && (Right[Index] != '\0')) {
    CHAR8 LeftCharacter = Left[Index];
    CHAR8 RightCharacter = Right[Index];
    if ((LeftCharacter >= 'a') && (LeftCharacter <= 'z')) {
      LeftCharacter = static_cast<CHAR8>(LeftCharacter - ('a' - 'A'));
    }
    if ((RightCharacter >= 'a') && (RightCharacter <= 'z')) {
      RightCharacter = static_cast<CHAR8>(RightCharacter - ('a' - 'A'));
    }
    if (LeftCharacter != RightCharacter) {
      return FALSE;
    }
    ++Index;
  }

  return ((Index == LeftLength) && (Right[Index] == '\0')) ? TRUE : FALSE;
}

[[nodiscard]] OsIcon ParseIcon(
    const CHAR8* Text,
    const UINTN Length) noexcept {
  if (EqualsIgnoreCase(Text, Length, "WINDOWS")) {
    return OsIcon::Windows;
  }
  if (EqualsIgnoreCase(Text, Length, "KALI")) {
    return OsIcon::Kali;
  }
  if (EqualsIgnoreCase(Text, Length, "BLACKARCH")) {
    return OsIcon::BlackArch;
  }
  if (EqualsIgnoreCase(Text, Length, "LINUX")) {
    return OsIcon::Linux;
  }
  return OsIcon::Generic;
}

[[nodiscard]] BOOLEAN CopyName(
    const CHAR8* Source,
    const UINTN Length,
    CHAR8* Destination) noexcept {
  if ((Source == nullptr) || (Destination == nullptr) || (Length == 0U) ||
      (Length >= kBootEntryNameCapacity)) {
    return FALSE;
  }

  for (UINTN Index = 0; Index < Length; ++Index) {
    CHAR8 Character = Source[Index];
    if ((Character < 0x20) || (Character > 0x7E)) {
      return FALSE;
    }
    if ((Character >= 'a') && (Character <= 'z')) {
      Character = static_cast<CHAR8>(Character - ('a' - 'A'));
    }
    Destination[Index] = Character;
  }
  Destination[Length] = '\0';
  return TRUE;
}

[[nodiscard]] BOOLEAN CopyPath(
    const CHAR8* Source,
    const UINTN Length,
    CHAR16* Destination) noexcept {
  if ((Source == nullptr) || (Destination == nullptr) || (Length < 2U) ||
      (Length >= kBootEntryPathCapacity) || (Source[0] != '\\')) {
    return FALSE;
  }

  for (UINTN Index = 0; Index < Length; ++Index) {
    const CHAR8 Character = Source[Index];
    if ((Character < 0x20) || (Character > 0x7E) ||
        (Character == '|')) {
      return FALSE;
    }
    Destination[Index] = static_cast<CHAR16>(Character);
  }
  Destination[Length] = 0;
  return TRUE;
}

[[nodiscard]] BOOLEAN IsHeader(
    const CHAR8* Line,
    const UINTN Length) noexcept {
  constexpr CHAR8 kHeader[] = "APEX32CFG|1";
  if (Length != (sizeof(kHeader) - 1U)) {
    return FALSE;
  }
  for (UINTN Index = 0; Index < Length; ++Index) {
    if (Line[Index] != kHeader[Index]) {
      return FALSE;
    }
  }
  return TRUE;
}

[[nodiscard]] EFI_STATUS ParseEntry(
    const CHAR8* Line,
    const UINTN Length,
    BootEntry* Entry) noexcept {
  if ((Line == nullptr) || (Entry == nullptr) || (Length < 8U)) {
    return EFI_LOAD_ERROR;
  }

  constexpr CHAR8 kPrefix[] = "ENTRY|";
  for (UINTN Index = 0; Index < (sizeof(kPrefix) - 1U); ++Index) {
    if ((Index >= Length) || (Line[Index] != kPrefix[Index])) {
      return EFI_LOAD_ERROR;
    }
  }

  const UINTN NameStart = sizeof(kPrefix) - 1U;
  UINTN NameEnd = NameStart;
  while ((NameEnd < Length) && (Line[NameEnd] != '|')) {
    ++NameEnd;
  }
  if (NameEnd >= Length) {
    return EFI_LOAD_ERROR;
  }

  const UINTN PathStart = NameEnd + 1U;
  UINTN PathEnd = PathStart;
  while ((PathEnd < Length) && (Line[PathEnd] != '|')) {
    ++PathEnd;
  }
  if (PathEnd >= Length) {
    return EFI_LOAD_ERROR;
  }

  const UINTN IconStart = PathEnd + 1U;
  if ((IconStart >= Length) ||
      !CopyName(Line + NameStart, NameEnd - NameStart, Entry->Name) ||
      !CopyPath(Line + PathStart, PathEnd - PathStart, Entry->LoaderPath)) {
    return EFI_LOAD_ERROR;
  }

  Entry->Icon = ParseIcon(Line + IconStart, Length - IconStart);
  Entry->Available = FALSE;
  return EFI_SUCCESS;
}

}  // namespace

EFI_STATUS BootConfig::ParseAscii(
    const CHAR8* Buffer,
    const UINTN BufferSize,
    BootConfiguration* Configuration) noexcept {
  if ((Buffer == nullptr) || (Configuration == nullptr)) {
    return EFI_UNSUPPORTED;
  }

  *Configuration = {};
  BOOLEAN HeaderSeen = FALSE;
  UINTN Offset = 0;

  while (Offset < BufferSize) {
    const UINTN LineStart = Offset;
    while ((Offset < BufferSize) && (Buffer[Offset] != '\n') &&
           (Buffer[Offset] != '\0')) {
      ++Offset;
    }

    UINTN LineLength = Offset - LineStart;
    if ((LineLength > 0U) && (Buffer[LineStart + LineLength - 1U] == '\r')) {
      --LineLength;
    }

    if ((Offset < BufferSize) && (Buffer[Offset] != '\0')) {
      ++Offset;
    } else if ((Offset < BufferSize) && (Buffer[Offset] == '\0')) {
      Offset = BufferSize;
    }

    if ((LineLength == 0U) || (Buffer[LineStart] == '#')) {
      continue;
    }

    if (!HeaderSeen) {
      if (!IsHeader(Buffer + LineStart, LineLength)) {
        return EFI_LOAD_ERROR;
      }
      HeaderSeen = TRUE;
      continue;
    }

    if (Configuration->Count >= kMaximumBootEntries) {
      return EFI_BAD_BUFFER_SIZE;
    }

    EFI_STATUS Status = ParseEntry(
        Buffer + LineStart,
        LineLength,
        &Configuration->Entries[Configuration->Count]);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    ++Configuration->Count;
  }

  if (!HeaderSeen) {
    return EFI_LOAD_ERROR;
  }

  Configuration->Loaded = TRUE;
  Configuration->Status = EFI_SUCCESS;
  return EFI_SUCCESS;
}

}  // namespace apex32
