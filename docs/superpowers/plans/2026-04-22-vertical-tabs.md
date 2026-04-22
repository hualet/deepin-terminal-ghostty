# Vertical Tabs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a persistent, translatable vertical tab mode with a compact titlebar, left tab-and-pane sidebar, and shared tab metadata between horizontal and vertical tab presentations.

**Architecture:** Keep `TermPane` as the owner of pane split topology and active-pane behavior, while `MainWindow` gains a lightweight `TabRecord` list plus a switchable app-level sidebar view. Persist the layout mode in `AppSettings`, expose pane snapshots from `TermPane`, and keep title synchronization flowing from the active pane through a single refresh path in `MainWindow`.

**Tech Stack:** C++20, Qt6 Widgets/Test, DTK6 Widgets/Core/Gui, existing `AppSettings`, `MainWindow`, `TermPane`, Qt translation flow, CMake/CTest.

---

## File Structure

- Modify: `src/app/AppSettings.h`
  Add a typed API for reading and writing the persisted vertical-tabs preference.
- Modify: `src/app/AppSettings.cpp`
  Implement the vertical-tabs setting accessors and connect any needed change notifications.
- Modify: `src/app/MainWindow.h`
  Add `TabRecord`, layout-switch helpers, sidebar refresh helpers, and menu action members.
- Modify: `src/app/MainWindow.cpp`
  Implement menu wiring, compact/normal layout switching, shared tab-record state, and title/sidebar synchronization.
- Modify: `src/app/TermPane.h`
  Define pane snapshot structures, pane-focusing API, and app-facing pane update signals.
- Modify: `src/app/TermPane.cpp`
  Track pane identities/titles, emit structure and active-pane updates, and expose read-only pane enumeration.
- Create: `src/app/VerticalTabSidebar.h`
  Small app-level widget that renders the two-level tab/pane navigation tree and emits navigation/expand-toggle requests.
- Create: `src/app/VerticalTabSidebar.cpp`
  Implement the narrow tab/pane sidebar widget without taking ownership of pane lifecycle.
- Modify: `src/app/settings/default-config.json`
  Add the persisted boolean option for vertical tabs mode.
- Modify: `src/app/settings/settings_translation.cpp`
  Register translations for any new visible settings/menu/sidebar strings if required by the current settings translation path.
- Modify: `tests/test_appsettings.cpp`
  Add coverage for the new persisted setting.
- Modify: `tests/test_main_window.cpp`
  Add coverage for menu toggling, layout switching, sidebar structure, and title synchronization.
- Modify: `tests/CMakeLists.txt`
  Compile the new sidebar widget into `test_main_window`.

### Task 1: Persisted Layout Mode And Window Skeleton

**Files:**
- Modify: `src/app/AppSettings.h`
- Modify: `src/app/AppSettings.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Modify: `src/app/settings/default-config.json`
- Test: `tests/test_appsettings.cpp`
- Test: `tests/test_main_window.cpp`

- [ ] **Step 1: Write the failing AppSettings test**

Add a new test in `tests/test_appsettings.cpp`:

```cpp
    void testVerticalTabsEnabled() {
        auto *s = AppSettings::instance();

        QCOMPARE(s->verticalTabsEnabled(), false);

        s->setVerticalTabsEnabled(true);
        QCOMPARE(s->verticalTabsEnabled(), true);

        AppSettings::releaseInstance();
        s = AppSettings::instance();
        QCOMPARE(s->verticalTabsEnabled(), true);
    }
```

- [ ] **Step 2: Run the focused AppSettings test to verify it fails**

Run: `cmake -B build -DBUILD_TESTING=ON && cmake --build build --target test_appsettings && ./build/tests/test_appsettings -functions`

Expected: build or test failure because `AppSettings` does not yet define `verticalTabsEnabled()` / `setVerticalTabsEnabled(bool)`.

- [ ] **Step 3: Add the minimal AppSettings API**

Update `src/app/AppSettings.h`:

```cpp
    bool verticalTabsEnabled() const;
    void setVerticalTabsEnabled(bool enabled);
```

Update `src/app/AppSettings.cpp` with the minimal implementation:

```cpp
bool AppSettings::verticalTabsEnabled() const {
    return m_dsettings->value("basic.interface.verticalTabs").toBool();
}

