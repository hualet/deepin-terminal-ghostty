#include "MainWindow.h"

#include "AppSettings.h"
#include "SettingsDialog.h"
#include "TermPane.h"
#include "TerminalWidget.h"
#include "ThemeLoader.h"
#include "VerticalTabSidebar.h"
#include "logging/Logging.h"
#include "remote/RemoteManagementPanel.h"
#include "remote/ServerConfig.h"
#include "remote/ServerConfigManager.h"

#include <DAboutDialog>
#include <DApplication>
#include <DDialog>
#include <DGuiApplicationHelper>
#include <DTitlebar>
#include <DWindowManagerHelper>
#include <QActionGroup>
#include <QDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

MainWindow::MainWindow(const StartupOptions &startupOptions, QWidget *parent)
    : DMainWindow(parent),
      m_stackWidget(new QStackedWidget(this)),
      m_contentHost(new QWidget(this)),
      m_startupOptions(startupOptions) {
    m_verticalTabsEnabled = AppSettings::instance()->verticalTabsEnabled();

    // Prevent DTK or Qt default actions from intercepting standard
    // terminal keybindings (Ctrl+A–Z). In a terminal every Ctrl+letter
    // combo must be sent to the PTY as a C0 control character.
    for (QAction *action : findChildren<QAction *>()) {
        QKeySequence seq = action->shortcut();
        if (seq.isEmpty())
            continue;
        int key = seq[0].key();
        Qt::KeyboardModifiers mods = seq[0].keyboardModifiers();
        if ((mods & Qt::ControlModifier) && !(mods & Qt::ShiftModifier) && key >= Qt::Key_A && key <= Qt::Key_Z) {
            action->setShortcut(QKeySequence());
        }
    }

    // Window basics
    QSize savedSize = AppSettings::instance()->windowSize();
    resize(savedSize.isValid() ? savedSize : QSize(960, 640));
    setWindowTitle("deepin-terminal-ghostty");

    // Central widget host keeps layout switching local to MainWindow.
    setCentralWidget(m_contentHost);
    rebuildCentralLayout();

    // Hide remote panel when focus moves outside of it.
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (!m_remotePanel || !m_remotePanel->isPanelVisible())
            return;
        if (now && m_remotePanel->isAncestorOf(now))
            return;
        if (QApplication::activePopupWidget())
            return;
        if (QApplication::activeModalWidget())
            return;
        m_remotePanel->hidePanel();
    });

    // Remote management panel overlays the right edge of the content host.
    m_remotePanel = new RemoteManagementPanel(m_contentHost);
    m_remotePanel->hide();
    connect(m_remotePanel, &RemoteManagementPanel::connectServer, this, &MainWindow::onConnectRemoteServer);

    // Titlebar: icon + tabs
    setupTitleBar();

    // Connect global settings changes to all terminals
    auto *settings = AppSettings::instance();
    connect(settings, &AppSettings::terminalFontChanged, this, [this](const QFont &font) {
        for (int i = 0; i < m_stackWidget->count(); ++i) {
            if (auto *pane = qobject_cast<TermPane *>(m_stackWidget->widget(i))) {
                if (auto *term = pane->currentTerminal())
                    term->setTerminalFont(font);
            }
        }
    });
    connect(settings, &AppSettings::cursorShapeChanged, this, [this](int shape) {
        for (int i = 0; i < m_stackWidget->count(); ++i) {
            if (auto *pane = qobject_cast<TermPane *>(m_stackWidget->widget(i))) {
                if (auto *term = pane->currentTerminal())
                    term->setCursorShape(shape);
            }
        }
    });
    connect(settings, &AppSettings::cursorBlinkChanged, this, [this](bool blink) {
        for (int i = 0; i < m_stackWidget->count(); ++i) {
            if (auto *pane = qobject_cast<TermPane *>(m_stackWidget->widget(i))) {
                if (auto *term = pane->currentTerminal())
                    term->setCursorBlinkEnabled(blink);
            }
        }
    });
    connect(settings, &AppSettings::scrollbackLinesChanged, this, [this](int lines) {
        for (int i = 0; i < m_stackWidget->count(); ++i) {
            if (auto *pane = qobject_cast<TermPane *>(m_stackWidget->widget(i))) {
                if (auto *term = pane->currentTerminal())
                    term->setScrollbackLines(lines);
            }
        }
    });
    connect(settings, &AppSettings::verticalTabsEnabledChanged, this, [this](bool enabled) {
        if (m_verticalTabsAction && m_verticalTabsAction->isChecked() != enabled)
            m_verticalTabsAction->setChecked(enabled);
        setVerticalTabsEnabled(enabled);
    });
    connect(settings, &AppSettings::colorSchemeChanged, this, [this]() {
        applyThemeToAll();
        if (m_themeGroup) {
            QString scheme = AppSettings::instance()->colorScheme();
            for (auto *act : m_themeGroup->actions()) {
                if (act->data().toString() == scheme)
                    act->setChecked(true);
            }
        }
    });
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, [this]() {
        if (AppSettings::instance()->colorScheme() == QStringLiteral("system"))
            applyThemeToAll();
    });

    // Tab bar configuration
    ensureTabBar();

    // First tab
    std::optional<PtySession::StartOptions> initialSessionOptions;
    if (!m_startupOptions.execute.isEmpty() || !m_startupOptions.workingDirectory.isEmpty()) {
        initialSessionOptions = PtySession::StartOptions{
            .command = m_startupOptions.execute,
            .workingDirectory = m_startupOptions.workingDirectory,
        };
    }
    addTab(true, initialSessionOptions);

    m_themes = ThemeLoader::loadThemes();
    applyThemeToAll();

    setupShortcuts();
    initWindowEffects();
}

