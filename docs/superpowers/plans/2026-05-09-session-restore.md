# Session Restore Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Save terminal session state (tabs, splits, working directories, terminal content with VT formatting) on window close and offer to restore it on next launch.

**Architecture:** TerminalWidget exposes `exportVtContent()`/`importVtContent()` that wrap Ghostty's formatter API. SessionSnapshot holds pure data models for the layout tree. SessionManager handles file I/O. MainWindow orchestrates save on close and restore on launch with a DDialog prompt.

**Tech Stack:** C++20, Qt6 JSON, Ghostty VT Formatter API, DTK6 DDialog

---

### Task 1: Add exportVtContent/importVtContent to TerminalWidget

**Files:**
- Modify: `src/libqtghostty/TerminalWidget.h`
- Modify: `src/libqtghostty/TerminalWidget.cpp`
- Test: `tests/test_terminal_widget.cpp`

- [ ] **Step 1: Add declarations to TerminalWidget.h**

Add these two public method declarations to `TerminalWidget.h`, after the existing public methods (after `scrollViewportToOffset` around line 59):

```cpp
QByteArray exportVtContent() const;
void importVtContent(const QByteArray &data);
```

- [ ] **Step 2: Implement exportVtContent in TerminalWidget.cpp**

Add at the end of the file (before the selection helper methods section), after the `scrollbackByteBudget()` method. This uses `ghostty_formatter_terminal_new` with `GHOSTTY_FORMATTER_FORMAT_VT` and all extra flags enabled:

```cpp
QByteArray TerminalWidget::exportVtContent() const {
    if (!m_terminal)
        return {};

    GhosttyFormatterScreenExtra screenExtra = {
        .size = sizeof(GhosttyFormatterScreenExtra),
        .cursor = true,
        .style = true,
        .hyperlink = true,
        .protection = true,
        .kitty_keyboard = true,
        .charsets = true,
    };
    GhosttyFormatterTerminalExtra terminalExtra = {
        .size = sizeof(GhosttyFormatterTerminalExtra),
        .palette = true,
        .modes = true,
        .scrolling_region = true,
        .tabstops = true,
        .pwd = true,
        .keyboard = true,
        .screen = screenExtra,
    };
    GhosttyFormatterTerminalOptions opts = {
        .size = sizeof(GhosttyFormatterTerminalOptions),
        .emit = GHOSTTY_FORMATTER_FORMAT_VT,
        .unwrap = false,
        .trim = false,
        .extra = terminalExtra,
        .selection = nullptr,
    };

    GhosttyFormatter formatter = nullptr;
    GhosttyResult err = ghostty_formatter_terminal_new(nullptr, &formatter, m_terminal, opts);
    if (err != GHOSTTY_SUCCESS || !formatter)
        return {};

    uint8_t *outPtr = nullptr;
    size_t outLen = 0;
    err = ghostty_formatter_format_alloc(formatter, nullptr, &outPtr, &outLen);
    ghostty_formatter_free(formatter);

    if (err != GHOSTTY_SUCCESS || !outPtr || outLen == 0)
        return {};

    QByteArray result(reinterpret_cast<const char *>(outPtr), static_cast<int>(outLen));
    ghostty_free(nullptr, outPtr, outLen);
    return result;
}
```

- [ ] **Step 3: Implement importVtContent in TerminalWidget.cpp**

```cpp
void TerminalWidget::importVtContent(const QByteArray &data) {
    if (!m_terminal || data.isEmpty())
        return;

    ghostty_terminal_vt_write(m_terminal, reinterpret_cast<const uint8_t *>(data.constData()),
                              static_cast<size_t>(data.size()));
    m_renderStateDirty = true;
    update();
}
```

- [ ] **Step 4: Verify it compiles**

Run: `cmake -B build -DBUILD_TESTING=ON && cmake --build build 2>&1 | tail -20`
Expected: Build succeeds with no errors.

- [ ] **Step 5: Commit**

```bash
git add src/libqtghostty/TerminalWidget.h src/libqtghostty/TerminalWidget.cpp
git commit -m "feat: add exportVtContent/importVtContent to TerminalWidget"
```

---

### Task 2: Create SessionSnapshot data model

**Files:**
- Create: `src/app/SessionSnapshot.h`
- Create: `src/app/SessionSnapshot.cpp`
- Create: `tests/test_session_snapshot.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Create SessionSnapshot.h**

```cpp
#pragma once

#include <QList>
#include <QString>
#include <QVariant>
#include <QtGlobal>

