# Boot-card interactions

The boot menu remains manual-only. APEX32 never starts a countdown and never
moves focus or launches an operating system without an explicit key event.

## Navigation

- Arrow keys move one card at a time and wrap at the ends.
- Tab moves to the next card.
- Home and End select the first and final discovered entries.
- Page Up and Page Down move by one bounded four-card page.
- Enter launches only the currently focused available entry.
- F2 opens read-only diagnostics, and Escape returns to firmware.

The layout continues to come from `Menu/CardLayout`, while bounded index
movement lives in `Menu/CardNavigation`; interaction code does not assume that
exactly two operating systems exist. Page transitions therefore work with one
entry, a partial final page, and the full 32-entry discovery limit.

## Focus animation

`Menu/CardAnimation` produces a bounded integer smoothstep value from 0 to
255. `WorkspaceMenu` uses that value to cross-fade the old and new card
background, text, accent, focus bar, and glow over six frames. There is no
floating point, dynamic allocation, or persistent animation state.

Frames are presented only after a navigation key. The short inter-frame stall
improves visual continuity but never selects or boots an entry. Missing timer
support merely removes the delay; it does not change menu policy. Any error
returned by GOP presentation or the firmware stall service is returned to the
caller.

## Identity marks

Identity classification and artwork remain outside the renderer in
`Assets/OsIdentity`. Known systems receive a registry-owned accent and mark;
unknown descriptions receive the neutral APEX32 generic EFI mark and remain
bootable. The renderer has no operating-system names or menu decisions.

## Validation and host safety

The sanitized host test checks easing endpoints, monotonicity, maximum-width
integer input, two-card cross-fading, deterministic frame counts, and both
simulated handoffs. The OVMF Windows-path handoff captures the framebuffer
after two Right-key transitions, requires each focus move to visibly change
the still-valid APEX32 frame, and only then sends Enter.

All OVMF work uses a temporary virtual ESP and private variable store. This
milestone does not run an installer, mount or write the host ESP, or change
host EFI variables.
