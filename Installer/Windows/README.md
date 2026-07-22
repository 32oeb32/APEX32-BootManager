# Windows one-launch installer foundation

The Windows package is one APEX32 product, not a separate boot manager. It
uses shared branding and will package the same verified firmware while
providing the native Windows authorization and packaging layer that Linux
cannot supply.

The current Windows preview produces one `APEX32-Community-Setup.exe`. After
installation, **Scan Systems** requests the standard UAC dialog, temporarily
mounts the EFI System Partition with the Windows-provided `mountvol` tool,
discovers EFI applications, unmounts the partition, and displays the results.
The user never selects a drive letter or types a boot command.

PR #7 adds a reusable schema 1 file/configuration transaction with immutable
backup, failure injection, rollback, reinstall, and restore tests on a
disposable Windows runner. The injected firmware store is a JSON file inside
the test sandbox; it cannot modify BCD, UEFI variables, or the host ESP.

`Install and Make Default` and `Restore Previous Boot State` intentionally
report `INSTALL|0` / `RESTORE|0` and remain disabled. Microsoft documents that
incorrect boot-configuration changes can make a computer unbootable. The
controls will only be enabled after a native firmware backend, packaged
verified EFI payload, disposable UEFI-VM lifecycle, and real-hardware
validation satisfy the same rollback contract. See
[Windows transaction foundation](../../Docs/WINDOWS_TRANSACTION_FOUNDATION.md).

Authoritative Windows references:

- [Mountvol](https://learn.microsoft.com/windows-server/administration/windows-commands/mountvol)
- [BCDEdit](https://learn.microsoft.com/windows-server/administration/windows-commands/bcdedit)
- [SetFirmwareEnvironmentVariableExW](https://learn.microsoft.com/windows/win32/api/winbase/nf-winbase-setfirmwareenvironmentvariableexw)

## VS Code and contributor build

Open the complete `APEX32-BootManager` repository folder in VS Code. Do not
open only `Installer/Windows`, because the installer packages shared project
files such as `LICENSE` and `DISCLAIMER.md`.

On a Windows x86_64 development host with Qt 6 for MSVC 2022, Visual Studio
2022 Build Tools, CMake, and NSIS, run these commands from the repository root:

```powershell
cmake -S Installer/Windows -B Installer/Windows/build `
  -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build Installer/Windows/build --config Release --parallel
cpack --config Installer/Windows/build/CPackConfig.cmake -C Release
```

The Windows GUI source is `Installer/Windows/Gui/main.cpp`. The Windows build
definition is `Installer/Windows/CMakeLists.txt`; the UEFI boot manager itself
is still built from the repository-level `Apex32BootManager.dsc`.

Release users will not run these commands. They will download and double-click
the generated setup executable from GitHub or apex32-secure.com.
