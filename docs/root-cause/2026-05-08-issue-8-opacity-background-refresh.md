# Issue #8: Opacity Changes Did Not Refresh Terminal Background

## Symptom

Changing the background opacity updated the setting but the visible terminal
background could remain at the previous opacity until terminal content changed
or a full redraw happened for another reason.

## Root Cause

`TerminalWidget` caches terminal rendering in `m_backBuffer`. `setOpacity()`
called `update()`, but it did not invalidate that cache. On the next paint,
Ghostty could report no dirty rows, so `renderTerminal()` reused the existing
back buffer with the old alpha value instead of repainting the background with
the new opacity.

## Fix

`setOpacity()` now ignores unchanged values, updates the translucent-background
attribute only when needed, and clears `m_backBuffer` when the opacity changes.
The next paint recreates the buffer and performs a full background redraw using
the current alpha.

## Verification

- Added `testSetOpacityRepaintsCachedBackground`, which renders an opaque frame,
  changes opacity, and verifies that the cached background is repainted with a
  lower alpha.
- Ran the opacity-focused `test_terminal_widget` cases under the Qt offscreen
  platform.
