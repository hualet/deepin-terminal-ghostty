# Restore Session Tab Activation Crash

## Symptom

After restoring a terminal session (auto or manual), clicking a tab could crash
the application inside `QObject::property("currentTitle")` called from
`MainWindow::onTabCurrentChanged()`. The crash was a use-after-free on a
`TerminalWidget` pointer that had already been freed.

Stack trace (abbreviated):

```text
#0  QObject::property (this=<freed>, name="currentTitle")
#1  MainWindow::onTabCurrentChanged
#4  DTabBar::currentChanged(int)
```

## Root Cause

Session restore calls `addTab(false, std::nullopt)` for each saved tab, which
creates a `TermPane` with an initial terminal and starts a PTY shell.
`TermPane::restoreFromSplitTree()` then replaces that terminal tree:

1. The initial terminal's container is removed from the layout and scheduled for
   deferred deletion via `deleteLater()`.
2. New restored terminals are created and installed as the pane's widget tree.
3. `m_currentTerm` is updated to point to the new terminal.

The initial terminal's `TerminalWidget::sessionClosed` signal is still connected
to the TermPane via `setupTerminalConnections()`. If the initial terminal's shell
exits before the deferred delete fires (e.g., the shell receives SIGHUP or exits
on its own), `removeTerminal(initialTerminal)` is invoked.

`removeTerminal()` searched the pane's splitter tree for the terminal's widget.
Because the initial terminal's container had already been replaced by the
restored tree, the search returned `nullptr`. The code interpreted this as "this
is the only terminal in the pane" and incorrectly emitted `sessionClosed()` on
the `TermPane`. `MainWindow::onTerminalSessionClosed` then closed the entire tab
via `closePane()` / `onTabCloseRequested()`.

This spurious tab closure could cascade across restored tabs and leave the
window in an inconsistent state. When the user subsequently clicked a tab,
`onTabCurrentChanged` dereferenced a `TerminalWidget` pointer from a pane whose
terminals were being torn down — a use-after-free crash.

Additionally, `m_currentTerm` and `m_startupTerminal` in `TermPane` were raw
pointers. If a terminal was destroyed without going through `removeTerminal()`,
these pointers would dangle silently. `TabRecord.pane` in `MainWindow` had the
same weakness.

## Fix

### Primary: Guard `removeTerminal` against unmanaged terminals

`TermPane::removeTerminal()` now checks whether the terminal's widget is the
pane's root widget before entering the "only terminal" branch. If the widget is
neither in a splitter nor the root widget, the terminal is not managed by this
pane (it was already replaced and is pending deferred deletion). The call is
ignored instead of emitting `sessionClosed()`.

### Defense-in-depth: Use `QPointer` for owning-like references

- `TermPane::m_currentTerm` changed to `QPointer<TerminalWidget>`.
- `TermPane::m_startupTerminal` changed to `QPointer<TerminalWidget>`.
- `MainWindow::TabRecord::pane` changed to `QPointer<TermPane>`.

If any of these objects are destroyed without explicit cleanup, the pointer
automatically becomes null instead of dangling. Callers already null-check
before dereferencing.

### Additional: Clear `m_startupTerminal` during restore

`restoreFromSplitTree()` now clears `m_startupTerminal` alongside
`m_currentTerm` when discarding the initial terminal, preventing a stale
reference to the replaced terminal.

## Verification

- Added `testRestoreFromSplitTreeDoesNotClosePaneWhenOldTerminalExits` which
  verifies that emitting `sessionClosed` on a replaced terminal does not close
  the pane. Confirmed this test fails without the fix (pane spuriously emits
  `sessionClosed`) and passes with it.
- Added `testRestoreSessionSwitchTabDoesNotCrash` and
  `testRestoreSessionWithSplitsSwitchTabDoesNotCrash` which restore multi-tab
  sessions (including splits) and switch between all tabs without crashing.
- All existing TermPane split/close/navigate tests continue to pass.
