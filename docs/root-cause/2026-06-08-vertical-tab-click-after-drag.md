# Vertical Tab Clicks Swallowed After Drag Support

## Symptom

After vertical tabs gained drag sorting, clicking a vertical tab became easy to
miss. A small pointer movement while pressing a tab could leave the current tab
unchanged.

## Cause

`ClickableSection` started treating any left-button movement at or beyond
`QApplication::startDragDistance()` as a drag. The reorder target calculation
also considered the dragged source section as an insertion boundary, so a small
movement inside the same tab could be misclassified as a reorder. On release,
the drag path skipped tab activation, which made ordinary clicks feel unreliable.

## Fix

`ClickableSection` now uses a larger vertical-sidebar drag start distance, so
ordinary press movement stays on the click path. `targetIndexForPosition()`
ignores the dragged source section when choosing an insertion point. Drag
completion now commits the current preview order instead of recalculating a new
target from the release position. `VerticalTabSidebar` also tracks whether
preview actually changed tab order, and `ClickableSection::mouseReleaseEvent()`
activates the tab when the drag path completes without a reorder. This preserves
normal click behavior for small in-place press movement while keeping real
reorders unchanged.

## Verification

- `QT_QPA_PLATFORM=offscreen ./build/tests/test_main_window -platform offscreen testVerticalSidebarSmallPressMovementStillSwitchesTab`
- `QT_QPA_PLATFORM=offscreen ./build/tests/test_main_window -platform offscreen testVerticalSidebarTabClickSwitchesCurrentTab testVerticalSidebarDragReordersTabs`
