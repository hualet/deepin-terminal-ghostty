# Manual Session Restore Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a Chrome-style "Restore Previous Session" feature: new windows open blank, users restore via menu item or `Ctrl+Shift+R` shortcut.

**Architecture:** Add `"manual"` as a third `sessionRestoreBehavior` option. When active, the constructor skips all restore logic and creates a blank tab. A new `restorePreviousSession()` method on `MainWindow` handles on-demand restore, triggered by a menu action and a configurable shortcut. The restore **always appends** tabs — it never tries to detect or replace existing tabs (see "Why always append" in spec).

**Tech Stack:** C++20, Qt6 Widgets, DTK6, existing `SessionManager` / `SessionSnapshot` infrastructure.

**Important:** Do not create git commits unless the user explicitly asks. The commit guidance in each task describes the scope and message for when commits are requested.

---

### Task 1: Add `"manual"` behavior to settings

**Files:**
- Modify: `src/app/settings/default-config.json:324-328`
- Modify: `src/app/AppSettings.cpp:126-132`

- [ ] **Step 1: Update `default-config.json` default value**

Change the default for `sessionRestoreBehavior` from `"ask"` to `"manual"`:

```json
                        {
                            "key": "sessionRestoreBehavior",
                            "name": "Restore behavior",
                            "type": "combobox",
                            "default": "manual"
                        }
```

- [ ] **Step 2: Add `"manual"` option to `AppSettings.cpp` behavior combobox items**

In `AppSettings::init()` around line 126, add `"manual"` to the keys/values lists:

```cpp
    auto behaviorOption = m_dsettings->option(kSessionRestoreBehaviorPath);
    if (behaviorOption) {
        QMap<QString, QVariant> items;
        items.insert(QStringLiteral("keys"),
                     QStringList() << QStringLiteral("ask") << QStringLiteral("auto") << QStringLiteral("manual"));
        items.insert(QStringLiteral("values"),
                     QStringList() << tr("Ask") << tr("Restore automatically") << tr("Manual (menu triggered)"));
        behaviorOption->setData(QStringLiteral("items"), items);
    }
```

- [ ] **Step 3: Build and verify settings loads**

Run: `cmake -B build && cmake --build build`
Expected: Clean build, no errors.

---

### Task 2: Handle `"manual"` in MainWindow constructor

**Files:**
- Modify: `src/app/MainWindow.cpp:154-179`

- [ ] **Step 1: Update constructor to skip restore for `"manual"` behavior**

In the constructor, the current restore block (lines 154-179) checks for `"auto"` and falls through to the `"ask"` dialog. Add an explicit `"ask"` guard so `"manual"` falls through safely:

Replace the block starting at `bool restored = false;` through the closing `}` of the outer `if`:

```cpp
    bool restored = false;
    if (AppSettings::instance()->sessionRestore() && m_startupOptions.execute.isEmpty()
        && m_startupOptions.workingDirectory.isEmpty()) {
        auto &mgr = SessionManager::instance();
        if (mgr.hasSnapshot()) {
            QString behavior = AppSettings::instance()->sessionRestoreBehavior();
            if (behavior == QStringLiteral("auto")) {
                restoreSession();
                restored = true;
            } else if (behavior == QStringLiteral("ask")) {
                auto *dlg = new DDialog(this);
                dlg->setWindowTitle(tr("Restore Session"));
                dlg->setMessage(tr("A previous terminal session was found. Restore it?"));
                dlg->addButton(tr("New Terminal"), false, DDialog::ButtonNormal);
                dlg->addButton(tr("Restore Session"), true, DDialog::ButtonRecommend);
                int result = dlg->exec();
                dlg->deleteLater();
                if (result == 1) {
                    restoreSession();
                    restored = true;
                } else {
                    mgr.clearSnapshot();
                }
            }
        }
    }
```

The key change: the `"ask"` behavior is now explicitly guarded by `else if (behavior == QStringLiteral("ask"))`, so `"manual"` falls through and creates a blank tab without dialog or snapshot clearing.

- [ ] **Step 2: Build and verify**

Run: `cmake --build build`
Expected: Clean build.

---

### Task 3: Add `restorePreviousSession()` method

**Files:**
- Modify: `src/app/MainWindow.h:99` (declare method)
- Modify: `src/app/MainWindow.h:118` (add action member)
- Modify: `src/app/MainWindow.h:144` (add shortcut member)
- Modify: `src/app/MainWindow.cpp:1327` (after `restoreSession()`)

- [ ] **Step 1: Declare method and members in `MainWindow.h`**

Add after the existing `restoreSession()` declaration at line 99:

```cpp
    void restorePreviousSession();
```

Add member for the menu action, after `m_hasConfirmedClose` (line 118):

```cpp
    QAction *m_restoreSessionAction = nullptr;
```

Add shortcut member after `m_scRemoteManagement` (line 144):

```cpp
    QShortcut *m_scRestoreSession = nullptr;
```

- [ ] **Step 2: Implement `restorePreviousSession()` in `MainWindow.cpp`**

Add after the existing `restoreSession()` method (after line 1327). This implementation **always appends** — it never removes existing tabs:

