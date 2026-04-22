#pragma once

#include "ServerConfig.h"

#include <DIconButton>
#include <DLabel>
#include <DPushButton>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

DWIDGET_USE_NAMESPACE

class ServerListItem;

class RemoteManagementPanel : public QWidget {
    Q_OBJECT

public:
    explicit RemoteManagementPanel(QWidget *parent = nullptr);
    ~RemoteManagementPanel() override;

    void showPanel();
    void hidePanel();
    bool isPanelVisible() const;

signals:
    void connectServer(const ServerConfig &config);
    void panelHidden();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onAddServerClicked();
    void onItemClicked(const QString &key);
    void onItemDoubleClicked(const QString &key);
    void refreshList();

private:
    void initUI();
    void animateShow();
    void animateHide();
    void updateEmptyState();
    void clearItems();

    QWidget *m_scrollContent = nullptr;
    QVBoxLayout *m_itemLayout = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    DPushButton *m_addButton = nullptr;
    DLabel *m_emptyIcon = nullptr;
    DLabel *m_emptyLabel = nullptr;
    QVBoxLayout *m_emptyLayout = nullptr;

    QPropertyAnimation *m_showAnim = nullptr;
    QPropertyAnimation *m_hideAnim = nullptr;

    QList<ServerListItem *> m_items;
    bool m_isVisible = false;
    static constexpr int kPanelWidth = 260;
};
