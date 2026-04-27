# Command Status Indicator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Track per-command exit codes via shell integration and display colored status dots (yellow=running, green=success, red=failure) on inactive tabs/panes in the vertical sidebar.

**Architecture:** Extend the existing OSC 777 shell integration channel with a new `ShellCommandResult` sequence to report exit codes. Introduce a `CommandState` enum on `TerminalWidget`, propagate it through the signal chain to `VerticalTabSidebar`, and render a small colored dot on each tab/pane row.

**Tech Stack:** C++20, Qt6 Widgets, DTK6, bash/zsh shell integration via OSC sequences.

**Design doc:** `docs/superpowers/specs/2026-04-27-command-status-indicator-design.md`

---

## File Structure

| File | Responsibility |
|------|----------------|
| `src/libqtghostty/TerminalWidget.h` | `CommandState` enum declaration, new signal |
| `src/libqtghostty/TerminalWidget.cpp` | OSC `ShellCommandResult` parsing, state machine, `commandStateChanged` signal |
| `src/libqtghostty/PtySession.cpp` | Shell integration prelude: add `__deepin_terminal_ghostty_report_result`, update bash PROMPT_COMMAND and zsh precmd |
| `src/app/TermPane.h` | Add `commandState` field to `PaneInfo`, add `paneCommandStateChanged` signal |
| `src/app/TermPane.cpp` | Connect `commandStateChanged`, auto-clear on focus, populate `commandState` in `paneInfos()` |
| `src/app/MainWindow.cpp` | Connect `paneCommandStateChanged` → sidebar refresh |
| `src/app/VerticalTabSidebar.cpp` | Create and render status dot widgets in `rebuild()` |
| `tests/test_terminal_widget.cpp` | Test exit code parsing and state transitions |

---

### Task 1: Add `CommandState` enum and `commandStateChanged` signal to TerminalWidget

**Files:**
- Modify: `src/libqtghostty/TerminalWidget.h:20-22` (after Q_OBJECT, before `public:`)
- Modify: `src/libqtghostty/TerminalWidget.h:65-70` (signals section)
- Modify: `src/libqtghostty/TerminalWidget.h:145-147` (private members section)

- [ ] **Step 1: Add `CommandState` enum before the `TerminalWidget` class definition**

In `src/libqtghostty/TerminalWidget.h`, add the following **before** `class TerminalWidget` (after the `#include <ghostty/vt.h>` line):

```cpp
enum class CommandState { Idle = 0, Running = 1, Succeeded = 2, Failed = 3 };
```

- [ ] **Step 2: Add `commandStateChanged` signal to the signals section**

In `src/libqtghostty/TerminalWidget.h`, in the `signals:` block (after `void focusGained();` at line 70), add:

```cpp
    void commandStateChanged(CommandState state);
```

- [ ] **Step 3: Add private members for state tracking**

In `src/libqtghostty/TerminalWidget.h`, in the `private:` section (after `QByteArray m_oscScanBuffer;` at line 146), add:

```cpp
    int m_pendingExitCode = -1;
    CommandState m_commandState = CommandState::Idle;
```

- [ ] **Step 4: Add private method declarations**

In `src/libqtghostty/TerminalWidget.h`, after `void setShellCommand(const QString &command);` (line 116), add:

```cpp
    void setShellCommandResult(int exitCode);
    void updateCommandState(CommandState newState);
```

- [ ] **Step 5: Build to verify header compiles**

Run: `cmake --build build 2>&1 | head -30`
Expected: Build errors in TerminalWidget.cpp (the new methods are declared but not defined yet). That's OK — we'll implement them in Task 2.

- [ ] **Step 6: Commit**

```bash
git add src/libqtghostty/TerminalWidget.h
git commit -m "feat: add CommandState enum and commandStateChanged signal"
```

---

### Task 2: Implement OSC `ShellCommandResult` parsing and state machine in TerminalWidget

**Files:**
- Modify: `src/libqtghostty/TerminalWidget.cpp:123-149` (OSC parsing functions)
- Modify: `src/libqtghostty/TerminalWidget.cpp:1343-1382` (scan and setShellCommand)

