# OSC 8 Hyperlink Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add OSC 8 hyperlink support to TerminalWidget: hover underline, context menu copy/open, Ctrl+click to open.

**Architecture:** Reuse Ghostty's native hyperlink API. Hover detection and underline rendering stay in `libqtghostty/TerminalWidget`. Context menu integration lives in `app/TermPane`. No regex parsing — only OSC 8 explicit hyperlinks.

**Tech Stack:** C++20, Qt6 Widgets, Ghostty VT API

---

## File Structure

| File | Responsibility |
|------|----------------|
| `src/libqtghostty/TerminalWidget.h` | Declare new signals (`hyperlinkHovered`, `hyperlinkActivated`), new private members (`m_hoverHyperlinkUri`, `m_lastMousePos`), new helper methods, and `leaveEvent` override |
| `src/libqtghostty/TerminalWidget.cpp` | Implement hyperlink URI querying, mouse event handling (hover + Ctrl+click), underline rendering in `renderRow()`, scroll re-detection |
| `src/app/TermPane.cpp` | Integrate hyperlink copy/open actions into `showTerminalContextMenu()` |
| `tests/test_terminal_widget.cpp` | Unit tests for hover detection, Ctrl+click activation, cursor changes |

---

## Task 1: TerminalWidget.h — Declare hyperlink API

**Files:**
- Modify: `src/libqtghostty/TerminalWidget.h`

- [ ] **Step 1: Add signals**

After the existing `viewportScrollStateChanged()` signal (line ~107):

```cpp
    void hyperlinkHovered(const QString &uri);
    void hyperlinkActivated(const QString &uri);
```

- [ ] **Step 2: Add `leaveEvent` override**

After `mouseReleaseEvent` in the `protected` section (line ~120):

```cpp
    void leaveEvent(QEvent *event) override;
```

- [ ] **Step 3: Add helper method declarations**

In the `private` section, after `updateGridSize()` (line ~136):

```cpp
    QString hyperlinkUriAt(int screenRow, int col) const;
    QString hyperlinkUriAtPosition(const QPoint &pos) const;
    void updateHyperlinkHoverState(const QPoint &pos);
```

- [ ] **Step 4: Add private member declarations**

After the `Selection m_selection;` member (line ~267):

```cpp
    QString m_hoverHyperlinkUri;
    QPoint m_lastMousePos;
```

- [ ] **Step 5: Commit**

```bash
git add src/libqtghostty/TerminalWidget.h
git commit -m "feat(hyperlink): declare hyperlink API in TerminalWidget.h

Add hyperlinkHovered/hyperlinkActivated signals, leaveEvent override,
helper methods, and state members for OSC 8 hyperlink support."
```

---

## Task 2: TerminalWidget.cpp — Implement hyperlink URI query helpers

**Files:**
- Modify: `src/libqtghostty/TerminalWidget.cpp`

- [ ] **Step 1: Implement `hyperlinkUriAt()`**

Add this method after `updateGridSize()` (around line 869):

```cpp
QString TerminalWidget::hyperlinkUriAt(int screenRow, int col) const {
    if (!m_terminal)
        return QString();

    GhosttyPoint point = {
        .tag = GHOSTTY_POINT_TAG_SCREEN,
        .value = {.coordinate = {.x = static_cast<uint16_t>(col), .y = static_cast<uint32_t>(screenRow)}}};
    GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
    if (ghostty_terminal_grid_ref(m_terminal, point, &ref) != GHOSTTY_SUCCESS)
        return QString();

    size_t requiredLen = 0;
    if (ghostty_grid_ref_hyperlink_uri(&ref, nullptr, 0, &requiredLen) != GHOSTTY_OUT_OF_SPACE || requiredLen == 0)
        return QString();

    QByteArray buffer(static_cast<int>(requiredLen), '\0');
    size_t written = 0;
    if (ghostty_grid_ref_hyperlink_uri(&ref, reinterpret_cast<uint8_t *>(buffer.data()), buffer.size(), &written)
        != GHOSTTY_SUCCESS) {
        return QString();
    }

    return QString::fromUtf8(buffer.constData(), static_cast<int>(written));
}
```

