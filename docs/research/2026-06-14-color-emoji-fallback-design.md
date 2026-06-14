# Color Emoji Fallback Rendering

## Background

Some Qt versions or platform font stacks cannot render color emoji correctly in
`QPainter` text drawing. The common symptoms are monochrome fallback glyphs,
tofu boxes, very small emoji, or visible variation-selector placeholders such as
`<fe0f>`.

`deepin-terminal-ghostty` keeps Qt native rendering when the runtime stack can
render color emoji, and switches to an internal color-emoji fallback when it
cannot. The fallback lives in `qtghostty` because it is part of terminal cell
rendering, not an application-level workflow.

## Goals

- Prefer Qt native emoji rendering on systems where it works.
- Render color emoji on older or limited Qt/font stacks.
- Preserve terminal grid semantics: plain text digits stay text, keycap emoji
  stay emoji, narrow symbols do not overwrite the next cell, and wide emoji can
  occupy two cells.
- Preserve Ghostty-rendered cell styles, including custom foreground,
  background, inverse, and text decorations.
- Keep the fallback path cached and bounded so paint-time cost stays controlled.

## Architecture

The implementation is concentrated in `src/libqtghostty/TerminalWidget.cpp` and
is split into four layers.

### Runtime Mode Selection

`TerminalWidget::emojiRenderMode()` returns either:

- `EmojiRenderMode::QtNative`
- `EmojiRenderMode::CustomFallback`

The mode is detected lazily by `qtCanRenderColorEmoji(...)`, which renders a
small sample into a `QImage` and checks whether the result contains color pixels.
Tests can force a mode with `debugSetEmojiRenderModeForTesting(...)`.

Font changes invalidate the detected mode and emoji image cache in
`setTerminalFont(...)` and `debugSetRawTerminalFont(...)`, because the Qt
rendering result depends on the terminal font and platform fallback chain.

### Emoji Classification

The helper functions near the top of `TerminalWidget.cpp` classify terminal cell
text before painting:

- `isEmojiCodepoint(...)` recognizes Unicode emoji blocks.
- `isEmojiPresentationBaseCodepoint(...)` extends that set with legacy emoji
  presentation bases such as `©`, `®`, and `™`.
- `isEmojiKeycapBaseCodepoint(...)` recognizes `0-9`, `#`, and `*`, but
  `emojiCodepoints(...)` only treats them as emoji when the keycap combining
  mark `U+20E3` is also present. This keeps normal text such as `K2.7` out of
  the emoji renderer.
- `emojiVariationSelectorPlaceholderCodepoint(...)` handles Ghostty diagnostic
  placeholders such as `<fe0f>` and `<fe0e>` so they are consumed as part of the
  emoji cluster instead of being painted literally.
- `fallbackEmojiCellSpan(...)` maps Ghostty's width classification to fallback
  paint width. Wide-head emoji cells use two cells; narrow legacy symbols such
  as `©️` stay within one cell.

### FreeType/HarfBuzz Fallback Renderer

`EmojiFreeTypeRenderer` renders emoji using:

- Fontconfig to locate a color emoji font. It first tries `Noto Color Emoji` and
  then other common emoji families.
- FreeType with `FT_LOAD_COLOR` to load color glyph bitmaps.
- HarfBuzz to shape multi-codepoint emoji sequences such as flags, keycaps,
  skin-tone modifiers, and ZWJ clusters.

If shaping returns a missing glyph, `render(...)` tries
`renderComponentEmoji(...)` before giving up. This is important for ZWJ
sequences: a missing composed glyph should not force the caller to erase the
terminal cells.

`selectSize(...)` chooses the closest fixed bitmap strike by comparing
FreeType's 26.6 `y_ppem` value after converting it back to pixels.

### Terminal Row Integration

`TerminalWidget::renderRow(...)` owns the integration with Ghostty cell data.
The render path has two fallback mechanisms:

1. Inline fallback drawing for single emoji cells.
2. Overlay fallback drawing for sequences split across several terminal cells,
   such as variation selectors, keycaps, flags, and ZWJ clusters.

Overlay cells record their original background color. When a cluster is found,
`drawEmojiClusterOverlays` first asks `emojiFallbackCellImage(...)` to build the
fallback image. It only clears the cells after the image exists. If fallback
rendering fails, the inline-rendered content remains visible instead of being
erased into blank cells.

`emojiFallbackCellImage(...)` prepares the cached image for a cell rectangle.
`drawEmojiFallback(...)` draws that image and updates test counters. Splitting
these operations lets overlay rendering validate success before mutating the
paint buffer.

## Cache Behavior

Emoji images are cached by:

- Unicode codepoint sequence
- logical cell rectangle size
- device pixel ratio

`m_emojiImageCache` stores the images and `m_emojiImageCacheOrder` provides a
bounded recent-use eviction order. The cache is cleared when the terminal font
changes or when tests force a different emoji mode.

## Key Functions

| Function | Role |
| --- | --- |
| `qtCanRenderColorEmoji(...)` | Detects whether Qt native text rendering can produce color emoji. |
| `TerminalWidget::emojiRenderMode()` | Chooses native Qt or custom fallback mode. |
| `emojiCodepoints(...)` | Validates whether a text cell is an emoji sequence. |
| `fallbackEmojiCellSpan(...)` | Maps Ghostty wide-head state to one or two terminal cells. |
| `EmojiFreeTypeRenderer::render(...)` | Shapes and renders color emoji bitmaps. |
| `EmojiFreeTypeRenderer::renderComponentEmoji(...)` | Fallback for unsupported composed sequences. |
| `TerminalWidget::emojiFallbackCellImage(...)` | Builds a cached fallback cell image without painting it. |
| `TerminalWidget::drawEmojiFallback(...)` | Paints a prepared fallback image into the terminal row. |
| `TerminalWidget::renderRow(...)` | Integrates emoji fallback with Ghostty cell rendering. |

## Tests

The focused coverage is in `tests/test_terminal_widget.cpp`:

- forced fallback rendering
- keycap emoji rendering
- plain digits staying plain text
- variation emoji sizing
- narrow emoji staying inside one cell
- ZWJ cluster overlay preserving custom backgrounds
- font changes invalidating emoji mode detection
- fallback glyphs not leaking into the next cell

`tests/emoji_render_probe.cpp` is a manual/offscreen probe that renders a known
emoji sample to `/tmp/deepin-terminal-emoji-auto.png` and
`/tmp/deepin-terminal-emoji-fallback.png`.
