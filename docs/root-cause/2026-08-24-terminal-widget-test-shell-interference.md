# TerminalWidget VT tests receive live shell output

## Symptom

The full offscreen test run reported three `TerminalWidget` failures while
validating the Ghostty upgrade:

- a split OSC 777 notification was emitted before the test supplied its second
  chunk;
- a box-drawing-only frame contained an unexpected text run;
- a split OSC 52 write did not preserve the expected clipboard transition.

## Root Cause

The three tests initialized `TerminalWidget` without start options. That starts
the user's interactive shell and enables its shell integration. Prompt text and
shell-integration OSC sequences could therefore arrive between the test's
synthetic PTY chunks.

This was independent of the Ghostty upgrade: the same tests also failed with
the previous vendored library. With `SHELL=/bin/sh`, the notification body was
observed as `Needs $ attention`, directly showing the prompt inserted between
the two test chunks. The same prompt accounted for the extra text render run and
invalidated the split OSC 52 payload.

## Fix

The affected notification, box-drawing, and split OSC 52 tests now use the same
silent `sleep 5` start command as neighboring render tests. The production PTY
and VT paths are unchanged; only unrelated live shell output is removed from the
test fixture.

## Verification

- The three focused regressions passed together: 5 QtTest cases, 0 failures.
- The complete `test_terminal_widget` binary passed: 182 tests, 0 failures.
- The complete offscreen CTest suite passed: 11 test binaries, 0 failures.
