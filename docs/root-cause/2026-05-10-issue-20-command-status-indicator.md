# Issue 20: command status indicator changes on pane activation

## Symptom

With vertical tabs enabled, a tab can contain two split panes: a coding agent on
one side and a shell on the other. After a command such as `git diff` or
`git show` finishes in the shell pane, switching focus to the other pane showed
a green command status dot next to the inactive shell pane, even though the tab
was still the current visible tab.

## Root Cause

Pane-level status dots were shown from layout state instead of command-result
notification state. `VerticalTabSidebar` passed `!pane.isActive` as the
`hasPending` argument for pane status dots, so any inactive pane with a
`Succeeded` or `Failed` command state could show an indicator inside the active
tab.

There was also a second activation-driven state change. `TermPane` cleared a
terminal's successful or failed command state to `Idle` when the terminal gained
focus. That meant pane activation emitted `commandStateChanged`, even though no
new command status had arrived from shell integration.

## Fix

Pane status dots are now gated by tab-level pending command-result state and are
hidden for the current tab. Activating a pane now only updates the active
terminal pointer; command state is updated only by shell integration command
start/result events.

## Verification

- Added `testCommandStatusDotNotShownForInactivePaneInCurrentTab` to cover the
  active-tab split-pane scenario from the issue.
- Added `testPaneActivationDoesNotClearCommandState` to verify pane activation
  does not emit command state changes or clear the previous command result.
- Ran the focused command-status tests under `QT_QPA_PLATFORM=offscreen`.
