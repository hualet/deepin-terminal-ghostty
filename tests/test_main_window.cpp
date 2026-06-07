#include "AppSettings.h"
#include "ApplicationMetadata.h"
#include "MainWindow.h"
#include "PageSearchBar.h"
#include "PtySession.h"
#include "QuakeWindow.h"
#include "SessionManager.h"
#include "SessionSnapshot.h"
#include "SettingsDialog.h"
#include "StartupOptions.h"
#include "TermPane.h"
#include "TerminalControlService.h"
#include "TerminalScrollContainer.h"
#include "TerminalWidget.h"
#include "ThemeLoader.h"
#include "VerticalTabSidebar.h"
#include "logging/Logging.h"
#include "remote/RemoteManagementPanel.h"
#include "remote/ServerConfigOptDlg.h"

#include <DApplication>
#include <DGuiApplicationHelper>
#include <DSettings>
#include <DTabBar>
#include <DTitlebar>
#include <QAbstractButton>
#include <QAccessible>
#include <QAction>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

DWIDGET_USE_NAMESPACE

namespace {
void clearVerticalTabsSetting();
}

class TestMainWindow : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        clearVerticalTabsSetting();
        m_originalPaletteType = DGuiApplicationHelper::instance()->paletteType();
    }

    void cleanup() {
        AppSettings::releaseInstance();
        clearVerticalTabsSetting();
        DGuiApplicationHelper::instance()->setPaletteType(m_originalPaletteType);
    }

    void testSingleTabCtrlDClosesWindow();
    void testClosedSessionRemovesOnlyCurrentTab();
    void testAltArrowWithKeypadModifierIsConsumedByPane();
    void testTermPaneReportsPaneSnapshotsAfterSplit();
    void testSplitSuppressesIntermediateRepaintWhileReparenting();
    void testCloseSplitSuppressesIntermediateRepaintWhileReparenting();
    void testNestedSplitDoesNotRepaintUnchangedSiblingTerminal();
    void testClosingCurrentSplitFocusesSiblingTerminal();
    void testClosingNestedSplitPreservesParentSplitterSizes();
    void testClosingRepeatedSameDirectionSplitsPreservesSiblings();
    void testCloseOtherTerminalsPublishesSingleStructureChange();
    void testTermPaneReportsProcessIconNames();
    void testAppTerminalsSetTerminalContentMargins();
    void testTermPaneWrapsTerminalWithFloatingScrollBar();
    void testScrollBarPositionDoesNotFollowBottomAfterOutput();
    void testVerticalTabsActionReflectsAndUpdatesSettings();
    void testVerticalTabsActionTracksExternalSettingChanges();
    void testVerticalTabsActionReflectsStartupSetting();
    void testVerticalSidebarShowsTabsAndPanes();
    void testActivePaneTitleUpdatesTabAndWindowTitles();
    void testSidebarExpansionSurvivesModeSwitch();
    void testHorizontalTitlebarTabsSurviveModeSwitch();
    void testHorizontalTitlebarTabsDoNotCoverMenuButton();
    void testVerticalSidebarTabClickSwitchesCurrentTab();
    void testVerticalSidebarTabClickRecoversFromStaleTabData();
    void testVerticalSidebarDragReordersTabs();
    void testVerticalSidebarIncludesDecorativeHierarchyElements();
    void testVerticalSidebarElidesLabelsWhenNarrow();
    void testCoreControlsExposeAccessibleLabels();
    void testVerticalSidebarExposesAccessibleLabels();
    void testSearchBarExposesAccessibleLabels();
    void testSettingsDialogExposesAccessibleLabels();
    void testShortcutViewerLaunchesFromDisplayShortcut();
    void testVerticalSidebarAccessibleLabelsTrackTitlesAndExpansion();
    void testRemoteManagementPanelExposesAccessibleLabels();
    void testServerConfigDialogExposesAccessibleLabels();
    void testAccessibleSearchControlsDriveFindActions();
    void testAccessibleVerticalSidebarButtonsActivateTargets();
    void testAccessibleRemoteAddButtonOpensConfigDialog();
    void testShortcutViewerPayloadListsConfiguredActions();
    void testProcessIconsAreAvailable();
    void testTerminalProcessBadgeHasVisibleColoredArtwork();
    void testLoggingCategoriesExposeExpectedNames();
    void testApplicationMetadataIsConfigured();
    void testStartupSessionFinishedEmitsExitCode();
    void testThemeLoaderLoadsAllThemes();
    void testThemeLoaderFindsThemeByName();
    void testThemeSettingDefaultIsSystem();
    void testThemeChangeAppliesToAllTerminals();
    void testThemeMenuHoverPreviewsAndRestoresTheme();
    void testNextTabShortcutSwitchesInVerticalMode();
    void testPrevTabShortcutSwitchesInVerticalMode();
    void testGotoTabShortcutSwitchesInVerticalMode();
    void testVerticalSidebarAddTabButtonCreatesNewTab();
    void testCommandStatusDotNotShownAfterSwitchingFromTabWhereCommandRan();
    void testCommandStatusDotShownWhenCommandFinishesInBackgroundTab();
    void testCommandStatusDotNotShownForInactivePaneInCurrentTab();
    void testPaneActivationDoesNotClearCommandState();
    void testQuakeWindowUsesTopScreenGeometry();
    void testQuakeWindowPresentationFlags();
    void testQuakeWindowShowAndHideUseTargetGeometry();
    void testQuakeWindowFocusLossHideHonorsSetting();
    void testManualBehaviorOpensBlankWindow();
    void testRestoreSessionMenuActionDisabledWithoutSnapshot();
    void testRestoreSessionMenuActionEnabledWithSnapshot();
    void testControlServiceListsWindowTabsPanesAndContent();
    void testControlServiceCreatesTabAndSplit();
    void testControlServiceSendsTextAndExecutesCommand();

private:
    DGuiApplicationHelper::ColorType m_originalPaletteType;
};

namespace {

QString settingsStorePath() {
    return QString("%1/%2/%3.conf")
        .arg(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation), "deepin", "deepin-terminal-ghostty");
}

void clearVerticalTabsSetting() {
    QFile::remove(settingsStorePath());
}

class ExposedTermPane : public TermPane {
public:
    using TermPane::eventFilter;
    using TermPane::TermPane;
};

TerminalWidget *currentTerminal(MainWindow &window) {
    auto *stack = window.findChild<QStackedWidget *>();
    if (!stack)
        return nullptr;
    auto *pane = qobject_cast<TermPane *>(stack->currentWidget());
    if (!pane)
        return nullptr;
    return pane->currentTerminal();
}

TermPane *currentPane(MainWindow &window) {
    auto *stack = window.findChild<QStackedWidget *>();
    if (!stack)
        return nullptr;
    return qobject_cast<TermPane *>(stack->currentWidget());
}

DTabBar *tabBar(MainWindow &window) {
    return window.findChild<DTabBar *>();
}

VerticalTabSidebar *sidebar(MainWindow &window) {
    return window.findChild<VerticalTabSidebar *>(QStringLiteral("verticalTabSidebar"));
}

QWidget *verticalTabSection(VerticalTabSidebar *sidebar, int tabId) {
    if (!sidebar)
        return nullptr;
    for (auto *section : sidebar->findChildren<QWidget *>(QStringLiteral("verticalTabSection"))) {
        if (section->property("tabId").toInt() == tabId)
            return section;
    }
    return nullptr;
}

DTitlebar *titlebar(MainWindow &window) {
    return window.findChild<DTitlebar *>();
}

QAction *findMenuActionByText(QMenu *menu, const QString &text) {
    if (!menu)
        return nullptr;
    for (auto *action : menu->actions()) {
        if (action->text() == text)
            return action;
    }
    return nullptr;
}

bool waitForTabCount(DTabBar *tabs, int expectedCount, int timeoutMs = 5000) {
    if (!tabs)
        return false;

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (tabs->count() == expectedCount)
            return true;
        QTest::qWait(50);
    }

    return tabs->count() == expectedCount;
}

PtySession *ptySession(TerminalWidget *terminal) {
    if (!terminal)
        return nullptr;
    return terminal->findChild<PtySession *>();
}

QString accessibleText(QObject *object, QAccessible::Text textType) {
    auto *iface = QAccessible::queryAccessibleInterface(object);
    if (!iface)
        return {};
    return iface->text(textType);
}

QAccessible::Role accessibleRole(QObject *object) {
    auto *iface = QAccessible::queryAccessibleInterface(object);
    if (!iface)
        return QAccessible::NoRole;
    return iface->role();
}

struct ShortcutViewerCapture {
    QTemporaryDir dir;
    QByteArray previousPath;
    QByteArray previousOutputPath;
    QString outputPath;
    bool installed = false;

    ~ShortcutViewerCapture() { restore(); }

    bool install() {
        if (!dir.isValid())
            return false;

        outputPath = dir.filePath(QStringLiteral("viewer-args.txt"));
        const QString scriptPath = dir.filePath(QStringLiteral("deepin-shortcut-viewer"));
        QFile script(scriptPath);
        if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        script.write("#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$DEEPIN_SHORTCUT_VIEWER_ARGS\"\n");
        script.close();
        if (!script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner))
            return false;

        previousPath = qgetenv("PATH");
        previousOutputPath = qgetenv("DEEPIN_SHORTCUT_VIEWER_ARGS");
        qputenv("PATH", QFile::encodeName(dir.path()) + ":" + previousPath);
        qputenv("DEEPIN_SHORTCUT_VIEWER_ARGS", QFile::encodeName(outputPath));
        installed = true;
        return true;
    }

    void restore() {
        if (!installed)
            return;
        qputenv("PATH", previousPath);
        if (previousOutputPath.isEmpty())
            qunsetenv("DEEPIN_SHORTCUT_VIEWER_ARGS");
        else
            qputenv("DEEPIN_SHORTCUT_VIEWER_ARGS", previousOutputPath);
        installed = false;
    }
};

QStringList readShortcutViewerArguments(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QStringList arguments;
    const auto lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines)
        arguments.append(line);
    return arguments;
}

QJsonObject parseControlResponse(const QString &payload) {
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return {};
    return doc.object();
}

QString firstPaneIdFromControlResponse(const QJsonObject &response) {
    const QJsonArray windows = response.value(QStringLiteral("windows")).toArray();
    if (windows.isEmpty())
        return {};
    const QJsonArray tabs = windows.first().toObject().value(QStringLiteral("tabs")).toArray();
    if (tabs.isEmpty())
        return {};
    const QJsonArray panes = tabs.first().toObject().value(QStringLiteral("panes")).toArray();
    if (panes.isEmpty())
        return {};
    return panes.first().toObject().value(QStringLiteral("id")).toString();
}

QJsonObject shortcutViewerPayload(const QStringList &arguments) {
    for (const QString &argument : arguments) {
        if (!argument.startsWith(QStringLiteral("-j=")))
            continue;
        const QByteArray json = argument.mid(3).toUtf8();
        return QJsonDocument::fromJson(json).object();
    }
    return {};
}

template <typename T> T *findByAccessibleName(QObject *root, const QString &name) {
    if (!root)
        return nullptr;
    for (auto *child : root->findChildren<T *>()) {
        if (accessibleText(child, QAccessible::Name) == name)
            return child;
    }
    return nullptr;
}

template <typename T> T *findByAccessibleNameContaining(QObject *root, const QString &text) {
    if (!root)
        return nullptr;
    for (auto *child : root->findChildren<T *>()) {
        if (accessibleText(child, QAccessible::Name).contains(text))
            return child;
    }
    return nullptr;
}

