# Kitty Image Protocol Support

The bundled engine and public headers are pinned to Ghostty 1.4.0 commit
`83fb95624b0439ee660c2679b43b38c8b63769cd`; the VT library reports
`0.1.0-dev+83fb95624`. Rebuild inputs are recorded in
[`dependencies/ghostty-vt.md`](dependencies/ghostty-vt.md).

## Rendering

`libghostty-vt` parses Kitty APC commands, owns image storage, resolves placement
geometry and composes animation frames. Qt installs a process-wide PNG decoder
callback, converts decoded RGB/RGBA/grayscale/grayscale-alpha buffers to owned
`QImage` pixels, and paints them with QPainter. Image storage is limited to
64 MiB; the Kitty APC payload limit is 80 MiB.

All image drawing uses `GHOSTTY_RENDER_STATE_DATA_IMAGE_ITERATOR`. Its unified
entries describe fixed placements, Unicode placeholder (`U=1`, U+10EEEE) runs,
and relative placements, including children whose parent is virtual. Qt reads
viewport coordinates, cell offsets, source crop, destination size, protocol,
image id/generation and layer/z with `ghostty_render_state_image_get_multi()`.
Ghostty resolves placeholder metadata; Qt does not decode its diacritics.

The renderer sorts entries by layer and z and composes them in this order:

1. Images below backgrounds.
2. Explicit cell backgrounds.
3. Images below text.
4. Text and text decorations.
5. Images above text.
6. Selection, search, cursor and other widget overlays.

Drawing is clipped to the terminal content rectangle, including placements with
negative viewport coordinates. U+10EEEE glyphs are suppressed while their cell
backgrounds and decorations remain visible. The former storage placement
iterator is no longer used for drawing, so fixed images are composed once.

The pixel cache uses protocol/image id plus each image's generation. Changing a
different image does not discard unchanged pixels. Only images needed by the
current render list are retained. Placement and pixel changes invalidate the
back buffer, including deletion of its last image. Frames containing images use
a full composition pass; text-only frames retain incremental row painting.
Synchronized output retains the previously published image list and pixel copies
until the hold ends.

## Animation

Visible terminal frames call `ghostty_kitty_graphics_animation_tick()` once with
a monotonic `QElapsedTimer` clock whose origin lasts as long as the terminal.
Image handles and pixels are looked up after ticking. A successful tick supplies
the absolute next deadline for a single-shot Qt timer, with an 8 ms minimum
delay. `GHOSTTY_NO_VALUE` stops scheduling.

Hidden or minimized terminals do not tick or schedule animation. Showing them
requests a frame and resumes playback. Rendering a hidden pane's snapshot does
not start a background animation loop. Synchronized-output holds stop animation
publication. Reset/import/session closure clear the relevant cache/timer state;
session closure prevents further playback scheduling.

## Local transfer policy

Direct-file and shared-memory media remain explicitly disabled. Temporary-file
media is disabled with a null directory; that option takes a `GhosttyString`
directory, not a boolean.

Enabling temporary files requires an application-controlled directory and a
path/symlink/lifetime policy. Shared memory needs Linux availability and cleanup
coverage. Direct-file access requires an explicit product policy before terminal
clients may request local paths. This rendering update does not enable any of
these media.

## Regression coverage and remaining checks

The existing inline PNG, unrelated-output cache and same-size retransmission
pixel tests remain. Captured protocol streams add virtual placements, relative
children, placeholder glyph suppression with backgrounds/decorations, source
cropping, z order, partial scrolling/clipping, deletion, alternate screens and
cache survival across another image's mutation. The two-frame animation fixture
comes from upstream's `example/c-vt-kitty-graphics/src/main.c`; timer tests cover
red/blue playback, stopping, hiding/minimizing, hidden snapshots, finite loops,
reset/import/session shutdown and synchronized-output holds.

These focused paths do not establish complete Kitty protocol conformance.
Uncovered transfer formats, malformed input, detailed placeholder metadata,
resize/reflow combinations, animation composition/frame loading and real-client
interoperability remain tracked in [`TODOs.md`](TODOs.md). Manual compatibility
checks should include `kitten icat`, `kitten icat --unicode-placeholder`,
`textual-image rich -m tgp`, and the non-Kitty `halfcell` fallback.

Stage 3 validation passed with the restored rootless development prefix: the
full application/test build completed, the focused image/engine run passed all
15 Qt Test results (including init/cleanup), and all 12 CTest entries passed.
`TerminalWidget` ran with `SHELL=/bin/sh`, an escape-only `PS1`, empty `PS2` and
`QT_QPA_PLATFORM=offscreen`; the other entries used `PS1='$ '` and a separate
isolated HOME. Formatting and diff checks passed.

An earlier full widget run reported two existing link-hover assertions that
passed in isolation and did not recur in the final rebuilt full suite. Their
cause was not established; no unrelated link code was changed.
