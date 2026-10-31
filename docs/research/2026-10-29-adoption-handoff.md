# Ghostty 1.4 adoption status

Research committed as `1ca7100`. The implementation was approved for commit on 2026-10-31. On 2026-10-30, the verified changes were
integrated into the primary checkout and checked byte-for-byte against
`.worktrees/ghostty-1.4`, branch `codex/ghostty-1.4`. The worktree is retained
with its rootless toolchain and verification logs.

Stages 1 (engine), 2 (paste protection) and 3 (Kitty images/animation) completed
with independent spec/quality reviews, builds and all 12 CTest entries passing.
Stage 4 implementation, full validation and independent specification/quality
reviews are complete. The quality reviewer independently reran the original
clipboard ownership, provider lifetime and backpressure reproductions after
their fixes. See the dated rich clipboard plan and root-cause report for evidence.

Stage 4 now has synchronous sized read/write replies, bounded owning MIME
snapshots, default-denied reads and Ghostty-owned password grants, rich paste
advertisement, immutable unsafe-text confirmation, and app consent previews.
The busy gate spans outer parsing/import/paste, snapshots/provider/reply,
preparse resize publication and post-processing. Source destruction during
virtual MIME calls aborts with BUSY; a copied provider survives nested setter
replacement. Supported pane/tab/window close and restore defer until idle.
The bounded bulk PTY queue preserves whole replies and treats EAGAIN as accepted
backpressure, while ordinary keyboard writes keep their existing queue policy.

Final host build passed. Offscreen tests in isolated homes and fresh
`dbus-run-session` buses passed all 12 CTest entries (TerminalWidget separately,
then the other 11). Project-wide formatting and diff checks passed; generated
`settings_translation.cpp` is unchanged. Compiled Chinese and Spanish consent
translations were checked. Logs remain under `build/stage4-final-*`.

The sandbox launcher failed with ENOENT; host execution works with `/bin/sh`,
login disabled and `sandbox_permissions=require_escalated`. This is not a
missing host shell. Rootless Qt/DTK dependencies and clang-format remain under
`build/local-deps`; source `build/local-deps/env.sh` for build/test/format. Keep
worktree `build/tests:build` first in `LD_LIBRARY_PATH`. TerminalWidget needs the
escape-reset prompt; other tests need the normal prompt. No sudo was used.

No native clipboard manager, real primary-selection backend or arbitrary CLI
interoperability has been verified. The binary, headers, source, tests, translations and all implementation
documents are now in the primary checkout. The verified implementation is included in this commit.

## Primary-checkout verification (2026-10-30)

The integrated source configured and built successfully in
`build/adoption-verify`. All 12 CTest entries passed in isolated homes and fresh
session buses: TerminalWidget 24.69 s, the remaining 11 entries 37.36 s. Logs are
`build/adoption-build.log`, `build/adoption-widget-ctest.log` and
`build/adoption-other-ctest.log` in the primary checkout.

The two shell smoke tests formerly hardcoded the old `build` executable. CMake
now passes the executable from the current build directory; both affected tests
and the complete remaining suite passed, and the test wiring passed independent
review. See `docs/root-cause/2026-10-30-out-of-tree-smoke-binary.md`.
