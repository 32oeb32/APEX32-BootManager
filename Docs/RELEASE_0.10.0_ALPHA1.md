# 0.10.0-alpha1

This milestone creates the separate public Community Edition while preserving
the personal v0.08 build unchanged.

## Implemented

- GPL-3.0 license and public repository documentation
- bounded generic boot configuration parser
- up to eight configured OS loaders, two visible per page
- cards containing only the OS icon and OS name
- `APEX32-SECURE.COM` directly beneath the cards
- generic EFI, Linux, Windows, Kali, and BlackArch visual identities
- manual-only navigation and F2 diagnostics
- Linux GUI scanner/configuration prototype
- polkit-authorized installation/default-entry helper prototype

## Validation

The sanitized host harness covers configuration loading, exact loader probes,
both simulated boot handoffs, diagnostics, keyboard navigation, safe return,
UEFI-entry lifecycle, and fallback backup behavior.

EDK II and real-hardware validation are required before this alpha is offered
as an installable package.
