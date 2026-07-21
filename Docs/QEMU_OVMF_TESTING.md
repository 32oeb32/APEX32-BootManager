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
./Tools/test-qemu-ovmf-handoff.sh
./Tools/test-qemu-ovmf-linux-loaders.sh
```

Expected result:

```text
PASS: APEX32 reached a stable OVMF framebuffer (800x600, ...)
PASS: OVMF rebooted through seeded Boot7A32 as first BootOrder entry
PASS: APEX32 completed a real UEFI handoff to the linux test payload (...)
PASS: APEX32 completed a real UEFI handoff to the windows test payload (...)
PASS: APEX32 launched real standalone GRUB and GRUB chainloaded the test payload
PASS: APEX32 launched real distribution shim and shim reached GRUB's test payload
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

The handoff pass boots APEX32 twice. QMP keyboard input selects the configured
Kali card during the first run and the configured Windows card during the
second, then sends Enter. Each configured path contains a test-only UEFI child
application with a unique framebuffer signature. Requiring that signature
proves that the real firmware handled input, resolved the loader on the same
virtual ESP, and successfully called UEFI `LoadImage()` and `StartImage()`.

The real-Linux-loader pass creates a standalone GRUB EFI application using the
host distribution's `grub-mkstandalone`, launches it from the configured Kali
path, and requires GRUB to chainload the signature application. It then places
the distribution's packaged `shimx64.efi` in front of the same GRUB image and
requires the complete APEX32 → shim → GRUB → test-payload chain to finish. The
real loader binaries come from the CI runner's operating-system packages; they
are never committed to or redistributed by this repository.

This proves the UEFI variable and default-entry mechanism in an isolated OVMF
variable store, the general child-image handoff mechanism, and representative
GRUB and shim execution with Secure Boot disabled. The Windows-path target is
still synthetic because Microsoft binaries cannot be redistributed in this
GPL repository. The gate also does not prove that the packaged hardware
installer performs the same transaction on every vendor firmware or that
Secure Boot accepts an unsigned development build. Those remain separate gates
before the installer exposes **Make Default** in a public package.
