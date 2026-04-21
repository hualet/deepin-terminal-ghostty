#pragma once

#include <QWidget>

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

signals:
    void terminalTitleChanged(const QString &title);
    void sessionClosed();
    void currentTerminalChanged(TerminalWidget *term);

private:
    TerminalWidget *createTerminal();
    void setupTerminalConnections(TerminalWidget *term);
    void setCurrentTerminal(TerminalWidget *term);
    void removeTerminal(TerminalWidget *term);

    QVBoxLayout *m_layout = nullptr;
    QWidget *m_rootWidget = nullptr;
    TerminalWidget *m_currentTerm = nullptr;
};
