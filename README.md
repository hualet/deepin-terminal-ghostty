# deepin-terminal-ghostty

`deepin-terminal-ghostty` is the next-generation Deepin terminal emulator, rewritten on top of `libghostty-vt`.

It keeps the Deepin desktop integration and DTK-native experience of `deepin-terminal`, while replacing the terminal core with Ghostty's VT engine to build a faster, cleaner, and more modern foundation for future terminal features.

## Overview

This project is a Linux terminal emulator built with:

- C++20
- Qt6 Widgets
- DTK6
- `libghostty-vt`

The repository currently produces:

- `qtghostty`: a reusable Qt wrapper around `libghostty-vt`
- `deepin-terminal-ghostty`: the desktop terminal application built on top of that library

## Why This Project

`deepin-terminal-ghostty` is intended to become the successor to `deepin-terminal`.

The rewrite focuses on three goals:

- modernize the terminal core with Ghostty's VT implementation
- preserve a native Deepin desktop experience instead of a generic cross-platform shell UI
- create a cleaner architecture that can support tabs, splits, settings, remote workflows, and future rendering improvements

## Current Highlights

The current development branch already includes:

- PTY-based local shell sessions
- Ghostty VT parsing and terminal state management
- Qt/DTK desktop application integration
- tabbed terminal workflow
- split panes inside a tab
- configurable vertical tab mode
- settings dialog backed by DTK settings
- configurable font, cursor, scrollback, and shortcuts
- built-in remote management entry and saved server configuration support
- application translations
- automated tests for PTY, terminal widget, app settings, and main window behavior

## Project Status

This project is under active development.

The core application is already usable and the main window workflow is in place, but the product is still evolving toward a full next-generation replacement for `deepin-terminal`.

## Architecture

The codebase is split into two layers.

### `qtghostty`

The library layer in `src/libqtghostty/` provides terminal-facing capabilities:

- PTY session lifecycle
- terminal widget integration with `libghostty-vt`
- input encoding and output processing
- rendering and terminal state updates

This layer is designed to stay reusable and free of app-specific UI concepts such as tabs, panes, or settings dialogs.

### `deepin-terminal-ghostty`

The application layer in `src/app/` provides the product experience:

- DTK main window and titlebar integration
- tab management
- split-pane orchestration
- settings UI
- remote management panel
- application-level actions and shortcuts

## Repository Layout

```text
.
├── CMakeLists.txt
├── lib/
│   ├── include/
│   └── libghostty-vt.so
├── src/
│   ├── libqtghostty/
│   ├── app/
│   └── logging/
├── tests/
├── translations/
├── debian/
└── docs/
```

## Build Requirements

Platform:

- Linux only

Build dependencies on Debian/Ubuntu:

```bash
sudo apt install cmake qt6-base-dev build-essential binutils \
  libdtk6widget-dev libdtk6core-dev libdtk6gui-dev
```

Ghostty headers are expected under `lib/include/` in this repository, or can be provided through `GHOSTTY_INCLUDE_DIR`.

## Build

```bash
cmake -B build
cmake --build build
```

Run the application:

```bash
./build/deepin-terminal-ghostty
```

## Tests

Enable tests, build, and run:

```bash
cmake -B build -DBUILD_TESTING=ON
cmake --build build
cd build && ctest --output-on-failure
```

Individual test binaries:

```bash
./build/tests/test_appsettings
./build/tests/test_pty_session
./build/tests/test_terminal_widget
./build/tests/test_main_window
```

## Packaging

Build Debian packages with:

```bash
dpkg-buildpackage -us -uc -b
```

The resulting `.deb` files will be generated in the parent directory.

## Roadmap

The long-term direction includes:

- continuing feature parity work with `deepin-terminal`
- improving text rendering and terminal fidelity
- expanding mouse, selection, clipboard, and search workflows
- maturing remote and session management features
- exploring a future GPU-backed rendering path

## License

LGPL-3.0-or-later