MainWindow::~MainWindow() = default;

DTabBar *MainWindow::ensureTabBar() {
    if (m_tabBar)
        return m_tabBar;

    m_tabBar = new DTabBar(this);
    m_tabBar->setTabsClosable(true);
    m_tabBar->setVisibleAddButton(true);
    m_tabBar->setElideMode(Qt::ElideRight);
    m_tabBar->setFocusPolicy(Qt::NoFocus);

    connect(m_tabBar, &DTabBar::tabAddRequested, this, &MainWindow::onTabAddRequested);
    connect(m_tabBar, &DTabBar::tabCloseRequested, this, [this](int index) { onTabCloseRequested(index, false); });
    connect(m_tabBar, &DTabBar::currentChanged, this, &MainWindow::onTabCurrentChanged);

    return m_tabBar;
}

QWidget *MainWindow::ensureTabTitlebarWidget() {
    if (!m_tabTitlebarWidget) {
        m_tabTitlebarWidget = new QWidget(this);
        auto *tabLayout = new QHBoxLayout(m_tabTitlebarWidget);
        tabLayout->setContentsMargins(0, 0, 0, 0);
    }

    auto *tabBar = ensureTabBar();
    auto *tabLayout = qobject_cast<QHBoxLayout *>(m_tabTitlebarWidget->layout());
    if (tabLayout && tabLayout->indexOf(tabBar) < 0)
        tabLayout->addWidget(tabBar, 0, Qt::AlignVCenter);

    return m_tabTitlebarWidget;
}

QWidget *MainWindow::ensureCompactTitlebarWidget() {
    if (m_compactTitlebarWidget)
        return m_compactTitlebarWidget;

    m_compactTitlebarWidget = new QWidget(this);
    m_compactTitlebarWidget->setObjectName(QStringLiteral("compactTitlebarWidget"));
    m_compactTitlebarWidget->setFixedHeight(36);
    auto *compactLayout = new QHBoxLayout(m_compactTitlebarWidget);
    compactLayout->setContentsMargins(12, 6, 12, 6);
    compactLayout->addStretch(1);

    return m_compactTitlebarWidget;
}

void MainWindow::detachTabBarFromTitlebarWidget() {
    if (!m_tabBar)
        return;

    if (m_tabBar->parentWidget() == m_tabTitlebarWidget)
        m_tabBar->setParent(this);
}

