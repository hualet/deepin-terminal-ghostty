#include "PtySession.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSignalSpy>
#include <QTest>

class TestPtySession : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {}
    void cleanupTestCase() {}

    void testStartShell();
    void testWriteAndRead();
    void testResize();
    void testSessionClose();
};

void TestPtySession::testStartShell() {
    PtySession session;
    QVERIFY(session.start(80, 24));
}

void TestPtySession::testWriteAndRead() {
    PtySession session;
    QVERIFY(session.start(80, 24));

    QSignalSpy spy(&session, &PtySession::dataReceived);
    QVERIFY(spy.isValid());

    // Write a simple command that prints something predictable
    session.write("echo 'qtghostty_test_hello'\n");

    // Wait for data (up to 2 seconds)
    bool received = spy.wait(2000);
    QVERIFY2(received, "Expected dataReceived signal within 2 seconds");

    // Verify we received something
    QVERIFY(spy.count() >= 1);

    // Check that the output contains our test string
    bool found = false;
    for (const auto &args : spy) {
        QByteArray data = args.at(0).toByteArray();
        if (data.contains("qtghostty_test_hello")) {
            found = true;
            break;
        }
    }
    QVERIFY2(found, "Expected output to contain 'qtghostty_test_hello'");
}

void TestPtySession::testResize() {
    PtySession session;
    QVERIFY(session.start(80, 24));

    // Resize should not crash
    session.resize(120, 40, 10, 20);
    session.resize(40, 10, 5, 10);

    // Verify session still works after resize
    QSignalSpy spy(&session, &PtySession::dataReceived);
    session.write("echo 'resize_ok'\n");
    QVERIFY(spy.wait(2000));
}

void TestPtySession::testSessionClose() {
    PtySession session;
    QVERIFY(session.start(80, 24));

    QSignalSpy closedSpy(&session, &PtySession::sessionClosed);
    QVERIFY(closedSpy.isValid());

    // Exit the shell
    session.write("exit\n");

    // Wait for session closed signal (up to 3 seconds)
    bool closed = closedSpy.wait(3000);
    QVERIFY2(closed, "Expected sessionClosed signal after exiting shell");
    QCOMPARE(closedSpy.count(), 1);
}

QTEST_MAIN(TestPtySession)
#include "test_pty_session.moc"
