# Overview capture must not lay out hidden tabs

## Evidence

During workspace overview implementation, the first capture path called
`TermPane::render()` for both visible and hidden stack pages. The focused
`testOverviewPreservesTerminalGeometry` test failed after entering the
mode: a hidden terminal changed from `100x30` to `399x550` pixels. Its
session identity remained the same, but `TerminalWidget::resizeEvent()`
updates the terminal grid and resizes the PTY, making this a functional
side effect rather than just a thumbnail layout difference.

## Cause

Qt widget rendering prepares hidden widget hierarchies for painting and
can activate their pending layouts. Capturing a whole `TermPane` therefore
cannot promise to preserve existing split-terminal geometry. The newly
created split in the test had not completed its layout when the parent tab
was hidden, making the side effect directly observable.

## Implementation

`TerminalWidget::renderSnapshot()` draws from the terminal renderer to a
bounded image without invoking widget rendering or changing geometry.
The method is a generic terminal capability in `qtghostty`.

`TermPane::renderPreview()` composes those snapshots according to the
existing splitter tree and weights. Only this app component knows about
split layout. The overview keeps the original widget hierarchy and renders
only the resulting scaled images.

## Verification

- The original geometry-preservation test now passes through entry and
  periodic refresh, checking widget sizes, grid dimensions, terminal
  pointers, and PTY session pointers.
- Hidden-tab preview tests confirm that output from both split terminals
  appears in the composited image using distinct ANSI background colors.
- Overview integration tests exercise focus restoration, tab activation,
  new/close actions, filtering, reorder/session-exit handling, and both tab
  navigation orientations.

Full CTest verification passed all 11 suites using an isolated D-Bus session
and `QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME=deepin`. The isolated
session avoids an existing desktop terminal control service invalidating
the CLI test's no-service assumption. Palette QA uses the Deepin platform
theme so DTK does not freeze the offscreen application's startup palette.
Chinese light, dark, and narrow overview screenshots were also inspected.

## Preview navigation revision

Horizontal tab clicks previously entered `onTabCurrentChanged()`, which
updated the selected tab while keeping the preview open. Card clicks used
a separate callback that also dismissed the preview. Clicking the current
tab additionally emits no `currentChanged` signal at all.

Both physical `tabBarClicked` events and card activation now use the same
stable-ID method to select the tab, dismiss preview, and restore its active
split focus. Programmatic current changes remain separate so reordering
and asynchronous session exit do not unexpectedly dismiss preview.

Vertical preview hides the whole sidebar/terminal splitter container.
Hiding only the sidebar would redistribute splitter space and resize the
PTY; preserving the container geometry avoids that side effect. The
overview fills the content host, and the saved vertical layout returns on
exit. Regression coverage checks hide/restore, sizes, focus, real horizontal
clicks on current/different/reordered tabs, and mode changes during preview.
