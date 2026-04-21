#include "MainWindow.h"

#include "AppSettings.h"
#include "SettingsDialog.h"
#include "TermPane.h"
#include "TerminalWidget.h"

#include <DTitlebar>
#include <QDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QShortcut>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent), m_tabBar(new DTabBar(this)), m_stackWidget(new QStackedWidget(this)) {
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
    resize(960, 640);
    setWindowTitle("deepin-terminal-ghostty");

    // Central widget — stacked pages, one per tab
    setCentralWidget(m_stackWidget);

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

    // Tab bar configuration
    m_tabBar->setTabsClosable(true);
    m_tabBar->setVisibleAddButton(true);
    m_tabBar->setElideMode(Qt::ElideRight);
    m_tabBar->setFocusPolicy(Qt::NoFocus);

    connect(m_tabBar, &DTabBar::tabAddRequested, this, &MainWindow::onTabAddRequested);
    connect(m_tabBar, &DTabBar::tabCloseRequested, this, &MainWindow::onTabCloseRequested);
    connect(m_tabBar, &DTabBar::currentChanged, this, &MainWindow::onTabCurrentChanged);

    // First tab
    addTab(true);

    setupShortcuts();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupTitleBar() {
    DTitlebar *tb = titlebar();
    if (!tb)
        return;

    tb->setTitle("");
    tb->setIcon(QIcon::fromTheme("utilities-terminal"));
    tb->setAutoHideOnFullscreen(true);

    // Embed the tab bar into the DTK titlebar via a custom widget
    QWidget *tabWrapper = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(tabWrapper);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tabBar, 0, Qt::AlignVCenter);
    tabWrapper->setLayout(layout);

    tb->setCustomWidget(tabWrapper);

    auto *menu = new QMenu(this);
    auto *settingsAction = menu->addAction(tr("Settings"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onSettingsTriggered);
    tb->setMenu(menu);
}

void MainWindow::addTab(bool activate) {
    auto *pane = new TermPane(m_stackWidget);

    connect(pane, &TermPane::terminalTitleChanged, this, &MainWindow::onTerminalTitleChanged);
    connect(pane, &TermPane::sessionClosed, this, &MainWindow::onTerminalSessionClosed);
    connect(pane, &TermPane::currentTerminalChanged, this, &MainWindow::onPaneTerminalChanged);
    connect(pane, &TermPane::requestSettings, this, &MainWindow::onSettingsTriggered);

    int stackIndex = m_stackWidget->addWidget(pane);

    // DTabBar::addTab returns the visual tab index; we keep it in sync with stack index
    int tabIndex = m_tabBar->addTab("Terminal");
    m_tabBar->setTabData(tabIndex, stackIndex);

    if (activate) {
        m_tabBar->setCurrentIndex(tabIndex);
        if (auto *term = pane->currentTerminal())
            term->setFocus();
    }
}

void MainWindow::onTabAddRequested() {
    addTab(true);
}

void MainWindow::onTabCloseRequested(int index) {
    int stackIndex = m_tabBar->tabData(index).toInt();
    QWidget *page = m_stackWidget->widget(stackIndex);
    if (!page)
        return;

    m_stackWidget->removeWidget(page);
    page->deleteLater();

    m_tabBar->removeTab(index);

    // If no tabs left, close the window
    if (m_tabBar->count() == 0) {
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
}

void MainWindow::onTerminalTitleChanged(const QString &title) {
    auto *pane = qobject_cast<TermPane *>(sender());
    if (!pane)
        return;

    int stackIndex = m_stackWidget->indexOf(pane);
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toInt() == stackIndex) {
            m_tabBar->setTabText(i, title);
            if (m_tabBar->currentIndex() == i)
                setWindowTitle(title);
            break;
        }
    }
}

void MainWindow::onTerminalSessionClosed() {
    auto *pane = qobject_cast<TermPane *>(sender());
    if (!pane)
        return;

    closePane(pane);
}

void MainWindow::onPaneTerminalChanged(TerminalWidget *term) {
    if (!term)
        return;
    auto *pane = qobject_cast<TermPane *>(sender());
    if (!pane || pane != currentPane())
        return;
    setWindowTitle(term->property("currentTitle").toString());
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

void MainWindow::onSettingsTriggered() {
    if (!m_settingsDialog) {
        m_settingsDialog = new SettingsDialog(this);
    }
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
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
    auto *pane = currentPane();
    if (!pane)
        return;

    bool ok = false;
    QString address = QInputDialog::getText(this, tr("Remote Management"), tr("SSH address (user@host):"),
                                            QLineEdit::Normal, QString(), &ok);
    if (ok && !address.isEmpty())
        pane->executeCommand(QString("ssh %1").arg(address));
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Clean up all pages so PtySession destructors run gracefully
    while (m_stackWidget->count() > 0) {
        QWidget *w = m_stackWidget->widget(0);
        m_stackWidget->removeWidget(w);
        w->deleteLater();
    }
    DMainWindow::closeEvent(event);
}
