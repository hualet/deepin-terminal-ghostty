#include "SessionManager.h"

#include "TerminalWidget.h"
#include "logging/Logging.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

SessionManager &SessionManager::instance() {
    static SessionManager mgr;
    return mgr;
}

QDir SessionManager::sessionDir() const {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/session");
}

void SessionManager::ensureSessionDir() const {
    QDir dir = sessionDir();
    if (!dir.exists())
        dir.mkpath(".");
}

bool SessionManager::hasSnapshot() const {
    return QFile::exists(sessionDir().filePath("snapshot.json"));
}

WindowSnapshot SessionManager::loadSnapshot() const {
    QFile file(sessionDir().filePath("snapshot.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return {};
    return windowSnapshotFromJson(doc.object());
}

QByteArray SessionManager::loadVtContent(const QString &uuid) const {
    QFile file(sessionDir().filePath("terminals/" + uuid + ".vt"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

void SessionManager::save(const WindowSnapshot &snapshot, const QList<QPair<QString, TerminalWidget *>> &terminals) {
    clearSnapshot();
    ensureSessionDir();

    QJsonObject root = windowSnapshotToJson(snapshot);
    root["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonDocument doc(root);

    QFile file(sessionDir().filePath("snapshot.json"));
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(appLog) << "Failed to write session snapshot";
        return;
    }
    file.write(doc.toJson());

    QDir termDir = sessionDir();
    termDir.mkpath("terminals");
    for (const auto &[uuid, term] : terminals) {
        QByteArray vtData = term->exportVtContent();
        if (vtData.isEmpty())
            continue;
        QFile vtFile(termDir.filePath("terminals/" + uuid + ".vt"));
        if (vtFile.open(QIODevice::WriteOnly))
            vtFile.write(vtData);
    }
}

void SessionManager::clearSnapshot() {
    QDir dir = sessionDir();
    if (!dir.exists())
        return;
    dir.remove("snapshot.json");
    QDir termDir = sessionDir().filePath("terminals");
    if (termDir.exists()) {
        QStringList vtFiles = termDir.entryList(QStringList("*.vt"), QDir::Files);
        for (const QString &f : vtFiles)
            termDir.remove(f);
    }
}

QString SessionManager::snapshotTimestamp() const {
    QFile file(sessionDir().filePath("snapshot.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return {};
    return doc.object()["timestamp"].toString();
}
