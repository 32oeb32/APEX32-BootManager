# Linux package and zero-terminal installation

The APEX32 Linux beta is delivered as an `amd64` Debian package for Debian,
Ubuntu, Kali, and compatible desktop systems. Cloning and compiling are
contributor workflows; release users do not need a terminal.

## Release-user experience

1. Download `apex32-boot-manager_0.11.0~beta1_amd64.deb` from the official
   GitHub release linked by [apex32-secure.com](https://apex32-secure.com).
2. Double-click the downloaded package and choose **Install** in the desktop
   software application. The package includes AppStream metadata, a desktop
   launcher, and a scalable APEX32 icon.
3. Launch **APEX32 Community Installer** from the application menu.
4. Open **Systems** and select **Scan Now**. The desktop displays its normal
   graphical administrator-password dialog if the ESP is protected.
5. Review the detected OS names and choose the systems to display.
6. Open **Install** and select **Install APEX32 and Make Default**.
7. Accept the graphical authorization dialog. APEX32 verifies its files,
   preserves the previous state, registers one firmware entry, promotes it to
   first in `BootOrder`, verifies the result, and reports success or rollback.
8. Restart when ready.

No terminal, manual ESP selection, file copying, `efibootmgr`, GRUB editing, or
rEFInd is part of the release-user workflow.

## Recovery and removal

Open **APEX32 Community Installer → Recovery** and choose **Restore Previous
Boot Manager** before removing the desktop package. Restore verifies the saved
state, returns the original files and boot order, removes the APEX32 entry when
the installer created it, and deletes its completed recovery transaction.

Package removal deliberately does not rewrite firmware automatically. Boot
configuration must never be changed by an unattended package-manager hook.

## Desktop authorization

GNOME, KDE Plasma, Cinnamon, and similar desktops normally start a graphical
PolicyKit agent automatically. Minimal compositors must also run a compatible
agent. APEX32 disables PolicyKit's terminal authentication agent and fails
closed with a clear GUI error when no graphical agent is available.

## Contributor package gate

`Tools/build-linux-package.sh` refuses root, validates the real x86_64 EFI
artifact, creates a clean Release build, embeds the firmware, enables the
production install/restore capability, and generates one `.deb` through CPack.

`Tools/test-linux-package.sh` also refuses root. It extracts the package into a
temporary directory and verifies:

- the GUI, helper, desktop launcher, icon, PolicyKit policy, firmware, GPL, and
  disclaimer are present;
- the desktop entry and Software Center/AppStream metadata validate cleanly;
- the installed helper path matches the PolicyKit policy;
- the firmware matches the verified EDK II output byte-for-byte;
- only the package build reports `INSTALL|1` and `RESTORE|1`;
- terminal authentication remains disabled; and
- runtime metadata requires `efibootmgr`, `pkexec`, and `util-linux` in
  addition to automatically detected shared-library dependencies.

The GitHub package workflow then installs the package on a disposable runner,
checks root ownership and capability mode, reinstalls it, purges it, and
confirms every packaged path was removed. The lifecycle script refuses to run
outside GitHub Actions, on a mounted `/boot/efi`, or where an APEX32 ESP path
already exists. It rejects packages containing Debian maintainer scripts and,
when the hosted runner exposes EFI variables, snapshots every variable before
and after the lifecycle so any mutation fails the gate.

The candidate remains unreleased until the hardware, multi-ESP, Secure Boot,
recovery-media, and release-signing gates in
[the GUI installer plan](GUI_INSTALLER_PLAN.md) are complete.