void MainWindow::setupTitleBar() {
    DTitlebar *tb = titlebar();
    if (!tb)
        return;

    tb->setTitle("");
    tb->setIcon(QIcon::fromTheme("utilities-terminal"));
    tb->setAutoHideOnFullscreen(true);
    tb->setSwitchThemeMenuVisible(false);

    // Embed the tab bar into the DTK titlebar via a custom widget.
    ensureTabTitlebarWidget();
    ensureCompactTitlebarWidget();

    auto *menu = new QMenu(this);

    m_verticalTabsAction = menu->addAction(tr("Vertical Tabs"));
    m_verticalTabsAction->setObjectName(QStringLiteral("verticalTabsAction"));
    m_verticalTabsAction->setCheckable(true);
    m_verticalTabsAction->setChecked(m_verticalTabsEnabled);
    connect(m_verticalTabsAction, &QAction::toggled, this, [this](bool checked) {
        AppSettings::instance()->setVerticalTabsEnabled(checked);
        setVerticalTabsEnabled(checked);
    });

    auto *remoteAction = menu->addAction(tr("Remote Management"));
    connect(remoteAction, &QAction::triggered, this, &MainWindow::onShortcutRemoteManagement);

    auto *settingsAction = menu->addAction(tr("Settings"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onSettingsTriggered);

    menu->addSeparator();

    auto *themeMenu = menu->addMenu(tr("Theme"));
    auto themes = ThemeLoader::loadThemes();
    QString currentScheme = AppSettings::instance()->colorScheme();

    auto *themeGroup = new QActionGroup(themeMenu);
    themeGroup->setExclusive(true);
    m_themeGroup = themeGroup;

    auto *lightAction = themeMenu->addAction(tr("Light"));
    lightAction->setCheckable(true);
    lightAction->setChecked(currentScheme == QStringLiteral("light"));
    lightAction->setData(QStringLiteral("light"));
    themeGroup->addAction(lightAction);
    connect(lightAction, &QAction::triggered, this,
            [this]() { AppSettings::instance()->setColorScheme(QStringLiteral("light")); });

    auto *darkAction = themeMenu->addAction(tr("Dark"));
    darkAction->setCheckable(true);
    darkAction->setChecked(currentScheme == QStringLiteral("dark"));
    darkAction->setData(QStringLiteral("dark"));
    themeGroup->addAction(darkAction);
    connect(darkAction, &QAction::triggered, this,
            [this]() { AppSettings::instance()->setColorScheme(QStringLiteral("dark")); });

    auto *systemAction = themeMenu->addAction(tr("System"));
    systemAction->setCheckable(true);
    systemAction->setChecked(currentScheme == QStringLiteral("system"));
    systemAction->setData(QStringLiteral("system"));
    themeGroup->addAction(systemAction);
    connect(systemAction, &QAction::triggered, this,
            [this]() { AppSettings::instance()->setColorScheme(QStringLiteral("system")); });

    themeMenu->addSeparator();

    QStringList extraNames;
    for (const auto &t : themes) {
        if (t.name != QStringLiteral("dark") && t.name != QStringLiteral("light"))
            extraNames.append(t.name);
    }
    std::sort(extraNames.begin(), extraNames.end(), [&themes](const QString &a, const QString &b) {
        QString dispA, dispB;
        for (const auto &t : themes) {
            if (t.name == a)
                dispA = t.displayName;
            if (t.name == b)
                dispB = t.displayName;
        }
        return dispA.localeAwareCompare(dispB) < 0;
    });

    QList<QAction *> extraActions;
    for (const QString &name : extraNames) {
        QString displayName;
        for (const auto &t : themes) {
            if (t.name == name) {
                displayName = t.displayName;
                break;
            }
        }
        auto *act = themeMenu->addAction(displayName);
        act->setCheckable(true);
        act->setChecked(currentScheme == name);
        act->setData(name);
        themeGroup->addAction(act);
        extraActions.append(act);
        connect(act, &QAction::triggered, this, [this, name]() { AppSettings::instance()->setColorScheme(name); });
    }

    menu->addSeparator();

    tb->setMenu(menu);
    updateTitlebarPresentation();
}

void MainWindow::addTab(bool activate, const std::optional<PtySession::StartOptions> &startOptions) {
    qCInfo(appLog) << "Creating terminal tab" << m_nextTabId;
    auto *pane = new TermPane(startOptions, m_stackWidget);

    connect(pane, &TermPane::terminalTitleChanged, this, &MainWindow::onTerminalTitleChanged);
    connect(pane, &TermPane::startupSessionExited, this, [this](int exitCode) {
        if (m_startupSessionHandled)
            return;
        m_startupSessionHandled = true;
        Q_EMIT startupSessionFinished(exitCode);
        if (m_startupOptions.waitForChild)
            close();
    });
    connect(pane, &TermPane::sessionClosed, this, &MainWindow::onTerminalSessionClosed);
    connect(pane, &TermPane::currentTerminalChanged, this, &MainWindow::onPaneTerminalChanged);
    connect(pane, &TermPane::paneStructureChanged, this, [this, pane]() {
        if (auto *record = tabRecordForPane(pane))
            refreshTabRecord(*record);
        syncTabWidgetsFromRecords();
    });
    connect(pane, &TermPane::activePaneChanged, this, [this, pane](const QUuid &) {
        if (auto *record = tabRecordForPane(pane))
            refreshTabRecord(*record);
        syncTabWidgetsFromRecords();
    });
    connect(pane, &TermPane::paneTitleChanged, this, [this, pane](const QUuid &, const QString &) {
        if (auto *record = tabRecordForPane(pane))
            refreshTabRecord(*record);
        syncTabWidgetsFromRecords();
    });
    connect(pane, &TermPane::paneCommandStateChanged, this, [this, pane](const QUuid &, TerminalWidget::CommandState) {
        if (auto *record = tabRecordForPane(pane))
            refreshTabRecord(*record);
        syncTabWidgetsFromRecords();
    });
    connect(pane, &TermPane::requestSettings, this, &MainWindow::onSettingsTriggered);

    int stackIndex = m_stackWidget->addWidget(pane);
    m_tabs.append(TabRecord{m_nextTabId++, pane, tr("Terminal"), true});

    // DTabBar::addTab returns the visual tab index; we keep it in sync with stack index
    int tabIndex = m_tabBar->addTab(tr("Terminal"));
    m_tabBar->setTabData(tabIndex, stackIndex);

    if (auto *record = tabRecordForPane(pane))
        refreshTabRecord(*record);
    syncTabWidgetsFromRecords();

    if (activate) {
        m_tabBar->setCurrentIndex(tabIndex);
        if (auto *term = pane->currentTerminal())
            term->setFocus();
    }

    if (!m_themes.isEmpty()) {
        auto theme = resolveTheme();
        for (auto *term : pane->findChildren<TerminalWidget *>())
            term->applyTheme(theme);
    }

    if (m_compositorHasBlur) {
        pane->setOpacity(AppSettings::instance()->opacity());
    }
}

void MainWindow::onTabAddRequested() {
    addTab(true);
}

void MainWindow::onTabCloseRequested(int index, bool hasConfirmed) {
    int stackIndex = m_tabBar->tabData(index).toInt();
    QWidget *page = m_stackWidget->widget(stackIndex);
    if (!page)
        return;

    if (!hasConfirmed) {
        auto *pane = qobject_cast<TermPane *>(page);
        if (pane && pane->runningTerminalCount() > 0) {
            const int count = pane->runningTerminalCount();
            const QString body = count == 1 ? tr("There is still a process running in this terminal. "
                                                 "Closing the terminal will kill it.")
                                            : tr("There are still %1 processes running in this terminal. "
                                                 "Closing the terminal will kill all of them.")
                                                  .arg(count);
            showExitConfirmDialog(tr("Close this terminal?"), body,
                                  [this, index]() { onTabCloseRequested(index, true); });
            return;
        }
    }

    qCInfo(appLog) << "Closing terminal tab at index" << index;

    auto *pane = qobject_cast<TermPane *>(page);
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs.at(i).pane == pane) {
            m_tabs.removeAt(i);
            break;
        }
    }

    m_stackWidget->removeWidget(page);
    page->deleteLater();

    m_tabBar->removeTab(index);

    // If no tabs left, close the window
    if (m_tabBar->count() == 0) {
        qCInfo(appLog) << "Last tab closed, closing main window";
        close();
        return;
    }

    // Sync remaining tab data with new stack indices
    for (int i = 0; i < m_tabBar->count(); ++i) {
        int oldStackIndex = m_tabBar->tabData(i).toInt();
        if (oldStackIndex > stackIndex) {
            m_tabBar->setTabData(i, oldStackIndex - 1);
        }
    }

    syncTabWidgetsFromRecords();
}