```cpp
void MainWindow::restorePreviousSession() {
    auto &mgr = SessionManager::instance();
    if (!mgr.hasSnapshot()) {
        auto *dlg = new DDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowTitle(tr("Restore Session"));
        dlg->setMessage(tr("No previous session found."));
        dlg->addButton(tr("OK"), true, DDialog::ButtonNormal);
        dlg->show();
        return;
    }

    WindowSnapshot snapshot = mgr.loadSnapshot();
    if (snapshot.tabs.isEmpty())
        return;

    int firstRestoredIndex = m_tabBar ? m_tabBar->count() : 0;

    for (const auto &tabSnap : snapshot.tabs) {
        addTab(false, std::nullopt);
        auto *pane = qobject_cast<TermPane *>(m_stackWidget->widget(m_stackWidget->count() - 1));
        if (!pane)
            continue;

        QList<QPair<QString, TerminalWidget *>> terminals = pane->restoreFromSplitTree(tabSnap.pane);

        for (const auto &[uuid, term] : terminals) {
            QByteArray vtData = mgr.loadVtContent(uuid);
            if (!vtData.isEmpty())
                term->importVtContent(vtData);
        }

        if (auto *record = tabRecordForPane(pane)) {
            record->title = tabSnap.title;
        }
    }

    syncTabWidgetsFromRecords();
    if (m_tabBar && m_tabBar->count() > 0)
        m_tabBar->setCurrentIndex(firstRestoredIndex);
}
```

Why always append:
- `tabCount() == 1` is not a reliable blank-window indicator — the user may already be working in their only tab.
- Calling `onTabCloseRequested(0)` on the last tab triggers `close()` (see `MainWindow.cpp:505-508`), which destroys the window.
- Even a forced removal path risks destroying user work. Append is always safe.
- The snapshot is not deleted after restore, so the menu action remains enabled for repeated use.

- [ ] **Step 3: Build and verify**

Run: `cmake --build build`
Expected: Clean build (method exists but nothing calls it yet — will wire up in Task 4).

---

### Task 4: Add menu item and shortcut

**Files:**
- Modify: `src/app/settings/default-config.json:270-307` (shortcuts section)
- Modify: `src/app/MainWindow.cpp:277-302` (menu setup area)
- Modify: `src/app/MainWindow.cpp:790-912` (setupShortcuts)
- Modify: `src/app/settings/settings_translation.cpp:30`

- [ ] **Step 1: Add shortcut definition to `default-config.json`**

In the `"advanced"` shortcuts group (line 270-307), add a new shortcut entry after the `remote_management` entry (after line 304):

```json
                        {
                            "key": "restore_session",
                            "name": "Restore previous session",
                            "type": "shortcut",
                            "default": "Ctrl+Shift+R"
                        }
```

- [ ] **Step 2: Add menu action in `setupTitleBar()` area**

In `MainWindow.cpp`, after the `remoteAction` connect block (around line 294) and **before the existing `menu->addSeparator()`** (line 302), insert the restore session action:

```cpp
    m_restoreSessionAction = menu->addAction(tr("Restore Previous Session"));
    m_restoreSessionAction->setObjectName(QStringLiteral("restoreSessionAction"));
    m_restoreSessionAction->setToolTip(tr("Restore the last saved terminal session"));
    m_restoreSessionAction->setStatusTip(tr("Restore the last saved terminal session"));
    m_restoreSessionAction->setEnabled(SessionManager::instance().hasSnapshot()
                                       && AppSettings::instance()->sessionRestore());
    connect(m_restoreSessionAction, &QAction::triggered, this, &MainWindow::restorePreviousSession);
```

The existing `menu->addSeparator()` at line 302 already separates these actions from the theme menu below, so no new separator is needed.

- [ ] **Step 3: Register shortcut in `setupShortcuts()`**

In the `setupShortcuts()` method, after the `m_scRemoteManagement` createOnce call (line 856), add:

```cpp
    createOnce(m_scRestoreSession, &MainWindow::restorePreviousSession);
```

And in the `updateShortcut` calls section, after line 902 (`updateShortcut(m_scRemoteManagement, "remote_management");`), add:

```cpp
    updateShortcut(m_scRestoreSession, "restore_session");
```

- [ ] **Step 4: Add i18n entry in `settings_translation.cpp`**

Add after the remote_management line (line 30):

```cpp
    auto shortcuts_advanced_restore_sessionName = QObject::tr("Restore previous session");
```

- [ ] **Step 5: Build and verify**

Run: `cmake -B build && cmake --build build`
Expected: Clean build.

- [ ] **Step 6: Manual smoke test**

Run: `./build/deepin-terminal-ghostty`
Expected:
- Window opens with a blank terminal (no restore dialog, no auto-restore).
- Hamburger menu shows "Restore Previous Session" (grayed out if no snapshot).
- If a previous session was saved, clicking the menu item or pressing `Ctrl+Shift+R` restores tabs + VT content.
- Restored tabs appear after the existing tab(s). Original tab is preserved.
- Action remains enabled after restore (snapshot is not deleted).

