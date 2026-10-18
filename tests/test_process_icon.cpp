#include "ProcessIcon.h"

#include <QTest>

class TestProcessIcon : public QObject {
    Q_OBJECT

private slots:
    void testResolvesProcess_data();
    void testResolvesProcess();
};

void TestProcessIcon::testResolvesProcess_data() {
    QTest::addColumn<QString>("processName");
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<QString>("iconName");

    const QString none;

    QTest::newRow("comm") << "vim" << QStringList{"vim", "notes/kimi.md"} << "vim";
    QTest::newRow("comm ignores arguments") << "man" << QStringList{"man", "top"} << none;
    QTest::newRow("argument named like program") << "cat" << QStringList{"cat", "claude"} << none;
    QTest::newRow("absolute argv0") << "dockerd-x" << QStringList{"/usr/bin/docker", "compose", "up"} << "docker";
    QTest::newRow("dotted variant") << "vim.basic" << QStringList{"vim.basic", "file"} << "vim";
    QTest::newRow("process title") << "claude" << QStringList{"claude"} << "claude";
    QTest::newRow("shell") << "zsh" << QStringList{} << none;
    QTest::newRow("empty") << none << QStringList{} << none;

    QTest::newRow("node script") << "node"
                                 << QStringList{"node", "--no-warnings",
                                                "/usr/lib/node_modules/@openai/codex/bin/codex.js"}
                                 << "codex";
    QTest::newRow("node package entry") << "node"
                                        << QStringList{"node", "/usr/lib/node_modules/@google/gemini-cli/dist/index.js"}
                                        << "gemini";
    QTest::newRow("npx shim") << "node" << QStringList{"node", "/usr/bin/npx", "-y", "@openai/codex@latest"} << "codex";
    QTest::newRow("npm exec title") << "npm exec codex" << QStringList{"npm exec @anthropic-ai/claude-code"}
                                    << "claude";
    QTest::newRow("npm install") << "node" << QStringList{"node", "/usr/bin/npm", "install", "codex"} << none;
    QTest::newRow("uvx") << "uvx" << QStringList{"uvx", "kimi-cli"} << "kimi";
    QTest::newRow("python module") << "python3" << QStringList{"python3", "-m", "aider.main"} << "aider";
    QTest::newRow("python script") << "python3" << QStringList{"/usr/bin/python3", "/home/u/.local/bin/aider"}
                                   << "aider";
    QTest::newRow("python script args") << "python3" << QStringList{"python3", "tool.py", "-m", "aider"} << none;
    QTest::newRow("unknown interpreter target") << "node" << QStringList{"node", "server.js"} << none;
}

void TestProcessIcon::testResolvesProcess() {
    QFETCH(QString, processName);
    QFETCH(QStringList, arguments);
    QFETCH(QString, iconName);

    QCOMPARE(processIconName(processName, arguments), iconName);
}

QTEST_APPLESS_MAIN(TestProcessIcon)
#include "test_process_icon.moc"