void MainWindow::onTabCurrentChanged(int index) {
    if (index < 0 || index >= m_tabBar->count())
        return;

    int stackIndex = m_tabBar->tabData(index).toInt();
    m_stackWidget->setCurrentIndex(stackIndex);

    if (auto *pane = currentPane()) {
        if (auto *term = pane->currentTerminal()) {
            setWindowTitle(term->property("currentTitle").toString());
            term->setFocus();
        }
    }

    syncTabWidgetsFromRecords();
}

void MainWindow::onTerminalTitleChanged(const QString &title) {
    auto *pane = qobject_cast<TermPane *>(sender());
    if (!pane)
        return;

    if (auto *record = tabRecordForPane(pane))
        record->title = title.isEmpty() ? tr("Terminal") : title;
    syncTabWidgetsFromRecords();
}

void MainWindow::onTerminalSessionClosed() {
    auto *pane = qobject_cast<TermPane *>(sender());
    if (!pane)
        return;

    qCInfo(appLog) << "Terminal session closed, removing pane";
    closePane(pane);
}

void MainWindow::onPaneTerminalChanged(TerminalWidget *term) {
    if (!term)
        return;
    auto *pane = qobject_cast<TermPane *>(sender());
    if (!pane || pane != currentPane())
        return;
    if (auto *record = tabRecordForPane(pane))
        refreshTabRecord(*record);
    syncTabWidgetsFromRecords();
    term->setFocus();
}

void MainWindow::closePane(TermPane *pane) {
    if (!pane)
        return;

    const int stackIndex = m_stackWidget->indexOf(pane);
    if (stackIndex < 0)
        return;

    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toInt() == stackIndex) {
            onTabCloseRequested(i);
            return;
        }
    }
}

TermPane *MainWindow::currentPane() const {
    return qobject_cast<TermPane *>(m_stackWidget->currentWidget());
}

TerminalWidget *MainWindow::currentTerminal() const {
    if (auto *pane = currentPane())
        return pane->currentTerminal();
    return nullptr;
}

int MainWindow::indexOfTabId(int tabId) const {
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs.at(i).id == tabId)
            return i;
    }
    return -1;
}

MainWindow::TabRecord *MainWindow::tabRecordForPane(TermPane *pane) {
    if (!pane)
        return nullptr;

    for (auto &record : m_tabs) {
        if (record.pane == pane)
            return &record;
    }
    return nullptr;
}

void MainWindow::refreshTabRecord(TabRecord &record) {
    if (!record.pane) {
        record.title = tr("Terminal");
        return;
    }

    const auto infos = record.pane->paneInfos();
    for (const auto &info : infos) {
        if (info.isActive) {
            record.title = info.title.isEmpty() ? tr("Terminal") : info.title;
            return;
        }
    }

    record.title = tr("Terminal");
}

void MainWindow::refreshTabRecords() {
    for (auto &record : m_tabs)
        refreshTabRecord(record);
}

void MainWindow::syncTabWidgetsFromRecords() {
    for (int i = 0; i < m_tabs.size() && i < m_tabBar->count(); ++i)
        m_tabBar->setTabText(i, m_tabs.at(i).title);

    const int currentIndex = m_tabBar->currentIndex();
    if (currentIndex >= 0 && currentIndex < m_tabs.size())
        setWindowTitle(m_tabs.at(currentIndex).title);
    else
        setWindowTitle(QStringLiteral("deepin-terminal-ghostty"));

    refreshSidebar();
}

void MainWindow::refreshSidebar() {
    if (!m_verticalSidebar)
        return;

    QList<VerticalTabSidebar::TabItem> items;
    items.reserve(m_tabs.size());
    for (int i = 0; i < m_tabs.size(); ++i) {
        const auto &record = m_tabs.at(i);
        VerticalTabSidebar::TabItem item;
        item.id = record.id;
        item.title = record.title.isEmpty() ? tr("Terminal") : record.title;
        item.isCurrent = (i == m_tabBar->currentIndex());
        item.expanded = record.expanded;
        if (record.pane)
            item.panes = record.pane->paneInfos();
        items.append(item);
    }

    m_verticalSidebar->setItems(items);
    m_verticalSidebar->setVisible(m_verticalTabsEnabled);
}

