# libghostty-vt Upgrade Design

## Goal

Upgrade the vendored `libghostty-vt` runtime and public headers from upstream
commit `07d31666e` to the pinned upstream commit
`2dd79f3bc6af649e68422b08e21ad0300fd8b391`.

## Scope

Treat the runtime library, public headers, and the one affected
`TerminalWidget` call site as an atomic compatibility unit:

- replace `lib/libghostty-vt.so` with a ReleaseFast, baseline-CPU build;
- synchronize the full upstream `include/` tree into `lib/include/`;
- migrate terminal construction away from the removed
  `GhosttyTerminalOptions`;
- preserve the existing byte-budget interpretation of the scrollback setting
  with `GHOSTTY_TERMINAL_OPT_SCROLLBACK_MAX_BYTES`.

Do not adopt newly exposed desktop-notification, progress, Unicode, color, or
compression APIs in this change.

## Compatibility

The new library retains the `libghostty-vt.so.0` SONAME and does not remove
exported symbol names, but `ghostty_terminal_new()` has a new signature.
Therefore the library and application must be rebuilt together; replacing only
the runtime library is unsupported.

The replacement library must report:

- version `0.1.0-dev+2dd79f3bc`;
- ReleaseFast optimization;
- SIMD enabled;
- Kitty graphics enabled;
- tmux control mode disabled.

## Verification

Use the existing scrollback budget regression as the behavioral contract.
After staging the new headers, first confirm that the old call site fails to
compile because `GhosttyTerminalOptions` no longer exists. Then migrate the
call site and verify:

- the focused scrollback test;
- the full `TerminalWidget` binary;
- all offscreen CTest groups;
- C++ formatting, artifact identity, diff integrity, and worktree scope.

The known vertical-sidebar close-button timeout is not part of this upgrade; it
must be reported separately if it reproduces with both old and new runtimes.