struct TerminalSnapshot {
    QString uuid;
    QString workingDirectory;
    QString title;
};

struct SplitNode {
    enum class Type { Terminal, Split };
    Type type = Type::Terminal;

    QString uuid;
    QString workingDirectory;
    QString title;

    Qt::Orientation orientation = Qt::Horizontal;
    QList<int> sizes;
    QList<SplitNode> children;

    static SplitNode terminal(const QString &uuid, const QString &workingDirectory, const QString &title) {
        return {.type = Type::Terminal, .uuid = uuid, .workingDirectory = workingDirectory, .title = title};
    }

    static SplitNode split(Qt::Orientation orientation, QList<int> sizes, QList<SplitNode> children) {
        return {.type = Type::Split, .orientation = orientation, .sizes = std::move(sizes), .children = std::move(children)};
    }
};

struct TabSnapshot {
    int id = 0;
    QString title;
    SplitNode pane;
};

struct WindowSnapshot {
    int width = 0;
    int height = 0;
    bool isMaximized = false;
    QList<TabSnapshot> tabs;
};

QJsonObject splitNodeToJson(const SplitNode &node);
SplitNode splitNodeFromJson(const QJsonObject &obj);

QJsonObject tabSnapshotToJson(const TabSnapshot &tab);
TabSnapshot tabSnapshotFromJson(const QJsonObject &obj);

QJsonObject windowSnapshotToJson(const WindowSnapshot &snapshot);
WindowSnapshot windowSnapshotFromJson(const QJsonObject &obj);
```

- [ ] **Step 2: Create SessionSnapshot.cpp**

Implement JSON serialization/deserialization for all types. Each `SplitNode` serializes as:
- Terminal: `{"type": "terminal", "uuid": "...", "workingDirectory": "...", "title": "..."}`
- Split: `{"type": "split", "orientation": "horizontal"|"vertical", "sizes": [...], "children": [...]}`

```cpp
#include "SessionSnapshot.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QJsonObject splitNodeToJson(const SplitNode &node) {
    QJsonObject obj;
    if (node.type == SplitNode::Type::Terminal) {
        obj["type"] = "terminal";
        obj["uuid"] = node.uuid;
        obj["workingDirectory"] = node.workingDirectory;
        obj["title"] = node.title;
    } else {
        obj["type"] = "split";
        obj["orientation"] = node.orientation == Qt::Horizontal ? "horizontal" : "vertical";
        QJsonArray sizesArr;
        for (int s : node.sizes)
            sizesArr.append(s);
        obj["sizes"] = sizesArr;
        QJsonArray childrenArr;
        for (const auto &child : node.children)
            childrenArr.append(splitNodeToJson(child));
        obj["children"] = childrenArr;
    }
    return obj;
}

SplitNode splitNodeFromJson(const QJsonObject &obj) {
    QString type = obj["type"].toString();
    if (type == "terminal") {
        return SplitNode::terminal(obj["uuid"].toString(), obj["workingDirectory"].toString(), obj["title"].toString());
    }
    Qt::Orientation orientation = obj["orientation"].toString() == "horizontal" ? Qt::Horizontal : Qt::Vertical;
    QList<int> sizes;
    QJsonArray sizesArr = obj["sizes"].toArray();
    for (const auto &v : sizesArr)
        sizes.append(v.toInt());
    QList<SplitNode> children;
    QJsonArray childrenArr = obj["children"].toArray();
    for (const auto &v : childrenArr)
        children.append(splitNodeFromJson(v.toObject()));
    return SplitNode::split(orientation, sizes, children);
}

QJsonObject tabSnapshotToJson(const TabSnapshot &tab) {
    QJsonObject obj;
    obj["id"] = tab.id;
    obj["title"] = tab.title;
    obj["pane"] = splitNodeToJson(tab.pane);
    return obj;
}

TabSnapshot tabSnapshotFromJson(const QJsonObject &obj) {
    TabSnapshot tab;
    tab.id = obj["id"].toInt();
    tab.title = obj["title"].toString();
    tab.pane = splitNodeFromJson(obj["pane"].toObject());
    return tab;
}

QJsonObject windowSnapshotToJson(const WindowSnapshot &snapshot) {
    QJsonObject window;
    window["width"] = snapshot.width;
    window["height"] = snapshot.height;
    window["isMaximized"] = snapshot.isMaximized;

    QJsonObject obj;
    obj["version"] = 1;
    obj["window"] = window;
    QJsonArray tabsArr;
    for (const auto &tab : snapshot.tabs)
        tabsArr.append(tabSnapshotToJson(tab));
    obj["tabs"] = tabsArr;
    return obj;
}

