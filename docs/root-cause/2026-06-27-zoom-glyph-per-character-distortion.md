# Zoom Glyph Per-Character Distortion

## Symptoms

After zooming the terminal in or out (Ctrl+`+` / Ctrl+`-`), a line of plain text
looked subtly distorted: not every character was scaled by the same factor, so
the letters in a single row looked slightly off relative to each other. The
default font size rendered fine, and only zoomed sizes showed the distortion.

## Root Cause

`TerminalWidget::renderRow()` clips each narrow cell with a cell rectangle and,
in the `drawClippedCellText` helper, optionally rescales a glyph to fit inside
its cell. The decision to enter that rescale branch was too broad.

`m_cellWidth` is the integer `QFontMetrics::horizontalAdvance('M')`, while
`QFontMetricsF::tightBoundingRect()` returns subpixel floats. The predicate
controlling the rescale branch was:

```cpp
const bool overflowsCell = metrics.horizontalAdvance(text) > m_cellWidth
                           || inkBounds.width() >= m_cellWidth
                           || inkBounds.right() >= m_cellWidth - 1
                           || inkBounds.left() < 0
                           || isEmojiText;
if (cells == 1 && overflowsCell
    && (text.size() > 1 || metrics.horizontalAdvance(text) > m_cellWidth)) {
    // rescale drawFont to fit, then paint through a cell-sized image buffer
}
```

`inkBounds.left() < 0` and `inkBounds.right() >= m_cellWidth - 1` hold for almost
every glyph in a monospace font, because most glyphs have ink slightly to the
left of the origin (hinting) or up to the cell edge. At the default font size
this happened to be harmless, but once the font was zoomed the rounded integer
`m_cellWidth` no longer matched the subpixel advances and the predicate started
firing for ordinary ASCII glyphs.

A standalone metrics probe confirmed this directly. At 11pt/13pt/14pt/15pt the
following characters all entered the rescale branch, each with a different
scale factor (ranging from 0.7 for wide glyphs like `W` to ~2.3–3.0 for narrow
ink glyphs like `.`, `:`):

```text
M W i l A @ a # 0 ) ( [ ] { } / \ T H K k j g q y f t I ! : ; . ` '
```

Because the branch rescales `drawFont` with
`setPointSizeF(pointSize * scale)` (or `setPixelSize`), every letter on the line
was rendered at a different point size. That is the source of the distortion:
the line was no longer drawn at a single, uniform font size. Narrow glyphs such
as `.` and `!` were actually *enlarged*, while wide glyphs like `W` were shrunk.

This regression was introduced in `b99dea8`
(`fix: fit bare warning glyphs inside cells`), which widened the predicate so
that a bare `⚠` (single codepoint, no variation selector) would also enter the
rescale branch. The intent was correct for the warning glyph, but the same
predicate also swept in all plain ASCII once the font was zoomed.

## Fix

The rescale branch is now restricted to the cases it was meant to handle:

```cpp
const bool overflowsCell =
    metrics.horizontalAdvance(text) > m_cellWidth || inkBounds.width() > m_cellWidth;
const bool needsFit = isEmojiText || (text.size() > 1 && overflowsCell);
if (cells == 1 && needsFit) { ... }
```

- Plain text always takes the native-advance draw path
  (`painter.drawText(textX, y + m_fontAscent, text)`), so every character in a
  row is drawn at the same font size and the distortion disappears.
- Emoji presentation bases (`⚠`, `⚠️`, `❗`, …) still enter the rescale branch
  via `isEmojiText`, preserving the original warning-glyph fix.
- Multi-code-unit graphemes that genuinely overflow the cell (the original
  emoji-variation-sequence case) still get fitted.
- The pre-existing cell rectangle clip
  (`painter.setClipRect(cellRect, Qt::IntersectClip)` earlier in the helper)
  still guards against any ink leaking into the next cell, so the no-overlap
  guarantee is preserved.

`overflowsCell` also dropped the `inkBounds.right() >= m_cellWidth - 1` and
`inkBounds.left() < 0` subpixel terms, which were the terms that fired for
ordinary glyphs, and tightened `inkBounds.width() >= m_cellWidth` to a strict
`>` to leave a one-pixel tolerance.

## Regression Coverage

Added `TestTerminalWidget::testZoomedAsciiNotReshapedPerCharacter`. The test
sets the terminal font to a non-default size (13pt, mirroring a zoom step),
feeds a line of ASCII glyphs known to trigger the old predicate
(`W.H!`, including the narrow glyphs the old branch enlarged), and asserts that
each rendered glyph keeps its native ink width instead of being rescaled.

The test failed before the fix (`W` ink width 8 vs native 11, because it was
shrunk) and passes after.

## Verification

Commands run:

```bash
cmake -B build -DBUILD_TESTING=ON
cmake --build build --target test_terminal_widget
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget \
  testZoomedAsciiNotReshapedPerCharacter \
  testSingleCodepointFallbackGlyphDoesNotClip \
  testFallbackGlyphDoesNotOverlapNextCell \
  testStyledTextKeepsCharactersOnCellGrid \
  testRendersSupplementaryPlaneCharacters \
  testRendersLongGraphemeCells
```

The new test failed on the unfixed code and passed after the fix; the emoji and
styled-text regression tests continued to pass, confirming the warning-glyph
and overlap fixes were preserved.

## Follow-up: Custom-Fallback Emoji Rendered Too Small

While verifying the zoom fix, emoji next to normal text were noticeably smaller
than the surrounding characters — only the warning emoji looked right. This was
a pre-existing sizing bug in the custom color-emoji fallback path (not a
regression from the zoom fix above), but it lived in the same "glyphs render at
the wrong size" family and was reported alongside it.

The terminal font (e.g. DejaVu Sans Mono) cannot render color emoji, so
`emojiRenderMode()` falls back to `CustomFallback`, and single-cell emoji go
through `emojiFallbackCellImage()`. That path inset the emoji bitmap before
scaling with a 4px horizontal inset (introduced in `9ecc712` to keep a visual
boundary before adjacent CJK punctuation). Because the terminal cell is narrow
(`cellW ≈ 8–13px` for common font sizes), a 4px-per-side inset left a target
width of only `cellW - 8`:

```text
10pt cell=8px  -> emoji bitmap width target 1px (12% of cell)
12pt cell=10px -> emoji bitmap width target 2px (20%)
14pt cell=11px -> emoji bitmap width target 3px (27%)
```

Emoji glyphs are roughly square, so the horizontal target dominated the
`Qt::KeepAspectRatio` downscale and the whole emoji shrank to a few pixels —
matching the reported "emoji are about half too small". The warning sign was
unaffected because it uses a 1px inset and is allowed to borrow the following
blank cell, giving it a two-cell-wide target.

### Fix

The non-warning horizontal inset is reduced from 4.0 to 2.0 (vertical stays at
2.0). The centered bitmap's right edge then lands at `cellLeft + cellWidth - 2`,
which is exactly the 2px boundary the CJK overlap regression test
(`testEmojiFallbackDoesNotOverlapFollowingCjkText`) requires, so the visual gap
before adjacent punctuation is preserved while the emoji itself grows roughly
2×. Verified with the emoji render probe: colorful pixels per frame rose from
198 to 442 for the mixed CJK/emoji sample.

### Regression Coverage

Added `TestTerminalWidget::testEmojiFallbackRendererSizedRelativeToCell`, which
forces `CustomFallback` mode, renders a sun emoji (`☀️`), and asserts the
rendered colorful footprint is at least 40% of the cell width. It failed on the
old 4px inset (emoji rendered 1px wide in a 9px cell) and passes after.

