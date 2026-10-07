#pragma once

#include <QPixmap>
#include <QPointer>
#include <QWidget>

class QVariantAnimation;

// Paint-only overlay that morphs between the terminal view and the
// workspace overview. Progress 0 shows the terminal frame, 1 the overview.
class OverviewTransition : public QWidget {
    Q_OBJECT

public:
    explicit OverviewTransition(QWidget *parent);

    // Shows the overlay holding the frame that is currently on screen.
    void begin(const QPixmap &terminalFrame, const QPixmap &overviewFrame, bool entering);
    // Supplies the frame being transitioned to and starts animating.
    void start(const QPixmap &terminalFrame, const QPixmap &overviewFrame, const QRect &paneRect, const QRect &cardRect,
               int durationMs);
    void finish();
    bool isRunning() const;
    double progress() const { return m_progress; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_terminalFrame;
    QPixmap m_overviewFrame;
    QRect m_paneRect;
    QRect m_cardRect;
    double m_progress = 0.0;
    bool m_entering = true;
    QPointer<QVariantAnimation> m_animation;
};
