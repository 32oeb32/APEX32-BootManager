# Source installer testing

This guide is for contributors and technical users who are comfortable cloning
a repository and running build commands. The eventual packaged APEX32 installer
must remain a desktop application that does not ask end users to type commands.

The tests are deliberately split into safe mock-ESP testing and an authorized,
read-only scan of the real EFI System Partition (ESP). The default alpha build
compiles the hardware-install operation out of both the GUI and helper.

## Safety rules

- Use a fresh clone, separate from any personal APEX32 development tree.
- Run the GUI and all test scripts as the normal desktop user, never with
  `sudo`.
- Confirm that **Hardware Installation Disabled in Alpha** is disabled.
- Do not copy files to the ESP or change UEFI variables as part of this guide.
- Stop if a password prompt appears inside the terminal. Authentication must be
  handled by the desktop's graphical PolicyKit agent.

## 1. Install source-test dependencies

On current Kali or Debian-based systems:

```bash
sudo apt update
sudo apt install --yes \
  git \
  build-essential \
  cmake \
  ninja-build \
  qt6-base-dev
```

A graphical PolicyKit authentication agent is also required for the real ESP
scan. Desktop environments such as GNOME and KDE normally provide one. On
Hyprland under Kali, install and start `hyprpolkitagent`:

```bash
sudo apt install --yes hyprpolkitagent
systemctl --user start hyprpolkitagent.service
systemctl --user is-active hyprpolkitagent.service
```

The final command must print `active`. Starting this user service does not grant
APEX32 permanent root access; it only provides the desktop password dialog used
by PolicyKit.

## 2. Clone a clean copy

For the default branch:

```bash
TEST_DIR="$HOME/Documents/APEX32-Community-Test-$(date +%Y%m%d-%H%M%S)"

git clone --depth 1 \
  https://github.com/32oeb32/APEX32-BootManager.git \
  "$TEST_DIR"

cd "$TEST_DIR"
```

To test a named candidate branch before it is merged, add
`--branch BRANCH_NAME` to the clone command. For example:

```bash
git clone --depth 1 \
  --branch installer/authorized-scan \
  https://github.com/32oeb32/APEX32-BootManager.git \
  "$TEST_DIR"
```

## 3. Run the non-privileged automated tests

Run both suites without `sudo`:

```bash
./Tools/test-host.sh
./Tools/test-installer-user.sh
```

Expected final lines include:

```text
PASS: 67 frames, 7 keys, dynamic config and manual boot paths clean
PASS: UEFI entry lifecycle, verbose parsing, duplicate guard, and rollback
PASS: fallback install, immutable backup, status, and restore
PASS: regular-user discovery found 5 systems, preferred shim, excluded APEX32, kept fallback as unselected recovery, parsed authorized scan, and generated schema 1
PASS: transactional install, idempotent reinstall, and immutable backup
PASS: injected boot-order failures restored files, order, and new entry
PASS: restore failure rolled back safely, then full restore removed state
PASS: transaction test binary was confined to its declared temporary ESP
PASS: helper enforced scan-only install gate and refused unprivileged scan
PASS: hardware-install opt-in required firmware and the mock ESP was unchanged
PASS: regular-user installer test completed without sudo or terminal authentication
```

The installer test creates a temporary mock ESP, detects Windows, Kali, Ubuntu,
a generic EFI tool, and the UEFI fallback loader, then deletes the temporary
directory. The fallback is shown as recovery and is unchecked by default. The
test never invokes PolicyKit and never touches the real ESP.

The same script builds a separate, non-installed transaction-test helper. That
binary is compile-time restricted to one explicitly declared temporary ESP
under `/tmp`. It exercises installation and reinstall, then injects a simulated
firmware boot-order failure and verifies automatic restoration of files, boot
order, and any newly created APEX32 entry. The normal source-built helper
remains compiled with `INSTALL|0`.

## 4. Inspect the safe GUI demo

Build the two scan-only installer binaries as the normal user:

```bash
./Tools/build-installer.sh
```

The final capability output must be:

```text
APEX32CAPS|1
SCAN|1
INSTALL|0
RESTORE|0
TERMINAL_AUTH|0
```

Launch the mock-ESP demo:

```bash
./Tools/run-installer-demo.sh
```

The window must show a cyan safe-test banner, mock operating systems, an
unchecked **UEFI FALLBACK (RECOVERY)** row, and a disabled hardware-install
button. Close the window after inspection.

## 5. Perform the authorized read-only ESP scan

Confirm the graphical authorization agent is running. Hyprland users can use:

```bash
systemctl --user is-active hyprpolkitagent.service
```

Launch the regular-user GUI:

```bash
./Installer/Linux/build/apex32-installer
```

Then:

1. Press **Scan Now**.
2. Approve the graphical PolicyKit dialog.
3. Confirm that the expected operating-system EFI loaders appear.
4. Confirm that `\EFI\APEX32\Apex32BootManager.efi` is not offered as an OS.
5. Confirm the generic fallback is marked recovery and unchecked.
6. Open **Install** and confirm hardware installation is disabled.
7. Take a screenshot for the test report, then close the installer.

The GUI stays unprivileged. PolicyKit starts the fixed helper only for the
read-only `scan` request. The helper emits a bounded list of EFI loader paths;
it does not write files or change NVRAM during this operation.

## Troubleshooting

### Terminal password prompt or `No session for cookie`

The graphical PolicyKit agent is missing or inactive. Cancel the prompt and
check:

```bash
systemctl --user --no-pager --full status hyprpolkitagent.service || true
pgrep -af hyprpolkitagent || true
```

Do not keep retrying a terminal password prompt.

### `pkexec` is missing

Check with:

```bash
command -v pkexec
```

Install the distribution's PolicyKit/`pkexec` package before continuing.

### ESP not detected

Confirm that an ESP is mounted at a conventional location:

```bash
findmnt /boot/efi || findmnt /efi
```

The current firmware loads targets from the same ESP as APEX32. Multi-ESP
discovery is a later release gate.

## Cleanup

After testing, close the GUI. The timestamped fresh clone may be moved to the
desktop trash after its path is checked. Never remove or overwrite a personal
APEX32 source tree.

## Privileged installation phase

The default source build rejects the helper's `install` operation even when
invoked directly. Do not bypass the compile-time gate. Only the package builder
may enable installation, and the resulting candidate remains unreleased until
live-hardware restore, multi-ESP, Secure Boot, recovery-media, and signing gates
pass.

## End-user packages and Windows

These commands are contributor tests, not the intended customer experience.
The Linux beta will be launched from a desktop icon and will use only the
desktop PolicyKit dialog. The planned Windows beta will be a signed MSI or EXE
that uses the standard UAC consent dialog. Neither packaged flow will require a
terminal. Current Windows users should not attempt to install this alpha from
source; a Windows installer has not been released.

## Debian package candidate

After the real firmware has been built, contributors can create the candidate
without root:

```bash
./Tools/build-linux-package.sh

PACKAGE="$(find Installer/Linux/package-build/packages -name '*.deb' -print -quit)"
./Tools/test-linux-package.sh \
  "$PACKAGE" \
  Build/DEBUG_GCC/X64/Apex32BootManager.efi
```

The package test extracts the `.deb` into a temporary directory. It does not
install anything, invoke PolicyKit, touch the real ESP, or change NVRAM. It
requires the packaged GUI to report `INSTALL|1`, `RESTORE|1`, and
`TERMINAL_AUTH|0`, and byte-compares the packaged firmware with the verified
EDK II artifact.
