# CJK And Emoji Fallback Crowding

## Symptoms

Mixed Chinese and emoji text such as:

```text
今天是个好日子 ☀️，早上喝了杯咖啡 ☕，
然后开始写代码 💻。中午和朋友约了饭 🍜，
```

could render with some emoji visually crowding or touching adjacent Chinese
punctuation/text. The issue was inconsistent because different emoji sequences
are classified into different terminal cell shapes by the VT state.

## Root Cause

The terminal grid was advancing correctly. The problem was in the custom color
emoji fallback rasterizer used when Qt cannot render color emoji directly.

`TerminalWidget::emojiFallbackCellImage()` scaled fallback emoji to almost the
full terminal cell rectangle, leaving only a one-pixel margin. That was enough
to satisfy hard no-spill clipping tests, but in CJK text the following glyph may
start at the next cell edge with very little visual whitespace. Emoji variation
sequences such as `☀️` and narrow emoji before CJK punctuation therefore looked
overlapped even though the grid cells themselves did not overlap.

## Fix

Fallback emoji images now use explicit horizontal and vertical insets before
scaling. This keeps the color bitmap inside its assigned terminal cell area and
leaves a visible boundary before adjacent CJK punctuation or text.

The change is limited to the custom fallback bitmap sizing path. It does not
change Ghostty cell width decisions, PTY data, cursor positioning, selection,
or native Qt emoji rendering.

## Regression Coverage

Added `TestTerminalWidget::testEmojiFallbackDoesNotOverlapFollowingCjkText`
with a mixed Chinese/emoji sample. The test verifies that color fallback pixels
do not enter the following CJK punctuation cell and that the fallback glyph
leaves a boundary before that cell.

The emoji render probe now uses the same mixed Chinese/emoji sample so the
generated `/tmp/deepin-terminal-emoji-{auto,fallback}.png` images exercise this
runtime-visible failure mode.

## Verification

Commands run:

```bash
cmake --build build --target test_terminal_widget emoji_render_probe
cd build
QT_QPA_PLATFORM=offscreen ./tests/test_terminal_widget \
  testEmojiFallbackDoesNotOverlapFollowingCjkText \
  testEmojiFallbackRendererKeepsNarrowEmojiInOneCell \
  testEmojiFallbackRendererSizesVariationEmoji \
  testFallbackGlyphDoesNotOverlapNextCell \
  testSingleCodepointFallbackGlyphDoesNotClip
QT_QPA_PLATFORM=offscreen ./tests/emoji_render_probe
```

The focused tests passed. The probe rendered both auto and forced fallback
paths for the mixed Chinese/emoji sample and wrote the images under `/tmp/`.