---

### Task 5: Update menu action state after window close saves snapshot

No code change needed — each `MainWindow` creates its menu fresh on construction. The enabled state is set correctly at that point. Snapshots only appear after a *different* window closes, so no dynamic update is needed within a single window's lifetime.

---

### Task 6: Add automated tests

**Files:**
- Modify: `tests/test_main_window.cpp`

**Important:** All tests that create a `MainWindow` with an existing snapshot must explicitly set `sessionRestoreBehavior` to `"manual"` to avoid the `"ask"` dialog hanging the test under `QT_QPA_PLATFORM=offscreen`.

- [ ] **Step 1: Write test for `"manual"` behavior opening blank window**

```cpp
void TestMainWindow::testManualBehaviorOpensBlankWindow() {
    auto *settings = AppSettings::instance();
    settings->dsettings()->setOption("advanced.session.sessionRestore", true);
    settings->dsettings()->setOption("advanced.session.sessionRestoreBehavior", QStringLiteral("manual"));

    SessionManager::instance().clearSnapshot();

    WindowSnapshot snap;
    snap.width = 800;
    snap.height = 600;
    snap.tabs.append(TabSnapshot{1, QStringLiteral("Saved tab"), SplitNode{}});
    QList<QPair<QString, TerminalWidget *>> noTerminals;
    SessionManager::instance().save(snap, noTerminals);
    QVERIFY(SessionManager::instance().hasSnapshot());

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);
    QCOMPARE(window.windowTitle(), QStringLiteral("deepin-terminal-ghostty"));

    SessionManager::instance().clearSnapshot();
}
```

- [ ] **Step 2: Write test for menu action existence and disabled state**

```cpp
void TestMainWindow::testRestoreSessionMenuActionDisabledWithoutSnapshot() {
    auto *settings = AppSettings::instance();
    settings->dsettings()->setOption("advanced.session.sessionRestoreBehavior", QStringLiteral("manual"));

    SessionManager::instance().clearSnapshot();

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *action = window.findChild<QAction *>(QStringLiteral("restoreSessionAction"));
    QVERIFY(action);
    QCOMPARE(action->text(), QStringLiteral("Restore Previous Session"));
    QVERIFY(!action->isEnabled());
}
```

- [ ] **Step 3: Write test for restore menu action enabled with snapshot**

```cpp
void TestMainWindow::testRestoreSessionMenuActionEnabledWithSnapshot() {
    auto *settings = AppSettings::instance();
    settings->dsettings()->setOption("advanced.session.sessionRestore", true);
    settings->dsettings()->setOption("advanced.session.sessionRestoreBehavior", QStringLiteral("manual"));

    WindowSnapshot snap;
    snap.width = 800;
    snap.height = 600;
    snap.tabs.append(TabSnapshot{1, QStringLiteral("Saved tab"), SplitNode{}});
    QList<QPair<QString, TerminalWidget *>> noTerminals;
    SessionManager::instance().save(snap, noTerminals);
    QVERIFY(SessionManager::instance().hasSnapshot());

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *action = window.findChild<QAction *>(QStringLiteral("restoreSessionAction"));
    QVERIFY(action);
    QVERIFY(action->isEnabled());

    SessionManager::instance().clearSnapshot();
}
```

- [ ] **Step 4: Register test methods in the class declaration**

Add to the `TestMainWindow` class private slots (after the existing test declarations):

```cpp
    void testManualBehaviorOpensBlankWindow();
    void testRestoreSessionMenuActionDisabledWithoutSnapshot();
    void testRestoreSessionMenuActionEnabledWithSnapshot();
```

Also add the required includes at the top of the test file if not already present:

```cpp
#include "SessionManager.h"
#include "SessionSnapshot.h"
```

- [ ] **Step 5: Build and run tests**

Run: `cmake -B build -DBUILD_TESTING=ON && cmake --build build && cd build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure -R test_main_window`
Expected: All tests pass, including the 3 new ones.

---

### Task 7: Format and final verification

**Files:**
- All changed files

- [ ] **Step 1: Run clang-format**

Run: `clang-format -i $(find src tests -name '*.cpp' -o -name '*.h')`

- [ ] **Step 2: Verify clang-format passes**

Run: `clang-format --dry-run --Werror $(find src tests -name '*.cpp' -o -name '*.h')`
Expected: No errors.

- [ ] **Step 3: Run full test suite**

Run: `cd build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure`
Expected: All tests pass.

---

## Spec Coverage Check

| Spec Requirement | Task |
|---|---|
| `"manual"` behavior option in settings | Task 1 |
| Constructor skips restore for `"manual"` | Task 2 |
| `restorePreviousSession()` method | Task 3 |
| Menu item in hamburger menu | Task 4 |
| Shortcut `Ctrl+Shift+R` | Task 4 |
| Always append (never replace existing tabs) | Task 3 |
| Enabled when snapshot exists and session restore is on | Task 4 |
| Remains enabled after restore (repeated restore supported) | Task 3 |
| Settings UI dropdown option | Task 1 |
| Tests | Task 6 |
