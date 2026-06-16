# Vertical Tab Drag Was Blocked by the Title Button

## Symptom

When the vertical sidebar showed tabs with no expanded pane list, those tabs
were difficult or impossible to drag into a new order. Tabs with multiple
panes still dragged normally.

After forwarding the button events, releasing a drag could also crash in
`MainWindow::onTabCurrentChanged()` while `moveTabById()` was applying the
new order.

## Cause

The draggable surface for a vertical tab was split across multiple child
widgets. Tabs with multiple panes had an extra transparent pane list area, so
users could start the drag there. Single-pane tabs only exposed the title
button and a few other interactive children, and the title button consumed the
mouse gesture before `ClickableSection` could turn it into a tab drag.

The reorder logic itself was fine. The gesture never reached it from the title
button path.

There were two follow-on issues once the gesture reached the drag path:

- forwarded mouse events recomputed the global position from the child widget
  after preview reflow had already moved that widget, so release could use a
  stale target
- `moveTabById()` restored the current `DTabBar` index while the tab records
  and tab widget data were being synchronized, which allowed a synchronous
  `currentChanged` signal to enter `onTabCurrentChanged()` in the middle of
  the reorder

## Fix

Forward mouse press, move, and release events from the tab title button back
to the parent `ClickableSection`. The section still owns click and drag
handling, while the button keeps its existing click behavior for accessibility
and keyboard/UI consistency.

Use the event's real global position when forwarding from title children, and
apply one final preview at drag release before committing the order. During
`moveTabById()`, block `DTabBar` signals while tab text, tab data, and current
index are synchronized, then run current-tab handling after the state is stable.

## Verification

- Added a regression test that starts a drag from the title button of a
  single-pane tab.
- Added a regression test that drags a later inactive single-pane tab forward
  from its title button and keeps the previously current tab active.
- Ran focused offscreen tests:
  - `testVerticalSidebarTabClickSwitchesCurrentTab`
  - `testVerticalSidebarSmallPressMovementStillSwitchesTab`
  - `testVerticalSidebarPressedMoveDoesNotPropagateAsWindowDrag`
  - `testVerticalSidebarTabClickRecoversFromStaleTabData`
  - `testVerticalSidebarDragReordersTabs`
  - `testVerticalSidebarTabButtonDragReordersTabs`
  - `testVerticalSidebarInactiveTabButtonDragKeepsCurrentTab`

## Prevention

For future vertical-sidebar interaction changes, verify both of these paths:

- drag starting from the title/button area of a single-pane tab
- drag starting from the expanded pane area of a multi-pane tab
