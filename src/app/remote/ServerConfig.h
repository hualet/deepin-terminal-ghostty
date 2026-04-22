#pragma once

#include <QString>

struct ServerConfig {
    QString m_serverName;
    QString m_address;
    QString m_userName;
    QString m_password;
    QString m_privateKey;
    QString m_port;
    QString m_group;
    QString m_path;
    QString m_command;
    QString m_encoding;
    QString m_backspaceKey;
    QString m_deleteKey;

    QString uniqueKey() const { return QString("%1@%2@%3@%4").arg(m_userName, m_address, m_port, m_serverName); }
};
