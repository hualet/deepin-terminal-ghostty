#include "TermPane.h"

#include "AppSettings.h"
#include "PageSearchBar.h"
#include "PtySession.h"
#include "TerminalWidget.h"
#include "remote/ServerConfig.h"

#include <QClipboard>
#include <QContextMenuEvent>
#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QSplitter>
#include <QStandardPaths>
#include <QUuid>
#include <QVBoxLayout>

namespace {

constexpr int kTerminalContentPadding = 4;

Qt::KeyboardModifiers normalizedModifiers(Qt::KeyboardModifiers modifiers) {
    return modifiers & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
}

bool matchesShortcut(const QKeyEvent *event, const QKeySequence &shortcut) {
    if (!event || shortcut.isEmpty())
        return false;

    const Qt::KeyboardModifiers eventModifiers = normalizedModifiers(event->modifiers());
    const auto eventKey = static_cast<Qt::Key>(event->key());

    for (qsizetype i = 0; i < shortcut.count(); ++i) {
        const QKeyCombination combination = shortcut[i];
        if (combination.key() != eventKey)
            continue;
        if (normalizedModifiers(combination.keyboardModifiers()) == eventModifiers)
            return true;
    }

    return false;
}

QUuid ensurePaneId(TerminalWidget *term) {
    const QVariant existing = term->property("paneId");
    if (existing.isValid())
        return existing.toUuid();

    const QUuid id = QUuid::createUuid();
    term->setProperty("paneId", id);
    return id;
}

QString paneTitle(TerminalWidget *term) {
    return term->property("currentTitle").toString();
}

TerminalWidget *firstTerminalWidget(QWidget *widget) {
    if (!widget)
        return nullptr;

    if (auto *term = qobject_cast<TerminalWidget *>(widget))
        return term;

    if (auto *splitter = qobject_cast<QSplitter *>(widget)) {
        for (int i = 0; i < splitter->count(); ++i) {
            if (auto *term = firstTerminalWidget(splitter->widget(i)))
                return term;
        }
    }

    return nullptr;
}

void collectTerminalWidgetsInVisualOrder(QWidget *widget, QList<TerminalWidget *> &terminals) {
    if (!widget)
        return;

    if (auto *term = qobject_cast<TerminalWidget *>(widget)) {
        terminals.append(term);
        return;
    }

    if (auto *splitter = qobject_cast<QSplitter *>(widget)) {
        for (int i = 0; i < splitter->count(); ++i)
            collectTerminalWidgetsInVisualOrder(splitter->widget(i), terminals);
    }
}

} // namespace

TermPane::TermPane(const std::optional<PtySession::StartOptions> &initialSessionOptions, QWidget *parent)
    : QWidget(parent), m_initialSessionOptions(initialSessionOptions) {
    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(0);
    m_layout->setContentsMargins(0, 0, 0, 0);

    auto *term = createTerminal();
    m_layout->addWidget(term);
    m_rootWidget = term;
    setCurrentTerminal(term);

    m_searchBar = new PageSearchBar(this);
    connect(m_searchBar, &PageSearchBar::findNext, this, &TermPane::onSearchFindNext);
    connect(m_searchBar, &PageSearchBar::findPrev, this, &TermPane::onSearchFindPrev);
    connect(m_searchBar, &PageSearchBar::keywordChanged, this, &TermPane::onSearchKeywordChanged);
    connect(m_searchBar, &PageSearchBar::closeSearchBar, this, &TermPane::hideSearchBar);
}

QList<TermPane::PaneInfo> TermPane::paneInfos() const {
    QList<PaneInfo> infos;
    const QList<TerminalWidget *> terminals = terminalsInVisualOrder();
    infos.reserve(terminals.size());
    for (TerminalWidget *term : terminals) {
        PaneInfo info;
        info.id = term->property("paneId").toUuid();
        info.title = paneTitle(term);
        info.isActive = (term == m_currentTerm);
        infos.append(info);
    }
    return infos;
}

QUuid TermPane::activePaneId() const {
    if (!m_currentTerm)
        return {};
    return m_currentTerm->property("paneId").toUuid();
}

bool TermPane::focusPane(const QUuid &paneId) {
    for (TerminalWidget *term : terminalsInVisualOrder()) {
        if (term->property("paneId").toUuid() != paneId)
            continue;
        setCurrentTerminal(term);
        term->setFocus();
        return true;
    }
    return false;
}

