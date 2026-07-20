# Installer security model

The installer must provide a zero-command user experience without turning the
desktop GUI into a permanently privileged process.

## Rules

- The Qt GUI runs as the logged-in user and performs read-only discovery.
- Installation invokes one fixed helper through polkit.
- The helper rejects non-root direct execution and validates the ESP mount,
  firmware source, and generated configuration before writing.
- External programs are called with argument arrays through `QProcess`; no
  shell command string is constructed.
- The first existing APEX32 firmware file is preserved before replacement.
- A stable release must add GUI-tested restore and uninstall operations.

The alpha helper currently depends on `findmnt`, `lsblk`, and `efibootmgr` at
fixed `/usr/bin` paths. Packaging must declare and verify these dependencies.
