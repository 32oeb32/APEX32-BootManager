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

## Graphical installer

The Linux GUI is the user-facing configuration plane. It scans mounted EFI
System Partitions for `.efi` applications, recognizes common vendor paths,
allows entries to be selected, generates schema 1, and requests installation
through a graphical authorization prompt.

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