TerminalWidget *TermPane::currentTerminal() const {
    return m_currentTerm;
}

TerminalWidget *TermPane::createTerminal() {
    auto *term = new TerminalWidget(this);
    term->setContentsMargins(kTerminalContentPadding, kTerminalContentPadding, kTerminalContentPadding,
                             kTerminalContentPadding);

    ensurePaneId(term);
    if (m_initialSessionOptions) {
        term->setStartOptions(*m_initialSessionOptions);
        m_startupTerminal = term;
        m_initialSessionOptions.reset();
    }
    term->initialize();

    auto *settings = AppSettings::instance();
    term->setTerminalFont(settings->terminalFont());
    term->setCursorShape(settings->cursorShape());
    term->setCursorBlinkEnabled(settings->cursorBlink());
    term->setScrollbackLines(settings->scrollbackLines());

    term->installEventFilter(this);
    setupTerminalConnections(term);
    return term;
}

void TermPane::setupTerminalConnections(TerminalWidget *term) {
    connect(term, &TerminalWidget::terminalTitleChanged, this, [this, term](const QString &title) {
        QString displayTitle = term->property("customTitle").toString();
        if (displayTitle.isEmpty())
            displayTitle = title;
        term->setProperty("currentTitle", displayTitle);
        Q_EMIT paneTitleChanged(ensurePaneId(term), displayTitle);
        if (m_currentTerm == term)
            Q_EMIT terminalTitleChanged(displayTitle);
    });
    connect(term, &TerminalWidget::sessionExited, this, [this, term](int exitCode) {
        if (term == m_startupTerminal) {
            m_startupTerminal = nullptr;
            Q_EMIT startupSessionExited(exitCode);
        }
    });
    connect(term, &TerminalWidget::sessionClosed, this, [this, term]() { removeTerminal(term); });
    connect(term, &TerminalWidget::focusGained, this, [this, term]() { setCurrentTerminal(term); });
}

void TermPane::setCurrentTerminal(TerminalWidget *term) {
    if (m_currentTerm == term)
        return;
    m_currentTerm = term;
    Q_EMIT currentTerminalChanged(term);
    Q_EMIT activePaneChanged(activePaneId());
}

void TermPane::splitCurrent(Qt::Orientation orientation) {
    if (!m_currentTerm)
        return;

    TerminalWidget *newTerm = createTerminal();
    splitTerminal(m_currentTerm, newTerm, orientation);
    setCurrentTerminal(newTerm);
    notifyPaneStructureChanged();
}

void TermPane::closeCurrentSplit() {
    removeTerminal(m_currentTerm);
}

void TermPane::removeTerminal(TerminalWidget *term) {
    if (!term)
        return;

    auto *splitter = qobject_cast<QSplitter *>(term->parentWidget());
    if (!splitter) {
        // This is the only terminal in the pane
        if (m_currentTerm == term)
            m_currentTerm = nullptr;
        if (m_rootWidget == term)
            m_rootWidget = nullptr;
        m_layout->removeWidget(term);
        term->deleteLater();
        Q_EMIT sessionClosed();
        if (!m_currentTerm)
            Q_EMIT activePaneChanged({});
        notifyPaneStructureChanged();
        return;
    }

    int index = splitter->indexOf(term);
    QList<int> sizes = splitter->sizes();
    const int removedSize = sizes.value(index, 0);
    TerminalWidget *replacementTerm = nullptr;
    if (m_currentTerm == term) {
        QList<QWidget *> remainingWidgets;
        for (int i = 0; i < splitter->count(); ++i) {
            QWidget *child = splitter->widget(i);
            if (child != term)
                remainingWidgets.append(child);
        }

        if (!remainingWidgets.isEmpty()) {
            const int replacementIndex = index > 0 ? index - 1 : 0;
            replacementTerm = firstTerminalWidget(remainingWidgets.at(replacementIndex));
        }
    }

    term->setParent(nullptr);
    term->deleteLater();

    sizes.removeAt(index);
    if (!sizes.isEmpty()) {
        const int recipientIndex = qMax(0, index - 1);
        sizes[recipientIndex] += removedSize;
        splitter->setSizes(sizes);
    }

    promoteSingleChildSplitter(splitter);

    // Update current terminal if the removed one was current
    if (m_currentTerm == term) {
        if (replacementTerm) {
            setCurrentTerminal(replacementTerm);
        } else {
            m_currentTerm = nullptr;
            Q_EMIT activePaneChanged({});
        }
    }

    notifyPaneStructureChanged();
}

