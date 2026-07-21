# Community architecture

APEX32 separates the trusted pre-OS runtime from installation and discovery.

## Firmware

The UEFI application is freestanding C++20 built with EDK II. It owns no
network stack and performs no NVRAM writes. Its responsibilities are limited to:

1. initialize GOP and play the APEX32 Secure intro;
2. read and validate the bounded `apex32.cfg` file;
3. verify configured loader files on the current ESP;
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

This means cards are not hardcoded, but the firmware does not yet enumerate
arbitrary `Boot####` variables or other ESPs by itself. The scanner discovers
loaders and writes up to 32 validated schema 1 records; firmware renders every
accepted record and keeps unknown loaders bootable with the generic identity.

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
