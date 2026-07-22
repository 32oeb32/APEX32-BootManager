#include "Assets/OsIdentity.hpp"

#include "Assets/OsLogos.hpp"
#include "Renderer/GopRenderer.hpp"

namespace apex32::osidentity {

namespace {

constexpr Descriptor kDescriptors[] = {
    {OsIcon::Generic, "generic", "EFI", {0xB8, 0xC2, 0xCC}},
    {OsIcon::Linux, "linux", "LNX", {0x21, 0xD4, 0xEA}},
    {OsIcon::Windows, "windows", "WIN", {0x36, 0x9C, 0xFF}},
    {OsIcon::Kali, "kali", "KALI", {0x21, 0xD4, 0xEA}},
    {OsIcon::BlackArch, "blackarch", "BA", {0xF0, 0x8A, 0x32}},
    {OsIcon::Ubuntu, "ubuntu", "UBU", {0xE9, 0x54, 0x20}},
    {OsIcon::Fedora, "fedora", "FED", {0x51, 0xA2, 0xDA}},
    {OsIcon::Arch, "arch", "ARCH", {0x6B, 0xC7, 0xF0}},
    {OsIcon::Debian, "debian", "DEB", {0xD7, 0x0A, 0x53}},
    {OsIcon::Mint, "mint", "MINT", {0x79, 0xC7, 0x4B}},
    {OsIcon::OpenSuse, "opensuse", "SUSE", {0x73, 0xBA, 0x25}},
    {OsIcon::PopOs, "popos", "POP", {0x48, 0xB9, 0xC7}},
    {OsIcon::OpenCore, "opencore", "OC", {0xC8, 0xD0, 0xD8}},
    {OsIcon::Recovery, "recovery", "REC", {0xF0, 0xB4, 0x48}},
    {OsIcon::Usb, "usb", "USB", {0x91, 0xD8, 0xE8}},
    {OsIcon::Network, "network", "NET", {0x5B, 0xA7, 0xFF}},
};

[[nodiscard]] CHAR8 UpperAscii(const CHAR8 Character) noexcept {
  return ((Character >= 'a') && (Character <= 'z'))
             ? static_cast<CHAR8>(Character - ('a' - 'A'))
             : Character;
}

[[nodiscard]] BOOLEAN EqualsIgnoreCase(
    const CHAR8* Left,
    const UINTN LeftLength,
    const CHAR8* Right) noexcept {
  if ((Left == nullptr) || (Right == nullptr)) {
    return FALSE;
  }
  UINTN Index = 0U;
  while ((Index < LeftLength) && (Right[Index] != '\0')) {
    if (UpperAscii(Left[Index]) != UpperAscii(Right[Index])) {
      return FALSE;
    }
    ++Index;
  }
  return ((Index == LeftLength) && (Right[Index] == '\0')) ? TRUE : FALSE;
}

[[nodiscard]] BOOLEAN ContainsIgnoreCase(
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

[[nodiscard]] BOOLEAN PathContainsIgnoreCase(
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

[[nodiscard]] const Descriptor& ByIcon(const OsIcon Icon) noexcept {
  for (UINTN Index = 0U;
       Index < (sizeof(kDescriptors) / sizeof(kDescriptors[0]));
       ++Index) {
    if (kDescriptors[Index].Icon == Icon) {
      return kDescriptors[Index];
    }
  }
  return kDescriptors[0];
}

[[nodiscard]] BOOLEAN EntryContains(
    const BootEntry& Entry,
    const CHAR8* Text) noexcept {
  return (ContainsIgnoreCase(Entry.Name, Text) ||
          PathContainsIgnoreCase(Entry.LoaderPath, Text))
             ? TRUE
             : FALSE;
}

void DrawCenteredBadge(
    GopRenderer& Renderer,
    const CHAR8* Badge,
    const UINTN CenterX,
    const UINTN Top,
    const UINTN Size,
    const RgbColor Color) noexcept {
  UINTN Scale = Size / 34U;
  if (Scale == 0U) {
    Scale = 1U;
  }
  UINTN Width = Renderer.MeasureText(Badge, Scale);
  while ((Width > (Size - (Size / 5U))) && (Scale > 1U)) {
    --Scale;
    Width = Renderer.MeasureText(Badge, Scale);
  }
  const UINTN X = (CenterX >= (Width / 2U)) ? (CenterX - (Width / 2U)) : 0U;
  const UINTN Height = 7U * Scale;
  const UINTN Y = Top + ((Size > Height) ? ((Size - Height) / 2U) : 0U);
  Renderer.DrawText(Badge, X, Y, Scale, Color);
}

void DrawGeometricMark(
    GopRenderer& Renderer,
    const Descriptor& Identity,
    const UINTN CenterX,
    const UINTN Top,
    const UINTN Size,
    const RgbColor Color) noexcept {
  const UINTN X = (CenterX >= (Size / 2U)) ? (CenterX - (Size / 2U)) : 0U;
  const UINTN Thickness = (Size >= 48U) ? (Size / 24U) : 1U;
  const UINTN Quarter = Size / 4U;
  const UINTN Half = Size / 2U;
  Renderer.DrawLine(X + Quarter, Top, X + Size - Quarter, Top, Thickness, Color);
  Renderer.DrawLine(
      X + Size - Quarter, Top, X + Size, Top + Half, Thickness, Color);
  Renderer.DrawLine(
      X + Size, Top + Half, X + Size - Quarter, Top + Size, Thickness, Color);
  Renderer.DrawLine(
      X + Size - Quarter, Top + Size, X + Quarter, Top + Size, Thickness, Color);
  Renderer.DrawLine(X + Quarter, Top + Size, X, Top + Half, Thickness, Color);
  Renderer.DrawLine(X, Top + Half, X + Quarter, Top, Thickness, Color);
  Renderer.DrawRectangle(
      X + (Size / 10U),
      Top + (Size / 10U),
      Size - (Size / 5U),
      Size - (Size / 5U),
      Thickness,
      ScaleColor(Color, 120U));
  DrawCenteredBadge(Renderer, Identity.Badge, CenterX, Top, Size, Color);
}

void DrawWindows(
    GopRenderer& Renderer,
    const UINTN CenterX,
    const UINTN Top,
    const UINTN Size,
    const RgbColor Color) noexcept {
  const UINTN X = (CenterX >= (Size / 2U)) ? (CenterX - (Size / 2U)) : 0U;
  const UINTN Gap = (Size >= 12U) ? (Size / 12U) : 1U;
  const UINTN Pane = (Size - Gap) / 2U;
  Renderer.FillRectangle(X, Top, Pane, Pane, Color);
  Renderer.FillRectangle(X + Pane + Gap, Top, Pane, Pane, Color);
  Renderer.FillRectangle(X, Top + Pane + Gap, Pane, Pane, Color);
  Renderer.FillRectangle(X + Pane + Gap, Top + Pane + Gap, Pane, Pane, Color);
}

void DrawBitmap(
    GopRenderer& Renderer,
    const oslogos::MonochromeLogo& Logo,
    const UINTN CenterX,
    const UINTN Top,
    const UINTN Size,
    const RgbColor Color) noexcept {
  UINTN Scale = Size / Logo.Width;
  if (Scale == 0U) {
    Scale = 1U;
  }
  const UINTN Width = Logo.Width * Scale;
  const UINTN Height = Logo.Height * Scale;
  const UINTN X = (CenterX >= (Width / 2U)) ? (CenterX - (Width / 2U)) : 0U;
  const UINTN Y = Top + ((Size > Height) ? ((Size - Height) / 2U) : 0U);
  Renderer.DrawMonochromeBitmap(
      Logo.Data,
      Logo.Width,
      Logo.Height,
      Logo.BytesPerRow,
      X,
      Y,
      Scale,
      Color);
}

}  // namespace

OsIcon ParseToken(const CHAR8* Text, const UINTN Length) noexcept {
  for (UINTN Index = 0U;
       Index < (sizeof(kDescriptors) / sizeof(kDescriptors[0]));
       ++Index) {
    if (EqualsIgnoreCase(Text, Length, kDescriptors[Index].Token)) {
      return kDescriptors[Index].Icon;
    }
  }
  return OsIcon::Generic;
}

const Descriptor& Resolve(const BootEntry& Entry) noexcept {
  if ((Entry.Icon != OsIcon::Generic) && (Entry.Icon != OsIcon::Linux)) {
    return ByIcon(Entry.Icon);
  }
  if (EntryContains(Entry, "BLACKARCH")) {
    return ByIcon(OsIcon::BlackArch);
  }
  if (EntryContains(Entry, "KALI")) {
    return ByIcon(OsIcon::Kali);
  }
  if (EntryContains(Entry, "WINDOWS") || EntryContains(Entry, "MICROSOFT")) {
    return ByIcon(OsIcon::Windows);
  }
  if (EntryContains(Entry, "UBUNTU")) {
    return ByIcon(OsIcon::Ubuntu);
  }
  if (EntryContains(Entry, "FEDORA")) {
    return ByIcon(OsIcon::Fedora);
  }
  if (EntryContains(Entry, "LINUX MINT") || EntryContains(Entry, "MINT")) {
    return ByIcon(OsIcon::Mint);
  }
  if (EntryContains(Entry, "OPENSUSE") || EntryContains(Entry, "SUSE")) {
    return ByIcon(OsIcon::OpenSuse);
  }
  if (EntryContains(Entry, "POP!_OS") || EntryContains(Entry, "POPOS")) {
    return ByIcon(OsIcon::PopOs);
  }
  if (EntryContains(Entry, "ARCH")) {
    return ByIcon(OsIcon::Arch);
  }
  if (EntryContains(Entry, "DEBIAN")) {
    return ByIcon(OsIcon::Debian);
  }
  if (EntryContains(Entry, "OPENCORE") || EntryContains(Entry, "DARWIN") ||
      EntryContains(Entry, "MACOS")) {
    return ByIcon(OsIcon::OpenCore);
  }
  if (EntryContains(Entry, "RECOVERY") || EntryContains(Entry, "RESCUE")) {
    return ByIcon(OsIcon::Recovery);
  }
  if (EntryContains(Entry, "USB")) {
    return ByIcon(OsIcon::Usb);
  }
  if (EntryContains(Entry, "NETWORK") || EntryContains(Entry, "PXE")) {
    return ByIcon(OsIcon::Network);
  }
  return ByIcon(Entry.Icon);
}

void Draw(
    GopRenderer& Renderer,
    const BootEntry& Entry,
    const UINTN CenterX,
    const UINTN Top,
    const UINTN Size,
    const RgbColor Color) noexcept {
  if (Size == 0U) {
    return;
  }
  const Descriptor& Identity = Resolve(Entry);
  if (Identity.Icon == OsIcon::Kali) {
    DrawBitmap(Renderer, oslogos::kKali, CenterX, Top, Size, Color);
    return;
  }
  if (Identity.Icon == OsIcon::BlackArch) {
    DrawBitmap(Renderer, oslogos::kBlackArch, CenterX, Top, Size, Color);
    return;
  }
  if (Identity.Icon == OsIcon::Windows) {
    DrawWindows(Renderer, CenterX, Top, Size, Color);
    return;
  }
  DrawGeometricMark(Renderer, Identity, CenterX, Top, Size, Color);
}

}  // namespace apex32::osidentity
