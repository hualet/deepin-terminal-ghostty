# Tab Context Menu Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a right-click context menu to terminal tabs with Close tab, Close other tabs, and Rename title, plus a single-input rename dialog targeting the right-clicked tab.

**Architecture:** `TabBar` detects right-click in its existing event filter and emits `tabMenuRequested(int index)`. `MainWindow::showTabContextMenu(int index)` builds a `QMenu` (matching the existing `TermPane::showTerminalContextMenu` pattern). `closeOtherTabs` and the rename dialog are refactored to target a specific tab index rather than always the current tab.

**Tech Stack:** C++20, Qt6 Widgets, DTK6 (`DTabBar`, `DDialog`), QtTest.

**Spec:** `docs/superpowers/specs/2026-06-24-tab-context-menu-design.md`

**Note on commits:** Per AGENTS.md, do not create commits unless the user asks. The commit steps below are therefore deferred to a single end-of-task commit gated on an explicit user request.

---

## File Structure

- **Modify:** `src/app/TabBar.h` — add `tabMenuRequested(int)` signal + `handleRightButtonClick` helper.
- **Modify:** `src/app/TabBar.cpp` — right-click detection + emit.
- **Modify:** `src/app/MainWindow.h` — `showTabContextMenu(int)` private slot, `closeOtherTabs(int)` private slot, `renameTabTitle(int)` private method.
- **Modify:** `src/app/MainWindow.cpp` — connect signal, build menu, refactor `closeOtherTabs`, extract `renameTabTitle`.
- **Modify:** `tests/test_main_window.cpp` — two new tests + declarations.

---

## Task 1: TabBar right-click signal

**Files:**
- Modify: `src/app/TabBar.h`
- Modify: `src/app/TabBar.cpp`
- Test: `tests/test_main_window.cpp`

- [ ] **Step 1: Add the test declaration and test body**

Add to the private slots list in `tests/test_main_window.cpp` (after `testHorizontalTabBarMiddleClickClosesTab`):

```cpp
void testTabBarRightClickRequestsMenu();
```

Add the test body near `testHorizontalTabBarMiddleClickClosesTab` (after it):

```cpp
void TestMainWindow::testTabBarRightClickRequestsMenu() {
    TabBar bar;
    bar.resize(400, 36);
    bar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&bar));
    bar.addTab(QStringLiteral("One"));
    bar.addTab(QStringLiteral("Two"));
    QCOMPARE(bar.count(), 2);

    QSignalSpy menuSpy(&bar, &TabBar::tabMenuRequested);
    QVERIFY(menuSpy.isValid());

    QTest::mousePress(&bar, Qt::RightButton, Qt::NoModifier, bar.tabRect(0).center());
    QCOMPARE(menuSpy.count(), 1);
    QCOMPARE(menuSpy.first().first().toInt(), 0);

    menuSpy.clear();
    QTest::mousePress(&bar, Qt::RightButton, Qt::NoModifier, bar.tabRect(1).center());
    QCOMPARE(menuSpy.count(), 1);
    QCOMPARE(menuSpy.first().first().toInt(), 1);
}
```

Note: this uses a **standalone `TabBar`** rather than the `MainWindow`'s tab bar.
The `MainWindow` connection (wired in Task 4) invokes the blocking `QMenu::exec`,
so driving the right-click on the integrated bar would hang the test. The
standalone bar isolates the right-click detection without that coupling.

- [ ] **Step 2: Run the test to verify it fails to compile**

Run:
```bash
cmake --build build 2>&1 | tail -20
```
Expected: compile error — `tabMenuRequested` is not a member of `TabBar`.

- [ ] **Step 3: Add the signal + helper to `TabBar.h`**

Replace the entire contents of `src/app/TabBar.h` with:

