# Block Element Glyph Shrinking

## Symptom

Kimi Code's two-line logo:

```text
▐█▛█▛█▌
▐█████▌
```

rendered as separated narrow strokes instead of a continuous cell-filling
shape. Other terminals rendered the same output correctly.

## Investigation

The Kimi logo uses Unicode Block Elements U+2580 through U+259F. With
`DejaVu Sans Mono` at 12 points, the terminal cell is 10 pixels wide while
Qt reports an 11-pixel ink width for `█` and `▛`.

A pixel regression rendered the exact logo and measured the five full-block
cells on its second row. Before the fix, the first `█` covered only 84 of 190
cell pixels.

An automated `git bisect` from v1.0.10 to v1.0.12 identified
`881c309 fix(libqtghostty): fit overflowing glyphs` as the first bad commit.
Its parent `75ec69e` passed the same regression. The vendored Ghostty upgrade
`35fa66d` came later, so the `.h` and `.so` replacement did not introduce the
rendering error.

## Root Cause

The renderer deliberately fits a single non-ASCII glyph when Qt reports an
advance or ink width wider than its terminal cell. This prevents symbols such
as `※` from being clipped.

Block Elements intentionally reach cell edges so adjacent glyphs form a
continuous shape. Their small metric overflow therefore has different
semantics. Treating it as ordinary glyph overflow reduced the font size to fit
inside a two-pixel inset, creating visible gaps between cells.

Ghostty supplied the correct codepoints and one-column cell widths. The
failure occurred only in the Qt pixel-rendering path.

## Fix

U+2580 through U+259F now bypass per-glyph overflow fitting. They retain the
active terminal font size and remain clipped to their assigned cell.

The change preserves:

- overflow fitting for other single-cell non-ASCII glyphs such as `※`;
- cell clipping, so glyphs cannot paint over adjacent terminal cells;
- the existing emoji fallback path;
- Ghostty grid widths, cursor positions, and PTY data.

## Regression Coverage

`testKimiBlockLogoKeepsCellFillingGeometry` renders the exact two-line logo
with a standard monospace font and verifies that every `█` in the second row
fills at least 80 percent of its cell. The test failed on the first bad commit
and current pre-fix code, then passed with the Block Elements exclusion.

## Verification

- The Kimi logo regression and the existing overflowing-glyph regression
  passed together.
- The complete `TerminalWidget` suite passed all 181 checks.
- The project built successfully.
- The full CTest run passed 9 of 11 test binaries. `MainWindow` retained its
  existing offscreen-only sidebar close timeout, and `DtermctlCli` could not
  exercise its no-service case because a desktop control service was already
  running. Neither failure executes the changed rendering path.
- The modified C++ files pass the project's clang-format check, and
  `git diff --check` reports no whitespace errors.
