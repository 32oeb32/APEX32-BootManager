# Community roadmap

## PR #4 — graphics foundation

- reusable clipped GOP renderer with canonical back buffer
- 1920×1080 logical canvas with centered letterboxing
- alpha blending, gradients, borders, lines, images, and aligned embedded text
- embedded original APEX32 emblem asset (not inserted into the approved intro)
- dynamic one-to-four-card pages for up to 32 validated entries
- pluggable OS identity registry and generic unknown-loader fallback
- resolution, overflow, and regional OVMF framebuffer validation

PR #4 deliberately left installation behavior unchanged and did not access
the host ESP. Native discovery was kept out of the renderer and completed as
the separate PR #5 milestone below.

## PR #5 — native boot discovery

- completed: bounded read-only `BootOrder` and `Boot####` enumeration;
- completed: active load-option parsing with malformed-entry rejection;
- completed: APEX32 self-entry filtering;
- completed: native device-path `LoadImage()` / `StartImage()` handoff in
  configuration-free recovery mode;
- completed: generic cards for future and unknown EFI descriptions;
- completed: isolated OVMF discovery and handoff through private variables;
- next: raw ESP scanning for loaders that have no firmware entry; and
- completed in PR #10: installed configuration is authoritative and uses the
  qualified same-ESP handoff instead of native path replacement; and
- next: broader vendor qualification for configuration-free recovery paths.

## PR #6 — animated boot-card interactions

- bounded six-frame focus cross-fades after explicit navigation input;
- Arrow, Tab, Home, End, Page Up, and Page Down navigation across dynamic
  four-card pages;
- no countdown, background focus movement, or automatic boot;
- host coverage for easing overflow guards and deterministic menu frames; and
- OVMF framebuffer validation after two real keyboard focus transitions.

PR #6 does not change Linux or Windows installer capabilities. In particular,
the Windows package remains a read-only scan preview until its native
transaction, rollback, and restore milestone is complete.

## PR #7 — Windows transaction and disposable lifecycle foundation

- reusable Windows file/configuration transaction independent from GUI code;
- schema 1 recovery state with immutable pre-community backups and hashes;
- injected firmware-store interface with duplicate-free APEX32 promotion;
- automatic rollback across file and simulated firmware failures;
- full restore for both replaced files and clean installs;
- disposable Windows-runner lifecycle coverage confined to a temporary ESP;
- NSIS assertion that the transaction test binary is never packaged; and
- visible GUI transaction/restore status while hardware controls remain
  disabled with `INSTALL|0` and `RESTORE|0`.

PR #7 deliberately does not contain a BCDEdit or firmware-variable backend.
The next Windows milestone packages the verified EFI image and implements the
narrow real firmware store behind this already-tested transaction contract.

## PR #8 — Windows transaction path repair

- normalized Qt transaction paths before case-insensitive confinement;
- restored the disposable Windows lifecycle on MSVC; and
- changed no installation capability or host firmware state.

## PR #9 — Windows production installer and isolated firmware lifecycle

- package the same pinned, verified EFI artifact used by Linux;
- enable graphical scan, install, make-default, and restore only in that
  hardware-enabled package;
- manage `Boot####` and `BootOrder` through Windows firmware-variable APIs;
- preserve exact variables and immutable ESP recovery state;
- block the unsigned Community beta while Secure Boot is enabled;
- test native logic against fake variables on Windows and a real create,
  promote, restore, and remove cycle under private OVMF firmware;
- install and remove the NSIS package on the disposable runner; and
- eliminate duplicate feature-branch push and pull-request workflows.

PR #9 does not claim live-hardware qualification or a signed Secure Boot
release. Those are the consolidated final release gates.

## PR #10 — first-hardware qualification regressions

The first HP/Kali qualification passed package safety, graphical scan,
transactional install, make-default, and graphical restore. Reboot exposed two
firmware regressions: native entries replaced the installer-selected same-ESP
paths, and unselected stale GRUB/Windows options appeared in the menu. The test
also showed that Kali/Hyprland could have no active graphical PolicyKit agent
after package installation.

- restore the previously approved APEX32 Secure intro without the inserted
  diamond/emblem scene;
- make a valid non-empty installer configuration an authoritative allow-list;
- use native `Boot####` discovery only when no usable configuration exists;
- add a private-OVMF precedence test with unrelated native entries present;
- recommend graphical PolicyKit agents in the Debian package;
- start the known Kali/Hyprland user agent automatically before authorization;
- preserve terminal authentication as disabled and fail closed; and
- document the physical qualification evidence and mandatory retest sequence.

PR #10 creates a new candidate; it does not turn the failed first reboot into a
pass. Linux must complete the full physical retest, and Windows must separately
pass an isolated Windows UEFI VM plus real Windows hardware before release.

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
