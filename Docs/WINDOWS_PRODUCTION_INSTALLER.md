# Windows graphical installation and recovery

The CI-built Windows qualification package contains the same verified
`Apex32BootManager.efi` artifact used by the Linux package. Release users
will eventually download one `APEX32-Community-Setup.exe`, install it normally,
and complete the entire boot-manager workflow in the GUI. The current artifact
is not public-release qualified.

## User flow

1. Open **APEX32 Community Installer**.
2. Select **Scan Systems** and approve the standard Windows UAC prompt.
3. Review every discovered EFI loader and uncheck any card that should not be
   displayed.
4. Select **Install APEX32 and Make Default**.
5. Restart when the installer reports that files, `Boot####`, and `BootOrder`
   were verified.
6. Use **Restore Previous Boot State** to restore the exact pre-install files
   and boot order.

No drive letter, PowerShell command, BCDEdit command, or manual EFI copy is
shown to the user. `mountvol` assigns a temporary private drive letter and the
installer always removes that mount when the operation ends.

## Firmware-variable backend

`WindowsFirmwareStore` implements the existing `FirmwareStore` transaction
contract through the Windows firmware environment APIs. It does not parse
localized BCDEdit output.

- The process enables only `SE_SYSTEM_ENVIRONMENT_NAME` after UAC elevation.
- `BootOrder` and the selected `Boot####` load option are captured byte for
  byte before mutation.
- The Microsoft boot option anchors the identity of the Windows system ESP
  mounted by `mountvol /S`; installation fails closed when that identity is
  unavailable.
- An existing APEX32 entry is reused only when its GPT hard-drive device-path
  node identifies that same ESP. A same-named entry on a Linux or recovery ESP
  is unrelated and remains byte-for-byte untouched.
- A new Windows-side entry is derived from the Microsoft option's device path
  and receives only the APEX32 file-path node and description.
- Promotion removes duplicate numbers from `BootOrder`, places APEX32 first,
  and preserves every other entry in its previous order.
- Restore writes back the exact saved option and order, or deletes the entry
  when the installer originally created it.
- Every operation is read back and verified. Failure invokes the shared file
  and firmware rollback path.

The native store depends on a narrow `FirmwareVariableAccess` interface. CI
uses an in-memory implementation and never calls Windows firmware APIs on the
host runner.

## Firmware identity and Secure Boot

Hardware capability is default-off in source builds. CMake enables it only
when `APEX32_ENABLE_HARDWARE_INSTALL=ON` and `APEX32_FIRMWARE` names an existing,
bounded PE/COFF application. Its SHA-256 is compiled into the GUI, the exact
file is installed under `share/apex32`, and the GUI verifies the hash again
before enabling installation.

The Community beta firmware is not signed for Secure Boot. When the UEFI
`SecureBoot` variable is enabled, installation fails closed before any file or
boot-order write. Restore remains available so a previous beta installation
can always be removed. A signed Secure Boot release remains a final release
gate.

## Isolated qualification

The Windows workflow performs all of the following without touching runner
firmware:

- builds the real EFI application from the pinned EDK II revision on Linux;
- transfers that exact artifact into the isolated Windows package job;
- runs file transaction install, reinstall, injected failure, rollback, and
  restore tests inside `QTemporaryDir`;
- runs the native `Boot####` implementation against fake variables, including
  same-ESP entry reuse, two-ESP isolation, exact restore, and a failed
  `BootOrder` commit;
- verifies packaged and source firmware SHA-256 values are identical;
- installs and removes the NSIS setup silently in a temporary directory; and
- confirms that neither test executable is included in the package.

The OVMF workflow separately creates a private boot option, promotes it,
restores the exact original `BootOrder`, removes the option, and requires a
firmware-level success exit. That synthetic gate does not boot Microsoft
Windows or validate the packaged GUI in Windows firmware. A snapshot-backed
Windows UEFI VM must complete scan, install, make-default, reboot, handoff, and
restore before a real Windows machine is used. Both are mandatory before
publication. In particular, the first physical Windows run must confirm that
an APEX32 entry already present on a separate Linux ESP is not reused for the
Windows ESP transaction.

## Contributor build

Open the repository root in VS Code. A source-only build remains scan-only:

```powershell
cmake -S Installer/Windows -B Installer/Windows/build `
  -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
```

A hardware-enabled package additionally requires a verified firmware artifact:

```powershell
cmake -S Installer/Windows -B Installer/Windows/build `
  -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64" `
  -DAPEX32_ENABLE_HARDWARE_INSTALL=ON `
  -DAPEX32_FIRMWARE="C:/verified/Apex32BootManager.efi"
cmake --build Installer/Windows/build --config Release --parallel
ctest --test-dir Installer/Windows/build -C Release --output-on-failure
cpack --config Installer/Windows/build/CPackConfig.cmake -C Release
```

Release users never run these commands.