void MainWindow::onSettingsTriggered() {
    if (!m_settingsDialog) {
        m_settingsDialog = new SettingsDialog(this);
    }
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

void MainWindow::setVerticalTabsEnabled(bool enabled) {
    if (m_verticalTabsEnabled == enabled)
        return;

    m_verticalTabsEnabled = enabled;
    if (m_verticalTabsAction && m_verticalTabsAction->isChecked() != enabled)
        m_verticalTabsAction->setChecked(enabled);
    updateTitlebarPresentation();
    rebuildCentralLayout();
}

void MainWindow::rebuildCentralLayout() {
    if (!m_contentHost)
        return;

    auto *layout = qobject_cast<QHBoxLayout *>(m_contentHost->layout());
    if (!layout) {
        layout = new QHBoxLayout(m_contentHost);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
    }

    while (QLayoutItem *item = layout->takeAt(0)) {
        delete item;
    }

    // Ensure remote panel stays on top of content host after layout rebuild
    if (m_remotePanel)
        m_remotePanel->raise();

    if (m_verticalTabsEnabled) {
        if (!m_verticalSidebar) {
            m_verticalSidebar = new VerticalTabSidebar(this);
            m_verticalSidebar->setAutoFillBackground(true);
            connect(m_verticalSidebar, &VerticalTabSidebar::tabActivated, this,
                    [this](int tabId) { gotoTab(indexOfTabId(tabId)); });
            connect(m_verticalSidebar, &VerticalTabSidebar::tabExpansionToggled, this, [this](int tabId) {
                const int index = indexOfTabId(tabId);
                if (index < 0 || index >= m_tabs.size())
                    return;
                m_tabs[index].expanded = !m_tabs[index].expanded;
                refreshSidebar();
            });
            connect(m_verticalSidebar, &VerticalTabSidebar::paneActivated, this,
                    [this](int tabId, const QUuid &paneId) {
                        const int index = indexOfTabId(tabId);
                        if (index < 0 || index >= m_tabs.size())
                            return;
                        gotoTab(index);
                        if (m_tabs[index].pane)
                            m_tabs[index].pane->focusPane(paneId);
                    });
        }
        if (!m_mainSplitter) {
            m_mainSplitter = new QSplitter(Qt::Horizontal, this);
            m_mainSplitter->setChildrenCollapsible(false);
            m_mainSplitter->setHandleWidth(1);
            m_mainSplitter->setStyleSheet(
                QStringLiteral("QSplitter::handle { background-color: rgba(128,128,128,0.25); }"));
            m_mainSplitter->setObjectName(QStringLiteral("verticalTabsSplitter"));
        }
        if (m_verticalSidebar->parentWidget() != m_mainSplitter)
            m_verticalSidebar->setParent(m_mainSplitter);
        if (m_stackWidget->parentWidget() != m_mainSplitter)
            m_stackWidget->setParent(m_mainSplitter);
        if (m_mainSplitter->indexOf(m_verticalSidebar) < 0)
            m_mainSplitter->insertWidget(0, m_verticalSidebar);
        if (m_mainSplitter->indexOf(m_stackWidget) < 0)
            m_mainSplitter->addWidget(m_stackWidget);
        m_mainSplitter->setStretchFactor(0, 0);
        m_mainSplitter->setStretchFactor(1, 1);
        m_verticalSidebar->setMinimumWidth(150);
        m_verticalSidebar->setMaximumWidth(width() / 2);
        bool sidebarJustAdded = (m_mainSplitter->indexOf(m_verticalSidebar) < 0);
        if (sidebarJustAdded) {
            int sw = qMin(360, width() / 2);
            m_mainSplitter->setSizes({sw, width() - sw});
        }
        layout->addWidget(m_mainSplitter);
    } else {
        layout->addWidget(m_stackWidget);
        if (m_verticalSidebar)
            m_verticalSidebar->hide();
    }

    refreshTabRecords();
    refreshSidebar();
}

void MainWindow::updateTitlebarPresentation() {
    DTitlebar *tb = titlebar();
    if (!tb)
        return;

    if (m_verticalTabsEnabled) {
        detachTabBarFromTitlebarWidget();
        ensureTabBar()->hide();
        tb->setCustomWidget(ensureCompactTitlebarWidget());
    } else {
        auto *tabBar = ensureTabBar();
        tb->setCustomWidget(ensureTabTitlebarWidget());
        tabBar->show();
    }
}

void MainWindow::setupShortcuts() {
    auto createOnce = [this](QShortcut *&ptr, const auto &slot) {
        if (!ptr) {
            ptr = new QShortcut(this);
            connect(ptr, &QShortcut::activated, this, slot);
        }
    };

    createOnce(m_scNewTab, &MainWindow::onTabAddRequested);
    createOnce(m_scCloseTab, [this]() {
        int idx = m_tabBar->currentIndex();
        if (idx >= 0)
            onTabCloseRequested(idx);
    });
    createOnce(m_scCloseOtherTabs, &MainWindow::closeOtherTabs);
    createOnce(m_scPrevTab, [this]() {
        int idx = m_tabBar->currentIndex();
        if (idx > 0)
            m_tabBar->setCurrentIndex(idx - 1);
    });
    createOnce(m_scNextTab, [this]() {
        int idx = m_tabBar->currentIndex();
        if (idx >= 0 && idx < m_tabBar->count() - 1)
            m_tabBar->setCurrentIndex(idx + 1);
    });
    createOnce(m_scVSplit, [this]() {
        if (auto *pane = currentPane())
            pane->splitCurrent(Qt::Vertical);
    });
    createOnce(m_scHSplit, [this]() {
        if (auto *pane = currentPane())
            pane->splitCurrent(Qt::Horizontal);
    });
    createOnce(m_scCloseWorkspace, [this]() {
        if (auto *pane = currentPane())
            pane->closeCurrentSplit();
    });
    createOnce(m_scCloseOtherWorkspaces, [this]() {
        if (auto *pane = currentPane())
            pane->closeOtherTerminals();
    });
    createOnce(m_scSelectUpper, [this]() {
        if (auto *pane = currentPane())
            pane->focusNavigation(Qt::TopEdge);
    });
    createOnce(m_scSelectLower, [this]() {
        if (auto *pane = currentPane())
            pane->focusNavigation(Qt::BottomEdge);
    });
    createOnce(m_scSelectLeft, [this]() {
        if (auto *pane = currentPane())
            pane->focusNavigation(Qt::LeftEdge);
    });
    createOnce(m_scSelectRight, [this]() {
        if (auto *pane = currentPane())
            pane->focusNavigation(Qt::RightEdge);
    });
    createOnce(m_scFullscreen, [this]() {
        if (isFullScreen())
            showNormal();
        else
            showFullScreen();
    });
    createOnce(m_scRenameTitle, &MainWindow::onShortcutRenameTitle);
    createOnce(m_scDisplayShortcuts, &MainWindow::onShortcutDisplayShortcuts);
    createOnce(m_scCustomCommand, &MainWindow::onShortcutCustomCommand);
    createOnce(m_scRemoteManagement, &MainWindow::onShortcutRemoteManagement);

    // Create switch-to-tab shortcuts (1-9) once
    for (int i = 1; i <= 9; ++i) {
        bool exists = false;
        for (QShortcut *sc : findChildren<QShortcut *>()) {
            if (sc->property("tabIndex").toInt() == i) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            auto *sc = new QShortcut(this);
            connect(sc, &QShortcut::activated, this, [this, i]() { gotoTab(i - 1); });
            sc->setProperty("tabIndex", i);
        }
    }

    auto *settings = AppSettings::instance();
    static bool connected = false;
    if (!connected) {
        connect(settings->dsettings(), &Dtk::Core::DSettings::valueChanged, this,
                [this](const QString &key, const QVariant &) {
                    if (key.startsWith("shortcuts."))
                        setupShortcuts();
                });
        connected = true;
    }

    updateShortcut(m_scNewTab, "new_tab");
    updateShortcut(m_scCloseTab, "close_tab");
    updateShortcut(m_scCloseOtherTabs, "close_other_tabs");
    updateShortcut(m_scPrevTab, "previous_tab");
    updateShortcut(m_scNextTab, "next_tab");
    updateShortcut(m_scVSplit, "vertical_split");
    updateShortcut(m_scHSplit, "horionzal_split");
    updateShortcut(m_scCloseWorkspace, "close_workspace");
    updateShortcut(m_scCloseOtherWorkspaces, "close_other_workspaces");
    updateShortcut(m_scSelectUpper, "select_upper_workspace");
    updateShortcut(m_scSelectLower, "select_lower_workspace");
    updateShortcut(m_scSelectLeft, "select_left_workspace");
    updateShortcut(m_scSelectRight, "select_right_workspace");
    updateShortcut(m_scFullscreen, "switch_fullscreen");
    updateShortcut(m_scRenameTitle, "rename_title");
    updateShortcut(m_scDisplayShortcuts, "display_shortcuts");
    updateShortcut(m_scCustomCommand, "custom_command");
    updateShortcut(m_scRemoteManagement, "remote_management");

    for (int i = 1; i <= 9; ++i) {
        for (QShortcut *sc : findChildren<QShortcut *>()) {
            if (sc->property("tabIndex").toInt() == i) {
                updateShortcut(sc, QString("switch_label_win_%1").arg(i));
                break;
            }
        }
    }
}

void MainWindow::updateShortcut(QShortcut *shortcut, const QString &name) {
    if (!shortcut)
        return;
    shortcut->setKey(AppSettings::instance()->shortcut(name));
}

void MainWindow::closeOtherTabs() {
    int current = m_tabBar->currentIndex();
    if (current < 0)
        return;
    for (int i = m_tabBar->count() - 1; i >= 0; --i) {
        if (i != current)
            onTabCloseRequested(i);
    }
}

void MainWindow::gotoTab(int index) {
    if (index >= 0 && index < m_tabBar->count())
        m_tabBar->setCurrentIndex(index);
}

void MainWindow::onShortcutRenameTitle() {
    auto *pane = currentPane();
    if (!pane)
        return;
    auto *term = pane->currentTerminal();
    if (!term)
        return;

    bool ok = false;
    QString currentText = m_tabBar->tabText(m_tabBar->currentIndex());
    QString text =
        QInputDialog::getText(this, tr("Rename title"), tr("New title:"), QLineEdit::Normal, currentText, &ok);
    if (ok && !text.isEmpty())
        pane->setCustomTitle(text);
}

void MainWindow::onShortcutDisplayShortcuts() {
    auto *dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Keyboard Shortcuts"));
    dialog->setMinimumSize(480, 520);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    auto *table = new QTableWidget(dialog);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels(QStringList() << tr("Action") << tr("Shortcut"));
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);

    struct Item {
        QString name;
        QString key;
    };
    QList<Item> items;
    auto *settings = AppSettings::instance();

    items << Item{tr("Copy"), settings->shortcut("copy").toString()};
    items << Item{tr("Paste"), settings->shortcut("paste").toString()};
    items << Item{tr("Find"), settings->shortcut("find").toString()};
    items << Item{tr("Zoom in"), settings->shortcut("zoom_in").toString()};
    items << Item{tr("Zoom out"), settings->shortcut("zoom_out").toString()};
    items << Item{tr("Default size"), settings->shortcut("default_size").toString()};
    items << Item{tr("Select all"), settings->shortcut("select_all").toString()};
    items << Item{tr("New tab"), settings->shortcut("new_tab").toString()};
    items << Item{tr("Close tab"), settings->shortcut("close_tab").toString()};
    items << Item{tr("Close other tabs"), settings->shortcut("close_other_tabs").toString()};
    items << Item{tr("Previous tab"), settings->shortcut("previous_tab").toString()};
    items << Item{tr("Next tab"), settings->shortcut("next_tab").toString()};
    items << Item{tr("Vertical split"), settings->shortcut("vertical_split").toString()};
    items << Item{tr("Horizontal split"), settings->shortcut("horionzal_split").toString()};
    items << Item{tr("Select upper workspace"), settings->shortcut("select_upper_workspace").toString()};
    items << Item{tr("Select lower workspace"), settings->shortcut("select_lower_workspace").toString()};
    items << Item{tr("Select left workspace"), settings->shortcut("select_left_workspace").toString()};
    items << Item{tr("Select right workspace"), settings->shortcut("select_right_workspace").toString()};
    items << Item{tr("Close workspace"), settings->shortcut("close_workspace").toString()};
    items << Item{tr("Close other workspaces"), settings->shortcut("close_other_workspaces").toString()};
    for (int i = 1; i <= 9; ++i)
        items << Item{tr("Go to tab %1").arg(i), settings->shortcut(QString("switch_label_win_%1").arg(i)).toString()};
    items << Item{tr("Fullscreen"), settings->shortcut("switch_fullscreen").toString()};
    items << Item{tr("Rename title"), settings->shortcut("rename_title").toString()};
    items << Item{tr("Custom command"), settings->shortcut("custom_command").toString()};
    items << Item{tr("Remote management"), settings->shortcut("remote_management").toString()};

    table->setRowCount(items.size());
    for (int i = 0; i < items.size(); ++i) {
        table->setItem(i, 0, new QTableWidgetItem(items[i].name));
        table->setItem(i, 1, new QTableWidgetItem(items[i].key));
    }

    auto *layout = new QVBoxLayout(dialog);
    layout->addWidget(table);
    dialog->setLayout(layout);
    dialog->show();
}

