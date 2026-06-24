# Tab Context Menu Design

## Goal

Add a right-click context menu to terminal tabs, referencing deepin-terminal's
tab menu, with three items: **Close tab**, **Close other tabs**, and
**Rename title**. The rename dialog is simplified to a single input field whose
content becomes the new tab name (not deepin-terminal's two-input format-string
dialog).

## Non-Goals

This change will not:

- add tab context menus inside the vertical sidebar (only the `DTabBar` widget)
- port deepin-terminal's format-string rename dialog (`%n %d %u ...` placeholders)
- add remote tab title handling
- change PTY, rendering, or terminal emulation behavior in `qtghostty`

## Constraints

- Keep menu composition in `src/app/` (not in `qtghostty`).
- Reuse existing handlers (`onTabCloseRequested`, `setCustomTitle`) where possible.
- Prefer small, local changes over broad refactors.
- Follow existing patterns (`TermPane::showTerminalContextMenu`, `QMenu` usage).

## Current State

- `TabBar` (`src/app/TabBar.h/.cpp`) already installs an event filter on itself
  to handle middle-click tab close. It has no right-click handling.
- `MainWindow` already has:
  - `onTabCloseRequested(int index, bool hasConfirmed)` — closes a tab by index.
  - `closeOtherTabs()` — closes every tab except the *current* one.
  - `onShortcutRenameTitle()` — opens `QInputDialog::getText` (already a single
    input) and calls `pane->setCustomTitle(text)` on the *current* pane.
- `TermPane::showTerminalContextMenu` shows the existing menu pattern: build a
  `QMenu`, add actions, `exec` at the global position.
- `TermPane::setCustomTitle(const QString &)` sets `customTitle`/`currentTitle`
  properties and emits `terminalTitleChanged`, which `MainWindow` already wires
  to refresh tab text and window title.

## Approach

`TabBar` detects right-click in its event filter and emits a signal;
`MainWindow` builds and shows the menu. This keeps menu structure in the app
layer and mirrors the existing `showTerminalContextMenu` pattern.

### TabBar changes (`src/app/TabBar.h/.cpp`)

- Add a new signal: `void tabMenuRequested(int index);`
- In the existing `eventFilter`, on `QEvent::MouseButtonPress` with
  `Qt::RightButton`, find the tab whose `tabRect(i)` contains the click
  position and emit `tabMenuRequested(i)`. Emit nothing when the click is on
  empty tab-bar space.

### MainWindow changes (`src/app/MainWindow.h/.cpp`)

- Connect `TabBar::tabMenuRequested` to a new private slot
  `showTabContextMenu(int index)`.
- `showTabContextMenu(int index)`:
  - Guards against out-of-range indices.
  - Builds a stack `QMenu` with three actions in order:
    1. `tr("Close tab")` → `onTabCloseRequested(index)`
    2. `tr("Close other tabs")` → `closeOtherTabs(index)`, disabled when
       `m_tabBar->count() < 2`
    3. `tr("Rename title")` → `renameTabTitle(index)`
  - `exec`s the menu at `QCursor::pos()`.
- Refactor `closeOtherTabs()`:
  - Add `closeOtherTabs(int keepIndex)` (private slot). It first selects the
    kept tab as current, then iterates from the end, closing every tab whose
    pane differs from the kept pane via `onTabCloseRequested`. Pane identity
    (not index) identifies the kept tab because indices shift as tabs close.
  - The existing no-arg `closeOtherTabs()` keeps the current tab and delegates
    to `closeOtherTabs(m_tabBar->currentIndex())`.
- Add `renameTabTitle(int index)` (private method):
  - Resolves the `TermPane` at `index` from `m_tabs`.
  - Pre-fills the dialog with the current tab text
    (`m_tabBar->tabText(index)`).
  - Opens `QInputDialog::getText(this, tr("Rename title"), tr("New title:"),
    QLineEdit::Normal, currentText, &ok)` — exactly one input field.
  - On accept with non-empty text, calls `pane->setCustomTitle(text)`.
- `onShortcutRenameTitle()` is simplified to call
  `renameTabTitle(m_tabBar->currentIndex())`.

### Why one input field

deepin-terminal's rename dialog has two inputs (normal + remote format) with
placeholder-insert dropdowns. The requirement is deliberately simpler: one
input field whose content is the literal new tab title. `QInputDialog::getText`
already satisfies this exactly, so no custom dialog widget is needed.

## Testing

- `testTabBarRightClickRequestsMenu`: right-click on tab N of a standalone
  `TabBar` emits `tabMenuRequested(N)`. Uses a standalone `TabBar` (not the
  `MainWindow`'s) because the `MainWindow` connection invokes the blocking
  `QMenu::exec`, which would hang a signal-driven test.
- `testTabContextMenuCloseOtherTabsKeepsClickedTab`: invoke
  `closeOtherTabs(keepIndex)` via `QMetaObject::invokeMethod` and verify only
  the kept tab remains.
- Close-tab and rename reuse the already-tested `onTabCloseRequested` and
  `setCustomTitle` paths; the modal input dialog itself is not unit-tested
  (no precedent in the repo for driving `QInputDialog`).

Run with:

```bash
QT_QPA_PLATFORM=offscreen ./build/tests/test_main_window TestMainWindow::testTabBarRightClickRequestsMenu
```

## Files Touched

- `src/app/TabBar.h` — add `tabMenuRequested` signal.
- `src/app/TabBar.cpp` — right-click detection + emit.
- `src/app/MainWindow.h` — `showTabContextMenu`, `closeOtherTabs(int)`,
  `renameTabTitle(int)` declarations.
- `src/app/MainWindow.cpp` — menu building, refactor, rename extraction.
- `tests/test_main_window.cpp` — two new tests.
