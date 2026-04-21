# deepin-terminal-ghostty — Agent Guide

## Purpose

`deepin-terminal-ghostty` is a Linux terminal emulator built with C++20, Qt6 Widgets, DTK6, and `libghostty-vt`.

The repo produces:

- `qtghostty`: reusable Qt wrapper library around `libghostty-vt`
- `deepin-terminal-ghostty`: DTK-based terminal application

## Stack

- Language: C++20
- Build: CMake >= 3.16
- UI: Qt6 Widgets, DTK6
- Terminal engine: `libghostty-vt`
- Platform: Linux only

## Repository Layout

```text
.
├── CMakeLists.txt
├── lib/
│   └── libghostty-vt.so
├── src/
│   ├── libqtghostty/
│   │   ├── PtySession.h/.cpp
│   │   └── TerminalWidget.h/.cpp
│   └── app/
│       ├── main.cpp
│       ├── MainWindow.h
│       └── MainWindow.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── test_pty_session.cpp
│   ├── test_terminal_widget.cpp
│   └── test_main_window.cpp
└── docs/
    ├── superpowers/
    └── research/
```

## Build And Run

Prerequisites on Debian/Ubuntu:

```bash
sudo apt install cmake qt6-base-dev build-essential binutils \
  libdtk6widget-dev libdtk6core-dev libdtk6gui-dev
```

Ghostty headers are bundled in `lib/include/`.

Build:

```bash
cmake -B build
cmake --build build
```

Run:

```bash
./build/deepin-terminal-ghostty
```

## Debian Package Build

Build dependencies:

```bash
sudo apt install build-essential debhelper cmake
```

Build:

```bash
dpkg-buildpackage -us -uc -b
```

Built `.deb` files will appear in the parent directory.

## Test Commands

Run all tests:

```bash
cmake -B build
cmake --build build
cd build && ctest --output-on-failure
```

Run individual binaries:

```bash
./build/tests/test_pty_session
./build/tests/test_terminal_widget
./build/tests/test_main_window
```

When changing PTY, input, tab, or window-close behavior, prefer running the most specific affected test binary first, then the full relevant set.

## Architecture Notes

### `PtySession`

`src/libqtghostty/PtySession.cpp`

- owns the PTY lifecycle with `forkpty()`
- resolves the shell from `$SHELL`, passwd entry, then `/bin/sh`
- integrates the PTY master fd into the Qt event loop with `QSocketNotifier`
- buffers writes safely
- emits `dataReceived(QByteArray)` and `sessionClosed()`

### `TerminalWidget`

`src/libqtghostty/TerminalWidget.cpp`

- owns Ghostty terminal, render state, and key encoder handles
- writes keyboard input to PTY
- feeds PTY output into Ghostty VT state
- paints with `QPainter`
- emits `terminalTitleChanged()` and `sessionClosed()`

### `MainWindow`

`src/app/MainWindow.cpp`

- hosts terminal widgets in `QStackedWidget`
- uses `DTabBar` for tabs
- keeps tab text and window title in sync with terminal title updates
- closes the matching tab when a terminal session ends
- closes the window when the last tab closes

## Code Expectations

- Follow existing Qt and C++ patterns already used in the repo.
- Prefer small, local changes over broad refactors.
- Use `rg` for search.
- Keep comments in English and only where they add real value.
- Preserve naming conventions already present in the codebase:
  - classes: `PascalCase`
  - members: `m_camelCase`
  - constants: `kCamelCase`
  - local helper functions: `camelCase`
- With Ghostty APIs:
  - check `GhosttyResult`
  - free handles in reverse creation order
  - use `GHOSTTY_INIT_SIZED` where required
- Keep the `#undef emit` workaround before including Ghostty headers where needed.

## Code Formatting

All C++ sources (`*.cpp`, `*.h`) must be formatted with the project's `.clang-format` configuration.

Before committing, run:

```bash
clang-format -i $(find src tests -name '*.cpp' -o -name '*.h')
```

Or verify without editing:

```bash
clang-format --dry-run --Werror $(find src tests -name '*.cpp' -o -name '*.h')
```

CI will reject changes that do not match the enforced style.

## Working Rules For Agents

- Explore current code before editing. Do not assume intent from names alone.
- Do not revert unrelated user changes.
- If behavior changes, add or update a focused automated test when practical.
- Verify changes with the narrowest meaningful test command before claiming success.
- If a bug spans PTY -> widget -> window layers, inspect the full signal/ownership chain before patching symptoms.

## Git And Commit Rules

- Do not create a commit unless the user asks for one.
- Before committing, check `git status --short` and avoid including unrelated changes.
- Ensure all changed C++ files pass `clang-format` validation before committing (see **Code Formatting** above).
- Commit messages must follow Conventional Commits:
  `https://www.conventionalcommits.org/en/v1.0.0/`
- Use standard prefixes such as:
  - `fix: ...`
  - `feat: ...`
  - `refactor: ...`
  - `test: ...`
  - `docs: ...`
  - `chore: ...`
- Keep the subject line short, imperative, and scoped to the actual change.
- If amending a commit, preserve the same convention-compliant format.

## Useful References

- Ghostty source and headers: `~/projects/g/ghostty`
- Ghostling reference implementation: `~/projects/g/ghostling`

Use these when the local implementation needs behavior or API reference, especially for PTY, rendering, input encoding, mouse handling, and terminal capability callbacks.
