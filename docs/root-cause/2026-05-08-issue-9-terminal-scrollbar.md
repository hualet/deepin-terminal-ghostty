# Issue 9: Terminal Scrollbar Missing

## Symptom

The terminal rendered scrollback content but did not expose a scrollbar for
dragging the viewport position.

## Root Cause

`TerminalWidget` owned viewport scrolling internally through Ghostty VT state,
but the application layer had no widget-level scrollbar wired to that state.
Wheel scrolling worked because it directly called Ghostty viewport scrolling,
while drag scrolling had no UI surface.

An intermediate attempt to make `TerminalWidget` the viewport of a
`QAbstractScrollArea` broke terminal painting because `TerminalWidget` already
owns its own rendering, geometry, focus, and input behavior as a standalone
widget.

## Fix

Expose a narrow scroll state and absolute scroll API from `TerminalWidget`, then
wrap each terminal in an app-layer `TerminalScrollContainer`. The container keeps
`TerminalWidget` as a normal full-size child and overlays a DTK/Qt styled
vertical scrollbar hosted outside the terminal widget. The scrollbar mirrors the
Ghostty viewport state and calls back into `TerminalWidget` for absolute scroll
changes.

## Verification

- `cmake --build build`
- `QT_QPA_PLATFORM=offscreen ./build/tests/test_main_window`
- `QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget`
- `clang-format --dry-run --Werror ...`
- `git diff --check`