TerminalWidget *terminalForPaneId(TermPane &pane, const QUuid &paneId) {
    const auto terminals = pane.findChildren<TerminalWidget *>();
    for (auto *terminal : terminals) {
        if (terminal->property("paneId").toUuid() == paneId)
            return terminal;
    }
    return nullptr;
}

class VisibilityEventCounter : public QObject {
public:
    explicit VisibilityEventCounter(QWidget *guardedWidget) : m_guardedWidget(guardedWidget) {}

    int hideCount = 0;
    int showCount = 0;
    int visibleHideUpdateCount = 0;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        switch (event->type()) {
            case QEvent::Hide:
            case QEvent::HideToParent:
                ++hideCount;
                if (m_guardedWidget && m_guardedWidget->updatesEnabled())
                    ++visibleHideUpdateCount;
                break;
            case QEvent::Show:
            case QEvent::ShowToParent:
                ++showCount;
                break;
            default:
                break;
        }
        return false;
    }

private:
    QWidget *m_guardedWidget = nullptr;
};

class PaintEventCounter : public QObject {
public:
    int paintCount = 0;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        Q_UNUSED(watched)
        if (event->type() == QEvent::Paint)
            ++paintCount;
        return false;
    }
};

} // namespace

void TestMainWindow::testSingleTabCtrlDClosesWindow() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *terminal = currentTerminal(window);
    QVERIFY(terminal);
    auto *session = ptySession(terminal);
    QVERIFY(session);
    QSignalSpy dataSpy(session, &PtySession::dataReceived);
    QVERIFY(dataSpy.isValid());

    (void)dataSpy.wait(2000);
    session->write("exit\n");

    QTRY_VERIFY_WITH_TIMEOUT(!window.isVisible(), 5000);
}

void TestMainWindow::testLoggingCategoriesExposeExpectedNames() {
    QCOMPARE(QString::fromUtf8(appLog().categoryName()), QStringLiteral("org.deepin_terminal_ghostty.app"));
    QCOMPARE(QString::fromUtf8(ptyLog().categoryName()), QStringLiteral("org.deepin_terminal_ghostty.pty"));
    QCOMPARE(QString::fromUtf8(terminalLog().categoryName()), QStringLiteral("org.deepin_terminal_ghostty.terminal"));
}

void TestMainWindow::testApplicationMetadataIsConfigured() {
    MainWindow window;

    auto *app = qobject_cast<DApplication *>(qApp);
    QVERIFY(app);
    QCOMPARE(app->applicationName(), QStringLiteral("deepin-terminal-ghostty"));
    QCOMPARE(app->applicationDisplayName(), QStringLiteral("Deepin Terminal Ghostty"));
    QCOMPARE(app->organizationName(), QStringLiteral("deepin"));
    QVERIFY(!app->applicationVersion().isEmpty());
    QVERIFY(!app->applicationDescription().isEmpty());
    QVERIFY(!app->applicationLicense().isEmpty());
    QCOMPARE(app->applicationHomePage(), QStringLiteral("https://github.com/linuxdeepin/deepin-terminal-ghostty"));

    const QIcon expectedLogo(QStringLiteral(":/icons/app/deepin-terminal-ghostty.png"));
    QVERIFY(!expectedLogo.isNull());
    QVERIFY(!app->windowIcon().isNull());
    QCOMPARE(app->windowIcon().pixmap(64, 64).toImage(), expectedLogo.pixmap(64, 64).toImage());
    QVERIFY(!window.windowIcon().isNull());
    QCOMPARE(window.windowIcon().pixmap(64, 64).toImage(), expectedLogo.pixmap(64, 64).toImage());
}

void TestMainWindow::testStartupSessionFinishedEmitsExitCode() {
    StartupOptions options;
    options.execute = QStringLiteral("exit 17");
    options.waitForChild = true;
    options.propagateExitCode = true;

    MainWindow window(options);
    QSignalSpy finishedSpy(&window, &MainWindow::startupSessionFinished);
    QVERIFY(finishedSpy.isValid());

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY2(finishedSpy.wait(5000), "Expected startupSessionFinished signal within 5 seconds");
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy.at(0).at(0).toInt(), 17);

    QTRY_VERIFY_WITH_TIMEOUT(!window.isVisible(), 5000);
}

void TestMainWindow::testClosedSessionRemovesOnlyCurrentTab() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);

    QVERIFY(QMetaObject::invokeMethod(&window, "onTabAddRequested", Qt::DirectConnection));
    QVERIFY(waitForTabCount(tabs, 2));

    tabs->setCurrentIndex(0);
    auto *terminal = currentTerminal(window);
    QVERIFY(terminal);
    auto *session = ptySession(terminal);
    QVERIFY(session);
    QSignalSpy dataSpy(session, &PtySession::dataReceived);
    QVERIFY(dataSpy.isValid());

    (void)dataSpy.wait(2000);
    session->write("exit\n");

    QTRY_COMPARE_WITH_TIMEOUT(tabs->count(), 1, 5000);
    QVERIFY(window.isVisible());
}

void TestMainWindow::testAltArrowWithKeypadModifierIsConsumedByPane() {
    ExposedTermPane pane;
    pane.resize(1200, 800);
    pane.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pane));

    pane.splitCurrent(Qt::Vertical);

    const QList<TerminalWidget *> terminals = pane.findChildren<TerminalWidget *>();
    QCOMPARE(terminals.count(), 2);

    auto *sourceTerminal = pane.currentTerminal();
    QVERIFY(sourceTerminal);

    QKeyEvent keyPress(QEvent::KeyPress, Qt::Key_Left, Qt::AltModifier | Qt::KeypadModifier);
    QVERIFY(pane.eventFilter(sourceTerminal, &keyPress));
}

void TestMainWindow::testTermPaneReportsPaneSnapshotsAfterSplit() {
    ExposedTermPane pane;
    pane.resize(1200, 800);
    pane.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pane));

    const QUuid firstPaneId = pane.paneInfos().first().id;
    QCOMPARE(pane.paneInfos().size(), 1);
    QVERIFY(pane.paneInfos().first().isActive);
    QCOMPARE(pane.activePaneId(), firstPaneId);

    QSignalSpy structureSpy(&pane, &TermPane::paneStructureChanged);
    QVERIFY(structureSpy.isValid());

    pane.splitCurrent(Qt::Vertical);

    QTRY_COMPARE(structureSpy.count(), 1);

    const auto infos = pane.paneInfos();
    QCOMPARE(infos.size(), 2);

    int activeCount = 0;
    QUuid activeId;
    QUuid inactiveId;
    for (const auto &info : infos) {
        if (info.isActive) {
            ++activeCount;
            activeId = info.id;
        } else {
            inactiveId = info.id;
        }
        QVERIFY(!info.id.isNull());
    }
    QCOMPARE(activeCount, 1);
    QCOMPARE(pane.activePaneId(), activeId);
    QVERIFY(!inactiveId.isNull());

    QSignalSpy titleSpy(&pane, &TermPane::paneTitleChanged);
    QVERIFY(titleSpy.isValid());
    pane.setCustomTitle(QStringLiteral("Pane title"));
    QTRY_VERIFY(!titleSpy.isEmpty());
    QCOMPARE(titleSpy.last().at(0).toUuid(), activeId);
    QCOMPARE(titleSpy.last().at(1).toString(), QStringLiteral("Pane title"));

    QSignalSpy activeSpy(&pane, &TermPane::activePaneChanged);
    QVERIFY(activeSpy.isValid());
    QVERIFY(pane.focusPane(inactiveId));
    QTRY_COMPARE(activeSpy.count(), 1);
    QCOMPARE(activeSpy.last().at(0).toUuid(), inactiveId);
    QCOMPARE(pane.activePaneId(), inactiveId);

    pane.closeCurrentSplit();

    const auto remainingInfos = pane.paneInfos();
    QCOMPARE(remainingInfos.size(), 1);
    const QUuid remainingPaneId = remainingInfos.first().id;
    QCOMPARE(pane.activePaneId(), remainingPaneId);
    QVERIFY(remainingInfos.first().isActive);

    QVERIFY(pane.focusPane(remainingPaneId));
    pane.splitCurrent(Qt::Horizontal);

    const auto nestedInfos = pane.paneInfos();
    QCOMPARE(nestedInfos.size(), 2);
    const QUuid secondNestedPaneId = nestedInfos.at(1).id;

    pane.splitCurrent(Qt::Vertical);

    const auto mixedInfos = pane.paneInfos();
    QCOMPARE(mixedInfos.size(), 3);
    QCOMPARE(mixedInfos.at(0).id, remainingPaneId);
    QCOMPARE(mixedInfos.at(1).id, secondNestedPaneId);

    const QUuid promotedClosePaneId = mixedInfos.at(1).id;
    const QUuid expectedActiveAfterPromote = mixedInfos.at(2).id;
    QVERIFY(pane.focusPane(promotedClosePaneId));
    QCOMPARE(pane.activePaneId(), promotedClosePaneId);

    pane.closeCurrentSplit();

    const auto promotedInfos = pane.paneInfos();
    QCOMPARE(promotedInfos.size(), 2);
    QCOMPARE(promotedInfos.at(0).id, remainingPaneId);
    QCOMPARE(promotedInfos.at(1).id, expectedActiveAfterPromote);
    QCOMPARE(pane.activePaneId(), expectedActiveAfterPromote);

    QVERIFY(pane.focusPane(promotedInfos.last().id));
    pane.closeCurrentSplit();
    QCOMPARE(pane.paneInfos().size(), 1);

    pane.closeCurrentSplit();
    QCOMPARE(pane.paneInfos().size(), 0);
    QVERIFY(pane.activePaneId().isNull());
}

void TestMainWindow::testSplitSuppressesIntermediateRepaintWhileReparenting() {
    ExposedTermPane pane;
    pane.resize(1200, 800);
    pane.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pane));

    auto *terminal = pane.currentTerminal();
    QVERIFY(terminal);
    QVERIFY(terminal->isVisible());

    VisibilityEventCounter counter(&pane);
    terminal->installEventFilter(&counter);

    pane.splitCurrent(Qt::Vertical);
    QCoreApplication::processEvents();

    QVERIFY(counter.hideCount > 0 || counter.showCount > 0);
    QCOMPARE(counter.visibleHideUpdateCount, 0);
}

void TestMainWindow::testCloseSplitSuppressesIntermediateRepaintWhileReparenting() {
    ExposedTermPane pane;
    pane.resize(1200, 800);
    pane.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pane));

    const QUuid firstPaneId = pane.activePaneId();
    pane.splitCurrent(Qt::Vertical);
    QCoreApplication::processEvents();
    QVERIFY(pane.focusPane(firstPaneId));

    auto *terminal = pane.currentTerminal();
    QVERIFY(terminal);
    QVERIFY(terminal->isVisible());

    VisibilityEventCounter counter(&pane);
    terminal->installEventFilter(&counter);

    pane.closeCurrentSplit();
    QCoreApplication::processEvents();

    QVERIFY(counter.hideCount > 0 || counter.showCount > 0);
    QCOMPARE(counter.visibleHideUpdateCount, 0);
}

