# deepin-terminal-ghostty — Agent Guide

## Project Overview

`deepin-terminal-ghostty` is a minimal terminal emulator built with **C++20** and **Qt6 Widgets**, embedding the `libghostty-vt` virtual terminal library from [Ghostty](https://ghostty.org). It is a proof-of-concept reference implementation showing how to integrate Ghostty's VT emulation and render-state APIs into a Qt application.

The project is intentionally small and focused: it opens a Qt window, spawns the user's shell in a PTY, renders terminal output via `QPainter`, accepts keyboard input (including Kitty keyboard protocol), handles window resize with terminal reflow, and supports scrollback scrolling.

**Status:** Minimal viable implementation. Functional for daily shell usage, with known limitations (see below).

## Technology Stack

- **Language:** C++20
- **Build System:** CMake >= 3.16
- **UI Framework:** Qt6 Widgets
- **Terminal Engine:** `libghostty-vt` (Ghostty C API)
- **Platform:** Linux only (PTY layer uses `forkpty`)

## Project Structure

```
.
├── CMakeLists.txt          # Build configuration with Ghostty auto-discovery
├── lib/
│   └── libghostty-vt.so    # libghostty-vt runtime library (prebuilt)
├── src/
│   ├── main.cpp            # QApplication entry point, top-level window
│   ├── PtySession.h/.cpp   # PTY lifecycle: forkpty, non-blocking I/O, child reaping
│   └── TerminalWidget.h/.cpp # Ghostty VT integration, QPainter rendering, input encoding
└── docs/
    └── superpowers/        # Design specs and implementation plans (agent workspace)
```

There are no other configuration files (no `pyproject.toml`, `package.json`, `Cargo.toml`, etc.). The entire build is driven by `CMakeLists.txt`.

## Build Commands

### Prerequisites

On Debian/Ubuntu:

```bash
sudo apt install cmake qt6-base-dev build-essential binutils
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

The application is a single-process, single-threaded Qt Widgets program with three layers:

### 1. PtySession (`src/PtySession.cpp`)

A `QObject` that wraps the Linux PTY APIs:

- Spawns the user's shell via `forkpty()` — resolving `$SHELL` → `passwd` entry → `/bin/sh`.
- Sets the PTY master fd to non-blocking mode and integrates it into the Qt event loop via `QSocketNotifier`.
- Provides `write(QByteArray)` with backpressure-safe buffering (max 1 MiB pending).
- Applies `TIOCSWINSZ` when terminal dimensions change.
- Handles graceful child shutdown (`SIGHUP` → wait → `SIGKILL`) and emits `dataReceived(QByteArray)` and `sessionClosed()` signals.

### 2. TerminalWidget (`src/TerminalWidget.cpp`)

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

### 3. main.cpp

Creates `QApplication`, a `QMainWindow`, instantiates `TerminalWidget`, and connects the title-changed signal to the window title.

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

**Current state:** The repository has no automated test harness. Verification is manual.

**Manual verification targets:**

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

- The executable expects `libghostty-vt.so.*` (with the correct SONAME) to be in the same directory at runtime, because `RPATH` is set to `$ORIGIN`.
- The build stages the library into the build directory automatically via a `POST_BUILD` custom command.
- No install target is currently defined.