- [ ] **Step 1: Add `shellCommandResultFromOscPayload` parsing function**

In `src/libqtghostty/TerminalWidget.cpp`, in the anonymous namespace, after `commandFromQtGhosttyShellCommand()` (line 129), add:

```cpp
std::optional<int> commandResultFromQtGhosttyShellCommandResult(const QByteArray &payload) {
    static constexpr QByteArrayView kPrefix("777;ShellCommandResult=");
    if (!payload.startsWith(kPrefix))
        return std::nullopt;

    const QByteArray value = payload.mid(kPrefix.size());
    bool ok = false;
    const int exitCode = value.toInt(&ok);
    if (!ok)
        return std::nullopt;
    return exitCode;
}
```

- [ ] **Step 2: Modify `scanShellIntegrationSequences()` to also parse result sequences**

Replace the current body of `scanShellIntegrationSequences()` (lines 1343-1373) with:

```cpp
void TerminalWidget::scanShellIntegrationSequences(const QByteArray &data) {
    m_oscScanBuffer.append(data);
    if (m_oscScanBuffer.size() > kMaxOscScanBufferBytes)
        m_oscScanBuffer = m_oscScanBuffer.right(kMaxOscScanBufferBytes);

    while (true) {
        const int oscStart = m_oscScanBuffer.indexOf("\033]");
        if (oscStart < 0) {
            m_oscScanBuffer = m_oscScanBuffer.endsWith('\033') ? QByteArray("\033") : QByteArray();
            return;
        }

        if (oscStart > 0)
            m_oscScanBuffer.remove(0, oscStart);

        const int belEnd = m_oscScanBuffer.indexOf('\a', 2);
        const int stEnd = m_oscScanBuffer.indexOf("\033\\", 2);
        if (belEnd < 0 && stEnd < 0)
            return;

        const bool useBel = belEnd >= 0 && (stEnd < 0 || belEnd < stEnd);
        const int payloadEnd = useBel ? belEnd : stEnd;
        const int sequenceEnd = useBel ? belEnd + 1 : stEnd + 2;
        const QByteArray payload = m_oscScanBuffer.mid(2, payloadEnd - 2);

        const std::optional<int> result = commandResultFromQtGhosttyShellCommandResult(payload);
        if (result.has_value()) {
            setShellCommandResult(result.value());
        } else {
            const std::optional<QString> command = shellCommandFromOscPayload(payload);
            if (command.has_value())
                setShellCommand(command.value());
        }

        m_oscScanBuffer.remove(0, sequenceEnd);
    }
}
```

- [ ] **Step 3: Implement `setShellCommandResult()`**

Add after `setShellCommand()` in `src/libqtghostty/TerminalWidget.cpp`:

```cpp
void TerminalWidget::setShellCommandResult(int exitCode) {
    m_pendingExitCode = exitCode;
}
```

- [ ] **Step 4: Modify `setShellCommand()` to implement the state machine**

Replace the existing `setShellCommand()` implementation (lines 1375-1382) with:

```cpp
void TerminalWidget::setShellCommand(const QString &command) {
    const QString trimmed = command.trimmed();
    if (property("shellCommand").toString() == trimmed)
        return;

    setProperty("shellCommand", trimmed);

    if (!trimmed.isEmpty()) {
        m_pendingExitCode = -1;
        updateCommandState(CommandState::Running);
    } else {
        if (m_pendingExitCode >= 0) {
            updateCommandState(m_pendingExitCode == 0 ? CommandState::Succeeded : CommandState::Failed);
            m_pendingExitCode = -1;
        } else {
            updateCommandState(CommandState::Idle);
        }
    }

    Q_EMIT shellCommandChanged(trimmed);
}
```

- [ ] **Step 5: Implement `updateCommandState()`**

Add after `setShellCommandResult()`:

```cpp
void TerminalWidget::updateCommandState(CommandState newState) {
    if (m_commandState == newState)
        return;

    m_commandState = newState;
    setProperty("commandState", static_cast<int>(newState));
    Q_EMIT commandStateChanged(newState);
}
```

- [ ] **Step 6: Build and verify**

Run: `cmake --build build 2>&1 | tail -5`
Expected: Build succeeds with no errors.

