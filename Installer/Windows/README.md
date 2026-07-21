# Windows one-launch installer foundation

The Windows package is one APEX32 product, not a separate boot manager. It
uses the shared branding and firmware while providing the native Windows
authorization and packaging layer that Linux cannot supply.

The current Windows preview produces one `APEX32-Community-Setup.exe`. After
installation, **Scan Systems** requests the standard UAC dialog, temporarily
mounts the EFI System Partition with the Windows-provided `mountvol` tool,
discovers EFI applications, unmounts the partition, and displays the results.
The user never selects a drive letter or types a boot command.

`Install and Make Default` intentionally reports `INSTALL|0` and remains
disabled. Microsoft documents that modifying BCD or firmware variables needs
administrator privileges and that incorrect BCD changes can make a computer
unbootable. It will only be enabled after the native transaction has backup,
rollback, disposable-VM, and real-hardware validation equivalent to Linux.

Authoritative Windows references:

- [Mountvol](https://learn.microsoft.com/windows-server/administration/windows-commands/mountvol)
- [BCDEdit](https://learn.microsoft.com/windows-server/administration/windows-commands/bcdedit)
- [SetFirmwareEnvironmentVariableExW](https://learn.microsoft.com/windows/win32/api/winbase/nf-winbase-setfirmwareenvironmentvariableexw)

## Contributor build

On a Windows x86_64 development host with Qt 6, CMake, Ninja, and NSIS:

```powershell
cmake -S Installer/Windows -B Installer/Windows/build -G Ninja
cmake --build Installer/Windows/build --parallel
cpack --config Installer/Windows/build/CPackConfig.cmake
```

Release users will not run these commands. They will download and double-click
the generated setup executable from GitHub or apex32-secure.com.
