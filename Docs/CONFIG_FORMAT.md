# APEX32 configuration format

The Community firmware reads an ASCII configuration file from:

```text
\EFI\APEX32\apex32.cfg
```

The Linux installer generates this file after **Systems → Scan Now**. The
format is intentionally bounded and simple enough to parse safely before an
operating system is running.

## Schema 1

```text
APEX32CFG|1
ENTRY|KALI LINUX|\EFI\kali\grubx64.efi|kali
ENTRY|WINDOWS BOOT MANAGER|\EFI\Microsoft\Boot\bootmgfw.efi|windows
```

Each `ENTRY` contains:

1. display name: printable ASCII, 1–39 characters;
2. absolute EFI path: printable ASCII, starts with `\`, up to 159 characters;
3. icon identifier: `generic`, `linux`, `windows`, `kali`, or `blackarch`.

Unknown icon identifiers deliberately fall back to the generic EFI mark.
Schema 1 supports at most eight entries and same-ESP paths. A malformed file
is rejected as a whole; the firmware never attempts to boot a partially parsed
configuration.

The installer writes configuration atomically. The firmware opens it read-only,
checks every configured loader, and disables handoff for any unavailable path.
