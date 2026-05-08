# Issue #13: Closing a Pane Did Not Restore Keyboard Focus

## Symptom

After closing the active split pane, the active pane metadata moved to another
pane, but keyboard input did not reliably return to that pane. The next key
press could be lost until the user clicked a terminal.

## Root Cause

`TermPane::removeTerminal()` selected a replacement terminal when the removed
terminal was current, but it only updated `m_currentTerm`. It did not call
`setFocus()` on the replacement terminal. Since the focused widget was being
deleted, Qt could leave focus unset instead of moving it to the logical sibling
pane.

## Fix

When the removed terminal was current, the close path now focuses the terminal
chosen as the new current terminal. This covers both the direct sibling choice
and the fallback to the first remaining terminal.

## Verification

- Added `testClosingCurrentSplitFocusesSiblingTerminal`, which closes a nested
  active split and verifies both the active pane ID and Qt keyboard focus.
- Ran the focused close-path `test_main_window` cases under the Qt offscreen
  platform.
