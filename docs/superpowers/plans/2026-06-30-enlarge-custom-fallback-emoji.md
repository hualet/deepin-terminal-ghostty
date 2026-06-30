# Enlarge Custom-Fallback Emoji Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enlarge ordinary custom-fallback color emoji modestly while retaining terminal-cell clipping and mixed-CJK spacing.

**Architecture:** Keep the change inside `TerminalWidget::emojiFallbackCellImage()`, which already owns fallback bitmap sizing. Strengthen the existing pixel-footprint regression before changing the horizontal inset, then validate automated geometry and rendered probe output.

**Tech Stack:** C++20, Qt6 Widgets and Qt Test, CMake, Fontconfig/FreeType/HarfBuzz

---

### Task 1: Strengthen the fallback emoji size regression

**Files:**
- Modify: `tests/test_terminal_widget.cpp:998`

- [ ] **Step 1: Raise the expected fallback footprint**

In `testEmojiFallbackRendererSizedRelativeToCell()`, replace the 40% threshold with 60% and use this comment:

```cpp
// With a 1.5px horizontal inset, the fallback image gains roughly one pixel at
// common terminal cell widths. Require at least 60% so the current 2px inset
// fails while leaving room for rasterization differences across environments.
```

- [ ] **Step 2: Run the focused test and verify RED**

Run `cmake --build build --target test_terminal_widget`, then:

```bash
cd build && QT_QPA_PLATFORM=offscreen ./tests/test_terminal_widget testEmojiFallbackRendererSizedRelativeToCell
```

Expected: FAIL because the current 2-pixel inset renders a 5-pixel footprint in a 9-pixel cell and does not meet 60%.

### Task 2: Enlarge ordinary custom-fallback emoji

**Files:**
- Modify: `src/libqtghostty/TerminalWidget.cpp:2275`

- [ ] **Step 1: Reduce only the ordinary horizontal inset**

Use:

```cpp
const bool warningSign = isWarningSignEmoji(*codepoints);
const qreal kEmojiHorizontalInset = warningSign ? 1.0 : 1.5;
const qreal kEmojiVerticalInset = warningSign ? 1.0 : 2.0;
```

Update the adjacent comment to explain that 1.5 pixels modestly enlarges ordinary emoji while retaining a mixed-script boundary.

- [ ] **Step 2: Run focused tests and verify GREEN**

```bash
cmake --build build --target test_terminal_widget
cd build && QT_QPA_PLATFORM=offscreen ./tests/test_terminal_widget \
  testEmojiFallbackRendererSizedRelativeToCell \
  testEmojiFallbackDoesNotOverlapFollowingCjkText \
  testEmojiFallbackRendererKeepsNarrowEmojiInOneCell \
  testEmojiFallbackRendererSizesVariationEmoji \
  testFallbackGlyphDoesNotOverlapNextCell \
  testSingleCodepointFallbackGlyphDoesNotClip
```

Expected: all listed tests PASS.

### Task 3: Verify rendered output and formatting

**Files:**
- Modify: `tests/test_terminal_widget.cpp`
- Modify: `src/libqtghostty/TerminalWidget.cpp`

- [ ] **Step 1: Regenerate emoji probe images**

```bash
cmake --build build --target emoji_render_probe
QT_QPA_PLATFORM=offscreen ./build/tests/emoji_render_probe
```

Expected: auto and fallback paths report fallback draws and write both `/tmp/deepin-terminal-emoji-*.png` images.

- [ ] **Step 2: Inspect both probe images**

Confirm ordinary emoji are modestly larger while remaining separated from following Chinese text and punctuation.

- [ ] **Step 3: Verify formatting and the working tree**

```bash
clang-format --dry-run --Werror src/libqtghostty/TerminalWidget.cpp tests/test_terminal_widget.cpp
git diff --check
git status --short
```

Expected: formatting and whitespace checks pass; only the two C++ files and approved design/plan documents are changed. Do not commit without explicit user authorization.
