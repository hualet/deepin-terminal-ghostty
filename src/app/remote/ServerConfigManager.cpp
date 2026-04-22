#include "ServerConfigManager.h"

#include <QDir>
#include <QStandardPaths>

ServerConfigManager *ServerConfigManager::s_instance = nullptr;

ServerConfigManager::ServerConfigManager(QObject *parent) : QObject(parent) {}

ServerConfigManager *ServerConfigManager::instance() {
    if (!s_instance)
        s_instance = new ServerConfigManager();
    return s_instance;
}

ServerConfigManager::~ServerConfigManager() {
    if (s_instance == this)
        s_instance = nullptr;
}

QString ServerConfigManager::configFilePath() const {
    QDir configDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    if (!configDir.exists())
        configDir.mkpath(configDir.absolutePath());
    return configDir.filePath("server-config.conf");
}

void ServerConfigManager::initServerConfig() {
    readAllConfigs();
}

void ServerConfigManager::readAllConfigs() {
    m_configs.clear();

    QString path = configFilePath();
    if (!QFile::exists(path))
        return;

    QSettings settings(path, QSettings::IniFormat);

    const QStringList groups = settings.childGroups();
    for (const QString &group : groups) {
        settings.beginGroup(group);
        ServerConfig config;
        config.m_userName = settings.value("userName").toString();
        config.m_address = settings.value("address").toString();
        config.m_port = settings.value("port").toString();
        config.m_serverName = settings.value("Name").toString();
        config.m_password = settings.value("Password").toString();
        config.m_group = settings.value("GroupName").toString();
        config.m_command = settings.value("Command").toString();
        config.m_path = settings.value("Path").toString();
        config.m_encoding = settings.value("Encode").toString();
        config.m_backspaceKey = settings.value("Backspace").toString();
        config.m_deleteKey = settings.value("Del").toString();
        config.m_privateKey = settings.value("PrivateKey").toString();
        settings.endGroup();

        if (!config.m_serverName.isEmpty())
            m_configs.insert(config.uniqueKey(), config);
    }
}

void ServerConfigManager::writeConfig(const ServerConfig &config) {
    QSettings settings(configFilePath(), QSettings::IniFormat);
    settings.beginGroup(config.uniqueKey());
    settings.setValue("userName", config.m_userName);
    settings.setValue("address", config.m_address);
    settings.setValue("port", config.m_port);
    settings.setValue("Name", config.m_serverName);
    settings.setValue("Password", config.m_password);
    settings.setValue("GroupName", config.m_group);
    settings.setValue("Command", config.m_command);
    settings.setValue("Path", config.m_path);
    settings.setValue("Encode", config.m_encoding);
    settings.setValue("Backspace", config.m_backspaceKey);
    settings.setValue("Del", config.m_deleteKey);
    settings.setValue("PrivateKey", config.m_privateKey);
    settings.endGroup();
    settings.sync();
}

void ServerConfigManager::removeConfig(const QString &key) {
    QSettings settings(configFilePath(), QSettings::IniFormat);
    settings.remove(key);
    settings.sync();
}

void ServerConfigManager::saveServerConfig(const ServerConfig &config) {
    m_configs.insert(config.uniqueKey(), config);
    writeConfig(config);
    emit serverConfigChanged();
}

void ServerConfigManager::delServerConfig(const QString &key) {
    m_configs.remove(key);
    removeConfig(key);
    emit serverConfigChanged();
}

void ServerConfigManager::modifyServerConfig(const ServerConfig &newConfig, const QString &oldKey) {
    if (oldKey != newConfig.uniqueKey()) {
        m_configs.remove(oldKey);
        removeConfig(oldKey);
    }
    m_configs.insert(newConfig.uniqueKey(), newConfig);
    writeConfig(newConfig);
    emit serverConfigChanged();
}

QMap<QString, QList<ServerConfig>> ServerConfigManager::getServerConfigs() const {
    QMap<QString, QList<ServerConfig>> result;
    for (auto it = m_configs.cbegin(); it != m_configs.cend(); ++it) {
        result[it.value().m_group].append(it.value());
    }
    return result;
}

ServerConfig ServerConfigManager::getServerConfig(const QString &key) const {
    return m_configs.value(key);
}

bool ServerConfigManager::hasServerConfig(const QString &key) const {
    return m_configs.contains(key);
}

QStringList ServerConfigManager::groups() const {
    QStringList result;
    for (auto it = m_configs.cbegin(); it != m_configs.cend(); ++it) {
        QString group = it.value().m_group;
        if (!group.isEmpty() && !result.contains(group))
            result.append(group);
    }
    return result;
}

QList<ServerConfig> ServerConfigManager::serversInGroup(const QString &group) const {
    QList<ServerConfig> result;
    for (auto it = m_configs.cbegin(); it != m_configs.cend(); ++it) {
        if (it.value().m_group == group)
            result.append(it.value());
    }
    return result;
}

QList<ServerConfig> ServerConfigManager::searchServers(const QString &filter, const QString &group) const {
    QList<ServerConfig> result;
    for (auto it = m_configs.cbegin(); it != m_configs.cend(); ++it) {
        const ServerConfig &config = it.value();
        if (!group.isEmpty() && config.m_group != group)
            continue;
        if (config.m_serverName.contains(filter, Qt::CaseInsensitive)
            || config.m_userName.contains(filter, Qt::CaseInsensitive)
            || config.m_address.contains(filter, Qt::CaseInsensitive)) {
            result.append(config);
        }
    }
    return result;
}
