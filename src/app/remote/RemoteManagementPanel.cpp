#include "RemoteManagementPanel.h"

#include "ServerConfigManager.h"
#include "ServerConfigOptDlg.h"
#include "ServerListItem.h"

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QPropertyAnimation>
#include <QVBoxLayout>

RemoteManagementPanel::RemoteManagementPanel(QWidget *parent) : QWidget(parent) {
    setFixedWidth(kPanelWidth);
    setAttribute(Qt::WA_DeleteOnClose, false);
    initUI();
    refreshList();

    connect(ServerConfigManager::instance(), &ServerConfigManager::serverConfigChanged, this,
            &RemoteManagementPanel::refreshList);
}

RemoteManagementPanel::~RemoteManagementPanel() = default;

void RemoteManagementPanel::initUI() {
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->setContentsMargins(8, 12, 8, 8);

    // Scroll area for server items
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_scrollContent = new QWidget(this);
    m_itemLayout = new QVBoxLayout(m_scrollContent);
    m_itemLayout->setSpacing(4);
    m_itemLayout->setContentsMargins(0, 0, 0, 0);
    m_itemLayout->addStretch();
    m_scrollContent->setLayout(m_itemLayout);
    m_scrollArea->setWidget(m_scrollContent);
    layout->addWidget(m_scrollArea, 1);

    // Empty state
    m_emptyLayout = new QVBoxLayout();
    m_emptyLayout->addStretch();

    auto *iconLayout = new QHBoxLayout();
    iconLayout->addStretch();
    m_emptyIcon = new DLabel(this);
    m_emptyIcon->setPixmap(QIcon::fromTheme("folder-remote").pixmap(64, 64));
    iconLayout->addWidget(m_emptyIcon);
    iconLayout->addStretch();
    m_emptyLayout->addLayout(iconLayout);

    auto *textLayout = new QHBoxLayout();
    textLayout->addStretch();
    m_emptyLabel = new DLabel(tr("No servers yet"), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    textLayout->addWidget(m_emptyLabel);
    textLayout->addStretch();
    m_emptyLayout->addLayout(textLayout);

    m_emptyLayout->addStretch();
    layout->addLayout(m_emptyLayout);

    // Add button
    m_addButton = new DPushButton(tr("Add Server"), this);
    layout->addWidget(m_addButton);

    setLayout(layout);

    connect(m_addButton, &DPushButton::clicked, this, &RemoteManagementPanel::onAddServerClicked);
}

void RemoteManagementPanel::showPanel() {
    if (m_isVisible)
        return;
    m_isVisible = true;
    raise();
    animateShow();
    refreshList();
}

void RemoteManagementPanel::hidePanel() {
    if (!m_isVisible)
        return;
    m_isVisible = false;
    animateHide();
}

bool RemoteManagementPanel::isPanelVisible() const {
    return m_isVisible;
}

void RemoteManagementPanel::animateShow() {
    if (!parentWidget())
        return;

    QRect hostRect = parentWidget()->rect();
    int panelHeight = hostRect.height();
    setFixedHeight(panelHeight);

    setGeometry(hostRect.width(), 0, kPanelWidth, panelHeight);
    show();

    if (m_showAnim)
        m_showAnim->stop();
    if (m_hideAnim)
        m_hideAnim->stop();
    m_showAnim = new QPropertyAnimation(this, "geometry", this);
    m_showAnim->setDuration(250);
    m_showAnim->setEasingCurve(QEasingCurve::OutQuad);
    m_showAnim->setStartValue(QRect(hostRect.width(), 0, kPanelWidth, panelHeight));
    m_showAnim->setEndValue(QRect(hostRect.width() - kPanelWidth, 0, kPanelWidth, panelHeight));
    m_showAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void RemoteManagementPanel::animateHide() {
    if (!parentWidget())
        return;

    QRect hostRect = parentWidget()->rect();
    int panelHeight = hostRect.height();

    if (m_hideAnim)
        m_hideAnim->stop();
    if (m_showAnim)
        m_showAnim->stop();
    m_hideAnim = new QPropertyAnimation(this, "geometry", this);
    m_hideAnim->setDuration(250);
    m_hideAnim->setEasingCurve(QEasingCurve::OutQuad);
    m_hideAnim->setStartValue(QRect(hostRect.width() - kPanelWidth, 0, kPanelWidth, panelHeight));
    m_hideAnim->setEndValue(QRect(hostRect.width(), 0, kPanelWidth, panelHeight));
    connect(m_hideAnim, &QPropertyAnimation::finished, this, [this]() {
        hide();
        emit panelHidden();
    });
    m_hideAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void RemoteManagementPanel::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_isVisible && parentWidget()) {
        QRect hostRect = parentWidget()->rect();
        setGeometry(hostRect.width() - kPanelWidth, 0, kPanelWidth, hostRect.height());
    }
}

void RemoteManagementPanel::clearItems() {
    for (ServerListItem *item : m_items) {
        m_itemLayout->removeWidget(item);
        item->deleteLater();
    }
    m_items.clear();
}

void RemoteManagementPanel::refreshList() {
    clearItems();

    auto configs = ServerConfigManager::instance()->getServerConfigs();
    QList<ServerConfig> allServers;

    for (auto it = configs.cbegin(); it != configs.cend(); ++it) {
        for (const ServerConfig &config : it.value())
            allServers.append(config);
    }

    std::sort(allServers.begin(), allServers.end(),
              [](const ServerConfig &a, const ServerConfig &b) { return a.m_serverName < b.m_serverName; });

    for (const ServerConfig &config : allServers) {
        auto *item = new ServerListItem(config.m_serverName,
                                        QString("%1@%2:%3").arg(config.m_userName, config.m_address, config.m_port),
                                        config.uniqueKey(), m_scrollContent);
        connect(item, &ServerListItem::itemClicked, this, &RemoteManagementPanel::onItemClicked);
        connect(item, &ServerListItem::itemDoubleClicked, this, &RemoteManagementPanel::onItemDoubleClicked);
        m_itemLayout->insertWidget(m_itemLayout->count() - 1, item);
        m_items.append(item);
    }

    updateEmptyState();
}

void RemoteManagementPanel::updateEmptyState() {
    bool hasItems = !m_items.isEmpty();
    m_scrollArea->setVisible(hasItems);
    m_emptyIcon->setVisible(!hasItems);
    m_emptyLabel->setVisible(!hasItems);
}

void RemoteManagementPanel::onAddServerClicked() {
    ServerConfigOptDlg dlg(ServerConfigOptDlg::SCT_ADD, ServerConfig(), this);
    if (dlg.exec() == QDialog::Accepted) {
        ServerConfig config = dlg.getData();
        if (!config.m_serverName.isEmpty())
            ServerConfigManager::instance()->saveServerConfig(config);
    }
}

void RemoteManagementPanel::onItemClicked(const QString &key) {
    QMenu menu(this);
    auto *connectAction = menu.addAction(tr("Connect"));
    auto *editAction = menu.addAction(tr("Edit"));
    auto *deleteAction = menu.addAction(tr("Delete"));

    auto *selected = menu.exec(QCursor::pos());
    if (selected == connectAction) {
        ServerConfig config = ServerConfigManager::instance()->getServerConfig(key);
        if (!config.m_serverName.isEmpty()) {
            emit connectServer(config);
            hidePanel();
        }
    } else if (selected == editAction) {
        ServerConfig config = ServerConfigManager::instance()->getServerConfig(key);
        ServerConfigOptDlg dlg(ServerConfigOptDlg::SCT_MODIFY, config, this);
        if (dlg.exec() == QDialog::Accepted) {
            if (dlg.isDelServer()) {
                ServerConfigManager::instance()->delServerConfig(key);
            } else {
                ServerConfig newConfig = dlg.getData();
                if (!newConfig.m_serverName.isEmpty())
                    ServerConfigManager::instance()->modifyServerConfig(newConfig, key);
            }
        }
    } else if (selected == deleteAction) {
        ServerConfigManager::instance()->delServerConfig(key);
    }
}

void RemoteManagementPanel::onItemDoubleClicked(const QString &key) {
    ServerConfig config = ServerConfigManager::instance()->getServerConfig(key);
    if (!config.m_serverName.isEmpty()) {
        emit connectServer(config);
        hidePanel();
    }
}
