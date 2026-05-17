# Issue 24: Selection Drag Does Not Auto-scroll Outside Viewport

## Problem

Dragging an active text selection above or below the terminal widget stopped at
the visible viewport edge. The viewport did not continue scrolling while the
mouse remained outside the widget.

## Root Cause

`TerminalWidget::mouseMoveEvent()` only extended the selection when Qt delivered
a mouse move event. The position-to-cell helpers clamp coordinates into the
visible terminal area, which is correct for hit testing, but there was no
separate timer-driven path to continue scrolling after the pointer moved outside
the content rectangle and stopped generating useful in-widget movement.

## Fix

Add a terminal-widget selection auto-scroll timer that runs only while a local
left-button selection drag is active and the last mouse position is above or
below the content rectangle. Each tick scrolls the Ghostty viewport by one row
and extends the selection endpoint using the same single-, word-, or line-mode
selection logic used by normal drag movement.

The behavior stays in `qtghostty` because it is generic terminal interaction
state, not app-level tab, pane, or menu behavior.

## Verification

- Added terminal-widget tests for drag-selection auto-scroll above the viewport.
- Added terminal-widget tests for drag-selection auto-scroll below the viewport.
- Verified the new tests first failed without the implementation and passed
  after the timer-driven scroll path was added.
