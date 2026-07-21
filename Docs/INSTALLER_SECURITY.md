# Installer security model

The installer must provide a zero-command user experience without turning the
desktop GUI into a permanently privileged process.

## Rules

- The Qt GUI runs as the logged-in user and performs read-only discovery.
- Default source builds define `APEX32_ENABLE_HARDWARE_INSTALL=OFF`. The GUI
  disables installation and the helper compiles out every mutating function.
- A direct `install` helper request is rejected before privilege or file checks.
- A future hardware-tested package may opt in at build time and must bundle an
  existing, verified firmware artifact; CMake otherwise fails configuration.
- Installation in such a future package invokes one fixed helper through polkit.
- The GUI passes `--disable-internal-agent` to `pkexec`, so authentication may
  use only the desktop's graphical PolicyKit agent and can never fall back to
  a terminal password prompt.
- The helper rejects non-root direct execution and validates the ESP mount,
  firmware source, and generated configuration before writing.
- External programs are called with argument arrays through `QProcess`; no
  shell command string is constructed.
- The first existing APEX32 firmware file is preserved before replacement.
- A stable release must add GUI-tested restore and uninstall operations.

The experimental, default-off install implementation depends on `findmnt`,
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
`SCAN|1`, `INSTALL|0`, and `TERMINAL_AUTH|0` for the public alpha. This makes an
accidental build-mode regression visible before release.

## Transaction test isolation

CI builds `apex32-installer-transaction-test` separately from the installed
helper. It is excluded from normal builds and never installed. Its mutating
code runs only when `APEX32_TRANSACTION_TEST=1` is explicit, the requested ESP
exactly matches `APEX32_TRANSACTION_TEST_ESP`, and that canonical path is below
the system temporary directory. Tool calls are redirected to deterministic
test stubs.

The transaction test covers staged SHA-256 verification, current-file
snapshots, immutable original backups, duplicate-free reinstall, post-install
verification, and rollback after an injected boot-order failure. The public
helper remains scan-only while this foundation is reviewed and extended with
restore/uninstall and QEMU/OVMF coverage.
