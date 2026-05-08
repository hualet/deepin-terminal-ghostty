# Issue #11 Inverse Text Color Root Cause

GitHub issue: <https://github.com/hualet/deepin-terminal-ghostty/issues/11>

## Summary

In zsh tab completion, the selected candidate could look like a solid block
because the text foreground matched the highlighted background.

The affected sequence is plain reverse video, such as `SGR 7`, without an
explicit foreground color. zsh completion commonly uses that style for the
active item.

## Root Cause

`TerminalWidget::renderRow()` swapped `fgColor` and `bgColor` when
`style.inverse` was set. That made the background resolve correctly: the
cell background became the terminal default foreground.

After the swap, the text color still used the pre-inversion `hasFg` decision:

```text
hasFg ? resolved fg : default foreground
```

For reverse video with no explicit foreground, `hasFg` is false, so the
renderer chose the default foreground for the text. The inverse background was
also the default foreground, making the glyphs invisible.

## Fix

When `style.inverse` is active, the renderer now always uses the already
swapped `fgColor` for text. This preserves normal cells with no explicit
foreground while making reverse-video cells use the resolved inverse
foreground.

## Regression Coverage

Added `TestTerminalWidget::testRendersInverseTextWithDefaultColors`.

The test applies a high-contrast theme, renders `SGR 7` text with no explicit
foreground, and verifies that:

- the cell background uses the default foreground color
- the glyph pixels use the default background color

## Verification

Commands run:

```bash
cmake --build build --target test_terminal_widget
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget testRendersInverseTextWithDefaultColors
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget
clang-format --dry-run --Werror src/libqtghostty/TerminalWidget.cpp src/libqtghostty/TerminalWidget.h tests/test_terminal_widget.cpp
git diff --check
cmake --build build
```

Results:

- Focused inverse-color regression passed.
- Full `test_terminal_widget` passed: 72 tests.
- Formatting validation passed.
- `git diff --check` passed.
- Full project build passed.
