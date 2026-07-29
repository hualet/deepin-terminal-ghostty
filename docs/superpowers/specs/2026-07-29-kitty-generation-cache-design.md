# Kitty Generation Cache Design

## Goal

Keep decoded Kitty images cached across unrelated terminal output while
invalidating an entry whenever Ghostty reports that the image was replaced.

## Design

Store a `generation` alongside each cached `QImage`. Query
`GHOSTTY_KITTY_IMAGE_DATA_GENERATION` before a cache lookup:

- return the cached image when both image ID and generation match;
- decode and replace the entry when the ID is new or its generation changed;
- keep clearing the cache on terminal reset.

Generation values are process-wide and never reused, so this also remains safe
across main/alternate screens. The cache stays in `TerminalWidget`, where the
Qt image conversion already belongs.

## Verification

Count test-only conversions. Render an image twice around ordinary text output
and require one conversion, then retransmit the same image ID with different
pixels and require a second conversion plus the new rendered color.
