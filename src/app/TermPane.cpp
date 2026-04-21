#include "TermPane.h"

#include "AppSettings.h"
#include "PageSearchBar.h"
#include "PtySession.h"
#include "TerminalWidget.h"

#include <QClipboard>
#include <QContextMenuEvent>
#include <QDebug>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QResizeEvent>
#include <QSplitter>
#include <QVBoxLayout>

namespace {

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

} // namespace

TermPane::TermPane(QWidget *parent) : QWidget(parent) {
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

TerminalWidget *TermPane::currentTerminal() const {
    return m_currentTerm;
}

TerminalWidget *TermPane::createTerminal() {
    auto *term = new TerminalWidget(this);
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
        if (m_currentTerm == term)
            Q_EMIT terminalTitleChanged(displayTitle);
    });
    connect(term, &TerminalWidget::sessionClosed, this, [this, term]() { removeTerminal(term); });
    connect(term, &TerminalWidget::focusGained, this, [this, term]() { setCurrentTerminal(term); });
}

void TermPane::setCurrentTerminal(TerminalWidget *term) {
    if (m_currentTerm == term)
        return;
    m_currentTerm = term;
    Q_EMIT currentTerminalChanged(term);
}

void TermPane::splitCurrent(Qt::Orientation orientation) {
    if (!m_currentTerm)
        return;

    TerminalWidget *term = m_currentTerm;
    auto *splitter = qobject_cast<QSplitter *>(term->parentWidget());

    if (splitter && splitter->orientation() == orientation) {
        // Same orientation: split the occupied space evenly
        int index = splitter->indexOf(term);
        QList<int> sizes = splitter->sizes();
        int half = sizes.at(index) / 2;
        sizes[index] = half;
        sizes.insert(index + 1, half);
        TerminalWidget *newTerm = createTerminal();
        splitter->insertWidget(index + 1, newTerm);
        splitter->setSizes(sizes);
        setCurrentTerminal(newTerm);
    } else if (splitter) {
        // Different orientation: create a sub-splitter that occupies the same space
        int index = splitter->indexOf(term);
        QList<int> sizes = splitter->sizes();
        auto *newSplitter = new QSplitter(orientation, this);
        newSplitter->setChildrenCollapsible(false);
        newSplitter->setHandleWidth(1);
        term->setParent(nullptr);
        TerminalWidget *newTerm = createTerminal();
        newSplitter->addWidget(term);
        newSplitter->addWidget(newTerm);
        newSplitter->setSizes({1, 1});
        splitter->insertWidget(index, newSplitter);
        splitter->setSizes(sizes);
        setCurrentTerminal(newTerm);
    } else {
        // No splitter yet: replace the root widget with a splitter
        auto *newSplitter = new QSplitter(orientation, this);
        newSplitter->setChildrenCollapsible(false);
        newSplitter->setHandleWidth(1);
        m_layout->removeWidget(term);
        term->setParent(nullptr);
        TerminalWidget *newTerm = createTerminal();
        newSplitter->addWidget(term);
        newSplitter->addWidget(newTerm);
        newSplitter->setSizes({1, 1});
        m_layout->addWidget(newSplitter);
        m_rootWidget = newSplitter;
        setCurrentTerminal(newTerm);
    }
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
        term->deleteLater();
        Q_EMIT sessionClosed();
        return;
    }

    int index = splitter->indexOf(term);
    term->setParent(nullptr);
    term->deleteLater();

    // If the splitter now has only one widget left, promote it
    if (splitter->count() == 1) {
        QWidget *remaining = splitter->widget(0);
        remaining->setParent(nullptr);
        auto *parentSplitter = qobject_cast<QSplitter *>(splitter->parentWidget());
        if (parentSplitter) {
            int parentIndex = parentSplitter->indexOf(splitter);
            QList<int> sizes = parentSplitter->sizes();
            parentSplitter->insertWidget(parentIndex, remaining);
            sizes.removeAt(parentIndex);
            sizes.insert(parentIndex, splitter->sizes().first());
            parentSplitter->setSizes(sizes);
            splitter->setParent(nullptr);
            splitter->deleteLater();
        } else {
            m_layout->removeWidget(splitter);
            splitter->setParent(nullptr);
            splitter->deleteLater();
            m_layout->addWidget(remaining);
            m_rootWidget = remaining;
        }
    }

    // Update current terminal if the removed one was current
    if (m_currentTerm == term) {
        if (splitter->count() > 0) {
            int newIndex = qMin(index, splitter->count() - 1);
            if (auto *newTerm = qobject_cast<TerminalWidget *>(splitter->widget(newIndex)))
                setCurrentTerminal(newTerm);
        } else {
            m_currentTerm = nullptr;
        }
    }
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
    QList<TerminalWidget *> terms = findChildren<TerminalWidget *>();
    if (terms.count() < 2)
        return;

    QList<TerminalWidget *> toRemove;
    for (TerminalWidget *term : terms) {
        if (term != m_currentTerm)
            toRemove.append(term);
    }
    for (TerminalWidget *term : toRemove)
        removeTerminal(term);
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
    Q_EMIT terminalTitleChanged(title);
}
