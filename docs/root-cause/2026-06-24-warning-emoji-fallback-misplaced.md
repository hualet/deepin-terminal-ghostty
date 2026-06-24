# Warning Emoji Fallback Misplaced In Mixed Text

## Symptoms

The line:

```text
⚠️ 需要 agent 各自写 tool 定义（mem0 SDK 减量但不包扩展）
```

rendered with the leading warning emoji missing or visually misplaced. In the
reported sample, the warning icon could appear near the later `mem0` text
instead of at the beginning of the line. The following text still occupied the
expected terminal cells, so the issue looked like an emoji alignment problem
rather than a PTY or grid-width problem.

## Root Cause

`TerminalWidget::renderRow()` correctly recorded the warning emoji variation
sequence (`U+26A0 U+FE0F`) at the first terminal cell and generated a color
fallback image for it. The misplaced output came later in the overlay pass.

`drawEmojiClusterOverlays()` allowed any later emoji-like overlay cell to join a
cluster if the current cluster had a joiner such as a variation selector. A
plain digit such as the `0` in `mem0` is emoji-sequence-compatible because it can
be a keycap base. Since the join code did not require the next overlay cell to
be adjacent, the leading `⚠️` cluster could be incorrectly joined with the later
`0` across normal text. The combined fallback image was then centered in a wide
rectangle spanning from the start of the line to `mem0`, making the warning icon
appear in the middle of the text instead of at the first cell.

The color fallback was not the problem by itself: the generated one-cell
warning image had color pixels, but the later overlay pass repainted it in the
wrong place.

## Fix

Overlay cluster construction now stops when the next emoji-like overlay cell is
not directly adjacent to the current cluster. That keeps variation selectors,
keycaps, flags, and ZWJ sequences working when their cells are contiguous, but
prevents a leading warning emoji from being merged with a later digit across
ordinary text.

The warning sign still uses the custom color emoji fallback. Its fallback bitmap
uses a smaller inset than normal emoji so it remains visible inside a narrow
single-cell terminal slot. When the warning sign is followed by an explicit
blank cell, the overlay may use that blank cell as part of the draw rectangle so
the color icon can stay close to its normal size without shifting the following
text.

## Regression Coverage

Added
`TestTerminalWidget::testWarningEmojiVariationDoesNotAddExtraGapBeforeText`.
The test feeds the exact mixed Chinese/ASCII warning line, forces custom emoji
fallback mode, and verifies:

- the warning sign remains a color emoji in the first terminal cell
- the warning sign may use the explicit following blank cell for normal size
- the first Chinese character still starts after the explicit space
- the logical row text remains unchanged

## Verification

Commands run:

```bash
cmake --build build --target test_terminal_widget
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget \
  testWarningEmojiVariationDoesNotAddExtraGapBeforeText \
  testFallbackGlyphDoesNotOverlapNextCell \
  testSingleCodepointFallbackGlyphDoesNotClip \
  testEmojiFallbackRendererSizesVariationEmoji \
  testEmojiFallbackDoesNotOverlapFollowingCjkText \
  testEmojiFallbackRendererKeepsNarrowEmojiInOneCell
cmake --build build --target emoji_render_probe
QT_QPA_PLATFORM=offscreen ./build/tests/emoji_render_probe
```

The new test failed before the fix because the first cell had no color warning
emoji pixels, then passed after requiring overlay clusters to be contiguous and
using a smaller warning-sign fallback inset. The adjacent rendering regression
tests and emoji probe also passed.
