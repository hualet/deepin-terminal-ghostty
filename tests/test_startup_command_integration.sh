#!/usr/bin/env bash

set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "${script_dir}/.." && pwd)
app_bin="${repo_root}/build/deepin-terminal-ghostty"

if [[ ! -x "${app_bin}" ]]; then
    echo "Missing terminal binary: ${app_bin}" >&2
    exit 1
fi

tmp_dir=$(mktemp -d)
trap 'rm -rf "${tmp_dir}"' EXIT

status=0
DISPLAY= QT_QPA_PLATFORM=offscreen "${app_bin}" \
    --working-directory "${tmp_dir}" \
    --execute "pwd > startup-pwd.txt; exit 23" \
    --propagate-exit-code || status=$?

if [[ "${status}" -ne 23 ]]; then
    echo "Expected exit code 23, got ${status}" >&2
    exit 1
fi

pwd_file="${tmp_dir}/startup-pwd.txt"
if [[ ! -f "${pwd_file}" ]]; then
    echo "Expected working-directory probe file: ${pwd_file}" >&2
    exit 1
fi

actual_pwd=$(tr -d '\r\n' < "${pwd_file}")
if [[ "${actual_pwd}" != "${tmp_dir}" ]]; then
    echo "Expected command to run in ${tmp_dir}, got ${actual_pwd}" >&2
    exit 1
fi
