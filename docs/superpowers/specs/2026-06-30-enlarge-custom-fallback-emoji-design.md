# Enlarge Custom-Fallback Emoji

## Goal

Make custom-rendered color emoji slightly larger without changing terminal cell
geometry, normal text rendering, or the warning-sign special case.

## Design

Reduce the horizontal inset used by ordinary emoji in
`TerminalWidget::emojiFallbackCellImage()` from 2 pixels to 1.5 pixels. Keep the
2-pixel vertical inset and the warning-sign 1-pixel insets unchanged. The image
remains centered and clipped to its assigned terminal cell, so the change
cannot paint over adjacent cells.

This fixed adjustment is preferred over a 1-pixel inset, which risks restoring
the crowded CJK/emoji appearance, and over proportional sizing, which adds
complexity without a demonstrated need.

## Verification

Update the focused fallback-size regression so it requires a modest increase
over the current footprint. Run the relevant custom-fallback size and CJK
spacing tests with the Qt offscreen platform, then regenerate and visually
inspect both emoji probe images.

Success means ordinary fallback emoji are visibly larger while mixed CJK text
remains separated and no existing fallback-glyph clipping tests regress.
