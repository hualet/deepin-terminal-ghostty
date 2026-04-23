#pragma once

#include "PtySession.h"

#include <QList>
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
        bool isActive = false;
    };

    explicit TermPane(const std::optional<PtySession::StartOptions> &initialSessionOptions = std::nullopt,
                      QWidget *parent = nullptr);

    QList<PaneInfo> paneInfos() const;
    QUuid activePaneId() const;
    bool focusPane(const QUuid &paneId);

    TerminalWidget *currentTerminal() const;
    void splitCurrent(Qt::Orientation orientation);
    void closeCurrentSplit();
    void focusNavigation(Qt::Edge dir);
    void closeOtherTerminals();
    void executeCommand(const QString &command);
    void setCustomTitle(const QString &title);
    void connectToRemoteServer(const ServerConfig &config);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void paneStructureChanged();
    void activePaneChanged(const QUuid &paneId);
    void paneTitleChanged(const QUuid &paneId, const QString &title);
    void terminalTitleChanged(const QString &title);
    void startupSessionExited(int exitCode);
    void sessionClosed();
    void currentTerminalChanged(TerminalWidget *term);
    void requestSettings();

private:
    TerminalWidget *createTerminal();
    void setupTerminalConnections(TerminalWidget *term);
    void setCurrentTerminal(TerminalWidget *term);
    void removeTerminal(TerminalWidget *term);
    void showTerminalContextMenu(TerminalWidget *term, const QPoint &globalPos);
    void showSearchBar();
    void hideSearchBar();
    void onSearchKeywordChanged(const QString &keyword);
    void onSearchFindNext();
    void onSearchFindPrev();

    QVBoxLayout *m_layout = nullptr;
    QWidget *m_rootWidget = nullptr;
    TerminalWidget *m_currentTerm = nullptr;
    PageSearchBar *m_searchBar = nullptr;
    std::optional<PtySession::StartOptions> m_initialSessionOptions;
    TerminalWidget *m_startupTerminal = nullptr;
};