WindowSnapshot windowSnapshotFromJson(const QJsonObject &obj) {
    WindowSnapshot snapshot;
    QJsonObject window = obj["window"].toObject();
    snapshot.width = window["width"].toInt();
    snapshot.height = window["height"].toInt();
    snapshot.isMaximized = window["isMaximized"].toBool();
    QJsonArray tabsArr = obj["tabs"].toArray();
    for (const auto &v : tabsArr)
        snapshot.tabs.append(tabSnapshotFromJson(v.toObject()));
    return snapshot;
}
```

- [ ] **Step 3: Add test file tests/test_session_snapshot.cpp**

Write tests for:
- Round-trip: terminal node → JSON → terminal node (verify all fields preserved)
- Round-trip: split node with nested children → JSON → split node
- Round-trip: full WindowSnapshot with multiple tabs → JSON → WindowSnapshot
- Edge case: empty tabs list

```cpp
#include <QTest>

#include "SessionSnapshot.h"

class TestSessionSnapshot : public QObject {
    Q_OBJECT

private slots:
    void testTerminalNodeRoundTrip() {
        SplitNode original = SplitNode::terminal("uuid-1", "/home/user", "bash");
        QJsonObject json = splitNodeToJson(original);
        SplitNode restored = splitNodeFromJson(json);
        QCOMPARE(restored.type, SplitNode::Type::Terminal);
        QCOMPARE(restored.uuid, "uuid-1");
        QCOMPARE(restored.workingDirectory, "/home/user");
        QCOMPARE(restored.title, "bash");
    }

    void testSplitNodeRoundTrip() {
        SplitNode original = SplitNode::split(
            Qt::Vertical, {300, 500},
            {SplitNode::terminal("a", "/tmp", "vim"), SplitNode::terminal("b", "/var", "top")});
        QJsonObject json = splitNodeToJson(original);
        SplitNode restored = splitNodeFromJson(json);
        QCOMPARE(restored.type, SplitNode::Type::Split);
        QCOMPARE(restored.orientation, Qt::Vertical);
        QCOMPARE(restored.sizes, QList<int>{300, 500});
        QCOMPARE(restored.children.size(), 2);
        QCOMPARE(restored.children[0].uuid, "a");
        QCOMPARE(restored.children[1].uuid, "b");
    }

    void testWindowSnapshotRoundTrip() {
        WindowSnapshot original;
        original.width = 1200;
        original.height = 800;
        original.isMaximized = true;
        original.tabs.append({1, "tab1", SplitNode::terminal("u1", "/home", "bash")});
        original.tabs.append({2, "tab2", SplitNode::terminal("u2", "/tmp", "vim")});
        QJsonObject json = windowSnapshotToJson(original);
        QCOMPARE(json["version"].toInt(), 1);
        QVERIFY(json.contains("window"));
        QCOMPARE(json["window"].toObject()["width"].toInt(), 1200);
        WindowSnapshot restored = windowSnapshotFromJson(json);
        QCOMPARE(restored.width, 1200);
        QCOMPARE(restored.height, 800);
        QVERIFY(restored.isMaximized);
        QCOMPARE(restored.tabs.size(), 2);
        QCOMPARE(restored.tabs[0].pane.uuid, "u1");
        QCOMPARE(restored.tabs[1].title, "tab2");
    }

    void testEmptyTabsRoundTrip() {
        WindowSnapshot original;
        original.width = 960;
        original.height = 640;
        QJsonObject json = windowSnapshotToJson(original);
        WindowSnapshot restored = windowSnapshotFromJson(json);
        QCOMPARE(restored.tabs.size(), 0);
        QCOMPARE(restored.width, 960);
    }

    void testNestedSplitRoundTrip() {
        SplitNode nested = SplitNode::split(
            Qt::Vertical, {200, 300},
            {SplitNode::terminal("c1", "/a", "vim"), SplitNode::terminal("c2", "/b", "top")});
        SplitNode original = SplitNode::split(
            Qt::Horizontal, {500, 500},
            {SplitNode::terminal("root", "/home", "bash"), nested});
        QJsonObject json = splitNodeToJson(original);
        SplitNode restored = splitNodeFromJson(json);
        QCOMPARE(restored.type, SplitNode::Type::Split);
        QCOMPARE(restored.children.size(), 2);
        QCOMPARE(restored.children[0].uuid, "root");
        QCOMPARE(restored.children[1].type, SplitNode::Type::Split);
        QCOMPARE(restored.children[1].children.size(), 2);
        QCOMPARE(restored.children[1].children[0].uuid, "c1");
        QCOMPARE(restored.children[1].children[1].uuid, "c2");
        QCOMPARE(restored.children[1].orientation, Qt::Vertical);
        QCOMPARE(restored.children[1].sizes, QList<int>{200, 300});
    }
};

