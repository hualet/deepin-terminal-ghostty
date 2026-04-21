#pragma once

#include <QWidget>

class PageSearchBar;
class QVBoxLayout;
class QSplitter;
class TerminalWidget;

class TermPane : public QWidget {
    Q_OBJECT

public:
    explicit TermPane(QWidget *parent = nullptr);

    TerminalWidget *currentTerminal() const;
    void splitCurrent(Qt::Orientation orientation);
    void closeCurrentSplit();

protected:
    void resizeEvent(QResizeEvent *event) override;

signals:
    void terminalTitleChanged(const QString &title);
    void sessionClosed();
    void currentTerminalChanged(TerminalWidget *term);

private:
    TerminalWidget *createTerminal();
    void setupTerminalConnections(TerminalWidget *term);
    void setCurrentTerminal(TerminalWidget *term);
    void removeTerminal(TerminalWidget *term);
    void showSearchBar();
    void hideSearchBar();
    void onSearchKeywordChanged(const QString &keyword);
    void onSearchFindNext();
    void onSearchFindPrev();

    QVBoxLayout *m_layout = nullptr;
    QWidget *m_rootWidget = nullptr;
    TerminalWidget *m_currentTerm = nullptr;
    PageSearchBar *m_searchBar = nullptr;
};
