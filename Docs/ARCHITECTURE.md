# Community architecture

APEX32 separates the trusted pre-OS runtime from installation and discovery.

## Firmware

The UEFI application is freestanding C++20 built with EDK II. It owns no
network stack and performs no NVRAM writes. Its responsibilities are limited to:

1. initialize GOP and play the APEX32 Secure intro;
2. read and validate bounded active `Boot####` load options in `BootOrder`;
3. merge and validate the bounded `apex32.cfg` fallback;
4. render name-only OS cards and read explicit keyboard input; and
5. hand the selected loader to UEFI `LoadImage` and `StartImage`.

There is no timer path, countdown, or automatic selection.

### Graphics pipeline

`GopRenderer` owns one canonical GOP BLT back buffer and all clipped drawing
primitives. `LogicalCanvas` maps a 1920×1080 scene into a centered,
aspect-preserving physical viewport. `Assets/OsIdentity` owns the extensible OS
token/accent/mark registry, while `Menu/CardLayout` creates bounded dynamic
one-to-four-card pages. Renderer code contains no menu policy or loader logic.
See [Graphics foundation](GRAPHICS_FOUNDATION.md).

## Graphical installer

The Linux GUI is the user-facing configuration plane. It scans mounted EFI
System Partitions for `.efi` applications, recognizes common vendor paths,
allows entries to be selected, generates schema 1, and requests installation
through a graphical authorization prompt.

Cards are not hardcoded. Firmware performs read-only native `BootOrder` and
`Boot####` discovery, retains complete validated device paths for cross-ESP
handoff, and merges up to 32 scanner records as a fallback. Unknown active
load options receive the generic identity and remain selectable. See
[Native UEFI boot discovery](NATIVE_BOOT_DISCOVERY.md).

The privileged helper has a narrow interface. It validates all paths, creates
an immutable first backup, atomically copies the firmware/configuration, creates
or reuses the APEX32 NVRAM entry, and places that entry first in `BootOrder`.
It does not accept shell fragments or execute a shell.

## Trust boundary

- Firmware configuration is data, never executable source.
- Loader paths are bounded and must be absolute EFI paths.
- The firmware is read-only after installation.
- Mutating operations remain in an OS-side, polkit-authorized helper.
- Recovery and uninstall must be available before a stable public release.
