# Bare Link Hover Required Click And Felt Slow

## Symptoms

Bare protocol links were only recognized after clicking or opening the context
menu, and recognition felt slow.

## Root Cause

Two issues combined:

- `TerminalWidget` did not enable Qt mouse tracking by default, so mouse move
  events were not delivered unless a button was pressed. The app therefore could
  not hover-detect bare links before a click.
- The bare-link row cache still called `textForScreenRow()` on every lookup to
  compare current text with the cached text. That method reads the row cell by
  cell through Ghostty, so repeated mouse movement over the same row still paid
  the row extraction cost.

## Fix

- Enable mouse tracking in `TerminalWidget` construction so hover detection runs
  without pressing a mouse button.
- Replace per-query row text validation with a terminal-content generation
  counter. Bare-link cache entries are reused while the generation is unchanged
  and invalidated on terminal content or layout changes.

## Verification

- Added `testBareLinkHoverWorksWithoutMouseButton`.
- Added `testBareLinkCachedRowDoesNotRefetchText`.
- Ran the focused bare-link and OSC 8 hyperlink regression tests under
  `QT_QPA_PLATFORM=offscreen`.
