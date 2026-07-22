# Windows one-launch installer

The Windows package is the native installer for the same APEX32 firmware used
on Linux. The release workflow produces one `APEX32-Community-Setup.exe` with
the verified EFI application, Qt runtime, graphical scanner, transactional
installation, make-default operation, and graphical restore.

**Scan Systems** requests standard UAC approval, mounts the EFI System
Partition temporarily with `mountvol`, discovers EFI applications, and removes
the temporary mount. The elevated window then keeps the selected systems and
can complete installation without opening PowerShell or Command Prompt.

Ordinary source builds remain scan-only. Only builds configured with both
`APEX32_ENABLE_HARDWARE_INSTALL=ON` and a verified `APEX32_FIRMWARE` report
`INSTALL|1` and `RESTORE|1`. The GUI validates the firmware SHA-256 before
allowing a transaction.

The native backend reads and writes UEFI `Boot####` and `BootOrder` through
`GetFirmwareEnvironmentVariableExW` and `SetFirmwareEnvironmentVariableExW`.
It is covered by fake-variable Windows tests and a separate disposable OVMF
lifecycle, so CI never mutates the runner's firmware. See
[Windows graphical installation and recovery](../../Docs/WINDOWS_PRODUCTION_INSTALLER.md).

## VS Code and contributor build

Open the complete `APEX32-BootManager` repository folder in VS Code. Do not
open only `Installer/Windows`, because the installer packages the shared
firmware and project licensing files.

Scan-only contributor build:

```powershell
cmake -S Installer/Windows -B Installer/Windows/build `
  -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build Installer/Windows/build --config Release --parallel
ctest --test-dir Installer/Windows/build -C Release --output-on-failure
```

The CI workflow is the reference for the hardware-enabled package because it
first builds the pinned EDK II firmware and transfers that exact artifact into
the Windows job. Release users only download and double-click the resulting
setup executable.

Authoritative Windows references:

- [Mountvol](https://learn.microsoft.com/windows-server/administration/windows-commands/mountvol)
- [GetFirmwareEnvironmentVariableExW](https://learn.microsoft.com/windows/win32/api/winbase/nf-winbase-getfirmwareenvironmentvariableexw)
- [SetFirmwareEnvironmentVariableExW](https://learn.microsoft.com/windows/win32/api/winbase/nf-winbase-setfirmwareenvironmentvariableexw)
