#!/usr/bin/env bash

set -euo pipefail

if [[ ${EUID} -eq 0 ]]; then
  echo "FAIL: test the package as a regular user, never with sudo" >&2
  exit 1
fi
if [[ $# -lt 1 || $# -gt 2 || ! -f "$1" ]]; then
  echo "usage: Tools/test-linux-package.sh PACKAGE.deb [EXPECTED_FIRMWARE.efi]" >&2
  exit 2
fi

package="$(realpath "$1")"
expected_firmware="${2:-}"
for program in cmp dpkg-deb find grep od realpath stat tr; do
  command -v "${program}" >/dev/null 2>&1 || {
    echo "FAIL: missing package test dependency: ${program}" >&2
    exit 2
  }
done

sandbox="$(mktemp -d -t apex32-package-test-XXXXXX)"
trap 'rm -rf "${sandbox}"' EXIT
root="${sandbox}/root"
mkdir -p "${root}"
dpkg-deb --extract "${package}" "${root}"

gui="${root}/usr/bin/apex32-installer"
helper="${root}/usr/libexec/apex32/apex32-installer-helper"
firmware="${root}/usr/share/apex32/Apex32BootManager.efi"
desktop="${root}/usr/share/applications/apex32-installer.desktop"
policy="${root}/usr/share/polkit-1/actions/org.apex32secure.installer.policy"
icon="${root}/usr/share/icons/hicolor/scalable/apps/apex32-installer.svg"

for required in \
  "${gui}" \
  "${helper}" \
  "${firmware}" \
  "${desktop}" \
  "${policy}" \
  "${icon}" \
  "${root}/usr/share/doc/apex32-boot-manager/LICENSE" \
  "${root}/usr/share/doc/apex32-boot-manager/DISCLAIMER.md"; do
  [[ -f "${required}" ]] || {
    echo "FAIL: package is missing ${required#${root}}" >&2
    exit 3
  }
done

[[ -x "${gui}" && -x "${helper}" ]] || {
  echo "FAIL: packaged executables are not executable" >&2
  exit 3
}
[[ "$(stat -c '%a' "${helper}")" == "755" ]] || {
  echo "FAIL: helper mode is not 0755" >&2
  exit 3
}
[[ "$(stat -c '%s' "${firmware}")" -ge 4096 ]] || {
  echo "FAIL: packaged firmware is unexpectedly small" >&2
  exit 3
}
[[ "$(od -An -t x1 -N2 "${firmware}" | tr -d ' \n')" == "4d5a" ]] || {
  echo "FAIL: packaged firmware is not PE/COFF" >&2
  exit 3
}
if [[ -n "${expected_firmware}" ]] && ! cmp -s "${expected_firmware}" "${firmware}"; then
  echo "FAIL: packaged firmware differs from the verified build artifact" >&2
  exit 3
fi

expected_capabilities=$'APEX32CAPS|1\nSCAN|1\nINSTALL|1\nRESTORE|1\nTERMINAL_AUTH|0'
actual_capabilities="$("${gui}" --capabilities)"
[[ "${actual_capabilities}" == "${expected_capabilities}" ]] || {
  echo "FAIL: packaged GUI has incorrect capability gates" >&2
  printf '%s\n' "${actual_capabilities}" >&2
  exit 4
}

grep -qx 'Exec=apex32-installer' "${desktop}"
grep -qx 'Icon=apex32-installer' "${desktop}"
grep -qx 'Terminal=false' "${desktop}"
grep -q '/usr/libexec/apex32/apex32-installer-helper' "${policy}"
grep -q 'org.freedesktop.policykit.exec.allow_gui' "${policy}"

depends="$(dpkg-deb --field "${package}" Depends)"
for dependency in efibootmgr pkexec util-linux; do
  [[ "${depends}" == *"${dependency}"* ]] || {
    echo "FAIL: package dependency is missing: ${dependency}" >&2
    exit 5
  }
done

echo "PASS: Debian package contains GUI, helper, policy, icon, firmware, and recovery documentation"
echo "PASS: packaged GUI reports INSTALL|1, RESTORE|1, and terminal authorization disabled"
echo "PASS: package metadata declares the required EFI and graphical authorization tools"