void TestMainWindow::testNestedSplitDoesNotRepaintUnchangedSiblingTerminal() {
    ExposedTermPane pane;
    pane.resize(1200, 800);
    pane.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pane));

    const QUuid leftPaneId = pane.activePaneId();
    pane.splitCurrent(Qt::Vertical);
    QCoreApplication::processEvents();

    auto *leftTerminal = terminalForPaneId(pane, leftPaneId);
    QVERIFY(leftTerminal);
    auto *rightTerminal = pane.currentTerminal();
    QVERIFY(rightTerminal);
    rightTerminal->setFocus();
    QCoreApplication::processEvents();

    PaintEventCounter counter;
    leftTerminal->installEventFilter(&counter);

    pane.splitCurrent(Qt::Horizontal);
    QCoreApplication::processEvents();

    QVERIFY2(counter.paintCount <= 1,
             qPrintable(QStringLiteral("unchanged sibling terminal repainted %1 times").arg(counter.paintCount)));
}

void TestMainWindow::testClosingCurrentSplitFocusesSiblingTerminal() {
    ExposedTermPane pane;
    pane.resize(1200, 800);
    pane.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pane));

    const QUuid firstPaneId = pane.activePaneId();
    pane.splitCurrent(Qt::Vertical);
    const QUuid secondPaneId = pane.activePaneId();
    QVERIFY(pane.focusPane(firstPaneId));
    pane.splitCurrent(Qt::Horizontal);
    const QUuid siblingPaneId = pane.activePaneId();

    QVERIFY(pane.focusPane(siblingPaneId));
    auto *siblingTerminal = terminalForPaneId(pane, siblingPaneId);
    QVERIFY(siblingTerminal);
    auto *closedTerminal = pane.currentTerminal();
    QVERIFY(closedTerminal);
    QCOMPARE(QApplication::focusWidget(), closedTerminal);

    pane.closeCurrentSplit();
    QCoreApplication::processEvents();

    QCOMPARE(pane.activePaneId(), firstPaneId);
    QCOMPARE(pane.currentTerminal(), terminalForPaneId(pane, firstPaneId));
    QCOMPARE(QApplication::focusWidget(), pane.currentTerminal());
    QVERIFY(terminalForPaneId(pane, secondPaneId));
}

void TestMainWindow::testClosingNestedSplitPreservesParentSplitterSizes() {
    ExposedTermPane pane;
    pane.resize(1200, 800);
    pane.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pane));

    const QUuid firstPaneId = pane.activePaneId();
    pane.splitCurrent(Qt::Horizontal);
    const QUuid secondPaneId = pane.activePaneId();
    pane.splitCurrent(Qt::Vertical);
    const QUuid thirdPaneId = pane.activePaneId();
    QVERIFY(!thirdPaneId.isNull());
    QCoreApplication::processEvents();

    auto *firstTerminal = terminalForPaneId(pane, firstPaneId);
    auto *secondTerminal = terminalForPaneId(pane, secondPaneId);
    QVERIFY(firstTerminal);
    QVERIFY(secondTerminal);

    const int firstWidthBeforeClose = firstTerminal->width();
    const int secondWidthBeforeClose = secondTerminal->width();

    pane.closeCurrentSplit();
    QCoreApplication::processEvents();

    QCOMPARE(pane.paneInfos().size(), 2);
    QCOMPARE(pane.activePaneId(), secondPaneId);
    QVERIFY2(qAbs(firstTerminal->width() - firstWidthBeforeClose) <= 2,
             qPrintable(QStringLiteral("first pane width changed from %1 to %2")
                            .arg(firstWidthBeforeClose)
                            .arg(firstTerminal->width())));
    QVERIFY2(qAbs(secondTerminal->width() - secondWidthBeforeClose) <= 2,
             qPrintable(QStringLiteral("second pane width changed from %1 to %2")
                            .arg(secondWidthBeforeClose)
                            .arg(secondTerminal->width())));
}

void TestMainWindow::testClosingRepeatedSameDirectionSplitsPreservesSiblings() {
    ExposedTermPane pane;
    pane.resize(1200, 800);
    pane.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pane));

    const QUuid firstPaneId = pane.activePaneId();
    pane.splitCurrent(Qt::Horizontal);
    const QUuid secondPaneId = pane.activePaneId();

    QVERIFY(pane.focusPane(firstPaneId));
    pane.splitCurrent(Qt::Horizontal);
    const QUuid thirdPaneId = pane.activePaneId();

    pane.splitCurrent(Qt::Horizontal);
    const QUuid fourthPaneId = pane.activePaneId();

    const auto beforeClose = pane.paneInfos();
    QCOMPARE(beforeClose.size(), 4);
    QCOMPARE(beforeClose.at(0).id, firstPaneId);
    QCOMPARE(beforeClose.at(1).id, thirdPaneId);
    QCOMPARE(beforeClose.at(2).id, fourthPaneId);
    QCOMPARE(beforeClose.at(3).id, secondPaneId);
    QCoreApplication::processEvents();

    auto *firstTerminal = terminalForPaneId(pane, firstPaneId);
    auto *secondTerminal = terminalForPaneId(pane, secondPaneId);
    auto *thirdTerminal = terminalForPaneId(pane, thirdPaneId);
    QVERIFY(firstTerminal);
    QVERIFY(secondTerminal);
    QVERIFY(thirdTerminal);

    const int firstWidthBeforeClose = firstTerminal->width();
    const int secondWidthBeforeClose = secondTerminal->width();
    const int thirdWidthBeforeClose = thirdTerminal->width();

    QVERIFY(pane.focusPane(fourthPaneId));
    pane.closeCurrentSplit();
    QCoreApplication::processEvents();

    const auto afterFourthClose = pane.paneInfos();
    QCOMPARE(afterFourthClose.size(), 3);
    QCOMPARE(afterFourthClose.at(0).id, firstPaneId);
    QCOMPARE(afterFourthClose.at(1).id, thirdPaneId);
    QCOMPARE(afterFourthClose.at(2).id, secondPaneId);
    QCOMPARE(pane.activePaneId(), thirdPaneId);
    QVERIFY(thirdTerminal->width() > thirdWidthBeforeClose);
    QVERIFY(firstTerminal->width() <= firstWidthBeforeClose + 1);

    pane.closeCurrentSplit();
    QCoreApplication::processEvents();

    const auto afterThirdClose = pane.paneInfos();
    QCOMPARE(afterThirdClose.size(), 2);
    QCOMPARE(afterThirdClose.at(0).id, firstPaneId);
    QCOMPARE(afterThirdClose.at(1).id, secondPaneId);
    QVERIFY2(secondTerminal->width() >= secondWidthBeforeClose - 1,
             qPrintable(QStringLiteral("second width changed from %1 to %2")
                            .arg(secondWidthBeforeClose)
                            .arg(secondTerminal->width())));
}

void TestMainWindow::testCloseOtherTerminalsPublishesSingleStructureChange() {
    ExposedTermPane pane;
    pane.resize(1200, 800);
    pane.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pane));

    pane.splitCurrent(Qt::Vertical);
    pane.splitCurrent(Qt::Horizontal);
    QCOMPARE(pane.paneInfos().size(), 3);

    QSignalSpy structureSpy(&pane, &TermPane::paneStructureChanged);
    QVERIFY(structureSpy.isValid());

    pane.closeOtherTerminals();

    QCOMPARE(pane.paneInfos().size(), 1);
    QCOMPARE(structureSpy.count(), 1);
}

void TestMainWindow::testTermPaneReportsProcessIconNames() {
    ExposedTermPane pane;
    pane.resize(1200, 800);
    pane.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pane));

    auto *terminal = pane.currentTerminal();
    QVERIFY(terminal);
    terminal->setProperty("shellCommand", QStringLiteral("bash -lc 'npx codex'"));

    const auto infos = pane.paneInfos();
    QCOMPARE(infos.size(), 1);
    QCOMPARE(infos.first().iconName, QStringLiteral("codex"));

    terminal->setProperty("shellCommand", QStringLiteral("bash -lc 'python -m aider'"));
    QCOMPARE(pane.paneInfos().first().iconName, QStringLiteral("aider"));

    terminal->setProperty("shellCommand", QStringLiteral("bash -lc 'docker compose up'"));
    QCOMPARE(pane.paneInfos().first().iconName, QStringLiteral("docker"));

    terminal->setProperty("shellCommand", QStringLiteral("claude --dangerously-skip-permissions"));
    QCOMPARE(pane.paneInfos().first().iconName, QStringLiteral("claude"));

    terminal->setProperty("shellCommand", QString());
    QCOMPARE(pane.paneInfos().first().iconName, QStringLiteral("terminal"));
}

void TestMainWindow::testAppTerminalsSetTerminalContentMargins() {
    ExposedTermPane pane;
    pane.resize(1200, 800);
    pane.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pane));

    auto *terminal = pane.currentTerminal();
    QVERIFY(terminal);
    QCOMPARE(terminal->contentsMargins(), QMargins(12, 12, 12, 12));
}

void TestMainWindow::testTermPaneWrapsTerminalWithFloatingScrollBar() {
    ExposedTermPane pane;
    pane.resize(360, 160);
    pane.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pane));

    auto *terminal = pane.currentTerminal();
    QVERIFY(terminal);

    auto *container = qobject_cast<TerminalScrollContainer *>(terminal->parentWidget());
    QVERIFY(container);
    QCOMPARE(container->terminal(), terminal);

    auto *scrollBar = container->scrollBar();
    QVERIFY(scrollBar);
    QCOMPARE(scrollBar->orientation(), Qt::Vertical);
    QCOMPARE(scrollBar->contextMenuPolicy(), Qt::NoContextMenu);
    QVERIFY(scrollBar->styleSheet().contains(QStringLiteral("width: 15")));
    QCOMPARE(terminal->geometry(), container->rect());
    QVERIFY(terminal->isVisibleTo(container));

    QSignalSpy spy(terminal, &TerminalWidget::viewportScrollStateChanged);
    QVERIFY(spy.isValid());

    QByteArray lines;
    for (int i = 0; i < 200; ++i)
        lines += QByteArray::number(i) + '\n';
    const bool invoked =
        QMetaObject::invokeMethod(terminal, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, lines));
    QVERIFY(invoked);
    QTRY_VERIFY(spy.count() > 0);

    const auto state = terminal->viewportScrollState();
    QVERIFY(state.canScroll());
    QTRY_VERIFY(scrollBar->parentWidget()->isVisibleTo(container));
    QCOMPARE(scrollBar->maximum(), state.maximumOffset());
    QCOMPARE(scrollBar->pageStep(), state.visibleRows);

    scrollBar->setValue(0);
    QTRY_COMPARE(terminal->viewportScrollState().offset, 0);
}

void TestMainWindow::testScrollBarPositionDoesNotFollowBottomAfterOutput() {
    ExposedTermPane pane;
    pane.resize(360, 160);
    pane.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pane));

    auto *terminal = pane.currentTerminal();
    QVERIFY(terminal);

    auto *container = qobject_cast<TerminalScrollContainer *>(terminal->parentWidget());
    QVERIFY(container);
    auto *scrollBar = container->scrollBar();
    QVERIFY(scrollBar);

    QSignalSpy spy(terminal, &TerminalWidget::viewportScrollStateChanged);
    QVERIFY(spy.isValid());

    QByteArray lines;
    for (int i = 0; i < 200; ++i)
        lines += QByteArray::number(i) + '\n';
    bool invoked =
        QMetaObject::invokeMethod(terminal, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, lines));
    QVERIFY(invoked);
    QTRY_VERIFY(spy.count() > 0);

    const int bottomOffset = terminal->viewportScrollState().maximumOffset();
    QVERIFY(bottomOffset > 0);
    QCOMPARE(terminal->viewportScrollState().offset, bottomOffset);
    QCOMPARE(scrollBar->value(), bottomOffset);

    scrollBar->setValue(bottomOffset / 2);
    QTRY_COMPARE(terminal->viewportScrollState().offset, bottomOffset / 2);

    spy.clear();
    invoked =
        QMetaObject::invokeMethod(terminal, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, "tail\n"));
    QVERIFY(invoked);
    QTRY_VERIFY(spy.count() > 0);

    QCOMPARE(terminal->viewportScrollState().offset, bottomOffset / 2);
    QCOMPARE(scrollBar->value(), bottomOffset / 2);
    QVERIFY(terminal->viewportScrollState().maximumOffset() > bottomOffset);
}

