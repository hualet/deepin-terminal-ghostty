# Split Pane Flicker Root Cause

## Summary

Splitting a terminal pane or closing a split could briefly flash the UI. The issue happened during the
synchronous Qt widget tree rebuild used by `TermPane`.

## Investigation Notes

`TermPane::splitTerminal()` and `TermPane::removeTerminal()` both restructure `QSplitter` trees by detaching
visible `TerminalWidget` instances with `setParent(nullptr)` and inserting them into another splitter or the
root layout. Qt emits hide/show events for visible widgets during this reparenting. Before this fix, those
hide events occurred while `TermPane` updates were still enabled, so an intermediate empty or partially rebuilt
layout could be painted.

The first fix blocked updates on the whole `TermPane` while reparenting. That removed the intermediate blank
paint, but it was still too broad for nested splits. In a left/right layout, splitting the right pane again
restored updates on the whole pane and caused the unchanged left terminal to repaint. This matched the remaining
case where a running Codex pane on the left flashed while splitting a different pane on the right.

The terminal engine and PTY path were not involved. The pane list, active pane tracking, and terminal session
ownership remained correct; the problem was visible repaint during app-layer splitter orchestration.

## Root Cause

The app layer rebuilt the split-pane widget tree with normal updates enabled, then initially fixed that by
blocking too large a repaint scope. The remaining nested-split flicker came from repainting sibling terminal
widgets that were not part of the split operation.

## Fix

`TermPane` now pauses updates with a scoped blocker while it performs split-tree reparenting for:

- splitting the current terminal
- closing a terminal inside a splitter
- closing all non-current terminals

For the first root split, updates are blocked on the whole pane because the root widget is being replaced. For
nested splits, updates are blocked only on the terminal being moved, and the parent splitter uses
`QSplitter::replaceWidget()` to swap in the new child splitter in place. Updates are restored after the affected
subtree reaches its final state, so Qt paints the completed layout instead of the intermediate detached state
without forcing unchanged sibling terminals to repaint.

## Regression Coverage

Added focused `TestMainWindow` coverage that observes terminal hide/show events during split and close
operations. The tests verify that hide events caused by reparenting do not happen while `TermPane` updates are
enabled.

Added `TestMainWindow::testNestedSplitDoesNotRepaintUnchangedSiblingTerminal` for the left/right then right-side
split case. The test fails on the broad `TermPane` update blocker because the unchanged left terminal receives
paint events, and passes when nested split repainting is limited to the affected branch.
