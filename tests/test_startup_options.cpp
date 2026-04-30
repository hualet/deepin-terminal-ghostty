#include "StartupOptions.h"

#include <QTest>

class TestStartupOptions : public QObject {
    Q_OBJECT

private slots:
    void testDefaults();
    void testHelpOption();
    void testExecuteAndWorkingDirectory();
    void testPropagateExitCodeImpliesWait();
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

QTEST_APPLESS_MAIN(TestStartupOptions)
#include "test_startup_options.moc"