void TestMainWindow::testVerticalTabsActionReflectsAndUpdatesSettings() {
    auto *settings = AppSettings::instance();
    settings->setVerticalTabsEnabled(false);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *action = window.findChild<QAction *>(QStringLiteral("verticalTabsAction"));
    QVERIFY(action);
    QVERIFY(action->isCheckable());
    QCOMPARE(action->isChecked(), false);

    action->trigger();

    QCOMPARE(action->isChecked(), true);
    QCOMPARE(AppSettings::instance()->verticalTabsEnabled(), true);
}

void TestMainWindow::testVerticalTabsActionTracksExternalSettingChanges() {
    auto *settings = AppSettings::instance();
    settings->setVerticalTabsEnabled(false);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *action = window.findChild<QAction *>(QStringLiteral("verticalTabsAction"));
    QVERIFY(action);
    QCOMPARE(action->isChecked(), false);

    settings->dsettings()->setOption("basic.layout.verticalTabs", true);

    QTRY_VERIFY(action->isChecked());
    QTRY_VERIFY(AppSettings::instance()->verticalTabsEnabled());
}

void TestMainWindow::testVerticalTabsActionReflectsStartupSetting() {
    auto *settings = AppSettings::instance();
    settings->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *action = window.findChild<QAction *>(QStringLiteral("verticalTabsAction"));
    QVERIFY(action);
    QVERIFY(action->isChecked());
}

void TestMainWindow::testVerticalSidebarShowsTabsAndPanes() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *verticalSidebar = sidebar(window);
    QVERIFY(verticalSidebar);
    QVERIFY(verticalSidebar->isVisible());

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);

    QVERIFY(QMetaObject::invokeMethod(&window, "onTabAddRequested", Qt::DirectConnection));
    QVERIFY(waitForTabCount(tabs, 2));

    auto *pane = currentPane(window);
    QVERIFY(pane);
    pane->splitCurrent(Qt::Vertical);

    QTRY_VERIFY(verticalSidebar->findChildren<QAbstractButton *>(QStringLiteral("verticalTabButton")).size() >= 2);
    QTRY_VERIFY(verticalSidebar->findChildren<QAbstractButton *>(QStringLiteral("verticalPaneButton")).size() >= 2);
}

void TestMainWindow::testActivePaneTitleUpdatesTabAndWindowTitles() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *pane = currentPane(window);
    QVERIFY(pane);
    pane->splitCurrent(Qt::Vertical);

    const auto infos = pane->paneInfos();
    QVERIFY(infos.size() >= 2);
    QVERIFY(pane->focusPane(infos.last().id));
    pane->setCustomTitle(QStringLiteral("Logs"));

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QTRY_COMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("Logs"));
    QTRY_COMPARE(window.windowTitle(), QStringLiteral("Logs"));

    auto *verticalSidebar = sidebar(window);
    QVERIFY(verticalSidebar);
    bool hasLogsPane = false;
    for (auto *button : verticalSidebar->findChildren<QAbstractButton *>(QStringLiteral("verticalPaneButton"))) {
        if (button->text() == QStringLiteral("Logs") && button->property("active").toBool()) {
            hasLogsPane = true;
            break;
        }
    }
    QVERIFY(hasLogsPane);
}

void TestMainWindow::testSidebarExpansionSurvivesModeSwitch() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *verticalSidebar = sidebar(window);
    QVERIFY(verticalSidebar);

    auto *pane = currentPane(window);
    QVERIFY(pane);
    pane->splitCurrent(Qt::Vertical);

    QTRY_VERIFY(verticalSidebar->findChildren<QAbstractButton *>(QStringLiteral("verticalPaneButton")).size() >= 2);

    auto *expandButton = verticalSidebar->findChild<QAbstractButton *>(QStringLiteral("verticalTabExpandButton"));
    QVERIFY(expandButton);
    QTest::mouseClick(expandButton, Qt::LeftButton);
    QTRY_COMPARE(verticalSidebar->findChildren<QAbstractButton *>(QStringLiteral("verticalPaneButton")).size(), 0);

    auto *verticalAction = window.findChild<QAction *>(QStringLiteral("verticalTabsAction"));
    QVERIFY(verticalAction);
    verticalAction->setChecked(false);
    verticalAction->setChecked(true);

    verticalSidebar = sidebar(window);
    QVERIFY(verticalSidebar);
    QTRY_COMPARE(verticalSidebar->findChildren<QAbstractButton *>(QStringLiteral("verticalPaneButton")).size(), 0);
}

void TestMainWindow::testHorizontalTitlebarTabsSurviveModeSwitch() {
    AppSettings::instance()->setVerticalTabsEnabled(false);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *verticalAction = window.findChild<QAction *>(QStringLiteral("verticalTabsAction"));
    QVERIFY(verticalAction);

    auto *tb = titlebar(window);
    QVERIFY(tb);
    QVERIFY(tb->customWidget());

    QPointer<DTabBar> initialTabs = tb->customWidget()->findChild<DTabBar *>();
    QVERIFY(initialTabs);
    QCOMPARE(initialTabs->count(), 1);

    verticalAction->setChecked(true);
    QTRY_VERIFY(tb->customWidget());
    QVERIFY(!tb->customWidget()->findChild<DTabBar *>());

    QVERIFY(initialTabs);
    QVERIFY(QMetaObject::invokeMethod(&window, "onTabAddRequested", Qt::DirectConnection));
    QVERIFY(waitForTabCount(initialTabs, 2));

    verticalAction->setChecked(false);
    QTRY_VERIFY(tb->customWidget());

    auto *restoredTabs = tb->customWidget()->findChild<DTabBar *>();
    QVERIFY(restoredTabs);
    QCOMPARE(restoredTabs, initialTabs.data());
    QCOMPARE(restoredTabs->count(), 2);
}

void TestMainWindow::testHorizontalTitlebarTabsDoNotCoverMenuButton() {
    AppSettings::instance()->setVerticalTabsEnabled(false);

    MainWindow window;
    window.resize(640, 420);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    auto *tb = titlebar(window);
    QVERIFY(tb);
    auto *optionButton = tb->findChild<QWidget *>(QStringLiteral("DTitlebarDWindowOptionButton"));
    QVERIFY(optionButton);

    const QRect tabsRect(tabs->mapToGlobal(QPoint(0, 0)), tabs->size());
    const QRect optionRect(optionButton->mapToGlobal(QPoint(0, 0)), optionButton->size());
    auto rectString = [](const QRect &rect) {
        return QStringLiteral("(%1,%2 %3x%4)").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
    };
    QVERIFY2(!tabsRect.intersects(optionRect), qPrintable(QStringLiteral("tabbar %1 overlaps titlebar menu button %2")
                                                              .arg(rectString(tabsRect), rectString(optionRect))));
}

void TestMainWindow::testVerticalSidebarTabClickSwitchesCurrentTab() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);

    QVERIFY(QMetaObject::invokeMethod(&window, "onTabAddRequested", Qt::DirectConnection));
    QVERIFY(waitForTabCount(tabs, 2));

    tabs->setCurrentIndex(0);
    auto *stack = window.findChild<QStackedWidget *>();
    QVERIFY(stack);
    QCOMPARE(stack->currentIndex(), 0);

    auto *verticalSidebar = sidebar(window);
    QVERIFY(verticalSidebar);

    QTRY_COMPARE(verticalSidebar->findChildren<QAbstractButton *>(QStringLiteral("verticalTabButton")).size(), 2);
    const auto buttons = verticalSidebar->findChildren<QAbstractButton *>(QStringLiteral("verticalTabButton"));
    QVERIFY(buttons.size() >= 2);

    QTest::mouseClick(buttons.at(1), Qt::LeftButton);

    QTRY_COMPARE(tabs->currentIndex(), 1);
    QTRY_COMPARE(stack->currentIndex(), 1);
}

void TestMainWindow::testVerticalSidebarTabClickRecoversFromStaleTabData() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);

    QVERIFY(QMetaObject::invokeMethod(&window, "onTabAddRequested", Qt::DirectConnection));
    QVERIFY(waitForTabCount(tabs, 2));

    auto *stack = window.findChild<QStackedWidget *>();
    QVERIFY(stack);

    tabs->setCurrentIndex(0);
    QCOMPARE(stack->currentIndex(), 0);

    tabs->setTabData(1, -1);
    tabs->setCurrentIndex(1);

    QTRY_COMPARE(tabs->currentIndex(), 1);
    QTRY_COMPARE(stack->currentIndex(), 1);
}

void TestMainWindow::testVerticalSidebarDragReordersTabs() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.resize(900, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);

    auto *pane = currentPane(window);
    QVERIFY(pane);
    pane->setCustomTitle(QStringLiteral("One"));

    QVERIFY(QMetaObject::invokeMethod(&window, "onTabAddRequested", Qt::DirectConnection));
    QVERIFY(waitForTabCount(tabs, 2));
    pane = currentPane(window);
    QVERIFY(pane);
    pane->setCustomTitle(QStringLiteral("Two"));

    QVERIFY(QMetaObject::invokeMethod(&window, "onTabAddRequested", Qt::DirectConnection));
    QVERIFY(waitForTabCount(tabs, 3));
    pane = currentPane(window);
    QVERIFY(pane);
    pane->setCustomTitle(QStringLiteral("Three"));

    auto *verticalSidebar = sidebar(window);
    QVERIFY(verticalSidebar);
    QTRY_COMPARE(verticalSidebar->items().size(), 3);

    const int firstTabId = verticalSidebar->items().at(0).id;
    QWidget *firstSection = verticalTabSection(verticalSidebar, firstTabId);
    QWidget *lastSection = verticalTabSection(verticalSidebar, verticalSidebar->items().at(2).id);
    QVERIFY(firstSection);
    QVERIFY(lastSection);

    QTest::mousePress(firstSection, Qt::LeftButton, Qt::NoModifier, firstSection->rect().center());
    QTest::mouseMove(firstSection,
                     firstSection->mapFrom(verticalSidebar, lastSection->geometry().bottomLeft() + QPoint(10, 8)));
    auto *dragProxy = verticalSidebar->findChild<QWidget *>(QStringLiteral("verticalTabDragProxy"));
    QVERIFY(dragProxy);
    QVERIFY(dragProxy->isVisibleTo(verticalSidebar));
    QVERIFY(firstSection->property("dragPlaceholder").toBool());
    QTRY_COMPARE(verticalSidebar->items().at(0).title, QStringLiteral("Two"));
    QCOMPARE(verticalSidebar->items().at(1).title, QStringLiteral("Three"));
    QCOMPARE(verticalSidebar->items().at(2).title, QStringLiteral("One"));
    QCOMPARE(tabs->tabText(0), QStringLiteral("One"));

    pane->setCustomTitle(QStringLiteral("Three done"));
    QCoreApplication::processEvents();
    dragProxy = verticalSidebar->findChild<QWidget *>(QStringLiteral("verticalTabDragProxy"));
    QVERIFY(dragProxy);
    QVERIFY(dragProxy->isVisibleTo(verticalSidebar));
    QCOMPARE(verticalSidebar->items().at(0).title, QStringLiteral("Two"));
    QCOMPARE(verticalSidebar->items().at(1).title, QStringLiteral("Three"));
    QCOMPARE(verticalSidebar->items().at(2).title, QStringLiteral("One"));

    verticalSidebar->finishTabDrag(firstTabId, firstSection->mapToGlobal(firstSection->rect().center()));
    QTRY_VERIFY(!verticalSidebar->findChild<QWidget *>(QStringLiteral("verticalTabDragProxy")));
    QWidget *releasedSection = verticalTabSection(verticalSidebar, firstTabId);
    QVERIFY(releasedSection);
    QVERIFY(!releasedSection->property("dragPlaceholder").toBool());

    QTRY_COMPARE(tabs->tabText(0), QStringLiteral("Two"));
    QCOMPARE(tabs->tabText(1), QStringLiteral("Three done"));
    QCOMPARE(tabs->tabText(2), QStringLiteral("One"));

    const QJsonArray snapshotTabs = window.controlSnapshot().value(QStringLiteral("tabs")).toArray();
    QCOMPARE(snapshotTabs.size(), 3);
    QCOMPARE(snapshotTabs.at(0).toObject().value(QStringLiteral("title")).toString(), QStringLiteral("Two"));
    QCOMPARE(snapshotTabs.at(1).toObject().value(QStringLiteral("title")).toString(), QStringLiteral("Three done"));
    QCOMPARE(snapshotTabs.at(2).toObject().value(QStringLiteral("title")).toString(), QStringLiteral("One"));
    QCOMPARE(tabs->currentIndex(), 1);
    QVERIFY(snapshotTabs.at(1).toObject().value(QStringLiteral("active")).toBool());
}

