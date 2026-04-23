#!/usr/bin/env bash

set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "${script_dir}/.." && pwd)
wrapper="${repo_root}/tests/run_esctest.sh"

if [[ ! -x "${wrapper}" ]]; then
    echo "Missing esctest wrapper: ${wrapper}" >&2
    exit 1
fi

fake_dir=$(mktemp -d)
trap 'rm -rf "${fake_dir}"' EXIT

cat > "${fake_dir}/esctest.py" <<'PY'
import os
import pathlib
import sys

pathlib.Path("wrapper-cwd.txt").write_text(os.getcwd(), encoding="utf-8")
sys.exit(17 if sys.argv[1:] == ["--tests", "cursor"] else 19)
PY

status=0
DISPLAY= "${wrapper}" \
    --no-xvfb \
    --esctest-dir "${fake_dir}" \
    --working-directory "${fake_dir}" \
    -- --tests cursor || status=$?

if [[ "${status}" -ne 17 ]]; then
    echo "Expected wrapper exit code 17, got ${status}" >&2
    exit 1
fi

cwd_file="${fake_dir}/wrapper-cwd.txt"
if [[ ! -f "${cwd_file}" ]]; then
    echo "Expected wrapper probe file: ${cwd_file}" >&2
    exit 1
fi

actual_cwd=$(tr -d '\r\n' < "${cwd_file}")
if [[ "${actual_cwd}" != "${fake_dir}" ]]; then
    echo "Expected wrapper to run in ${fake_dir}, got ${actual_cwd}" >&2
    exit 1
fi
