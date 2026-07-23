# Install APEX32 on Windows

This is the planned end-user guide for the APEX32 Community beta on Windows.
The Windows package is built and tested in CI, but it must still pass the
isolated Windows UEFI VM and physical Windows qualification gates before the
download is approved for end users.

**Do not install a workflow artifact or release candidate on physical Windows
until this page marks the package as hardware-qualified.**

## Requirements

- Windows 11 x64 for the qualified beta test matrix;
- Windows 10 1809 or later is technically targeted but not yet physically
  qualified;
- Windows booted in UEFI mode, not Legacy BIOS/CSM;
- Secure Boot disabled for the unsigned Community beta;
- an administrator account for the standard UAC consent dialog;
- recovery media and a working Windows Boot Manager entry; and
- a recent backup of important files.

## Download

After Windows qualification is complete:

1. Open the official [GitHub Releases](https://github.com/32oeb32/APEX32-BootManager/releases).
2. Open the latest qualified beta release.
3. Download both:
   - `APEX32-Community-Setup.exe`
   - `APEX32-Community-Setup.exe.sha256`
4. Verify that the release is marked as Windows-qualified.
5. Double-click `APEX32-Community-Setup.exe`.

Windows users do not clone the repository, install Qt, run CMake, open
PowerShell, use BCDEdit, or assign an ESP drive letter.

## Install the desktop application

1. Open the Setup executable.
2. Review the Windows security and publisher information.
3. Complete the normal Setup wizard.
4. Leave **Run APEX32 Community Installer** selected on the final page.

Installing or removing the desktop application alone does not modify EFI
files or firmware variables.

## Scan and install APEX32

1. Select **Scan Systems**.
2. Approve the standard Windows UAC dialog.
3. Wait while APEX32 temporarily mounts the Windows system ESP, scans it, and
   removes the private mount automatically.
4. Review every detected EFI loader.
5. Select only the operating systems that should appear in APEX32.
6. Select **Install APEX32 and Make Default**.
7. Confirm the transaction.
8. Restart only after APEX32 reports verified success.

The installer verifies its packaged EFI image, captures the existing files and
exact UEFI variables, commits the new firmware and configuration, promotes the
correct APEX32 entry, and reads the result back. Failure invokes rollback.

## Multiple EFI System Partitions

Windows and Linux may use different ESPs on the same computer. APEX32 binds the
Windows transaction to the GPT hard-drive identity used by Windows Boot
Manager. An APEX32 entry on a Linux or recovery ESP is preserved and is never
reused for files written to the Windows ESP.

If Windows Boot Manager is missing or multiple conflicting Windows ESP
identities are present, installation stops before mutation instead of guessing.

## First reboot checklist

1. Confirm the approved APEX32 Secure intro appears.
2. Confirm only the systems selected in the Windows GUI appear.
3. Select **Windows** manually.
4. Confirm Windows starts normally.
5. Open APEX32 again and verify Recovery is available.

Keep a VM snapshot or physical recovery medium until installation, handoff,
and recovery all pass.

## Graphical recovery

1. Open **APEX32 Community Installer** in Windows.
2. Approve UAC when requested.
3. Select **Restore Previous Boot State**.
4. Confirm the operation.
5. Wait for verified success.
6. Restart and confirm Windows Boot Manager starts normally.

Restore writes back the exact saved `Boot####` option and `BootOrder`, restores
or removes transaction-owned files, removes only the APEX32 entry created by
the Windows transaction, and preserves unrelated APEX32 entries on other ESPs.

Always complete graphical Restore before uninstalling the desktop application.

## Secure Boot

The Community beta EFI application is unsigned. Installation refuses to make
it the default while Secure Boot is enabled. Do not bypass this protection.
A future signed release will have a separate Secure Boot qualification and
key-management guide.

## Current Windows qualification sequence

The exact downloadable Setup executable must pass, in order:

1. disposable Windows packaging and native-variable tests;
2. private OVMF boot and firmware lifecycle tests;
3. a snapshot-backed Windows 11 UEFI VM;
4. scan, install, make-default, reboot, Windows handoff, and graphical restore;
5. a physical Windows UEFI machine with recovery media; and
6. release signing and Secure Boot policy qualification.

Steps 1 and 2 are complete. Steps 3–5 remain before the Windows beta is
approved for physical users.

## Report a problem

Open a [GitHub issue](https://github.com/32oeb32/APEX32-BootManager/issues)
with:

- computer or VM model;
- Windows version and build;
- UEFI and Secure Boot state;
- installer version and SHA-256;
- detected loader names;
- exact GUI error; and
- a screenshot without product keys, recovery keys, serial numbers, or other
  personal information.

Implementation and qualification details are in
[Windows graphical installation](WINDOWS_PRODUCTION_INSTALLER.md),
[Installer security](INSTALLER_SECURITY.md), and
[Hardware qualification](HARDWARE_QUALIFICATION.md).