void TestMainWindow::testVerticalSidebarIncludesDecorativeHierarchyElements() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *pane = currentPane(window);
    QVERIFY(pane);
    pane->splitCurrent(Qt::Vertical);

    auto *verticalSidebar = sidebar(window);
    QVERIFY(verticalSidebar);

    QTRY_VERIFY(verticalSidebar->findChild<QWidget *>(QStringLiteral("verticalTabBadge")));
    QTRY_VERIFY(verticalSidebar->findChild<QWidget *>(QStringLiteral("verticalPaneGuide")));
    QTRY_VERIFY(verticalSidebar->findChild<QWidget *>(QStringLiteral("verticalPaneBadge")));
}

void TestMainWindow::testVerticalSidebarElidesLabelsWhenNarrow() {
    VerticalTabSidebar sidebar;
    sidebar.resize(110, 400);

    VerticalTabSidebar::TabItem item;
    item.id = 1;
    item.title = QStringLiteral("Very long terminal tab label that must be elided");
    item.isCurrent = true;
    item.expanded = true;

    TermPane::PaneInfo pane;
    pane.id = QUuid::createUuid();
    pane.title = QStringLiteral("Very long pane title that must not overflow the sidebar");
    pane.isActive = true;
    item.panes.append(pane);

    TermPane::PaneInfo pane2;
    pane2.id = QUuid::createUuid();
    pane2.title = QStringLiteral("Second pane");
    pane2.isActive = false;
    item.panes.append(pane2);

    sidebar.setItems({item});
    sidebar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&sidebar));
    QCoreApplication::processEvents();

    auto *scrollArea = sidebar.findChild<QScrollArea *>(QStringLiteral("verticalTabSidebarScrollArea"));
    QVERIFY(scrollArea);
    QVERIFY(scrollArea->widget());
    QVERIFY(scrollArea->widget()->width() <= scrollArea->viewport()->width());

    auto *tabButton = sidebar.findChild<QAbstractButton *>(QStringLiteral("verticalTabButton"));
    auto *paneButton = sidebar.findChild<QAbstractButton *>(QStringLiteral("verticalPaneButton"));
    QVERIFY(tabButton);
    QVERIFY(paneButton);
    QVERIFY(tabButton->text().contains(QChar(0x2026)));
    QVERIFY(paneButton->text().contains(QChar(0x2026)));
}

void TestMainWindow::testCoreControlsExposeAccessibleLabels() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QCOMPARE(accessibleText(&window, QAccessible::Name), QStringLiteral("Deepin Terminal Ghostty"));
    QVERIFY(accessibleText(&window, QAccessible::Description).contains(QStringLiteral("terminal emulator")));
    QVERIFY(accessibleRole(&window) != QAccessible::NoRole);

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QCOMPARE(accessibleText(tabs, QAccessible::Name), QStringLiteral("Terminal tabs"));
    QVERIFY(accessibleText(tabs, QAccessible::Description).contains(QStringLiteral("terminal tabs")));
    QVERIFY(accessibleRole(tabs) != QAccessible::NoRole);

    auto *terminal = currentTerminal(window);
    QVERIFY(terminal);
    QCOMPARE(accessibleText(terminal, QAccessible::Name), QStringLiteral("Terminal pane"));
    QVERIFY(accessibleText(terminal, QAccessible::Description).contains(QStringLiteral("terminal input and output")));
    QVERIFY(accessibleRole(terminal) != QAccessible::NoRole);

    auto *verticalTabsAction = window.findChild<QAction *>(QStringLiteral("verticalTabsAction"));
    auto *remoteAction = window.findChild<QAction *>(QStringLiteral("remoteManagementAction"));
    auto *settingsAction = window.findChild<QAction *>(QStringLiteral("settingsAction"));
    QVERIFY(verticalTabsAction);
    QVERIFY(remoteAction);
    QVERIFY(settingsAction);
    QCOMPARE(verticalTabsAction->toolTip(), QStringLiteral("Toggle vertical tab navigation"));
    QCOMPARE(remoteAction->toolTip(), QStringLiteral("Open remote server management"));
    QCOMPARE(settingsAction->toolTip(), QStringLiteral("Open application settings"));
}

void TestMainWindow::testVerticalSidebarExposesAccessibleLabels() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *pane = currentPane(window);
    QVERIFY(pane);
    pane->splitCurrent(Qt::Vertical);

    auto *verticalAction = window.findChild<QAction *>(QStringLiteral("verticalTabsAction"));
    QVERIFY(verticalAction);
    verticalAction->setChecked(true);

    auto *verticalSidebar = sidebar(window);
    QVERIFY(verticalSidebar);
    QTRY_VERIFY(verticalSidebar->isVisible());

    QCOMPARE(accessibleText(verticalSidebar, QAccessible::Name), QStringLiteral("Vertical terminal tabs"));
    QVERIFY(accessibleText(verticalSidebar, QAccessible::Description).contains(QStringLiteral("tabs and panes")));
    QVERIFY(accessibleRole(verticalSidebar) != QAccessible::NoRole);

    auto *expandButton = verticalSidebar->findChild<QAbstractButton *>(QStringLiteral("verticalTabExpandButton"));
    auto *tabButton = verticalSidebar->findChild<QAbstractButton *>(QStringLiteral("verticalTabButton"));
    const auto paneButtons = verticalSidebar->findChildren<QAbstractButton *>(QStringLiteral("verticalPaneButton"));
    auto *tabBadge = verticalSidebar->findChild<QLabel *>(QStringLiteral("verticalTabBadge"));
    auto *paneBadge = verticalSidebar->findChild<QLabel *>(QStringLiteral("verticalPaneBadge"));
    QVERIFY(expandButton);
    QVERIFY(tabButton);
    QVERIFY(paneButtons.size() >= 2);
    QVERIFY(tabBadge);
    QVERIFY(paneBadge);

    QVERIFY(accessibleText(expandButton, QAccessible::Name).contains(QStringLiteral("panes")));
    QVERIFY(accessibleText(tabButton, QAccessible::Name).contains(QStringLiteral("Terminal tab")));
    QVERIFY(accessibleText(paneButtons.first(), QAccessible::Name).contains(QStringLiteral("Terminal pane")));
    QVERIFY(accessibleText(tabBadge, QAccessible::Name).contains(QStringLiteral("Process")));
    QVERIFY(accessibleText(paneBadge, QAccessible::Name).contains(QStringLiteral("Process")));
}

void TestMainWindow::testSearchBarExposesAccessibleLabels() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *terminal = currentTerminal(window);
    QVERIFY(terminal);
    terminal->setFocus();
    QTest::keyClick(terminal, Qt::Key_F, Qt::ControlModifier | Qt::AltModifier);

    auto *searchBar = window.findChild<QWidget *>(QStringLiteral("pageSearchBar"));
    QVERIFY(searchBar);
    QTRY_VERIFY(searchBar->isVisible());
    QCOMPARE(accessibleText(searchBar, QAccessible::Name), QStringLiteral("Terminal search"));
    QVERIFY(accessibleText(searchBar, QAccessible::Description).contains(QStringLiteral("Search text")));
    QVERIFY(accessibleRole(searchBar) != QAccessible::NoRole);

    auto *searchEdit = searchBar->findChild<QWidget *>(QStringLiteral("terminalSearchEdit"));
    auto *previousButton = searchBar->findChild<QAbstractButton *>(QStringLiteral("findPreviousButton"));
    auto *nextButton = searchBar->findChild<QAbstractButton *>(QStringLiteral("findNextButton"));
    QVERIFY(searchEdit);
    QVERIFY(previousButton);
    QVERIFY(nextButton);
    QCOMPARE(accessibleText(searchEdit, QAccessible::Name), QStringLiteral("Search text"));
    QCOMPARE(accessibleText(previousButton, QAccessible::Name), QStringLiteral("Find previous"));
    QCOMPARE(accessibleText(nextButton, QAccessible::Name), QStringLiteral("Find next"));
}

void TestMainWindow::testSettingsDialogExposesAccessibleLabels() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *settingsAction = window.findChild<QAction *>(QStringLiteral("settingsAction"));
    QVERIFY(settingsAction);
    settingsAction->trigger();

    auto *dialog = window.findChild<SettingsDialog *>();
    QVERIFY(dialog);
    QTRY_VERIFY(dialog->isVisible());
    QCOMPARE(accessibleText(dialog, QAccessible::Name), QStringLiteral("Settings"));
    QVERIFY(accessibleText(dialog, QAccessible::Description).contains(QStringLiteral("Configure terminal")));
    QVERIFY(accessibleRole(dialog) != QAccessible::NoRole);
}

void TestMainWindow::testShortcutViewerLaunchesFromDisplayShortcut() {
    ShortcutViewerCapture capture;
    QVERIFY(capture.install());

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&window));

    QTest::keyClick(&window, Qt::Key_Slash, Qt::ControlModifier | Qt::ShiftModifier);

    QTRY_VERIFY(QFile::exists(capture.outputPath));
    const QStringList arguments = readShortcutViewerArguments(capture.outputPath);
    capture.restore();

    QCOMPARE(arguments.size(), 2);
    QVERIFY(arguments.at(0).startsWith(QStringLiteral("-j=")));
    QVERIFY(arguments.at(1).startsWith(QStringLiteral("-p=")));
}

