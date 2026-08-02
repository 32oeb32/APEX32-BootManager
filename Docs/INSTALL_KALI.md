# Install APEX32 on Kali Linux

This is the end-user guide for the APEX32 Community beta on Kali Linux. It
uses the tested package and graphical installer; users do not build EDK II or
manually edit the EFI System Partition.

## Requirements

- x86_64 computer running Kali Linux in UEFI mode;
- Secure Boot disabled for the unsigned Community beta;
- an internet connection for package download and dependencies;
- at least one working Kali EFI loader;
- a graphical desktop session; and
- recovery media or a known working firmware boot entry.

The corrected candidate has been physically qualified on Kali Rolling with
Hyprland, Kali, and BlackArch on the same ESP. Debian and Ubuntu package
installation are covered by CI, but wider physical hardware coverage remains
ongoing.

## Option A — one verified command

After `v0.11.0-beta1` is published, open a terminal as the normal desktop user
and run:

```bash
git clone https://github.com/32oeb32/APEX32-BootManager.git && cd APEX32-BootManager && ./install.sh
```

Do **not** add `sudo`. The launcher:

1. confirms Linux, x86_64, UEFI, and Debian-family compatibility;
2. downloads `apex32-boot-manager_amd64.deb` from the latest GitHub release;
3. downloads and verifies its SHA-256 file;
4. requests administrator approval through the desktop's graphical PolicyKit
   dialog;
5. installs the package with APT; and
6. opens **APEX32 Community Installer**.

The repository clone and package installation do not change EFI files or the
boot order. Hardware changes begin only after the explicit install action in
the GUI.

## Option B — download the package

1. Open the official [GitHub Releases](https://github.com/32oeb32/APEX32-BootManager/releases).
2. Open the latest qualified beta release.
3. Download both:
   - `apex32-boot-manager_amd64.deb`
   - `apex32-boot-manager_amd64.deb.sha256`
4. Double-click the `.deb` and install it with the desktop software manager.
5. Open **APEX32 Community Installer** from the application launcher.

Do not install packages copied from workflow artifacts, pull requests, chat
attachments, or unofficial mirrors on a physical machine.

## Scan and install

1. Open the **Systems** tab.
2. Select **Scan Now**.
3. Approve the normal graphical administrator-password dialog.
4. Confirm the displayed EFI System Partition.
5. Review every detected loader.
6. Check only the operating systems that should appear in APEX32.
7. Open **Install**.
8. Keep **Make APEX32 Secure Gateway the default boot manager** selected.
9. Select **Install APEX32 and Make Default**.
10. Approve the graphical authorization dialog.
11. Restart only after the GUI reports that APEX32 is installed and first in
    the UEFI boot order.

The installer preserves the previous APEX32 firmware when present, stores an
immutable recovery state, writes the selected loader configuration, creates or
reuses one APEX32 firmware entry, promotes it, and verifies every committed
change. Any failed verification invokes rollback.

## First reboot checklist

The first APEX32 boot should show:

- the approved APEX32 Secure intro;
- only the systems selected in the installer;
- no unexpected GRUB, Windows, disk, or maintenance cards; and
- manual keyboard selection with no countdown or automatic boot.

Test every selected operating-system card before treating the installation as
qualified. Keep the firmware boot menu available until all cards work.

## Graphical recovery

To return to the previous boot manager:

1. Boot Kali directly or through APEX32.
2. Open **APEX32 Community Installer**.
3. Open **Recovery**.
4. Select **Restore Previous Boot Manager**.
5. Confirm the graphical authorization request.
6. Wait for the verified success message.
7. Restart and confirm the original operating system boots normally.

Recovery restores the saved firmware file and exact prior `BootOrder`, removes
installer-generated configuration and recovery state, and removes the APEX32
entry only when the transaction originally created it.

Always perform graphical Recovery before uninstalling the desktop package.
Package removal intentionally does not rewrite firmware automatically.

## Update or reinstall

Install a newer qualified `.deb` normally, launch APEX32, scan again, and use
the graphical install action. The transaction reuses its owned entry without
creating duplicates while preserving the original recovery baseline.

## Common problems

### Secure Boot is enabled

The unsigned Community beta refuses installation. Disable Secure Boot in UEFI
settings or wait for a signed release.

### No graphical authentication agent

The package recommends compatible PolicyKit agents. On Kali/Hyprland, APEX32
starts the installed `hyprpolkitagent.service` in the current user session
automatically. Users must not manually start a service or use terminal
authentication. If no graphical agent can be started, APEX32 stops safely.

### An operating system is missing

APEX32 currently installs and chainloads loaders from the selected ESP. A
loader on another disk or ESP may not appear. Do not manually copy it; restore
the previous boot manager and report the partition layout.

### A card fails to boot

Use Esc or the firmware boot menu to start a known-good OS entry. Do not repeat
installation. Open APEX32 Recovery, restore the previous state, and include the
loader path and machine model in the issue report.

## Report a problem

Open a [GitHub issue](https://github.com/32oeb32/APEX32-BootManager/issues)
with:

- computer model;
- Kali version and desktop environment;
- Secure Boot state;
- installer version;
- selected operating systems;
- exact GUI error; and
- a photograph or screenshot without passwords, recovery keys, serial
  numbers, or personal paths.

For architecture and safety details, see the
[Installer security model](INSTALLER_SECURITY.md) and
[Hardware qualification](HARDWARE_QUALIFICATION.md).
