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