void TestMainWindow::testVerticalSidebarAccessibleLabelsTrackTitlesAndExpansion() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *pane = currentPane(window);
    QVERIFY(pane);
    pane->splitCurrent(Qt::Vertical);

    const auto infos = pane->paneInfos();
    QVERIFY(infos.size() >= 2);
    QVERIFY(pane->focusPane(infos.last().id));
    pane->setCustomTitle(QStringLiteral("Build Logs"));

    auto *verticalSidebar = sidebar(window);
    QVERIFY(verticalSidebar);
    QTRY_VERIFY(verticalSidebar->isVisible());

    bool foundBuildLogsPane = false;
    for (auto *button : verticalSidebar->findChildren<QAbstractButton *>(QStringLiteral("verticalPaneButton"))) {
        if (accessibleText(button, QAccessible::Name).contains(QStringLiteral("Build Logs"))) {
            foundBuildLogsPane = true;
            break;
        }
    }
    QVERIFY(foundBuildLogsPane);

    auto *expandButton = verticalSidebar->findChild<QAbstractButton *>(QStringLiteral("verticalTabExpandButton"));
    QVERIFY(expandButton);
    QVERIFY(accessibleText(expandButton, QAccessible::Name).contains(QStringLiteral("Collapse panes")));

    QTest::mouseClick(expandButton, Qt::LeftButton);
    QTRY_COMPARE(verticalSidebar->findChildren<QAbstractButton *>(QStringLiteral("verticalPaneButton")).size(), 0);

    QCoreApplication::processEvents();
    expandButton = verticalSidebar->findChild<QAbstractButton *>(QStringLiteral("verticalTabExpandButton"));
    QVERIFY(expandButton);
    QVERIFY(accessibleText(expandButton, QAccessible::Name).contains(QStringLiteral("Expand panes")));
}

void TestMainWindow::testRemoteManagementPanelExposesAccessibleLabels() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *remoteAction = window.findChild<QAction *>(QStringLiteral("remoteManagementAction"));
    QVERIFY(remoteAction);
    remoteAction->trigger();

    auto *panel = window.findChild<RemoteManagementPanel *>();
    QVERIFY(panel);
    QTRY_VERIFY(panel->isVisible());
    QCOMPARE(accessibleText(panel, QAccessible::Name), QStringLiteral("Remote management"));
    QVERIFY(accessibleText(panel, QAccessible::Description).contains(QStringLiteral("remote servers")));

    auto *emptyLabel = panel->findChild<QLabel *>(QStringLiteral("remoteEmptyLabel"));
    auto *addButton = panel->findChild<QAbstractButton *>(QStringLiteral("addRemoteServerButton"));
    QVERIFY(emptyLabel);
    QVERIFY(addButton);
    QCOMPARE(accessibleText(emptyLabel, QAccessible::Name), QStringLiteral("No remote servers configured"));
    QCOMPARE(accessibleText(addButton, QAccessible::Name), QStringLiteral("Add remote server"));
    QVERIFY(accessibleText(addButton, QAccessible::Description).contains(QStringLiteral("Create a remote server")));
}

void TestMainWindow::testServerConfigDialogExposesAccessibleLabels() {
    ServerConfigOptDlg dialog(ServerConfigOptDlg::SCT_ADD);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));

    QCOMPARE(accessibleText(&dialog, QAccessible::Name), QStringLiteral("Add remote server"));
    QVERIFY(accessibleText(&dialog, QAccessible::Description).contains(QStringLiteral("remote server connection")));

    auto *serverName = dialog.findChild<QWidget *>(QStringLiteral("serverNameEdit"));
    auto *address = dialog.findChild<QWidget *>(QStringLiteral("serverAddressEdit"));
    auto *port = dialog.findChild<QWidget *>(QStringLiteral("serverPortSpinBox"));
    auto *userName = dialog.findChild<QWidget *>(QStringLiteral("serverUserNameEdit"));
    auto *advanced = dialog.findChild<QAbstractButton *>(QStringLiteral("advancedServerOptionsButton"));
    auto *cancel = dialog.findChild<QAbstractButton *>(QStringLiteral("cancelServerConfigButton"));
    auto *add = dialog.findChild<QAbstractButton *>(QStringLiteral("saveServerConfigButton"));
    QVERIFY(serverName);
    QVERIFY(address);
    QVERIFY(port);
    QVERIFY(userName);
    QVERIFY(advanced);
    QVERIFY(cancel);
    QVERIFY(add);

    QCOMPARE(accessibleText(serverName, QAccessible::Name), QStringLiteral("Server name"));
    QCOMPARE(accessibleText(address, QAccessible::Name), QStringLiteral("Address"));
    QCOMPARE(accessibleText(port, QAccessible::Name), QStringLiteral("Port"));
    QCOMPARE(accessibleText(userName, QAccessible::Name), QStringLiteral("Username"));
    QCOMPARE(accessibleText(advanced, QAccessible::Name), QStringLiteral("Advanced options"));
    QCOMPARE(accessibleText(cancel, QAccessible::Name), QStringLiteral("Cancel"));
    QCOMPARE(accessibleText(add, QAccessible::Name), QStringLiteral("Add remote server"));
}

void TestMainWindow::testAccessibleSearchControlsDriveFindActions() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *terminal = currentTerminal(window);
    QVERIFY(terminal);
    terminal->setFocus();
    QTest::keyClick(terminal, Qt::Key_F, Qt::ControlModifier | Qt::AltModifier);

    auto *searchBar =
        qobject_cast<PageSearchBar *>(findByAccessibleName<QWidget>(&window, QStringLiteral("Terminal search")));
    QVERIFY(searchBar);
    QTRY_VERIFY(searchBar->isVisible());

    auto *lineEdit = searchBar->findChild<QLineEdit *>();
    auto *previousButton = findByAccessibleName<QAbstractButton>(searchBar, QStringLiteral("Find previous"));
    auto *nextButton = findByAccessibleName<QAbstractButton>(searchBar, QStringLiteral("Find next"));
    QVERIFY(lineEdit);
    QVERIFY(previousButton);
    QVERIFY(nextButton);

    QSignalSpy keywordSpy(searchBar, &PageSearchBar::keywordChanged);
    QSignalSpy nextSpy(searchBar, &PageSearchBar::findNext);
    QSignalSpy previousSpy(searchBar, &PageSearchBar::findPrev);
    QVERIFY(keywordSpy.isValid());
    QVERIFY(nextSpy.isValid());
    QVERIFY(previousSpy.isValid());

    QTest::keyClicks(lineEdit, QStringLiteral("build"));
    QTRY_COMPARE(searchBar->searchText(), QStringLiteral("build"));
    QVERIFY(keywordSpy.count() > 0);

    QTest::mouseClick(nextButton, Qt::LeftButton);
    QCOMPARE(nextSpy.count(), 1);

    QTest::mouseClick(previousButton, Qt::LeftButton);
    QCOMPARE(previousSpy.count(), 1);
}

void TestMainWindow::testAccessibleVerticalSidebarButtonsActivateTargets() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);

    QVERIFY(QMetaObject::invokeMethod(&window, "onTabAddRequested", Qt::DirectConnection));
    QVERIFY(waitForTabCount(tabs, 2));

    auto *pane = currentPane(window);
    QVERIFY(pane);
    pane->setCustomTitle(QStringLiteral("Second Tab"));

    auto *verticalSidebar = sidebar(window);
    QVERIFY(verticalSidebar);
    QTRY_VERIFY(verticalSidebar->isVisible());

    auto *firstTabButton =
        findByAccessibleName<QAbstractButton>(verticalSidebar, QStringLiteral("Terminal tab: Terminal"));
    QVERIFY(firstTabButton);
    QTest::mouseClick(firstTabButton, Qt::LeftButton);
    QTRY_COMPARE(tabs->currentIndex(), 0);

    auto *secondTabButton =
        findByAccessibleName<QAbstractButton>(verticalSidebar, QStringLiteral("Terminal tab: Second Tab"));
    QVERIFY(secondTabButton);
    QTest::mouseClick(secondTabButton, Qt::LeftButton);
    QTRY_COMPARE(tabs->currentIndex(), 1);

    pane = currentPane(window);
    QVERIFY(pane);
    pane->splitCurrent(Qt::Vertical);

    const auto paneInfos = pane->paneInfos();
    QVERIFY(paneInfos.size() >= 2);
    pane->setCustomTitle(QStringLiteral("Build Pane"));
    QVERIFY(pane->focusPane(paneInfos.first().id));

    auto *firstTabButtonAgain =
        findByAccessibleName<QAbstractButton>(verticalSidebar, QStringLiteral("Terminal tab: Terminal"));
    QVERIFY(firstTabButtonAgain);
    QTest::mouseClick(firstTabButtonAgain, Qt::LeftButton);
    QTRY_COMPARE(tabs->currentIndex(), 0);

    const auto paneList = verticalSidebar->findChildren<QAbstractButton *>(QStringLiteral("verticalPaneButton"));
    QVERIFY(paneList.size() >= 2);
}

void TestMainWindow::testAccessibleRemoteAddButtonOpensConfigDialog() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *remoteAction = window.findChild<QAction *>(QStringLiteral("remoteManagementAction"));
    QVERIFY(remoteAction);
    remoteAction->trigger();

    auto *panel = findByAccessibleName<RemoteManagementPanel>(&window, QStringLiteral("Remote management"));
    QVERIFY(panel);
    QTRY_VERIFY(panel->isVisible());

    auto *addButton = findByAccessibleName<QAbstractButton>(panel, QStringLiteral("Add remote server"));
    QVERIFY(addButton);

    bool sawDialog = false;
    QString dialogName;
    QString serverNameField;
    QTimer::singleShot(50, &window, [&]() {
        auto *dialog = window.findChild<ServerConfigOptDlg *>();
        if (!dialog)
            return;
        sawDialog = true;
        dialogName = accessibleText(dialog, QAccessible::Name);
        if (auto *serverName = dialog->findChild<QWidget *>(QStringLiteral("serverNameEdit")))
            serverNameField = accessibleText(serverName, QAccessible::Name);
        dialog->reject();
    });

    QTest::mouseClick(addButton, Qt::LeftButton);

    QVERIFY(sawDialog);
    QCOMPARE(dialogName, QStringLiteral("Add remote server"));
    QCOMPARE(serverNameField, QStringLiteral("Server name"));
}

void TestMainWindow::testShortcutViewerPayloadListsConfiguredActions() {
    ShortcutViewerCapture capture;
    QVERIFY(capture.install());

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&window));

    QTest::keyClick(&window, Qt::Key_Slash, Qt::ControlModifier | Qt::ShiftModifier);

    QTRY_VERIFY(QFile::exists(capture.outputPath));
    const QJsonObject payload = shortcutViewerPayload(readShortcutViewerArguments(capture.outputPath));
    capture.restore();

    const QJsonArray groups = payload.value(QStringLiteral("shortcut")).toArray();
    QCOMPARE(groups.size(), 3);

    QStringList groupNames;
    QStringList actions;
    for (const QJsonValue &groupValue : groups) {
        const QJsonObject group = groupValue.toObject();
        groupNames.append(group.value(QStringLiteral("groupName")).toString());
        const QJsonArray items = group.value(QStringLiteral("groupItems")).toArray();
        for (const QJsonValue &itemValue : items)
            actions.append(itemValue.toObject().value(QStringLiteral("name")).toString());
    }

    QVERIFY(groupNames.contains(QStringLiteral("Terminal")));
    QVERIFY(groupNames.contains(QStringLiteral("Tabs")));
    QVERIFY(groupNames.contains(QStringLiteral("Others")));
    QVERIFY(actions.contains(QStringLiteral("Copy")));
    QVERIFY(actions.contains(QStringLiteral("Paste")));
    QVERIFY(actions.contains(QStringLiteral("Find")));
    QVERIFY(actions.contains(QStringLiteral("New tab")));
    QVERIFY(actions.contains(QStringLiteral("Display shortcuts")));
    QVERIFY(actions.contains(QStringLiteral("Remote management")));
}

