# Overview transition capture must not flush hidden geometry events

## Evidence

While adding overview enter/exit animations, the transition captured the
content host with `QWidget::grab()`. `testOverviewPreservesTerminalGeometry`
then failed: a never-shown split terminal in a hidden tab changed from
`100x30` to `49x30`, which resizes its PTY. Skipping the capture made the
test pass. A backtrace from `TerminalWidget::resizeEvent()` showed
`QWidget::grab()` -> `QWidgetPrivate::prepareToRender()` ->
recursive `sendPendingMoveAndResizeEvents()`.

## Cause

`prepareToRender()` delivers pending move/resize events through the whole
top-level window, not just the grabbed subtree. Grabbing the overview or
even the visible pane therefore delivered the hidden splitter's pending
resize and laid out its terminals. Rendering panes through
`TermPane::renderPreview()` was not enough, since the overview grab
triggered the same flush.

## Fix

Captures go through `MainWindow::grabOverviewFrame()`, which uses a scoped
guard that clears `WA_PendingMoveEvent`/`WA_PendingResizeEvent` on hidden
widgets and restores them afterwards. Hidden widgets still get those
events when shown, as they would without the overview.

## Verification

- `testOverviewPreservesTerminalGeometry` now also checks sizes and grids
  after exiting with the animation.
- `testOverviewAnimatesEnterAndExit` covers both tab modes, immediate
  state and focus changes, mouse transparency, and interrupted transitions.
- Full offscreen `test_main_window` passed (129 passed, 1 pre-existing skip).
