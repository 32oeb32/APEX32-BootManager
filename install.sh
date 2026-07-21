#!/usr/bin/env bash

set -euo pipefail

readonly RELEASE_BASE_URL="${APEX32_RELEASE_BASE_URL:-https://github.com/32oeb32/APEX32-BootManager/releases/latest/download}"
readonly PACKAGE_NAME="apex32-boot-manager_amd64.deb"
readonly CHECKSUM_NAME="${PACKAGE_NAME}.sha256"
readonly TEST_MODE="${APEX32_TEST_MODE:-0}"

fail() {
  printf 'APEX32 installation failed: %s\n' "$*" >&2
  exit 1
}

if [[ ${EUID} -eq 0 && "${TEST_MODE}" != "1" ]]; then
  fail "run ./install.sh as your normal desktop user, never with sudo"
fi

[[ "$(uname -s)" == "Linux" ]] || fail "this launcher supports Linux only"
case "$(uname -m)" in
  x86_64|amd64) ;;
  *) fail "the current release supports x86_64 computers only" ;;
esac

if [[ "${TEST_MODE}" != "1" ]]; then
  [[ -d /sys/firmware/efi ]] || fail "the computer is not running in UEFI mode"
  [[ -r /etc/debian_version ]] ||
    fail "the current release supports Debian, Ubuntu, Kali, and compatible systems"
  if command -v mokutil >/dev/null 2>&1; then
    secure_boot_state="$(mokutil --sb-state 2>/dev/null || true)"
    if [[ "${secure_boot_state,,}" == *"secureboot enabled"* ]]; then
      fail "Secure Boot is enabled; use a signed APEX32 release before installation"
    fi
  fi
fi

for program in curl sha256sum mktemp; do
  command -v "${program}" >/dev/null 2>&1 ||
    fail "required system program is missing: ${program}"
done

pkexec_path="/usr/bin/pkexec"
apt_get_path="/usr/bin/apt-get"
installer_path="/usr/bin/apex32-installer"
if [[ "${TEST_MODE}" == "1" ]]; then
  pkexec_path="${APEX32_PKEXEC:-${pkexec_path}}"
  apt_get_path="${APEX32_APT_GET:-${apt_get_path}}"
  installer_path="${APEX32_INSTALLER_BINARY:-${installer_path}}"
fi

[[ -x "${pkexec_path}" ]] ||
  fail "graphical administrator authorization is unavailable (pkexec missing)"
[[ -x "${apt_get_path}" ]] || fail "the Debian package manager is unavailable"

download_dir="$(mktemp -d -t apex32-install-XXXXXX)"
trap 'rm -rf "${download_dir}"' EXIT
package_path="${download_dir}/${PACKAGE_NAME}"
checksum_path="${download_dir}/${CHECKSUM_NAME}"

printf 'Downloading the verified APEX32 Community package...\n'
curl \
  --fail \
  --location \
  --proto '=https' \
  --silent \
  --show-error \
  --output "${package_path}" \
  "${RELEASE_BASE_URL}/${PACKAGE_NAME}" ||
  fail "the Linux release package could not be downloaded"
curl \
  --fail \
  --location \
  --proto '=https' \
  --silent \
  --show-error \
  --output "${checksum_path}" \
  "${RELEASE_BASE_URL}/${CHECKSUM_NAME}" ||
  fail "the release checksum could not be downloaded"

read -r expected_hash _ < "${checksum_path}" ||
  fail "the release checksum is unreadable"
[[ "${expected_hash}" =~ ^[[:xdigit:]]{64}$ ]] ||
  fail "the release checksum has an invalid format"
actual_hash_line="$(sha256sum "${package_path}")"
actual_hash="${actual_hash_line%% *}"
[[ "${actual_hash,,}" == "${expected_hash,,}" ]] ||
  fail "the downloaded package failed verification and was not installed"

printf 'Package verified. Approve the graphical administrator dialog to install it.\n'
"${pkexec_path}" \
  --disable-internal-agent \
  "${apt_get_path}" \
  install \
  --yes \
  "${package_path}" ||
  fail "installation was cancelled or the package manager rejected the package"

[[ -x "${installer_path}" ]] ||
  fail "the package installed without the APEX32 graphical application"

printf 'Opening APEX32 Community Installer...\n'
"${installer_path}"
