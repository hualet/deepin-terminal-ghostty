# Terminal Rich Clipboard Implementation Plan

> Use superpowers:subagent-driven-development, one implementer followed by spec
> and quality review. Leave implementation changes uncommitted.

**Goal:** Add safe rich clipboard reads and user paste events through Ghostty's
synchronous MIME clipboard API.

**Architecture:** Generic clipboard transport and consent callback in qtghostty;
DTK preview/permission UI in app; scoped engine processing protects callback
lifetime across a modal event loop.

**Tech Stack:** C++20, Qt6, DTK6, libghostty-vt1.4, Qt Test.

## Task1: Lock down callback and paste behavior with tests

Files: tests/test_terminal_widget.cpp, tests/test_main_window.cpp.

- [x] Add an OSC5522 write ACK regression, default-denied and approved reads,
  binary data and requested-MIME filtering; observe failures first.
- [x] Verify a binary reply larger than the existing1MiB PTY queue is complete
  or explicitly rejected before any reply payload; never silently truncate.
- [x] Add mode5522 user paste event and granted follow-up read cases using real
  protocol fixtures from upstream tests; validate list queries preserve grants.
- [x] Test consent snapshot and per-password session authorization isolation.

## Task2: Synchronous callback transport and bounded snapshots

Files: src/libqtghostty/TerminalWidget.h/.cpp and a focused helper if needed.

- [x] Correct effectClipboardWrite to void+sized reply on every path.
- [x] Add effectClipboardRead and generic owning request/provider API, default
  deny, granted/list-only fast paths and Ghostty-owned remembered grants.
- [x] Validate formats/pointers/size, enforce read/write budgets, preserve binary
  data and primary selection identity, and snapshot before prompting.
- [x] Extend stage2 paste snapshots to MIME data and route ghostty_terminal_paste
  without bypassing safety confirmation or user-paste grant semantics.

## Task3: Protect the outer terminal operation

Files: src/libqtghostty/TerminalWidget.h/.cpp, PtySession.h/.cpp,
src/app/TermPane.cpp, MainWindow.cpp.

- [x] Move each pending PTY batch into stable local storage before parsing.
- [x] Expose isTerminalBusy/terminalIdle; gate engine operations, PTY I/O/poll,
  timers, import/export, snapshots and session-end until outer processing ends.
- [x] Defer supported pane/tab/window close and restore paths until idle, using
  stable ids and guarded callbacks. Never destroy an active engine callback.
- [x] Add nested-loop tests for incoming data, rendering/resize, exit and close;
  assert preserved byte order and no nested engine access.

## Task4: Application permission UI and verification

Files: src/app/TermPane.cpp or focused ClipboardPermissionDialog helper,
translations/*.ts, docs/root-cause/2026-10-29-clipboard-callback-contract.md.

- [x] Install app provider with bounded plain-text/image preview, no external
  resource loading, Cancel default and remember only if supported.
- [x] Prevent nested consent dialogs across terminals; return BUSY on conflict.
- [x] Update Paste enabled state for image-only MIME and Chinese translations.
- [x] Run focused transport/UI/lifecycle tests, full CTest offscreen, formatting
  and diff checks. Record the original callback defect and tested limitations.

## Implementer verification (2026-10-30)

- Observed failing regressions before fixes for write ACK, missing reads, callback
  reentry, active-selection copy, primary image-only routing, unavailable text
  MIME fallback, preparse resize publication, clipboard owner/provider
  replacement, and bulk enqueue under PTY backpressure.
- Focused transport and lifecycle tests cover 2 MiB byte-exact replies, 17 MiB
  rejection before DATA, requested formats, pointer/count/size rejection,
  PNG/HTML/binary writes, empty versus clear, password/direction/terminal grants,
  one-time paste grants preserved by list queries, immutable snapshots and
  nested data ordering, real child exit, and supported pane/window close.
- App tests inspect plain text, bounded PNG preview, Cancel as default,
  conditional unchecked Remember, and BUSY for a second terminal prompt.
- `cmake --build build -j4` passed. With isolated homes and fresh session buses,
  offscreen CTest passed all 12 entries: TerminalWidget separately with the
  escape-reset prompt (24.83 s), then the other 11 with the normal prompt
  (36.86 s). Logs: `build/stage4-final-build.log`,
  `build/stage4-final-widget-ctest.log`, and
  `build/stage4-final-other-ctest.log`.
- Project-wide `clang-format --dry-run --Werror` and `git diff --check` passed;
  logs are `build/stage4-format.log` and `build/stage4-diff-check.log`.
  Generated `settings_translation.cpp` remains unchanged. Chinese and Spanish
  consent strings were compiled and confirmed in the resulting QM catalogs.
- Initial inherited-bus CLI failure was environmental; final runs used
  `dbus-run-session` and could not connect to the user's existing service.
  A bare-link hover assertion failed once and passed in isolation and in the
  final complete widget runs; no link-related production change was made.

Independent specification review and final quality review passed. The quality
reviewer independently reran all three original lifetime/backpressure probes
and the focused regressions. The verified implementation was integrated into
the primary checkout on 2026-10-30 with exact file parity. The user approved
committing the verified implementation on 2026-10-31.
Native clipboard managers, real X11/Wayland primary selection and arbitrary CLI
interoperability remain unverified. The primary image-only regression uses the
protected MIME source seam and checks routing rather than platform support.
