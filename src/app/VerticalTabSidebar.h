#pragma once

#include "TermPane.h"

#include <QFrame>
#include <QList>
#include <QWidget>

class QVBoxLayout;

class VerticalTabSidebar;

class ClickableSection : public QFrame {
    Q_OBJECT

public:
    explicit ClickableSection(QWidget *parent = nullptr) : QFrame(parent) {}

    int tabId = 0;
    VerticalTabSidebar *sidebar = nullptr;

protected:
    void mousePressEvent(QMouseEvent *event) override;
};

class VerticalTabSidebar : public QWidget {
    Q_OBJECT

public:
    struct TabItem {
        int id = 0;
        QString title;
        bool isCurrent = false;
        bool expanded = true;
        QList<TermPane::PaneInfo> panes;
        bool hasPendingCommandResult = false;
    };

    explicit VerticalTabSidebar(QWidget *parent = nullptr);

    void setItems(const QList<TabItem> &items);
    QList<TabItem> items() const;

    void setOpacity(qreal opacity);

signals:
    void tabActivated(int tabId);
    void tabExpansionToggled(int tabId);
    void addTabRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void rebuild();
    void updateButtonElisions();
    void applyStylesheet();

    QList<TabItem> m_items;
    QVBoxLayout *m_layout = nullptr;
    qreal m_opacity = 1.0;
};