- [ ] **Step 7: Run existing tests**

Run: `cd build && ctest --output-on-failure`
Expected: All existing tests pass.

- [ ] **Step 8: Commit**

```bash
git add src/libqtghostty/TerminalWidget.cpp
git commit -m "feat: parse ShellCommandResult OSC and implement command state machine"
```

---

### Task 3: Add tests for command state transitions

**Files:**
- Modify: `tests/test_terminal_widget.cpp`

- [ ] **Step 1: Add test declarations**

In `tests/test_terminal_widget.cpp`, in the `TestTerminalWidget` class (after line 62 `void testApplyThemeSetsColors();`), add:

```cpp
    void testCommandStateRunning();
    void testCommandStateSucceeded();
    void testCommandStateFailed();
    void testCommandStateIdleWhenNoResult();
    void testCommandStateTransitionSequence();
```

- [ ] **Step 2: Add test for Running state**

At the end of the file (before the `QTEST_MAIN` or `QTEST_APPLESS_MAIN` line), add:

```cpp
void TestTerminalWidget::testCommandStateRunning() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    QSignalSpy spy(&widget, &TerminalWidget::commandStateChanged);
    QVERIFY(spy.isValid());

    const QByteArray command = QByteArray("\033]777;ShellCommand=") + QByteArray("bWFrZQ==").append("\033\\");
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, command));

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 100);
    QCOMPARE(widget.property("commandState").toInt(), static_cast<int>(CommandState::Running));
}
```

- [ ] **Step 3: Add test for Succeeded state**

```cpp
void TestTerminalWidget::testCommandStateSucceeded() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    QSignalSpy spy(&widget, &TerminalWidget::commandStateChanged);
    QVERIFY(spy.isValid());

    const QByteArray command = QByteArray("\033]777;ShellCommand=") + QByteArray("bWFrZQ==").append("\033\\");
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, command));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 100);

    const QByteArray result = "\033]777;ShellCommandResult=0\033\\";
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, result));

    const QByteArray clear = "\033]777;ShellCommand=\033\\";
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, clear));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 3, 100);

    QCOMPARE(widget.property("commandState").toInt(), static_cast<int>(CommandState::Succeeded));
}
```

- [ ] **Step 4: Add test for Failed state**

```cpp
void TestTerminalWidget::testCommandStateFailed() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    QSignalSpy spy(&widget, &TerminalWidget::commandStateChanged);
    QVERIFY(spy.isValid());

    const QByteArray command = QByteArray("\033]777;ShellCommand=") + QByteArray("bWFrZQ==").append("\033\\");
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, command));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 100);

    const QByteArray result = "\033]777;ShellCommandResult=2\033\\";
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, result));

    const QByteArray clear = "\033]777;ShellCommand=\033\\";
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, clear));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 3, 100);

    QCOMPARE(widget.property("commandState").toInt(), static_cast<int>(CommandState::Failed));
}
```

- [ ] **Step 5: Add test for Idle when no result sequence**

```cpp
void TestTerminalWidget::testCommandStateIdleWhenNoResult() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    QSignalSpy spy(&widget, &TerminalWidget::commandStateChanged);
    QVERIFY(spy.isValid());

    const QByteArray clear = "\033]777;ShellCommand=\033\\";
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, clear));

    QCOMPARE(widget.property("commandState").toInt(), static_cast<int>(CommandState::Idle));
}
```

- [ ] **Step 6: Add test for full transition sequence (Running → Succeeded → Running → Failed)**

```cpp
void TestTerminalWidget::testCommandStateTransitionSequence() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    QSignalSpy spy(&widget, &widget.commandStateChanged);
    QVERIFY(spy.isValid());

    auto sendOsc = [&widget](const QByteArray &osc) {
        QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, osc));
    };

    sendOsc(QByteArray("\033]777;ShellCommand=") + QByteArray("bWFrZQ==").append("\033\\"));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 100);
    QCOMPARE(widget.property("commandState").toInt(), static_cast<int>(CommandState::Running));

    sendOsc("\033]777;ShellCommandResult=0\033\\");
    sendOsc("\033]777;ShellCommand=\033\\");
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 3, 100);
    QCOMPARE(widget.property("commandState").toInt(), static_cast<int>(CommandState::Succeeded));

    sendOsc(QByteArray("\033]777;ShellCommand=") + QByteArray("bWFrZSBhbGw=").append("\033\\"));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 4, 100);
    QCOMPARE(widget.property("commandState").toInt(), static_cast<int>(CommandState::Running));

    sendOsc("\033]777;ShellCommandResult=1\033\\");
    sendOsc("\033]777;ShellCommand=\033\\");
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 6, 100);
    QCOMPARE(widget.property("commandState").toInt(), static_cast<int>(CommandState::Failed));
}
```