void AppSettings::setVerticalTabsEnabled(bool enabled) {
    m_dsettings->setOption("basic.interface.verticalTabs", enabled);
}
```

Add the option to `src/app/settings/default-config.json` under the existing `basic.interface` group:

```json
                        {
                            "key": "verticalTabs",
                            "name": "Vertical Tabs",
                            "type": "bool",
                            "default": false
                        }
```

- [ ] **Step 4: Run the focused AppSettings test to verify it passes**

Run: `cmake --build build --target test_appsettings && ./build/tests/test_appsettings testVerticalTabsEnabled`

Expected: `PASS   : TestAppSettings::testVerticalTabsEnabled()`

- [ ] **Step 5: Write the failing MainWindow layout-mode test**

Add a focused test in `tests/test_main_window.cpp`:

```cpp
void TestMainWindow::testVerticalTabsMenuTogglesPersistedLayout() {
    AppSettings::instance()->setVerticalTabsEnabled(false);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *verticalAction = window.findChild<QAction *>("verticalTabsAction");
    QVERIFY(verticalAction);
    QVERIFY(verticalAction->isCheckable());
    QCOMPARE(verticalAction->isChecked(), false);

    verticalAction->trigger();

    QCOMPARE(verticalAction->isChecked(), true);
    QCOMPARE(AppSettings::instance()->verticalTabsEnabled(), true);
}
```

- [ ] **Step 6: Run the focused MainWindow test to verify it fails**

Run: `cmake --build build --target test_main_window && ./build/tests/test_main_window testVerticalTabsMenuTogglesPersistedLayout`

Expected: failure because the menu action does not exist and `MainWindow` does not yet read or write the layout mode.

- [ ] **Step 7: Add the minimal layout-mode state and menu wiring**

In `src/app/MainWindow.h`, add the core members:

```cpp
    struct TabRecord {
        int id = 0;
        TermPane *pane = nullptr;
        QString title;
        bool expanded = true;
    };

    void setVerticalTabsEnabled(bool enabled);
    void rebuildCentralLayout();

    QList<TabRecord> m_tabs;
    QAction *m_verticalTabsAction = nullptr;
    QWidget *m_contentHost = nullptr;
    bool m_verticalTabsEnabled = false;
    int m_nextTabId = 1;
```

In the `MainWindow` constructor, initialize from settings:

```cpp
    m_verticalTabsEnabled = AppSettings::instance()->verticalTabsEnabled();
```

In `setupTitleBar()`, create the checkable action:

```cpp
    m_verticalTabsAction = menu->addAction(tr("Vertical Tabs"));
    m_verticalTabsAction->setObjectName(QStringLiteral("verticalTabsAction"));
    m_verticalTabsAction->setCheckable(true);
    m_verticalTabsAction->setChecked(m_verticalTabsEnabled);
    connect(m_verticalTabsAction, &QAction::toggled, this, [this](bool checked) {
        AppSettings::instance()->setVerticalTabsEnabled(checked);
        setVerticalTabsEnabled(checked);
    });
