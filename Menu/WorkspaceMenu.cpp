#include "Menu/WorkspaceMenu.hpp"

extern "C" {
#include <Library/UefiBootServicesTableLib.h>
}

#include "Assets/OsLogos.hpp"
#include "Boot/BootDiscovery.hpp"
#include "Boot/EfiLoader.hpp"
#include "Fonts/Font5x7.hpp"
#include "Renderer/GopRenderer.hpp"
#include "Themes/DefaultTheme.hpp"

namespace apex32 {

namespace {

constexpr CHAR8 kBrand[] = "APEX32";
constexpr CHAR8 kIdentity[] = "SECURE GATEWAY";
constexpr CHAR8 kInstruction[] = "CHOOSE YOUR OPERATING SYSTEM";
constexpr CHAR8 kRuntimeLabel[] = "SYSTEM STATUS";
constexpr CHAR8 kRuntimeValue[] = "PROTECTED";
constexpr CHAR8 kFirmwareLabel[] = "FIRMWARE MODE";
constexpr CHAR8 kFirmwareValue[] = "UEFI X64";
constexpr CHAR8 kTelemetryReady[] =
    "GOP ACTIVE   CONFIGURATION VERIFIED   MANUAL CONTROL";
constexpr CHAR8 kTelemetryEmpty[] =
    "GOP ACTIVE   INSTALLER CONFIGURATION REQUIRED";
constexpr CHAR8 kWebsite[] = "APEX32-SECURE.COM";
constexpr CHAR8 kWebsiteLine[] =
    "OPEN SOURCE // DOCUMENTATION // RECOVERY";
constexpr CHAR8 kSelectedReady[] = "READY // ENTER TO BOOT";
constexpr CHAR8 kSelectedOffline[] = "LOADER OFFLINE // RESCAN IN INSTALLER";
constexpr CHAR8 kNoSystems[] =
    "NO SYSTEMS CONFIGURED // OPEN INSTALLER AND SELECT SCAN NOW";
constexpr CHAR8 kConfigPath[] = "\\EFI\\APEX32\\APEX32.CFG";
constexpr CHAR8 kConfigOffline[] = "CONFIGURATION NOT AVAILABLE";
constexpr CHAR8 kHelp[] =
    "ARROWS / TAB  MOVE     ENTER  BOOT     F2  STATUS     ESC  RETURN";
constexpr CHAR8 kEmptyHelp[] =
    "APEX32 INSTALLER  >  SYSTEMS  >  SCAN NOW";
constexpr CHAR8 kVersion[] = "ABM CE 0.10.0-A1";
constexpr CHAR8 kRuntime[] = "GPL-3.0 // COMMUNITY";
constexpr CHAR8 kLaunchEyebrow[] = "APEX32 SECURE BOOT CHANNEL";
constexpr CHAR8 kLaunchSubtitle[] = "DIRECT UEFI HANDOFF";
constexpr CHAR8 kLaunchTelemetry[] =
    "LOADIMAGE  >  STARTIMAGE  >  OPERATING SYSTEM";
constexpr CHAR8 kLaunchSafety[] =
    "VERIFIED CONFIG PATH   //   SAFE RETURN ON EFI ERROR";
constexpr CHAR8 kDiagnosticsEyebrow[] = "APEX32 READ-ONLY CONTROL PLANE";
constexpr CHAR8 kDiagnosticsTitle[] = "SYSTEM DIAGNOSTICS";
constexpr CHAR8 kDiagnosticsSubtitle[] =
    "COMMUNITY CONFIGURATION // NO AUTOMATIC BOOT";
constexpr CHAR8 kManualPolicy[] = "MANUAL SELECTION ONLY";
constexpr CHAR8 kDisabled[] = "DISABLED";
constexpr CHAR8 kVerified[] = "VERIFIED";
constexpr CHAR8 kOffline[] = "OFFLINE";
constexpr CHAR8 kReadOnly[] = "READ ONLY";
constexpr CHAR8 kNone[] = "NONE";
constexpr CHAR8 kNotAvailable[] = "NOT AVAILABLE";
constexpr CHAR8 kDiagnosticsHelp[] =
    "F2 / ESC  RETURN TO SECURE GATEWAY";

[[nodiscard]] UINTN AtLeastOne(const UINTN Value) noexcept {
  return (Value == 0U) ? 1U : Value;
}

[[nodiscard]] UINTN ResponsiveUnit(const GopRenderer& Renderer) noexcept {
  const UINTN WidthScale = Renderer.Width() / 96U;
  const UINTN HeightScale = Renderer.Height() / 54U;
  const UINTN Scale = (WidthScale < HeightScale) ? WidthScale : HeightScale;
  return AtLeastOne(Scale);
}

void AppendText(
    CHAR8* Buffer,
    const UINTN Capacity,
    UINTN* Offset,
    const CHAR8* Text) noexcept {
  if ((Buffer == nullptr) || (Offset == nullptr) || (Text == nullptr) ||
      (Capacity == 0U)) {
    return;
  }

  UINTN Index = 0;
  while ((Text[Index] != '\0') && ((*Offset + 1U) < Capacity)) {
    Buffer[*Offset] = Text[Index];
    ++(*Offset);
    ++Index;
  }
  Buffer[*Offset] = '\0';
}

void AppendDecimal(
    CHAR8* Buffer,
    const UINTN Capacity,
    UINTN* Offset,
    UINTN Value) noexcept {
  CHAR8 Digits[24]{};
  UINTN Count = 0;
  do {
    Digits[Count] = static_cast<CHAR8>('0' + (Value % 10U));
    Value /= 10U;
    ++Count;
  } while ((Value > 0U) && (Count < sizeof(Digits)));

  while (Count > 0U) {
    --Count;
    if ((*Offset + 1U) < Capacity) {
      Buffer[*Offset] = Digits[Count];
      ++(*Offset);
      Buffer[*Offset] = '\0';
    }
  }
}

void AppendStatusHex(
    CHAR8* Buffer,
    const UINTN Capacity,
    UINTN* Offset,
    const EFI_STATUS Status) noexcept {
  constexpr CHAR8 kHexDigits[] = "0123456789ABCDEF";
  AppendText(Buffer, Capacity, Offset, "0X");
  for (UINTN Nibble = 0; Nibble < (sizeof(EFI_STATUS) * 2U); ++Nibble) {
    const UINTN Shift = ((sizeof(EFI_STATUS) * 2U) - Nibble - 1U) * 4U;
    const UINTN Digit = (Status >> Shift) & 0xFU;
    if ((*Offset + 1U) < Capacity) {
      Buffer[*Offset] = kHexDigits[Digit];
      ++(*Offset);
      Buffer[*Offset] = '\0';
    }
  }
}

void PathToAscii(
    const CHAR16* Path,
    CHAR8* Buffer,
    const UINTN Capacity) noexcept {
  if ((Path == nullptr) || (Buffer == nullptr) || (Capacity == 0U)) {
    return;
  }

  UINTN Index = 0;
  while ((Path[Index] != 0) && ((Index + 1U) < Capacity)) {
    CHAR16 Character = Path[Index];
    if ((Character >= static_cast<CHAR16>('a')) &&
        (Character <= static_cast<CHAR16>('z'))) {
      Character = static_cast<CHAR16>(
          Character - static_cast<CHAR16>('a' - 'A'));
    }
    Buffer[Index] = (Character <= 0x7FU)
                        ? static_cast<CHAR8>(Character)
                        : static_cast<CHAR8>('?');
    ++Index;
  }
  Buffer[Index] = '\0';
}

void FormatDisplayMode(
    const GopRenderer& Renderer,
    CHAR8* Buffer,
    const UINTN Capacity) noexcept {
  Buffer[0] = '\0';
  UINTN Offset = 0;
  AppendDecimal(Buffer, Capacity, &Offset, Renderer.Width());
  AppendText(Buffer, Capacity, &Offset, " X ");
  AppendDecimal(Buffer, Capacity, &Offset, Renderer.Height());
}

void FormatEntryCount(
    const UINTN Count,
    CHAR8* Buffer,
    const UINTN Capacity) noexcept {
  Buffer[0] = '\0';
  UINTN Offset = 0;
  AppendDecimal(Buffer, Capacity, &Offset, Count);
  AppendText(Buffer, Capacity, &Offset, " CONFIGURED");
}

void FormatPage(
    const UINTN First,
    const UINTN Last,
    const UINTN Total,
    CHAR8* Buffer,
    const UINTN Capacity) noexcept {
  Buffer[0] = '\0';
  UINTN Offset = 0;
  AppendText(Buffer, Capacity, &Offset, "SYSTEMS ");
  AppendDecimal(Buffer, Capacity, &Offset, First);
  AppendText(Buffer, Capacity, &Offset, "-");
  AppendDecimal(Buffer, Capacity, &Offset, Last);
  AppendText(Buffer, Capacity, &Offset, " OF ");
  AppendDecimal(Buffer, Capacity, &Offset, Total);
}

[[nodiscard]] const CHAR8* LaunchStageText(
    const EfiLaunchStage Stage) noexcept {
  switch (Stage) {
    case EfiLaunchStage::ResolveParent:
      return "PARENT DEVICE";
    case EfiLaunchStage::BuildPath:
      return "DEVICE PATH";
    case EfiLaunchStage::LoadImage:
      return "LOADIMAGE";
    case EfiLaunchStage::StartImage:
      return "STARTIMAGE";
    case EfiLaunchStage::Returned:
      return "LOADER RETURNED";
  }
  return "UNKNOWN";
}

void FormatLaunchNotice(
    const EfiLaunchResult Result,
    const CHAR8* TargetName,
    CHAR8* Buffer,
    const UINTN Capacity) noexcept {
  Buffer[0] = '\0';
  UINTN Offset = 0;
  AppendText(Buffer, Capacity, &Offset, TargetName);
  AppendText(
      Buffer,
      Capacity,
      &Offset,
      (Result.Stage == EfiLaunchStage::Returned)
          ? " LOADER RETURNED // "
          : " BOOT FAILED // ");
  AppendText(Buffer, Capacity, &Offset, LaunchStageText(Result.Stage));
  AppendText(Buffer, Capacity, &Offset, " // EFI ");
  AppendStatusHex(Buffer, Capacity, &Offset, Result.Status);
}

void FormatConfigNotice(
    const EFI_STATUS Status,
    CHAR8* Buffer,
    const UINTN Capacity) noexcept {
  Buffer[0] = '\0';
  UINTN Offset = 0;
  AppendText(Buffer, Capacity, &Offset, "CONFIG LOAD FAILED // EFI ");
  AppendStatusHex(Buffer, Capacity, &Offset, Status);
}

void DrawGrid(GopRenderer& Renderer, const UINTN Unit) noexcept {
  const UINTN Step = 4U * Unit;
  for (UINTN X = 0; X < Renderer.Width(); X += Step) {
    Renderer.FillRectangle(X, 0, 1, Renderer.Height(), theme::kGrid);
  }
  for (UINTN Y = 0; Y < Renderer.Height(); Y += Step) {
    Renderer.FillRectangle(0, Y, Renderer.Width(), 1, theme::kGrid);
  }
}

void DrawBorder(
    GopRenderer& Renderer,
    const UINTN X,
    const UINTN Y,
    const UINTN Width,
    const UINTN Height,
    const UINTN Thickness,
    const RgbColor Color) noexcept {
  Renderer.FillRectangle(X, Y, Width, Thickness, Color);
  Renderer.FillRectangle(X, Y + Height - Thickness, Width, Thickness, Color);
  Renderer.FillRectangle(X, Y, Thickness, Height, Color);
  Renderer.FillRectangle(X + Width - Thickness, Y, Thickness, Height, Color);
}

void DrawCornerTrace(
    GopRenderer& Renderer,
    const UINTN X,
    const UINTN Y,
    const UINTN Width,
    const UINTN Height,
    const UINTN Length,
    const UINTN Thickness,
    const RgbColor Color) noexcept {
  Renderer.FillRectangle(X, Y, Length, Thickness, Color);
  Renderer.FillRectangle(X, Y, Thickness, Length, Color);
  Renderer.FillRectangle(X + Width - Length, Y, Length, Thickness, Color);
  Renderer.FillRectangle(
      X + Width - Thickness, Y, Thickness, Length, Color);
  Renderer.FillRectangle(X, Y + Height - Thickness, Length, Thickness, Color);
  Renderer.FillRectangle(
      X, Y + Height - Length, Thickness, Length, Color);
  Renderer.FillRectangle(
      X + Width - Length, Y + Height - Thickness, Length, Thickness, Color);
  Renderer.FillRectangle(
      X + Width - Thickness,
      Y + Height - Length,
      Thickness,
      Length,
      Color);
}

void DrawCenteredText(
    GopRenderer& Renderer,
    const CHAR8* Text,
    const UINTN CenterX,
    const UINTN Y,
    const UINTN Scale,
    const RgbColor Color) noexcept {
  const UINTN TextWidth = Renderer.MeasureText(Text, Scale);
  const UINTN X = (CenterX >= (TextWidth / 2U))
                      ? (CenterX - (TextWidth / 2U))
                      : 0U;
  Renderer.DrawText(Text, X, Y, Scale, Color);
}

void DrawShield(
    GopRenderer& Renderer,
    const UINTN X,
    const UINTN Y,
    const UINTN Size,
    const UINTN Thickness,
    const RgbColor Color,
    const BOOLEAN WithCheck) noexcept {
  const UINTN Quarter = AtLeastOne(Size / 4U);
  const UINTN Half = Size / 2U;
  Renderer.DrawLine(X + Quarter, Y, X + Size - Quarter, Y, Thickness, Color);
  Renderer.DrawLine(
      X + Size - Quarter, Y, X + Size, Y + Quarter, Thickness, Color);
  Renderer.DrawLine(
      X + Size, Y + Quarter, X + Half, Y + Size, Thickness, Color);
  Renderer.DrawLine(
      X + Half, Y + Size, X, Y + Quarter, Thickness, Color);
  Renderer.DrawLine(X, Y + Quarter, X + Quarter, Y, Thickness, Color);
  if (WithCheck) {
    Renderer.DrawLine(
        X + Quarter,
        Y + Half,
        X + Half,
        Y + Size - Quarter,
        Thickness,
        Color);
    Renderer.DrawLine(
        X + Half,
        Y + Size - Quarter,
        X + Size - Quarter,
        Y + Quarter,
        Thickness,
        Color);
  }
}

void DrawTarget(
    GopRenderer& Renderer,
    const UINTN X,
    const UINTN Y,
    const UINTN Size,
    const UINTN Thickness,
    const RgbColor Color) noexcept {
  const UINTN Half = Size / 2U;
  const UINTN Quarter = AtLeastOne(Size / 4U);
  Renderer.DrawLine(
      X + Half, Y + Quarter, X + Size - Quarter, Y + Half, Thickness, Color);
  Renderer.DrawLine(
      X + Size - Quarter,
      Y + Half,
      X + Half,
      Y + Size - Quarter,
      Thickness,
      Color);
  Renderer.DrawLine(
      X + Half,
      Y + Size - Quarter,
      X + Quarter,
      Y + Half,
      Thickness,
      Color);
  Renderer.DrawLine(
      X + Quarter, Y + Half, X + Half, Y + Quarter, Thickness, Color);
  Renderer.DrawLine(X + Half, Y, X + Half, Y + Size, Thickness, Color);
  Renderer.DrawLine(X, Y + Half, X + Size, Y + Half, Thickness, Color);
}

void DrawCircuitRail(
    GopRenderer& Renderer,
    const UINTN X,
    const UINTN Top,
    const UINTN Height,
    const UINTN Unit,
    const BOOLEAN Mirror) noexcept {
  const UINTN Thickness = AtLeastOne(Unit / 10U);
  const UINTN BranchLength = 2U * Unit;
  const UINTN BranchX = Mirror ? (X - BranchLength) : (X + BranchLength);
  Renderer.DrawLine(X, Top, X, Top + Height, Thickness, theme::kCircuit);
  constexpr UINT8 kOffsets[] = {3U, 9U, 15U, 23U};
  for (UINTN Index = 0; Index < (sizeof(kOffsets) / sizeof(kOffsets[0]));
       ++Index) {
    const UINTN Y = Top + (static_cast<UINTN>(kOffsets[Index]) * Unit);
    Renderer.DrawLine(X, Y, BranchX, Y, Thickness, theme::kCircuit);
    Renderer.FillRectangle(
        BranchX - Thickness,
        Y - Thickness,
        3U * Thickness,
        3U * Thickness,
        (Index == 2U) ? theme::kCyan : theme::kCircuit);
  }
}

void DrawBitmapLogo(
    GopRenderer& Renderer,
    const UINTN CenterX,
    const UINTN Top,
    const UINTN Unit,
    const oslogos::MonochromeLogo& Logo,
    const RgbColor Color) noexcept {
  const UINTN Scale = AtLeastOne(Unit / 10U);
  const UINTN RenderedWidth = Logo.Width * Scale;
  const UINTN X = (CenterX >= (RenderedWidth / 2U))
                      ? (CenterX - (RenderedWidth / 2U))
                      : 0U;
  Renderer.DrawMonochromeBitmap(
      Logo.Data,
      Logo.Width,
      Logo.Height,
      Logo.BytesPerRow,
      X,
      Top,
      Scale,
      Color);
}

void DrawEntryLogo(
    GopRenderer& Renderer,
    const BootEntry& Entry,
    const UINTN CenterX,
    const UINTN Top,
    const UINTN Unit,
    const RgbColor Color) noexcept {
  if (Entry.Icon == OsIcon::Kali) {
    DrawBitmapLogo(Renderer, CenterX, Top, Unit, oslogos::kKali, Color);
    return;
  }
  if (Entry.Icon == OsIcon::BlackArch) {
    DrawBitmapLogo(Renderer, CenterX, Top, Unit, oslogos::kBlackArch, Color);
    return;
  }

  const UINTN Size = 7U * Unit;
  const UINTN X = CenterX - (Size / 2U);
  const UINTN Thickness = AtLeastOne(Unit / 8U);
  if (Entry.Icon == OsIcon::Windows) {
    const UINTN Pane = 3U * Unit;
    Renderer.FillRectangle(X, Top, Pane, Pane, Color);
    Renderer.FillRectangle(X + (4U * Unit), Top, Pane, Pane, Color);
    Renderer.FillRectangle(X, Top + (4U * Unit), Pane, Pane, Color);
    Renderer.FillRectangle(
        X + (4U * Unit), Top + (4U * Unit), Pane, Pane, Color);
    return;
  }

  if (Entry.Icon == OsIcon::Linux) {
    DrawShield(Renderer, X, Top, Size, Thickness, Color, TRUE);
    return;
  }

  DrawBorder(Renderer, X, Top, Size, Size, Thickness, Color);
  DrawCornerTrace(
      Renderer, X, Top, Size, Size, 2U * Unit, Thickness, theme::kCyanCore);
  DrawCenteredText(
      Renderer,
      "EFI",
      CenterX,
      Top + (3U * Unit),
      AtLeastOne(Unit / 5U),
      Color);
}

void DrawCard(
    GopRenderer& Renderer,
    const BootEntry& Entry,
    const UINTN X,
    const UINTN Y,
    const UINTN Width,
    const UINTN Height,
    const UINTN Unit,
    const BOOLEAN Focused) noexcept {
  const UINTN Thickness = AtLeastOne(Unit / 8U);
  const UINTN CenterX = X + (Width / 2U);
  const RgbColor Accent = Entry.Available ? theme::kCyan : theme::kRed;
  const RgbColor QuietAccent = ScaleColor(Accent, 120U);

  if (Focused && (X >= (3U * Thickness)) && (Y >= (3U * Thickness))) {
    for (UINTN Layer = 1U; Layer <= 3U; ++Layer) {
      const UINTN Offset = Layer * Thickness;
      DrawBorder(
          Renderer,
          X - Offset,
          Y - Offset,
          Width + (2U * Offset),
          Height + (2U * Offset),
          Thickness,
          ScaleColor(
              Entry.Available ? theme::kCyanGlow : theme::kRedGlow,
              static_cast<UINT8>(150U / Layer)));
    }
  }

  Renderer.FillRectangle(
      X,
      Y,
      Width,
      Height,
      Focused ? theme::kPanelFocused : theme::kPanel);
  DrawBorder(Renderer, X, Y, Width, Height, Thickness, theme::kPanelLine);
  DrawBorder(
      Renderer,
      X + Unit,
      Y + Unit,
      Width - (2U * Unit),
      Height - (2U * Unit),
      Thickness,
      theme::kPanelInset);
  DrawCornerTrace(
      Renderer,
      X,
      Y,
      Width,
      Height,
      6U * Unit,
      Thickness,
      Focused ? Accent : QuietAccent);

  if (Focused) {
    Renderer.FillRectangle(
        CenterX - Unit,
        Y,
        2U * Unit,
        2U * Thickness,
        theme::kCyanCore);
  }

  DrawEntryLogo(
      Renderer,
      Entry,
      CenterX,
      Y + (4U * Unit),
      Unit,
      Focused ? Accent : QuietAccent);
  DrawCenteredText(
      Renderer,
      Entry.Name,
      CenterX,
      Y + (15U * Unit),
      AtLeastOne(Unit / 4U),
      Focused ? theme::kPrimaryText : theme::kSecondaryText);
}

[[nodiscard]] EFI_STATUS RenderBootLaunch(
    GopRenderer& Renderer,
    const BootEntry& Entry) noexcept {
  const UINTN Unit = ResponsiveUnit(Renderer);
  const UINTN CenterX = Renderer.Width() / 2U;
  const UINTN TinyScale = AtLeastOne(Unit / 10U);
  const UINTN SmallScale = AtLeastOne(Unit / 7U);
  const UINTN TitleScale = AtLeastOne(Unit / 3U);
  const UINTN Thickness = AtLeastOne(Unit / 8U);
  const UINTN PanelWidth = 58U * Unit;
  const UINTN PanelHeight = 32U * Unit;
  const UINTN PanelX = (Renderer.Width() - PanelWidth) / 2U;
  const UINTN PanelY = 10U * Unit;
  CHAR8 LaunchTitle[80]{};
  CHAR8 LoaderPath[kBootEntryPathCapacity]{};
  UINTN Offset = 0;
  AppendText(LaunchTitle, sizeof(LaunchTitle), &Offset, "STARTING ");
  AppendText(LaunchTitle, sizeof(LaunchTitle), &Offset, Entry.Name);
  PathToAscii(Entry.LoaderPath, LoaderPath, sizeof(LoaderPath));

  Renderer.BeginFrame(theme::kBackground);
  DrawGrid(Renderer, Unit);
  DrawCircuitRail(Renderer, 5U * Unit, 8U * Unit, 38U * Unit, Unit, FALSE);
  DrawCircuitRail(
      Renderer,
      Renderer.Width() - (5U * Unit),
      8U * Unit,
      38U * Unit,
      Unit,
      TRUE);
  Renderer.FillRectangle(
      PanelX, PanelY, PanelWidth, PanelHeight, theme::kPanelFocused);
  DrawBorder(
      Renderer,
      PanelX,
      PanelY,
      PanelWidth,
      PanelHeight,
      Thickness,
      theme::kPanelLine);
  DrawCornerTrace(
      Renderer,
      PanelX,
      PanelY,
      PanelWidth,
      PanelHeight,
      8U * Unit,
      Thickness,
      theme::kCyan);
  DrawCenteredText(
      Renderer,
      kLaunchEyebrow,
      CenterX,
      PanelY + (2U * Unit),
      TinyScale,
      theme::kCyan);
  DrawEntryLogo(
      Renderer,
      Entry,
      CenterX,
      PanelY + (5U * Unit),
      Unit,
      theme::kCyan);
  DrawCenteredText(
      Renderer,
      LaunchTitle,
      CenterX,
      PanelY + (13U * Unit),
      TitleScale,
      theme::kPrimaryText);
  DrawCenteredText(
      Renderer,
      kLaunchSubtitle,
      CenterX,
      PanelY + (17U * Unit),
      SmallScale,
      theme::kCyanCore);
  DrawCenteredText(
      Renderer,
      kLaunchTelemetry,
      CenterX,
      PanelY + (21U * Unit),
      TinyScale,
      theme::kSecondaryText);
  DrawCenteredText(
      Renderer,
      LoaderPath,
      CenterX,
      PanelY + (24U * Unit),
      SmallScale,
      theme::kCyan);
  DrawCenteredText(
      Renderer,
      kLaunchSafety,
      CenterX,
      PanelY + (28U * Unit),
      TinyScale,
      theme::kSecondaryText);
  return Renderer.Present();
}

[[nodiscard]] EFI_STATUS ReadKey(
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL* Input,
    EFI_INPUT_KEY* Key) noexcept {
  if ((Input == nullptr) || (Key == nullptr) ||
      (Input->ReadKeyStroke == nullptr) || (Input->WaitForKey == nullptr) ||
      (gBS == nullptr) || (gBS->WaitForEvent == nullptr)) {
    return EFI_UNSUPPORTED;
  }

  EFI_EVENT WaitForKey = Input->WaitForKey;
  UINTN EventIndex = 0;
  const EFI_STATUS WaitStatus = gBS->WaitForEvent(1U, &WaitForKey, &EventIndex);
  if (EFI_ERROR(WaitStatus)) {
    return WaitStatus;
  }
  return Input->ReadKeyStroke(Input, Key);
}

}  // namespace

EFI_STATUS WorkspaceMenu::Run(
    GopRenderer& Renderer,
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL* Input,
    const EFI_HANDLE ImageHandle) noexcept {
  const BootConfiguration Configuration = BootDiscovery::LoadSameEsp(
      ImageHandle);
  UINTN FocusedIndex = 0U;
  for (UINTN Index = 0; Index < Configuration.Count; ++Index) {
    if (Configuration.Entries[Index].Available) {
      FocusedIndex = Index;
      break;
    }
  }

  CHAR8 ConfigNotice[112]{};
  const BOOLEAN ConfigFailed = EFI_ERROR(Configuration.Status) ? TRUE : FALSE;
  if (ConfigFailed) {
    FormatConfigNotice(
        Configuration.Status, ConfigNotice, sizeof(ConfigNotice));
  }

  EFI_STATUS Status = Render(
      Renderer,
      Configuration,
      FocusedIndex,
      ConfigFailed ? ConfigNotice : nullptr,
      ConfigFailed);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  BOOLEAN DiagnosticsVisible = FALSE;
  for (;;) {
    EFI_INPUT_KEY Key{};
    Status = ReadKey(Input, &Key);
    if (Status == EFI_NOT_READY) {
      continue;
    }
    if (EFI_ERROR(Status)) {
      return Status;
    }

    if (DiagnosticsVisible) {
      if ((Key.ScanCode == SCAN_F2) || (Key.ScanCode == SCAN_ESC) ||
          (Key.UnicodeChar == 0x1BU)) {
        DiagnosticsVisible = FALSE;
        Status = Render(
            Renderer, Configuration, FocusedIndex, nullptr, FALSE);
        if (EFI_ERROR(Status)) {
          return Status;
        }
      }
      continue;
    }

    if (Key.ScanCode == SCAN_F2) {
      DiagnosticsVisible = TRUE;
      Status = RenderDiagnostics(Renderer, Configuration, FocusedIndex);
      if (EFI_ERROR(Status)) {
        return Status;
      }
      continue;
    }

    if ((Key.ScanCode == SCAN_ESC) || (Key.UnicodeChar == 0x1BU)) {
      Renderer.BeginFrame(theme::kBlack);
      return Renderer.Present();
    }

    if (Configuration.Count == 0U) {
      continue;
    }

    BOOLEAN StateChanged = FALSE;
    if ((Key.ScanCode == SCAN_LEFT) || (Key.ScanCode == SCAN_UP)) {
      FocusedIndex = (FocusedIndex == 0U)
                         ? (Configuration.Count - 1U)
                         : (FocusedIndex - 1U);
      StateChanged = TRUE;
    } else if ((Key.ScanCode == SCAN_RIGHT) ||
               (Key.ScanCode == SCAN_DOWN) ||
               (Key.UnicodeChar == static_cast<CHAR16>('\t'))) {
      FocusedIndex = (FocusedIndex + 1U) % Configuration.Count;
      StateChanged = TRUE;
    } else if (Key.UnicodeChar == static_cast<CHAR16>('\r')) {
      const BootEntry& Entry = Configuration.Entries[FocusedIndex];
      if (!Entry.Available) {
        Status = Render(
            Renderer,
            Configuration,
            FocusedIndex,
            kSelectedOffline,
            TRUE);
        if (EFI_ERROR(Status)) {
          return Status;
        }
        continue;
      }

      Status = RenderBootLaunch(Renderer, Entry);
      if (EFI_ERROR(Status)) {
        return Status;
      }
      const EfiLaunchResult Result = EfiLoader::LaunchFromSameEsp(
          ImageHandle, Entry.LoaderPath);
      CHAR8 Notice[112]{};
      FormatLaunchNotice(Result, Entry.Name, Notice, sizeof(Notice));
      Status = Render(
          Renderer,
          Configuration,
          FocusedIndex,
          Notice,
          (Result.Stage == EfiLaunchStage::Returned) ? FALSE : TRUE);
      if (EFI_ERROR(Status)) {
        return Status;
      }
      continue;
    }

    if (StateChanged) {
      Status = Render(Renderer, Configuration, FocusedIndex, nullptr, FALSE);
      if (EFI_ERROR(Status)) {
        return Status;
      }
    }
  }
}

EFI_STATUS WorkspaceMenu::Render(
    GopRenderer& Renderer,
    const BootConfiguration& Configuration,
    const UINTN FocusedIndex,
    const CHAR8* Notice,
    const BOOLEAN NoticeIsError) noexcept {
  const UINTN Unit = ResponsiveUnit(Renderer);
  const UINTN CanvasWidth = 92U * Unit;
  const UINTN CanvasX = (Renderer.Width() - CanvasWidth) / 2U;
  const UINTN CardWidth = 40U * Unit;
  const UINTN CardHeight = 23U * Unit;
  const UINTN CardGap = 4U * Unit;
  const UINTN CardsWidth = (2U * CardWidth) + CardGap;
  const UINTN CardsX = (Renderer.Width() - CardsWidth) / 2U;
  const UINTN CardY = 14U * Unit;
  const UINTN HeaderScale = AtLeastOne(Unit / 4U);
  const UINTN SmallScale = AtLeastOne(Unit / 8U);
  const UINTN TinyScale = AtLeastOne(Unit / 10U);
  const UINTN Thickness = AtLeastOne(Unit / 8U);
  const UINTN VisibleStart = (FocusedIndex / 2U) * 2U;
  const UINTN VisibleCount =
      (Configuration.Count > VisibleStart)
          ? (((Configuration.Count - VisibleStart) > 2U) ? 2U
                                                         : (Configuration.Count - VisibleStart))
          : 0U;

  Renderer.BeginFrame(theme::kBackground);
  DrawGrid(Renderer, Unit);
  DrawCenteredText(
      Renderer, kBrand, Renderer.Width() / 2U, Unit, TinyScale, theme::kRed);
  DrawCenteredText(
      Renderer,
      kIdentity,
      Renderer.Width() / 2U,
      3U * Unit,
      HeaderScale,
      theme::kPrimaryText);
  DrawCenteredText(
      Renderer,
      kInstruction,
      Renderer.Width() / 2U,
      7U * Unit,
      SmallScale,
      theme::kPrimaryText);
  DrawCenteredText(
      Renderer,
      (Configuration.Loaded && (Configuration.Count > 0U))
          ? kTelemetryReady
          : kTelemetryEmpty,
      Renderer.Width() / 2U,
      9U * Unit,
      TinyScale,
      theme::kCyanDim);

  DrawShield(
      Renderer,
      4U * Unit,
      2U * Unit,
      3U * Unit,
      AtLeastOne(Unit / 10U),
      theme::kCyan,
      TRUE);
  Renderer.DrawText(
      kRuntimeLabel, 8U * Unit, 2U * Unit, TinyScale, theme::kSecondaryText);
  Renderer.DrawText(
      kRuntimeValue, 8U * Unit, 4U * Unit, SmallScale, theme::kCyanCore);

  const UINTN RightIconX = Renderer.Width() - (7U * Unit);
  const UINTN RightTextX = Renderer.Width() - (19U * Unit);
  DrawTarget(
      Renderer,
      RightIconX,
      2U * Unit,
      3U * Unit,
      AtLeastOne(Unit / 10U),
      theme::kCyanDim);
  Renderer.DrawText(
      kFirmwareLabel,
      RightTextX,
      2U * Unit,
      TinyScale,
      theme::kSecondaryText);
  Renderer.DrawText(
      kFirmwareValue,
      RightTextX,
      4U * Unit,
      SmallScale,
      theme::kCyanCore);

  Renderer.FillRectangle(
      CanvasX, 11U * Unit, CanvasWidth, Thickness, theme::kPanelInset);
  DrawCircuitRail(Renderer, 3U * Unit, 14U * Unit, 24U * Unit, Unit, FALSE);
  DrawCircuitRail(
      Renderer,
      Renderer.Width() - (3U * Unit),
      14U * Unit,
      24U * Unit,
      Unit,
      TRUE);

  if (VisibleCount == 1U) {
    DrawCard(
        Renderer,
        Configuration.Entries[VisibleStart],
        (Renderer.Width() - CardWidth) / 2U,
        CardY,
        CardWidth,
        CardHeight,
        Unit,
        TRUE);
  } else if (VisibleCount == 2U) {
    DrawCard(
        Renderer,
        Configuration.Entries[VisibleStart],
        CardsX,
        CardY,
        CardWidth,
        CardHeight,
        Unit,
        (FocusedIndex == VisibleStart) ? TRUE : FALSE);
    DrawCard(
        Renderer,
        Configuration.Entries[VisibleStart + 1U],
        CardsX + CardWidth + CardGap,
        CardY,
        CardWidth,
        CardHeight,
        Unit,
        (FocusedIndex == (VisibleStart + 1U)) ? TRUE : FALSE);
  }

  if (Configuration.Count > 0U) {
    CHAR8 Page[48]{};
    FormatPage(
        VisibleStart + 1U,
        VisibleStart + VisibleCount,
        Configuration.Count,
        Page,
        sizeof(Page));
    DrawCenteredText(
        Renderer,
        Page,
        Renderer.Width() / 2U,
        38U * Unit,
        TinyScale,
        theme::kCyanDim);
  }

  DrawCenteredText(
      Renderer,
      kWebsite,
      Renderer.Width() / 2U,
      40U * Unit,
      SmallScale,
      theme::kCyanCore);
  DrawCenteredText(
      Renderer,
      kWebsiteLine,
      Renderer.Width() / 2U,
      42U * Unit,
      TinyScale,
      theme::kSecondaryText);

  const CHAR8* StatusText = Notice;
  BOOLEAN SelectedAvailable = FALSE;
  if ((Configuration.Count > 0U) && (FocusedIndex < Configuration.Count)) {
    SelectedAvailable = Configuration.Entries[FocusedIndex].Available;
    if (StatusText == nullptr) {
      StatusText = SelectedAvailable ? kSelectedReady : kSelectedOffline;
    }
  } else if (StatusText == nullptr) {
    StatusText = Configuration.Loaded ? kNoSystems : kConfigOffline;
  }
  DrawCenteredText(
      Renderer,
      StatusText,
      Renderer.Width() / 2U,
      45U * Unit,
      SmallScale,
      (NoticeIsError || !SelectedAvailable) ? theme::kRed
                                             : theme::kSecondaryText);
  DrawCenteredText(
      Renderer,
      (Configuration.Count > 0U) ? kHelp : kEmptyHelp,
      Renderer.Width() / 2U,
      49U * Unit,
      SmallScale,
      theme::kSecondaryText);
  Renderer.DrawText(
      kVersion, 4U * Unit, 52U * Unit, TinyScale, theme::kCyanDim);
  const UINTN RuntimeWidth = Renderer.MeasureText(kRuntime, TinyScale);
  Renderer.DrawText(
      kRuntime,
      Renderer.Width() - (4U * Unit) - RuntimeWidth,
      52U * Unit,
      TinyScale,
      theme::kCyanDim);
  return Renderer.Present();
}

EFI_STATUS WorkspaceMenu::RenderDiagnostics(
    GopRenderer& Renderer,
    const BootConfiguration& Configuration,
    const UINTN FocusedIndex) noexcept {
  const UINTN Unit = ResponsiveUnit(Renderer);
  const UINTN TinyScale = AtLeastOne(Unit / 10U);
  const UINTN SmallScale = AtLeastOne(Unit / 8U);
  const UINTN HeaderScale = AtLeastOne(Unit / 4U);
  const UINTN Thickness = AtLeastOne(Unit / 8U);
  const UINTN PanelX = 8U * Unit;
  const UINTN PanelY = 10U * Unit;
  const UINTN PanelWidth = Renderer.Width() - (16U * Unit);
  const UINTN PanelHeight = 36U * Unit;
  const UINTN LabelX = PanelX + (5U * Unit);
  const UINTN ValueX = PanelX + (27U * Unit);
  CHAR8 DisplayMode[48]{};
  CHAR8 EntryCount[48]{};
  CHAR8 SelectedPath[kBootEntryPathCapacity]{};
  FormatDisplayMode(Renderer, DisplayMode, sizeof(DisplayMode));
  FormatEntryCount(Configuration.Count, EntryCount, sizeof(EntryCount));

  const BOOLEAN HasSelection =
      (Configuration.Count > 0U) && (FocusedIndex < Configuration.Count);
  const BootEntry* Selected =
      HasSelection ? &Configuration.Entries[FocusedIndex] : nullptr;
  if (Selected != nullptr) {
    PathToAscii(Selected->LoaderPath, SelectedPath, sizeof(SelectedPath));
  }

  Renderer.BeginFrame(theme::kBackground);
  DrawGrid(Renderer, Unit);
  DrawCenteredText(
      Renderer,
      kDiagnosticsEyebrow,
      Renderer.Width() / 2U,
      2U * Unit,
      TinyScale,
      theme::kRed);
  DrawCenteredText(
      Renderer,
      kDiagnosticsTitle,
      Renderer.Width() / 2U,
      4U * Unit,
      HeaderScale,
      theme::kPrimaryText);
  DrawCenteredText(
      Renderer,
      kDiagnosticsSubtitle,
      Renderer.Width() / 2U,
      7U * Unit,
      SmallScale,
      theme::kCyanDim);
  Renderer.FillRectangle(
      PanelX, PanelY, PanelWidth, PanelHeight, theme::kPanel);
  DrawBorder(
      Renderer,
      PanelX,
      PanelY,
      PanelWidth,
      PanelHeight,
      Thickness,
      theme::kPanelLine);
  DrawCornerTrace(
      Renderer,
      PanelX,
      PanelY,
      PanelWidth,
      PanelHeight,
      7U * Unit,
      Thickness,
      theme::kCyan);

  constexpr const CHAR8* kLabels[] = {
      "BOOT CONTROL",
      "AUTOBOOT",
      "CONFIG FILE",
      "CONFIG ENTRIES",
      "SELECTED SYSTEM",
      "SELECTED PATH",
      "LOADER STATE",
      "ESP ACCESS",
      "NVRAM WRITES",
  };
  const CHAR8* Values[] = {
      kManualPolicy,
      kDisabled,
      Configuration.Loaded ? kConfigPath : kOffline,
      EntryCount,
      (Selected != nullptr) ? Selected->Name : kNotAvailable,
      (Selected != nullptr) ? SelectedPath : kNotAvailable,
      (Selected != nullptr)
          ? (Selected->Available ? kVerified : kOffline)
          : kNotAvailable,
      kReadOnly,
      kNone,
  };

  for (UINTN Index = 0; Index < 9U; ++Index) {
    const UINTN RowY = PanelY + ((3U + (3U * Index)) * Unit);
    Renderer.DrawText(
        kLabels[Index], LabelX, RowY, SmallScale, theme::kSecondaryText);
    const BOOLEAN ErrorRow =
        ((Index == 2U) && !Configuration.Loaded) ||
        ((Index == 6U) && (Selected != nullptr) && !Selected->Available);
    Renderer.DrawText(
        Values[Index],
        ValueX,
        RowY,
        SmallScale,
        ErrorRow ? theme::kRed : theme::kCyanCore);
  }

  DrawCenteredText(
      Renderer,
      kDiagnosticsHelp,
      Renderer.Width() / 2U,
      49U * Unit,
      SmallScale,
      theme::kSecondaryText);
  Renderer.DrawText(
      kVersion, 4U * Unit, 52U * Unit, TinyScale, theme::kCyanDim);
  const UINTN RuntimeWidth = Renderer.MeasureText(kRuntime, TinyScale);
  Renderer.DrawText(
      kRuntime,
      Renderer.Width() - (4U * Unit) - RuntimeWidth,
      52U * Unit,
      TinyScale,
      theme::kCyanDim);
  return Renderer.Present();
}

}  // namespace apex32
