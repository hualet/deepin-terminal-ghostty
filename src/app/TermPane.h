#pragma once

#include "PtySession.h"
#include "SessionSnapshot.h"
#include "TerminalWidget.h"

#include <QList>
#include <QPair>
#include <QUuid>
#include <QWidget>

#include <optional>

struct ServerConfig;
class PageSearchBar;
class QVBoxLayout;
class QSplitter;
class TerminalWidget;

class TermPane : public QWidget {
    Q_OBJECT

public:
    struct PaneInfo {
        QUuid id;
        QString title;
        QString iconName;
        bool isActive = false;
        TerminalWidget::CommandState commandState = TerminalWidget::CommandState::Idle;
    };

    explicit TermPane(const std::optional<PtySession::StartOptions> &initialSessionOptions = std::nullopt,
                      QWidget *parent = nullptr);

    QList<PaneInfo> paneInfos() const;
    QUuid activePaneId() const;
    bool focusPane(const QUuid &paneId);
    int runningTerminalCount() const;

    TerminalWidget *currentTerminal() const;
    void splitCurrent(Qt::Orientation orientation);
    void closeCurrentSplit();
    void focusNavigation(Qt::Edge dir);
    void closeOtherTerminals();
    void executeCommand(const QString &command);
    void setCustomTitle(const QString &title);
    void setOpacity(qreal opacity);
    void connectToRemoteServer(const ServerConfig &config);

    SplitNode buildSplitTree() const;
    QList<QPair<QString, TerminalWidget *>> restoreFromSplitTree(const SplitNode &node);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void paneStructureChanged();
    void activePaneChanged(const QUuid &paneId);
    void paneTitleChanged(const QUuid &paneId, const QString &title);
    void paneCommandStateChanged(const QUuid &paneId, TerminalWidget::CommandState state);
    void terminalTitleChanged(const QString &title);
    void startupSessionExited(int exitCode);
    void sessionClosed();
    void currentTerminalChanged(TerminalWidget *term);
    void requestSettings();

private:
    TerminalWidget *createTerminal(const std::optional<PtySession::StartOptions> &options = std::nullopt);
    void setupTerminalConnections(TerminalWidget *term);
    void setCurrentTerminal(TerminalWidget *term);
    void removeTerminal(TerminalWidget *term);
    QSplitter *createPaneSplitter(Qt::Orientation orientation);
    QList<TerminalWidget *> terminalsInVisualOrder() const;
    void splitTerminal(TerminalWidget *term, TerminalWidget *newTerm, Qt::Orientation orientation);
    void promoteSingleChildSplitter(QSplitter *splitter);
    void notifyPaneStructureChanged();
    void showTerminalContextMenu(TerminalWidget *term, const QPoint &globalPos);
    void showSearchBar();
    void hideSearchBar();
    void onSearchKeywordChanged(const QString &keyword);
    void onSearchFindNext();
    void onSearchFindPrev();

    TerminalWidget *createTerminalWithUuid(const QString &uuid,
                                           const std::optional<PtySession::StartOptions> &options = std::nullopt);
    QList<QPair<QString, TerminalWidget *>> rebuildTreeRecursive(const SplitNode &node, TerminalWidget *sibling,
                                                                 Qt::Orientation parentOrientation);

    QVBoxLayout *m_layout = nullptr;
    QWidget *m_rootWidget = nullptr;
    TerminalWidget *m_currentTerm = nullptr;
    PageSearchBar *m_searchBar = nullptr;
    std::optional<PtySession::StartOptions> m_initialSessionOptions;
    TerminalWidget *m_startupTerminal = nullptr;
    bool m_deferPaneStructureChanged = false;
    bool m_pendingPaneStructureChanged = false;
};
