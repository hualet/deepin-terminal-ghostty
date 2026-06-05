#include "StartupOptions.h"

#include <QTest>

class TestStartupOptions : public QObject {
    Q_OBJECT

private slots:
    void testDefaults();
    void testHelpOption();
    void testExecuteAndWorkingDirectory();
    void testPropagateExitCodeImpliesWait();
    void testQuakeModeOption();
    void testShortExecuteOption();
    void testShortWorkingDirectoryOption();
    void testShortQuakeModeOption();
    void testOldWorkDirectoryOption();
    void testTraceVtOption();
};

void TestStartupOptions::testDefaults() {
    const StartupOptions options = parseStartupOptions(QStringList{QStringLiteral("deepin-terminal-ghostty")});

    QVERIFY(options.isValid);
    QVERIFY(options.execute.isEmpty());
    QVERIFY(options.workingDirectory.isEmpty());
    QVERIFY(!options.waitForChild);
    QVERIFY(!options.propagateExitCode);
}

void TestStartupOptions::testHelpOption() {
    const StartupOptions shortHelp =
        parseStartupOptions(QStringList{QStringLiteral("deepin-terminal-ghostty"), QStringLiteral("-h")});

    QVERIFY(shortHelp.isValid);
    QVERIFY(shortHelp.showHelp);
    QVERIFY(shortHelp.helpText.contains(QStringLiteral("Usage: deepin-terminal-ghostty")));
    QVERIFY(shortHelp.helpText.contains(QStringLiteral("--execute")));
    QVERIFY(shortHelp.helpText.contains(QStringLiteral("--working-directory")));

    const StartupOptions longHelp =
        parseStartupOptions(QStringList{QStringLiteral("deepin-terminal-ghostty"), QStringLiteral("--help")});

    QVERIFY(longHelp.isValid);
    QVERIFY(longHelp.showHelp);
    QCOMPARE(longHelp.helpText, shortHelp.helpText);
}

void TestStartupOptions::testExecuteAndWorkingDirectory() {
    const StartupOptions options = parseStartupOptions(
        QStringList{QStringLiteral("deepin-terminal-ghostty"), QStringLiteral("--execute"),
                    QStringLiteral("printf 'hello'"), QStringLiteral("--working-directory"), QStringLiteral("/tmp")});

    QVERIFY(options.isValid);
    QCOMPARE(options.execute, QStringLiteral("printf 'hello'"));
    QCOMPARE(options.workingDirectory, QStringLiteral("/tmp"));
    QVERIFY(!options.waitForChild);
    QVERIFY(!options.propagateExitCode);
}

void TestStartupOptions::testPropagateExitCodeImpliesWait() {
    const StartupOptions options = parseStartupOptions(
        QStringList{QStringLiteral("deepin-terminal-ghostty"), QStringLiteral("--propagate-exit-code")});

    QVERIFY(options.isValid);
    QVERIFY(options.waitForChild);
    QVERIFY(options.propagateExitCode);
}

void TestStartupOptions::testQuakeModeOption() {
    const StartupOptions options =
        parseStartupOptions(QStringList{QStringLiteral("deepin-terminal-ghostty"), QStringLiteral("--quake-mode")});

    QVERIFY(options.isValid);
    QVERIFY(options.quakeMode);
}

void TestStartupOptions::testShortExecuteOption() {
    const StartupOptions options = parseStartupOptions(
        QStringList{QStringLiteral("deepin-terminal-ghostty"), QStringLiteral("-e"), QStringLiteral("ls -la")});

    QVERIFY(options.isValid);
    QCOMPARE(options.execute, QStringLiteral("ls -la"));
}

void TestStartupOptions::testShortWorkingDirectoryOption() {
    const StartupOptions options = parseStartupOptions(
        QStringList{QStringLiteral("deepin-terminal-ghostty"), QStringLiteral("-w"), QStringLiteral("/tmp")});

    QVERIFY(options.isValid);
    QCOMPARE(options.workingDirectory, QStringLiteral("/tmp"));
}

void TestStartupOptions::testShortQuakeModeOption() {
    const StartupOptions options =
        parseStartupOptions(QStringList{QStringLiteral("deepin-terminal-ghostty"), QStringLiteral("-q")});

    QVERIFY(options.isValid);
    QVERIFY(options.quakeMode);
}

void TestStartupOptions::testOldWorkDirectoryOption() {
    const StartupOptions options = parseStartupOptions(QStringList{
        QStringLiteral("deepin-terminal-ghostty"), QStringLiteral("--work-directory"), QStringLiteral("/home/user")});

    QVERIFY(options.isValid);
    QCOMPARE(options.workingDirectory, QStringLiteral("/home/user"));
}

void TestStartupOptions::testTraceVtOption() {
    const StartupOptions options = parseStartupOptions(QStringList{
        QStringLiteral("deepin-terminal-ghostty"), QStringLiteral("--trace-vt"), QStringLiteral("/tmp/omp-vt.log")});

    QVERIFY(options.isValid);
    QCOMPARE(options.traceVtPath, QStringLiteral("/tmp/omp-vt.log"));

    const StartupOptions help =
        parseStartupOptions(QStringList{QStringLiteral("deepin-terminal-ghostty"), QStringLiteral("--help")});
    QVERIFY(help.helpText.contains(QStringLiteral("--trace-vt")));
}

QTEST_APPLESS_MAIN(TestStartupOptions)
#include "test_startup_options.moc"