```

Implement the initial minimal switch method:

```cpp
void MainWindow::setVerticalTabsEnabled(bool enabled) {
    if (m_verticalTabsEnabled == enabled)
        return;

    m_verticalTabsEnabled = enabled;
    if (m_verticalTabsAction)
        m_verticalTabsAction->setChecked(enabled);
    rebuildCentralLayout();
}
```

For now, `rebuildCentralLayout()` can preserve the existing `m_stackWidget` as the only content widget until Task 3 introduces the sidebar.

- [ ] **Step 8: Run the focused MainWindow test to verify it passes**

Run: `cmake --build build --target test_main_window && ./build/tests/test_main_window testVerticalTabsMenuTogglesPersistedLayout`

Expected: `PASS   : TestMainWindow::testVerticalTabsMenuTogglesPersistedLayout()`

- [ ] **Step 9: Commit the settings-and-window skeleton work**

```bash
git add src/app/AppSettings.h src/app/AppSettings.cpp src/app/MainWindow.h src/app/MainWindow.cpp src/app/settings/default-config.json tests/test_appsettings.cpp tests/test_main_window.cpp
git commit -m "feat: add persisted vertical tabs mode"
```

### Task 2: Expose Pane Snapshots From TermPane

**Files:**
- Modify: `src/app/TermPane.h`
- Modify: `src/app/TermPane.cpp`
- Test: `tests/test_main_window.cpp`

- [ ] **Step 1: Write the failing pane snapshot test**

Add a focused test in `tests/test_main_window.cpp` using the existing `ExposedTermPane` helper:

```cpp
void TestMainWindow::testTermPaneReportsPaneSnapshotsAfterSplit() {
    ExposedTermPane pane;
    pane.resize(1200, 800);
    pane.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pane));

    QCOMPARE(pane.paneInfos().size(), 1);
    QVERIFY(pane.paneInfos().first().isActive);

    pane.splitCurrent(Qt::Vertical);

    const auto infos = pane.paneInfos();
    QCOMPARE(infos.size(), 2);

    int activeCount = 0;
    for (const auto &info : infos) {
        if (info.isActive)
            ++activeCount;
        QVERIFY(!info.id.isNull());
    }
    QCOMPARE(activeCount, 1);
}
```

- [ ] **Step 2: Run the focused pane snapshot test to verify it fails**

Run: `cmake --build build --target test_main_window && ./build/tests/test_main_window testTermPaneReportsPaneSnapshotsAfterSplit`

Expected: build failure because `paneInfos()` and the snapshot type do not yet exist.

- [ ] **Step 3: Add the minimal pane snapshot API**

In `src/app/TermPane.h`, define the snapshot type and new APIs:

```cpp
#include <QUuid>

class TermPane : public QWidget {
    Q_OBJECT

public:
    struct PaneInfo {
        QUuid id;
        QString title;
        bool isActive = false;
    };

    QList<PaneInfo> paneInfos() const;
    QUuid activePaneId() const;
    bool focusPane(const QUuid &paneId);

signals:
    void paneStructureChanged();
    void activePaneChanged(const QUuid &paneId);
    void paneTitleChanged(const QUuid &paneId, const QString &title);
```

Add minimal internal bookkeeping in `src/app/TermPane.cpp` by assigning each `TerminalWidget` a persistent property-backed id:

```cpp
namespace {

QUuid ensurePaneId(TerminalWidget *term) {
    const QVariant existing = term->property("paneId");
    if (existing.isValid())
        return existing.toUuid();

    const QUuid id = QUuid::createUuid();
    term->setProperty("paneId", id);
    return id;
}

QString paneTitle(TerminalWidget *term) {
    return term->property("currentTitle").toString();
}

} // namespace
```

Use the helpers in `createTerminal()`, `setCurrentTerminal(...)`, the title-change lambda, `splitCurrent(...)`, and `removeTerminal(...)` so that:

- every terminal has a stable `paneId`
- `paneStructureChanged()` fires after split/close operations
- `activePaneChanged(...)` fires when the active terminal changes
- `paneTitleChanged(...)` fires when a pane title changes

Implement the read-only API:

```cpp
QList<TermPane::PaneInfo> TermPane::paneInfos() const {
    QList<PaneInfo> infos;
    for (TerminalWidget *term : findChildren<TerminalWidget *>()) {
        PaneInfo info;
        info.id = term->property("paneId").toUuid();
        info.title = term->property("currentTitle").toString();
        info.isActive = (term == m_currentTerm);
        infos.append(info);
    }
    return infos;
}

QUuid TermPane::activePaneId() const {
    if (!m_currentTerm)
        return {};
    return m_currentTerm->property("paneId").toUuid();
}

bool TermPane::focusPane(const QUuid &paneId) {
    for (TerminalWidget *term : findChildren<TerminalWidget *>()) {
        if (term->property("paneId").toUuid() == paneId) {
            setCurrentTerminal(term);
            term->setFocus();
            return true;
        }
    }
    return false;
}
```

- [ ] **Step 4: Run the focused pane snapshot test to verify it passes**

Run: `cmake --build build --target test_main_window && ./build/tests/test_main_window testTermPaneReportsPaneSnapshotsAfterSplit`

Expected: `PASS   : TestMainWindow::testTermPaneReportsPaneSnapshotsAfterSplit()`

- [ ] **Step 5: Commit the pane snapshot API**

```bash
git add src/app/TermPane.h src/app/TermPane.cpp tests/test_main_window.cpp
git commit -m "feat: expose pane snapshots for vertical tabs"
```

### Task 3: Build The Sidebar And Shared Tab Records

**Files:**
- Create: `src/app/VerticalTabSidebar.h`
- Create: `src/app/VerticalTabSidebar.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/test_main_window.cpp`

- [ ] **Step 1: Write the failing sidebar structure test**

Add a focused test to `tests/test_main_window.cpp`:

```cpp
void TestMainWindow::testVerticalSidebarShowsTabsAndPanes() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *sidebar = window.findChild<QWidget *>("verticalTabSidebar");
    QVERIFY(sidebar);

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);

    QVERIFY(QMetaObject::invokeMethod(&window, "onTabAddRequested", Qt::DirectConnection));
    QVERIFY(waitForTabCount(tabs, 2));

    auto *pane = currentPane(window);
    QVERIFY(pane);
    pane->splitCurrent(Qt::Vertical);

    const auto sidebarButtons = sidebar->findChildren<QAbstractButton *>();
    QVERIFY(sidebarButtons.size() >= 3);
}
```

- [ ] **Step 2: Run the focused sidebar structure test to verify it fails**

Run: `cmake --build build --target test_main_window && ./build/tests/test_main_window testVerticalSidebarShowsTabsAndPanes`

Expected: failure because no sidebar widget exists and `MainWindow` does not yet maintain shared tab records.

- [ ] **Step 3: Create the minimal vertical sidebar widget**

Create `src/app/VerticalTabSidebar.h`:

```cpp
#pragma once

