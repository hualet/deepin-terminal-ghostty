#!/usr/bin/env bash

set -euo pipefail

binary="${1:-./build/deepin-terminal-ghostty}"
log_dir="${2:-/tmp}"

if [[ ! -x "${binary}" ]]; then
    echo "error: binary not found or not executable: ${binary}" >&2
    exit 1
fi

mkdir -p "${log_dir}"

timestamp="$(date +%Y%m%d-%H%M%S)"
log_file="${log_dir%/}/$(basename "${binary}")-gdb-${timestamp}.log"

echo "Starting ${binary} under gdb."
echo "Reproduce the crash in the app window."
echo "Crash log will be written to: ${log_file}"

gdb -batch \
    -ex "set pagination off" \
    -ex "set debuginfod enabled off" \
    -ex "set confirm off" \
    -ex "set logging file ${log_file}" \
    -ex "set logging overwrite on" \
    -ex "set logging enabled on" \
    -ex "run" \
    -ex "thread apply all bt full" \
    -ex "info registers" \
    --args "${binary}"

echo "gdb finished. Log saved to: ${log_file}"
