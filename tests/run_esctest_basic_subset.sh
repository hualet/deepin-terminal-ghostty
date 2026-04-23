#!/usr/bin/env bash

set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "${script_dir}/.." && pwd)

app_bin="${repo_root}/build/deepin-terminal-ghostty"
esctest_dir="${repo_root}/third_party/esctest"
logfile="${repo_root}/build/test-logs/esctest-basic-subset.log"
print_command=0
timeout_seconds="0.5"

usage() {
    cat <<'EOF'
Usage:
  tests/run_esctest_basic_subset.sh [options] [-- <extra-esctest-args...>]

Options:
  --app <path>           Terminal binary to launch.
  --esctest-dir <path>   Local esctest checkout. Defaults to third_party/esctest.
  --logfile <path>       Path to the esctest log file.
  --timeout <seconds>    esctest read timeout. Defaults to 0.5.
  --print-command        Print the resolved launcher command.
  -h, --help             Show this help text.
EOF
}

prepare_esctest_checkout() {
    local dir="$1"
    local sentinel="${dir}/.codex-python3-ready"

    if [[ -f "${sentinel}" ]]; then
        return 0
    fi

    if [[ ! -d "${dir}/esctest" ]]; then
        echo "esctest checkout is missing ${dir}/esctest" >&2
        return 1
    fi

    python3 -m lib2to3 -w "${dir}/esctest" >/dev/null
    : > "${sentinel}"
}

build_include_regex() {
    local names=("$@")
    local regex=""
    local escaped
    local name

    for name in "${names[@]}"; do
        escaped=${name//./\\.}
        if [[ -n "${regex}" ]]; then
            regex+="|"
        fi
        regex+="${escaped}"
    done

    printf '^(%s)$' "${regex}"
}

parse_log() {
    local path="$1"

    if grep -Eq '^\*\*\* TEST .* FAILED:' "${path}"; then
        echo "esctest reported failures. See ${path}" >&2
        grep -E '^Failing tests:|^[A-Za-z0-9_]+\.[A-Za-z0-9_]+' "${path}" >&2 || true
        return 1
    fi

    if grep -Eq '^(Fails as expected:|EXPECTED FAILURE)' "${path}"; then
        echo "esctest hit known-bug or option-gated cases. See ${path}" >&2
        return 1
    fi

    grep -E '^\*\*\* ' "${path}" | tail -n 1 || true
}

basic_tests=(
    CHATests.test_CHA_DefaultParam
    CHATests.test_CHA_ExplicitParam
    CHATests.test_CHA_IgnoresScrollRegion
    CHATests.test_CHA_OutOfBoundsLarge
    CHATests.test_CHA_ZeroParam
    CNLTests.test_CNL_DefaultParam
    CNLTests.test_CNL_ExplicitParam
    CNLTests.test_CNL_StopsAtBottomLine
    CNLTests.test_CNL_StopsAtBottomLineWhenBegunBelowScrollRegion
    CNLTests.test_CNL_StopsAtBottomMarginInScrollRegion
    CPLTests.test_CPL_DefaultParam
    CPLTests.test_CPL_ExplicitParam
    CPLTests.test_CPL_StopsAtTopLineWhenBegunAboveScrollRegion
    CPLTests.test_CPL_StopsAtTopMarginInScrollRegion
    CUBTests.test_CUB_DefaultParam
    CUBTests.test_CUB_ExplicitParam
    CUBTests.test_CUB_StopsAtLeftEdge
    CUBTests.test_CUB_StopsAtLeftEdgeWhenBegunLeftOfScrollRegion
    CUDTests.test_CUD_DefaultParam
    CUDTests.test_CUD_StopsAtBottomLineWhenBegunBelowScrollRegion
    CUDTests.test_CUD_StopsAtBottomMarginInScrollRegion
    CUFTests.test_CUF_DefaultParam
    CUFTests.test_CUF_ExplicitParam
    CUFTests.test_CUF_StopsAtRightEdgeWhenBegunRightOfScrollRegion
    CUFTests.test_CUF_StopsAtRightMarginInScrollRegion
    CUFTests.test_CUF_StopsAtRightSide
    CUPTests.test_CUP_ColumnOnly
    CUPTests.test_CUP_DefaultParams
    CUPTests.test_CUP_OutOfBoundsParams
    CUPTests.test_CUP_RowOnly
    CUPTests.test_CUP_ZeroIsTreatedAsOne
    CUUTests.test_CUU_DefaultParam
    CUUTests.test_CUU_StopsAtTopLineWhenBegunAboveScrollRegion
    CUUTests.test_CUU_StopsAtTopMarginInScrollRegion
    HPATests.test_HPA_DefaultParams
    HPATests.test_HPA_DoesNotChangeRow
    HPATests.test_HPA_IgnoresOriginMode
    HPATests.test_HPA_StopsAtRightEdge
    HPRTests.test_HPR_DefaultParams
    HPRTests.test_HPR_DoesNotChangeRow
    HPRTests.test_HPR_StopsAtRightEdge
    HVPTests.test_HVP_ColumnOnly
    HVPTests.test_HVP_DefaultParams
    HVPTests.test_HVP_OutOfBoundsParams
    HVPTests.test_HVP_RowOnly
    HVPTests.test_HVP_ZeroIsTreatedAsOne
    VPATests.test_VPA_DefaultParams
    VPATests.test_VPA_DoesNotChangeColumn
    VPATests.test_VPA_IgnoresOriginMode
    VPATests.test_VPA_StopsAtBottomEdge
    VPRTests.test_VPR_DefaultParams
    VPRTests.test_VPR_DoesNotChangeColumn
    VPRTests.test_VPR_StopsAtBottomEdge
    DECSTBMTests.test_DECSTBM_MovsCursorToOrigin
    INDTests.test_IND_Basic
    LFTests.test_LF_Basic
    NELTests.test_NEL_Basic
    RITests.test_RI_Basic
    VTTests.test_VT_Basic
)

extra_args=()
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
        --logfile)
            logfile="$2"
            shift 2
            ;;
        --timeout)
            timeout_seconds="$2"
            shift 2
            ;;
        --print-command)
            print_command=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            extra_args=("$@")
            break
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

mkdir -p "$(dirname "${logfile}")"
prepare_esctest_checkout "${esctest_dir}"

include_regex=$(build_include_regex "${basic_tests[@]}")
esctest_args=(
    --expected-terminal=xterm
    --include "${include_regex}"
    --timeout "${timeout_seconds}"
    --logfile "${logfile}"
)

if ((${#extra_args[@]} > 0)); then
    esctest_args+=("${extra_args[@]}")
fi

unset DISPLAY
unset QT_QPA_PLATFORM

runner_args=(
    --app "${app_bin}"
    --esctest-dir "${esctest_dir}"
    --no-xvfb
)

if ((print_command)); then
    runner_args+=(--print-command)
fi

"${repo_root}/tests/run_esctest.sh" "${runner_args[@]}" -- "${esctest_args[@]}"
parse_log "${logfile}"