```cpp
#pragma once

#include <DTabBar>

DWIDGET_USE_NAMESPACE

class TabBar : public DTabBar {
    Q_OBJECT

public:
    explicit TabBar(QWidget *parent = nullptr);

signals:
    void tabMenuRequested(int index);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void handleMiddleButtonClick(QMouseEvent *mouseEvent);
    void handleRightButtonClick(QMouseEvent *mouseEvent);
};
```

- [ ] **Step 4: Add right-click handling to `TabBar.cpp`**

In `src/app/TabBar.cpp`, extend `eventFilter` and add `handleRightButtonClick`. The full file becomes:

```cpp
#include "TabBar.h"

#include <QMouseEvent>

TabBar::TabBar(QWidget *parent) : DTabBar(parent) {
    installEventFilter(this);
}

bool TabBar::eventFilter(QObject *watched, QEvent *event) {
    Q_UNUSED(watched)

    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::MiddleButton)
            handleMiddleButtonClick(mouseEvent);
        else if (mouseEvent->button() == Qt::RightButton)
            handleRightButtonClick(mouseEvent);
    }

    return false;
}

void TabBar::handleMiddleButtonClick(QMouseEvent *mouseEvent) {
    const QPoint position = mouseEvent->pos();

    for (int i = 0; i < count(); ++i) {
        if (tabRect(i).contains(position)) {
            Q_EMIT tabCloseRequested(i);
            break;
        }
    }
}

void TabBar::handleRightButtonClick(QMouseEvent *mouseEvent) {
    const QPoint position = mouseEvent->pos();

    for (int i = 0; i < count(); ++i) {
        if (tabRect(i).contains(position)) {
            Q_EMIT tabMenuRequested(i);
            break;
        }
    }
}
```

- [ ] **Step 5: Build and run the test to verify it passes**

Run:
```bash
cmake --build build && QT_QPA_PLATFORM=offscreen ./build/tests/test_main_window TestMainWindow::testTabBarRightClickRequestsMenu
```
Expected: PASS (1 test, 0 failures).

---

## Task 2: closeOtherTabs(int keepIndex) refactor

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Test: `tests/test_main_window.cpp`

- [ ] **Step 1: Add the test declaration and body**

Add to the private slots list in `tests/test_main_window.cpp`:

```cpp
void testTabContextMenuCloseOtherTabsKeepsClickedTab();
```

Add the test body near the other tab tests:

```cpp
void TestMainWindow::testTabContextMenuCloseOtherTabsKeepsClickedTab() {
    AppSettings::instance()->setVerticalTabsEnabled(false);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);

    auto *firstPane = currentPane(window);
    QVERIFY(firstPane);
    firstPane->setCustomTitle(QStringLiteral("One"));

    QVERIFY(QMetaObject::invokeMethod(&window, "onTabAddRequested", Qt::DirectConnection));
    QVERIFY(waitForTabCount(tabs, 2));
    currentPane(window)->setCustomTitle(QStringLiteral("Two"));

    QVERIFY(QMetaObject::invokeMethod(&window, "onTabAddRequested", Qt::DirectConnection));
    QVERIFY(waitForTabCount(tabs, 3));
    currentPane(window)->setCustomTitle(QStringLiteral("Three"));

    QVERIFY(QMetaObject::invokeMethod(&window, "closeOtherTabs", Qt::DirectConnection, Q_ARG(int, 0)));
    QVERIFY(waitForTabCount(tabs, 1));
    QCOMPARE(tabs->tabText(0), QStringLiteral("One"));
    QCOMPARE(currentPane(window), firstPane);
}
```

Note: freshly spawned idle shells report `hasRunningProcess() == false` (foreground pgid equals the shell pid), so `onTabCloseRequested` closes them immediately without the async exit-confirm dialog.

- [ ] **Step 2: Run the test to verify it fails**

Run:
```bash
cmake --build build 2>&1 | tail -20
```
Expected: compile/link error — `closeOtherTabs` with an `int` argument is not a slot, so `QMetaObject::invokeMethod` cannot resolve it.

