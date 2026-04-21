#include "MainWindow.h"
#include "PtySession.h"
#include "TermPane.h"
#include "TerminalWidget.h"

#include <DApplication>
#include <DTabBar>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTest>

DWIDGET_USE_NAMESPACE

class TestMainWindow : public QObject {
    Q_OBJECT

private slots:
    void testSingleTabCtrlDClosesWindow();
    void testClosedSessionRemovesOnlyCurrentTab();
};

namespace {

TerminalWidget *currentTerminal(MainWindow &window) {
    auto *stack = window.findChild<QStackedWidget *>();
    if (!stack)
        return nullptr;
    auto *pane = qobject_cast<TermPane *>(stack->currentWidget());
    if (!pane)
        return nullptr;
    return pane->currentTerminal();
}

DTabBar *tabBar(MainWindow &window) {
    return window.findChild<DTabBar *>();
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

int main(int argc, char *argv[]) {
    DApplication app(argc, argv);
    TestMainWindow tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_main_window.moc"
