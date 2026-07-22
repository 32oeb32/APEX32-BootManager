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
- The helper rejects non-root direct execution and validates the ESP mount,
  firmware source, and generated configuration before writing.
- External programs are called with argument arrays through `QProcess`; no
  shell command string is constructed.
- The first existing APEX32 firmware file is preserved before replacement.
- A stable release requires live-hardware GUI restore qualification.

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

The Windows GUI reports `TRANSACTION|1`, `INSTALL|0`, and `RESTORE|0`. Its
schema 1 transaction engine is compiled independently from GUI presentation,
but the only implemented firmware-store adapter is
`FileFirmwareStore`. That adapter reads and writes one caller-supplied JSON
file and is used exclusively by `apex32-windows-transaction-test`.

The Windows lifecycle executable creates its ESP with `QTemporaryDir`, injects
failure points, verifies rollback and restore, and checks a sentinel outside
the temporary ESP. It is not installed by CMake or CPack. Windows CI runs it
before packaging and explicitly rejects a staged test executable. No BCDEdit
or firmware-variable API exists in this milestone, so the transaction
foundation cannot mutate the runner's boot state. See
[`WINDOWS_TRANSACTION_FOUNDATION.md`](WINDOWS_TRANSACTION_FOUNDATION.md).