QSplitter *TermPane::createPaneSplitter(Qt::Orientation orientation) {
    auto *splitter = new QSplitter(orientation, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(1);
    return splitter;
}

QList<TerminalWidget *> TermPane::terminalsInVisualOrder() const {
    QList<TerminalWidget *> terminals;
    collectTerminalWidgetsInVisualOrder(m_rootWidget, terminals);
    return terminals;
}

void TermPane::splitTerminal(TerminalWidget *term, TerminalWidget *newTerm, Qt::Orientation orientation) {
    auto *splitter = qobject_cast<QSplitter *>(term->parentWidget());
    if (!splitter)
        m_layout->removeWidget(term);

    auto *newSplitter = createPaneSplitter(orientation);
    const int index = splitter ? splitter->indexOf(term) : -1;
    const QList<int> sizes = splitter ? splitter->sizes() : QList<int>();
    term->setParent(nullptr);
    newSplitter->addWidget(term);
    newSplitter->addWidget(newTerm);
    newSplitter->setSizes({1, 1});

    if (splitter) {
        splitter->insertWidget(index, newSplitter);
        splitter->setSizes(sizes);
        return;
    }

    m_layout->addWidget(newSplitter);
    m_rootWidget = newSplitter;
}

void TermPane::promoteSingleChildSplitter(QSplitter *splitter) {
    if (!splitter || splitter->count() != 1)
        return;

    const int promotedSize = splitter->sizes().value(0, 1);
    QWidget *remaining = splitter->widget(0);
    remaining->setParent(nullptr);

    auto *parentSplitter = qobject_cast<QSplitter *>(splitter->parentWidget());
    if (parentSplitter) {
        const int parentIndex = parentSplitter->indexOf(splitter);
        QList<int> sizes = parentSplitter->sizes();
        splitter->setParent(nullptr);
        parentSplitter->insertWidget(parentIndex, remaining);
        sizes.removeAt(parentIndex);
        sizes.insert(parentIndex, promotedSize);
        parentSplitter->setSizes(sizes);
    } else {
        m_layout->removeWidget(splitter);
        m_layout->addWidget(remaining);
        m_rootWidget = remaining;
    }

    splitter->deleteLater();
}

void TermPane::notifyPaneStructureChanged() {
    if (m_deferPaneStructureChanged) {
        m_pendingPaneStructureChanged = true;
        return;
    }

    Q_EMIT paneStructureChanged();
}

void TermPane::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_searchBar && m_searchBar->isVisible())
        m_searchBar->move(width() - m_searchBar->width(), 0);
}

bool TermPane::eventFilter(QObject *watched, QEvent *event) {
    auto *term = qobject_cast<TerminalWidget *>(watched);
    if (!term)
        return false;

    if (event->type() == QEvent::ContextMenu) {
        auto *contextMenuEvent = static_cast<QContextMenuEvent *>(event);
        setCurrentTerminal(term);
        showTerminalContextMenu(term, contextMenuEvent->globalPos());
        return true;
    }

    if (event->type() != QEvent::KeyPress)
        return false;

    auto *keyEvent = static_cast<QKeyEvent *>(event);
    auto *settings = AppSettings::instance();

    if (matchesShortcut(keyEvent, settings->shortcut("find"))) {
        showSearchBar();
        return true;
    }
    if (matchesShortcut(keyEvent, settings->shortcut("copy"))) {
        term->copyToClipboard();
        return true;
    }
    if (matchesShortcut(keyEvent, settings->shortcut("paste"))) {
        term->pasteFromClipboard();
        return true;
    }
    if (matchesShortcut(keyEvent, settings->shortcut("select_all"))) {
        term->selectAll();
        return true;
    }
    if (matchesShortcut(keyEvent, settings->shortcut("zoom_in"))) {
        term->zoomIn();
        return true;
    }
    if (matchesShortcut(keyEvent, settings->shortcut("zoom_out"))) {
        term->zoomOut();
        return true;
    }
    if (matchesShortcut(keyEvent, settings->shortcut("default_size"))) {
        term->setTerminalFont(settings->terminalFont());
        return true;
    }

    // Workspace navigation and close — intercept before TerminalWidget sends to PTY
    if (matchesShortcut(keyEvent, settings->shortcut("select_upper_workspace"))) {
        focusNavigation(Qt::TopEdge);
        return true;
    }
    if (matchesShortcut(keyEvent, settings->shortcut("select_lower_workspace"))) {
        focusNavigation(Qt::BottomEdge);
        return true;
    }
    if (matchesShortcut(keyEvent, settings->shortcut("select_left_workspace"))) {
        focusNavigation(Qt::LeftEdge);
        return true;
    }
    if (matchesShortcut(keyEvent, settings->shortcut("select_right_workspace"))) {
        focusNavigation(Qt::RightEdge);
        return true;
    }
    if (matchesShortcut(keyEvent, settings->shortcut("close_workspace"))) {
        closeCurrentSplit();
        return true;
    }
    if (matchesShortcut(keyEvent, settings->shortcut("close_other_workspaces"))) {
        closeOtherTerminals();
        return true;
    }

    return false;
}

