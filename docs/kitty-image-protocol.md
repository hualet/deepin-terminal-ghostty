# Kitty Image Protocol Support

This document tracks the current Kitty image protocol support in
`deepin-terminal-ghostty` and the intended design for the remaining work.

## Current Status

The terminal now supports the common inline/direct Kitty image path:

- The vendored engine and complete public headers are pinned to Ghostty commit
  `e77b2309fca3a27db1123a4f904b7fb432ee7162` and the library reports
  `0.1.0-dev+e77b2309f`.
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
- File and shared-memory media are explicitly disabled. Temporary-file media is
  disabled with a null directory; it must not be passed a boolean because the
  Ghostty option requires a `GhosttyString` directory.
- The upgraded engine includes pending-payload, relative-placement, deletion,
  retransmission, path validation, and animation-frame improvements. These are
  core capabilities, not claims that every corresponding Qt rendering path has
  conformance coverage yet.

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
The upgraded C API exposes a row-level
`GHOSTTY_ROW_DATA_KITTY_VIRTUAL_PLACEHOLDER` flag plus the graphemes and styles
needed to inspect the cells, but it does not expose Ghostty's internal decoded
virtual-placement iterator. Our Qt renderer therefore still paints those
placeholder codepoints as text, so fonts without `U+10EEEE` show visible square
glyphs instead of the image.

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

The preferred long-term approach is to expose virtual placement render fragments
from `libghostty-vt` through a C API, so Qt does not duplicate Ghostty's
placeholder decoding rules. Each fragment needs image/placement ids, viewport
cell geometry, source geometry, and z-layer. If that API is unavailable, the
fallback is to implement the placeholder decoder in `TerminalWidget` using the
row flag, cell graphemes and styles, and a version-pinned copy of Ghostty's
documented row/column diacritic table.

Relative placements whose root is pinned can use the current placement render
helper. Relative placements whose root is virtual have the same unresolved
dependency as Unicode placeholders.

## Other Remaining Dependencies

- Animated image pixels are exposed as the current frame and image generation
  changes when Ghostty advances that frame. The C API does not yet expose the
  animation tick or next deadline, so Qt cannot schedule animation correctly
  without an upstream API addition.
- Temporary-file media needs an application-controlled directory and an
  explicit path/symlink/lifetime policy. Shared-memory media needs Linux
  availability and lifecycle tests. Direct-file media remains disabled until a
  product security policy permits terminal clients to request arbitrary local
  paths.
- Protocol parsing support inherited from Ghostty still needs wrapper-level
  regression coverage for chunked formats, queries, placement geometry,
  retransmission/deletion, pending payloads, animation, and main/alternate
  screen lifetime.

The actionable implementation and dependency matrix is maintained in
[`TODOs.md`](TODOs.md) under `kitty image protocol`.

## Verification

Current automated coverage:

- `TestTerminalWidget::testRendersInlineKittyPngImage` sends an inline Kitty PNG
  APC sequence and verifies red pixels are rendered in the placement cells.
- `TestTerminalWidget::testKittyImageCacheSurvivesUnrelatedOutput` verifies
  unrelated terminal output does not reconvert an unchanged image.
- `TestTerminalWidget::testKittyImageGenerationInvalidatesReplacement` verifies
  retransmitting an image id invalidates same-sized cached pixels.
- Full `test_terminal_widget` coverage verifies the image render path does not
  regress text rendering, cursor repainting, selection, search, input, or mouse
  behavior.

Manual compatibility checks to keep using:

- Kitty/icat-style direct image display.
- Inline PNG image display.
- `textual-image rich -m tgp` for Unicode placeholder placement support once the
  placeholder path is implemented.
- `textual-image rich -m halfcell` as a non-Kitty fallback sanity check.
