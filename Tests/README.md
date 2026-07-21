# Tests

Run the complete host suite with:

```bash
./Tools/test-host.sh
```

The firmware smoke test compiles production C++ against narrow host UEFI
stubs with AddressSanitizer and UndefinedBehaviorSanitizer. It validates:

- the complete intro and manual-only menu lifecycle;
- schema 1 configuration loading from the canonical ESP path;
- exact configured loader probes;
- dynamic card navigation and F2 diagnostics;
- both simulated `LoadImage()` / `StartImage()` handoffs;
- cleanup and safe menu return after loader failure; and
- final Escape to a black frame with the text cursor restored.

The same command runs isolated UEFI-entry and fallback lifecycle tests using
stubbed system tools. They verify registration, one-time test boot, primary
promotion without dropping unrelated entries, exact `BootOrder` restoration,
immutable first backup behavior, and fallback restoration.

Host tests do not validate EDK II linkage, PE/COFF generation, real GOP
hardware, firmware NVRAM behavior, Qt/polkit integration, or the final package.
Those are separate release gates.

After building `Apex32BootManager.efi`, run:

```bash
./Tools/test-qemu-ovmf.sh
./Tools/test-qemu-ovmf-bootorder.sh
./Tools/test-qemu-ovmf-handoff.sh
./Tools/test-qemu-ovmf-linux-loaders.sh
```

This boots the real EFI application under QEMU/OVMF and validates a captured
GOP framebuffer. The second command creates a private APEX32 `Boot####` option,
promotes it to the first `BootOrder` entry, cold reboots OVMF, and requires the
gateway to render through that entry. The third command navigates the real
APEX32 menu and requires successful `LoadImage()` / `StartImage()` transfer to
test-only Linux-path and Windows-path UEFI child applications. All commands use
only a temporary virtual ESP and private OVMF variable store. See
[QEMU/OVMF firmware testing](../Docs/QEMU_OVMF_TESTING.md).

The fourth command adds real Linux loader coverage. It builds a self-contained
GRUB EFI image with an early embedded configuration from the locally installed
distribution package, first launches GRUB directly through APEX32, then
launches the packaged shim which in turn starts GRUB. Both paths must chainload
the isolated framebuffer-signature payload. Neither GRUB nor shim is stored in
this repository.

The Debian package has its own isolated gate:

```bash
./Tools/test-linux-package.sh \
  Installer/Linux/package-build/packages/apex32-boot-manager_0.11.0~beta1_amd64.deb \
  Build/DEBUG_GCC/X64/Apex32BootManager.efi
```

It extracts rather than installs the package and verifies the installed file
layout, executable modes, desktop integration, PolicyKit path, declared runtime
dependencies, install/restore capability flags, and byte-identical firmware.
The `linux-package` workflow then runs the CI-confined package lifecycle test,
which installs, reinstalls, and purges the candidate only on a disposable
runner with no ESP or UEFI variable filesystem.