- [ ] **Step 2: Implement `hyperlinkUriAtPosition()`**

After the above method:

```cpp
QString TerminalWidget::hyperlinkUriAtPosition(const QPoint &pos) const {
    const int col = viewportColumnForPosition(pos);
    const int viewportRow = viewportRowForPosition(pos);
    const int screenRow = screenRowForViewportRow(viewportRow);
    return hyperlinkUriAt(screenRow, col);
}
```

- [ ] **Step 3: Implement `updateHyperlinkHoverState()`**

After the above method:

```cpp
void TerminalWidget::updateHyperlinkHoverState(const QPoint &pos) {
    const QString uri = hyperlinkUriAtPosition(pos);
    if (uri != m_hoverHyperlinkUri) {
        m_hoverHyperlinkUri = uri;
        if (uri.isEmpty()) {
            unsetCursor();
        } else {
            setCursor(Qt::PointingHandCursor);
        }
        Q_EMIT hyperlinkHovered(uri);
        update();
    }
}
```

- [ ] **Step 4: Commit**

```bash
git add src/libqtghostty/TerminalWidget.cpp
git commit -m "feat(hyperlink): implement hyperlink URI query helpers

Add hyperlinkUriAt, hyperlinkUriAtPosition, and updateHyperlinkHoverState
for detecting OSC 8 hyperlinks at specific coordinates."
```

---

## Task 3: TerminalWidget.cpp — Mouse event handling

**Files:**
- Modify: `src/libqtghostty/TerminalWidget.cpp`

- [ ] **Step 1: Modify `mouseMoveEvent`**

In `mouseMoveEvent` (around line 2657), at the very beginning after the null checks, before the `mouseTracking` check, add:

```cpp
    m_lastMousePos = event->pos();
```

After the mouse tracking block (around line 2704, after the `return` from mouse tracking handling), before the `if (event->buttons() & Qt::LeftButton)` block, add:

```cpp
    updateHyperlinkHoverState(event->pos());
```

- [ ] **Step 2: Add `leaveEvent` implementation**

After `mouseReleaseEvent` in the file, add:

```cpp
void TerminalWidget::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    if (!m_hoverHyperlinkUri.isEmpty()) {
        m_hoverHyperlinkUri.clear();
        unsetCursor();
        Q_EMIT hyperlinkHovered(QString());
        update();
    }
}
```

- [ ] **Step 3: Modify `mousePressEvent` for Ctrl+click**

In `mousePressEvent` (around line 2592), at the very beginning after the null checks, add before the `mouseTracking` check:

```cpp
    // Ctrl+LeftButton: check for hyperlink first, regardless of mouse tracking
    if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ControlModifier)) {
        const QString uri = hyperlinkUriAtPosition(event->pos());
        if (!uri.isEmpty()) {
            Q_EMIT hyperlinkActivated(uri);
            event->accept();
            return;
        }
    }
```

- [ ] **Step 4: Commit**

```bash
git add src/libqtghostty/TerminalWidget.cpp
git commit -m "feat(hyperlink): wire up mouse events for hyperlink detection

- Record mouse position in mouseMoveEvent
- Call updateHyperlinkHoverState on normal mouse movement
- Add leaveEvent to clear hover state when mouse leaves widget
- Ctrl+LeftButton now checks for hyperlink before mouse tracking/selection"
```

---

## Task 4: TerminalWidget.cpp — Render underline in `renderRow()`

**Files:**
- Modify: `src/libqtghostty/TerminalWidget.cpp`

- [ ] **Step 1: Modify `renderRow` signature**

Change the declaration in `TerminalWidget.h` (line ~140):

```cpp
    void renderRow(QPainter &painter, int y, int screenRow, const GhosttyRenderStateColors &colors, RowRenderPass pass);
```

Change the definition in `TerminalWidget.cpp` (line ~1199):

```cpp
void TerminalWidget::renderRow(QPainter &painter, int y, int screenRow, const GhosttyRenderStateColors &colors, RowRenderPass pass) {
```

- [ ] **Step 2: Update `renderRow` call sites in `renderTerminal`**

