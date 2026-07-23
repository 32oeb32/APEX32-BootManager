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

## HP Victus / Kali — corrected v0.11.0-beta1 candidate

The PR #10 corrective candidate was retested on the same machine without
manual PolicyKit service or firmware commands. The complete result passed:

- the graphical PolicyKit agent became available automatically;
- the scan found the expected Kali, BlackArch, and recovery loaders;
- only Kali and BlackArch were selected for the installed configuration;
- installation completed transactionally with one APEX32 firmware entry;
- reboot displayed the approved intro without the rejected diamond scene;
- exactly the two selected operating-system cards appeared;
- the Kali card successfully started Kali;
- the BlackArch card successfully started BlackArch;
- graphical Recovery reported verified restoration; and
- the machine subsequently booted Kali normally.

The configured-precedence OVMF test also passes while unrelated native options
are present. Together, these results complete the corrected Linux physical
qualification for this machine. Broader vendor coverage, signed artifacts,
Secure Boot, and recovery-media qualification remain separate release gates.

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

PR #11 additionally binds Windows transactions to the GPT hard-drive identity
used by Windows Boot Manager. Its fake-variable tests cover a separate
Linux-side APEX32 entry, exact Windows-side restore, and ambiguous-identity
failure. This closes the code-level multi-ESP blocker but does not replace the
VM or physical Windows gates above.
