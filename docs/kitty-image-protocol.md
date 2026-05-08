# Kitty Image Protocol Support

This document tracks the current Kitty image protocol support in
`deepin-terminal-ghostty` and the intended design for the remaining work.

## Current Status

The terminal now supports the common inline/direct Kitty image path:

- `libghostty-vt` parses Kitty graphics APC sequences and owns protocol state.
- `TerminalWidget` enables Kitty image storage with a 64 MiB storage limit.
- A process-wide Ghostty PNG decoder callback is installed through
  `GHOSTTY_SYS_OPT_DECODE_PNG`; the callback decodes PNG data with Qt `QImage`
  and returns RGBA pixels through Ghostty's allocator.
- `TerminalWidget` reads `GHOSTTY_TERMINAL_DATA_KITTY_GRAPHICS`, iterates image
  placements, converts Ghostty image buffers to `QImage`, and paints them into
  the terminal back buffer.
- The renderer honors Ghostty's placement z-layer grouping:
  below background, below text, and above text.
- File, temporary-file, and shared-memory media are disabled for now. The first
  supported transport is inline/direct data.

The covered scenario is the standard non-virtual placement path used by tools
that transmit image data and ask the terminal to place the image at the cursor
or at an explicit pinned position.

## Known Gap: Unicode Placeholder Placements

`textual-image` uses Kitty's Unicode placeholder placement mode:

- It transmits image data.
- It creates a virtual placement with `U=1`.
- It prints `U+10EEEE` placeholder cells plus combining diacritics that encode
  image id, row, and column.

Ghostty stores these as virtual placements. The current C placement rendering
helper marks virtual placements as not directly visible because their geometry
comes from text-grid placeholder cells, not from a pinned placement rectangle.
Our Qt renderer currently paints those placeholder codepoints as text, so fonts
without `U+10EEEE` show visible square glyphs instead of the image.

Supporting `textual-image` therefore requires a separate placeholder rendering
path, not just PNG decoding.

## Design

Keep protocol parsing and storage in `libghostty-vt`; keep Qt responsible only
for converting renderable image state into pixels.

Current non-virtual placement path:

1. PTY output is fed into `ghostty_terminal_vt_write()`.
2. Ghostty parses Kitty graphics commands, decodes PNG through the installed
   callback, and stores images/placements.
3. During `TerminalWidget::renderTerminal()`, Qt paints:
   - below-background Kitty placements,
   - terminal row backgrounds,
   - below-text Kitty placements,
   - terminal text,
   - above-text Kitty placements,
   - overlays such as selection, search, and cursor.
4. Image buffers are copied into a small `QImage` cache keyed by Ghostty image id.
   The cache is cleared before mutating terminal input because image ids can be
   deleted or replaced by subsequent protocol commands.

Remaining virtual placement path:

1. Detect `U+10EEEE` cells in rendered rows.
2. Decode the Kitty placeholder diacritics and foreground color encoding to
   recover image id, placement id, row, and column.
3. Resolve the backing image from Ghostty image storage.
4. Paint the matching source fragment over the placeholder cell rectangle.
5. Suppress drawing of the placeholder glyphs themselves.

The preferred long-term approach is to expose virtual placement render data from
`libghostty-vt` through a C API, so Qt does not duplicate Ghostty's placeholder
decoding rules. If that API is unavailable, the fallback is to implement the
placeholder decoder in `TerminalWidget` using Ghostty's documented row/column
diacritic table.

## Verification

Current automated coverage:

- `TestTerminalWidget::testRendersInlineKittyPngImage` sends an inline Kitty PNG
  APC sequence and verifies red pixels are rendered in the placement cells.
- Full `test_terminal_widget` coverage verifies the image render path does not
  regress text rendering, cursor repainting, selection, search, input, or mouse
  behavior.

Manual compatibility checks to keep using:

- Kitty/icat-style direct image display.
- Inline PNG image display.
- `textual-image rich -m tgp` for Unicode placeholder placement support once the
  placeholder path is implemented.
- `textual-image rich -m halfcell` as a non-Kitty fallback sanity check.
