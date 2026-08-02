# One-step installation experience

APEX32 has one product identity and one firmware codebase. Platform packaging
differs only because Linux uses PolicyKit and Debian packages while Windows
uses UAC and a setup executable.

## Linux

Full instructions: [Install APEX32 on Kali Linux](INSTALL_KALI.md).

The technical-user path is one shell command:

```bash
git clone https://github.com/32oeb32/APEX32-BootManager.git && cd APEX32-BootManager && ./install.sh
```

`install.sh` performs the remaining bootstrap automatically:

1. verifies Linux, x86_64, UEFI mode, and a supported Debian-family system;
2. downloads `apex32-boot-manager_amd64.deb` and its checksum from the latest
   GitHub release;
3. refuses installation if verification fails;
4. asks for administrator approval through the graphical PolicyKit agent;
5. installs the package without terminal password authentication; and
6. opens **APEX32 Community Installer**.

The user then reviews the detected systems and explicitly selects **Install
APEX32 and Make Default**. Boot configuration is never changed merely by
cloning the repository or installing the desktop package.

The release must contain both stable asset names:

- `apex32-boot-manager_amd64.deb`
- `apex32-boot-manager_amd64.deb.sha256`

The package workflow creates and tests those exact files. Until a release is
published with them, `install.sh` fails closed instead of building an untested
firmware image on the user's computer.

## Windows

Full instructions and the current qualification warning:
[Install APEX32 on Windows](INSTALL_WINDOWS.md).

The Windows user downloads and double-clicks one file:

```text
APEX32-Community-Setup.exe
```

The CI-built package installs and launches the complete graphical installer.
**Scan Systems** requests standard UAC approval, temporarily mounts the EFI
System Partition, discovers EFI loaders, unmounts it, and shows the results.
No PowerShell, Command Prompt, drive-letter selection, or manual boot command
is exposed to the user.

The hardware-enabled package contains the verified EFI payload and enables
**Install APEX32 and Make Default** plus **Restore Previous Boot State**. Its
transaction engine preserves immutable backups and the exact original
`BootOrder`. Windows-native variable logic is tested with fake variables, and
OVMF independently exercises create, promote, restore, and remove against
private firmware. Ordinary source builds remain `INSTALL|0`/`RESTORE|0`.

The unsigned Community beta refuses installation while Secure Boot is
enabled. Linux physical qualification passed on the recorded HP/Kali machine.
The Windows UEFI VM, physical Windows matrix, and signed build remain release
gates.

## Shared graphical flow

Both packaged applications converge on the same visible workflow:

1. Open APEX32.
2. Select **Scan Systems**.
3. Review detected operating systems.
4. Select **Install APEX32 and Make Default**.
5. Receive verified success or automatic rollback.
6. Use **Recovery** to restore the previous boot manager.

Linux and Windows now implement transactional install, make-default, and
graphical recovery behind platform-native authorization. Final publication
still requires the live-hardware matrix, signing, and release qualification.
