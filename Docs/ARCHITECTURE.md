# Community architecture

APEX32 separates the trusted pre-OS runtime from installation and discovery.

## Firmware

The UEFI application is freestanding C++20 built with EDK II. It owns no
network stack and performs no NVRAM writes. Its responsibilities are limited to:

1. initialize GOP and play the APEX32 Secure intro;
2. load the bounded installer `apex32.cfg` allow-list when it is present;
3. otherwise read bounded active `Boot####` options as a recovery fallback;
4. render name-only OS cards and read explicit keyboard input; and
5. hand the selected loader to UEFI `LoadImage` and `StartImage`.

There is no countdown, timer-driven selection, or automatic boot. Finite
focus-transition frames are presented only after explicit navigation input.

### Graphics pipeline

`GopRenderer` owns one canonical GOP BLT back buffer and all clipped drawing
primitives. `LogicalCanvas` maps a 1920×1080 scene into a centered,
aspect-preserving physical viewport. `Assets/OsIdentity` owns the extensible OS
token/accent/mark registry, while `Menu/CardLayout` creates bounded dynamic
one-to-four-card pages. `Menu/CardNavigation` owns bounded index movement, and
`Menu/CardAnimation` owns bounded integer easing for explicit focus changes.
Renderer code contains no menu policy or loader logic.
See [Graphics foundation](GRAPHICS_FOUNDATION.md) and
[Boot-card interactions](BOOT_CARD_INTERACTIONS.md).

## Graphical installer

The Linux and Windows GUIs are the user-facing configuration planes. They scan
the EFI System Partition for `.efi` applications, recognize common vendor
paths, allow entries to be selected, generate schema 1, and request platform
graphical authorization before any privileged operation.

Cards are not hardcoded. The installer writes up to 32 selected scanner
records as an authoritative same-ESP allow-list. If that configuration is
absent or empty, firmware performs bounded read-only native `BootOrder` and
`Boot####` recovery discovery. Unknown active load options receive the generic
identity and remain selectable in that fallback mode. See
[Native UEFI boot discovery](NATIVE_BOOT_DISCOVERY.md).

The privileged helper has a narrow interface. It validates all paths, creates
an immutable first backup, atomically copies the firmware/configuration, creates
or reuses the APEX32 NVRAM entry, and places that entry first in `BootOrder`.
It does not accept shell fragments or execute a shell.

On Windows, `WindowsFirmwareStore` implements the same transaction contract
through the native firmware-environment APIs. It captures exact variable bytes
and attributes, creates or reuses one APEX32 load option, verifies promotion,
and supports exact graphical restore. The package is hardware-capable only
when CMake embeds the SHA-256 of a verified EFI payload; ordinary source builds
remain scan-only. See
[Windows production installer](WINDOWS_PRODUCTION_INSTALLER.md).

## Trust boundary

- Firmware configuration is data, never executable source.
- Loader paths are bounded and must be absolute EFI paths.
- The firmware is read-only after installation.
- Mutating operations remain in a graphically authorized OS-side component:
  the narrow polkit helper on Linux or the UAC-elevated native backend on
  Windows.
- Recovery and uninstall must be available before a stable public release.