In `renderTerminal`, the Background pass loop (around line 1007):

```cpp
    int y = 0;
    int viewportRow = 0;
    while (ghostty_render_state_row_iterator_next(m_rowIter)) {
        bool rowDirty = true;
        ghostty_render_state_row_get(m_rowIter, GHOSTTY_RENDER_STATE_ROW_DATA_DIRTY, &rowDirty);
        if (!fullRedraw && !rowDirty) {
            y += m_cellHeight;
            ++viewportRow;
            continue;
        }
        // ...
        renderRow(backPainter, y, static_cast<int>(viewportRow + currentViewportOffset), colors, RowRenderPass::Background);
        y += m_cellHeight;
        ++viewportRow;
    }
```

And the Text pass loop (around line 1027):

```cpp
    y = 0;
    viewportRow = 0;
    while (ghostty_render_state_row_iterator_next(m_rowIter)) {
        bool rowDirty = true;
        ghostty_render_state_row_get(m_rowIter, GHOSTTY_RENDER_STATE_ROW_DATA_DIRTY, &rowDirty);
        if (!fullRedraw && !rowDirty) {
            y += m_cellHeight;
            ++viewportRow;
            continue;
        }
        // ...
        renderRow(backPainter, y, static_cast<int>(viewportRow + currentViewportOffset), colors, RowRenderPass::Text);
        y += m_cellHeight;
        ++viewportRow;
    }
```

- [ ] **Step 3: Add hyperlink underline drawing in `renderRow`**

At the end of the cell loop in `renderRow`, before the final `x += m_cellWidth;` (around line 1487), add:

```cpp
        // Draw hyperlink underline for hovered links (Text pass only)
        if (pass == RowRenderPass::Text && !m_hoverHyperlinkUri.isEmpty()) {
            const int col = x / m_cellWidth;
            bool cellHasHyperlink = false;
            ghostty_cell_get(rawCell, GHOSTTY_CELL_DATA_HAS_HYPERLINK, &cellHasHyperlink);
            if (cellHasHyperlink) {
                const QString cellUri = hyperlinkUriAt(screenRow, col);
                if (cellUri == m_hoverHyperlinkUri) {
                    const int underlineY = qMin(y + m_cellHeight - 1, y + m_fontAscent + 2);
                    const QPen previousPen = painter.pen();
                    painter.setPen(QPen(defaultForeground));
                    painter.drawLine(x, underlineY, x + cellRenderWidth, underlineY);
                    painter.setPen(previousPen);
                }
            }
        }
```

- [ ] **Step 4: Commit**

```bash
git add src/libqtghostty/TerminalWidget.h src/libqtghostty/TerminalWidget.cpp
git commit -m "feat(hyperlink): render underline for hovered hyperlinks

Add screenRow parameter to renderRow, update call sites in renderTerminal,
and draw underline under cells matching the hovered hyperlink URI."
```

---

## Task 5: TerminalWidget.cpp — Scroll re-detection

**Files:**
- Modify: `src/libqtghostty/TerminalWidget.cpp`

- [ ] **Step 1: Modify `scrollViewportBy` and `scrollViewportToOffset`**

In `scrollViewportBy` (around line 549), after `update();`, add:

```cpp
    if (rect().contains(m_lastMousePos))
        updateHyperlinkHoverState(m_lastMousePos);
```

In `scrollViewportToOffset` (around line 563), after the existing body, the method calls `scrollViewportBy`, so it will automatically pick up the re-detection.

- [ ] **Step 2: Commit**

```bash
git add src/libqtghostty/TerminalWidget.cpp
git commit -m "feat(hyperlink): re-detect hyperlink after scroll

When viewport scrolls and mouse is still inside widget, re-check
hyperlink state at the last known mouse position."
```

---

## Task 6: TermPane.cpp — Context menu integration

**Files:**
- Modify: `src/app/TermPane.cpp`

- [ ] **Step 1: Add includes**

At the top of `TermPane.cpp`, after existing includes, add:

```cpp
#include <QDesktopServices>
#include <QUrl>
```

