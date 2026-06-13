# Warning Glyph Cell Overlap Root Cause

## Summary

Text such as `⚠DI上涨，节点延期` could show the warning sign and the following
`D` visually overlapping. The terminal grid advanced correctly, but the glyph
ink from the warning sign could spill into the next cell when Qt selected a
fallback or emoji-shaped glyph.

## Root Cause

`TerminalWidget::renderRow()` anchored narrow cells to fixed terminal cell
origins, but each cell was still painted with `QPainter::drawText()` without a
cell clip. Qt is allowed to paint glyph ink outside the logical advance or
bounding box of the terminal cell. This matters for fallback glyphs and emoji
variation sequences, where the selected glyph can be wider than the fixed
terminal cell width.

The VT state and cursor position were not the source of the bug: the overlap was
created only when converting a grid cell into pixels.

## Fix

Terminal cell text is now clipped to the cell area while preserving the existing
positioning model:

- narrow cells draw at their fixed cell origin and are clipped to one cell
- wide cells keep the existing centered rendering and are clipped to two cells

This prevents one glyph from painting into the next logical terminal cell without
changing PTY behavior, Ghostty grid state, cursor geometry, or selection logic.

## Follow-up

The initial hard cell clip fixed overlap but could cut emoji-shaped fallback
glyphs in half when Qt selected a glyph wider than one narrow terminal cell. The
follow-up keeps the cell clip, but fits multi-code-unit narrow graphemes inside
the single cell before drawing them. That preserves the no-overlap guarantee
while avoiding a visible half-rendered emoji.

A later startup message exposed the same visual failure for bare `⚠` without an
emoji variation selector. That glyph is a single codepoint, so it did not enter
the previous multi-code-unit fitting branch. Its measured advance could still be
slightly wider than the terminal cell, leaving the right edge clipped. The
renderer now also fits single-codepoint fallback glyphs when their advance
exceeds the cell and renders fitted glyphs through a cell-sized image buffer so
fallback/color glyph painting cannot leak into the next cell.

## Regression Coverage

Added `TestTerminalWidget::testFallbackGlyphDoesNotOverlapNextCell`. The test
uses the warning-sign emoji variation sequence, which had a wider Qt advance in
the local font stack, and verifies that the next terminal cell remains unchanged
when only the warning glyph is rendered. The follow-up also verifies that the
glyph is not cut at the right edge of its own cell.

Added `TestTerminalWidget::testSingleCodepointFallbackGlyphDoesNotClip` for the
bare warning sign used by startup warnings such as `⚠ MCP startup incomplete`.

## Verification

Commands run:

```bash
cmake --build build --target test_terminal_widget
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget \
  testFallbackGlyphDoesNotOverlapNextCell \
  testRendersWideCharactersAcrossTwoCells \
  testStyledTextKeepsCharactersOnCellGrid \
  testRendersTextDecorations \
  testRendersSupplementaryPlaneCharacters
```

The new test first failed with 15 changed pixels in the next cell, then passed
after clipping terminal-cell text painting.