#include "TermPane.h"

#include <QWidget>

class QToolButton;
class QVBoxLayout;

class VerticalTabSidebar : public QWidget {
    Q_OBJECT

public:
    struct TabItem {
        int id = 0;
        QString title;
        bool isCurrent = false;
        bool expanded = true;
        QList<TermPane::PaneInfo> panes;
    };

    explicit VerticalTabSidebar(QWidget *parent = nullptr);
    void setItems(const QList<TabItem> &items);

signals:
    void tabActivated(int tabId);
    void tabExpansionToggled(int tabId);
    void paneActivated(int tabId, const QUuid &paneId);

private:
    void rebuild();

    QList<TabItem> m_items;
    QVBoxLayout *m_layout = nullptr;
};
```

Create `src/app/VerticalTabSidebar.cpp` with a minimal rebuild implementation using a `QVBoxLayout`, one top-level `QToolButton` row per tab, and one child `QToolButton` row per pane when expanded:

```cpp
#include "VerticalTabSidebar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

VerticalTabSidebar::VerticalTabSidebar(QWidget *parent) : QWidget(parent), m_layout(new QVBoxLayout(this)) {
    setObjectName(QStringLiteral("verticalTabSidebar"));
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->setSpacing(4);
}

void VerticalTabSidebar::setItems(const QList<TabItem> &items) {
    m_items = items;
    rebuild();
}
```

Inside `rebuild()`, create:

- a top-level button per tab with text from `item.title`
- a small arrow button per tab that emits `tabExpansionToggled(item.id)`
- pane buttons indented under expanded tabs that emit `paneActivated(item.id, pane.id)`

Mark the current tab/pane with a checked or highlighted state, but keep the widget narrow and app-specific.

- [ ] **Step 4: Wire MainWindow to shared tab records and sidebar refresh**

In `src/app/MainWindow.h`, add helpers:

```cpp
    void refreshTabRecords();
    void refreshSidebar();
    int indexOfTabId(int tabId) const;
    TabRecord *tabRecordForPane(TermPane *pane);

    VerticalTabSidebar *m_verticalSidebar = nullptr;
    QSplitter *m_mainSplitter = nullptr;
```

In `addTab(bool activate)`, append a `TabRecord` when the `TermPane` is created:

```cpp
    TabRecord record;
    record.id = m_nextTabId++;
    record.pane = pane;
    record.title = QStringLiteral("Terminal");
    record.expanded = true;
    m_tabs.append(record);