- [ ] **Step 3: Declare `closeOtherTabs(int)` as a private slot in `MainWindow.h`**

In `src/app/MainWindow.h`, in the `private slots:` block, add after `void onSettingsTriggered();`:

```cpp
    void closeOtherTabs(int keepIndex);
```

- [ ] **Step 4: Implement `closeOtherTabs(int)` and refactor the no-arg version in `MainWindow.cpp`**

In `src/app/MainWindow.cpp`, replace the existing `closeOtherTabs()` definition:

```cpp
void MainWindow::closeOtherTabs() {
    int current = m_tabBar->currentIndex();
    if (current < 0)
        return;
    for (int i = m_tabBar->count() - 1; i >= 0; --i) {
        if (i != current)
            onTabCloseRequested(i);
    }
}
```

with:

```cpp
void MainWindow::closeOtherTabs() {
    closeOtherTabs(m_tabBar->currentIndex());
}

void MainWindow::closeOtherTabs(int keepIndex) {
    if (keepIndex < 0 || keepIndex >= m_tabs.size())
        return;

    auto *keepPane = m_tabs.at(keepIndex).pane;
    if (!keepPane)
        return;

    if (m_tabBar->currentIndex() != keepIndex)
        m_tabBar->setCurrentIndex(keepIndex);

    for (int i = m_tabBar->count() - 1; i >= 0; --i) {
        int stackIndex = m_tabBar->tabData(i).toInt();
        QWidget *page = m_stackWidget->widget(stackIndex);
        if (page == keepPane)
            continue;
        onTabCloseRequested(i);
    }
}
```

- [ ] **Step 5: Build and run the test to verify it passes**

Run:
```bash
cmake --build build && QT_QPA_PLATFORM=offscreen ./build/tests/test_main_window TestMainWindow::testTabContextMenuCloseOtherTabsKeepsClickedTab
```
Expected: PASS.

---

## Task 3: renameTabTitle(int index) extraction

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Declare `renameTabTitle(int)` in `MainWindow.h`**

In `src/app/MainWindow.h`, in the `private:` block, replace:

```cpp
    void onShortcutRenameTitle();
```

with:

```cpp
    void onShortcutRenameTitle();
    void renameTabTitle(int index);
```

- [ ] **Step 2: Implement `renameTabTitle(int)` and simplify `onShortcutRenameTitle`**

In `src/app/MainWindow.cpp`, replace the existing `onShortcutRenameTitle()` definition:

```cpp
void MainWindow::onShortcutRenameTitle() {
    auto *pane = currentPane();
    if (!pane)
        return;
    auto *term = pane->currentTerminal();
    if (!term)
        return;

    bool ok = false;
    QString currentText = m_tabBar->tabText(m_tabBar->currentIndex());
    QString text =
        QInputDialog::getText(this, tr("Rename title"), tr("New title:"), QLineEdit::Normal, currentText, &ok);
    if (ok && !text.isEmpty())
        pane->setCustomTitle(text);
}
```

with:

```cpp
void MainWindow::onShortcutRenameTitle() {
    renameTabTitle(m_tabBar->currentIndex());
}

void MainWindow::renameTabTitle(int index) {
    if (index < 0 || index >= m_tabs.size())
        return;
    auto *pane = m_tabs.at(index).pane;
    if (!pane)
        return;

    bool ok = false;
    QString currentText = m_tabBar->tabText(index);
    QString text =
        QInputDialog::getText(this, tr("Rename title"), tr("New title:"), QLineEdit::Normal, currentText, &ok);
    if (ok && !text.isEmpty())
        pane->setCustomTitle(text);
}
```

- [ ] **Step 3: Build to verify it compiles**

Run:
```bash
cmake --build build
```
Expected: clean build. (No new test for this task — the modal `QInputDialog` is not unit-tested; the `setCustomTitle` path it calls is already covered.)

---