void TermPane::showTerminalContextMenu(TerminalWidget *term, const QPoint &globalPos) {
    if (!term)
        return;

    QMenu menu(this);

    if (term->hasSelection())
        menu.addAction(tr("Copy"), term, &TerminalWidget::copyToClipboard);

    auto *pasteAction = menu.addAction(tr("Paste"), term, &TerminalWidget::pasteFromClipboard);
    pasteAction->setEnabled(!QGuiApplication::clipboard()->text().isEmpty());

    menu.addSeparator();
    auto *searchAction = menu.addAction(tr("Search"));
    menu.addSeparator();
    auto *hSplitAction = menu.addAction(tr("Horizontal Split"));
    auto *vSplitAction = menu.addAction(tr("Vertical Split"));
    menu.addSeparator();
    auto *closeSplitAction = menu.addAction(tr("Close Split"));
    menu.addSeparator();
    auto *settingsAction = menu.addAction(tr("Settings"));

    auto *action = menu.exec(globalPos);
    if (action == searchAction)
        showSearchBar();
    else if (action == hSplitAction)
        splitCurrent(Qt::Horizontal);
    else if (action == vSplitAction)
        splitCurrent(Qt::Vertical);
    else if (action == closeSplitAction)
        closeCurrentSplit();
    else if (action == settingsAction)
        Q_EMIT requestSettings();
}

void TermPane::showSearchBar() {
    if (!m_searchBar)
        return;
    m_searchBar->raise();
    m_searchBar->show();
    m_searchBar->move(width() - m_searchBar->width(), 0);
    m_searchBar->setFocusOnEdit();
}

void TermPane::hideSearchBar() {
    if (!m_searchBar)
        return;
    m_searchBar->hide();
    if (m_currentTerm)
        m_currentTerm->clearSearch();
    if (m_currentTerm)
        m_currentTerm->setFocus();
}

void TermPane::onSearchKeywordChanged(const QString &keyword) {
    if (!m_currentTerm)
        return;
    m_currentTerm->performSearch(keyword);
    m_searchBar->setNoMatchAlert(!keyword.isEmpty() && !m_currentTerm->hasSearchMatches());
}

void TermPane::onSearchFindNext() {
    if (!m_currentTerm)
        return;
    m_currentTerm->findNext();
    m_searchBar->setNoMatchAlert(!m_currentTerm->hasSearchMatches());
}

void TermPane::onSearchFindPrev() {
    if (!m_currentTerm)
        return;
    m_currentTerm->findPrevious();
    m_searchBar->setNoMatchAlert(!m_currentTerm->hasSearchMatches());
}

