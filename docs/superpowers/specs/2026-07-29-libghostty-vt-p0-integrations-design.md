# libghostty-vt P0 Integrations Design

## Scope

Integrate the three highest-value, low-risk capabilities unlocked by the
vendored `libghostty-vt` upgrade:

1. make the scrollback line setting precise and effective at runtime;
2. delegate terminal clipboard protocols to Ghostty's semantic callback;
3. use Ghostty's absolute viewport-row operation for scrollbar positioning.

The work stays inside `qtghostty`. Application-specific presentation and
policy are not introduced in this batch.

## Scrollback Limits

`TerminalWidget` will configure both of Ghostty's independent limits:

- `GHOSTTY_TERMINAL_OPT_SCROLLBACK_MAX_LINES` receives the user-visible line
  setting;
- `GHOSTTY_TERMINAL_OPT_SCROLLBACK_MAX_BYTES` retains the existing byte
  budget as a memory safety ceiling.

Both limits are applied during terminal setup and whenever
`setScrollbackLines()` changes the value. Ghostty prunes against whichever
limit is reached first. A failed runtime update is logged and leaves the
widget usable.

Tests will query Ghostty's configured values through testing-only accessors,
proving that initialization and live updates reach the terminal rather than
only changing a C++ member.

## Clipboard Writes

`TerminalWidget` will register
`GHOSTTY_TERMINAL_OPT_CLIPBOARD_WRITE`. The callback receives complete,
decoded clipboard writes after Ghostty has normalized OSC 52 and iTerm2
OSC 1337 Copy.

The callback will:

- map the standard location to `QClipboard::Clipboard`;
- map selection and primary locations to `QClipboard::Selection`;
- reject unsupported selection clipboards;
- commit all MIME representations atomically through one `QMimeData`;
- clear the requested clipboard when Ghostty supplies zero representations;
- return the matching `GhosttyClipboardWriteResult`.

Clipboard read requests remain ignored by Ghostty. The existing raw OSC
scanner will continue handling project shell-integration markers but will no
longer parse clipboard writes.

Tests will retain OSC 52 coverage and add iTerm2 Copy coverage so the new
semantic callback, rather than the old raw scanner, is required to pass.

## Absolute Viewport Scrolling

`scrollViewportToOffset()` will clamp the requested offset and send
`GHOSTTY_SCROLL_VIEWPORT_ROW` directly. It will then perform the same render
dirtying, viewport-state publication, and repaint scheduling as relative
scrolling.

The existing absolute-scroll behavior test will verify top, middle, and
bottom positions round-trip through Ghostty's scrollbar row space.

## Verification

Each behavior is developed test-first. Verification consists of:

- the focused new `TerminalWidget` tests;
- the complete `TerminalWidget` test binary with the offscreen platform;
- a fresh build;
- `clang-format --dry-run --Werror` on changed C++ files;
- `git diff --check`.