void TestMainWindow::testProcessIconsAreAvailable() {
    const QSet<QString> webpIcons = {
        QStringLiteral("claude"),   QStringLiteral("gemini"), QStringLiteral("codex"),   QStringLiteral("qwen"),
        QStringLiteral("opencode"), QStringLiteral("goose"),  QStringLiteral("copilot"), QStringLiteral("kimi"),
    };
    const QStringList iconNames = {
        QStringLiteral("codex"),          QStringLiteral("claude"),   QStringLiteral("gemini"),
        QStringLiteral("aider"),          QStringLiteral("opencode"), QStringLiteral("goose"),
        QStringLiteral("github-copilot"), QStringLiteral("qwen"),     QStringLiteral("shell"),
        QStringLiteral("docker"),         QStringLiteral("podman"),   QStringLiteral("kubernetes"),
        QStringLiteral("helm"),           QStringLiteral("vim"),      QStringLiteral("nvim"),
        QStringLiteral("nano"),           QStringLiteral("emacs"),    QStringLiteral("htop"),
        QStringLiteral("terminal"),
    };

    for (const QString &iconName : iconNames) {
        const QString badgeName = iconName == QStringLiteral("github-copilot") ? QStringLiteral("copilot") : iconName;
        const QString ext = webpIcons.contains(badgeName) ? QStringLiteral("webp") : QStringLiteral("svg");
        const QString path = QStringLiteral(":/badges/process/%1.%2").arg(badgeName, ext);
        QVERIFY2(!QIcon(path).isNull(), qPrintable(path));
    }
}

void TestMainWindow::testTerminalProcessBadgeHasVisibleColoredArtwork() {
    VerticalTabSidebar sidebar;
    VerticalTabSidebar::TabItem item;
    item.id = 1;
    item.title = QStringLiteral("Terminal");
    item.isCurrent = true;

    TermPane::PaneInfo pane;
    pane.id = QUuid::createUuid();
    pane.title = QStringLiteral("Terminal");
    pane.isActive = true;
    item.panes.append(pane);

    sidebar.setItems({item});
    sidebar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&sidebar));
    QCoreApplication::processEvents();

    auto *badge = sidebar.findChild<QLabel *>(QStringLiteral("verticalTabBadge"));
    QVERIFY(badge);

    const QImage image = badge->pixmap().toImage();
    QVERIFY(!image.isNull());

    QSet<QRgb> colors;
    bool hasVisibleAccent = false;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.alpha() > 0) {
                colors.insert(color.rgb());
                if (color.saturation() > 80 && color.value() > 110)
                    hasVisibleAccent = true;
            }
        }
    }

    QVERIFY(colors.size() > 2);
    QVERIFY(hasVisibleAccent);
    QCOMPARE(image.pixelColor(0, 0).alpha(), 0);
    QCOMPARE(image.pixelColor(image.width() - 1, 0).alpha(), 0);
    QCOMPARE(image.pixelColor(0, image.height() - 1).alpha(), 0);
    QCOMPARE(image.pixelColor(image.width() - 1, image.height() - 1).alpha(), 0);
}

void TestMainWindow::testThemeLoaderLoadsAllThemes() {
    auto themes = ThemeLoader::loadThemes();
    const auto themeFiles = QDir(QStringLiteral(":/themes")).entryList({QStringLiteral("*.json")}, QDir::Files);
    QCOMPARE(themes.size(), themeFiles.size());

    QStringList names;
    for (const auto &t : themes)
        names.append(t.name);

    QCOMPARE(names.removeDuplicates(), 0);
    QVERIFY(names.contains(QStringLiteral("dark")));
    QVERIFY(names.contains(QStringLiteral("light")));
    QVERIFY(names.contains(QStringLiteral("bim")));
    QVERIFY(names.contains(QStringLiteral("tomorrow-night-blue")));
    QVERIFY(names.contains(QStringLiteral("ocean-dark")));
    QVERIFY(names.contains(QStringLiteral("hybrid")));
    QVERIFY(names.contains(QStringLiteral("one-light")));
    QVERIFY(names.contains(QStringLiteral("classic-dark")));
}

void TestMainWindow::testThemeLoaderFindsThemeByName() {
    auto themes = ThemeLoader::loadThemes();

    auto bim = ThemeLoader::findTheme(themes, QStringLiteral("bim"));
    QCOMPARE(bim.name, QStringLiteral("bim"));
    QCOMPARE(bim.displayName, QStringLiteral("Bim"));
    QVERIFY(bim.isDark);
    QCOMPARE(bim.foreground, QColor(255, 213, 0));
    QCOMPARE(bim.background, QColor(1, 40, 73));

    auto light = ThemeLoader::findTheme(themes, QStringLiteral("one-light"));
    QVERIFY(!light.isDark);

    auto fallback = ThemeLoader::findTheme(themes, QStringLiteral("nonexistent"));
    QCOMPARE(fallback.name, QStringLiteral("dark"));
}

void TestMainWindow::testThemeSettingDefaultIsSystem() {
    auto *settings = AppSettings::instance();
    QCOMPARE(settings->colorScheme(), QStringLiteral("system"));
}

void TestMainWindow::testThemeChangeAppliesToAllTerminals() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *settings = AppSettings::instance();
    QSignalSpy spy(settings, &AppSettings::colorSchemeChanged);
    QVERIFY(spy.isValid());

    settings->setColorScheme(QStringLiteral("bim"));
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(settings->colorScheme(), QStringLiteral("bim"));

    auto *terminal = currentTerminal(window);
    QVERIFY(terminal);
    QTRY_COMPARE(terminal->debugAppliedForeground(), QColor(255, 213, 0));
    QTRY_COMPARE(terminal->debugAppliedBackground(), QColor(1, 40, 73));

    settings->setColorScheme(QStringLiteral("system"));
    QTRY_COMPARE(settings->colorScheme(), QStringLiteral("system"));
}

void TestMainWindow::testThemeMenuHoverPreviewsAndRestoresTheme() {
    auto *settings = AppSettings::instance();
    settings->setColorScheme(QStringLiteral("light"));

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *terminal = currentTerminal(window);
    QVERIFY(terminal);
    QTRY_COMPARE(terminal->debugAppliedForeground(), QColor(0, 0, 0));
    QTRY_COMPARE(terminal->debugAppliedBackground(), QColor(248, 248, 248));

    auto *tb = titlebar(window);
    QVERIFY(tb);
    auto *rootMenu = tb->menu();
    QVERIFY(rootMenu);
    auto *themeAction = findMenuActionByText(rootMenu, QStringLiteral("Theme"));
    QVERIFY(themeAction);
    auto *themeMenu = themeAction->menu();
    QVERIFY(themeMenu);

    auto *bimAction = findMenuActionByText(themeMenu, QStringLiteral("Bim"));
    QVERIFY(bimAction);
    QMetaObject::invokeMethod(themeMenu, "hovered", Qt::DirectConnection, Q_ARG(QAction *, bimAction));

    QCOMPARE(settings->colorScheme(), QStringLiteral("light"));
    QTRY_COMPARE(terminal->debugAppliedForeground(), QColor(255, 213, 0));
    QTRY_COMPARE(terminal->debugAppliedBackground(), QColor(1, 40, 73));

    QMetaObject::invokeMethod(themeMenu, "aboutToHide", Qt::DirectConnection);
    QTRY_COMPARE(terminal->debugAppliedForeground(), QColor(0, 0, 0));
    QTRY_COMPARE(terminal->debugAppliedBackground(), QColor(248, 248, 248));
    QCOMPARE(settings->colorScheme(), QStringLiteral("light"));

    QMetaObject::invokeMethod(themeMenu, "hovered", Qt::DirectConnection, Q_ARG(QAction *, bimAction));
    bimAction->trigger();
    QCOMPARE(settings->colorScheme(), QStringLiteral("bim"));
    QTRY_COMPARE(terminal->debugAppliedForeground(), QColor(255, 213, 0));
    QTRY_COMPARE(terminal->debugAppliedBackground(), QColor(1, 40, 73));
}

void TestMainWindow::testNextTabShortcutSwitchesInVerticalMode() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);

    QVERIFY(QMetaObject::invokeMethod(&window, "onTabAddRequested", Qt::DirectConnection));
    QVERIFY(waitForTabCount(tabs, 2));
    QCOMPARE(tabs->currentIndex(), 1);

    QTest::keyClick(&window, Qt::Key_Tab, Qt::ControlModifier);
    QTRY_COMPARE(tabs->currentIndex(), 0);

    QTest::keyClick(&window, Qt::Key_Tab, Qt::ControlModifier);
    QTRY_COMPARE(tabs->currentIndex(), 1);
}

void TestMainWindow::testPrevTabShortcutSwitchesInVerticalMode() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);

    QVERIFY(QMetaObject::invokeMethod(&window, "onTabAddRequested", Qt::DirectConnection));
    QVERIFY(waitForTabCount(tabs, 2));
    QCOMPARE(tabs->currentIndex(), 1);

    QTest::keyClick(&window, Qt::Key_Tab, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_COMPARE(tabs->currentIndex(), 0);

    QTest::keyClick(&window, Qt::Key_Tab, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_COMPARE(tabs->currentIndex(), 1);
}

void TestMainWindow::testGotoTabShortcutSwitchesInVerticalMode() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);

    QVERIFY(QMetaObject::invokeMethod(&window, "onTabAddRequested", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&window, "onTabAddRequested", Qt::DirectConnection));
    QVERIFY(waitForTabCount(tabs, 3));
    QCOMPARE(tabs->currentIndex(), 2);

    QTest::keyClick(&window, Qt::Key_1, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_COMPARE(tabs->currentIndex(), 0);

    QTest::keyClick(&window, Qt::Key_2, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_COMPARE(tabs->currentIndex(), 1);
}

void TestMainWindow::testVerticalSidebarAddTabButtonCreatesNewTab() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);

    auto *verticalSidebar = sidebar(window);
    QVERIFY(verticalSidebar);

    auto *addButton = verticalSidebar->findChild<QAbstractButton *>(QStringLiteral("verticalAddTabButton"));
    QVERIFY(addButton);
    QVERIFY(addButton->isVisible());

    QTest::mouseClick(addButton, Qt::LeftButton);
    QVERIFY(waitForTabCount(tabs, 2));
}

void triggerCommandSucceeded(TerminalWidget *terminal) {
    const QByteArray command = QByteArray("\033]777;ShellCommand=") + QByteArray("bWFrZQ==") + QByteArray("\033\\");
    QMetaObject::invokeMethod(terminal, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, command));
    QCoreApplication::processEvents();

    const QByteArray result = "\033]777;ShellCommandResult=0\033\\";
    QMetaObject::invokeMethod(terminal, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, result));

    const QByteArray clear = "\033]777;ShellCommand=\033\\";
    QMetaObject::invokeMethod(terminal, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, clear));
    QCoreApplication::processEvents();
}

int visibleCommandStatusDotCount(QWidget *root) {
    int count = 0;
    for (auto *dot : root->findChildren<QLabel *>(QStringLiteral("commandStatusDot"))) {
        if (dot->isVisibleTo(root))
            ++count;
    }
    return count;
}

