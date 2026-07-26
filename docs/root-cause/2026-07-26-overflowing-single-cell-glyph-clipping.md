# Overflowing Single-Cell Glyph Clipping

## Symptom

In text such as `※ recap`, the `※` reference mark was visibly incomplete. Its
right side reached the terminal cell boundary and was clipped.

## Root Cause

Ghostty correctly placed U+203B in one terminal column under the active
`C.UTF-8` character-width rules. The terminal's 11-point monospace font
produced a 9-pixel cell, while Qt resolved `※` to a glyph with an 18-pixel
advance and a 12-pixel ink width.

`TerminalWidget::renderRow()` clips all narrow glyph painting to one cell. This
is necessary to stop fallback glyphs from overlapping adjacent terminal cells,
but the fitting condition only covered emoji presentation glyphs and
multi-code-unit graphemes. A non-emoji single codepoint such as `※` therefore
kept its native size and lost the portion outside the 9-pixel clip.

The PTY bytes, Ghostty grid position, and cursor advance were correct. The
failure occurred only while converting the one-cell render state to pixels.

## Fix

The renderer now detects a single non-ASCII Unicode scalar independently of
emoji presentation logic. When its measured advance or ink width genuinely
exceeds one cell, it reuses the existing proportional fit-and-center path.

The change does not:

- change Unicode column widths, cursor positions, selection, or PTY geometry;
- alter custom color-emoji fallback detection or rendering;
- rescale ordinary ASCII glyphs;
- permit glyph ink to enter the following cell.

## Regression Coverage

`testOverflowingSingleCodepointGlyphFitsCell` renders the reported text
`※ recap` with custom emoji fallback mode enabled. It isolates pixels changed
inside the first terminal cell and verifies that the glyph has balanced,
non-clipped margins, the following blank cell is untouched, and no custom emoji
fallback draw occurs.

Before the production change, the test failed because the changed glyph pixels
reached the right edge of the cell. It passed after adding the generic
single-codepoint fitting condition.

## Verification

```bash
cmake --build build --target test_terminal_widget

QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget \
  testOverflowingSingleCodepointGlyphFitsCell \
  testZoomedAsciiNotReshapedPerCharacter \
  testSingleCodepointFallbackGlyphDoesNotClip \
  testFallbackGlyphDoesNotOverlapNextCell \
  testStyledTextKeepsCharactersOnCellGrid \
  testEmojiFallbackRendererCanBeForced \
  testEmojiFallbackDoesNotOverlapFollowingCjkText

clang-format --dry-run --Werror \
  src/libqtghostty/TerminalWidget.cpp tests/test_terminal_widget.cpp
```
