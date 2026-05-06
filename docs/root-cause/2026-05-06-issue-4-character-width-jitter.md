# Issue #4 Character Width Jitter Root Cause

GitHub issue: <https://github.com/hualet/deepin-terminal-ghostty/issues/4>

## Summary

Styled terminal text could visually shift horizontally when the active SGR span changed across a line. This showed up as:

- zsh path completion leaving a one-cell-looking gap between the cursor and the last visible character.
- Codex's animated `Working` text changing apparent width while color or bold styling moved from left to right.

The root cause was in `TerminalWidget::renderRow()`: adjacent terminal cells with the same style were coalesced into one Qt text run and painted with a single `QPainter::drawText(x, baseline, text)` call. That let Qt use the font's natural glyph advances inside the run. Terminal layout, however, requires every logical cell to stay anchored to `column * cellWidth`, regardless of the natural advance of the glyph, fallback font, or bold variant.

## Investigation Notes

The PTY and Ghostty VT state were not the source of the issue:

- Cursor position and screen cells are tracked by Ghostty in terminal grid coordinates.
- `effectSize()` and PTY resize report one fixed `cell_width` and `cell_height`.
- The visual instability only appeared when rendering style boundaries moved across existing text.

The render path was the layer where terminal-grid coordinates were converted into pixels:

- `m_cellWidth` is computed once from the terminal font metrics.
- Cell backgrounds, cursor rectangles, selection, mouse positions, and PTY size all use `m_cellWidth`.
- Text runs previously used `drawText()` on the whole coalesced string, so Qt advanced characters according to the selected font's shaping and metrics inside that run.

This made the left position of later characters depend on where the current text run began. If a bold/color segment moved from `W` to `r` in `Working`, the suffix run could be laid out from a different start position and with different font metrics. Even a small mismatch between glyph advance and `m_cellWidth` was visible as a gap or horizontal jitter.

## Root Cause

The renderer mixed two coordinate systems:

- Terminal grid positioning: each cell begins at `x + col * m_cellWidth`.
- Qt text-run positioning: each glyph begins after the previous glyph's natural advance.

Coalescing text runs improved performance, but it accidentally allowed Qt's text layout to decide intra-run character positions. Terminal emulators must keep cell origins fixed; font metrics may influence glyph shape, but must not move the logical cell grid.

## Fix

`TerminalWidget::renderRow()` now still coalesces compatible cells into a render run for style state changes and debug accounting, but stores each cell's text separately. When flushing a run, it paints each cell at:

```text
runStartX + cellIndex * m_cellWidth
```

This keeps every narrow cell anchored to the terminal grid while preserving the existing handling for:

- foreground and background colors
- bold and italic font variants
- underline, double underline, strikethrough, and overline decorations
- wide cells, which continue through the separate wide-cell drawing path

## Regression Coverage

Added `TestTerminalWidget::testStyledTextKeepsCharactersOnCellGrid`.

The test uses a test-only raw font hook to simulate a fallback/proportional font where natural glyph advances are not equal to the terminal cell width. It verifies:

- repeated narrow glyphs still reach their logical later cells instead of bunching up by natural font advance
- moving a bold segment across `Working` keeps the same later character anchored to the same cell
- the apparent right edge of the styled line stays stable across animation frames

## Verification

Commands run:

```bash
cmake --build build --target test_terminal_widget
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget \
  testStyledTextKeepsCharactersOnCellGrid \
  testCoalescesPlainTextIntoRenderRuns \
  testRendersWideCharactersAcrossTwoCells \
  testRendersTextDecorations
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget
clang-format --dry-run --Werror src/libqtghostty/TerminalWidget.cpp src/libqtghostty/TerminalWidget.h tests/test_terminal_widget.cpp
git diff --check
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

Results:

- Focused render tests passed.
- Full `test_terminal_widget` passed: 69 tests.
- Formatting validation passed.
- `git diff --check` passed.
- Full `ctest` built and ran. Render-related tests passed, but two unrelated existing tests failed:
  - `AppSettings::testVerticalTabsEnabled`
  - `ServerConfigManager::testInitServerConfigReadsFromFile`

## Follow-Up Notes

The fix is intentionally limited to narrow-cell text painting. It does not change PTY behavior, Ghostty state, cursor computation, selection, mouse tracking, or the wide-cell path.

If future render optimizations touch `renderRow()`, they must preserve the invariant that terminal cell origins are computed from the terminal grid, not from Qt's cumulative text advance.
