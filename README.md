# APEX32 Boot Manager Community Edition

APEX32 is a native x86_64 UEFI boot manager written in freestanding C++20
with EDK II and the Graphics Output Protocol. It is an independent boot
manager, not a GRUB or rEFInd theme.

The Community Edition keeps the approved APEX32 Secure intro and
cyber-interface. An installed system renders only the operating-system cards
selected in the versioned installer configuration. Bounded read-only
`Boot####` discovery is retained as a recovery fallback when no usable
configuration exists.
Cards intentionally show only the OS mark and OS name. Permanent project
branding and recovery links are displayed beneath the cards:

**[apex32-secure.com](https://apex32-secure.com)**

This repository is a separate public product line. Do not overlay it onto a
personal APEX32 installation or development tree.

![APEX32 Community secure gateway](Docs/Images/APEX32-Community-v0.10.0-alpha1-preview.png)

## Status

`0.10.0-alpha1` is the published source milestone. The merged
`0.11.0-beta1` foundation and current one-step installer candidate add:

- the approved pre-graphics APEX32 intro and reusable resolution-independent
  GOP renderer;
- a bounded configuration parser supporting up to 32 UEFI loaders;
- authoritative same-ESP configured handoff for installer-selected systems,
  with bounded read-only `BootOrder`/`Boot####` discovery only when no usable
  configuration exists;
- paged, manual-only OS selection with no countdown or autoboot;
- bounded animated focus cross-fades plus Arrow, Tab, Home, End, and page
  navigation;
- a pluggable identity registry for Kali, BlackArch, Windows, Ubuntu, Fedora,
  Arch, Debian, Mint, openSUSE, Pop!_OS, OpenCore, recovery, USB, network,
  generic Linux, and unknown EFI loaders;
- same-ESP loader verification before a card can boot;
- F2 read-only diagnostics;
- a Linux Qt 6 installer prototype with **Systems → Scan Now**;
- graphical authorization for read-only discovery on root-only ESP mounts;
- a fail-closed, scan-only default build whose GUI and helper both omit the
  unfinished hardware-install path;
- isolated Linux and Windows mock-ESP transaction tests covering verified staging,
  duplicate-free reinstall, immutable backup, automatic rollback, persistent
  recovery state, and full restore/uninstall semantics;
- an OVMF visual gate that boots the real EFI application from a temporary
  virtual ESP, then seeds a private APEX32 `Boot####` entry, promotes it to
  first in `BootOrder`, reboots, and verifies the gateway framebuffer;
- a transactional polkit-authorized install and restore helper;
- a Debian package builder that embeds the verified EFI application and is
  the only build mode that reports `INSTALL|1` and `RESTORE|1`;
- an extracted-package CI gate covering the GUI, helper, firmware, desktop
  launcher, icon, PolicyKit policy, dependencies, and capability mode;
- a disposable-runner lifecycle gate that performs a real package install,
  reinstall, and purge while refusing access to any ESP or UEFI variables;
- real OVMF gates for removable fallback, NVRAM-first boot, Linux and Windows
  handoffs, GRUB, and shim; and
- host tests for firmware UI, configuration, loader handoff, firmware-entry
  management, and recovery fallback behavior.

This is a beta candidate, not yet a universal production installer. The first
physical HP/Kali qualification proved package safety plus graphical install and
restore, but exposed a native-device-path boot regression. The candidate was
restored rather than released. The corrected build must pass the same physical
install, reboot, every-selected-OS handoff, and restore sequence before release.
Raw multi-ESP discovery, signed artifacts, additional distro packages, and
broader hardware qualification remain release gates.
The Windows package now contains the reusable transaction engine, a native
`Boot####`/`BootOrder` backend, and the same verified EFI payload as Linux.
The Windows and OVMF workflows cover isolated file and firmware-variable
lifecycle tests without touching runner firmware. See
[boot-card interactions](Docs/BOOT_CARD_INTERACTIONS.md) and the
[Windows graphical installer](Docs/WINDOWS_PRODUCTION_INSTALLER.md) for the
precise capability and qualification boundary.
The first physical-machine results and the mandatory corrective retest are
recorded in [hardware qualification](Docs/HARDWARE_QUALIFICATION.md).

Installer contributors can run a root-refusing mock-ESP test and a visibly
disabled safe GUI demo without touching their boot configuration. See
[Installer testing](Docs/INSTALLER_TESTING.md). These contributor commands are
temporary; packaged beta users will launch the installer from their desktop.

The installer guide includes a complete fresh-clone workflow, dependency
setup, expected test output, automatic Hyprland graphical authorization, a safe GUI
demo, and an authorized read-only scan of the real ESP. It deliberately stops
before installation on hardware. The source build reports `INSTALL|0`, keeps
the Install control disabled, and rejects direct helper install requests.

The tested Debian package candidate enables the final controls. It is not a
public release until live-hardware restore, multi-ESP selection, Secure Boot,
and release-signing gates pass. End users will download the release package,
open it graphically, launch APEX32, select **Scan Now**, select their systems,
choose **Install APEX32 and Make Default**, and finish. They will never copy EFI
files or edit NVRAM by hand. See
[Linux package and zero-terminal installation](Docs/LINUX_PACKAGE.md).

The Debian-family release uses portable Qt dependency alternatives and CI
installs the same package on both Ubuntu and Kali rolling before publication.
It recommends a graphical PolicyKit agent and starts the known Kali Hyprland
user service automatically before authorization. Normal users do not run a
`systemctl` command.

## One-step user installation

Linux technical users run one command after choosing a trusted release:

```bash
git clone https://github.com/32oeb32/APEX32-BootManager.git && cd APEX32-BootManager && ./install.sh
```

The launcher downloads and verifies the tested package, requests graphical
PolicyKit authorization, installs it, and opens the GUI. It never builds EDK II
or asks the user to copy EFI files or edit firmware variables.

Windows users download and double-click one
`APEX32-Community-Setup.exe`. The CI-built package embeds the verified
firmware and enables graphical scan, install, make-default, and restore. A
source-only contributor build remains scan-only unless it explicitly receives
that verified firmware. See
[one-step installation](Docs/ONE_STEP_INSTALL.md).

The Windows package uses one normal UAC consent dialog and never asks users to
clone the repository or type a command. The unsigned Community beta refuses
installation while Secure Boot is enabled; signing and live-hardware
qualification remain release gates. See the
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
./Tools/test-qemu-ovmf-native-discovery.sh
./Tools/test-qemu-ovmf-config-precedence.sh
./Tools/test-qemu-ovmf-handoff.sh
./Tools/test-qemu-ovmf-linux-loaders.sh
```

See [QEMU/OVMF firmware testing](Docs/QEMU_OVMF_TESTING.md).
Renderer internals, supported GOP behavior, scaling, and clipping guarantees
are documented in [Graphics foundation](Docs/GRAPHICS_FOUNDATION.md).
Native load-option parsing, ordering, bounds, and handoff are documented in
[Native UEFI boot discovery](Docs/NATIVE_BOOT_DISCOVERY.md).

## Build the Debian beta package

After building the verified EFI application, contributors can produce and
inspect the same hardware-enabled package used by CI:

```bash
./Tools/build-linux-package.sh

PACKAGE="$(find Installer/Linux/package-build/packages -name '*.deb' -print -quit)"
./Tools/test-linux-package.sh \
  "$PACKAGE" \
  Build/DEBUG_GCC/X64/Apex32BootManager.efi
```

These are contributor commands. Release users install the `.deb` from their
desktop without opening a terminal.

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
