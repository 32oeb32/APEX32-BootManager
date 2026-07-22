# Native UEFI boot discovery

APEX32 discovers operating systems from the firmware's existing boot options
without changing NVRAM. The implementation reads `BootOrder`, parses each
active `Boot####` `EFI_LOAD_OPTION`, then performs a bounded variable walk to
append active options that are not listed in `BootOrder`.

## Safety and bounds

- discovery uses only `GetVariable()` and `GetNextVariableName()`;
- APEX32 never calls `SetVariable()` from production firmware;
- no more than 32 cards are retained;
- load-option buffers are capped at 4096 bytes;
- copied device paths are capped at 512 bytes per card;
- every device-path node is length-checked before it is copied;
- an End Entire node must terminate the copied path;
- inactive, truncated, malformed, and oversized options are ignored;
- APEX32's own entry is removed to prevent recursive self-launch;
- enumeration has a fixed upper bound and cannot loop forever on broken
  firmware.

All multi-byte fields are decoded from bytes, so unaligned firmware data is
never dereferenced as an integer or structure.

## Ordering and fallback

Valid entries appear in `BootOrder` order. Active `Boot####` variables omitted
from `BootOrder` are appended. The installer-generated `apex32.cfg` remains a
portable fallback and is merged after firmware discovery. Entries with the
same case-insensitive EFI file path are deduplicated.

Known names and file paths resolve through `Assets/OsIdentity`. A future or
unrecognized description receives the generic silver APEX32 identity and
remains selectable. A file-path node is extracted for diagnostics and
deduplication when present; the complete original device path is retained for
launch.

## Handoff

Firmware entries are handed directly to UEFI `LoadImage()` and `StartImage()`
using the validated device path copied from the load option. Configuration
entries continue to use a same-ESP `FileDevicePath()`. Failures return to the
menu with a visible EFI stage and status instead of changing `BootNext`,
resetting the machine, or silently falling through.

Some vendor-specific boot options are BDS policies rather than loadable EFI
images. APEX32 displays structurally valid active options, but `LoadImage()`
may reject a vendor-only or legacy device path. The menu reports that failure
safely. Legacy BIOS/BBS boot is not supported.

## Isolated tests

Host tests cover valid, inactive, truncated, unlisted, self-referential, and
unknown options plus native `LoadImage()`/`StartImage()` transfer. The OVMF
gate creates private `Boot7A32`–`Boot7A35` variables inside a disposable VARS
image, reboots through APEX32, discovers the native Kali entry, and launches
its signature payload:

```bash
./Tools/test-qemu-ovmf-native-discovery.sh
```

The script never reads or writes the host EFI System Partition or host UEFI
variables.
