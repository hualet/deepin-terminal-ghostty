# Stale Tab Data Activation Crash

## Symptom

Clicking a vertical tab could crash in `MainWindow::onTabCurrentChanged()` while reading
`currentTitle` from the active terminal:

```text
QObject::property(char const*) const
MainWindow::onTabCurrentChanged at src/app/MainWindow.cpp
VerticalTabSidebar::tabActivated
ClickableSection::mouseReleaseEvent
```

## Root Cause

`onTabCurrentChanged()` trusted `DTabBar::tabData(index)` as the stack index for the
selected tab. If that tab data became stale while the sidebar, tab records, and stack
were being refreshed, `QStackedWidget::setCurrentIndex()` could fail to move to the
intended pane. The handler then used `currentPane()`, which reads the stack's current
widget rather than the pane represented by the activated tab.

That left the handler able to focus or read properties from the wrong pane, including
objects in the window between stack removal and deferred deletion.

## Fix

Use `m_tabs[index].pane` as the source of truth when a tab is activated. The handler now
recomputes the stack index from the pane, repairs stale tab data, and ignores activation
if the pane is no longer present in the stack.

## Verification

- Added `testVerticalSidebarTabClickRecoversFromStaleTabData`.
- Confirmed the new test failed before the fix with `stack->currentIndex()` staying at
  the old page.
- Verified the new test plus existing vertical-sidebar click and drag tests pass.
