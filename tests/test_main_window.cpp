#include "AppSettings.h"
#include "ApplicationMetadata.h"
#include "MainWindow.h"
#include "PtySession.h"
#include "StartupOptions.h"
#include "TermPane.h"
#include "TerminalWidget.h"
#include "VerticalTabSidebar.h"
#include "logging/Logging.h"

#include <DApplication>
#include <DSettings>
#include <DTabBar>
#include <DTitlebar>
#include <QAbstractButton>
#include <QAction>
#include <QFile>
#include <QPointer>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTest>

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
    }

    void cleanup() {
        AppSettings::releaseInstance();
        clearVerticalTabsSetting();
    }

    void testSingleTabCtrlDClosesWindow();
    void testClosedSessionRemovesOnlyCurrentTab();
    void testAltArrowWithKeypadModifierIsConsumedByPane();
    void testTermPaneReportsPaneSnapshotsAfterSplit();
    void testAppTerminalsSetTerminalContentMargins();
    void testVerticalTabsActionReflectsAndUpdatesSettings();
    void testVerticalTabsActionTracksExternalSettingChanges();
    void testVerticalTabsActionReflectsStartupSetting();
    void testVerticalSidebarShowsTabsAndPanes();
    void testActivePaneTitleUpdatesTabAndWindowTitles();
    void testSidebarExpansionSurvivesModeSwitch();
    void testHorizontalTitlebarTabsSurviveModeSwitch();
    void testVerticalSidebarTabClickSwitchesCurrentTab();
    void testVerticalSidebarIncludesDecorativeHierarchyElements();
    void testLoggingCategoriesExposeExpectedNames();
    void testApplicationMetadataIsConfigured();
    void testStartupSessionFinishedEmitsExitCode();
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

DTitlebar *titlebar(MainWindow &window) {
    return window.findChild<DTitlebar *>();
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

void TestMainWindow::testAppTerminalsSetTerminalContentMargins() {
    ExposedTermPane pane;
    pane.resize(1200, 800);
    pane.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pane));

    auto *terminal = pane.currentTerminal();
    QVERIFY(terminal);
    QCOMPARE(terminal->contentsMargins(), QMargins(4, 4, 4, 4));
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

    settings->dsettings()->setOption("basic.interface.verticalTabs", true);

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

int main(int argc, char *argv[]) {
    DApplication app(argc, argv);
    applyApplicationMetadata(app);
    TestMainWindow tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_main_window.moc"