void MainWindow::onShortcutCustomCommand() {
    auto *pane = currentPane();
    if (!pane)
        return;

    bool ok = false;
    QString command =
        QInputDialog::getText(this, tr("Custom Command"), tr("Enter command:"), QLineEdit::Normal, QString(), &ok);
    if (ok && !command.isEmpty())
        pane->executeCommand(command);
}

void MainWindow::onShortcutRemoteManagement() {
    if (!m_remotePanel)
        return;

    if (m_remotePanel->isPanelVisible())
        m_remotePanel->hidePanel();
    else
        m_remotePanel->showPanel();
}

void MainWindow::onConnectRemoteServer(const ServerConfig &config) {
    auto *pane = currentPane();
    if (!pane)
        return;

    pane->connectToRemoteServer(config);
}

void MainWindow::initWindowEffects() {
    auto *wmHelper = DWindowManagerHelper::instance();
    m_compositorHasBlur = wmHelper->hasBlurWindow();

    connect(wmHelper, &DWindowManagerHelper::hasBlurWindowChanged, this, &MainWindow::onCompositorCapabilitiesChanged);

    auto *settings = AppSettings::instance();
    connect(settings, &AppSettings::opacityChanged, this, &MainWindow::applyOpacityToAll);
    connect(settings, &AppSettings::backgroundBlurChanged, this, &MainWindow::setWindowBlurEnabled);

    applyOpacityToAll();
    setWindowBlurEnabled(settings->backgroundBlur());
}