```

When a tab closes, remove the matching record by `pane` pointer before reindexing the visible widgets.

Build the central layout in `rebuildCentralLayout()`:

```cpp
void MainWindow::rebuildCentralLayout() {
    if (!m_contentHost)
        m_contentHost = new QWidget(this);

    auto *layout = qobject_cast<QHBoxLayout *>(m_contentHost->layout());
    if (!layout) {
        layout = new QHBoxLayout(m_contentHost);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
    }

    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget())
            item->widget()->setParent(nullptr);
        delete item;
    }

    if (m_verticalTabsEnabled) {
        if (!m_verticalSidebar)
            m_verticalSidebar = new VerticalTabSidebar(this);
        if (!m_mainSplitter)
            m_mainSplitter = new QSplitter(Qt::Horizontal, this);
        m_mainSplitter->setChildrenCollapsible(false);
        m_mainSplitter->setHandleWidth(1);
        m_mainSplitter->addWidget(m_verticalSidebar);
        m_mainSplitter->addWidget(m_stackWidget);
        m_mainSplitter->setStretchFactor(1, 1);
        layout->addWidget(m_mainSplitter);
    } else {
        layout->addWidget(m_stackWidget);
    }

    setCentralWidget(m_contentHost);
    refreshSidebar();
}
```

Connect `VerticalTabSidebar` signals so:

- `tabActivated(tabId)` calls `gotoTab(indexOfTabId(tabId))`
- `tabExpansionToggled(tabId)` flips `TabRecord.expanded` and calls `refreshSidebar()`
- `paneActivated(tabId, paneId)` activates the tab and calls `record->pane->focusPane(paneId)`

Update `tests/CMakeLists.txt` to compile the new file:

```cmake
  ${CMAKE_SOURCE_DIR}/src/app/VerticalTabSidebar.cpp
```

- [ ] **Step 5: Run the focused sidebar structure test to verify it passes**

Run: `cmake --build build --target test_main_window && ./build/tests/test_main_window testVerticalSidebarShowsTabsAndPanes`

Expected: `PASS   : TestMainWindow::testVerticalSidebarShowsTabsAndPanes()`

- [ ] **Step 6: Commit the sidebar and shared tab record work**

```bash
git add src/app/VerticalTabSidebar.h src/app/VerticalTabSidebar.cpp src/app/MainWindow.h src/app/MainWindow.cpp tests/CMakeLists.txt tests/test_main_window.cpp
git commit -m "feat: add vertical tab sidebar"
```

### Task 4: Title Synchronization, Translation, And Final Verification

**Files:**
- Modify: `src/app/MainWindow.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/TermPane.cpp`
- Modify: `src/app/settings/settings_translation.cpp`
- Modify: `tests/test_main_window.cpp`
- Modify: `tests/test_appsettings.cpp`

- [ ] **Step 1: Write the failing title-sync and expand-state tests**

Add these focused tests to `tests/test_main_window.cpp`:

```cpp
void TestMainWindow::testActivePaneTitleUpdatesTabAndWindowTitles() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *pane = currentPane(window);
    QVERIFY(pane);
    pane->splitCurrent(Qt::Vertical);

    auto infos = pane->paneInfos();
    QVERIFY(infos.size() >= 2);
    QVERIFY(pane->focusPane(infos.last().id));
    pane->setCustomTitle(QStringLiteral("Logs"));

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("Logs"));
    QCOMPARE(window.windowTitle(), QStringLiteral("Logs"));
}

void TestMainWindow::testSidebarExpansionSurvivesModeSwitch() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *verticalAction = window.findChild<QAction *>("verticalTabsAction");
    QVERIFY(verticalAction);

    auto *sidebar = window.findChild<VerticalTabSidebar *>("verticalTabSidebar");
    QVERIFY(sidebar);

    QMetaObject::invokeMethod(sidebar, "tabExpansionToggled", Qt::DirectConnection, Q_ARG(int, 1));

    verticalAction->setChecked(false);
    verticalAction->setChecked(true);

    sidebar = window.findChild<VerticalTabSidebar *>("verticalTabSidebar");
    QVERIFY(sidebar);
    QVERIFY(sidebar->findChildren<QAbstractButton *>().size() >= 1);
}
```

- [ ] **Step 2: Run the focused tests to verify they fail**

Run: `cmake --build build --target test_main_window && ./build/tests/test_main_window testActivePaneTitleUpdatesTabAndWindowTitles testSidebarExpansionSurvivesModeSwitch`

Expected: failure because `MainWindow` is not yet refreshing all title surfaces from `TermPane` pane state and the expand/collapse state is not yet preserved through a full mode switch path.

- [ ] **Step 3: Normalize title refresh and sidebar updates**

In `src/app/MainWindow.h`, add:

```cpp
    void refreshTabRecord(TabRecord &record);
    void syncTabWidgetsFromRecords();
