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
    ├── root-cause/
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

## Linglong Package Build

The `linglong/linglong.yaml` manifest describes the Linglong package.

Build (requires `ll-builder` installed):

```bash
cd linglong && ll-builder build
```

Export the built artifact:

```bash
cd linglong && ll-builder export
```

## Version Bumping

When bumping the version (e.g. `1.0` → `1.0.1`), keep the Debian/CMake
version to at most three numeric segments (`major.minor.patch`) and update
these files:

1. **`CMakeLists.txt`** — change the `VERSION` in the `project()` call
2. **`debian/changelog`** — prepend a new entry using the same version plus
   Debian revision, e.g. `1.0.1-1`. Follow the rules below.
3. **`linglong.yaml`** — update `package.version` to the Linglong
   four-segment form derived from the same release, e.g. `1.0.1.0`
4. Rebuild and verify: `cmake -B build && cmake --build build && cd build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure`

### Changelog Rules

- Use `git log <previous-version-tag>..HEAD --oneline` to collect all changes.
- Group changes by significance: features first, then improvements, then fixes.
- Lead with the most impactful user-visible changes, not the most recent commits.
- Each bullet is one concise sentence describing the *what* and *why*, not the commit hash.
- Do not list every commit — merge related changes into single bullets.
- Avoid implementation details (file names, function names) — describe the user-visible result.

## Test Commands

Widget tests require the Qt **offscreen** platform or they abort
without a display. Always set `QT_QPA_PLATFORM=offscreen`:

```bash
cmake -B build -DBUILD_TESTING=ON
cmake --build build
cd build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure
```

Individual binaries (offscreen still required):

```bash
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget
```

Prefer the most specific affected binary first, then the full set.

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

### `qtghostty` vs `app` Boundary

- `src/libqtghostty/` must stay reusable as a library layer.
- `qtghostty` may own terminal-facing capabilities only:
  - PTY/session lifecycle
  - terminal rendering and input encoding
  - selection, copy/paste, scrollback, search, title propagation
  - generic Qt widget behavior required to present a terminal
- `qtghostty` must not encode application product concepts:
  - menus or menu structure
  - tabs, panes, split management, window management
  - settings dialogs, app actions, or other DTK-specific workflow
- `src/app/` owns all application composition:
  - context menus and action wiring
  - split-pane orchestration
  - tab/window behavior
  - settings UI and other app-specific commands
- When a feature crosses the boundary, prefer exposing a narrow terminal capability from `qtghostty` and let `app` decide how it is triggered or presented.

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

## Logging

- Use project logging categories from `src/logging/Logging.h` instead of raw `qDebug()` or `qWarning()` in normal application code.
- Keep category names under the `org.deepin_terminal_ghostty.*` namespace.
- Use `qCInfo` for lifecycle milestones, `qCWarning` for recoverable failures or fallbacks, and `qCCritical` for unrecoverable initialization failures.
- Avoid logging in paint paths, per-byte PTY data paths, and other high-frequency loops unless handling an error.
- Failure logs should include enough context to identify the failing operation.

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
- Only write a root cause report under `docs/root-cause/` when debugging or fixing a bug with non-trivial investigation.
- New feature work does not need a root cause report unless it also uncovers and fixes a related bug.
- When debugging or fixing a related bug, read existing reports in `docs/root-cause/`; they capture prior failure modes, evidence, fixes, and verification gaps.
- Do not revert unrelated user changes.
- If behavior changes, add or update a focused automated test when practical.
- Verify changes with the narrowest meaningful test command before claiming success.
- If a bug spans PTY -> widget -> window layers, inspect the full signal/ownership chain before patching symptoms.

## Git And Commit Rules

- Do not create a commit unless the user asks for one.
- Before committing, check `git status --short` and avoid including
  unrelated changes; ensure changed C++ files pass `clang-format`.
- Follow Conventional Commits 1.0.0 (`https://www.conventionalcommits.org/`).

### Format

```text
<type>[scope]: <subject>

<body>
```

- **type**: `feat`, `fix`, `refactor`, `perf`, `test`, `docs`, `build`,
  `chore`, `ci`.
- **scope** (optional, lower-case): affected area, e.g.
  `libqtghostty`, `app`, `root-cause`.

### Subject

- Imperative mood, lowercase, no trailing period.
- Scope the actual change, not the whole feature. Max 50 chars (72 hard).

### Body

- Required for every non-trivial commit.
- Explain *why* and *what* — not how (read the diff for that).
- Wrap at 72 chars; never exceed 80. Blank line after the subject.

### Example

```text
fix(libqtghostty): copy word/line selections to primary clipboard

Double/triple-click selections were only synced to the selection
clipboard after a drag. Write whenever a selection is active and
extract the write path so it is testable without a primary selection.
```

When amending or squashing, rewrite the final message to follow this
section rather than concatenating subjects.

## Useful References

- Ghostty source and headers: `~/projects/g/ghostty`
- Ghostling reference implementation: `~/projects/g/ghostling`
- Original deepin-terminal source repo: `~/projects/deepin/deepin-terminal`

Use these when the local implementation needs behavior or API reference, especially for PTY, rendering, input encoding, mouse handling, and terminal capability callbacks.
