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

help_output=$("${app_bin}" -platform offscreen --help)
if [[ "${help_output}" != *"Usage:"* || "${help_output}" != *"--execute"* ]]; then
    echo "Expected -platform offscreen --help to reach application help output" >&2
    exit 1
fi

status=0
DISPLAY= QT_QPA_PLATFORM=offscreen "${app_bin}" \
    --working-directory "${tmp_dir}" \
    --trace-vt "${tmp_dir}/startup-vt.log" \
    --execute "printf trace-visible; pwd > startup-pwd.txt; exit 23" \
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

trace_file="${tmp_dir}/startup-vt.log"
if [[ ! -f "${trace_file}" ]]; then
    echo "Expected VT trace file: ${trace_file}" >&2
    exit 1
fi
if ! grep -q "trace.enabled" "${trace_file}" || ! grep -q "pty.start" "${trace_file}" || ! grep -q "pty.read" "${trace_file}"; then
    echo "Expected VT trace to include startup and PTY data events" >&2
    cat "${trace_file}" >&2
    exit 1
fi