- [ ] **Step 2: Modify `showTerminalContextMenu`**

After the existing `pasteAction` setup (around line 656), add:

```cpp
    QPoint localPos = term->mapFromGlobal(globalPos);
    QString hyperlinkUri = term->hyperlinkUriAtPosition(localPos);
    if (!hyperlinkUri.isEmpty()) {
        menu.addSeparator();
        auto *copyLinkAction = menu.addAction(tr("Copy Link"));
        auto *openLinkAction = menu.addAction(tr("Open Link"));
        
        connect(copyLinkAction, &QAction::triggered, this, [hyperlinkUri]() {
            QGuiApplication::clipboard()->setText(hyperlinkUri);
        });
        
        connect(openLinkAction, &QAction::triggered, this, [hyperlinkUri]() {
            QDesktopServices::openUrl(QUrl::fromUserInput(hyperlinkUri));
        });
    }
```

- [ ] **Step 3: Connect `hyperlinkActivated` signal in `setupTerminalConnections`**

In `setupTerminalConnections` (around line 480), add:

```cpp
    connect(term, &TerminalWidget::hyperlinkActivated, this, [](const QString &uri) {
        QDesktopServices::openUrl(QUrl::fromUserInput(uri));
    });
```

- [ ] **Step 4: Commit**

```bash
git add src/app/TermPane.cpp
git commit -m "feat(hyperlink): add context menu actions for hyperlinks

- Add Copy Link and Open Link to terminal context menu
- Open link on Ctrl+click via hyperlinkActivated signal"
```

---

## Task 7: Write unit tests

**Files:**
- Modify: `tests/test_terminal_widget.cpp`

- [ ] **Step 1: Add test declarations**

In the `TestTerminalWidget` class, after the existing test declarations (around line 100), add:

```cpp
    void testHyperlinkHoverDetection();
    void testHyperlinkCtrlClick();
    void testHyperlinkCursorChange();
    void testHyperlinkLeaveEvent();
```

- [ ] **Step 2: Implement `testHyperlinkHoverDetection`**

Add the test implementation:

```cpp
void TestTerminalWidget::testHyperlinkHoverDetection() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    
    QSignalSpy hoverSpy(&widget, &TerminalWidget::hyperlinkHovered);
    
    // Write OSC 8 hyperlink sequence
    QByteArray osc8("\033]8;;https://example.com\aLink\033]8;;\a\n");
    widget.importVtContent(osc8);
    QApplication::processEvents();
    
    // Simulate mouse move over the hyperlink text
    QPoint hoverPos(widget.terminalContentRect().left() + 5,
                    widget.terminalContentRect().top() + 5);
    QTest::mouseMove(&widget, hoverPos);
    QApplication::processEvents();
    
    QTRY_VERIFY(hoverSpy.count() > 0);
    QCOMPARE(hoverSpy.last().first().toString(), QString("https://example.com"));
}
```

- [ ] **Step 3: Implement `testHyperlinkCtrlClick`**

Add the test implementation:

```cpp
void TestTerminalWidget::testHyperlinkCtrlClick() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    
    QSignalSpy activateSpy(&widget, &TerminalWidget::hyperlinkActivated);
    
    // Write OSC 8 hyperlink sequence
    QByteArray osc8("\033]8;;https://example.com\aLink\033]8;;\a\n");
    widget.importVtContent(osc8);
    QApplication::processEvents();
    
    // Simulate Ctrl+LeftButton click over hyperlink
    QPoint clickPos(widget.terminalContentRect().left() + 5,
                    widget.terminalContentRect().top() + 5);
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::ControlModifier, clickPos);
    QApplication::processEvents();
    
    QCOMPARE(activateSpy.count(), 1);
    QCOMPARE(activateSpy.first().first().toString(), QString("https://example.com"));
}
```

- [ ] **Step 4: Implement `testHyperlinkCursorChange`**

Add the test implementation:

