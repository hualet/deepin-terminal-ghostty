# Vertical Tabs Design

## Goal

Add a switchable vertical tab layout for the terminal window, exposed as a translatable checkable menu item named `Vertical Tabs`, with the enabled state persisted in application settings.

When vertical tabs are enabled:

- the titlebar becomes a compact strip sized to fit the app icon on the left and window controls on the right with modest padding
- a left sidebar appears, separated from the terminal content by a splitter-style vertical divider
- the sidebar shows a two-level structure:
  - top level: tabs
  - second level: panes inside each tab
- each tab title follows the active pane title for that tab
- top-level tabs support expand/collapse in the sidebar only

## Non-Goals

This change will not:

- move tab or pane lifecycle ownership out of `MainWindow` / `TermPane`
- add pane closing, reordering, drag-and-drop, renaming, or context menus in the sidebar
- change PTY, rendering, or terminal emulation behavior in `qtghostty`
- add a new persistence model for session restoration

## Constraints

- Keep `src/libqtghostty/` reusable and terminal-focused only.
- Keep application-specific layout, menus, and tab/sidebar composition in `src/app/`.
- Reuse as much internal data as practical between horizontal and vertical tab modes.
- Avoid a broad model-layer rewrite; prefer small, local changes over a full architecture reset.

## Current State

Today `MainWindow` owns:

- `DTabBar *m_tabBar`
- `QStackedWidget *m_stackWidget`
- one `TermPane` per tab

Today `TermPane` owns:

- the pane split tree using nested `QSplitter`
- current terminal focus
- split / close split / focus navigation behavior
- title propagation from `TerminalWidget`

This means the current implementation stores most state implicitly in widgets. That is sufficient for the horizontal tab bar, but it is too coupled to drive both a horizontal tab bar and a vertical tree sidebar without duplication.

## Approach

Use a lightweight window-level tab record model and keep `TermPane` as the owner of pane structure.

The implementation will introduce:

- a small `TabRecord` structure in `MainWindow` for tab-level state shared by both horizontal and vertical presentations
- a small read-only `PaneInfo` snapshot structure in `TermPane` for pane-level state exposed to the window layer

This is intentionally a hybrid model:

- `MainWindow` owns tab metadata and layout mode state
- `TermPane` still owns real pane widgets, splitters, and focus behavior
- both the horizontal tab bar and the vertical sidebar render from the same `TabRecord`

This avoids duplicating tab state across two UIs without forcing a full pane-tree refactor.

## Data Model

### `TabRecord`

`MainWindow` will maintain one `TabRecord` per logical tab. Each record should contain only the state the window layer actually needs:

- stable tab identifier
- `TermPane *pane`
- current display title
- expanded/collapsed state for the vertical sidebar

The expanded/collapsed state belongs to `MainWindow`, not `TermPane`, because it is only a presentation detail of the sidebar.

### `PaneInfo`

`TermPane` will expose a lightweight snapshot for the sidebar and title synchronization logic. Each `PaneInfo` should include only minimal window-facing data:

- stable pane identifier
- display title
- whether this pane is currently active
- enough information to focus that pane again, either by:
  - carrying `TerminalWidget *terminal`, or
  - carrying an internal pane identifier that `TermPane` can resolve

The preferred direction is to expose a stable pane identifier plus an explicit `focusPane(...)` method so the window layer does not need to manipulate terminal widgets directly.

## UI Composition

### Horizontal Mode

Horizontal mode keeps the current structure:

- `DTitlebar` custom widget contains `DTabBar`
- central widget shows the existing content stack

This mode continues to use the same `TabRecord` list, but only the horizontal tab presentation is visible.

### Vertical Mode

Vertical mode changes composition only at the window layer:

- the titlebar custom widget becomes a compact placeholder widget with low height and horizontal padding
- the main content area becomes a two-column layout:
  - left: vertical sidebar
  - right: existing `QStackedWidget`
- the columns are separated by a splitter-style vertical divider

The right side remains the current stacked page area. Enabling vertical tabs must not create a second content stack or rebuild tabs.

### Sidebar Structure

The sidebar is an app-level tree-style navigation surface with two levels:

- level 1: one row per tab using `TabRecord`
- level 2: one row per pane using `TermPane::paneInfos()`

Behavior:

- clicking a tab row switches to that tab
- clicking a pane row switches to that tab and focuses that pane
- expanding/collapsing a tab only shows or hides its pane list in the sidebar

No extra pane actions will be shown in the sidebar for this change.

## Title And Focus Rules