## Task 4: showTabContextMenu + wire the signal

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Declare `showTabContextMenu(int)` as a private slot in `MainWindow.h`**

In `src/app/MainWindow.h`, in the `private slots:` block, add (after `void closeOtherTabs(int keepIndex);`):

```cpp
    void showTabContextMenu(int index);
```

- [ ] **Step 2: Add the `QCursor` include in `MainWindow.cpp`**

In `src/app/MainWindow.cpp`, add after the `#include <QMenu>` line:

```cpp
#include <QCursor>
```

- [ ] **Step 3: Connect the signal in `ensureTabBar`**

In `src/app/MainWindow.cpp`, in `ensureTabBar()`, after the line:

```cpp
    connect(m_tabBar, &DTabBar::tabReleaseRequested, this, &MainWindow::onTabReleaseRequested);
```

add:

```cpp
    connect(static_cast<TabBar *>(m_tabBar.data()), &TabBar::tabMenuRequested, this,
            &MainWindow::showTabContextMenu);
```

- [ ] **Step 4: Implement `showTabContextMenu`**

In `src/app/MainWindow.cpp`, add this new method right after `onTabReleaseRequested`:

```cpp
void MainWindow::showTabContextMenu(int index) {
    if (index < 0 || index >= m_tabBar->count())
        return;

    QMenu menu(this);
    menu.addAction(tr("Close tab"), this, [this, index]() { onTabCloseRequested(index); });

    auto *closeOtherAction =
        menu.addAction(tr("Close other tabs"), this, [this, index]() { closeOtherTabs(index); });
    closeOtherAction->setEnabled(m_tabBar->count() >= 2);

    menu.addAction(tr("Rename title"), this, [this, index]() { renameTabTitle(index); });

    menu.exec(QCursor::pos());
}
```

- [ ] **Step 5: Build to verify it compiles**

Run:
```bash
cmake --build build
```
Expected: clean build.

---

## Task 5: Format, full build, and full test run

- [ ] **Step 1: Apply clang-format**

Run:
```bash
clang-format -i $(find src tests -name '*.cpp' -o -name '*.h')
```

- [ ] **Step 2: Verify formatting is clean**

Run:
```bash
clang-format --dry-run --Werror $(find src tests -name '*.cpp' -o -name '*.h')
```
Expected: no output (all formatted).

- [ ] **Step 3: Full build with tests**

Run:
```bash
cmake -B build -DBUILD_TESTING=ON && cmake --build build
```
Expected: clean build.

- [ ] **Step 4: Run the two new tests**

Run:
```bash
QT_QPA_PLATFORM=offscreen ./build/tests/test_main_window TestMainWindow::testTabBarRightClickRequestsMenu TestMainWindow::testTabContextMenuCloseOtherTabsKeepsClickedTab
```
Expected: 2 pass.

- [ ] **Step 5: Run the full test suite**

Run:
```bash
cd build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure
```
Expected: all tests pass (no regressions).

---

## Self-Review

**Spec coverage:**
- "Close tab" menu item → Task 4 Step 4 (first action).
- "Close other tabs" (disabled <2) → Task 4 Step 4 (`closeOtherAction->setEnabled(count >= 2)`), logic in Task 2.
- "Rename title" single-input → Task 3 (`renameTabTitle`), Task 4 Step 4 (third action).
- TabBar right-click emits signal → Task 1.
- Menu built in MainWindow (app layer) → Task 4.
- `closeOtherTabs(int)` overload → Task 2.

All spec sections covered.

**Placeholder scan:** None — every code step contains complete code.

**Type/signature consistency:** `tabMenuRequested(int)` (Task 1) → connected in Task 4 Step 3. `closeOtherTabs(int keepIndex)` (Task 2 slot) → invoked in Task 4 lambda. `renameTabTitle(int index)` (Task 3) → invoked in Task 4 lambda. `showTabContextMenu(int index)` (Task 4 slot) → matches connect target. All consistent.
