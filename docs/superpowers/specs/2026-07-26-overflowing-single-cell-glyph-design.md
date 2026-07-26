# Overflowing Single-Cell Glyph Design

## Problem

Some non-ASCII codepoints occupy one terminal grid cell but resolve to a font
glyph whose ink is wider than that cell. For example, `※` is one column in the
terminal state, while the active Qt font renders it wider than the cell. The
renderer clips every cell to prevent overlap, so the right side of the glyph is
lost.

## Scope

Keep the terminal grid, cursor position, selection, and PTY width unchanged.
Fit only non-ASCII, single-codepoint text whose measured advance or ink width
genuinely exceeds a one-cell render rectangle.

This path must remain independent of emoji presentation detection and the
custom color-emoji fallback renderer. Existing emoji behavior must not change.
Ordinary ASCII text must continue to render at its native font size.

## Rendering Behavior

Extend the existing cell-fitting decision in `TerminalWidget::renderRow()` with
a separate condition for overflowing non-ASCII single codepoints. Reuse the
existing proportional fit-and-center operation after the emoji fallback path
has already had its opportunity to render.

Do not special-case `※`, change Unicode cell widths, or let glyph ink spill
into adjacent cells.

## Regression Coverage

Add a focused offscreen `TerminalWidget` test using the exact text `※ recap`.
The test will verify that the first glyph:

- remains confined to its assigned terminal cell;
- has visible ink on both sides of the cell center, proving it is not clipped
  to a partial glyph;
- leaves the following cell undisturbed;
- does not trigger a custom emoji fallback draw.

Run the new test together with the existing zoomed-ASCII, fallback-glyph,
styled-text, and emoji fallback regressions.
