# Quake Mode Design

## Goal

Add a dedicated quake-mode terminal window that drops down from the top of the
current screen and reuses the existing tab, split, theme, and terminal behavior.

## Scope

The first version adds `--quake-mode`, a `QuakeWindow` subclass, top-of-screen
geometry, show/hide animation, optional focus-loss hiding, and tests for the
window behavior. It does not add global desktop hotkey registration, DBus
single-instance activation, or per-desktop visibility tracking.

## Architecture

`QuakeWindow` lives in `src/app/QuakeWindow.h/.cpp` and subclasses
`MainWindow`. `MainWindow` keeps owning tab, split, session, settings, and
terminal composition. The subclass owns only quake-specific window policy:
positioning, titlebar presentation, animation, and focus-loss hiding.

`StartupOptions` gains `quakeMode`. `main.cpp` creates `QuakeWindow` when this
flag is present, otherwise it creates `MainWindow`.

## Behavior

- `deepin-terminal-ghostty --quake-mode` opens a quake window.
- The window uses the screen under the cursor when available, otherwise the
  primary screen.
- The window is placed at the screen's available top-left corner.
- The width matches the available screen width.
- The target height is 40% of available screen height.
- The window is kept above normal windows with `Qt::WindowStaysOnTopHint`.
- The DTK titlebar is hidden for the drop-down presentation.
- Showing the quake window animates from height 0 to the target height.
- Hiding the quake window animates back to height 0 and then calls `hide()`.
- When `advanced.window.hideQuakeOnFocusLoss` is enabled, losing activation
  hides the quake window.
- Focus-loss hiding is skipped while a child dialog owned by the quake window is
  active.

## Settings

Add an `advanced.window` group with:

- `hideQuakeOnFocusLoss`: checkbox, default `true`.

The option is read through `AppSettings::hideQuakeOnFocusLoss()`.

## Testing

Use TDD:

- `test_startup_options` verifies `--quake-mode`.
- `test_main_window` includes `QuakeWindow` and verifies construction,
  geometry, titlebar hiding, stays-on-top flag, and focus-loss setting behavior.

Manual visual validation can follow later with a real display, but automated
tests should cover deterministic logic under `QT_QPA_PLATFORM=offscreen`.
