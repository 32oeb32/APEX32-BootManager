# Installer security model

The installer must provide a zero-command user experience without turning the
desktop GUI into a permanently privileged process.

## Rules

- The Qt GUI runs as the logged-in user and performs read-only discovery.
- Default source builds define `APEX32_ENABLE_HARDWARE_INSTALL=OFF`. The GUI
  disables installation and the helper compiles out every mutating function.
- A direct `install` helper request is rejected before privilege or file checks.
- The beta package candidate opts in at build time and must bundle an existing,
  verified firmware artifact; CMake otherwise fails configuration.
- Installation in that package invokes one fixed helper through polkit.
- The GUI passes `--disable-internal-agent` to `pkexec`, so authentication may
  use only the desktop's graphical PolicyKit agent and can never fall back to
  a terminal password prompt.
- The Debian package recommends compatible graphical PolicyKit agents. Before
  authorization the GUI starts the fixed installed Kali/Hyprland user unit
  when present, without a shell, root service manager, or terminal command.
- The helper rejects non-root direct execution and validates the ESP mount,
  firmware source, and generated configuration before writing.
- External programs are called with argument arrays through `QProcess`; no
  shell command string is constructed.
- The first existing APEX32 firmware file is preserved before replacement.
- A stable release requires live-hardware install, selected-loader handoff,
  and GUI restore qualification.

The default-off install implementation depends on `findmnt`,
`lsblk`, and `efibootmgr` at fixed `/usr/bin` paths. Packaging must declare and
verify these dependencies before enabling it.

The GUI also provides an explicit regular-user test mode backed by a temporary
mock ESP. Test mode disables installation and never invokes the helper. See
[`INSTALLER_TESTING.md`](INSTALLER_TESTING.md).

Many distributions mount the real ESP with root-only permissions. The GUI
first attempts unprivileged read-only discovery. An explicit **Scan Now** may
invoke the same narrowly scoped helper through a graphical authorization
prompt with the `scan` operation. That operation emits only bounded EFI loader
paths and cannot write files or firmware variables. Multi-ESP enumeration is
still required before beta.

The GUI exposes a machine-readable `--capabilities` response. CI requires
`SCAN|1`, `INSTALL|0`, and `TERMINAL_AUTH|0` for ordinary source builds. This makes an
accidental build-mode regression visible before release.

## Transaction test isolation

CI builds `apex32-installer-transaction-test` separately from the installed
helper. It is excluded from normal builds and never installed. Its mutating
code runs only when `APEX32_TRANSACTION_TEST=1` is explicit, the requested ESP
exactly matches `APEX32_TRANSACTION_TEST_ESP`, and that canonical path is below
the system temporary directory. Tool calls are redirected to deterministic
test stubs.

The transaction test covers staged SHA-256 verification, current-file
snapshots, immutable original backups, persistent pre-install state,
duplicate-free reinstall, post-install verification, rollback after an
injected boot-order failure, an injected restore failure, and complete restore
of files, boot order, and firmware-entry ownership. Ordinary source builds
remain scan-only.

The Debian package workflow is deliberately separate. It must embed the
verified PE/COFF firmware and its extracted GUI must report `INSTALL|1`,
`RESTORE|1`, and `TERMINAL_AUTH|0`. The production helper accepts firmware only
from the root-owned packaged path and accepts configuration only from a private
installer-generated temporary file owned by the authenticated desktop user.
Every configuration entry is validated again before privileged mutation.

## Windows transaction isolation

Ordinary Windows source builds report `TRANSACTION|1`, `INSTALL|0`, and
`RESTORE|0`. The CI package opts in only after receiving the verified firmware
from the pinned EDK II build; that package reports `INSTALL|1` and `RESTORE|1`.
The packaged SHA-256 is compiled into the GUI and verified again before any
transaction.

The Windows lifecycle executable creates its ESP with `QTemporaryDir`, injects
failure points, verifies rollback and restore, and checks a sentinel outside
the temporary ESP. A second executable runs `NativeFirmwareStore` against
in-memory variables and verifies new-entry creation, same-ESP entry reuse,
cross-ESP isolation, promotion failure rollback, exact `BootOrder` restore,
ambiguous-identity failure, and Secure Boot state parsing. Neither executable
is installed by CMake or CPack.

Production uses `GetFirmwareEnvironmentVariableExW` and
`SetFirmwareEnvironmentVariableExW` after enabling the narrowly scoped
`SE_SYSTEM_ENVIRONMENT_NAME` privilege. It reuses an APEX32 entry only when its
GPT hard-drive node matches Windows Boot Manager, preserves same-named entries
on other ESPs, and rejects duplicate same-ESP entries, malformed load options,
missing or ambiguous Windows ESP identities, and unexpected changes to its
reserved boot number. It never parses localized BCDEdit output.

The OVMF job provides the real variable-writing gate: inside private firmware
it creates and promotes an APEX32 entry, restores the exact original order,
deletes the created entry, and exits only after read-back verification. The
Windows runner never invokes the production variable adapter. Secure Boot
causes installation to fail closed because the Community beta is unsigned;
Restore remains available. See
[`WINDOWS_PRODUCTION_INSTALLER.md`](WINDOWS_PRODUCTION_INSTALLER.md).