- [ ] **Step 7: Build and run the new tests**

Run: `cmake -B build -DBUILD_TESTING=ON && cmake --build build && cd build && ctest --output-on-failure -R test_terminal_widget`
Expected: All tests pass, including the 5 new command state tests.

- [ ] **Step 8: Commit**

```bash
git add tests/test_terminal_widget.cpp
git commit -m "test: add tests for command state machine transitions"
```

---

### Task 4: Add exit code reporting to shell integration scripts

**Files:**
- Modify: `src/libqtghostty/PtySession.cpp:96-148` (prelude and bash integration)
- Modify: `src/libqtghostty/PtySession.cpp:151-178` (zsh integration)

- [ ] **Step 1: Add `__deepin_terminal_ghostty_report_result` to the prelude**

In `src/libqtghostty/PtySession.cpp`, replace the `shellIntegrationPrelude()` function (lines 96-105) with:

```cpp
QByteArray shellIntegrationPrelude() {
    return R"SH(
__deepin_terminal_ghostty_emit_command() {
    printf '\033]777;ShellCommand=%s\033\\' "$(printf '%s' "$1" | base64 | tr -d '\n')"
}
__deepin_terminal_ghostty_clear_command() {
    printf '\033]777;ShellCommand=\033\\'
}
__deepin_terminal_ghostty_report_result() {
    printf '\033]777;ShellCommandResult=%s\033\\' "$?"
}
)SH";
}
```

- [ ] **Step 2: Update bash PROMPT_COMMAND to report result before clearing**

In `src/libqtghostty/PtySession.cpp`, change the bash `PROMPT_COMMAND` line (line 139) from:

```bash
PROMPT_COMMAND="__deepin_terminal_ghostty_clear_command${PROMPT_COMMAND:+;$PROMPT_COMMAND}"
```

to:

```bash
PROMPT_COMMAND="__deepin_terminal_ghostty_report_result;__deepin_terminal_ghostty_clear_command${PROMPT_COMMAND:+;$PROMPT_COMMAND}"
```

- [ ] **Step 3: Update zsh precmd to report result before clearing**

In `src/libqtghostty/PtySession.cpp`, change the zsh `__deepin_terminal_ghostty_precmd` function (lines 166-168) from:

```bash
__deepin_terminal_ghostty_precmd() {
    emulate -L zsh
    __deepin_terminal_ghostty_clear_command
}
```

to:

```bash
__deepin_terminal_ghostty_precmd() {
    emulate -L zsh
    __deepin_terminal_ghostty_report_result
    __deepin_terminal_ghostty_clear_command
}
```

- [ ] **Step 4: Build**

Run: `cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 5: Run all tests**

Run: `cd build && ctest --output-on-failure`
Expected: All tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/libqtghostty/PtySession.cpp
git commit -m "feat: report command exit code via ShellCommandResult OSC in shell integration"
```

---

### Task 5: Extend `PaneInfo` with `commandState` and propagate through signal chain

**Files:**
- Modify: `src/app/TermPane.h:21-26` (PaneInfo struct)
- Modify: `src/app/TermPane.h:49-57` (signals section)
- Modify: `src/app/TermPane.cpp:182-195` (paneInfos)
- Modify: `src/app/TermPane.cpp:259-279` (setupTerminalConnections)

- [ ] **Step 1: Add `commandState` to `PaneInfo` and include TerminalWidget.h**

In `src/app/TermPane.h`, add an include for `TerminalWidget.h` (after the existing includes), and add `commandState` field to `PaneInfo`:

