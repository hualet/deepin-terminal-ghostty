# Overflowing Single-Cell Glyph Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render `※` and other genuinely overflowing non-ASCII single-codepoint
glyphs completely inside their one-cell terminal grid allocation.

**Architecture:** Keep Ghostty's cell widths and the existing cell clip
unchanged. Add a generic, emoji-independent single-codepoint predicate to the
existing fit decision, then reuse the renderer's proportional fit-and-center
path only when that glyph actually exceeds one cell.

**Tech Stack:** C++20, Qt6 Widgets painting and font metrics, Qt Test,
`libghostty-vt`

---

### Task 1: Add the failing `※ recap` regression

**Files:**

- Modify: `tests/test_terminal_widget.cpp`

- [ ] **Step 1: Declare the focused test slot**

Add this declaration next to the existing fallback-glyph rendering tests:

```cpp
void testOverflowingSingleCodepointGlyphFitsCell();
```

- [ ] **Step 2: Implement the real-widget regression**

Add a test next to `testSingleCodepointFallbackGlyphDoesNotClip()`:

```cpp
void TestTerminalWidget::testOverflowingSingleCodepointGlyphFitsCell() {
    PtySession::StartOptions options;
    options.command = QStringLiteral("sleep 5");

    CountingTerminalWidget widget;
    widget.setStartOptions(options);
    widget.debugSetEmojiRenderModeForTesting(TerminalWidget::EmojiRenderMode::CustomFallback);
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    feedTerminalOutput(widget, QByteArray("\033[?25l"));

    QInputMethodQueryEvent queryEvent(Qt::ImCursorRectangle);
    QApplication::sendEvent(&widget, &queryEvent);
    const QRect cursorRect = queryEvent.value(Qt::ImCursorRectangle).toRect();
    QVERIFY(cursorRect.isValid());

    const QImage before = renderWidgetImage(widget);
    feedTerminalOutput(widget, QStringLiteral("※ recap").toUtf8());
    const QImage after = renderWidgetImage(widget);

    const QRect markCell(cursorRect.topLeft(), cursorRect.size());
    const QRect followingCell(cursorRect.topLeft() + QPoint(cursorRect.width(), 0), cursorRect.size());
    const QRect glyphBounds = changedBounds(before.copy(markCell), after.copy(markCell));

    QVERIFY(glyphBounds.isValid());
    QVERIFY(glyphBounds.left() > 0);
    QVERIFY(glyphBounds.right() < markCell.width() - 1);
    QVERIFY2(qAbs(glyphBounds.left() - (markCell.width() - 1 - glyphBounds.right())) <= 1,
             "overflowing single-codepoint glyph should be fitted and centered instead of clipped");
    QCOMPARE(countChangedPixels(before, after, followingCell), 0);
    QCOMPARE(widget.debugLastFrameEmojiFallbackDrawCount(), 0);
}
```

- [ ] **Step 3: Build and verify the test fails for the clipping reason**

Run:

```bash
cmake --build build --target test_terminal_widget
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget \
  testOverflowingSingleCodepointGlyphFitsCell
```

Expected: the test fails because `※` reaches the right cell edge or has
asymmetric margins. It must not fail during widget initialization or because an
emoji fallback draw occurred.

### Task 2: Fit overflowing non-ASCII single codepoints

**Files:**

- Modify: `src/libqtghostty/TerminalWidget.cpp:182-199`
- Modify: `src/libqtghostty/TerminalWidget.cpp:2474-2489`

- [ ] **Step 1: Add an emoji-independent Unicode scalar predicate**

Add this helper next to `appendCodepoint()`:

```cpp
bool isSingleNonAsciiCodepoint(QStringView text) {
    if (text.size() == 1)
        return !text.front().isSurrogate() && text.front().unicode() > 0x7F;

    return text.size() == 2 && text.at(0).isHighSurrogate()
           && text.at(1).isLowSurrogate();
}
```

- [ ] **Step 2: Extend only the generic fit decision**

Keep the custom emoji fallback block unchanged. Replace the fit predicate with:

```cpp
const bool overflowsCell =
    metrics.horizontalAdvance(text) > m_cellWidth || inkBounds.width() > m_cellWidth;
const bool overflowingSingleCodepoint =
    isSingleNonAsciiCodepoint(QStringView(text)) && overflowsCell;
const bool needsFit =
    isEmojiText || overflowingSingleCodepoint || (text.size() > 1 && overflowsCell);
```

Update the nearby comment to state that ordinary ASCII keeps native rendering,
while overflowing non-ASCII single codepoints use the generic fit path. Do not
change `firstEmojiCodepoint()`, `singleEmojiCodepoint()`,
`drawEmojiFallback()`, or `emojiFallbackCellImage()`.

- [ ] **Step 3: Build and verify the new regression is green**

Run:

```bash
cmake --build build --target test_terminal_widget
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget \
  testOverflowingSingleCodepointGlyphFitsCell
```

Expected: PASS with zero custom emoji fallback draws.

- [ ] **Step 4: Run adjacent rendering regressions**

Run:

```bash
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget \
  testZoomedAsciiNotReshapedPerCharacter \
  testSingleCodepointFallbackGlyphDoesNotClip \
  testFallbackGlyphDoesNotOverlapNextCell \
  testStyledTextKeepsCharactersOnCellGrid \
  testEmojiFallbackRendererCanBeForced \
  testEmojiFallbackDoesNotOverlapFollowingCjkText
```

Expected: all selected tests PASS.

### Task 3: Document and verify the fix

**Files:**

- Create: `docs/root-cause/2026-07-26-overflowing-single-cell-glyph-clipping.md`
- Verify: `src/libqtghostty/TerminalWidget.cpp`
- Verify: `tests/test_terminal_widget.cpp`

- [ ] **Step 1: Write the root-cause report**

Document:

- `※` is one terminal column in the active locale;
- Qt measures its resolved glyph wider than `m_cellWidth`;
- the one-cell clip prevents overlap but cuts the glyph;
- the existing fit condition excludes non-emoji single codepoints;
- the fix adds an emoji-independent overflow predicate and preserves ASCII,
  emoji fallback, grid, cursor, and PTY behavior;
- the exact focused and adjacent verification commands.

- [ ] **Step 2: Format the changed C++ files**

Run:

```bash
clang-format -i src/libqtghostty/TerminalWidget.cpp tests/test_terminal_widget.cpp
```

- [ ] **Step 3: Run final focused verification**

Run:

```bash
cmake --build build --target test_terminal_widget
QT_QPA_PLATFORM=offscreen ./build/tests/test_terminal_widget \
  testOverflowingSingleCodepointGlyphFitsCell \
  testZoomedAsciiNotReshapedPerCharacter \
  testSingleCodepointFallbackGlyphDoesNotClip \
  testFallbackGlyphDoesNotOverlapNextCell \
  testStyledTextKeepsCharactersOnCellGrid \
  testEmojiFallbackRendererCanBeForced \
  testEmojiFallbackDoesNotOverlapFollowingCjkText
clang-format --dry-run --Werror \
  src/libqtghostty/TerminalWidget.cpp tests/test_terminal_widget.cpp
git diff --check
git status --short
```

Expected: build exits zero, all seven tests pass, formatting and diff checks
exit zero, and status lists only the scoped source, test, design, plan, and
root-cause documentation changes.

No commit is included because repository instructions require an explicit user
request before committing.