QTEST_MAIN(TestSessionSnapshot)
#include "test_session_snapshot.moc"
```

- [ ] **Step 4: Add test to tests/CMakeLists.txt**

Add after the existing test definitions (before the final `add_test` for `EsctestWrapperSmoke`):

```cmake
add_executable(test_session_snapshot
  test_session_snapshot.cpp
  ${CMAKE_SOURCE_DIR}/src/app/SessionSnapshot.cpp
)

target_link_libraries(test_session_snapshot PRIVATE
  Qt6::Core
  Qt6::Test
)

add_test(NAME SessionSnapshot COMMAND test_session_snapshot)
```

- [ ] **Step 5: Build and run test**

Run: `cmake -B build -DBUILD_TESTING=ON && cmake --build build && cd build && ctest -R SessionSnapshot --output-on-failure`
Expected: All 5 tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/app/SessionSnapshot.h src/app/SessionSnapshot.cpp tests/test_session_snapshot.cpp tests/CMakeLists.txt
git commit -m "feat: add SessionSnapshot data model with JSON serialization"
```

---

### Task 3: Create SessionManager

**Files:**
- Create: `src/app/SessionManager.h`
- Create: `src/app/SessionManager.cpp`
- Create: `tests/test_session_manager.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Create SessionManager.h**

```cpp
#pragma once

#include "SessionSnapshot.h"

#include <QDir>
#include <QString>

class TerminalWidget;

class SessionManager {
public:
    static SessionManager &instance();

    bool hasSnapshot() const;
    WindowSnapshot loadSnapshot() const;
    QByteArray loadVtContent(const QString &uuid) const;
    void save(const WindowSnapshot &snapshot, const QList<QPair<QString, TerminalWidget *>> &terminals);
    void clearSnapshot();
    QString snapshotTimestamp() const;

private:
    SessionManager() = default;
    QDir sessionDir() const;
    void ensureSessionDir() const;
};
```

- [ ] **Step 2: Create SessionManager.cpp**

```cpp
#include "SessionManager.h"
#include "TerminalWidget.h"
#include "logging/Logging.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>

SessionManager &SessionManager::instance() {
    static SessionManager mgr;
    return mgr;
}

QDir SessionManager::sessionDir() const {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/session");
}

void SessionManager::ensureSessionDir() const {
    QDir dir = sessionDir();
    if (!dir.exists())
        dir.mkpath(".");
}

bool SessionManager::hasSnapshot() const {
    return QFile::exists(sessionDir().filePath("snapshot.json"));
}

