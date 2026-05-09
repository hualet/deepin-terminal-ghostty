#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QtGlobal>

struct SplitNode {
    enum class Type { Terminal, Split };
    Type type = Type::Terminal;

    QString uuid;
    QString workingDirectory;
    QString title;

    Qt::Orientation orientation = Qt::Horizontal;
    QList<int> sizes;
    QList<SplitNode> children;

    static SplitNode terminal(const QString &uuid, const QString &workingDirectory, const QString &title) {
        return {.type = Type::Terminal, .uuid = uuid, .workingDirectory = workingDirectory, .title = title};
    }

    static SplitNode split(Qt::Orientation orientation, QList<int> sizes, QList<SplitNode> children) {
        return {.type = Type::Split,
                .orientation = orientation,
                .sizes = std::move(sizes),
                .children = std::move(children)};
    }
};

struct TabSnapshot {
    int id = 0;
    QString title;
    SplitNode pane;
};

struct WindowSnapshot {
    int width = 0;
    int height = 0;
    bool isMaximized = false;
    QList<TabSnapshot> tabs;
};

QJsonObject splitNodeToJson(const SplitNode &node);
SplitNode splitNodeFromJson(const QJsonObject &obj);

QJsonObject tabSnapshotToJson(const TabSnapshot &tab);
TabSnapshot tabSnapshotFromJson(const QJsonObject &obj);

QJsonObject windowSnapshotToJson(const WindowSnapshot &snapshot);
WindowSnapshot windowSnapshotFromJson(const QJsonObject &obj);
