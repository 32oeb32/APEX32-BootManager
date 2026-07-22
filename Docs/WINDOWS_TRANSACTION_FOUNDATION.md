# Windows transaction foundation

PR #7 defines the rollback contract that the native Windows firmware backend
must satisfy before `Install and Make Default` or `Restore` can be enabled.
The transaction engine is shared by the Windows GUI build and a disposable
Windows-runner lifecycle test, but the public application still reports
`INSTALL|0` and `RESTORE|0`.

## Safety boundary

The qualified test backend accepts two explicit paths:

- a temporary directory that models the EFI System Partition; and
- a JSON file that models firmware entries and their order.

It never invokes BCDEdit, never calls a firmware-variable API, never mounts the
host ESP, and is not installed by the NSIS package. CI fails if the test binary
appears in the package staging directory. The graphical application continues
to use UAC only for its existing read-only `mountvol /S` discovery operation.

This is a transaction qualification milestone, not authorization to install
APEX32 on Windows hardware.

## Persistent state

The first successful install creates schema 1 recovery metadata beneath the
declared mock ESP:

```text
EFI/APEX32/Apex32BootManager.efi
EFI/APEX32/apex32.cfg
EFI/APEX32/recovery-state.json
EFI/APEX32/recovery/firmware.before-community
EFI/APEX32/recovery/config.before-community
```

The state records whether firmware and configuration files existed before
APEX32, their SHA-256 hashes, and the complete pre-install firmware-store
snapshot. Backups are immutable across reinstall. If an original file did not
exist, restore removes the APEX32-created file instead of inventing a backup.

Every target path is constructed from fixed relative names and checked to be
inside the declared ESP root. Firmware input must begin with the PE/COFF `MZ`
signature. Transaction inputs are bounded to 64 MiB and configuration data to
1 MiB. Files are staged with `QSaveFile`, committed, hashed, and verified
before the firmware-store promotion is accepted.

## Rollback contract

Before each operation, the engine captures the current known files and the
firmware-store state. Any error restores both domains. Tests inject failures:

- after file commit;
- while promoting the APEX32 entry;
- after firmware promotion;
- while restoring the original firmware state; and
- after restore has begun.

The lifecycle test also covers initial install, duplicate-free reinstall,
immutable backup, clean restore when APEX32 replaced existing files, clean
restore when no files existed, invalid-firmware rejection, and a sentinel
outside the mock ESP.

## Run the lifecycle test

On a Windows contributor machine with Qt 6 and MSVC 2022, configure and build
the normal Windows project, then run:

```powershell
ctest --test-dir Installer/Windows/build -C Release --output-on-failure
```

The expected line is:

```text
PASS: Windows transaction install, reinstall, rollback, restore, and temporary-ESP confinement
```

GitHub Actions runs the same test on a disposable Windows 2022 runner before
building the NSIS artifact.

## Remaining Windows gates

The following work is intentionally not claimed by PR #7:

- package the verified APEX32 EFI binary into the Windows artifact;
- implement a narrow native firmware-entry backend;
- run the same failure matrix against a disposable UEFI Windows VM;
- add power-loss recovery at every durable boundary;
- qualify install, reboot, reinstall, restore, and uninstall on real hardware;
- sign the EFI binary and Windows installer; and
- change the release capability to `INSTALL|1` and `RESTORE|1` only after all
  preceding gates pass.

Microsoft documents `mountvol /S` as the operation that mounts the EFI System
Partition. It also warns that changing boot configuration can make a system
unbootable. The project therefore keeps mounting, file transactions, firmware
entry management, and release enablement as separate reviewable boundaries.

Authoritative references:

- [Mountvol](https://learn.microsoft.com/windows-server/administration/windows-commands/mountvol)
- [BCDEdit](https://learn.microsoft.com/windows-server/administration/windows-commands/bcdedit)
- [Adding boot entries](https://learn.microsoft.com/windows-hardware/drivers/devtest/adding-boot-entries)
