# Paste input bypassed terminal safety checks

## Symptom and reproduction

Clipboard text containing an unbracketed LF, CR or CRLF was written directly to
the PTY. A paste could therefore submit a shell command without another Enter
key. File-drop paths were shell-quoted, but line terminators inside a local
filename still bypassed paste handling. Middle-click used a separate copy of
the clipboard implementation.

Before changing production code, `testUnsafePasteWritesNothing` was run against
the previous widget implementation with an explicit `sleep 30` child and a
`QSignalSpy` on `PtySession::dataWritten`. All four data rows failed the
zero-byte assertion. Actual writes included `echo first\necho second`,
`echo first\recho second`, `echo first\r\necho second`, and an ESC `[201~`
terminator payload. These assertions observe bytes written by the widget,
without executing the payload or depending on a shell prompt or input echo.

## Root cause

`pasteFromClipboard` and middle-click checked bracketed mode and called the
low-level encoder only in that mode. Neither path used the terminal-aware
safety check. Their unbracketed path, and their encoder-error fallback, sent
raw clipboard bytes through `writeUserInput`. The file-drop path always used
`writeUserInput`, bypassing both terminal encoding and safety checks.

The prior file-drop fix in
`2026-06-08-issue-28-file-drop-path-input.md` correctly added URL acceptance and
shell quoting; it did not address paste safety. The idle child fixture follows
`2026-08-24-terminal-widget-test-shell-interference.md`, which records why live
shell prompts can corrupt VT tests.

## Fix

All three entry points now capture their UTF-8 bytes and call
`ghostty_terminal_paste` with a `text/plain` MIME reader and
`allow_unsafe=false`. File drops retain shell quoting and use the text source;
clipboard and middle-click preserve their standard/primary locations. Ghostty
chooses framing and encoding from terminal state. Ordinary multiline bracketed
paste is accepted; unsafe unbracketed newlines and embedded bracketed-paste
terminators request confirmation without writing bytes. Errors are logged and
never fall back to raw bytes. Successful pastes still follow the viewport to
the bottom.

`TerminalWidget` stores one pending immutable snapshot and a monotonically
increasing request id. Confirmation retries the captured bytes, even if the
system clipboard has changed. Cancel, replayed ids and superseded ids write
nothing. Every new paste, including safe or empty input, invalidates prior
pending authorization. Restore and session end also clear authorization. A
generic completion signal lets the application dismiss obsolete previews.

`TermPane` creates a nonmodal DTK dialog with a read-only plain text preview,
Cancel as the default, and an explicit warning Paste button. Dialogs are QObject
children of their terminals. QPointer guards and terminal-bound callbacks keep
closure and replacement safe during terminal or pane destruction. The library
contains no application or DTK dialog concepts. Keyboard and IME paths keep
their existing behavior.

## Verification

- Before the fix: the four unsafe-paste PTY-byte rows failed as described above.
- Focused widget tests: 16 QtTest cases passed, including newline and terminator
  rejection/approval, immutable clipboard snapshots, cancel/replay/stale ids,
  later safe-paste invalidation, safe multiline bracketed paste, restore/session
  invalidation, LF/CR/CRLF file URLs, middle click and scroll follow.
- Focused application tests: 6 QtTest cases passed, including approval, cancel,
  plain read-only preview, Cancel default, nonmodal behavior, stale dialog
  replacement, safe-paste/restore dismissal and pane destruction while pending.
  Safe-paste and restore dismissal were separately observed failing before the
  lifecycle signal was added.
- `cmake --build build -j6` passed, including the application and all test targets.
- Complete offscreen CTest passed in two isolated HOME environments:

  ```sh
  HOME="$(mktemp -d)" SHELL=/bin/sh PS1=$'\033[0m' PS2='' QT_QPA_PLATFORM=offscreen \
    ctest --test-dir build -R '^TerminalWidget$' --output-on-failure
  HOME="$(mktemp -d)" SHELL=/bin/sh PS1='$ ' PS2='' QT_QPA_PLATFORM=offscreen \
    ctest --test-dir build -E '^TerminalWidget$' --output-on-failure
  ```

  TerminalWidget passed in 20.08 seconds; all other 11 entries passed in 29.86
  seconds. MainWindow reported 136 passed, 0 failed, 1 skipped: the existing
  D-Bus object-registration test needs a session bus unavailable in this run.
  The visible PS1 in the second group is required by an existing MainWindow
  shell-readiness test.
- Changed C++ files passed `clang-format --dry-run --Werror`; `git diff --check`
  passed. Both TS translation files parse and contain translated dialog strings.

The offscreen platform does not provide native primary-selection clipboard
storage. The middle-click regression overrides the generic selection clipboard
reader, then sends actual middle-button mouse events through the widget and
checks its PTY bytes. Native X11/Wayland primary clipboard integration has not
been exercised by this run.