void MainWindow::applyOpacityToAll() {
    qreal opacity = AppSettings::instance()->opacity();
    if (!m_compositorHasBlur)
        opacity = 1.0;

    bool needsTranslucent = opacity < 1.0 || AppSettings::instance()->backgroundBlur();
    setAttribute(Qt::WA_TranslucentBackground, needsTranslucent && m_compositorHasBlur);

    for (int i = 0; i < m_stackWidget->count(); ++i) {
        if (auto *pane = qobject_cast<TermPane *>(m_stackWidget->widget(i)))
            pane->setOpacity(opacity);
    }
}

void MainWindow::setWindowBlurEnabled(bool enabled) {
    if (!m_compositorHasBlur)
        enabled = false;

    setEnableBlurWindow(enabled);

    bool needsTranslucent = enabled || AppSettings::instance()->opacity() < 1.0;
    setAttribute(Qt::WA_TranslucentBackground, needsTranslucent && m_compositorHasBlur);
}

void MainWindow::onCompositorCapabilitiesChanged() {
    m_compositorHasBlur = DWindowManagerHelper::instance()->hasBlurWindow();

    if (!m_compositorHasBlur) {
        setWindowBlurEnabled(false);
    }

    applyOpacityToAll();

    auto *settings = AppSettings::instance();
    setWindowBlurEnabled(settings->backgroundBlur());
}

