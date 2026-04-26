#pragma once

#include "TermPane.h"

#include <QList>
#include <QWidget>

class QVBoxLayout;

class VerticalTabSidebar : public QWidget {
    Q_OBJECT

public:
    struct TabItem {
        int id = 0;
        QString title;
        bool isCurrent = false;
        bool expanded = true;
        QList<TermPane::PaneInfo> panes;
    };

    explicit VerticalTabSidebar(QWidget *parent = nullptr);

    void setItems(const QList<TabItem> &items);
    QList<TabItem> items() const;
signals:
    void tabActivated(int tabId);
    void tabExpansionToggled(int tabId);
    void paneActivated(int tabId, const QUuid &paneId);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuild();
    void updateButtonElisions();
    void applyStylesheet();

    QList<TabItem> m_items;
    QVBoxLayout *m_layout = nullptr;
};