WindowSnapshot SessionManager::loadSnapshot() const {
    QFile file(sessionDir().filePath("snapshot.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return {};
    return windowSnapshotFromJson(doc.object());
}

QByteArray SessionManager::loadVtContent(const QString &uuid) const {
    QFile file(sessionDir().filePath("terminals/" + uuid + ".vt"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

void SessionManager::save(const WindowSnapshot &snapshot,
                           const QList<QPair<QString, TerminalWidget *>> &terminals) {
    clearSnapshot();
    ensureSessionDir();

    QJsonObject root = windowSnapshotToJson(snapshot);
    root["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonDocument doc(root);

    QFile file(sessionDir().filePath("snapshot.json"));
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(appLog) << "Failed to write session snapshot";
        return;
    }
    file.write(doc.toJson());

    QDir termDir = sessionDir();
    termDir.mkpath("terminals");
    for (const auto &[uuid, term] : terminals) {
        QByteArray vtData = term->exportVtContent();
        if (vtData.isEmpty())
            continue;
        QFile vtFile(termDir.filePath("terminals/" + uuid + ".vt"));
        if (vtFile.open(QIODevice::WriteOnly))
            vtFile.write(vtData);
    }
}

void SessionManager::clearSnapshot() {
    QDir dir = sessionDir();
    if (!dir.exists())
        return;
    dir.remove("snapshot.json");
    QDir termDir = sessionDir().filePath("terminals");
    if (termDir.exists()) {
        QStringList vtFiles = termDir.entryList(QStringList("*.vt"), QDir::Files);
        for (const QString &f : vtFiles)
            termDir.remove(f);
    }
}

QString SessionManager::snapshotTimestamp() const {
    QFile file(sessionDir().filePath("snapshot.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return {};
    return doc.object()["timestamp"].toString();
}
```

Note: Need `#include <QStandardPaths>` in the .cpp file.

- [ ] **Step 3: Add test file tests/test_session_manager.cpp**

Test save/load round-trip using QTemporaryDir as the session directory. Since SessionManager uses a singleton with hardcoded path, we test the serialization logic indirectly through save+load with a temporary override, or test the data flow through the snapshot + VT file I/O.

For a practical test, create a helper that writes a snapshot + VT content to a temp dir and reads it back:

```cpp
#include <QTest>
#include <QTemporaryDir>
#include <QJsonDocument>

#include "SessionManager.h"

class TestSessionManager : public QObject {
    Q_OBJECT

private slots:
    void testSaveAndLoadSnapshot() {
        WindowSnapshot original;
        original.width = 1024;
        original.height = 768;
        original.tabs.append({1, "test", SplitNode::terminal("u1", "/home", "sh")});

        auto &mgr = SessionManager::instance();
        mgr.clearSnapshot();
        mgr.save(original, {});

        QVERIFY(mgr.hasSnapshot());
        WindowSnapshot loaded = mgr.loadSnapshot();
        QCOMPARE(loaded.width, 1024);
        QCOMPARE(loaded.height, 768);
        QCOMPARE(loaded.tabs.size(), 1);
        QCOMPARE(loaded.tabs[0].pane.uuid, "u1");

        mgr.clearSnapshot();
        QVERIFY(!mgr.hasSnapshot());
    }

    void testClearRemovesAll() {
        WindowSnapshot snap;
        snap.tabs.append({1, "t", SplitNode::terminal("uuid-clear-test", "/", "")});
        auto &mgr = SessionManager::instance();
        mgr.save(snap, {});
        QVERIFY(mgr.hasSnapshot());
        mgr.clearSnapshot();
        QVERIFY(!mgr.hasSnapshot());
    }
};

QTEST_MAIN(TestSessionManager)
#include "test_session_manager.moc"
```

- [ ] **Step 4: Add test to tests/CMakeLists.txt**

```cmake
add_executable(test_session_manager
  test_session_manager.cpp
  ${CMAKE_SOURCE_DIR}/src/app/SessionManager.cpp
  ${CMAKE_SOURCE_DIR}/src/app/SessionSnapshot.cpp
)

target_include_directories(test_session_manager PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/app
  ${GHOSTTY_STAGED_INCLUDE_DIR}
)

target_link_libraries(test_session_manager PRIVATE
  Qt6::Core
  Qt6::Test
)

add_test(NAME SessionManager COMMAND test_session_manager)
```

- [ ] **Step 5: Build and run test**

Run: `cmake -B build -DBUILD_TESTING=ON && cmake --build build && cd build && ctest -R SessionManager --output-on-failure`
Expected: All tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/app/SessionManager.h src/app/SessionManager.cpp tests/test_session_manager.cpp tests/CMakeLists.txt
git commit -m "feat: add SessionManager for saving and loading session state"
```

---

### Task 4: Add split tree introspection to TermPane

**Files:**
- Modify: `src/app/TermPane.h`
- Modify: `src/app/TermPane.cpp`

- [ ] **Step 1: Add declarations to TermPane.h**

Add after the existing public methods (around line 46, after `connectToRemoteServer`). Also add a private helper:

```cpp
#include "SessionSnapshot.h"

SplitNode buildSplitTree() const;
QList<QPair<QString, TerminalWidget *>> restoreFromSplitTree(const SplitNode &node);
```

Add to the private section (after `m_pendingPaneStructureChanged`):

```cpp
TerminalWidget *createTerminalWithUuid(const QString &uuid, const std::optional<PtySession::StartOptions> &options = std::nullopt);
QList<QPair<QString, TerminalWidget *>> rebuildTreeRecursive(const SplitNode &node, TerminalWidget *sibling);
```

- [ ] **Step 2: Implement buildSplitTree in TermPane.cpp**

Add a helper inside the existing anonymous namespace at the top of the file:

```cpp
SplitNode buildSplitTreeFromWidget(QWidget *widget) {
    if (auto *container = qobject_cast<TerminalScrollContainer *>(widget)) {
        auto *term = container->terminal();
        return SplitNode::terminal(
            term->property("paneId").toUuid().toString(QUuid::WithoutBraces),
            term->workingDirectory(),
            term->property("currentTitle").toString());
    }
    if (auto *term = qobject_cast<TerminalWidget *>(widget)) {
        return SplitNode::terminal(
            term->property("paneId").toUuid().toString(QUuid::WithoutBraces),
            term->workingDirectory(),
            term->property("currentTitle").toString());
    }
    if (auto *splitter = qobject_cast<QSplitter *>(widget)) {
        QList<SplitNode> children;
        for (int i = 0; i < splitter->count(); ++i)
            children.append(buildSplitTreeFromWidget(splitter->widget(i)));
        return SplitNode::split(splitter->orientation(), splitter->sizes(), children);
    }
    return SplitNode::terminal({}, {}, {});
}
```

Then add the public method:

```cpp
SplitNode TermPane::buildSplitTree() const {
    if (!m_rootWidget)
        return SplitNode::terminal({}, {}, {});
    return buildSplitTreeFromWidget(m_rootWidget);
}
```

- [ ] **Step 3: Implement createTerminalWithUuid**

This is the same as `createTerminal` but sets the saved UUID instead of generating a new one:

```cpp
TerminalWidget *TermPane::createTerminalWithUuid(const QString &uuid, const std::optional<PtySession::StartOptions> &options) {
    auto *container = new TerminalScrollContainer(this);
    auto *term = container->terminal();
    term->setContentsMargins(kTerminalContentPadding, kTerminalContentPadding, kTerminalContentPadding,
                             kTerminalContentPadding);

    term->setProperty("paneId", QUuid(uuid));
    if (options) {
        term->setStartOptions(*options);
    }
    term->initialize();

    auto *settings = AppSettings::instance();
    term->setTerminalFont(settings->terminalFont());
    term->setCursorShape(settings->cursorShape());
    term->setCursorBlinkEnabled(settings->cursorBlink());
    term->setScrollbackLines(settings->scrollbackLines());

    auto themes = ThemeLoader::loadThemes();
    QString scheme = settings->colorScheme();
    if (scheme == QStringLiteral("system")) {
        auto colorType = DGuiApplicationHelper::instance()->themeType();
        scheme = colorType == DGuiApplicationHelper::DarkType ? QStringLiteral("dark") : QStringLiteral("light");
    }
    term->applyTheme(ThemeLoader::findTheme(themes, scheme));

    term->installEventFilter(this);
    setupTerminalConnections(term);

    if (m_currentTerm)
        term->setOpacity(m_currentTerm->opacity());

    return term;
}
```

- [ ] **Step 4: Implement restoreFromSplitTree (recursive)**

This clears the existing pane, creates the first terminal from the tree root, then recursively rebuilds the entire split tree. Returns a list of (uuid, terminal) pairs so MainWindow can load VT content by saved UUID.

```cpp
QList<QPair<QString, TerminalWidget *>> TermPane::restoreFromSplitTree(const SplitNode &node) {
    QList<QPair<QString, TerminalWidget *>> result;

    if (node.type == SplitNode::Type::Terminal) {
        std::optional<PtySession::StartOptions> opts;
        if (!node.workingDirectory.isEmpty())
            opts = PtySession::StartOptions{{}, node.workingDirectory};
        auto *term = createTerminalWithUuid(node.uuid, opts);
        m_layout->addWidget(layoutWidgetForTerminal(term));
        m_rootWidget = layoutWidgetForTerminal(term);
        setCurrentTerminal(term);
        result.append({node.uuid, term});
        return result;
    }

    if (node.children.isEmpty())
        return result;

    auto firstChild = node.children.first();
    std::optional<PtySession::StartOptions> firstOpts;
    if (!firstChild.workingDirectory.isEmpty())
        firstOpts = PtySession::StartOptions{{}, firstChild.workingDirectory};
    auto *firstTerm = createTerminalWithUuid(firstChild.uuid, firstOpts);
    m_layout->addWidget(layoutWidgetForTerminal(firstTerm));
    m_rootWidget = layoutWidgetForTerminal(firstTerm);
    setCurrentTerminal(firstTerm);
    result.append({firstChild.uuid, firstTerm});

    for (int i = 1; i < node.children.size(); ++i) {
        const auto &child = node.children[i];
        result.append(rebuildTreeRecursive(child, firstTerm));
    }

    QWidget *root = m_rootWidget;
    if (auto *splitter = qobject_cast<QSplitter *>(root))
        splitter->setSizes(node.sizes);

    return result;
}

QList<QPair<QString, TerminalWidget *>> TermPane::rebuildTreeRecursive(const SplitNode &node, TerminalWidget *sibling) {
    QList<QPair<QString, TerminalWidget *>> result;

    if (node.type == SplitNode::Type::Terminal) {
        std::optional<PtySession::StartOptions> opts;
        if (!node.workingDirectory.isEmpty())
            opts = PtySession::StartOptions{{}, node.workingDirectory};
        auto *term = createTerminalWithUuid(node.uuid, opts);
        splitTerminal(sibling, term, qobject_cast<QSplitter *>(m_rootWidget->parentWidget())
                            ? qobject_cast<QSplitter *>(m_rootWidget->parentWidget())->orientation()
                            : Qt::Horizontal);
        result.append({node.uuid, term});
        return result;
    }

    if (node.children.isEmpty())
        return result;

    auto firstChild = node.children.first();
    std::optional<PtySession::StartOptions> firstOpts;
    if (!firstChild.workingDirectory.isEmpty())
        firstOpts = PtySession::StartOptions{{}, firstChild.workingDirectory};
    auto *firstTerm = createTerminalWithUuid(firstChild.uuid, firstOpts);
    splitTerminal(sibling, firstTerm, node.orientation);
    result.append({firstChild.uuid, firstTerm});

    for (int i = 1; i < node.children.size(); ++i)
        result.append(rebuildTreeRecursive(node.children[i], firstTerm));

    QWidget *termWidget = layoutWidgetForTerminal(firstTerm);
    if (auto *splitter = qobject_cast<QSplitter *>(termWidget->parentWidget()))
        splitter->setSizes(node.sizes);

    return result;
}
```

- [ ] **Step 5: Build and verify compilation**

Run: `cmake -B build -DBUILD_TESTING=ON && cmake --build build 2>&1 | tail -20`
Expected: Build succeeds.

- [ ] **Step 6: Commit**

```bash
git add src/app/TermPane.h src/app/TermPane.cpp
git commit -m "feat: add buildSplitTree and restoreFromSplitTree to TermPane"
```

---

### Task 5: Integrate session save into MainWindow closeEvent

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Add includes and method declaration**

In `MainWindow.cpp`, add at the top:

```cpp
#include "SessionManager.h"
#include "SessionSnapshot.h"
```

In `MainWindow.h`, add a private method:

```cpp
void saveSessionState();
```

- [ ] **Step 2: Implement saveSessionState**

Add to `MainWindow.cpp`:

```cpp
void MainWindow::saveSessionState() {
    if (m_tabs.isEmpty())
        return;

    WindowSnapshot snapshot;
    snapshot.width = width();
    snapshot.height = height();
    snapshot.isMaximized = isMaximized();

    QList<QPair<QString, TerminalWidget *>> allTerminals;

    for (const auto &rec : m_tabs) {
        if (!rec.pane)
            continue;
        TabSnapshot tabSnap;
        tabSnap.id = rec.id;
        tabSnap.title = rec.title;
        tabSnap.pane = rec.pane->buildSplitTree();
        snapshot.tabs.append(tabSnap);

        for (auto *term : rec.pane->findChildren<TerminalWidget *>()) {
            QString uuid = term->property("paneId").toUuid().toString(QUuid::WithoutBraces);
            if (!uuid.isEmpty())
                allTerminals.append({uuid, term});
        }
    }

    SessionManager::instance().save(snapshot, allTerminals);
}
```

- [ ] **Step 3: Call saveSessionState in closeEvent**

In `MainWindow::closeEvent`, add `saveSessionState()` right before the widget cleanup block (before `while (m_stackWidget->count() > 0)` at line 1174):

```cpp
saveSessionState();
```

This saves the session after the user has confirmed close but before widgets are destroyed.

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DBUILD_TESTING=ON && cmake --build build 2>&1 | tail -20`
Expected: Build succeeds.

- [ ] **Step 5: Manual smoke test**

Run: `./build/deepin-terminal-ghostty`
- Open multiple tabs with different content
- Optionally split some panes
- Close the window (confirm if asked)
- Verify `~/.config/deepin-terminal-ghostty/session/snapshot.json` exists and has correct content
- Verify `~/.config/deepin-terminal-ghostty/session/terminals/` has .vt files

- [ ] **Step 6: Commit**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "feat: save session state on window close"
```

---

### Task 6: Integrate session restore into MainWindow startup

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Add restoreSession declaration to MainWindow.h**

```cpp
void restoreSession();
```

- [ ] **Step 2: Implement restoreSession in MainWindow.cpp**

The restore calls `TermPane::restoreFromSplitTree()` which is a public API that returns `(uuid, terminal)` pairs with the saved UUIDs already set on each terminal. VT content is then loaded by those saved UUIDs.

```cpp
void MainWindow::restoreSession() {
    auto &mgr = SessionManager::instance();
    if (!mgr.hasSnapshot())
        return;

    WindowSnapshot snapshot = mgr.loadSnapshot();
    if (snapshot.tabs.isEmpty())
        return;

    if (snapshot.isMaximized)
        showMaximized();
    else if (snapshot.width > 0 && snapshot.height > 0)
        resize(snapshot.width, snapshot.height);

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
        m_tabBar->setCurrentIndex(0);
}
```

- [ ] **Step 3: Add restore dialog to MainWindow constructor**

In the constructor, after the `addTab()` call that creates the first default tab, add the restore dialog logic. Replace the current first tab creation logic:

In `MainWindow::MainWindow`, the current code at lines 150-158 does:
```cpp
ensureTabBar();
std::optional<PtySession::StartOptions> initialSessionOptions;
if (!m_startupOptions.execute.isEmpty() || !m_startupOptions.workingDirectory.isEmpty()) {
    initialSessionOptions = PtySession::StartOptions{...};
}
addTab(true, initialSessionOptions);
```

Change to:
```cpp
ensureTabBar();

bool restored = false;
if (m_startupOptions.execute.isEmpty() && m_startupOptions.workingDirectory.isEmpty()) {
    auto &mgr = SessionManager::instance();
    if (mgr.hasSnapshot()) {
        auto *dlg = new DDialog(this);
        dlg->setWindowTitle(tr("Restore Session"));
        dlg->setMessage(tr("A previous terminal session was found. Restore it?"));
        dlg->addButton(tr("New Terminal"), false, DDialog::ButtonNormal);
        dlg->addButton(tr("Restore Session"), true, DDialog::ButtonRecommended);
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

if (!restored) {
    std::optional<PtySession::StartOptions> initialSessionOptions;
    if (!m_startupOptions.execute.isEmpty() || !m_startupOptions.workingDirectory.isEmpty()) {
        initialSessionOptions = PtySession::StartOptions{
            .command = m_startupOptions.execute,
            .workingDirectory = m_startupOptions.workingDirectory,
        };
    }
    addTab(true, initialSessionOptions);
}
```

- [ ] **Step 4: Add includes for SessionManager**

Ensure `MainWindow.cpp` has:
```cpp
#include "SessionManager.h"
#include "SessionSnapshot.h"
```

Also need `<DDialog>` which is already included.

- [ ] **Step 5: Build and verify**

Run: `cmake -B build -DBUILD_TESTING=ON && cmake --build build 2>&1 | tail -20`
Expected: Build succeeds.

- [ ] **Step 6: End-to-end manual test**

1. Run the terminal, create some tabs and content
2. Close the window
3. Run the terminal again
4. Verify the restore dialog appears
5. Click "Restore Session" — verify tabs, splits, and content are restored
6. Close and run again
7. Click "New Terminal" — verify it starts fresh and snapshot is cleared

- [ ] **Step 7: Commit**

```bash
git add src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "feat: restore session on startup with user confirmation dialog"
```

---

### Task 7: Update CMakeLists.txt and test build

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Add new source files to the main CMakeLists.txt**

In `CMakeLists.txt`, add to the `deepin-terminal-ghostty` executable sources (after `src/app/TermPane.cpp`):

```cmake
src/app/SessionSnapshot.cpp
src/app/SessionManager.cpp
```

- [ ] **Step 2: Rebuild and run all tests**

Run: `cmake -B build -DBUILD_TESTING=ON && cmake --build build && cd build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure`
Expected: All tests pass.

- [ ] **Step 3: Run clang-format**

Run: `clang-format -i $(find src tests -name '*.cpp' -o -name '*.h')`

- [ ] **Step 4: Verify format**

Run: `clang-format --dry-run --Werror $(find src tests -name '*.cpp' -o -name '*.h')`
Expected: No output (all files formatted).

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt tests/CMakeLists.txt
git commit -m "chore: wire SessionManager into build system"
```
