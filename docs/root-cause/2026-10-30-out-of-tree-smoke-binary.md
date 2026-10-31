# Smoke tests launched an executable from a different build directory

## Evidence

A fresh build configured with `-B build/adoption-verify` compiled successfully.
Its TerminalWidget and MainWindow suites passed, while StartupCommandIntegration
and EsctestWrapperSmoke aborted with `free(): invalid pointer` or
`munmap_chunk(): invalid pointer` (exit 134).

Both smoke scripts derived the repository root and hard-coded
`build/deepin-terminal-ghostty`. The primary checkout still had an older
executable at that path, while CTest was running in `build/adoption-verify`.
The test environment placed the new build's libraries first in
`LD_LIBRARY_PATH`, pairing the old executable with the new `libqtghostty`.
TerminalWidget's private layout had changed, so this was an incompatible
executable/library pairing rather than a valid test of the new application.

Directly launching `build/adoption-verify/deepin-terminal-ghostty` with the same
isolated home, session bus and library environment returned the requested child
exit code 23. Selecting that exact executable in the two scripts made both
originally failing smoke tests pass without a production C++ change.

## Change

CTest now passes `$<TARGET_FILE:deepin-terminal-ghostty>` to each smoke script.
The startup integration script accepts an optional executable path. The wrapper
smoke accepts the same path and forwards it using the existing `--app` option.
Standalone script invocation retains its existing default build path.

## Verification

After reconfiguring the primary `build/adoption-verify` tree, both affected
CTest entries passed in 0.54 seconds under a fresh session bus and isolated home.
Evidence: `build/adoption-harness-configure.log` and
`build/adoption-harness-ctest.log`. The worktree's affected two tests also
passed in 0.51 seconds. The complete primary non-widget CTest group then passed
all 11 entries in 37.36 seconds, including both repaired smoke tests; evidence
is `build/adoption-other-ctest.log`. The test harness changes are identical in
the primary checkout and the adoption worktree. `git diff --check` passed.
