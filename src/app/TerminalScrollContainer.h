#pragma once

#include "TerminalWidget.h"

#include <QWidget>

class QAbstractScrollArea;
class QProgressBar;
class QScrollBar;

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
    void updateProgress(TerminalWidget::ProgressState state, int progress);

    TerminalWidget *m_terminal = nullptr;
    QAbstractScrollArea *m_scrollHost = nullptr;
    QScrollBar *m_scrollBar = nullptr;
    QProgressBar *m_progressBar = nullptr;
    bool m_updatingScrollBar = false;
};
