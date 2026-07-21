# Zero-terminal GUI installer release plan

The APEX32 end-user installer must behave like a normal desktop application.
Cloning, compiling, and terminal commands are contributor workflows only.

## Published alpha and beta candidate

- Linux Qt GUI for safe loader discovery and preview.
- Read-only ESP scan through a narrow PolicyKit helper when required.
- Graphical authorization only; `pkexec` terminal fallback is disabled.
- Source builds keep hardware installation compiled out in both GUI and
  helper.
- The CI-built Debian beta package is the only build mode that embeds verified
  firmware and enables `INSTALL|1` and `RESTORE|1`.
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

## Release gates

- Completed: transactional ESP staging, verification, commit, and automatic
  rollback.
- Completed: persistent recovery state and full restore behavior in confined
  transaction tests.
- Completed: real OVMF fallback, NVRAM-first boot, Linux/Windows handoff,
  GRUB, and shim execution gates.
- Completed: reproducible package layout and capability inspection without
  installing the package on the CI host.
- Completed: install, reinstall, desktop/AppStream validation, and purge on a
  disposable CI runner that refuses hosts with an ESP or EFI variables.
- Multi-disk and multi-ESP discovery with explicit device identity.
- Live-hardware install, reboot, reinstall, restore, and independent recovery
  validation on the release machine matrix.
- Power-loss simulations at each committed transaction boundary.
- Signed firmware and signed Linux/Windows packages with reproducible hashes.
- Secure Boot behavior documented and tested.
- Recovery media instructions and a verified independent fallback path.
- CI assertion that release artifacts contain the intended capability mode.

Until the remaining gates pass, source builds continue to report `INSTALL|0`.
Only the tested package candidate may report `INSTALL|1`; it must not be
published as a stable release yet.

### Implemented beta foundation

The source tree contains a CI-only transaction helper that tests staged
copying, SHA-256 verification, immutable backups, persistent recovery state,
reinstall without duplicate entries, automatic rollback after simulated
firmware failure, and full restore/uninstall semantics. The GUI recovery action
is present but compile-time disabled in source builds. The test helper is not
installed and cannot target the real ESP. A separate package workflow embeds
the real EDK II output, enables the production helper, extracts the `.deb`
without root, and verifies all payloads and dependencies. Remaining gates are
live hardware, multi-ESP identity, Secure Boot, recovery media, and release
signing.
