#include "TerminalScrollContainer.h"

#include "TerminalWidget.h"

#include <QAbstractScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>

namespace {
constexpr int kFloatingScrollBarMargin = 2;
constexpr int kMinimumScrollBarWidth = 15;
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

    connect(m_terminal, &TerminalWidget::viewportScrollStateChanged, this, &TerminalScrollContainer::updateScrollBar);
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
}
