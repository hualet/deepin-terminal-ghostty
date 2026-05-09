#include "SessionManager.h"

#include <QStandardPaths>
#include <QTest>

class TestSessionManager : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        SessionManager::instance().clearSnapshot();
    }

    void testSaveAndLoadSnapshot() {
        WindowSnapshot original;
        original.width = 1024;
        original.height = 768;
        original.tabs.append({1, "test", SplitNode::terminal("u1", "/home", "sh")});

        auto &mgr = SessionManager::instance();
        mgr.save(original, {});

        QVERIFY(mgr.hasSnapshot());
        WindowSnapshot loaded = mgr.loadSnapshot();
        QCOMPARE(loaded.width, 1024);
        QCOMPARE(loaded.height, 768);
        QCOMPARE(loaded.tabs.size(), 1);
        QCOMPARE(loaded.tabs[0].pane.uuid, QStringLiteral("u1"));

        mgr.clearSnapshot();
        QVERIFY(!mgr.hasSnapshot());
    }

    void testClearRemovesAll() {
        WindowSnapshot snap;
        snap.tabs.append({1, "t", SplitNode::terminal("uuid-clear-test", "/", "")});
        auto &mgr = SessionManager::instance();
        mgr.save(snap, {});
        QVERIFY(mgr.hasSnapshot());
        mgr.clearSnapshot();
        QVERIFY(!mgr.hasSnapshot());
    }

    void testTimestampIsRecorded() {
        WindowSnapshot snap;
        snap.width = 800;
        snap.height = 600;
        auto &mgr = SessionManager::instance();
        mgr.save(snap, {});
        QVERIFY(!mgr.snapshotTimestamp().isEmpty());
        mgr.clearSnapshot();
    }

    void cleanupTestCase() { SessionManager::instance().clearSnapshot(); }
};

QTEST_MAIN(TestSessionManager)
#include "test_session_manager.moc"
