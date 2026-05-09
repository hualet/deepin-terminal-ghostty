#pragma once

#include "SessionSnapshot.h"

#include <QDir>
#include <QList>
#include <QPair>
#include <QString>

class TerminalWidget;

class SessionManager {
public:
    static SessionManager &instance();

    bool hasSnapshot() const;
    WindowSnapshot loadSnapshot() const;
    QByteArray loadVtContent(const QString &uuid) const;
    void save(const WindowSnapshot &snapshot, const QList<QPair<QString, TerminalWidget *>> &terminals);
    void clearSnapshot();
    QString snapshotTimestamp() const;

private:
    SessionManager() = default;
    QDir sessionDir() const;
    void ensureSessionDir() const;
};
