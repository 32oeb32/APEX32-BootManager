# APEX32 Boot Manager Community Edition

APEX32 is a native x86_64 UEFI boot manager written in freestanding C++20
with EDK II and the Graphics Output Protocol. It is an independent boot
manager, not a GRUB or rEFInd theme.

The Community Edition keeps the APEX32 Secure intro and cyber-interface while
discovering its operating-system cards from a versioned configuration file.
Cards intentionally show only the OS mark and OS name. Permanent project
branding and recovery links are displayed beneath the cards:

**[apex32-secure.com](https://apex32-secure.com)**

This repository is a separate public product line. Do not overlay it onto a
personal APEX32 installation or development tree.

![APEX32 Community secure gateway](Docs/Images/APEX32-Community-v0.10.0-alpha1-preview.png)

## Status

`0.10.0-alpha1` is the first public-source architecture milestone. It includes:

- the hardware-tested APEX32 intro and resolution-independent GOP renderer;
- a generic configuration parser supporting up to eight UEFI loaders;
- paged, manual-only OS selection with no countdown or autoboot;
- Kali, BlackArch, Windows, generic Linux, and generic EFI marks;
- same-ESP loader verification before a card can boot;
- F2 read-only diagnostics;
- a Linux Qt 6 installer prototype with **Systems → Scan Now**;
- graphical authorization for read-only discovery on root-only ESP mounts;
- a fail-closed, scan-only default build whose GUI and helper both omit the
  unfinished hardware-install path;
- an isolated mock-ESP transaction test covering verified staging,
  duplicate-free reinstall, immutable backup, automatic rollback, persistent
  recovery state, and full restore/uninstall semantics;
- an OVMF visual gate that boots the real EFI application from a temporary
  virtual ESP, then seeds a private APEX32 `Boot####` entry, promotes it to
  first in `BootOrder`, reboots, and verifies the gateway framebuffer;
- a polkit-authorized helper design reserved for a future packaged installer
  that will install APEX32 without terminal commands; and
- host tests for firmware UI, configuration, loader handoff, firmware-entry
  management, and recovery fallback behavior.

This is an alpha source milestone, not yet a universal production installer.
The current firmware executes loaders located on its own EFI System Partition.
Multi-ESP device resolution, signed release artifacts, distro packages, and
the completed recovery GUI remain release gates.

Installer contributors can run a root-refusing mock-ESP test and a visibly
disabled safe GUI demo without touching their boot configuration. See
[Installer testing](Docs/INSTALLER_TESTING.md). These contributor commands are
temporary; packaged beta users will launch the installer from their desktop.

The installer guide includes a complete fresh-clone workflow, dependency
setup, expected test output, Hyprland graphical authorization setup, a safe GUI
demo, and an authorized read-only scan of the real ESP. It deliberately stops
before installation on hardware. The source build reports `INSTALL|0`, keeps
the Install control disabled, and rejects direct helper install requests.

The intended public experience remains: clone or install a package, open the
graphical installer, select **Scan Now**, select **Make Default**, and finish.
Those final controls stay disabled until the remaining hardware and package
gates pass; no public user should be asked to copy EFI files or edit NVRAM by
hand.

There is no Windows installer in this alpha. The planned Windows release is a
signed graphical package using the normal UAC consent dialog; Windows users
will not be asked to clone the repository or use a terminal. See
[GUI installer release plan](Docs/GUI_INSTALLER_PLAN.md).

## Build the firmware

The repository directory must be named `APEX32-BootManager`. The pinned EDK II
revision is listed in `Tools/edk2-version`.

```bash
./Tools/test-host.sh

EDK2_DIR=/path/to/edk2 \
  ./Tools/build-edk2.sh
```

The EFI artifact is written to:

```text
Build/DEBUG_GCC/X64/Apex32BootManager.efi
```

The real firmware can then be booted safely in a disposable QEMU/OVMF machine:

```bash
./Tools/test-qemu-ovmf.sh
./Tools/test-qemu-ovmf-bootorder.sh
./Tools/test-qemu-ovmf-handoff.sh
```

See [QEMU/OVMF firmware testing](Docs/QEMU_OVMF_TESTING.md).

## Configuration

The graphical installer generates:

```text
\EFI\APEX32\apex32.cfg
```

See [Docs/CONFIG_FORMAT.md](Docs/CONFIG_FORMAT.md) for the versioned format.
End users are not expected to edit it.

## Repository layout

- `Firmware entry`: `BootManager/`
- `Renderer and animation`: `Renderer/`, `Animation/`, `Fonts/`, `Themes/`
- `Dynamic boot configuration`: `Config/`
- `Loader discovery and handoff`: `Boot/`
- `Name-only OS interface`: `Menu/`
- `Compiled firmware-safe marks`: `Assets/`
- `Graphical Linux installer prototype`: `Installer/Linux/`
- `Host tests and stubs`: `Tests/`
- `Architecture and security documentation`: `Docs/`

## License and safety

Copyright (C) 2026 Oussama / APEX32 Secure contributors.

APEX32 is free software licensed under the
[GNU General Public License v3.0](LICENSE). It is provided **without warranty**
as described by GPL-3.0 sections 15 and 16. Firmware and boot-order changes
carry inherent risk; preserve recovery media and verified backups.

Kali, BlackArch, Windows, Linux, UEFI, and other names or marks remain the
property of their respective owners. Their appearance identifies compatible
boot targets and does not imply endorsement.
