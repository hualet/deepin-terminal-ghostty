# Kitty Images and Animation from Render State

Stage 3 follows the completed engine upgrade and paste protection. Replace
storage placement iteration with Ghostty 1.4's unified render-state image
iterator; Qt keeps QPainter and does not decode placeholder metadata itself.

Read image entries after render-state updates and cache a draw list containing
protocol, image id/generation, layer/z, destination viewport rectangle, and
source pixel rectangle. Sort by layer and z, clip to the content area, and draw
fixed, virtual and relative placements through one path. Suppress U+10EEEE glyph
painting but preserve cell backgrounds/decorations. Remove the old drawing
iterator so fixed placements are never painted twice. Cache QImage pixels per
image generation rather than clearing all images on a storage mutation.

Image placement generation and pixel generation must participate in back-buffer
invalidation, including the last image being deleted or scrolled away. Prefer
correct full redraw while visible images exist over retaining incorrect partial
composition; preserve the existing incremental text path when no images exist.
Opacity, all three layers, negative coordinates, alternate screens and terminal
reset must retain their documented behavior. Bound retained QImage cache entries
to currently needed images so deletion does not leak converted pixels.

Drive ghostty_kitty_graphics_animation_tick from a monotonic clock per terminal.
Use a single-shot Qt timer at the returned deadline, with an 8ms minimum delay;
stop when no animation is playing or widget/window is hidden/minimized. Resume
by ticking on the next visible frame. Fetch image handles again after tick.
Respect synchronized output rather than letting image timers publish a held
frame. Snapshot rendering must not turn hidden panes into continuously animated
widgets. Local-file, temporary-file and shared-memory media remain disabled.

Use captured protocol streams and pixel checks for fixed, virtual and relative
placements, image replacement/deletion, crop/z order, scrolling and main/alternate
screen transitions. Test a two-frame animation with deterministic frame state
and bounded waits, no idle timer when stopped/hidden, and existing image cache
regressions. Upstream example/c-vt-kitty-graphics/src/main.c supplies animation
and placeholder fixtures. Do not claim complete protocol conformance without
covering the remaining transfer and malformed-input matrix in docs/TODOs.md.
