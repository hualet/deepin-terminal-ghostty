#pragma once

#include "TermPane.h"

#include <QFrame>
#include <QList>
#include <QPoint>
#include <QPointer>
#include <QWidget>

class QVBoxLayout;
class QLabel;
class QEvent;

class VerticalTabSidebar;

class ClickableSection : public QFrame {
    Q_OBJECT

public:
    explicit ClickableSection(QWidget *parent = nullptr) : QFrame(parent) {}

    int tabId = 0;
    VerticalTabSidebar *sidebar = nullptr;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QPoint m_dragStartPos;
    bool m_leftButtonPressed = false;
    bool m_dragging = false;
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
    bool isTabDragActive() const;
    void requestTabMove(int tabId, const QPoint &globalPos);
    void requestTabClose(int tabId);
    void beginTabDrag(int tabId, const QPoint &globalPos, const QPoint &hotSpot);
    void previewTabMove(int tabId, const QPoint &globalPos);
    bool finishTabDrag(int tabId, const QPoint &globalPos);

signals:
    void tabActivated(int tabId);
    void tabExpansionToggled(int tabId);
    void tabMoveRequested(int tabId, int targetIndex);
    void tabDragFinished();
    void addTabRequested();
    void tabCloseRequested(int tabId);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void rebuild();
    void updateButtonElisions();
    void applyStylesheet();
    int indexOfItemId(int tabId) const;
    int targetIndexForPosition(int tabId, const QPoint &globalPos) const;
    ClickableSection *sectionForTabId(int tabId) const;
    void moveDragProxy(const QPoint &globalPos);
    void clearDragProxy();

    QList<TabItem> m_items;
    QVBoxLayout *m_layout = nullptr;
    QPointer<QLabel> m_dragProxy;
    qreal m_opacity = 1.0;
    int m_dragTabId = 0;
    int m_dragOriginalIndex = -1;
    QPoint m_dragHotSpot;
    bool m_dragMoved = false;
};
