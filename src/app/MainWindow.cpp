#include "MainWindow.h"
#include "TerminalWidget.h"

#include <DTitlebar>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QIcon>

MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent)
    , m_tabBar(new DTabBar(this))
    , m_stackWidget(new QStackedWidget(this))
{
    // Prevent DTK or Qt default actions from intercepting standard
    // terminal keybindings (Ctrl+A–Z). In a terminal every Ctrl+letter
    // combo must be sent to the PTY as a C0 control character.
    for (QAction *action : findChildren<QAction *>()) {
        QKeySequence seq = action->shortcut();
        if (seq.isEmpty())
            continue;
        int key = seq[0].key();
        Qt::KeyboardModifiers mods = seq[0].keyboardModifiers();
        if ((mods & Qt::ControlModifier) &&
            !(mods & Qt::ShiftModifier) &&
            key >= Qt::Key_A && key <= Qt::Key_Z) {
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

void MainWindow::setupTitleBar()
{
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
}

void MainWindow::addTab(bool activate)
{
    auto *terminal = new TerminalWidget(m_stackWidget);
    if (!terminal->initialize()) {
        terminal->deleteLater();
        return;
    }

    connect(terminal, &TerminalWidget::terminalTitleChanged,
            this, &MainWindow::onTerminalTitleChanged);
    connect(terminal, &TerminalWidget::sessionClosed,
            this, &MainWindow::onTerminalSessionClosed);

    int stackIndex = m_stackWidget->addWidget(terminal);

    // DTabBar::addTab returns the visual tab index; we keep it in sync with stack index
    int tabIndex = m_tabBar->addTab("Terminal");
    m_tabBar->setTabData(tabIndex, stackIndex);

    if (activate) {
        m_tabBar->setCurrentIndex(tabIndex);
        terminal->setFocus();
    }
}

void MainWindow::onTabAddRequested()
{
    addTab(true);
}

void MainWindow::onTabCloseRequested(int index)
{
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

void MainWindow::onTabCurrentChanged(int index)
{
    if (index < 0 || index >= m_tabBar->count())
        return;

    int stackIndex = m_tabBar->tabData(index).toInt();
    m_stackWidget->setCurrentIndex(stackIndex);

    TerminalWidget *term = currentTerminal();
    if (term) {
        setWindowTitle(term->property("currentTitle").toString());
        term->setFocus();
    }
}

void MainWindow::onTerminalTitleChanged(const QString &title)
{
    auto *term = qobject_cast<TerminalWidget *>(sender());
    if (!term)
        return;

    // Store title on the widget so we can retrieve it later
    term->setProperty("currentTitle", title);

    int stackIndex = m_stackWidget->indexOf(term);
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toInt() == stackIndex) {
            m_tabBar->setTabText(i, title);
            if (m_tabBar->currentIndex() == i)
                setWindowTitle(title);
            break;
        }
    }
}

void MainWindow::onTerminalSessionClosed()
{
    auto *terminal = qobject_cast<TerminalWidget *>(sender());
    if (!terminal)
        return;

    closeTerminal(terminal);
}

void MainWindow::closeTerminal(TerminalWidget *terminal)
{
    if (!terminal)
        return;

    const int stackIndex = m_stackWidget->indexOf(terminal);
    if (stackIndex < 0)
        return;

    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toInt() == stackIndex) {
            onTabCloseRequested(i);
            return;
        }
    }
}

TerminalWidget *MainWindow::currentTerminal() const
{
    return qobject_cast<TerminalWidget *>(m_stackWidget->currentWidget());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Clean up all pages so PtySession destructors run gracefully
    while (m_stackWidget->count() > 0) {
        QWidget *w = m_stackWidget->widget(0);
        m_stackWidget->removeWidget(w);
        w->deleteLater();
    }
    DMainWindow::closeEvent(event);
}
