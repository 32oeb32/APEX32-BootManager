# Installer testing

The Community installer is tested in two deliberately separate phases. The
first phase is safe for a contributor's normal desktop session. The second
phase changes firmware state and belongs in QEMU/OVMF before hardware.

## Regular-user phase

The regular-user test:

- refuses to run as root;
- builds the Qt GUI and privileged helper;
- creates a temporary mock EFI System Partition;
- discovers Windows, Kali, Ubuntu, and a generic EFI tool;
- verifies that shim is preferred over GRUB for the same system;
- verifies that APEX32 never discovers itself as a boot target;
- generates schema 1 configuration in memory;
- confirms that the helper refuses a direct unprivileged install; and
- confirms that the mock ESP is unchanged.

On Debian and Kali, contributors install the build dependencies once:

```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev
```

Run the automated test **without sudo**:

```bash
./Tools/test-installer-user.sh
```

To inspect the GUI safely, first build it and then launch the demo:

```bash
./Tools/build-installer.sh
./Tools/run-installer-demo.sh
```

The demo displays a prominent safe-test banner and disables installation. It
does not invoke polkit, access the real ESP, or change UEFI variables.

These are contributor QA commands. The public beta package must expose a
desktop launcher and require no terminal commands from end users.

## Privileged phase

The helper's `scan` operation is read-only and may be exercised against a
root-only ESP through the GUI's **Scan Now** action. Do not exercise the alpha
helper's `install` operation against a daily-use ESP. Transaction rollback,
restore/uninstall operations, and QEMU/OVMF integration tests remain required
before hardware installation is offered.
