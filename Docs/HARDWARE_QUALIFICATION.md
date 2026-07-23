# Hardware qualification

Automated OVMF and host tests are necessary but do not qualify a release by
themselves. This document records physical-machine evidence, including failed
results, so a regression cannot be hidden by a green CI run.

## HP Victus / Kali — first v0.11.0-beta1 candidate

Environment:

- HP Victus 15-fa1xxx, x86_64 UEFI;
- Kali Linux with Hyprland;
- Secure Boot disabled;
- Kali and BlackArch loaders on the selected ESP;
- existing Windows, GRUB, disk, USB, and APEX32 firmware options; and
- original APEX32 firmware and fallback preserved before the test.

Passed before reboot:

- Linux artifact checksum and package metadata verification;
- package installation without changing the ESP or UEFI variables;
- graphical scan of the real ESP after graphical PolicyKit authorization;
- selection of Kali and BlackArch only;
- transactional firmware/config installation;
- immutable backup and recovery-state creation;
- one duplicate-free APEX32 firmware entry; and
- APEX32 first in the preserved `BootOrder`.

Failed after reboot:

- the startup animation inserted an unapproved diamond/emblem scene;
- stale GRUB and Windows firmware options appeared beside the two selected
  systems; and
- every displayed card failed its loader handoff on the physical firmware.

The operator escaped APEX32, used firmware setup to boot Kali directly, then
used the graphical Recovery tab. Recovery passed:

- the pre-community firmware hash was restored;
- the transaction backup was consumed;
- generated configuration and recovery state were removed; and
- the exact original `BootOrder` was restored.

The candidate is therefore **failed and not releasable**, despite its earlier
green CI runs. The transaction/recovery design is supported by physical
evidence; the boot-selection and handoff design required correction.

## Corrective candidate gates

Before Linux publication, the corrected candidate must pass on the same
machine without manual service or firmware commands:

1. package install and launch from the desktop;
2. automatic graphical PolicyKit agent availability;
3. scan showing the expected loaders;
4. install with only Kali and BlackArch selected;
5. reboot into the approved intro and exactly two cards;
6. boot Kali successfully;
7. reboot and boot BlackArch successfully;
8. reinstall/update without duplicates;
9. graphical restore; and
10. direct Kali boot with exact file and `BootOrder` restoration verified.

The candidate must also pass the isolated configured-precedence OVMF test,
which seeds unrelated native options but requires the selected same-ESP loader
to be the only card and successful handoff.

## Windows boundary

The Windows package is not qualified by Linux or OVMF results. Before Windows
publication it must pass, in order:

1. disposable Windows CI packaging and transaction tests;
2. an isolated Windows UEFI virtual machine with snapshot recovery;
3. graphical scan, install, make-default, reboot, loader handoff, and restore;
4. on multi-ESP systems, preservation of any Linux-ESP APEX32 entry while the
   Windows transaction creates, verifies, and restores only its Windows-ESP
   entry;
5. a real Windows UEFI machine with recovery media available; and
6. artifact signing and Secure Boot policy qualification.

No public release claim may be based only on scripted static tests or a
synthetic Windows-path payload.
