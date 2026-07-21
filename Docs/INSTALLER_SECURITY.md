# Installer security model

The installer must provide a zero-command user experience without turning the
desktop GUI into a permanently privileged process.

## Rules

- The Qt GUI runs as the logged-in user and performs read-only discovery.
- Installation invokes one fixed helper through polkit.
- The GUI passes `--disable-internal-agent` to `pkexec`, so authentication may
  use only the desktop's graphical PolicyKit agent and can never fall back to
  a terminal password prompt.
- The helper rejects non-root direct execution and validates the ESP mount,
  firmware source, and generated configuration before writing.
- External programs are called with argument arrays through `QProcess`; no
  shell command string is constructed.
- The first existing APEX32 firmware file is preserved before replacement.
- A stable release must add GUI-tested restore and uninstall operations.

The alpha helper currently depends on `findmnt`, `lsblk`, and `efibootmgr` at
fixed `/usr/bin` paths. Packaging must declare and verify these dependencies.

The GUI also provides an explicit regular-user test mode backed by a temporary
mock ESP. Test mode disables installation and never invokes the helper. See
[`INSTALLER_TESTING.md`](INSTALLER_TESTING.md).

Many distributions mount the real ESP with root-only permissions. The GUI
first attempts unprivileged read-only discovery. An explicit **Scan Now** may
invoke the same narrowly scoped helper through a graphical authorization
prompt with the `scan` operation. That operation emits only bounded EFI loader
paths and cannot write files or firmware variables. Multi-ESP enumeration is
still required before beta.
