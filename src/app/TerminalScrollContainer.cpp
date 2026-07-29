#include "TerminalScrollContainer.h"

#include "TerminalWidget.h"

#include <QAbstractScrollArea>
#include <QProgressBar>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>

namespace {
constexpr int kFloatingScrollBarMargin = 2;
constexpr int kMinimumScrollBarWidth = 15;
constexpr int kProgressBarHeight = 3;
} // namespace

TerminalScrollContainer::TerminalScrollContainer(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("terminalScrollContainer"));
    setFocusPolicy(Qt::NoFocus);

    m_terminal = new TerminalWidget(this);
    setFocusProxy(m_terminal);

    m_scrollHost = new QAbstractScrollArea(this);
    m_scrollHost->setObjectName(QStringLiteral("terminalFloatingScrollHost"));
    m_scrollHost->setFrameShape(QFrame::NoFrame);
    m_scrollHost->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollHost->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollHost->setAttribute(Qt::WA_TranslucentBackground);
    m_scrollHost->viewport()->setAttribute(Qt::WA_TranslucentBackground);
    m_scrollHost->hide();

    m_scrollBar = m_scrollHost->verticalScrollBar();
    m_scrollBar->setObjectName(QStringLiteral("terminalFloatingScrollBar"));
    m_scrollBar->setContextMenuPolicy(Qt::NoContextMenu);
    m_scrollBar->setCursor(Qt::ArrowCursor);
    m_scrollBar->setStyleSheet(QStringLiteral("margin: 0px 0 15px 0;width: 15"));
    if (!m_scrollBar->style()->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, m_scrollBar))
        m_scrollBar->setAutoFillBackground(true);
    m_scrollBar->hide();

    m_progressBar = new QProgressBar(this);
    m_progressBar->setObjectName(QStringLiteral("terminalProgressBar"));
    m_progressBar->setTextVisible(false);
    m_progressBar->setRange(0, 100);
    m_progressBar->setStyleSheet(
        QStringLiteral("QProgressBar { border: none; background: transparent; }"
                       "QProgressBar::chunk { background: #2ca7f8; }"
                       "QProgressBar[progressState=\"pause\"]::chunk { background: #e6a23c; }"
                       "QProgressBar[progressState=\"error\"]::chunk { background: #e64545; }"));
    m_progressBar->hide();

    connect(m_terminal, &TerminalWidget::viewportScrollStateChanged, this, &TerminalScrollContainer::updateScrollBar);
    connect(m_terminal, &TerminalWidget::progressChanged, this, &TerminalScrollContainer::updateProgress);
    connect(m_scrollBar, &QScrollBar::valueChanged, this, [this](int value) {
        if (m_updatingScrollBar)
            return;
        m_terminal->scrollViewportToOffset(value);
    });
}

TerminalWidget *TerminalScrollContainer::terminal() const {
    return m_terminal;
}

QScrollBar *TerminalScrollContainer::scrollBar() const {
    return m_scrollBar;
}

void TerminalScrollContainer::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);

    if (m_terminal->geometry() != rect())
        m_terminal->setGeometry(rect());

    const int scrollBarWidth = qMax(
        kMinimumScrollBarWidth, m_scrollBar->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, m_scrollBar));
    const QRect scrollHostRect(width() - scrollBarWidth - kFloatingScrollBarMargin, kFloatingScrollBarMargin,
                               scrollBarWidth, qMax(0, height() - 2 * kFloatingScrollBarMargin));
    if (m_scrollHost->geometry() != scrollHostRect)
        m_scrollHost->setGeometry(scrollHostRect);
    m_scrollHost->raise();

    m_progressBar->setGeometry(0, qMax(0, height() - kProgressBarHeight), width(), kProgressBarHeight);
    m_progressBar->raise();
}

void TerminalScrollContainer::updateProgress(TerminalWidget::ProgressState state, int progress) {
    if (state == TerminalWidget::ProgressState::Remove) {
        m_progressBar->hide();
        return;
    }

    QString stateName;
    if (state == TerminalWidget::ProgressState::Indeterminate) {
        stateName = QStringLiteral("indeterminate");
        m_progressBar->setRange(0, 0);
    } else {
        m_progressBar->setRange(0, 100);
        if (progress >= 0)
            m_progressBar->setValue(qBound(0, progress, 100));

        switch (state) {
            case TerminalWidget::ProgressState::Set:
                stateName = QStringLiteral("set");
                break;
            case TerminalWidget::ProgressState::Pause:
                stateName = QStringLiteral("pause");
                break;
            case TerminalWidget::ProgressState::Error:
                stateName = QStringLiteral("error");
                break;
            default:
                return;
        }
    }

    m_progressBar->setProperty("progressState", stateName);
    m_progressBar->style()->unpolish(m_progressBar);
    m_progressBar->style()->polish(m_progressBar);
    m_progressBar->show();
    m_progressBar->raise();
}

void TerminalScrollContainer::updateScrollBar() {
    const TerminalWidget::ViewportScrollState state = m_terminal->viewportScrollState();
    const bool visible = state.canScroll();

    m_updatingScrollBar = true;
    const QSignalBlocker blocker(m_scrollBar);
    m_scrollHost->setVerticalScrollBarPolicy(visible ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff);
    m_scrollBar->setRange(0, state.maximumOffset());
    m_scrollBar->setSingleStep(1);
    m_scrollBar->setPageStep(qMax(1, state.visibleRows));
    m_scrollBar->setValue(qBound(0, state.offset, state.maximumOffset()));
    m_updatingScrollBar = false;

    m_scrollHost->setVisible(visible);
    if (visible) {
        m_scrollBar->show();
        m_scrollHost->raise();
    }
    if (m_progressBar->isVisible())
        m_progressBar->raise();
}