```cpp
struct PaneInfo {
    QUuid id;
    QString title;
    QString iconName;
    bool isActive = false;
    CommandState commandState = CommandState::Idle;
};
```

Note: `CommandState` is declared in `TerminalWidget.h` which is already indirectly included via `PtySession.h`, but we should include it explicitly.

- [ ] **Step 2: Add `paneCommandStateChanged` signal to TermPane**

In `src/app/TermPane.h`, in the `signals:` section (after `void paneTitleChanged`), add:

```cpp
    void paneCommandStateChanged(const QUuid &paneId, CommandState state);
```

- [ ] **Step 3: Populate `commandState` in `paneInfos()`**

In `src/app/TermPane.cpp`, in `paneInfos()` (around line 191), add after `info.isActive = ...`:

```cpp
        info.commandState = static_cast<CommandState>(term->property("commandState").toInt());
```

- [ ] **Step 4: Connect `commandStateChanged` in `setupTerminalConnections()`**

In `src/app/TermPane.cpp`, in `setupTerminalConnections()` (after the `shellCommandChanged` connect at line 275-276), add:

```cpp
    connect(term, &TerminalWidget::commandStateChanged, this,
            [this, term](CommandState state) { Q_EMIT paneCommandStateChanged(ensurePaneId(term), state); });
```

- [ ] **Step 5: Add auto-clear on focus gain**

In `src/app/TermPane.cpp`, in `setupTerminalConnections()`, modify the existing `focusGained` connection (line 278) from:

```cpp
    connect(term, &TerminalWidget::focusGained, this, [this, term]() { setCurrentTerminal(term); });
```

to:

```cpp
    connect(term, &TerminalWidget::focusGained, this, [this, term]() {
        setCurrentTerminal(term);
        const auto state = static_cast<CommandState>(term->property("commandState").toInt());
        if (state == CommandState::Succeeded || state == CommandState::Failed) {
            QMetaObject::invokeMethod(term, "updateCommandState", Qt::DirectConnection,
                                      Q_ARG(CommandState, CommandState::Idle));
        }
    });
```

Note: `updateCommandState` is private. We need an alternative approach. Instead, add a public method `clearCommandState()` to `TerminalWidget`, or directly set the property and emit the signal from TermPane. Since `TermPane` is a friend class of `TerminalWidget` (see TerminalWidget.h line 214), it can access private members. Use:

```cpp
    connect(term, &TerminalWidget::focusGained, this, [this, term]() {
        setCurrentTerminal(term);
        if (term->m_commandState == CommandState::Succeeded || term->m_commandState == CommandState::Failed)
            term->updateCommandState(CommandState::Idle);
    });
```

This works because `TermPane` is declared as a friend of `TerminalWidget`.

- [ ] **Step 6: Build**

Run: `cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 7: Commit**

```bash
git add src/app/TermPane.h src/app/TermPane.cpp
git commit -m "feat: propagate CommandState through PaneInfo and auto-clear on focus"
```

---

### Task 6: Connect `paneCommandStateChanged` in MainWindow

**Files:**
- Modify: `src/app/MainWindow.cpp:347-351` (after the paneTitleChanged connection)

- [ ] **Step 1: Add connection for `paneCommandStateChanged`**

In `src/app/MainWindow.cpp`, in the `addTab()` method, after the `paneTitleChanged` connection (line 347-351), add:

```cpp
    connect(pane, &TermPane::paneCommandStateChanged, this, [this, pane](const QUuid &, CommandState) {
        if (auto *record = tabRecordForPane(pane))
            refreshTabRecord(*record);
        syncTabWidgetsFromRecords();
    });