void TermPane::focusNavigation(Qt::Edge dir) {
    if (!m_currentTerm)
        return;

    auto getComparePoint = [](TerminalWidget *term, Qt::Edge edge) -> QPoint {
        QPoint leftTop = term->mapTo(term->window(), QPoint(0, 0));
        QPoint leftBottom = term->mapTo(term->window(), QPoint(0, term->height()));
        QPoint rightTop = term->mapTo(term->window(), QPoint(term->width(), 0));
        switch (edge) {
            case Qt::LeftEdge:
                return leftTop + QPoint(-1, 1);
            case Qt::RightEdge:
                return rightTop + QPoint(1, 1);
            case Qt::TopEdge:
                return leftTop + QPoint(1, -1);
            case Qt::BottomEdge:
                return leftBottom + QPoint(1, 1);
            default:
                return leftTop;
        }
    };

    auto getTermRect = [](TerminalWidget *term) -> QRect {
        QPoint leftTop = term->mapTo(term->window(), QPoint(0, 0));
        QPoint rightBottom = term->mapTo(term->window(), QPoint(term->width(), term->height()));
        return QRect(leftTop, rightBottom);
    };

    QPoint comparPoint = getComparePoint(m_currentTerm, dir);
    for (TerminalWidget *term : findChildren<TerminalWidget *>()) {
        if (term == m_currentTerm)
            continue;
        if (getTermRect(term).contains(comparPoint)) {
            setCurrentTerminal(term);
            term->setFocus();
            return;
        }
    }
}

void TermPane::closeOtherTerminals() {
    QList<TerminalWidget *> terms = terminalsInVisualOrder();
    if (terms.count() < 2)
        return;

    QList<TerminalWidget *> toRemove;
    for (TerminalWidget *term : terms) {
        if (term != m_currentTerm)
            toRemove.append(term);
    }
    m_deferPaneStructureChanged = true;
    for (TerminalWidget *term : toRemove)
        removeTerminal(term);
    m_deferPaneStructureChanged = false;

    if (m_pendingPaneStructureChanged) {
        m_pendingPaneStructureChanged = false;
        notifyPaneStructureChanged();
    }
}

void TermPane::executeCommand(const QString &command) {
    if (!m_currentTerm || command.isEmpty())
        return;
    if (m_currentTerm->m_ptySession) {
        m_currentTerm->m_ptySession->write(command.toUtf8());
        m_currentTerm->m_ptySession->write("\n");
    }
}

void TermPane::setCustomTitle(const QString &title) {
    if (!m_currentTerm)
        return;
    m_currentTerm->setProperty("customTitle", title);
    m_currentTerm->setProperty("currentTitle", title);
    Q_EMIT paneTitleChanged(ensurePaneId(m_currentTerm), title);
    Q_EMIT terminalTitleChanged(title);
}

void TermPane::connectToRemoteServer(const ServerConfig &config) {
    if (!m_currentTerm)
        return;

    // Read the expect script template from resources
    QFile sourceFile(":/other/ssh_login.sh");
    QString fileString;
    if (sourceFile.open(QIODevice::ReadOnly)) {
        fileString = sourceFile.readAll();
        sourceFile.close();
    } else {
        qWarning() << "Failed to open ssh_login.sh resource";
        return;
    }

    // Build arguments string
    QString strArgs = QString(" '%1' %2 %3 '%4' '%5'")
                          .arg(config.m_userName, config.m_address.trimmed(), config.m_port,
                               config.m_privateKey.isEmpty() ? "NoPrivateKeyPath" : config.m_privateKey,
                               config.m_password.toLatin1().toHex());

    // Replace placeholders in script
    if (config.m_privateKey.isEmpty()) {
        fileString.replace("<<AUTHENTICATION>>", "no");
    } else {
        fileString.replace("<<AUTHENTICATION>>", "yes");
    }

    QString remoteCommand;
    if (!config.m_path.isEmpty())
        remoteCommand += "cd " + config.m_path + " && ";
    if (!config.m_command.isEmpty())
        remoteCommand += config.m_command + " && ";

    fileString.replace("<<REMOTE_COMMAND>>", remoteCommand);

    // Write temporary script
    QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString toFileStr =
        tmpDir + "/deepin-terminal-ghostty-" + QString::number(QRandomGenerator::global()->bounded(100000, 999999));
    QFile toFile(toFileStr);
    if (!toFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create temporary expect script";
        return;
    }
    toFile.write(fileString.toUtf8());
    toFile.close();
    toFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    // Execute expect command
    QString expectCmd = QString("expect -f %1 %2\n").arg(toFileStr, strArgs);
    executeCommand(expectCmd.trimmed());

    // Set custom title to reflect remote connection
    if (!config.m_serverName.isEmpty())
        setCustomTitle(config.m_serverName);
}
