#include "remote/ServerConfigManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

namespace {

ServerConfig makeConfig(const QString &name, const QString &addr, const QString &user, const QString &port,
                        const QString &group = {}) {
    ServerConfig c;
    c.m_serverName = name;
    c.m_address = addr;
    c.m_userName = user;
    c.m_port = port;
    c.m_group = group;
    return c;
}

QString configFilePath() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)).filePath("server-config.conf");
}

void removeConfigFile() {
    QFile::remove(configFilePath());
}

} // namespace

class TestServerConfigManager : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void init() {
        delete ServerConfigManager::instance();
        removeConfigFile();
    }

    void cleanup() {
        delete ServerConfigManager::instance();
        removeConfigFile();
    }

    void testSaveAndGetConfig() {
        auto *mgr = ServerConfigManager::instance();
        ServerConfig cfg = makeConfig("myserver", "192.168.1.1", "root", "22");
        QString key = cfg.uniqueKey();

        mgr->saveServerConfig(cfg);

        QVERIFY(mgr->hasServerConfig(key));
        ServerConfig retrieved = mgr->getServerConfig(key);
        QCOMPARE(retrieved.m_serverName, QString("myserver"));
        QCOMPARE(retrieved.m_address, QString("192.168.1.1"));
        QCOMPARE(retrieved.m_userName, QString("root"));
        QCOMPARE(retrieved.m_port, QString("22"));
    }

    void testDeleteConfig() {
        auto *mgr = ServerConfigManager::instance();
        ServerConfig cfg = makeConfig("delserver", "10.0.0.1", "admin", "22");
        QString key = cfg.uniqueKey();

        mgr->saveServerConfig(cfg);
        QVERIFY(mgr->hasServerConfig(key));

        mgr->delServerConfig(key);
        QVERIFY(!mgr->hasServerConfig(key));
    }

    void testModifyConfigSameKey() {
        auto *mgr = ServerConfigManager::instance();
        ServerConfig cfg = makeConfig("modserver", "10.0.0.2", "user", "22");
        QString key = cfg.uniqueKey();

        mgr->saveServerConfig(cfg);

        ServerConfig modified = cfg;
        modified.m_address = "10.0.0.99";
        mgr->modifyServerConfig(modified, key);

        QVERIFY(!mgr->hasServerConfig(key));
        QVERIFY(mgr->hasServerConfig(modified.uniqueKey()));
        QCOMPARE(mgr->getServerConfig(modified.uniqueKey()).m_address, QString("10.0.0.99"));
    }

    void testModifyConfigDifferentKey() {
        auto *mgr = ServerConfigManager::instance();
        ServerConfig cfg = makeConfig("oldserver", "1.1.1.1", "user1", "2222");
        QString oldKey = cfg.uniqueKey();

        mgr->saveServerConfig(cfg);
        QVERIFY(mgr->hasServerConfig(oldKey));

        ServerConfig newCfg = makeConfig("newserver", "2.2.2.2", "user2", "3333");
        mgr->modifyServerConfig(newCfg, oldKey);

        QVERIFY(!mgr->hasServerConfig(oldKey));
        QVERIFY(mgr->hasServerConfig(newCfg.uniqueKey()));
        QCOMPARE(mgr->getServerConfig(newCfg.uniqueKey()).m_serverName, QString("newserver"));
    }

    void testGetServerConfigsGroupsByGroup() {
        auto *mgr = ServerConfigManager::instance();
        mgr->saveServerConfig(makeConfig("s1", "a1", "u1", "22", "grpA"));
        mgr->saveServerConfig(makeConfig("s2", "a2", "u2", "22", "grpB"));
        mgr->saveServerConfig(makeConfig("s3", "a3", "u3", "22", "grpA"));

        QMap<QString, QList<ServerConfig>> grouped = mgr->getServerConfigs();

        QCOMPARE(grouped["grpA"].size(), 2);
        QCOMPARE(grouped["grpB"].size(), 1);
    }

    void testGroupsReturnsUniqueGroups() {
        auto *mgr = ServerConfigManager::instance();
        mgr->saveServerConfig(makeConfig("s1", "a1", "u1", "22", "grpX"));
        mgr->saveServerConfig(makeConfig("s2", "a2", "u2", "22", "grpX"));
        mgr->saveServerConfig(makeConfig("s3", "a3", "u3", "22", "grpY"));

        QStringList grps = mgr->groups();

        QCOMPARE(grps.size(), 2);
        QVERIFY(grps.contains("grpX"));
        QVERIFY(grps.contains("grpY"));
    }

    void testServersInGroup() {
        auto *mgr = ServerConfigManager::instance();
        mgr->saveServerConfig(makeConfig("s1", "a1", "u1", "22", "grpA"));
        mgr->saveServerConfig(makeConfig("s2", "a2", "u2", "22", "grpB"));
        mgr->saveServerConfig(makeConfig("s3", "a3", "u3", "22", "grpA"));

        QList<ServerConfig> result = mgr->serversInGroup("grpA");
        QCOMPARE(result.size(), 2);
    }

    void testSearchServersByName() {
        auto *mgr = ServerConfigManager::instance();
        mgr->saveServerConfig(makeConfig("production-db", "10.0.0.1", "admin", "22"));
        mgr->saveServerConfig(makeConfig("staging-web", "10.0.0.2", "deploy", "22"));

        QList<ServerConfig> result = mgr->searchServers("production");
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.first().m_serverName, QString("production-db"));
    }

    void testSearchServersByAddress() {
        auto *mgr = ServerConfigManager::instance();
        mgr->saveServerConfig(makeConfig("s1", "192.168.1.100", "u1", "22"));
        mgr->saveServerConfig(makeConfig("s2", "10.0.0.5", "u2", "22"));

        QList<ServerConfig> result = mgr->searchServers("192.168");
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.first().m_address, QString("192.168.1.100"));
    }

    void testSearchServersByUserName() {
        auto *mgr = ServerConfigManager::instance();
        mgr->saveServerConfig(makeConfig("s1", "a1", "deploybot", "22"));
        mgr->saveServerConfig(makeConfig("s2", "a2", "admin", "22"));

        QList<ServerConfig> result = mgr->searchServers("deploy");
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.first().m_userName, QString("deploybot"));
    }

    void testSearchServersCaseInsensitive() {
        auto *mgr = ServerConfigManager::instance();
        mgr->saveServerConfig(makeConfig("MyServer", "10.0.0.1", "Admin", "22"));

        QList<ServerConfig> result = mgr->searchServers("myserver");
        QCOMPARE(result.size(), 1);

        result = mgr->searchServers("admin");
        QCOMPARE(result.size(), 1);
    }

    void testSearchServersWithGroupFilter() {
        auto *mgr = ServerConfigManager::instance();
        mgr->saveServerConfig(makeConfig("db", "10.0.0.1", "u1", "22", "production"));
        mgr->saveServerConfig(makeConfig("db", "10.0.0.2", "u2", "22", "staging"));

        QList<ServerConfig> result = mgr->searchServers("db", "production");
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.first().m_group, QString("production"));
    }

    void testServerConfigChangedSignal() {
        auto *mgr = ServerConfigManager::instance();
        QSignalSpy spy(mgr, &ServerConfigManager::serverConfigChanged);
        QVERIFY(spy.isValid());

        ServerConfig cfg = makeConfig("sigserver", "1.1.1.1", "u1", "22");
        mgr->saveServerConfig(cfg);
        QCOMPARE(spy.count(), 1);

        mgr->delServerConfig(cfg.uniqueKey());
        QCOMPARE(spy.count(), 2);

        mgr->saveServerConfig(cfg);
        spy.clear();
        mgr->modifyServerConfig(cfg, cfg.uniqueKey());
        QCOMPARE(spy.count(), 1);
    }

    void testInitServerConfigReadsFromFile() {
        QString path = configFilePath();
        QDir().mkpath(QFileInfo(path).absolutePath());

        ServerConfig cfg = makeConfig("fileserver", "172.16.0.1", "fileuser", "2222");
        cfg.m_group = "filegroup";
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.beginGroup(cfg.uniqueKey());
            settings.setValue("userName", cfg.m_userName);
            settings.setValue("address", cfg.m_address);
            settings.setValue("port", cfg.m_port);
            settings.setValue("Name", cfg.m_serverName);
            settings.setValue("GroupName", cfg.m_group);
            settings.endGroup();
            settings.sync();
        }

        auto *mgr = ServerConfigManager::instance();
        mgr->initServerConfig();

        QVERIFY(mgr->hasServerConfig(cfg.uniqueKey()));
        ServerConfig loaded = mgr->getServerConfig(cfg.uniqueKey());
        QCOMPARE(loaded.m_serverName, QString("fileserver"));
        QCOMPARE(loaded.m_address, QString("172.16.0.1"));
        QCOMPARE(loaded.m_userName, QString("fileuser"));
        QCOMPARE(loaded.m_port, QString("2222"));
        QCOMPARE(loaded.m_group, QString("filegroup"));
    }

    void testEmptyGroupNotInGroupsList() {
        auto *mgr = ServerConfigManager::instance();
        mgr->saveServerConfig(makeConfig("s1", "a1", "u1", "22", ""));
        mgr->saveServerConfig(makeConfig("s2", "a2", "u2", "22", "realgroup"));

        QStringList grps = mgr->groups();
        QVERIFY(!grps.contains(""));
        QCOMPARE(grps.size(), 1);
        QVERIFY(grps.contains("realgroup"));
    }
};

int main(int argc, char *argv[]) {
    const QByteArray testHome = "/tmp/deepin-terminal-ghostty-test-home-server-config";
    QDir(QString::fromLocal8Bit(testHome)).removeRecursively();
    qputenv("HOME", testHome);

    QCoreApplication app(argc, argv);
    TestServerConfigManager tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_server_config_manager.moc"
