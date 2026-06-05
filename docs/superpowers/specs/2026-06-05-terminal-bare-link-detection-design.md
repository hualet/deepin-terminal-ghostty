# Terminal Bare Link Detection Design

## Goal

Add bare link detection to `TerminalWidget` without weakening terminal output,
rendering, or paint performance. The first version detects these protocol links:

- `http://...`
- `https://...`
- `ssh://...`
- `mailto:...`
- `file://...`

Existing OSC 8 hyperlinks remain supported and keep priority over detected text
links. The implementation should be split into two reviewable phases:

1. Refactor OSC 8 hover underlining to use cached ranges with no behavior change.
2. Add bare protocol link detection on top of that range infrastructure.

## Non-Goals

- No generic file path detection such as `/tmp/file.txt` or `src/main.cpp:12`.
- No issue, commit, or repository reference detection.
- No scanning during PTY input, render-row processing, or full paint cycles.
- No app-specific behavior in `src/libqtghostty/`.

## Architecture

The feature belongs mostly in `src/libqtghostty/TerminalWidget.*` because link
detection is a terminal-facing capability. `src/app/TermPane.cpp` should keep a
single interaction path for context menus and activation, and should not know
whether a link came from OSC 8 metadata or bare text detection.

`TerminalWidget` will expose an explicit combined link query while preserving
OSC 8-specific APIs:

- `hyperlinkUriAtPosition()` remains OSC 8-only.
- `linkUriAtPosition()` returns either an OSC 8 URI or a bare detected URI.
- `linkHovered()` and `linkActivated()` are emitted for combined links.
- Existing `hyperlinkHovered()` and `hyperlinkActivated()` remain OSC 8-specific
  compatibility signals.

`TermPane` should be migrated to the combined `link*` APIs after auditing its
current uses. The current codebase has two app-layer uses:

- activation wiring in `TermPane::setupTerminalConnections()`;
- context menu lookup in `TermPane::showTerminalContextMenu()`.

Both should move to `linkActivated()` and `linkUriAtPosition()` so bare links can
open from Ctrl-click and the context menu. Tests that explicitly cover OSC 8
behavior should stay on the `hyperlink*` APIs or add parallel assertions for the
new `link*` APIs.

Both `hyperlinkUriAtPosition()` and `linkUriAtPosition()` should remain `const`
so callers can query from const contexts. Link scan caches are implementation
detail and should therefore be stored in `mutable` members, with mutation limited
to lookup-time cache fill and LRU bookkeeping.

Internal lookup order:

1. `hyperlinkUriAtPosition()` queries Ghostty for an OSC 8 hyperlink only.
2. `linkUriAtPosition()` first calls the OSC 8 lookup.
3. If OSC 8 is present, return it.
4. Otherwise, lazily scan the visible text row containing the cell.
5. Return the detected range if the cell lies inside it.

## Data Model

`TerminalWidget` should cache detected bare links per screen row:

```cpp
struct LinkRange {
    QString uri;
    int screenRow = 0;
    int startCol = 0;
    int endCol = 0; // exclusive
    bool osc8 = false;
};

struct LinkScanCacheEntry {
    QString text;
    QVector<LinkRange> ranges;
};
```

The cache key is the screen row. The cached text is the validation token: when
`textForScreenRow(screenRow)` differs from the cached text, rescan that row.
This avoids needing a separate row version counter while remaining correct when
scrollback, imports, or terminal mutations change row contents.

The cache must be bounded. Use a small LRU cap, such as 256 rows, because the
user can hover while browsing deep scrollback. On insertion beyond the cap,
evict the least recently used row entry. This keeps memory bounded without
coupling cache lifetime to viewport movement.

The current hover state should store both URI and range:

```cpp
LinkRange m_hoverLink;
bool m_hoverLinkActive = false;
```

This lets overlay painting underline the current range directly instead of
asking Ghostty or rescanning every cell. It also lets signal emission distinguish
OSC 8 links from bare detected links.

## Implementation Phases

### Phase 1: OSC 8 Range Refactor

Move the existing OSC 8 hover underline behavior from full-grid cell scanning to
range-based drawing without adding bare link detection.

Requirements:

- `hyperlinkUriAtPosition()` keeps its current OSC 8-only meaning.
- `hyperlinkHovered()` and `hyperlinkActivated()` keep their current behavior.
- Hovering an OSC 8 link expands a range by walking left and right from the
  hovered cell while adjacent cells return the same OSC 8 URI.
- `renderOverlays()` draws only `m_hoverLink` when it is active and visible.
- The current full-grid loop that calls `hyperlinkUriAtViewportCell()` for every
  visible cell must be removed.

Phase 1 tests should cover existing OSC 8 hover, activation, cursor, leave, and
underline behavior before Phase 2 adds bare detection. This makes any OSC 8
regression attributable to the refactor rather than to scanner logic.

### Phase 2: Bare Protocol Detection

Add `linkUriAtPosition()`, `linkHovered()`, and `linkActivated()` as combined
link APIs, then migrate app-layer opening and context-menu behavior to those
combined APIs.

Requirements:

- OSC 8 remains highest priority.
- Bare link detection is queried only when no OSC 8 link exists at the cell.
- Bare link ranges use the same `m_hoverLink` overlay path introduced in Phase 1.
- `hyperlink*` APIs remain available for OSC 8-only tests and callers.

## Scanner Rules

Use a small hand-written scanner instead of a broad regular expression.

The scanner walks a row string and looks for `:`. For each colon, it extracts the
scheme immediately to the left and accepts only:

- `http`
- `https`
- `ssh`
- `mailto`
- `file`

Rules by scheme:

