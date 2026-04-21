#include "MainWindow.h"

#include "AppSettings.h"
#include "SettingsDialog.h"
#include "TermPane.h"
#include "TerminalWidget.h"

#include <DTitlebar>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QStackedWidget>

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

void MainWindow::closeEvent(QCloseEvent *event) {
    // Clean up all pages so PtySession destructors run gracefully
    while (m_stackWidget->count() > 0) {
        QWidget *w = m_stackWidget->widget(0);
        m_stackWidget->removeWidget(w);
        w->deleteLater();
    }
    DMainWindow::closeEvent(event);
}
