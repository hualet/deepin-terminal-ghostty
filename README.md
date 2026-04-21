# deepin-terminal-ghostty

A minimal terminal emulator built with **C++20 / Qt6 Widgets / DTK6** and the **libghostty-vt** virtual terminal library extracted from [Ghostty](https://ghostty.org).

This is a proof-of-concept reference implementation showing how to embed `libghostty-vt` into a Qt application. It supports PTY shell sessions, VT sequence parsing, incremental render-state based drawing, keyboard encoding (including Kitty keyboard protocol), focus events, and scrollback scrolling.

> **Status:** Minimal viable implementation. Functional for daily shell usage, with known limitations listed below.

---

## Prerequisites

- Linux (PTY layer uses `forkpty`)
- CMake >= 3.16
- Qt6 Widgets development packages
- DTK6 development packages (`libdtk6widget-dev`, `libdtk6core-dev`, `libdtk6gui-dev`)
- Ghostty C headers (`ghostty/vt/*.h`) — the build system auto-detects them from common locations
- `readelf` (for auto-detecting `libghostty-vt` SONAME)

On Debian/Ubuntu:

```bash
sudo apt install cmake qt6-base-dev build-essential binutils libdtk6widget-dev libdtk6core-dev libdtk6gui-dev
```

## Build

```bash
cmake -B build
cmake --build build
```

The build system will:

1. Auto-locate Ghostty headers from sibling paths (`../../g/ghostty/include`, `../ghostty/include`, etc.) or from `GHOSTTY_INCLUDE_DIR`.
2. Stage headers into the build directory for clean include paths.
3. Detect the SONAME of `lib/libghostty-vt.so` and stage it alongside the executable with correct runtime linking.

To run:

```bash
./build/deepin-terminal-ghostty
```

## Project Structure

```
.
├── CMakeLists.txt          # Build configuration with Ghostty auto-discovery
├── lib/
│   └── libghostty-vt.so    # libghostty-vt runtime library
├── src/
│   ├── libqtghostty/       # Qt wrapper library around libghostty-vt
│   │   ├── PtySession.h/.cpp   # PTY lifecycle: forkpty, non-blocking I/O, child reaping
│   │   └── TerminalWidget.h/.cpp # Ghostty VT integration, QPainter rendering, input encoding
│   └── app/
│       ├── main.cpp        # Application entry point
│       ├── MainWindow.h    # DMainWindow with DTabBar + QStackedWidget
│       └── MainWindow.cpp  # Multi-tab terminal window implementation
└── docs/
    └── superpowers/        # Design plans and specs (agent workspace)
```

## Architecture

The project is split into a reusable Qt library (`libqtghostty`) and a minimal demo application.

### libqtghostty

#### PtySession

Wraps `forkpty()` into a Qt-friendly `QObject`:

- Spawns the user's shell (`$SHELL` → passwd entry → `/bin/sh`)
- Reads the PTY master fd via `QSocketNotifier` (non-blocking)
- Writes to the PTY with backpressure-safe buffering
- Handles graceful + forced child shutdown (`SIGHUP` → `SIGKILL`)
- Emits `dataReceived(QByteArray)` and `sessionClosed()` signals

#### TerminalWidget

A `QWidget` that owns the full Ghostty VT stack:

| Ghostty Handle | Purpose |
|---------------|---------|
| `GhosttyTerminal` | Core VT state machine — parses escape sequences, maintains screen/cursor/styles |
| `GhosttyRenderState` | Snapshot of the terminal screen optimized for rendering |
| `GhosttyRenderStateRowIterator` + `RowCells` | Iterates dirty rows and cells to draw |
| `GhosttyKeyEncoder` + `KeyEvent` | Encodes Qt key events into VT escape sequences |

**Rendering pipeline** (`paintEvent`):

1. `ghostty_render_state_update()` — snapshot terminal state, consume dirty flags
2. `ghostty_render_state_colors_get()` — resolve default fg/bg and palette
3. Iterate rows → iterate cells → draw background rectangles + foreground text
4. Draw a semi-transparent block cursor when focused
5. Reset dirty flags for the next frame

**Input pipeline** (`keyPressEvent`):

1. Map `Qt::Key` → `GhosttyKey`
2. Build `GhosttyKeyEvent` with modifiers, unshifted codepoint, and UTF-8 text
3. `ghostty_key_encoder_setopt_from_terminal()` — sync encoder to terminal modes
4. Encode → write bytes to PTY

**Effects callbacks** (registered on the terminal):

- `write_pty` — forwards query responses back to the PTY
- `size` — reports current cell dimensions for XTWINOPS queries
- `device_attributes` — reports VT220 conformance so apps like vim/htop can probe capabilities
- `xtversion` — reports `"deepin-terminal-ghostty"`
- `title_changed` — emits `terminalTitleChanged()` signal to update the window title
- `color_scheme` — returns false (no OS scheme query implemented yet)

### deepin-terminal-ghostty (`src/app/MainWindow.cpp`)

A minimal demo application that links against `libqtghostty`. `MainWindow` (a `DMainWindow`) embeds a `DTabBar` into the DTK titlebar and uses a `QStackedWidget` to host multiple `TerminalWidget` instances:

- **New tab**: Click the "+" button on the tab bar.
- **Switch tab**: Click a tab; the corresponding `TerminalWidget` is brought to the front.
- **Close tab**: Click the "×" on a tab.
- **Title sync**: Each `TerminalWidget` emits `terminalTitleChanged`; the active tab text and window title are updated accordingly.
- **Titlebar icon**: `titlebar()->setIcon(QIcon::fromTheme("utilities-terminal"))` sets the logo in the top-left corner.

## Known Limitations

This is intentionally a **minimal** implementation. Notable gaps:

- **Wide characters (CJK)** are drawn as single-width cells — may be truncated or overlap
- **Mouse event forwarding** is not implemented (wheel only scrolls scrollback history)
- **Kitty Graphics Protocol** images are not rendered
- **Copy/paste** and **selection** are not implemented
- **Font fallback** for missing glyphs relies on Qt's default behavior
- **24-bit true color** cells work, but bold/italic rendering is basic (QFont weight/slant only)

## License

LGPL-3.0-or-later