```cpp
void TestTerminalWidget::testHyperlinkCursorChange() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    
    // Write OSC 8 hyperlink sequence
    QByteArray osc8("\033]8;;https://example.com\aLink\033]8;;\a\n");
    widget.importVtContent(osc8);
    QApplication::processEvents();
    
    // Default cursor should be Qt::ArrowCursor
    QCOMPARE(widget.cursor().shape(), Qt::ArrowCursor);
    
    // Move mouse over hyperlink
    QPoint hoverPos(widget.terminalContentRect().left() + 5,
                    widget.terminalContentRect().top() + 5);
    QTest::mouseMove(&widget, hoverPos);
    QApplication::processEvents();
    
    QCOMPARE(widget.cursor().shape(), Qt::PointingHandCursor);
}
```

- [ ] **Step 5: Implement `testHyperlinkLeaveEvent`**

Add the test implementation:

```cpp
void TestTerminalWidget::testHyperlinkLeaveEvent() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    
    QSignalSpy hoverSpy(&widget, &TerminalWidget::hyperlinkHovered);
    
    // Write OSC 8 hyperlink sequence
    QByteArray osc8("\033]8;;https://example.com\aLink\033]8;;\a\n");
    widget.importVtContent(osc8);
    QApplication::processEvents();
    
    // Move mouse over hyperlink
    QPoint hoverPos(widget.terminalContentRect().left() + 5,
                    widget.terminalContentRect().top() + 5);
    QTest::mouseMove(&widget, hoverPos);
    QApplication::processEvents();
    
    QVERIFY(hoverSpy.count() > 0);
    
    // Simulate leave event
    QLeaveEvent leaveEvent(QPoint(), QPoint());
    QApplication::sendEvent(&widget, &leaveEvent);
    
    QCOMPARE(hoverSpy.last().first().toString(), QString());
}
```

- [ ] **Step 6: Build and run tests**

```bash
cmake -B build -DBUILD_TESTING=ON
cmake --build build
cd build
QT_QPA_PLATFORM=offscreen ctest --output-on-failure -R TerminalWidget
```

Expected: All 4 new tests pass.

- [ ] **Step 7: Commit**

```bash
git add tests/test_terminal_widget.cpp
git commit -m "test(hyperlink): add unit tests for OSC 8 hyperlink support

Test hover detection, Ctrl+click activation, cursor changes, and
leave event clearing."
```

---

## Task 8: Final verification

**Files:**
- All modified files

- [ ] **Step 1: Run full test suite**

```bash
cd build
QT_QPA_PLATFORM=offscreen ctest --output-on-failure
```

Expected: All tests pass, including the 4 new ones.

- [ ] **Step 2: Format code**

```bash
clang-format -i $(find src tests -name '*.cpp' -o -name '*.h')
```

- [ ] **Step 3: Build application**

```bash
cmake --build build
```

- [ ] **Step 4: Manual smoke test**

```bash
./build/deepin-terminal-ghostty
```

Test:
1. Run `echo -e '\e]8;;https://example.com\aLink\e]8;;\a'`
2. Hover over "Link" — underline should appear
3. Right-click on "Link" — "Copy Link" and "Open Link" should appear
4. Ctrl+click on "Link" — browser should open example.com

- [ ] **Step 5: Commit formatting changes**

```bash
git add -A
git commit -m "style: format code with clang-format"
```

---

## Self-Review Checklist

**1. Spec coverage:**
- [x] Hover underline → Task 4
- [x] Context menu copy/open → Task 6
- [x] Ctrl+click to open → Task 3 + Task 6
- [x] Performance (no full-screen scan) → Task 4 renders inline in existing loop
- [x] Mouse tracking priority → Task 3: Ctrl+click before mouse tracking check
- [x] Right-click uses snapshot URI → Task 6: queries at event position, not hover cache

**2. Placeholder scan:**
- [x] No "TBD", "TODO", or "implement later"
- [x] All steps show actual code
- [x] No vague instructions

**3. Type consistency:**
- [x] `hyperlinkUriAtPosition` used consistently
- [x] Signal signatures match declarations
- [x] `renderRow` signature updated everywhere

**4. Gap check:**
- [x] `leaveEvent` implemented (was in spec)
- [x] Scroll re-detection included (was in spec)
- [x] Test coverage for all major behaviors
