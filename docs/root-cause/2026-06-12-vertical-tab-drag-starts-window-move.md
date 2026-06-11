# Vertical Tab Drag Starts Window Move

## Symptom

After fixing unreliable vertical-tab clicks, dragging a vertical sidebar tab no
longer reliably started tab reordering. The same gesture could instead move the
application window.

## Cause

The click fix raised the vertical-tab reorder threshold so ordinary press
movement would not be misclassified as a tab drag. That fixed the click path,
but it left a gap between Qt's normal window-drag distance and the larger
vertical-tab reorder distance.

`ClickableSection::mouseMoveEvent()` only accepted left-button move events after
the larger reorder threshold was reached. Moves in the gap were passed to the
base widget implementation, which ignored them. In a real window this allowed
the drag gesture to propagate to the window-moving behavior before the tab
sidebar had a chance to begin reordering.

## Fix

`ClickableSection` now owns the full left-button gesture after it accepts the
press. It accepts all left-button move events while pressed, starts sidebar
reordering only after the larger vertical-tab threshold is reached, and keeps
previewing once the drag state has started.

This keeps click-sized movement on the click path, prevents the pre-drag gap
from becoming a window drag, and preserves real vertical-tab reorder behavior.

## Verification

- Confirmed the new regression test failed before the fix because the pre-drag
  move event was not accepted.
- `cmake --build build --target test_main_window`
- `QT_QPA_PLATFORM=offscreen ./build/tests/test_main_window -platform offscreen testVerticalSidebarPressedMoveDoesNotPropagateAsWindowDrag testVerticalSidebarSmallPressMovementStillSwitchesTab testVerticalSidebarTabClickSwitchesCurrentTab testVerticalSidebarDragReordersTabs`

## Prevention

For vertical-sidebar gesture changes, verify all three gesture zones together:

- Click-sized movement still activates the tab.
- Movement between the window-drag threshold and the reorder threshold remains
  owned by the sidebar.
- Real drag movement still reorders tabs and survives sidebar refreshes.
