# deepin-terminal-ghostty — Agent Guide

## Project Overview

`deepin-terminal-ghostty` is a minimal terminal emulator built with **C++20** and **Qt6 Widgets**, embedding the `libghostty-vt` virtual terminal library from [Ghostty](https://ghostty.org). It is a proof-of-concept reference implementation showing how to integrate Ghostty's VT emulation and render-state APIs into a Qt application.

The project is intentionally small and focused: it opens a Qt window, spawns the user's shell in a PTY, renders terminal output via `QPainter`, accepts keyboard input (including Kitty keyboard protocol), handles window resize with terminal reflow, and supports scrollback scrolling.

**Status:** Minimal viable implementation. Functional for daily shell usage, with known limitations (see below).

## Technology Stack

- **Language:** C++20
- **Build System:** CMake >= 3.16
- **UI Framework:** Qt6 Widgets, DTK6 (Deepin Tool Kit)
- **Terminal Engine:** `libghostty-vt` (Ghostty C API)
- **UI Toolkit:** DTK6 (`DApplication`, `DMainWindow`, `DGuiApplicationHelper` theming)
- **Platform:** Linux only (PTY layer uses `forkpty`)

## Project Structure

```
.
├── CMakeLists.txt          # Build configuration with Ghostty auto-discovery
├── lib/
│   └── libghostty-vt.so    # libghostty-vt runtime library (prebuilt)
├── src/
│   ├── libqtghostty/       # Qt wrapper library around libghostty-vt
│   │   ├── PtySession.h/.cpp   # PTY lifecycle: forkpty, non-blocking I/O, child reaping
│   │   └── TerminalWidget.h/.cpp # Ghostty VT integration, QPainter rendering, input encoding
│   └── app/
│       ├── main.cpp        # DApplication entry point
│       ├── MainWindow.h    # DMainWindow with DTabBar + QStackedWidget
│       └── MainWindow.cpp  # Multi-tab terminal window implementation
├── tests/
│   ├── CMakeLists.txt      # Test configuration
│   ├── test_pty_session.cpp    # PtySession unit tests
│   └── test_terminal_widget.cpp # TerminalWidget unit tests
└── docs/
    ├── superpowers/        # Design specs and implementation plans (agent workspace)
    └── research/           # Technical research reports
        └── 2026-04-21-gpu-rendering-feasibility.md
```

There are no other configuration files (no `pyproject.toml`, `package.json`, `Cargo.toml`, etc.). The entire build is driven by `CMakeLists.txt`.

## Build Commands

### Prerequisites

On Debian/Ubuntu:

```bash
sudo apt install cmake qt6-base-dev build-essential binutils libdtk6widget-dev libdtk6core-dev libdtk6gui-dev
```

You also need Ghostty C headers (`ghostty/vt/*.h`). The build system auto-detects them from common sibling paths, or you can set `GHOSTTY_INCLUDE_DIR` explicitly.

### Configure and Build

```bash
cmake -B build
cmake --build build
```

The build system will:

1. Auto-locate Ghostty headers from sibling paths (`../../g/ghostty/include`, `../ghostty/include`, `./ghostty/include`) or from the `GHOSTTY_INCLUDE_DIR` CMake cache variable / environment variable.
2. Stage headers into the build directory (`build/ghostty-include/`).
3. Detect the SONAME of `lib/libghostty-vt.so` using `readelf` and stage the library alongside the executable with correct runtime linking.
4. Set `RPATH` to `$ORIGIN` so the executable finds the staged shared library at runtime.

### Run

```bash
./build/deepin-terminal-ghostty
```

## Runtime Architecture

The project produces two artifacts: a shared library (`libqtghostty`) and an executable (`deepin-terminal-ghostty`). The application is a single-process, single-threaded Qt Widgets program built on top of `libqtghostty`:

### libqtghostty (`src/libqtghostty/`)

A Qt6 wrapper around `libghostty-vt` providing reusable terminal components:

### 1. PtySession (`src/libqtghostty/PtySession.cpp`)

A `QObject` that wraps the Linux PTY APIs:

- Spawns the user's shell via `forkpty()` — resolving `$SHELL` → `passwd` entry → `/bin/sh`.
- Sets the PTY master fd to non-blocking mode and integrates it into the Qt event loop via `QSocketNotifier`.
- Provides `write(QByteArray)` with backpressure-safe buffering (max 1 MiB pending).
- Applies `TIOCSWINSZ` when terminal dimensions change.
- Handles graceful child shutdown (`SIGHUP` → wait → `SIGKILL`) and emits `dataReceived(QByteArray)` and `sessionClosed()` signals.

### 2. TerminalWidget (`src/libqtghostty/TerminalWidget.cpp`)

A `QWidget` that owns the full Ghostty VT stack:

| Ghostty Handle | Purpose |
|---------------|---------|
| `GhosttyTerminal` | Core VT state machine — parses escape sequences, maintains screen/cursor/styles |
| `GhosttyRenderState` | Snapshot of the terminal screen optimized for rendering |
| `GhosttyRenderStateRowIterator` + `RowCells` | Iterates dirty rows and cells to draw |
| `GhosttyKeyEncoder` + `KeyEvent` | Encodes Qt key events into VT escape sequences |

**Rendering pipeline** (`paintEvent`):

1. `ghostty_render_state_update()` — snapshot terminal state, consume dirty flags.
2. `ghostty_render_state_colors_get()` — resolve default fg/bg and palette.
3. Iterate rows → iterate cells → draw background rectangles + foreground text.
4. Draw a semi-transparent block cursor when focused.
5. Reset dirty flags for the next frame.

**Input pipeline** (`keyPressEvent`):

1. Map `Qt::Key` → `GhosttyKey`.
2. Build `GhosttyKeyEvent` with modifiers, unshifted codepoint, and UTF-8 text.
3. `ghostty_key_encoder_setopt_from_terminal()` — sync encoder to terminal modes.
4. Encode → write bytes to PTY.

**Effects callbacks** (C-linkage friends registered on the terminal):

- `write_pty` — forwards query responses back to the PTY.
- `size` — reports current cell dimensions for XTWINOPS queries.
- `device_attributes` — reports VT220 conformance so apps like vim/htop can probe capabilities.
- `xtversion` — reports `"deepin-terminal-ghostty"`.
- `title_changed` — emits `terminalTitleChanged()` signal to update the window title.
- `color_scheme` — returns false (no OS scheme query implemented yet).

### deepin-terminal-ghostty (`src/app/MainWindow.cpp`)

The demo application. `MainWindow` (a `DMainWindow`) embeds a `DTabBar` into the DTK titlebar and uses a `QStackedWidget` to host multiple `TerminalWidget` instances:

- **New tab**: Click the "+" button on the tab bar (or via `DTabBar::tabAddRequested`).
- **Switch tab**: Click a tab; the corresponding `TerminalWidget` is brought to the front via `QStackedWidget::setCurrentIndex`.
- **Close tab**: Click the "×" on a tab; the page is removed from the stack and destroyed.
- **Title sync**: Each `TerminalWidget` emits `terminalTitleChanged`; `MainWindow` updates both the tab text and the window title for the active tab.
- **Titlebar icon**: `titlebar()->setIcon(QIcon::fromTheme("utilities-terminal"))` sets the logo in the top-left corner.

## Code Style Guidelines

- **C++ Standard:** C++20 (`CMAKE_CXX_STANDARD 20`).
- **Qt Patterns:** Use Qt's signal/slot system, `QPointer` for guarded pointers, and `QSocketNotifier` for fd integration.
- **Naming:**
  - Class names: `PascalCase` (e.g., `TerminalWidget`).
  - Member variables: `m_camelCase` (e.g., `m_masterFd`, `m_ptySession`).
  - Constants: `kCamelCase` (e.g., `kChildPollIntervalMs`).
  - Free functions in anonymous namespaces: `camelCase`.
- **Comments:** Code comments are in English. Complex logic (e.g., child shutdown, write buffering) includes inline explanatory comments.
- **Ghostty API usage:**
  - Always check `GhosttyResult` for `GHOSTTY_SUCCESS` before proceeding.
  - Free Ghostty handles in reverse creation order in destructors.
  - `GHOSTTY_INIT_SIZED` is used to zero-initialize structs before passing them to getters.
- **Emit macro workaround:** Because Qt defines `emit` as a no-op macro and Ghostty headers use `emit` as a struct member name, `TerminalWidget.h` explicitly `#undef emit` before including `<ghostty/vt.h>`.

## Testing Strategy

The project uses **Qt Test** (`Qt6::Test`) for automated unit tests. Tests are located in `tests/` and registered with CTest.

### Running Tests

```bash
cmake -B build
cmake --build build
cd build && ctest --output-on-failure
```

Or run individual test binaries directly:

```bash
./build/tests/test_pty_session
./build/tests/test_terminal_widget
```

### Test Coverage

| Test File | Target | Key Scenarios |
|-----------|--------|--------------|
| `test_pty_session.cpp` | `PtySession` | Start shell, write/read I/O, resize, session close signal |
| `test_terminal_widget.cpp` | `TerminalWidget` | Initialize, size report, OSC title sequence, grid sizing |

### Manual verification targets

- Configure and build with CMake successfully.
- Launch the demo: `./build/deepin-terminal-ghostty`.
- Confirm shell prompt appears.
- Confirm printable input echoes correctly.
- Confirm Enter, Backspace, and arrow keys behave correctly in a shell.
- Confirm resizing the window updates terminal size without crashing.

**Future testing:** A smoke test (`GHOSTTY_QT_SMOKE=1` auto-exit path) is described in `docs/superpowers/plans/` but not yet integrated into `CMakeLists.txt`.

## Security Considerations

- The PTY layer uses `forkpty()` and `execve()` with the user's shell. No privilege escalation is involved.
- The PTY master fd is set to `O_NONBLOCK` and `FD_CLOEXEC`.
- Child shutdown sends `SIGHUP` to the process group, then `SIGKILL` after a grace period (300 ms). Destruction uses a blocking shutdown with the same grace period.
- Write buffering is capped at 1 MiB to prevent unbounded memory growth if the PTY consumer stalls.
- `QPointer` guards are used when emitting signals that could destroy the emitter (e.g., `dataReceived` → `update()` path).

## Known Limitations

This is intentionally a **minimal** implementation. Notable gaps:

- **Wide characters (CJK)** are drawn as single-width cells — may be truncated or overlap.
- **Mouse event forwarding** is not implemented (wheel only scrolls scrollback history).
- **Kitty Graphics Protocol** images are not rendered.
- **Copy/paste** and **text selection** are not implemented.
- **Font fallback** for missing glyphs relies on Qt's default behavior.
- **24-bit true color** cells work, but bold/italic rendering is basic (`QFont` weight/slant only).
- **No GPU rendering** — everything is CPU-rendered via `QPainter`.
- **No tabs, splits, settings UI, or session persistence.**

## Reference Code Locations

When extending this project, the following local codebases are the primary references:

| Project | Local Path | What to reference |
|---------|-----------|-------------------|
| **Ghostty** (source + C headers) | `~/projects/g/ghostty` | C API definitions: `zig-out/include/ghostty/vt/*.h` — terminal, render, key, mouse, focus, modes, device, size_report, style, color, etc. |
| **Ghostling** (macOS reference impl) | `~/projects/g/ghostling` | `main.c` — complete raylib-based reference showing PTY setup, render loop, input handling (keyboard + mouse), effects callbacks, and Kitty graphics |

These paths are expected to exist on the development machine. The build system already auto-detects Ghostty headers from `~/projects/g/ghostty/include` (via the `../../g/ghostty/include` sibling search).

## Dependency: Ghostty Headers and Library

The build requires:

1. `lib/libghostty-vt.so` — a prebuilt shared library checked into the repository.
2. Ghostty C headers (`ghostty/vt.h`, `ghostty/vt/build_info.h`, etc.) — **not** checked in. The build system searches for them at configure time.

To explicitly specify the header location:

```bash
cmake -B build -DGHOSTTY_INCLUDE_DIR=/path/to/ghostty/include
```

Or via environment variable:

```bash
export GHOSTTY_INCLUDE_DIR=/path/to/ghostty/include
cmake -B build
```

## Deployment Notes

- The executable expects both `libqtghostty.so` and `libghostty-vt.so.*` (with the correct SONAME) to be in the same directory at runtime, because `RPATH` is set to `$ORIGIN`.
- The build stages both libraries into the build directory automatically via `POST_BUILD` custom commands.
- No install target is currently defined.
