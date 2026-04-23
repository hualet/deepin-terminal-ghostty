#!/usr/bin/env bash

set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "${script_dir}/.." && pwd)

app_bin="${repo_root}/build/deepin-terminal-ghostty"
esctest_dir=""
esctest_command=""
working_directory=""
print_command=0
use_xvfb=1

usage() {
    cat <<'EOF'
Usage:
  tests/run_esctest.sh [options] [-- <esctest-args...>]

Options:
  --app <path>                Terminal binary to launch.
  --esctest-dir <path>        Upstream esctest checkout directory.
  --esctest-command <shell>   Exact shell command to run inside the startup tab.
  --working-directory <path>  Working directory for the startup session.
  --print-command             Print the resolved launcher command before running it.
  --no-xvfb                   Do not wrap the terminal with xvfb-run.
  -h, --help                  Show this help text.

Examples:
  tests/run_esctest.sh --esctest-dir ~/src/esctest -- --tests cursor
  tests/run_esctest.sh --esctest-command "python3 /path/to/esctest.py --tests sgr"
EOF
}

quote_args() {
    local quoted=()
    local arg
    for arg in "$@"; do
        quoted+=("$(printf '%q' "${arg}")")
    done
    printf '%s' "${quoted[*]}"
}

detect_esctest_command() {
    local dir="$1"

    if [[ -x "${dir}/bin/esctest" ]]; then
        printf '%q' "${dir}/bin/esctest"
        return 0
    fi

    if [[ -f "${dir}/esctest.py" ]]; then
        printf 'python3 %q' "${dir}/esctest.py"
        return 0
    fi

    if [[ -f "${dir}/main.py" ]]; then
        printf 'python3 %q' "${dir}/main.py"
        return 0
    fi

    return 1
}

esctest_args=()
while (($# > 0)); do
    case "$1" in
        --app)
            app_bin="$2"
            shift 2
            ;;
        --esctest-dir)
            esctest_dir="$2"
            shift 2
            ;;
        --esctest-command)
            esctest_command="$2"
            shift 2
            ;;
        --working-directory)
            working_directory="$2"
            shift 2
            ;;
        --print-command)
            print_command=1
            shift
            ;;
        --no-xvfb)
            use_xvfb=0
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            esctest_args=("$@")
            break
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ ! -x "${app_bin}" ]]; then
    echo "Terminal binary not found or not executable: ${app_bin}" >&2
    exit 1
fi

if [[ -z "${esctest_command}" ]]; then
    if [[ -z "${esctest_dir}" ]]; then
        echo "Pass --esctest-dir or --esctest-command." >&2
        exit 2
    fi

    if [[ ! -d "${esctest_dir}" ]]; then
        echo "esctest directory not found: ${esctest_dir}" >&2
        exit 1
    fi

    if ! esctest_command=$(detect_esctest_command "${esctest_dir}"); then
        echo "Unable to detect an esctest entrypoint under ${esctest_dir}." >&2
        echo "Pass --esctest-command with the exact command to run." >&2
        exit 2
    fi

    if ((${#esctest_args[@]} > 0)); then
        esctest_command+=" $(quote_args "${esctest_args[@]}")"
    fi
fi

launcher=()
if ((use_xvfb)) && [[ -z "${DISPLAY:-}" ]]; then
    if command -v xvfb-run >/dev/null 2>&1; then
        launcher+=(xvfb-run -a)
    else
        echo "DISPLAY is unset and xvfb-run is not installed." >&2
        exit 1
    fi
fi

qt_qpa_platform="${QT_QPA_PLATFORM:-}"
if [[ -z "${DISPLAY:-}" ]]; then
    if ((use_xvfb)); then
        if [[ -z "${qt_qpa_platform}" ]]; then
            qt_qpa_platform="xcb"
        fi
    else
        qt_qpa_platform="offscreen"
    fi
elif [[ -z "${qt_qpa_platform}" ]]; then
    qt_qpa_platform="xcb"
fi

if [[ -z "${qt_qpa_platform}" ]]; then
    echo "Unable to determine QT_QPA_PLATFORM." >&2
    exit 1
fi

launcher+=(env QT_QPA_PLATFORM="${qt_qpa_platform}")
launcher+=("${app_bin}" --execute "${esctest_command}" --propagate-exit-code)
if [[ -n "${working_directory}" ]]; then
    launcher+=(--working-directory "${working_directory}")
fi

if ((print_command)); then
    printf 'Launcher: %s\n' "$(quote_args "${launcher[@]}")"
fi

exec "${launcher[@]}"
