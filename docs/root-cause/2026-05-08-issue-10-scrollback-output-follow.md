# Issue #10: Output Could Pull a Scrolled-Back View to the Bottom

## Symptom

When terminal output was still arriving, scrolling back with the mouse should
keep the viewport on historical content. New output must not force the viewport
back to the newest line until the user explicitly returns to the bottom.

## Root Cause

The issue was caused by a mismatch between the scrollback contract exposed by
the app and the storage budget passed to ghostty:

`GhosttyTerminalOptions.max_scrollback` is documented in the C header as
"Maximum number of lines" but is interpreted as bytes throughout ghostty's
internal implementation. The app initially passed `m_scrollbackLines` (e.g.
1000) directly as `max_scrollback`, giving ghostty only about 1000 bytes of
scrollback storage.

The first byte-budget fix used ghostty's upstream default of 10MB. Runtime
diagnostics showed that this was still too small for high-volume output in this
wrapper: the terminal retained only about 5000 scrollable rows, and each PTY
flush pruned hundreds of old rows. Once the user-scrolled viewport reached
offset 0, the viewed history had been deleted and the visible top of scrollback
changed on every flush.

An intermediate app-layer attempt saved and restored the viewport around every
PTY write. That duplicated ghostty's own viewport policy and made the Qt wrapper
responsible for behavior that belongs in the VT layer.

## Fix

Convert the configured scrollback line count to a byte budget before passing it
to ghostty, with a 100MB minimum budget for each terminal surface. Then let
ghostty own viewport retention during PTY writes. The Qt scrollbar remains a
mirror/control surface for `GHOSTTY_TERMINAL_DATA_SCROLLBAR` and
`ghostty_terminal_scroll_viewport`; it does not implement separate scroll state.

## Verification

- Added terminal-widget coverage for preserving scrollback position after
  absolute scrolling, mouse-wheel scrolling, and pending-output flushes.
- Added terminal-widget coverage proving unchanged scrolled-back viewport
  content remains stable after new output.
- Added app-layer scrollbar coverage to verify the floating scrollbar stays at
  the user's selected offset when new output arrives.
- Ran the focused terminal and main-window scrollback tests under the Qt
  offscreen platform.
