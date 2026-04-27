# Command Status Indicator Design

## Summary

Add per-command exit code tracking to shell integration and display colored status dots on inactive tabs/panes in the vertical sidebar: yellow (running), green (succeeded, exit 0), red (failed, exit non-zero). The dot clears when the user switches to that tab/pane.

## Requirements

- Track individual command exit codes via shell integration (bash and zsh)
- Expose a `CommandState` enum on `TerminalWidget`: `Idle`, `Running`, `Succeeded`, `Failed`
- Propagate state through `TermPane` → `MainWindow` → `VerticalTabSidebar`
- Render a 6-8px colored dot on the right side of each tab/pane row in the vertical sidebar
- Single-pane tabs: dot on tab row; multi-pane tabs: dot on each pane row (same logic as process badge)
- Dot visibility: always hidden on the active tab/pane; visible on all others when state is not Idle
- Dot auto-clears when user switches to that tab/pane (state → Idle)
- No changes to horizontal DTabBar

## Architecture

### 1. Shell Integration — Exit Code Reporting

Add a new helper function to the shared prelude in `PtySession::shellIntegrationPrelude()`:

```bash
__deepin_terminal_ghostty_report_result() {
    printf '\033]777;ShellCommandResult=%s\033\\' "$?"
}
```

New OSC sequence: `OSC 777;ShellCommandResult=<exit_code> ST`

#### Bash

Modify `PROMPT_COMMAND` to report the exit code before clearing the command:

```bash
PROMPT_COMMAND="__deepin_terminal_ghostty_report_result;__deepin_terminal_ghostty_clear_command${PROMPT_COMMAND:+;$PROMPT_COMMAND}"
```

#### Zsh

Modify the `precmd` hook to report the exit code before clearing:

```bash
__deepin_terminal_ghostty_precmd() {
    __deepin_terminal_ghostty_report_result
    __deepin_terminal_ghostty_clear_command
}
```

The `$?` is captured at the top of precmd, before any other commands in the hook could alter it.

### 2. TerminalWidget — Command State

#### New enum (TerminalWidget.h)

```cpp
enum class CommandState { Idle, Running, Succeeded, Failed };
```

#### New parsing in `scanShellIntegrationSequences()`

Extend the OSC 777 parsing to recognize `ShellCommandResult=<number>` alongside the existing `ShellCommand=`. Store the exit code in a member variable `m_pendingExitCode` (int, default -1).

#### State transition logic in `setShellCommand()`

```
ShellCommand=<non-empty> → state = Running, clear m_pendingExitCode
ShellCommandResult=<N>   → m_pendingExitCode = N
ShellCommand=<empty>     → if m_pendingExitCode >= 0
                              state = (m_pendingExitCode == 0) ? Succeeded : Failed
                              m_pendingExitCode = -1
                            else
                              state = Idle
```

#### New signal

`commandStateChanged(CommandState state)` — emitted on every state transition.

#### New property

`commandState` — Qt dynamic property, readable by TermPane.

### 3. Data Flow

```
Shell preexec  → OSC 777;ShellCommand=<base64>
Shell precmd   → OSC 777;ShellCommandResult=<N>
Shell precmd   → OSC 777;ShellCommand=

TerminalWidget
  scanShellIntegrationSequences() → setShellCommand()
  property: commandState
  signal: commandStateChanged(CommandState)

TermPane
  connects TerminalWidget::commandStateChanged
  emits paneCommandStateChanged(QUuid paneId, CommandState)
  paneInfos() includes CommandState in PaneInfo

MainWindow
  connects TermPane::paneCommandStateChanged
  calls refreshSidebar()

VerticalTabSidebar
  reads PaneInfo.commandState
  renders status dot per tab/pane row
```

### 4. PaneInfo Extension

Add `CommandState commandState` to `TermPane::PaneInfo`:

```cpp
struct PaneInfo {
    QUuid id;
    QString title;
    QString iconName;
    bool isActive = false;
    CommandState commandState = CommandState::Idle;
};
```

`paneInfos()` reads `property("commandState")` from each `TerminalWidget`.

### 5. VerticalTabSidebar — Status Dot Rendering

#### Widget

A `QLabel` subclass (or plain `QLabel`) with:
- Fixed size: 8×8 px circle
- Object name: `commandStatusDot`
- Positioned at the right edge of each tab/pane row, vertically centered, with a small right margin
- Overlaps the title text area (floats above text) using a layout with negative spacing or manual positioning

#### Colors

| State   | Color                              |
|---------|------------------------------------|
| Running | Yellow (#FFB800 or theme-aware)    |
| Succeeded | Green (#2ED573 or theme-aware)   |
| Failed  | Red (#FF4757 or theme-aware)       |
| Idle    | Hidden (widget hidden, not opaque) |

#### Visibility rules

Same logic as process badge:
- Single-pane tab: dot on tab row only
- Multi-pane tab: dot on each pane row only (tab row dot hidden)

Additional rule:
- Dot is hidden when the tab/pane is the currently active one (the user is already looking at it)

### 6. Auto-Clear on Focus

When a `TerminalWidget` receives focus (via `focusGained()` signal or `activePaneChanged`), set its `commandState` back to `Idle` if it is currently `Succeeded` or `Failed`. This makes the dot disappear when the user switches to that tab/pane.

Implementation in `TermPane::setupTerminalConnections()`:

```cpp
connect(term, &TerminalWidget::focusGained, this, [this, term]() {
    if (term->property("commandState").toInt() != static_cast<int>(CommandState::Idle)) {
        term->setProperty("commandState", static_cast<int>(CommandState::Idle));
        Q_EMIT paneCommandStateChanged(ensurePaneId(term), CommandState::Idle);
    }
});
```

## Scope

- In scope: shell integration exit code, CommandState enum, sidebar status dots, auto-clear on focus
- Out of scope: horizontal DTabBar status dots, fish shell integration, sound/notification on completion, configurable colors

## Files Changed

| File | Change |
|------|--------|
| `src/libqtghostty/PtySession.cpp` | Add `__deepin_terminal_ghostty_report_result` to prelude; update bash PROMPT_COMMAND and zsh precmd |
| `src/libqtghostty/TerminalWidget.h` | Add `CommandState` enum, `commandStateChanged` signal |
| `src/libqtghostty/TerminalWidget.cpp` | Parse `ShellCommandResult` OSC; implement state machine in `setShellCommand()` |
| `src/app/TermPane.h` | Add `commandState` to `PaneInfo`; add `paneCommandStateChanged` signal |
| `src/app/TermPane.cpp` | Connect `commandStateChanged` and `focusGained`; populate `commandState` in `paneInfos()` |
| `src/app/MainWindow.cpp` | Connect `paneCommandStateChanged` → `refreshSidebar()` |
| `src/app/VerticalTabSidebar.h` | No structural changes needed (TabItem carries PaneInfo which now has commandState) |
| `src/app/VerticalTabSidebar.cpp` | Create and position status dot widgets in `rebuild()`; apply color based on state |
