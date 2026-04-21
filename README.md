# deepin-terminal-ghostty

A **feature-rich terminal emulator** for Linux built with **C++20 / Qt6 Widgets / DTK6** and the **libghostty-vt** virtual terminal library from [Ghostty](https://ghostty.org). It aims to deliver a high-performance terminal experience deeply integrated with the Deepin desktop environment.

The project provides PTY shell sessions, full VT sequence parsing, incremental render-state based drawing, keyboard encoding (including Kitty keyboard protocol), DTK-native window chrome with multi-tab support, focus events, and scrollback scrolling. The rendering layer is currently CPU-based (`QPainter`) with a planned migration to GPU rendering.

> **Status:** Active development. Core VT, PTY, multi-tab UI, and DTK theming are functional. See the Roadmap section below for upcoming features.

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

## Tests

```bash
cmake -B build
cmake --build build
cd build && ctest --output-on-failure
```

Individual test binaries:

```bash
./build/tests/test_pty_session
./build/tests/test_terminal_widget
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
├── tests/
│   ├── CMakeLists.txt      # Test configuration
│   ├── test_pty_session.cpp    # PtySession unit tests
│   └── test_terminal_widget.cpp # TerminalWidget unit tests
└── docs/
    ├── superpowers/        # Design plans and specs (agent workspace)
    └── research/           # Technical research reports
        └── 2026-04-21-gpu-rendering-feasibility.md
```

## Architecture

The project is split into a reusable Qt library (`libqtghostty`) and the terminal application (`deepin-terminal-ghostty`).

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

The main terminal application. `MainWindow` (a `DMainWindow`) embeds a `DTabBar` into the DTK titlebar and uses a `QStackedWidget` to host multiple `TerminalWidget` instances:

- **New tab**: Click the "+" button on the tab bar.
- **Switch tab**: Click a tab; the corresponding `TerminalWidget` is brought to the front.
- **Close tab**: Click the "×" on a tab.
- **Title sync**: Each `TerminalWidget` emits `terminalTitleChanged`; the active tab text and window title are updated accordingly.
- **Titlebar icon**: `titlebar()->setIcon(QIcon::fromTheme("utilities-terminal"))` sets the logo in the top-left corner.

## Roadmap

| Feature | Status | Notes |
|---------|--------|-------|
| Core VT emulation | ✅ Done | Via `libghostty-vt` |
| PTY session management | ✅ Done | `forkpty`, non-blocking I/O, graceful child shutdown |
| Incremental CPU rendering | ✅ Done | Dirty-row tracking with `QPainter` |
| Kitty keyboard protocol | ✅ Done | Full key event encoding |
| DTK6 native chrome | ✅ Done | `DMainWindow`, `DApplication`, light/dark theme |
| Multi-tab support | ✅ Done | `DTabBar` + `QStackedWidget` |
| Focus events & scrollback | ✅ Done | — |
| Unit tests | ✅ Done | `Qt Test` + CTest |
| **GPU rendering** | 🚧 Planned | OpenGL/Vulkan migration (see `docs/research/`) |
| CJK wide characters | 🚧 Planned | Proper double-width cell handling |
| Mouse event forwarding | 🚧 Planned | Full mouse reporting to terminal apps |
| Kitty Graphics Protocol | 🚧 Planned | Inline image rendering |
| Copy/paste & selection | 🚧 Planned | Clipboard integration and mouse selection |
| Font fallback (fontconfig) | 🚧 Planned | Beyond Qt default behavior |
| Bold/italic font variants | 🚧 Planned | Load matching font faces |
| Window splits | 🚧 Planned | Horizontal/vertical panes |
| Settings UI | 🚧 Planned | Preferences dialog |
| Session persistence | 🚧 Planned | Restore tabs on restart |

## License

LGPL-3.0-or-later
