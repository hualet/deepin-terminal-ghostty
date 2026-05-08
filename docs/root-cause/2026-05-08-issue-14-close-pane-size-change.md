# Issue #14: Closing a Nested Pane Changed Parent Split Sizes

## Symptom

With a side-by-side split, splitting the right pane again, then closing the
nested pane caused the original side-by-side panes to change width. The
remaining nested pane visibly grew and the sibling pane shrank.

## Root Cause

When `TermPane::promoteSingleChildSplitter()` removed a nested splitter, it
replaced that splitter in the parent and then inserted the nested splitter's
remaining child size into the parent splitter's size list. That value belonged
to the child splitter's orientation, not the parent splitter's orientation.
Using it as a parent size changed unrelated sibling pane geometry.

## Fix

Nested splitter promotion now preserves the parent splitter's existing sizes
when replacing the nested splitter with its only remaining child. The parent
layout keeps the same allocation while the child branch collapses.

## Verification

- Added `testClosingNestedSplitPreservesParentSplitterSizes`, which reproduces
  the 1/2 then 2/3 split layout and verifies the parent pane widths stay stable
  after closing pane 3.
- Ran the focused split-close `test_main_window` cases under the Qt offscreen
  platform.