- `http`, `https`, `ssh`, and `file` require `://`.
- `mailto` accepts `mailto:` followed by a payload containing exactly the
  minimum useful shape: at least one `@`, at least one character before it, and
  at least one character after it.
- The link extends until whitespace or terminal control replacement characters.
- Trim trailing punctuation commonly adjacent to prose: `.`, `,`, `;`, `:`,
  `!`, `?`, `)`, `]`, `}`, `>`, `"`, `'`, and `` ` ``.
- Trim closing delimiters only when unmatched within the detected link. For
  example, keep `https://example.com/a(b)` but trim the final `)` in
  `(https://example.com)`.
- Reject empty or hostless `http`, `https`, and `ssh` payloads.
- Accept `file:///absolute/path` with an empty host and non-empty absolute path.
  Reject `file://` with no host and no absolute path.

The scanner returns column spans in terminal cell coordinates. For the first
version, it can treat each QString code unit as one cell because the target
schemes and delimiters are ASCII. This is an explicit first-version limitation:
if wide characters, combining marks, or emoji appear before or inside a detected
link on the same row, the QString-index-to-cell mapping can be off for hover hit
testing and underline drawing. Tests should cover ASCII links, which are the
supported first-version target.

## Interaction Flow

Hover:

1. `mouseMoveEvent()` records the mouse position as it does today.
2. `updateHyperlinkHoverState()` is kept for OSC 8 behavior in Phase 1.
3. Phase 2 introduces a unified link hover update that queries `linkUriAtPosition()`.
4. If the active link changes, update cursor, emit `linkHovered(uri)`, emit
   `hyperlinkHovered(uri)` only for OSC 8 links, and repaint only the affected
   overlay area when practical.

Ctrl-click:

1. `mousePressEvent()` keeps checking link activation before mouse tracking.
2. If a link exists at the click position, emit `linkActivated(uri)` and consume
   the event.
3. If that link is OSC 8, also emit `hyperlinkActivated(uri)` for compatibility.
4. Otherwise, continue with existing mouse tracking or selection behavior.

Context menu:

`TermPane::showTerminalContextMenu()` should call `linkUriAtPosition(localPos)`.
It receives either an OSC 8 URI or bare URI and keeps the existing "Copy Link"
and "Open Link" actions.

## Paint Behavior

Do not scan links from `paintEvent()`, `renderTerminal()`, `renderRow()`, or
`renderOverlays()`.

`renderOverlays()` should draw the current hover underline from cached hover
range state:

- compute the visible viewport row from `screenRow - scrollOffset`;
- ignore the range if it is outside the viewport;
- draw from `startCol` to `endCol`.

The existing full-grid OSC 8 hover underline scan must be removed in Phase 1,
before bare link detection is added. OSC 8 hover underlining must use the same
range-based drawing path that Phase 2 later reuses for bare links: expand left
and right from the hovered cell while adjacent cells return the same OSC 8 URI,
store the result in `m_hoverLink`, then draw that single range in
`renderOverlays()`. That expansion happens only when the hover target changes,
not on every paint.

## Cache Invalidation

The row cache is naturally validated by row text. It is still useful to clear it
on broad terminal changes to cap memory and avoid stale rows:

- after VT import;
- after resize that changes columns;
- after scrollback setting changes;
- after terminal reset if one is introduced;
- when clearing or replacing terminal content.

Normal PTY output does not need eager invalidation. A row is rescanned only when
queried and its current text differs from the cached text.

Viewport scrolling does not need to clear the row cache, because cache keys use
absolute screen rows and entries are validated by text. It must still invalidate
the active hover range when the viewport scroll offset changes, then repaint the
overlay. This avoids a stale `m_hoverLink` surviving after the visual cell under
the pointer has changed. As a defensive fallback, `renderOverlays()` must also
skip any active range whose `screenRow - scrollOffset` is outside the viewport.

## Performance Constraints

- No work proportional to terminal output size in `onPtyDataReceived()`.
- No full-screen link scanning in paint or render paths.
- Hover over the same row should reuse cached scan results.
- Underline painting should be proportional to one hovered range, not all cells
  on screen.
- Link detection memory is bounded by a fixed-size LRU cache, clearing on broad
  changes, and storing only rows the user actually hovers or queries.

## Testing

Add focused tests in `tests/test_terminal_widget.cpp`:

- Phase 1: OSC 8 hover, activation, cursor, leave, and underline behavior still
  pass after replacing full-grid hover underlining with range-based drawing.
- Bare `http`, `https`, `ssh`, `mailto`, and `file` links are detected on hover.
- Ctrl-click activates a bare detected link.
- Context-menu query path returns a bare detected link through
  `linkUriAtPosition()`.
- `hyperlinkUriAtPosition()` remains OSC 8-only and returns empty for bare links.
- OSC 8 hyperlinks take priority over bare text in the same cells.
- Trailing punctuation is excluded, for example `(https://example.com).`, and
  backticks are trimmed from prose-delimited links.
- Balanced parentheses inside URLs are preserved while unmatched closing
  delimiters around URLs are trimmed.
- Repeated hover on the same row does not rescan the row; expose a testing-only
  scan counter if needed.
- Rendering a frame without hover does not trigger bare link scans.
- Hovering a link and then scrolling invalidates the active hover range and stops
  drawing the stale underline until the next mouse hover query.
- Drag selection across a detected link still selects and copies the raw terminal
  text; link detection must not change selection boundaries or copy behavior.

Verification commands:

```bash
cmake -B build -DBUILD_TESTING=ON
cmake --build build
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget
QT_QPA_PLATFORM=offscreen ctest --output-on-failure
```
