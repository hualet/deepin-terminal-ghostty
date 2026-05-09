#include "SessionSnapshot.h"

#include <QTest>

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
        SplitNode original =
            SplitNode::split(Qt::Vertical, {300, 500},
                             {SplitNode::terminal("a", "/tmp", "vim"), SplitNode::terminal("b", "/var", "top")});
        QJsonObject json = splitNodeToJson(original);
        SplitNode restored = splitNodeFromJson(json);
        QCOMPARE(restored.type, SplitNode::Type::Split);
        QCOMPARE(restored.orientation, Qt::Vertical);
        QCOMPARE(restored.sizes, (QList<int>{300, 500}));
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
            Qt::Vertical, {200, 300}, {SplitNode::terminal("c1", "/a", "vim"), SplitNode::terminal("c2", "/b", "top")});
        SplitNode original =
            SplitNode::split(Qt::Horizontal, {500, 500}, {SplitNode::terminal("root", "/home", "bash"), nested});
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
        QCOMPARE(restored.children[1].sizes, (QList<int>{200, 300}));
    }
};

QTEST_MAIN(TestSessionSnapshot)
#include "test_session_snapshot.moc"
