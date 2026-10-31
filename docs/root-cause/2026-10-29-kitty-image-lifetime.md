# Stale Kitty images after the last placement is deleted

## Reproduction and evidence

The Stage 3 image fixture transmits a red 1x1 RGBA image with id 6, displays it
across four columns and two rows, switches to the alternate screen and back,
then sends `ESC _ G a=d,d=I,i=6,q=2 ESC \\` to delete the image and placements.
The original renderer left 1296 red pixels in the tested rectangle after the
last deletion. The new `testKittyImageDeletionAndAlternateScreen` failed at its
zero-red-pixel assertion before production changes.

The same test with the unified render-state implementation passes. The fixture
uses an idle child command and captured protocol bytes, so no prompt output is
needed to cause a repaint.

## Cause

The back buffer's early-return condition only considered text render dirtiness,
viewport offset and buffer size. Deleting an image can leave every text row
clean, so the renderer reused pixels painted by the old placement. The image
cache and storage iterator were consulted later in the composition path; they
could not invalidate a frame that had already returned early.

The storage iterator also omitted Unicode placeholder placements and children
whose root was virtual. Those missing renderables are addressed by the feature's
unified image iterator, rather than a local placeholder metadata decoder.

## Fix and lifetime constraints

Before deciding to reuse the back buffer, read the render state's unified image
list and placement generation. Include layout and per-image pixel generations
in invalidation, including an image-list transition to empty. Full redraws while
images are present preserve correct composition with dirty text rows; the
text-only incremental path remains.

Retain QImage copies by protocol/image id and per-image generation. Prune images
that are no longer in the captured list. A storage mutation does not flush
unrelated images. Image handles remain borrowed only within preparation of one
frame, after any animation tick.

Synchronized-output mode retains the last published image list and owned pixel
copies, following the existing constraints described in
[the synchronized-output report](2026-06-29-synchronized-output-frame-jump.md).
Animation callbacks must not publish a partially held frame.

## Verification scope

The final focused run passed 15 Qt Test results (including init/cleanup): the
engine pin, three existing image/cache cases and nine image/animation cases.
The full build and all 12 offscreen CTest entries passed, including text-only
incremental/cursor rendering, opacity and synchronized-output coverage.

An earlier full widget run had two link-hover assertions fail after wide
characters; both passed in isolation and did not recur in the final rebuilt
full widget run. Their cause was not established. No link code was changed.
This report does not establish full protocol conformance or enable local
file/shared-memory transfers.