```

In `src/app/MainWindow.cpp`, implement a single refresh path:

```cpp
void MainWindow::refreshTabRecord(TabRecord &record) {
    if (!record.pane)
        return;

    const auto infos = record.pane->paneInfos();
    for (const auto &info : infos) {
        if (info.isActive) {
            record.title = info.title.isEmpty() ? tr("Terminal") : info.title;
            break;
        }
    }
}

void MainWindow::syncTabWidgetsFromRecords() {
    for (int i = 0; i < m_tabs.size() && i < m_tabBar->count(); ++i)
        m_tabBar->setTabText(i, m_tabs.at(i).title);

    const int index = m_tabBar->currentIndex();
    if (index >= 0 && index < m_tabs.size())
        setWindowTitle(m_tabs.at(index).title);

    refreshSidebar();
}
```

Hook this path from:

- `onTerminalTitleChanged(...)`
- `onPaneTerminalChanged(...)`
- the new `paneStructureChanged`
- the new `activePaneChanged`
- the new `paneTitleChanged`
- `onTabCurrentChanged(...)`

Make `refreshSidebar()` build each sidebar item from the current `TabRecord` plus `record.pane->paneInfos()`.

Update the compact titlebar widget when vertical mode is enabled by replacing the tab-wrapper custom widget with a fixed-height padded placeholder widget:

```cpp
    QWidget *compactTitlebarWidget = new QWidget(this);
    compactTitlebarWidget->setFixedHeight(36);
    auto *compactLayout = new QHBoxLayout(compactTitlebarWidget);
    compactLayout->setContentsMargins(12, 6, 12, 6);
    tb->setCustomWidget(compactTitlebarWidget);
```

- [ ] **Step 4: Add the new translatable UI string registration**

In `src/app/settings/settings_translation.cpp`, add:

```cpp
    auto window_vertical_tabsName = QObject::tr("Vertical Tabs");
```

If the file uses grouped no-op variables only for extraction, keep the same pattern and naming style already present there.

- [ ] **Step 5: Run focused tests and then the relevant full set**

Run the focused checks first:

`cmake --build build --target test_main_window test_appsettings && ./build/tests/test_main_window testActivePaneTitleUpdatesTabAndWindowTitles testSidebarExpansionSurvivesModeSwitch && ./build/tests/test_appsettings`

Expected: both focused `test_main_window` cases pass and all `test_appsettings` cases pass.

Then run the broader affected suite:

`cd build && ctest --output-on-failure -R "AppSettings|MainWindow"`

Expected: all matching tests pass.

- [ ] **Step 6: Run formatting verification**

Run: `clang-format --dry-run --Werror $(find src tests -name '*.cpp' -o -name '*.h')`

Expected: no output and exit code 0.

- [ ] **Step 7: Commit the final vertical-tabs integration**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp src/app/TermPane.h src/app/TermPane.cpp src/app/settings/settings_translation.cpp tests/test_main_window.cpp tests/test_appsettings.cpp
git commit -m "feat: add switchable vertical tabs layout"
```

## Self-Review

### Spec coverage

- persisted vertical-tabs menu item: covered in Task 1
- compact titlebar and layout switching: covered in Task 1 and Task 4
- shared tab metadata between horizontal and vertical UIs: covered in Task 3 and Task 4
- two-level sidebar with expand/collapse: covered in Task 3 and Task 4
- pane snapshot exposure from `TermPane`: covered in Task 2
- title synchronization from active pane: covered in Task 4
- translation and settings integration: covered in Task 1 and Task 4
- focused verification for `AppSettings` and `MainWindow`: covered in every task and finalized in Task 4

No spec requirement is currently uncovered.

### Placeholder scan

- no `TODO`, `TBD`, or “implement later” placeholders remain
- every task includes exact files, commands, expected outcomes, and concrete code fragments

### Type consistency

- `TabRecord`, `PaneInfo`, `verticalTabsEnabled`, `setVerticalTabsEnabled`, `paneInfos`, and `focusPane` are named consistently across tasks
- `VerticalTabSidebar::TabItem` is used consistently as the sidebar input type

