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
```

Expected result:

```text
PASS: APEX32 reached a stable OVMF framebuffer (800x600, ...)
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

It does not yet prove that a hardware installer creates and promotes a real
`Boot####` entry, that every discovered OS loader starts successfully, or that
Secure Boot accepts an unsigned development build. Those remain separate gates
before the installer exposes **Make Default** in a public package.
