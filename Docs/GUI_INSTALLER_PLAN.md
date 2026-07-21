# Zero-terminal GUI installer release plan

The APEX32 end-user installer must behave like a normal desktop application.
Cloning, compiling, and terminal commands are contributor workflows only.

## Current public alpha

- Linux Qt GUI for safe loader discovery and preview.
- Read-only ESP scan through a narrow PolicyKit helper when required.
- Graphical authorization only; `pkexec` terminal fallback is disabled.
- Hardware installation compiled out by default in both GUI and helper.
- No Windows installer is published yet.

## Linux beta experience

1. The user downloads a signed distribution package or AppImage bundle.
2. The desktop launches **APEX32 Community Installer** from an icon.
3. **Scan Now** shows detected operating systems and recovery loaders.
4. The user reviews the proposed configuration.
5. A single desktop PolicyKit dialog authorizes installation.
6. The GUI verifies the firmware, creates a recovery backup, installs APEX32,
   registers it, verifies the resulting boot state, and shows a success or
   rollback result without opening a terminal.

## Windows beta experience

1. The user downloads a signed MSI or installer EXE from the GitHub release.
2. Windows displays the standard publisher and UAC consent dialogs.
3. The GUI discovers the EFI System Partition using Windows APIs, without
   asking the user for a drive letter or command.
4. The installer previews detected Windows and other UEFI loaders.
5. A privileged, narrowly scoped install component performs the transaction.
6. The installer verifies files and firmware state, then offers restart and
   graphical recovery options.

Windows support requires a native backend; the Linux helper and PolicyKit code
will not be reused as a shortcut.

## Release gates before enabling Install

- Transactional ESP staging, verification, commit, and automatic rollback.
- Restore and uninstall buttons tested from the GUI.
- Multi-disk and multi-ESP discovery with explicit device identity.
- QEMU/OVMF destructive integration tests and power-loss simulations.
- Signed firmware and signed Linux/Windows packages with reproducible hashes.
- Secure Boot behavior documented and tested.
- Recovery media instructions and a verified independent fallback path.
- CI assertion that release artifacts contain the intended capability mode.

Until all gates pass, the public build must continue to report `INSTALL|0`.

### Implemented beta foundation

The source tree now contains a CI-only transaction helper that tests staged
copying, SHA-256 verification, immutable backups, persistent recovery state,
reinstall without duplicate entries, automatic rollback after simulated
firmware failure, and full restore/uninstall semantics. The GUI recovery action
is present but compile-time disabled in public builds. The test helper is not
installed and cannot target the real ESP. Remaining gates are QEMU/OVMF boot
verification, package signing, and hardware qualification.
