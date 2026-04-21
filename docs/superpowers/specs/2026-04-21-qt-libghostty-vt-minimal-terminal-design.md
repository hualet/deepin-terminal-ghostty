# Qt libghostty-vt Minimal Terminal Design

**Date:** 2026-04-21

**Goal**

Build a minimal runnable terminal emulator for this repository using C++/Qt and `libghostty-vt`, modeled after the scope of `ghostling` but adapted to Qt. The result should open a Qt window, spawn the user's shell in a PTY, render terminal output, accept basic keyboard input, and handle window resize with terminal reflow.

**Non-Goals**

- GPU rendering in the first iteration
- Tabs, splits, settings UI, session persistence
- Mouse protocol, selection, clipboard, IME, search, scrollbars, or Kitty graphics UI
- Full font shaping or fallback font management beyond Qt defaults

**Constraints**

- Use the local Ghostty checkout at `/home/hualet/projects/g/ghostty` as the source of headers and library expectations.
- Use the existing local `libghostty-vt.so` in the current repository as the runtime library for the first iteration.
- Follow the `ghostling` integration model where the embedder owns PTY, windowing, input, and drawing, and `libghostty-vt` owns terminal emulation and render-state extraction.
- Keep the first implementation small enough to verify the integration boundary before any renderer abstraction work.

## Architecture

The application will be a single-process Qt Widgets program with three focused units:

- `PtySession`: creates and owns the PTY master fd, spawns the user shell, writes input to the PTY, reads output, and updates PTY window size.
- `TerminalWidget`: owns `GhosttyTerminal`, `GhosttyRenderState`, and the reusable row iterator/cell handles. It consumes bytes from `PtySession`, updates terminal state, maps Qt keyboard events to Ghostty key encoding, and paints the visible terminal grid with `QPainter`.
- `main.cpp`: creates the `QApplication`, top-level window, and the terminal widget.

The program stays single-threaded. PTY readiness integrates into the Qt event loop through `QSocketNotifier`, which is sufficient for a minimal demo and matches the intended lightweight structure.

## Rendering Approach

The first renderer will use `QWidget` and `QPainter`, not `QOpenGLWidget`.

Reasons:

- `libghostty-vt` exposes render state, not a GPU-only renderer.
- `ghostling` proves a CPU 2D renderer is a valid minimal integration.
- `QPainter` keeps the first milestone focused on terminal correctness rather than graphics pipeline setup.

Per frame, `TerminalWidget` will:

1. Call `ghostty_render_state_update(...)`.
2. Read terminal colors and row/cell data from the render state.
3. Paint cell backgrounds first, then glyphs, then the cursor.

The first version will support:

- Default foreground/background colors
- 16/256-color and RGB foreground/background values exposed through cell style
- Basic bold, italic, underline, and inverse where they are straightforward to map with Qt font and pen/brush settings
- Wide character cell advance using terminal cell width rules, not custom shaping

The first version will intentionally omit:

- Dirty-row optimization beyond full-widget repaint
- Custom glyph atlas / texture cache
- Advanced text shaping
- Inline image drawing

## Data Flow

### PTY to terminal

- `PtySession` receives readable notifications from the PTY master fd.
- It drains available bytes and emits them to `TerminalWidget`.
- `TerminalWidget` forwards bytes into `ghostty_terminal_vt_write(...)`.
- Any terminal effect that needs to write back to the PTY will use the Ghostty write-PTY callback and route back through `PtySession`.

### Keyboard to PTY

- `TerminalWidget` handles `keyPressEvent`.
- For printable text, it writes UTF-8 bytes to the PTY.
- For special keys and modifier combinations, it uses Ghostty key-encoding APIs so the terminal protocol stays aligned with terminal modes.
- Encoded bytes are written through `PtySession::write(...)`.

### Resize

- `TerminalWidget` measures the active font using `QFontMetrics`.
- Widget pixel size is converted into `cols` and `rows`.
- On size changes, the widget updates both the Ghostty terminal size and the PTY winsize.
- A repaint follows so reflowed content becomes visible immediately.

## Components

### `src/PtySession.h` and `src/PtySession.cpp`

Responsibilities:

- Spawn shell with `forkpty` on Linux
- Set non-blocking mode on the master fd
- Provide `write(QByteArrayView)` for terminal input/output responses
- Emit a Qt signal when PTY bytes are available
- Emit a signal when the child exits or the PTY closes
- Apply `TIOCSWINSZ` when terminal dimensions change

### `src/TerminalWidget.h` and `src/TerminalWidget.cpp`

Responsibilities:

- Initialize `GhosttyTerminalOptions` with a small default size
- Create `GhosttyTerminal`, `GhosttyRenderState`, row iterator, and row cells objects
- Register terminal callbacks needed for PTY writeback and optional title updates
- Own font, palette defaults, cell metrics, and repaint scheduling
- Translate Qt input events into UTF-8 or Ghostty key sequences
- Paint the terminal contents using render-state iteration

### `src/main.cpp`

Responsibilities:

- Start `QApplication`
- Create a top-level window
- Insert `TerminalWidget` as the central widget
- Set an initial size large enough for a useful shell session

### `CMakeLists.txt`

Responsibilities:

- Configure a Qt6 Widgets C++ project
- Link against the local `libghostty-vt.so`
- Add include directories for the Ghostty headers
- Set runtime rpath or equivalent so the executable can find `libghostty-vt.so` from the build tree or repository root during development

## Error Handling

The minimal implementation will handle failures explicitly and early:

- If PTY creation or shell exec setup fails, the program exits with a clear message.
- If Ghostty object creation fails, the widget construction fails fast and the application exits.
- If the PTY closes after startup, the widget keeps the last rendered screen and shows the session as closed rather than crashing.
- If a resize computes zero columns or rows, the code clamps dimensions to at least `1x1`.

## Testing Strategy

The first milestone will emphasize executable verification over deep automated coverage, because the repository currently has no test harness and the work is primarily systems integration.

Verification targets:

- Configure and build with CMake successfully
- Launch the demo successfully
- Confirm shell prompt appears
- Confirm printable input echoes correctly
- Confirm Enter, Backspace, and arrow keys behave correctly in a shell
- Confirm resizing the window updates terminal size without crashing

Automated coverage in the first iteration will be limited to a small CTest smoke test if practical, such as a build-only or startup-only check. Rich UI or PTY interaction tests are explicitly deferred.

## Future Path

If the minimal widget works, the next iteration can cleanly evolve in either of two directions without rewriting terminal integration:

- Introduce a renderer abstraction and implement a `QOpenGLWidget` or QRhi-backed renderer using the same Ghostty render state
- Add terminal features above the current integration layer, such as selection, clipboard, scrollbars, and mouse reporting

This is why the first iteration keeps PTY/session ownership separate from rendering and keeps Ghostty API usage concentrated inside `TerminalWidget`.
