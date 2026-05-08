#pragma once

#include <QWidget>

class QAbstractScrollArea;
class QScrollBar;
class TerminalWidget;

class TerminalScrollContainer : public QWidget {
    Q_OBJECT

public:
    explicit TerminalScrollContainer(QWidget *parent = nullptr);

    TerminalWidget *terminal() const;
    QScrollBar *scrollBar() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateScrollBar();

    TerminalWidget *m_terminal = nullptr;
    QAbstractScrollArea *m_scrollHost = nullptr;
    QScrollBar *m_scrollBar = nullptr;
    bool m_updatingScrollBar = false;
};
