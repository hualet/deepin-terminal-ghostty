#include "PtySession.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestPtySession : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {}
    void cleanupTestCase() {}

    void testStartShell();
    void testWriteAndRead();
    void testStartCommand();
    void testStartCommandInWorkingDirectory();
    void testReportsChildExitCode();
    void testBashShellIntegrationHookReportsCommands();
    void testZshShellIntegrationHookReportsCommands();
    void testResize();
    void testSessionClose();
    void testPreservesUtf8Locale();
    void testHasRunningProcessReturnsFalseForShell();
    void testHasRunningProcessTracksForegroundCommand();
    void testHasRunningProcessReturnsFalseAfterExit();
    void testWriteWhenNotStartedIsIgnored();
    void testResizeWhenNotStartedIsIgnored();
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

void TestPtySession::testStartCommand() {
    PtySession session;
    PtySession::StartOptions options;
    options.command = QStringLiteral("printf 'qtghostty_cmd_ok'");
    QVERIFY(session.start(80, 24, options));

    QSignalSpy spy(&session, &PtySession::dataReceived);
    QVERIFY(spy.isValid());

    QByteArray output;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 2000 && !output.contains("qtghostty_cmd_ok")) {
        spy.wait(100);
        for (const auto &args : spy)
            output.append(args.at(0).toByteArray());
        spy.clear();
    }

    QVERIFY2(output.contains("qtghostty_cmd_ok"),
             qPrintable(QString::fromLatin1("Expected command output, got: %1").arg(QString::fromUtf8(output))));
}

void TestPtySession::testStartCommandInWorkingDirectory() {
    QTemporaryDir workingDir;
    QVERIFY(workingDir.isValid());

    PtySession session;
    PtySession::StartOptions options;
    options.command = QStringLiteral("pwd");
    options.workingDirectory = workingDir.path();
    QVERIFY(session.start(80, 24, options));

    QSignalSpy spy(&session, &PtySession::dataReceived);
    QVERIFY(spy.isValid());

    QByteArray output;
    QElapsedTimer timer;
    timer.start();
    const QByteArray expectedPath = QFile::encodeName(workingDir.path());
    while (timer.elapsed() < 2000 && !output.contains(expectedPath)) {
        spy.wait(100);
        for (const auto &args : spy)
            output.append(args.at(0).toByteArray());
        spy.clear();
    }

    QVERIFY2(output.contains(expectedPath),
             qPrintable(
                 QString::fromLatin1("Expected working directory in output, got: %1").arg(QString::fromUtf8(output))));
}

void TestPtySession::testReportsChildExitCode() {
    PtySession session;
    PtySession::StartOptions options;
    options.command = QStringLiteral("cd /tmp && exit 23");
    QVERIFY(session.start(80, 24, options));

    QSignalSpy exitSpy(&session, &PtySession::childExited);
    QVERIFY(exitSpy.isValid());

    QVERIFY2(exitSpy.wait(3000), "Expected childExited signal within 3 seconds");
    QCOMPARE(exitSpy.count(), 1);
    QCOMPARE(exitSpy.at(0).at(0).toInt(), 23);
}

void verifyShellIntegrationHookReportsCommands(const QString &shellPath) {
    const QByteArray previousShell = qgetenv("SHELL");
    qputenv("SHELL", QFile::encodeName(shellPath));

    PtySession session;
    QVERIFY(session.start(80, 24));

    if (previousShell.isEmpty())
        qunsetenv("SHELL");
    else
        qputenv("SHELL", previousShell);

    QSignalSpy spy(&session, &PtySession::dataReceived);
    QVERIFY(spy.isValid());

    session.write("printf 'qtghostty_hook_ok'\n");

    QByteArray output;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 3000 && output.count("]777;ShellCommand=") < 2) {
        spy.wait(100);
        for (const auto &args : spy)
            output.append(args.at(0).toByteArray());
        spy.clear();
    }

    QVERIFY2(output.count("]777;ShellCommand=") >= 2,
             qPrintable(
                 QString::fromLatin1("Expected command start and clear OSC, got: %1").arg(QString::fromUtf8(output))));
}

void TestPtySession::testBashShellIntegrationHookReportsCommands() {
    if (!QFile::exists(QStringLiteral("/bin/bash")))
        QSKIP("bash is not available");

    verifyShellIntegrationHookReportsCommands(QStringLiteral("/bin/bash"));
}

