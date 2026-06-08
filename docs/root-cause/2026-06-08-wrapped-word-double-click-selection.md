# Wrapped Word Double-Click Selection

## Symptom

Double-clicking a filename only selected the visible screen-row fragment when the
filename was soft-wrapped by the terminal grid. For example, clicking the
continuation row of a long package filename selected only that row's substring
instead of the full filename.

## Root Cause

`TerminalWidget::selectWordAt()` used `wordBoundsAt()`, which scans word
boundaries only within a single Ghostty screen row. `selectedText()` already
knows how to join selected soft-wrapped rows, but the selection range created by
double-click never included adjacent wrapped rows. The second mouse release also
ran through `extendWordSelection()`, which used the same single-row word bounds
and could shrink the selection back to the clicked row fragment.

## Fix

Add reusable Ghostty grid helpers for cell codepoints, cell width, and row wrap
flags. Build word selection ranges with `wordRangeAt()`, which expands across
adjacent soft-wrapped rows while preserving the existing word-boundary rules.
Use that range for both initial double-click selection and word-mode selection
extension.

## Verification

- `./build/tests/test_terminal_widget -platform offscreen testDoubleClickSelectsWrappedFilename`
- `./build/tests/test_terminal_widget -platform offscreen testDoubleClickSelectsWord testDoubleClickSelectsWordWithDots testDoubleClickSelectsWrappedFilename testDoubleClickSelectsWordWithHyphens testDoubleClickSelectsWordWithUnderscores testDoubleClickSelectsWordWithMixedPunctuation testDoubleClickSelectsPath testDoubleClickDragExtendsByWord`
