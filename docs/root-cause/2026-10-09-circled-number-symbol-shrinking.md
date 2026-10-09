# Circled Number Symbol Shrinking

## Symptom

In text such as `问题 1 断在 ①：9 月 20 日后`, the circled number `①` rendered
noticeably smaller than the surrounding text.

## Root Cause

U+2460 is East Asian Ambiguous width, so Ghostty places it in one terminal
column. At 11pt the cell is 9px wide, but the primary monospace font has no
glyph for it and Qt falls back to Noto Sans Mono CJK, whose `①` has a 15px
advance and a 15x15 ink box.

`renderRow()` treats an overflowing single non-ASCII codepoint by fitting it
into one cell with a 2px horizontal inset (added for `※`, see
`2026-07-26-overflowing-single-cell-glyph-clipping.md`). For `①` that scales
the glyph to about 7px, roughly half its natural size.

libghostty-vt has no "ambiguous width is wide" option, so the column width
cannot be changed. Upstream Ghostty renders such symbols with
`constraintWidth()`: a symbol-like glyph may span two cells when the next cell
is blank and the previous cell is not another symbol, otherwise it is fitted
into one cell.

## Fix

The renderer now pre-scans the row's codepoints and applies Ghostty's rule to
narrow, single-codepoint, non-emoji symbols (private use areas, Arrows,
Enclosed Alphanumerics and related symbol blocks):

- before a blank cell the glyph may use two cells and keeps its natural size;
- otherwise it is fitted into one cell without the horizontal inset and is
  never enlarged.

Emoji presentation codepoints (`⚠`, `✓`, `☀`, ...) are excluded and keep the
existing one-cell fallback behavior, which the warning-glyph overlap tests rely
on. `※` and other non-symbol glyphs keep the previous fit path.

In the reported text `①` is followed by a fullwidth colon, so it is still
constrained to one cell; it grows from about 7px to the full 9px cell width.

## Regression Coverage

`testSymbolGlyphUsesBlankNeighborCell` renders `① x` and `①：`. It verifies the
first extends into the blank neighbor cell and the second fills its single
cell. Before the fix the first assertion failed.

## Verification

```bash
cmake --build build --target test_terminal_widget
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget
```

All rendering regression tests named in earlier root-cause reports pass.
