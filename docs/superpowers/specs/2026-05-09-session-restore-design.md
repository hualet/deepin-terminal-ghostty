# Session Restore Design

## Goal

When the user closes the terminal window, save the full session state (tabs,
splits, terminal content with colors and styles). On next launch, ask whether
to restore the previous session.

## Requirements

- Save on window close (not periodic)
- Full restore: layout (tabs + splits), working directory, terminal content
  with VT formatting (colors, styles, cursor, scrollback history)
- Launch dialog asking user whether to restore
- Running processes are not restored (accepted limitation)

## Storage Layout

```
~/.config/deepin-terminal-ghostty/session/
├── snapshot.json          # layout + metadata
└── terminals/
    ├── <uuid-1>.vt        # terminal 1 VT sequence content
    ├── <uuid-2>.vt        # terminal 2 VT sequence content
    └── ...
```

Each save overwrites the previous snapshot (only one snapshot retained).

## Snapshot Format

### snapshot.json

```json
{
  "version": 1,
  "timestamp": "2026-05-09T14:30:00Z",
  "window": {
    "width": 1200,
    "height": 800,
    "isMaximized": false
  },
  "tabs": [
    {
      "id": 1,
      "title": "user@host: ~/projects",
      "pane": {
        "type": "terminal",
        "uuid": "uuid-1",
        "workingDirectory": "/home/user/projects",
        "title": "vim main.cpp"
      }
    },
    {
      "id": 2,
      "title": "user@host: ~",
      "pane": {
        "type": "split",
        "orientation": "horizontal",
        "sizes": [500, 500],
        "children": [
          {
            "type": "terminal",
            "uuid": "uuid-2",
            "workingDirectory": "/home/user/projects",
            "title": "bash"
          },
          {
            "type": "terminal",
            "uuid": "uuid-3",
            "workingDirectory": "/home/user",
            "title": "top"
          }
        ]
      }
    }
  ]
}
```

The pane tree is recursive: each node is either a `"terminal"` leaf or a
`"split"` inner node with `orientation` (horizontal/vertical), `sizes`
(pixel widths/heights), and `children`.

### VT content files

Each `<uuid>.vt` file contains the full terminal content exported via
`ghostty_formatter_format_alloc` with `GHOSTTY_FORMATTER_FORMAT_VT` format
and all `GhosttyFormatterTerminalExtra` flags enabled (cursor, style,
hyperlink, protection, kitty_keyboard, charsets, palette, modes,
scrolling_region, tabstops, pwd, keyboard).

This includes scrollback history, so the restored terminal shows the same
visible content and can be scrolled back.

## Components

### TerminalWidget API additions (`src/libqtghostty/TerminalWidget.h/.cpp`)

Two new public methods that encapsulate all Ghostty formatter details:

```cpp
QByteArray exportVtContent() const;              // export full VT state
void importVtContent(const QByteArray &data);    // replay VT state into terminal
```

`exportVtContent()`:
1. Create `ghostty_formatter_terminal_new` with `GHOSTTY_FORMATTER_FORMAT_VT`
2. Enable all `GhosttyFormatterTerminalExtra` flags
3. Call `ghostty_formatter_format_alloc` to get output
4. Return as `QByteArray`

`importVtContent()`:
1. Write data into terminal via `ghostty_terminal_vt_write`
2. Mark render state dirty

This keeps the qtghostty/app boundary clean: `app` never touches Ghostty
handles directly.

### SessionSnapshot (`src/app/SessionSnapshot.h/.cpp`)

Pure data model:

```
TerminalSnapshot { uuid, workingDirectory, title }
SplitNode        { variant: terminal | split(orientation, sizes, children) }
TabSnapshot      { id, title, rootPane: SplitNode }
WindowSnapshot   { width, height, isMaximized, tabs: list<TabSnapshot> }
```

Provides `toJson()` / `fromJson()` for serialization.

### SessionManager (`src/app/SessionManager.h/.cpp`)

Coordinates save and restore:

- `save(const WindowSnapshot &snapshot, ...)`:
  - Clear old snapshot directory
  - Write `snapshot.json`
  - For each terminal, call `exportVtContent()` and write to `terminals/<uuid>.vt`

- `hasSnapshot()`: check if `snapshot.json` exists

- `loadSnapshot()`: read and parse `snapshot.json`

- `readVtContent(const QString &uuid)`: read VT file for a terminal

- `clearSnapshot()`: remove snapshot directory

- `snapshotTimestamp()`: return timestamp for display in restore dialog

### MainWindow changes (`src/app/MainWindow.cpp`)

**Save (on close):**
- In `closeEvent()`, after user confirms close, before widget destruction:
  1. Collect current state into `WindowSnapshot`
  2. Walk each tab's TermPane to build the split tree, collecting each
     terminal's UUID, working directory, title, and VT content
  3. Call `SessionManager::save()`

**Restore (on launch):**
- After `MainWindow` is constructed:
  1. Check `SessionManager::hasSnapshot()`
  2. If snapshot exists, show `DDialog`:
     - Title: "恢复上次会话"
     - Body: "检测到上次关闭时的终端会话，是否恢复？"
     - Buttons: "恢复会话" / "新建终端"
  3. On "恢复会话": call restore flow
  4. On "新建终端": clear snapshot, proceed with default single tab

**Restore flow:**
1. Load `WindowSnapshot` from `SessionManager`
2. Apply window geometry (size, maximized state)
3. For each tab in snapshot:
   a. Create a new `TermPane` with first terminal's working directory
   b. If tab has splits, call `TermPane::splitAt()` (or equivalent) to
      recreate the split tree with correct orientation and sizes
   c. For each terminal in the tab, call `importVtContent()` with the
      saved VT data after the terminal is initialized
4. After all tabs restored, activate the first tab

### TermPane changes (`src/app/TermPane.h/.cpp`)

Need a way to introspect the current split tree and recreate one from data.

**New methods for save:**
- `buildSplitTree() const`: walk QSplitter tree, return `SplitNode`
- `terminalsInLayoutOrder() const`: same as `terminalsInVisualOrder()` but
  with stable ordering matching the tree structure

**New methods for restore:**
- `restoreFromSplitTree(const SplitNode &node)`: build the QSplitter tree
  from the saved layout, creating terminals with specified working directories

## Edge Cases

- **No tabs on close**: skip saving, next launch starts fresh
- **Snapshot write failure**: log warning, continue close without saving
- **Corrupted snapshot**: `SessionManager::loadSnapshot()` returns empty,
  fall back to fresh start
- **VT content too large**: no explicit size limit; large scrollback
  histories produce large files but this matches user expectations
- **Restore when terminal count changed externally**: each terminal is
  independent; if one VT file is missing, that terminal starts with a fresh
  shell in the correct working directory

## Post-Restore User Experience

After VT content is replayed into each terminal:
- The screen shows the previous session's content with full formatting
- Scrollback history is available
- The shell prompt appears after the restored content
- User presses Enter to get a fresh prompt and resume work
- Previous content remains scrollable and copyable

## Testing Strategy

- Unit tests for `SessionSnapshot` serialization/deserialization
- Unit tests for `SessionManager` save/load with temp directories
- Integration test: create terminals with known content, save snapshot,
  load in a new MainWindow, verify terminal content matches
- Test with single tab, multiple tabs, splits (nested and flat)
- Test restore dialog acceptance and rejection
- Test corrupted/missing snapshot files
