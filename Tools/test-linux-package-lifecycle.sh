#!/usr/bin/env bash

set -euo pipefail

if [[ "${CI:-}" != "true" || "${GITHUB_ACTIONS:-}" != "true" ]]; then
  echo "FAIL: package lifecycle testing is confined to a disposable GitHub Actions runner" >&2
  exit 1
fi
if [[ ${EUID} -eq 0 ]]; then
  echo "FAIL: launch the lifecycle test as the regular CI user" >&2
  exit 1
fi
if [[ $# -ne 2 || ! -f "$1" || ! -f "$2" ]]; then
  echo "usage: Tools/test-linux-package-lifecycle.sh PACKAGE.deb EXPECTED_FIRMWARE.efi" >&2
  exit 2
fi

for program in \
  apt-get appstreamcli awk cmp desktop-file-validate dpkg-deb dpkg-query \
  mountpoint realpath sha256sum stat sudo; do
  command -v "${program}" >/dev/null 2>&1 || {
    echo "FAIL: missing lifecycle dependency: ${program}" >&2
    exit 2
  }
done

package="$(realpath "$1")"
expected_firmware="$(realpath "$2")"
package_name="$(dpkg-deb --field "${package}" Package)"
[[ "${package_name}" == "apex32-boot-manager" ]] || {
  echo "FAIL: unexpected package name: ${package_name}" >&2
  exit 3
}

if mountpoint -q /boot/efi 2>/dev/null ||
   mountpoint -q /sys/firmware/efi/efivars 2>/dev/null ||
   [[ -e /boot/efi/EFI/APEX32 ]]; then
  echo "FAIL: lifecycle test refuses a host with a real ESP or EFI variables" >&2
  exit 3
fi

cleanup() {
  sudo env DEBIAN_FRONTEND=noninteractive \
    apt-get purge --yes "${package_name}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

sudo env DEBIAN_FRONTEND=noninteractive \
  apt-get install --yes "${package}"

gui=/usr/bin/apex32-installer
helper=/usr/libexec/apex32/apex32-installer-helper
firmware=/usr/share/apex32/Apex32BootManager.efi
desktop=/usr/share/applications/apex32-installer.desktop
policy=/usr/share/polkit-1/actions/org.apex32secure.installer.policy
metainfo=/usr/share/metainfo/com.apex32secure.APEX32Installer.metainfo.xml
icon=/usr/share/icons/hicolor/scalable/apps/apex32-installer.svg

for path in \
  "${gui}" "${helper}" "${firmware}" "${desktop}" \
  "${policy}" "${metainfo}" "${icon}"; do
  [[ -f "${path}" ]] || {
    echo "FAIL: installed package is missing ${path}" >&2
    exit 4
  }
done

[[ "$(stat -c '%U:%G:%a' "${helper}")" == "root:root:755" ]]
[[ "$(stat -c '%U:%G:%a' "${firmware}")" == "root:root:644" ]]
cmp "${expected_firmware}" "${firmware}"
desktop-file-validate "${desktop}"
appstreamcli validate --no-net "${metainfo}"

expected_capabilities=$'APEX32CAPS|1\nSCAN|1\nINSTALL|1\nRESTORE|1\nTERMINAL_AUTH|0'
[[ "$("${gui}" --capabilities)" == "${expected_capabilities}" ]]
[[ "$(dpkg-query -W -f='${db:Status-Abbrev}' "${package_name}")" == "ii " ]]
[[ ! -e /boot/efi/EFI/APEX32 ]]

firmware_hash="$(sha256sum "${firmware}" | awk '{print $1}')"
sudo env DEBIAN_FRONTEND=noninteractive \
  apt-get install --reinstall --yes "${package}"
[[ "$(sha256sum "${firmware}" | awk '{print $1}')" == "${firmware_hash}" ]]
[[ "$("${gui}" --capabilities)" == "${expected_capabilities}" ]]
[[ ! -e /boot/efi/EFI/APEX32 ]]

sudo env DEBIAN_FRONTEND=noninteractive \
  apt-get purge --yes "${package_name}"
trap - EXIT

if dpkg-query -W "${package_name}" >/dev/null 2>&1; then
  echo "FAIL: package remains registered after purge" >&2
  exit 5
fi
for path in \
  "${gui}" "${helper}" "${firmware}" "${desktop}" \
  "${policy}" "${metainfo}" "${icon}"; do
  [[ ! -e "${path}" ]] || {
    echo "FAIL: purge left packaged path behind: ${path}" >&2
    exit 5
  }
done
[[ ! -e /boot/efi/EFI/APEX32 ]]

echo "PASS: disposable runner installed the Debian package with protected ownership and complete desktop metadata"
echo "PASS: package reinstall preserved the verified firmware and install/restore capability mode"
echo "PASS: package purge removed every packaged path without touching an ESP or UEFI variables"