```

- [ ] **Step 2: Build**

Run: `cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add src/app/MainWindow.cpp
git commit -m "feat: connect paneCommandStateChanged to sidebar refresh in MainWindow"
```

---

### Task 7: Render status dots in VerticalTabSidebar

**Files:**
- Modify: `src/app/VerticalTabSidebar.cpp:24-103` (add helper function)
- Modify: `src/app/VerticalTabSidebar.cpp:293-414` (rebuild method)
- Modify: `src/app/VerticalTabSidebar.cpp:142-264` (stylesheet — add dot styles)

- [ ] **Step 1: Add helper function and constants for the status dot**

In `src/app/VerticalTabSidebar.cpp`, in the anonymous namespace, after the `kProcessIconSize` constant (line 27), add:

```cpp
constexpr int kStatusDotSize = 8;
```

After `createProcessBadge()` (after line 103), add:

```cpp
QLabel *createCommandStatusDot(QWidget *parent, CommandState state, bool isActive) {
    auto *dot = new QLabel(parent);
    dot->setObjectName(QStringLiteral("commandStatusDot"));
    dot->setFixedSize(kStatusDotSize, kStatusDotSize);

    if (isActive || state == CommandState::Idle) {
        dot->setVisible(false);
        return dot;
    }

    const auto *helper = DGuiApplicationHelper::instance();
    const bool isDark = helper->themeType() == DGuiApplicationHelper::DarkType;

    QColor color;
    switch (state) {
    case CommandState::Running:
        color = isDark ? QColor(255, 184, 0) : QColor(210, 150, 0);
        break;
    case CommandState::Succeeded:
        color = isDark ? QColor(46, 213, 115) : QColor(34, 170, 91);
        break;
    case CommandState::Failed:
        color = isDark ? QColor(255, 71, 87) : QColor(210, 55, 70);
        break;
    default:
        dot->setVisible(false);
        return dot;
    }

    QPixmap pixmap(kStatusDotSize * dot->devicePixelRatioF(), kStatusDotSize * dot->devicePixelRatioF());
    pixmap.setDevicePixelRatio(dot->devicePixelRatioF());
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(0, 0, kStatusDotSize, kStatusDotSize);
    painter.end();

    dot->setPixmap(pixmap);
    return dot;
}
```

- [ ] **Step 2: Add status dot to tab header row in `rebuild()`**

In `VerticalTabSidebar::rebuild()`, after creating `tabBadge` (lines 338-342) and before creating `tabButton` (line 344), add the status dot creation:

```cpp
        CommandState tabCommandState = CommandState::Idle;
        if (paneCount == 1)
            tabCommandState = tab.panes.first().commandState;
        auto *tabStatusDot = createCommandStatusDot(header, tabCommandState, tab.isCurrent);
        if (isMultiPane)
            tabStatusDot->setVisible(false);
```

Then in the header layout section (after line 353 `headerLayout->addWidget(tabButton, 1);`), add:

```cpp
        headerLayout->addWidget(tabStatusDot, 0, Qt::AlignVCenter);
```

- [ ] **Step 3: Add status dot to each pane row in `rebuild()`**

In the pane row section, after creating `paneBadge` (lines 387-389) and `paneButton`, and before `paneRowLayout->addWidget(paneButton, 1);` (line 400), add:

```cpp
                auto *paneStatusDot = createCommandStatusDot(paneRow, pane.commandState, pane.isActive);
```

Then after `paneRowLayout->addWidget(paneButton, 1);`, add:

```cpp
                paneRowLayout->addWidget(paneStatusDot, 0, Qt::AlignVCenter);
```

- [ ] **Step 4: Build and verify**

Run: `cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 5: Run all tests**

Run: `cd build && ctest --output-on-failure`
Expected: All tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/app/VerticalTabSidebar.cpp
git commit -m "feat: render command status dots in vertical tab sidebar"
```

---

### Task 8: Format and final verification

**Files:**
- All modified files

- [ ] **Step 1: Run clang-format on all changed files**

Run: `clang-format -i $(find src tests -name '*.cpp' -o -name '*.h')`

- [ ] **Step 2: Verify clang-format passes**

Run: `clang-format --dry-run --Werror $(find src tests -name '*.cpp' -o -name '*.h')`
Expected: No output (all files pass formatting).

- [ ] **Step 3: Full build and test**

Run: `cmake -B build -DBUILD_TESTING=ON && cmake --build build && cd build && ctest --output-on-failure`
Expected: All builds and tests pass.

- [ ] **Step 4: Amend last commit or create a new one with formatting fixes**

If clang-format changed any files:

```bash
git add -u
git commit -m "style: apply clang-format to all source files"
```