void TestPtySession::testZshShellIntegrationHookReportsCommands() {
    if (!QFile::exists(QStringLiteral("/bin/zsh")))
        QSKIP("zsh is not available");

    verifyShellIntegrationHookReportsCommands(QStringLiteral("/bin/zsh"));
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

void TestPtySession::testPreservesUtf8Locale() {
    PtySession session;
    QVERIFY(session.start(80, 24));

    QSignalSpy spy(&session, &PtySession::dataReceived);
    QVERIFY(spy.isValid());

    session.write("locale charmap\n");
    QByteArray output;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 2000) {
        spy.wait(100);
        for (const auto &args : spy)
            output.append(args.at(0).toByteArray());
        spy.clear();

        if (output.contains("UTF-8") || output.contains("UTF8") || output.contains("ANSI_X3.4-1968"))
            break;
    }

    QVERIFY2(output.contains("UTF-8") || output.contains("UTF8"),
             qPrintable(QString::fromLatin1("Expected UTF-8 locale, got: %1").arg(QString::fromUtf8(output))));
}

void TestPtySession::testHasRunningProcessReturnsFalseForShell() {
    PtySession session;
    QVERIFY(session.start(80, 24));
    // The shell's startup (oh-my-zsh plugins, compinit, etc.) briefly runs
    // foreground commands such as `git --version`; wait for it to settle at
    // its prompt before asserting the idle state.
    QTRY_VERIFY_WITH_TIMEOUT(!session.hasRunningProcess(), 10000);
}

void TestPtySession::testHasRunningProcessTracksForegroundCommand() {
    PtySession session;
    QVERIFY(session.start(80, 24));
    // Start from a settled, idle shell so the foreground command below is the
    // only thing that can make hasRunningProcess() report true.
    QTRY_VERIFY_WITH_TIMEOUT(!session.hasRunningProcess(), 10000);

    // A single bounded foreground command (/usr/bin/sleep is an external
    // binary in its own process group) holds the foreground continuously, so
    // hasRunningProcess() reports true while it runs — no fork churn, no
    // transient sample that the check could pass on by luck.
    session.write("sleep 60\n");
    QTRY_VERIFY_WITH_TIMEOUT(session.hasRunningProcess(), 10000);

    // Stop the command. We must confirm it has actually exited, not merely
    // that hasRunningProcess() flipped false for a moment, so wait for a
    // sentinel echo that only reaches the PTY after the shell reclaims the
    // prompt — that requires sleep to have genuinely terminated.
    session.write("\x03"); // Ctrl-C ends sleep.
    session.write("echo __foreground_done__\n");

    QSignalSpy spy(&session, &PtySession::dataReceived);
    QVERIFY(spy.isValid());
    QByteArray collected;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10000) {
        spy.wait(200);
        for (const auto &args : spy)
            collected.append(args.at(0).toByteArray());
        spy.clear();
        if (collected.contains("__foreground_done__"))
            break;
    }
    QVERIFY2(collected.contains("__foreground_done__"), "foreground command did not return to the prompt within 10s");

    // The shell has reclaimed the prompt, so the foreground check is idle.
    // Retry to ride through transient prompt-render helpers an interactive
    // shell (oh-my-zsh: git status, etc.) briefly foregrounds at the prompt.
    QTRY_VERIFY_WITH_TIMEOUT(!session.hasRunningProcess(), 10000);
}

void TestPtySession::testHasRunningProcessReturnsFalseAfterExit() {
    PtySession session;
    PtySession::StartOptions opts;
    opts.command = QStringLiteral("true");
    QVERIFY(session.start(80, 24, opts));

    QSignalSpy spy(&session, &PtySession::sessionClosed);
    QVERIFY(spy.wait(3000));

    QCOMPARE(session.hasRunningProcess(), false);
}

void TestPtySession::testWriteWhenNotStartedIsIgnored() {
    PtySession session;
    session.write("test");
    QCOMPARE(session.hasRunningProcess(), false);
}

void TestPtySession::testResizeWhenNotStartedIsIgnored() {
    PtySession session;
    session.resize(80, 24, 10, 20);
    QCOMPARE(session.hasRunningProcess(), false);
}

QTEST_MAIN(TestPtySession)
#include "test_pty_session.moc"