The title source of truth remains the active pane inside each `TermPane`.

Rules:

- each tab title always matches that tab's active pane title
- the window title matches the active pane title of the current tab
- horizontal and vertical modes must show the same tab title for a given tab
- custom title renaming still applies to the active pane and therefore updates the tab title

This keeps the title model consistent across:

- `DTabBar`
- vertical sidebar top-level tab rows
- main window title

## Runtime Behavior

### Menu And Settings

Add a translatable checkable menu action:

- `tr("Vertical Tabs")`

The action will:

- initialize its checked state from `AppSettings`
- write back changes to `AppSettings`
- call a dedicated `MainWindow` layout switch method

The setting should persist across restarts because it is a layout preference, not a transient action.

### Layout Switching

`MainWindow` will own a method such as `setVerticalTabsEnabled(bool enabled)` that:

- switches visible window composition between horizontal and vertical modes
- preserves all existing tabs and `TermPane` instances
- preserves the current tab
- preserves the active pane inside each tab
- preserves vertical sidebar expansion state

Switching modes must not reconstruct `TermPane` or reset terminal state.

### Pane Structure Updates

`TermPane` will emit focused app-facing signals whenever the window layer needs to refresh sidebar or title state. The exact names can vary, but the responsibilities are:

- active pane changed
- pane list structure changed
- pane title changed

These signals should fire when relevant operations occur:

- split current pane
- close current split
- close other panes
- focus navigation between panes
- terminal title changes
- custom title changes

`MainWindow` consumes those signals to refresh:

- `TabRecord.currentTitle`
- horizontal tab text
- sidebar top-level tab text
- sidebar second-level pane rows
- window title for the current tab

## Class Responsibilities

### `MainWindow`

Will own:

- tab records
- layout mode switching
- sidebar widget creation and refresh
- menu action wiring
- settings integration for the vertical tabs preference

Will not own:

- pane split tree mutation logic
- terminal focus routing inside a tab beyond delegating to `TermPane`

### `TermPane`

Will continue to own:

- pane creation and removal
- nested splitter topology
- active pane tracking
- focus navigation and split commands

Will additionally expose:

- pane snapshot enumeration for the sidebar
- a way to focus a pane by identifier
- signals for pane structure / active pane / title changes

### `AppSettings`

Will gain:

- a persisted boolean preference for vertical tabs mode

It should provide a small, existing-style API rather than forcing callers to manipulate raw settings keys directly.

## Translation

The new menu item text must use `tr()` and be included in the existing translation workflow.

At minimum this change adds a new translatable string for:

- `Vertical Tabs`

Any new sidebar labels or placeholder strings introduced by the implementation should also use `tr()`.

## Testing

### `tests/test_main_window.cpp`

Add focused coverage for:

- default layout mode reflects the persisted setting
- toggling the menu action switches between horizontal and vertical layouts
- switching layout preserves current tab and current pane focus
- creating a tab adds a new top-level sidebar entry in vertical mode
- splitting the active pane adds a second-level pane entry
- closing a split removes the corresponding pane entry
- active pane title changes update:
  - window title
  - horizontal tab text
  - vertical tab row text
- expand/collapse only affects sidebar visibility and survives mode switches

### `tests/test_appsettings.cpp`

Add focused coverage for:

- default value of the vertical tabs setting
- writing and reading the vertical tabs preference

### Optional `TermPane` Coverage

If the new pane snapshot API is easy to test in isolation, add focused checks for:

- pane snapshot count after split operations
- active pane identifier after focus changes
- pane display title after terminal or custom title updates

## Risks

- `MainWindow` currently maps tabs to pages by index and `tabData`; introducing a shared tab record list must keep those mappings consistent during close and reorder-sensitive operations
- `TermPane` currently exposes only the current terminal; adding pane snapshots must avoid leaking too much internal widget structure
- title synchronization can become inconsistent if pane title changes, active pane changes, and tab switching are not normalized through one refresh path
- titlebar compacting may need minor iteration to align with DTK titlebar spacing behavior

## Implementation Notes

- Prefer introducing small helper methods instead of adding more index-walking logic inline in `MainWindow`.
- Keep the sidebar implementation in `src/app/`; do not push navigation widgets into `qtghostty`.
- The splitter-style divider can be implemented with Qt/DTK layout primitives; it does not need to become a general pane-management abstraction.
- Preserve existing shortcuts and menu behavior unless directly affected by the new layout mode.
- If a small app-level widget class is needed for the sidebar tree, keep it narrowly scoped to tab/pane navigation rather than building a generic tree framework.
