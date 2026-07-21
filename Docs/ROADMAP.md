# Community roadmap

## PR #4 — graphics foundation

- reusable clipped GOP renderer with canonical back buffer
- 1920×1080 logical canvas with centered letterboxing
- alpha blending, gradients, borders, lines, images, and aligned embedded text
- original APEX32 emblem
- dynamic one-to-four-card pages for up to 32 validated entries
- pluggable OS identity registry and generic unknown-loader fallback
- resolution, overflow, and regional OVMF framebuffer validation

PR #4 deliberately leaves installation behavior unchanged and does not access
the host ESP. Native `Boot####`/multi-ESP enumeration remains a later discovery
milestone rather than being mixed into the renderer.

## 0.10.0-alpha1 — dynamic public foundation

- GPL-3.0 repository foundation
- dynamic schema 1 configuration
- name-only, paged OS cards
- permanent `APEX32-SECURE.COM` branding
- generic Windows/Linux/EFI recognition
- Linux Qt installer and privileged-helper prototype

## 0.10.0-alpha2 — portable discovery

- enumerate every mounted and unmounted FAT ESP through UDisks2
- identify loaders using paths, NVRAM entries, and signed-image metadata
- store partition GUID plus loader path for multi-ESP handoff
- add rename, icon selection, remove, and rescan actions

## 0.10.0-beta1 — zero-command packages

- signed Debian and Arch packages
- complete Install, Update, Recovery, Restore, and Uninstall tabs
- automatic dependency checks and graphical error recovery
- QEMU/OVMF integration matrix for Windows, GRUB, shim, and systemd-boot

Current beta foundation gates:

- completed: transactional mock-ESP install, rollback, persistent recovery
  state, restore, and idempotent reinstall;
- completed: boot the real APEX32 EFI application under OVMF and validate its
  captured GOP framebuffer;
- completed: seed and verify a private OVMF `Boot####` entry, promote it
  to first in `BootOrder`, cold reboot, and require the APEX32 framebuffer;
- completed: navigate Linux and Windows cards and transfer control through
  real UEFI `LoadImage()` / `StartImage()` to isolated signature payloads;
- automated gate: launch a real embedded-config GRUB image both directly and
  through the distribution's packaged shim, then require GRUB to chainload the
  isolated signature payload;
- completed: build a hardware-enabled Debian package from the verified EFI
  artifact, extract it without root, and verify its complete payload,
  dependency metadata, firmware identity, and `INSTALL|1` / `RESTORE|1`
  capability mode;
- automated gate: install, reinstall, and purge the package on a disposable
  runner, validate desktop/AppStream integration and protected ownership, and
  prove that package management does not touch an ESP or UEFI variables;
- next: exercise systemd-boot and a user-supplied Windows recovery image; and
- next: install, update, restore, and remove the package through the desktop
  GUI on the live-hardware qualification matrix.

## 1.0.0 — public stable

- reproducible firmware and installer builds
- signed release checksums and SBOM
- Secure Boot documentation and signing workflow
- hardware validation across multiple vendors and resolutions
- no terminal commands required for install, configuration, update, or removal

Direct ISO boot, legacy BIOS, and non-x86_64 firmware are not 1.0 targets.
