#include "SessionSnapshot.h"

#include <QJsonArray>
#include <QJsonObject>

QJsonObject splitNodeToJson(const SplitNode &node) {
    QJsonObject obj;
    if (node.type == SplitNode::Type::Terminal) {
        obj["type"] = "terminal";
        obj["uuid"] = node.uuid;
        obj["workingDirectory"] = node.workingDirectory;
        obj["title"] = node.title;
    } else {
        obj["type"] = "split";
        obj["orientation"] = node.orientation == Qt::Horizontal ? "horizontal" : "vertical";
        QJsonArray sizesArr;
        for (int s : node.sizes)
            sizesArr.append(s);
        obj["sizes"] = sizesArr;
        QJsonArray childrenArr;
        for (const auto &child : node.children)
            childrenArr.append(splitNodeToJson(child));
        obj["children"] = childrenArr;
    }
    return obj;
}

SplitNode splitNodeFromJson(const QJsonObject &obj) {
    QString type = obj["type"].toString();
    if (type == "terminal") {
        return SplitNode::terminal(obj["uuid"].toString(), obj["workingDirectory"].toString(), obj["title"].toString());
    }
    Qt::Orientation orientation = obj["orientation"].toString() == "horizontal" ? Qt::Horizontal : Qt::Vertical;
    QList<int> sizes;
    QJsonArray sizesArr = obj["sizes"].toArray();
    for (const auto &v : sizesArr)
        sizes.append(v.toInt());
    QList<SplitNode> children;
    QJsonArray childrenArr = obj["children"].toArray();
    for (const auto &v : childrenArr)
        children.append(splitNodeFromJson(v.toObject()));
    return SplitNode::split(orientation, sizes, children);
}

QJsonObject tabSnapshotToJson(const TabSnapshot &tab) {
    QJsonObject obj;
    obj["id"] = tab.id;
    obj["title"] = tab.title;
    obj["pane"] = splitNodeToJson(tab.pane);
    return obj;
}

TabSnapshot tabSnapshotFromJson(const QJsonObject &obj) {
    TabSnapshot tab;
    tab.id = obj["id"].toInt();
    tab.title = obj["title"].toString();
    tab.pane = splitNodeFromJson(obj["pane"].toObject());
    return tab;
}

QJsonObject windowSnapshotToJson(const WindowSnapshot &snapshot) {
    QJsonObject window;
    window["width"] = snapshot.width;
    window["height"] = snapshot.height;
    window["isMaximized"] = snapshot.isMaximized;

    QJsonObject obj;
    obj["version"] = 1;
    obj["window"] = window;
    QJsonArray tabsArr;
    for (const auto &tab : snapshot.tabs)
        tabsArr.append(tabSnapshotToJson(tab));
    obj["tabs"] = tabsArr;
    return obj;
}

WindowSnapshot windowSnapshotFromJson(const QJsonObject &obj) {
    WindowSnapshot snapshot;
    QJsonObject window = obj["window"].toObject();
    snapshot.width = window["width"].toInt();
    snapshot.height = window["height"].toInt();
    snapshot.isMaximized = window["isMaximized"].toBool();
    QJsonArray tabsArr = obj["tabs"].toArray();
    for (const auto &v : tabsArr)
        snapshot.tabs.append(tabSnapshotFromJson(v.toObject()));
    return snapshot;
}
