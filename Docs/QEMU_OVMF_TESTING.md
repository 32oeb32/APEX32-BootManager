# QEMU/OVMF firmware testing

The OVMF visual smoke test boots the real APEX32 PE/COFF application in a
temporary virtual machine. It does not simulate the renderer with host stubs.
The test waits for the APEX32 gateway, captures the virtual framebuffer through
QEMU's machine protocol, and verifies the dark, cyan, and red interface palette.

The test is isolated from the workstation:

- it creates a temporary virtual EFI System Partition;
- it uses a private writable copy of the OVMF variable store;
- it does not mount, read, or modify the host ESP;
- it cannot change the host `BootOrder`; and
- it removes the virtual ESP and variable store when finished.

## Local prerequisites

On Kali, Debian, and Ubuntu:

```bash
sudo apt install qemu-system-x86 ovmf
```

Build the firmware with the pinned EDK II revision, then run:

```bash
./Tools/test-qemu-ovmf.sh
./Tools/test-qemu-ovmf-bootorder.sh
```

Expected result:

```text
PASS: APEX32 reached a stable OVMF framebuffer (800x600, ...)
PASS: OVMF rebooted through seeded Boot7A32 as first BootOrder entry
```

The test locates common Debian, Ubuntu, Fedora, and Arch OVMF paths. A custom
installation can specify an explicit matching pair:

```bash
OVMF_CODE=/path/to/OVMF_CODE.fd \
OVMF_VARS=/path/to/OVMF_VARS.fd \
  ./Tools/test-qemu-ovmf.sh
```

Set `APEX32_QEMU_SCREENSHOT` to retain the captured PPM image. Set
`APEX32_KEEP_QEMU_SANDBOX=1` only while debugging to retain the complete
temporary virtual ESP, serial log, and OVMF variable store.

## What this gate proves

This gate proves that EDK II produced a bootable x86_64 UEFI application, OVMF
can launch it from the removable-media fallback path, GOP initializes, the
configuration is read from the virtual ESP, and the branded OS menu renders.

The second pass starts with a test-only seeder as `BOOTX64.EFI`. The seeder
creates `Boot7A32` for the canonical APEX32 path, removes duplicate `7A32`
values, places it first in `BootOrder`, verifies the stored order, and cold
reboots the guest. Because the fallback path still contains only the seeder,
the APEX32 framebuffer can appear after that reboot only when OVMF launches the
new NVRAM entry.

This proves the UEFI variable and default-entry mechanism in an isolated OVMF
variable store. It does not yet prove that the packaged hardware installer
performs the same transaction on every vendor firmware, that every discovered
OS loader starts successfully, or that Secure Boot accepts an unsigned
development build. Those remain separate gates before the installer exposes
**Make Default** in a public package.
