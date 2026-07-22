# QEMU/OVMF firmware testing

The OVMF visual smoke test boots the real APEX32 PE/COFF application in a
temporary virtual machine. It does not simulate the renderer with host stubs.
The test waits for the APEX32 gateway, captures the virtual framebuffer through
QEMU's machine protocol, and verifies the dark, cyan, and red interface palette.
It also requires accent pixels in the central scene region, so a stray colored
pixel or fully black frame cannot satisfy the gate. Validation is regional and
threshold-based rather than fragile exact screenshot matching.

The test is isolated from the workstation:

- it creates a temporary virtual EFI System Partition;
- it uses a private writable copy of the OVMF variable store;
- it does not mount, read, or modify the host ESP;
- it cannot change the host `BootOrder`; and
- it removes the virtual ESP and variable store when finished.

The graphics-foundation branch and PR #4 never invoke the hardware installer.

## Local prerequisites

On Kali, Debian, and Ubuntu:

```bash
sudo apt install qemu-system-x86 ovmf
```

Build the firmware with the pinned EDK II revision, then run:

```bash
./Tools/test-qemu-ovmf.sh
./Tools/test-qemu-ovmf-bootorder.sh
./Tools/test-qemu-ovmf-native-discovery.sh
./Tools/test-qemu-ovmf-handoff.sh
./Tools/test-qemu-ovmf-linux-loaders.sh
./Tools/test-qemu-ovmf-installer-lifecycle.sh
```

Expected result:

```text
PASS: APEX32 reached a stable OVMF framebuffer (800x600, ...)
PASS: OVMF rebooted through seeded Boot7A32 as first BootOrder entry
PASS: APEX32 discovered Boot7A33 and launched its native device path
PASS: APEX32 completed a real UEFI handoff to the linux test payload (...)
PASS: APEX32 completed a real UEFI handoff to the windows test payload (...)
PASS: APEX32 launched real embedded-config GRUB and GRUB chainloaded the test payload
PASS: APEX32 launched real distribution shim and shim reached GRUB's test payload
PASS: OVMF created and promoted an APEX32 entry, then restored exact BootOrder and removed it
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

The native-discovery pass seeds private APEX32, Linux, Windows, and unknown
`Boot####` options in the disposable variable store. APEX32 must exclude its
own option and the read-only `BootCurrent` option that launched it, retain the
unknown option as a generic card, select `Boot7A33`, and
launch the Linux signature payload using the complete device path stored in
that firmware variable. Platform-internal firmware-volume applications such as
OVMF setup and its internal shell are excluded because they are maintenance
tools rather than operating-system targets. The host's NVRAM and ESP remain
unreachable.

The handoff pass boots APEX32 twice. QMP keyboard input selects the configured
Kali card during the first run and the configured Windows card during the
second, then sends Enter. Before the Windows handoff, the analyzer captures
both Right-key focus transitions, requires each transition to visibly change
the framebuffer, and requires the resulting frame to retain the APEX32 gateway
palette. Each configured path contains a test-only UEFI child
application with a unique framebuffer signature. Requiring that signature
proves that the real firmware handled input, resolved the loader on the same
virtual ESP, and successfully called UEFI `LoadImage()` and `StartImage()`.

The real-Linux-loader pass creates a self-contained GRUB EFI application using
the host distribution's `grub-mkimage`, embeds an early chainload configuration
and the required filesystem/search/chain modules, launches it from the
configured Kali path, and requires GRUB to chainload the signature application.
It then places
the distribution's packaged `shimx64.efi` in front of the same GRUB image and
requires the complete APEX32 → shim → GRUB → test-payload chain to finish. The
real loader binaries come from the CI runner's operating-system packages; they
are never committed to or redistributed by this repository.

The installer-lifecycle pass boots a dedicated test-only UEFI application. It
creates `Boot7A40` in the private OVMF variable store, places it first without
dropping existing entries, reads the result back, restores the byte-identical
original `BootOrder`, deletes `Boot7A40`, and reports success through QEMU's
debug-exit device. This test has no path to the host ESP or firmware variables.

Together these gates prove the UEFI variable/default-entry mechanism in an
isolated OVMF store, exact lifecycle restoration, the general child-image
handoff mechanism, and representative GRUB and shim execution with Secure Boot
disabled. The Windows-path target remains synthetic because Microsoft binaries
cannot be redistributed in this GPL repository. They do not prove behavior on
every vendor firmware or permit the unsigned Community beta under Secure Boot;
those remain final live-hardware and signing gates.