void TestMainWindow::testCommandStatusDotNotShownAfterSwitchingFromTabWhereCommandRan() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = tabBar(window);
    QVERIFY(tabs);

    auto *pane = currentPane(window);
    QVERIFY(pane);

    triggerCommandSucceeded(pane->currentTerminal());
    QTRY_COMPARE(pane->currentTerminal()->property("commandState").toInt(),
                 static_cast<int>(TerminalWidget::CommandState::Succeeded));

    QVERIFY(QMetaObject::invokeMethod(&window, "onTabAddRequested", Qt::DirectConnection));
    QVERIFY(waitForTabCount(tabs, 2));
    QCoreApplication::processEvents();

    auto *verticalSidebar = sidebar(window);
    QVERIFY(verticalSidebar);
    const auto items = verticalSidebar->items();
    QCOMPARE(items.size(), 2);
    QVERIFY(!items.at(0).isCurrent);
    QVERIFY(!items.at(0).hasPendingCommandResult);
}

void TestMainWindow::testCommandStatusDotShownWhenCommandFinishesInBackgroundTab() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = tabBar(window);
    QVERIFY(tabs);

    QVERIFY(QMetaObject::invokeMethod(&window, "onTabAddRequested", Qt::DirectConnection));
    QVERIFY(waitForTabCount(tabs, 2));

    auto *verticalSidebar = sidebar(window);
    QVERIFY(verticalSidebar);

    int firstTabIndex = tabs->currentIndex() == 0 ? 0 : 1;
    tabs->setCurrentIndex(firstTabIndex);
    QCoreApplication::processEvents();

    auto *pane = currentPane(window);
    QVERIFY(pane);

    const QByteArray command = QByteArray("\033]777;ShellCommand=") + QByteArray("bWFrZQ==") + QByteArray("\033\\");
    QMetaObject::invokeMethod(pane->currentTerminal(), "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, command));
    QCoreApplication::processEvents();

    int secondTabIndex = firstTabIndex == 0 ? 1 : 0;
    tabs->setCurrentIndex(secondTabIndex);
    QCoreApplication::processEvents();

    auto *firstPane = qobject_cast<TermPane *>(window.findChild<QStackedWidget *>()->widget(firstTabIndex));
    QVERIFY(firstPane);
    auto *firstTerminal = firstPane->currentTerminal();
    QVERIFY(firstTerminal);

    const QByteArray result = "\033]777;ShellCommandResult=0\033\\";
    QMetaObject::invokeMethod(firstTerminal, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, result));
    const QByteArray clear = "\033]777;ShellCommand=\033\\";
    QMetaObject::invokeMethod(firstTerminal, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, clear));
    QCoreApplication::processEvents();

    QTRY_VERIFY_WITH_TIMEOUT(verticalSidebar->items().size() > static_cast<int>(firstTabIndex)
                                 && verticalSidebar->items().at(firstTabIndex).hasPendingCommandResult,
                             1000);
}

void TestMainWindow::testCommandStatusDotNotShownForInactivePaneInCurrentTab() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *pane = currentPane(window);
    QVERIFY(pane);
    pane->splitCurrent(Qt::Vertical);
    QCoreApplication::processEvents();

    const auto panes = pane->paneInfos();
    QCOMPARE(panes.size(), 2);

    const QUuid shellPaneId = pane->activePaneId();
    auto *shellTerminal = pane->currentTerminal();
    QVERIFY(shellTerminal);

    triggerCommandSucceeded(shellTerminal);
    QTRY_COMPARE(shellTerminal->property("commandState").toInt(),
                 static_cast<int>(TerminalWidget::CommandState::Succeeded));

    const QUuid otherPaneId = panes.first().id == shellPaneId ? panes.last().id : panes.first().id;
    QVERIFY(pane->focusPane(otherPaneId));
    QCoreApplication::processEvents();

    auto *verticalSidebar = sidebar(window);
    QVERIFY(verticalSidebar);
    QCOMPARE(verticalSidebar->items().size(), 1);
    QVERIFY(verticalSidebar->items().first().isCurrent);
    QTRY_COMPARE(visibleCommandStatusDotCount(verticalSidebar), 0);
}

void TestMainWindow::testPaneActivationDoesNotClearCommandState() {
    AppSettings::instance()->setVerticalTabsEnabled(true);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *pane = currentPane(window);
    QVERIFY(pane);
    pane->splitCurrent(Qt::Vertical);
    QCoreApplication::processEvents();

    const auto panes = pane->paneInfos();
    QCOMPARE(panes.size(), 2);

    const QUuid shellPaneId = pane->activePaneId();
    auto *shellTerminal = pane->currentTerminal();
    QVERIFY(shellTerminal);

    triggerCommandSucceeded(shellTerminal);
    QTRY_COMPARE(shellTerminal->property("commandState").toInt(),
                 static_cast<int>(TerminalWidget::CommandState::Succeeded));

    const QUuid otherPaneId = panes.first().id == shellPaneId ? panes.last().id : panes.first().id;
    QVERIFY(pane->focusPane(otherPaneId));
    QCoreApplication::processEvents();

    QSignalSpy commandStateSpy(shellTerminal, &TerminalWidget::commandStateChanged);
    QVERIFY(commandStateSpy.isValid());

    QVERIFY(pane->focusPane(shellPaneId));
    QCoreApplication::processEvents();

    QCOMPARE(commandStateSpy.count(), 0);
    QCOMPARE(shellTerminal->property("commandState").toInt(),
             static_cast<int>(TerminalWidget::CommandState::Succeeded));
}

void TestMainWindow::testQuakeWindowUsesTopScreenGeometry() {
    QuakeWindow window;
    const QRect target = window.targetGeometry();
    const QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QVERIFY(screen);

    const QRect available = screen->availableGeometry();
    QCOMPARE(target.x(), available.x());
    QCOMPARE(target.y(), available.y());
    QCOMPARE(target.width(), available.width());
    QCOMPARE(target.height(), qMax(1, available.height() * 2 / 5));
}

void TestMainWindow::testQuakeWindowPresentationFlags() {
    QuakeWindow window;

    QVERIFY(window.isQuakeMode());
    QVERIFY(window.windowFlags() & Qt::WindowStaysOnTopHint);
    QVERIFY(titlebar(window));
    QCOMPARE(titlebar(window)->height(), 0);
}

void TestMainWindow::testQuakeWindowShowAndHideUseTargetGeometry() {
    QuakeWindow window;
    window.debugSetAnimationDuration(0);

    window.hide();
    window.showQuake();
    QTRY_VERIFY(window.isVisible());
    QCOMPARE(window.geometry(), window.targetGeometry());

    window.hideQuake();
    QTRY_VERIFY(!window.isVisible());
}

void TestMainWindow::testQuakeWindowFocusLossHideHonorsSetting() {
    QuakeWindow window;
    window.debugSetAnimationDuration(0);
    window.showQuake();
    QTRY_VERIFY(window.isVisible());

    window.debugHandleActivationChange(false);
    QTRY_VERIFY(!window.isVisible());

    AppSettings::instance()->dsettings()->setOption(QStringLiteral("advanced.window.hideQuakeOnFocusLoss"), false);
    window.showQuake();
    QTRY_VERIFY(window.isVisible());

    window.debugHandleActivationChange(false);
    QVERIFY(window.isVisible());
}

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
    QVERIFY2(SessionManager::instance().hasSnapshot(), "SessionManager::save() failed — snapshot not written");

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 1);

    SessionManager::instance().clearSnapshot();
}

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

void TestMainWindow::testRestoreSessionMenuActionEnabledWithSnapshot() {
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
    QVERIFY2(SessionManager::instance().hasSnapshot(), "SessionManager::save() failed — snapshot not written");

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *action = window.findChild<QAction *>(QStringLiteral("restoreSessionAction"));
    QVERIFY(action);
    QVERIFY(action->isEnabled());

    SessionManager::instance().clearSnapshot();
}

void TestMainWindow::testControlServiceListsWindowTabsPanesAndContent() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *term = currentTerminal(window);
    QVERIFY(term);
    term->importVtContent(QByteArrayLiteral("agent-visible-content\n"));

    TerminalControlService service;
    const QJsonObject response = parseControlResponse(service.list());
    QVERIFY(response.value(QStringLiteral("ok")).toBool());

    const QJsonArray windows = response.value(QStringLiteral("windows")).toArray();
    QVERIFY(!windows.isEmpty());
    const QJsonObject windowObject = windows.first().toObject();
    QVERIFY(!windowObject.value(QStringLiteral("id")).toString().isEmpty());

    const QJsonArray tabs = windowObject.value(QStringLiteral("tabs")).toArray();
    QCOMPARE(tabs.size(), 1);
    const QJsonObject tabObject = tabs.first().toObject();
    QCOMPARE(tabObject.value(QStringLiteral("active")).toBool(), true);

    const QJsonArray panes = tabObject.value(QStringLiteral("panes")).toArray();
    QCOMPARE(panes.size(), 1);
    const QJsonObject paneObject = panes.first().toObject();
    QVERIFY(!paneObject.value(QStringLiteral("id")).toString().isEmpty());
    QCOMPARE(paneObject.value(QStringLiteral("active")).toBool(), true);
    QVERIFY(paneObject.value(QStringLiteral("content")).toString().contains(QStringLiteral("agent-visible-content")));
}

void TestMainWindow::testControlServiceCreatesTabAndSplit() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    TerminalControlService service;
    QJsonObject response = parseControlResponse(service.list());
    const QString windowId =
        response.value(QStringLiteral("windows")).toArray().first().toObject().value(QStringLiteral("id")).toString();
    QVERIFY(!windowId.isEmpty());

    response = parseControlResponse(service.newTab(windowId));
    QVERIFY(response.value(QStringLiteral("ok")).toBool());
    auto *tabs = tabBar(window);
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 2);

    const QString paneId = firstPaneIdFromControlResponse(parseControlResponse(service.list()));
    response = parseControlResponse(service.split(paneId, QStringLiteral("horizontal")));
    QVERIFY(response.value(QStringLiteral("ok")).toBool());

    response = parseControlResponse(service.list());
    const QJsonArray tabsAfterSplit =
        response.value(QStringLiteral("windows")).toArray().first().toObject().value(QStringLiteral("tabs")).toArray();
    bool foundSplitTab = false;
    for (const auto &tabValue : tabsAfterSplit) {
        if (tabValue.toObject().value(QStringLiteral("panes")).toArray().size() == 2) {
            foundSplitTab = true;
            break;
        }
    }
    QVERIFY(foundSplitTab);
}

void TestMainWindow::testControlServiceSendsTextAndExecutesCommand() {
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    TerminalControlService service;
    const QString paneId = firstPaneIdFromControlResponse(parseControlResponse(service.list()));
    auto *pane = currentPane(window);
    QVERIFY(pane);
    auto *term = terminalForPaneId(*pane, QUuid::fromString(QStringLiteral("{%1}").arg(paneId)));
    QVERIFY(term);
    auto *session = ptySession(term);
    QVERIFY(session);
    QSignalSpy spy(session, &PtySession::dataWritten);

    QJsonObject response = parseControlResponse(service.send(paneId, QStringLiteral("typed text")));
    QVERIFY(response.value(QStringLiteral("ok")).toBool());
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 500);
    QCOMPARE(spy.takeFirst().at(0).toByteArray(), QByteArrayLiteral("typed text"));

    response = parseControlResponse(service.exec(paneId, QStringLiteral("printf control-ok")));
    QVERIFY(response.value(QStringLiteral("ok")).toBool());
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 500);
    QCOMPARE(spy.takeFirst().at(0).toByteArray(), QByteArrayLiteral("printf control-ok\n"));
}

int main(int argc, char *argv[]) {
    DApplication app(argc, argv);
    applyApplicationMetadata(app);
    TestMainWindow tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_main_window.moc"
