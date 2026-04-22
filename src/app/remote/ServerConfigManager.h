#pragma once

#include "ServerConfig.h"

#include <QMap>
#include <QObject>
#include <QSettings>

class ServerConfigManager : public QObject {
    Q_OBJECT

public:
    static ServerConfigManager *instance();
    ~ServerConfigManager() override;

    void initServerConfig();
    void saveServerConfig(const ServerConfig &config);
    void delServerConfig(const QString &key);
    void modifyServerConfig(const ServerConfig &newConfig, const QString &oldKey);

    QMap<QString, QList<ServerConfig>> getServerConfigs() const;
    ServerConfig getServerConfig(const QString &key) const;
    bool hasServerConfig(const QString &key) const;

    QStringList groups() const;
    QList<ServerConfig> serversInGroup(const QString &group) const;
    QList<ServerConfig> searchServers(const QString &filter, const QString &group = QString()) const;

signals:
    void serverConfigChanged();

private:
    explicit ServerConfigManager(QObject *parent = nullptr);

    static ServerConfigManager *s_instance;

    QString configFilePath() const;
    void writeConfig(const ServerConfig &config);
    void removeConfig(const QString &key);
    void readAllConfigs();

    QMap<QString, ServerConfig> m_configs;
};
