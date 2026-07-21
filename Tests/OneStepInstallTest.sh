#!/usr/bin/env bash

set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
sandbox="$(mktemp -d -t apex32-one-step-test-XXXXXX)"
trap 'rm -rf "${sandbox}"' EXIT

fixture_dir="${sandbox}/release"
stub_dir="${sandbox}/bin"
mkdir -p "${fixture_dir}" "${stub_dir}"
package_name="apex32-boot-manager_amd64.deb"
printf 'APEX32 test package\n' > "${fixture_dir}/${package_name}"
(
  cd "${fixture_dir}"
  sha256sum "${package_name}" > "${package_name}.sha256"
)

cat > "${stub_dir}/curl" <<'STUB'
#!/usr/bin/env bash
set -euo pipefail
output=""
url=""
while (($#)); do
  case "$1" in
    --output)
      output="$2"
      shift 2
      ;;
    --fail|--location|--silent|--show-error)
      shift
      ;;
    --proto)
      shift 2
      ;;
    *)
      url="$1"
      shift
      ;;
  esac
done
cp "${APEX32_TEST_RELEASE_DIR}/${url##*/}" "${output}"
STUB

cat > "${stub_dir}/pkexec" <<'STUB'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' "$@" > "${APEX32_TEST_PKEXEC_LOG}"
STUB

cat > "${stub_dir}/apt-get" <<'STUB'
#!/usr/bin/env bash
exit 99
STUB

cat > "${stub_dir}/apex32-installer" <<'STUB'
#!/usr/bin/env bash
set -euo pipefail
printf 'launched\n' > "${APEX32_TEST_LAUNCH_LOG}"
STUB

chmod +x "${stub_dir}"/*
pkexec_log="${sandbox}/pkexec.log"
launch_log="${sandbox}/launch.log"

PATH="${stub_dir}:${PATH}" \
APEX32_TEST_MODE=1 \
APEX32_TEST_RELEASE_DIR="${fixture_dir}" \
APEX32_TEST_PKEXEC_LOG="${pkexec_log}" \
APEX32_TEST_LAUNCH_LOG="${launch_log}" \
APEX32_RELEASE_BASE_URL="https://release.test.invalid" \
APEX32_PKEXEC="${stub_dir}/pkexec" \
APEX32_APT_GET="${stub_dir}/apt-get" \
APEX32_INSTALLER_BINARY="${stub_dir}/apex32-installer" \
  "${project_root}/install.sh" >/dev/null

grep -qx -- '--disable-internal-agent' "${pkexec_log}"
grep -qx -- "${stub_dir}/apt-get" "${pkexec_log}"
grep -qx -- 'install' "${pkexec_log}"
grep -qx -- '--yes' "${pkexec_log}"
grep -q "/${package_name}$" "${pkexec_log}"
grep -qx 'launched' "${launch_log}"

printf '0%.0s' {1..64} > "${fixture_dir}/${package_name}.sha256"
printf '  %s\n' "${package_name}" >> "${fixture_dir}/${package_name}.sha256"
rm -f "${pkexec_log}" "${launch_log}"
if PATH="${stub_dir}:${PATH}" \
  APEX32_TEST_MODE=1 \
  APEX32_TEST_RELEASE_DIR="${fixture_dir}" \
  APEX32_TEST_PKEXEC_LOG="${pkexec_log}" \
  APEX32_TEST_LAUNCH_LOG="${launch_log}" \
  APEX32_RELEASE_BASE_URL="https://release.test.invalid" \
  APEX32_PKEXEC="${stub_dir}/pkexec" \
  APEX32_APT_GET="${stub_dir}/apt-get" \
  APEX32_INSTALLER_BINARY="${stub_dir}/apex32-installer" \
    "${project_root}/install.sh" >/dev/null 2>&1; then
  echo "FAIL: one-step installer accepted a corrupt package" >&2
  exit 1
fi
[[ ! -e "${pkexec_log}" && ! -e "${launch_log}" ]]

echo "PASS: one-command installer verified the package, required graphical authorization, and launched the GUI"
echo "PASS: checksum failure stopped before package installation"