TerminalTheme MainWindow::resolveTheme() const {
    if (m_themes.isEmpty())
        const_cast<MainWindow *>(this)->m_themes = ThemeLoader::loadThemes();
    QString setting = AppSettings::instance()->colorScheme();
    if (setting == QStringLiteral("system")) {
        auto colorType = DGuiApplicationHelper::instance()->themeType();
        return ThemeLoader::findTheme(m_themes, colorType == DGuiApplicationHelper::DarkType ? QStringLiteral("dark")
                                                                                             : QStringLiteral("light"));
    }
    return ThemeLoader::findTheme(m_themes, setting);
}

void MainWindow::applyThemeToAll() {
    auto theme = resolveTheme();
    for (int i = 0; i < m_stackWidget->count(); ++i) {
        auto *pane = qobject_cast<TermPane *>(m_stackWidget->widget(i));
        if (!pane)
            continue;
        for (auto *term : pane->findChildren<TerminalWidget *>())
            term->applyTheme(theme);
    }

    auto *helper = DGuiApplicationHelper::instance();
    QString setting = AppSettings::instance()->colorScheme();
    if (setting == QStringLiteral("system"))
        helper->setPaletteType(DGuiApplicationHelper::UnknownType);
    else
        helper->setPaletteType(theme.isDark ? DGuiApplicationHelper::DarkType : DGuiApplicationHelper::LightType);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    DMainWindow::resizeEvent(event);
    AppSettings::instance()->saveWindowSize(event->size());

    if (m_verticalTabsEnabled && m_verticalSidebar)
        m_verticalSidebar->setMaximumWidth(event->size().width() / 2);

    if (m_remotePanel && m_remotePanel->isPanelVisible() && m_contentHost) {
        QRect hostRect = m_contentHost->rect();
        m_remotePanel->setGeometry(hostRect.width() - 260, 0, 260, hostRect.height());
    }
}

void MainWindow::showExitConfirmDialog(const QString &title, const QString &body, std::function<void()> onConfirm) {
    auto *dlg = new DDialog(title, body, this);
    dlg->setWindowModality(Qt::WindowModal);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->addButton(QObject::tr("Cancel", "ExitConfirmDialog"), false, DDialog::ButtonNormal);
    dlg->addButton(QObject::tr("Close", "ExitConfirmDialog"), true, DDialog::ButtonWarning);
    QObject::connect(dlg, &DDialog::buttonClicked, dlg, [onConfirm = std::move(onConfirm)](int index) {
        if (index == 1)
            onConfirm();
    });
    dlg->show();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    auto *app = qobject_cast<DApplication *>(qApp);
    if (app) {
        if (auto *aboutDialog = app->aboutDialog()) {
            disconnect(aboutDialog, &QObject::destroyed, nullptr, nullptr);
            connect(aboutDialog, &QObject::destroyed, this, [app]() { app->setAboutDialog(nullptr); });
        }
    }

    if (!m_hasConfirmedClose) {
        int runningCount = 0;
        for (const auto &rec : m_tabs) {
            if (rec.pane)
                runningCount += rec.pane->runningTerminalCount();
        }
        if (runningCount > 0) {
            event->ignore();
            if (isMinimized())
                setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);

            const bool singleTab = m_tabs.size() == 1;
            const QString dialogTitle = singleTab ? tr("Close this terminal?") : tr("Close this window?");
            const QString dialogBody =
                singleTab ? (runningCount == 1 ? tr("There is still a process running in this terminal. "
                                                    "Closing the terminal will kill it.")
                                               : tr("There are still %1 processes running in this terminal. "
                                                    "Closing the terminal will kill all of them.")
                                                     .arg(runningCount))
                          : tr("There are still processes running in this window. "
                               "Closing the window will kill all of them.");

            showExitConfirmDialog(dialogTitle, dialogBody, [this]() {
                m_hasConfirmedClose = true;
                close();
            });
            return;
        }
    }

    while (m_stackWidget->count() > 0) {
        QWidget *w = m_stackWidget->widget(0);
        m_stackWidget->removeWidget(w);
        w->deleteLater();
    }
    DMainWindow::closeEvent(event);
}
