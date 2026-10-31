# Terminal Paste Protection Implementation Plan

> Use superpowers:subagent-driven-development, one implementer at a time,
> followed by spec and quality review. Leave changes uncommitted.

**Goal:** Require explicit confirmation before paste data can execute commands.

**Architecture:** TerminalWidget owns bytes and pending authorization; TermPane
owns a nonmodal DTK confirmation dialog. All paste entry points use Ghostty's
terminal-aware encoder and safety check.

**Tech Stack:** C++20, Qt6, DTK6, libghostty-vt 1.4, Qt Test.

## Task 1: Red regressions

Files: tests/test_terminal_widget.cpp, tests/test_main_window.cpp.

- [x] Add PTY-byte assertions for unbracketed CR/LF, terminator injection,
  confirmation, cancel, stale id and clipboard snapshot behavior.
- [x] Reproduce the old unsafe write before modifying production code.
- [x] Cover primary-selection paste when the offscreen platform supports it,
  otherwise exercise the shared capability with an explicit primary location.
- [x] Add a file URL containing a line terminator and verify no automatic write.

## Task 2: Unified paste capability and app confirmation

Files: src/libqtghostty/TerminalWidget.h/.cpp, src/app/TermPane.cpp.

- [x] Expose pasteConfirmationRequested(quint64, QString) and confirmPaste(id,
  bool), with immutable pending bytes and a single outstanding id.
- [x] Add a private text-paste helper that constructs GhosttyPaste with one
  text/plain MIME and a GhosttyMimeReader writing the captured bytes.
- [x] On GHOSTTY_REJECTED store the snapshot and emit; on errors log operation
  and result. Never fall back to raw unsafe bytes.
- [x] Route system clipboard, primary paste and shell-quoted file drops through
  the helper. Approval uses the captured bytes; cancellation and stale ids do
  nothing. Clear pending requests on import/session end.
- [x] In TermPane, show one nonmodal DDialog per requesting terminal, with
  read-only plain text preview, Cancel default, Paste warning action, and safe
  QPointer handling on pane/session destruction.
- [x] Add integration assertions for dialog approval and cancellation.

## Task 3: Verification and documentation

Files: translations/deepin-terminal-ghostty_zh_CN.ts,
translations/deepin-terminal-ghostty_es.ts, docs/root-cause/2026-10-29-paste-protection.md.

- [x] Translate new visible Chinese strings, preserve existing translations.
- [x] Run focused tests, complete TerminalWidget and MainWindow tests, then full
  CTest offscreen with isolated HOME, SHELL=/bin/sh, a nonprinting PS1 reset (ESC[0m), PS2=''.
- [x] Format changed C++, inspect diff and document root cause plus verification.

## Implementation notes

- Added `pasteConfirmationFinished(id)` so safe/empty paste replacement,
  restore, cancellation and session end also close the matching app preview.
- App-created dialogs are QObject children of their terminal. This avoids
  terminal-destruction callbacks touching already-destroyed pane members.
- Primary-selection tests use the generic protected clipboard reader because
  the offscreen platform cannot store a native primary selection.
- Full-suite verification uses two environment groups: TerminalWidget with a
  nonprinting PS1 reset, and the other 11 CTest entries with visible PS1='$ '.
  A pre-existing MainWindow readiness test requires a visible shell prompt.

## Final verification

- Full build passed (`cmake --build build -j6`).
- Focused widget set: 16 passed; focused app set: 6 passed.
- Complete TerminalWidget CTest passed (20.08 s), then all other 11 CTest
  entries passed (29.86 s) with the split environments above.
- MainWindow: 136 passed, 0 failed, 1 pre-existing session-D-Bus-dependent skip.
- Changed C++ clang-format verification, TS parsing, and git diff checks passed.
- Changes remain uncommitted; engine upgrade files were not modified by stage 2.
