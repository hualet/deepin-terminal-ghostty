# Manual Session Restore

Date: 2026-05-20

## Problem

Current session restore has two pain points:

1. With `auto` mode, every new window immediately shows the previous session's terminal output, which is jarring.
2. The `ask` mode adds an unwanted dialog on every launch.

Neither mode matches the workflow of a terminal launched from a desktop menu or shortcut, where the user typically wants a blank terminal and may choose to restore later.

## Design

### Behavior Model

Replace the default restore behavior with a Chrome-style "restore previous session" pattern:

- **Auto-save on close** remains unchanged (when `sessionRestore` setting is enabled).
- **New windows always open blank** — no dialog, no automatic restore.
- **User restores explicitly** via hamburger menu item or `Ctrl+Shift+R` shortcut.

### Settings Change

Add a third value to `sessionRestoreBehavior`:

| Value | Behavior |
|---|---|
| `"ask"` | Show dialog asking whether to restore (existing) |
| `"auto"` | Automatically restore on launch (existing) |
| `"manual"` | New window is blank; restore via menu/shortcut (new default) |

The default changes from `"ask"` to `"manual"`. Existing users who explicitly chose `"ask"` or `"auto"` keep their preference.

Settings UI: add "Manual (menu triggered)" option to the restore behavior dropdown.

### New Window Behavior

In `MainWindow` constructor, when `sessionRestoreBehavior == "manual"`:

- Create a single blank tab (same as if no snapshot exists).
- Do not delete the saved snapshot — it remains available for manual restore.

### Restore Operation

New private method `MainWindow::restorePreviousSession()`:

1. Check `SessionManager::hasSnapshot()`. If no snapshot exists, show a brief notification and return.
2. Load `WindowSnapshot` from `SessionManager`.
3. **Always append** snapshot tabs after existing tabs. This avoids the risk of incorrectly detecting a "blank" window or triggering the last-tab-closes-window path via `onTabCloseRequested()`. The restored tabs simply appear after whatever the user already has open.
4. For each tab: call existing `TermPane::restoreFromSplitTree()` + `TerminalWidget::importVtContent()`.
5. Activate the first restored tab.

**Why always append:** Using `tabCount() == 1` to detect a "blank" window is unreliable — the user may have already started working in their only tab. Calling `onTabCloseRequested(0)` on the last tab triggers `close()` (see `MainWindow.cpp:505-508`). Even if we added a forced removal path, the risk of destroying user work outweighs the minor convenience of not seeing the initial tab. Append is always safe.

### Menu and Shortcut

**Menu item** in the hamburger menu (title bar menu):

- Label: "Restore Previous Session"
- Position: between "Vertical Tabs" and "Remote Management", preceded by a separator.
- State: disabled (grayed out) when no snapshot is available.
- Uses `m_restoreSessionAction` member to track enabled state.

**Shortcut**: `Ctrl+Shift+R` — distinct from the existing `Ctrl+Shift+T` (New Tab) shortcut.

**Shortcut registration**: via `QShortcut` or `addAction` with `QKeySequence`, connected to `restorePreviousSession()`.

### Snapshot Availability Tracking

- After `closeEvent` saves a snapshot: nothing special needed (snapshot exists on disk).
- After `restorePreviousSession()` completes: snapshot remains on disk (user can restore again in another window).
- `SessionManager::hasSnapshot()` already checks for `snapshot.json` existence.

Update `m_restoreSessionAction` enabled state:
- Enabled when `sessionRestore` setting is on AND `SessionManager::hasSnapshot()`.
- Remains enabled after a successful restore — the snapshot is not deleted, so the user can restore again in the same or another window.
- No need for dynamic updates — the menu is rebuilt on each window creation, and snapshots only appear after a previous window closes.

### Files Changed

| File | Change |
|---|---|
| `src/app/AppSettings.h/.cpp` | Add `"manual"` to behavior enum handling |
| `src/app/settings/default-config.json` | Change default to `"manual"` |
| `src/app/settings/settings_translation.cpp` | Add i18n string for "Manual (menu triggered)" |
| `src/app/MainWindow.h` | Declare `restorePreviousSession()` and `m_restoreSessionAction` |
| `src/app/MainWindow.cpp` | Constructor: handle `"manual"` behavior; add menu item + shortcut; implement `restorePreviousSession()` |

No changes to `qtghostty` library layer or `SessionManager`.

### Edge Cases

- **Multiple windows open simultaneously**: last window to close saves the snapshot (existing behavior). Restore in any window restores that snapshot.
- **Restore when snapshot is corrupt or VT files missing**: existing graceful handling applies — affected terminals start with a fresh shell in the saved working directory.
- **Repeated restores**: each invocation appends the same snapshot content as new tabs. No snapshot deletion after restore.
- **Session restore setting disabled**: snapshot is cleared on close (existing behavior), so `restorePreviousSession()` will find no snapshot and show the notification.
