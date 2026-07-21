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
  find grep mountpoint realpath sha256sum sort stat sudo tar; do
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

control_listing="$(dpkg-deb --ctrl-tarfile "${package}" | tar -tf -)"
for maintainer_script in preinst postinst prerm postrm; do
  if grep -Fqx "./${maintainer_script}" <<<"${control_listing}" ||
     grep -Fqx "${maintainer_script}" <<<"${control_listing}"; then
    echo "FAIL: package contains maintainer script: ${maintainer_script}" >&2
    exit 3
  fi
done

if [[ -e /boot/efi/EFI/APEX32 ]]; then
  echo "FAIL: lifecycle test refuses a host containing /boot/efi/EFI/APEX32" >&2
  exit 3
fi

snapshot_esp() {
  local esp_path=/boot/efi

  if mountpoint -q "${esp_path}" 2>/dev/null; then
    {
      sudo find "${esp_path}" -xdev \
        -printf 'META|%y|%m|%U|%G|%s|%p|%l\n'
      sudo find "${esp_path}" -xdev -type f \
        -exec sha256sum {} +
    } | LC_ALL=C sort
  fi
}

snapshot_efivars() {
  local efivar_path=/sys/firmware/efi/efivars

  if mountpoint -q "${efivar_path}" 2>/dev/null; then
    sudo find "${efivar_path}" -maxdepth 1 -type f \
      -exec sha256sum {} + | LC_ALL=C sort
  fi
}

esp_snapshot_before="$(snapshot_esp)"
efivar_snapshot_before="$(snapshot_efivars)"

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

esp_snapshot_after="$(snapshot_esp)"
if [[ "${esp_snapshot_after}" != "${esp_snapshot_before}" ]]; then
  echo "FAIL: EFI System Partition changed during package lifecycle testing" >&2
  exit 6
fi

efivar_snapshot_after="$(snapshot_efivars)"
if [[ "${efivar_snapshot_after}" != "${efivar_snapshot_before}" ]]; then
  echo "FAIL: EFI variables changed during package lifecycle testing" >&2
  exit 6
fi

trap - EXIT

echo "PASS: disposable runner installed the Debian package with protected ownership and complete desktop metadata"
echo "PASS: package reinstall preserved the verified firmware and install/restore capability mode"
echo "PASS: package has no maintainer scripts and purge removed every packaged path"
echo "PASS: package lifecycle left the complete ESP and EFI-variable snapshots unchanged"
