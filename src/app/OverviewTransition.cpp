#include "OverviewTransition.h"

#include <QEasingCurve>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

namespace {

constexpr qreal kCardRadius = 6.0;
constexpr int kShadowLayers = 6;

QRectF lerpRect(const QRectF &from, const QRectF &to, qreal t) {
    return QRectF(from.x() + (to.x() - from.x()) * t, from.y() + (to.y() - from.y()) * t,
                  from.width() + (to.width() - from.width()) * t, from.height() + (to.height() - from.height()) * t);
}

QRectF physicalRect(const QPixmap &pixmap, const QRect &rect) {
    const qreal dpr = pixmap.devicePixelRatio();
    return QRectF(rect.x() * dpr, rect.y() * dpr, rect.width() * dpr, rect.height() * dpr);
}

} // namespace

OverviewTransition::OverviewTransition(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("workspaceOverviewTransition"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::NoFocus);
    hide();
}

void OverviewTransition::begin(const QPixmap &terminalFrame, const QPixmap &overviewFrame, bool entering) {
    finish();
    m_terminalFrame = terminalFrame;
    m_overviewFrame = overviewFrame;
    m_paneRect = QRect();
    m_cardRect = QRect();
    m_entering = entering;
    m_progress = entering ? 0.0 : 1.0;
    setGeometry(parentWidget()->rect());
    show();
    raise();
    update();
}

void OverviewTransition::start(const QPixmap &terminalFrame, const QPixmap &overviewFrame, const QRect &paneRect,
                               const QRect &cardRect, int durationMs) {
    if (!isVisible())
        return;
    m_terminalFrame = terminalFrame;
    m_overviewFrame = overviewFrame;
    m_paneRect = paneRect;
    m_cardRect = cardRect;
    if (durationMs <= 0 || m_terminalFrame.isNull() || m_overviewFrame.isNull()) {
        finish();
        return;
    }
    auto *animation = new QVariantAnimation(this);
    animation->setDuration(durationMs);
    animation->setStartValue(m_entering ? 0.0 : 1.0);
    animation->setEndValue(m_entering ? 1.0 : 0.0);
    animation->setEasingCurve(m_entering ? QEasingCurve::OutCubic : QEasingCurve::InOutCubic);
    connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_progress = value.toDouble();
        update();
    });
    connect(animation, &QVariantAnimation::finished, this, &OverviewTransition::finish);
    m_animation = animation;
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void OverviewTransition::finish() {
    if (m_animation) {
        QVariantAnimation *animation = m_animation;
        m_animation = nullptr;
        animation->stop();
    }
    hide();
    m_terminalFrame = QPixmap();
    m_overviewFrame = QPixmap();
}

bool OverviewTransition::isRunning() const {
    return isVisible();
}

void OverviewTransition::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal t = qBound(0.0, m_progress, 1.0);

    if (!m_terminalFrame.isNull())
        painter.drawPixmap(rect(), m_terminalFrame);
    else
        painter.fillRect(rect(), palette().color(QPalette::Window));
    if (!m_overviewFrame.isNull()) {
        painter.setOpacity(m_terminalFrame.isNull() ? 1.0 : t);
        painter.drawPixmap(rect(), m_overviewFrame);
        painter.setOpacity(1.0);
    }

    if (m_terminalFrame.isNull() || m_paneRect.isEmpty() || m_cardRect.isEmpty())
        return;
    const QRectF target = lerpRect(QRectF(m_paneRect), QRectF(m_cardRect), t);
    const qreal radius = kCardRadius * t;
    painter.setRenderHint(QPainter::Antialiasing);
    // Soft shadow lifts the moving pane off backgrounds of similar color.
    painter.setPen(Qt::NoPen);
    const qreal lift = qMin(t, 1.0 - t) * 2.0;
    for (int i = kShadowLayers; i > 0; --i) {
        painter.setBrush(QColor(0, 0, 0, int(10 * lift)));
        painter.drawRoundedRect(target.adjusted(-i, -i + 2, i, i + 2), radius + i, radius + i);
    }
    QPainterPath clip;
    clip.addRoundedRect(target, radius, radius);
    painter.save();
    painter.setClipPath(clip);
    painter.drawPixmap(target, m_terminalFrame, physicalRect(m_terminalFrame, m_paneRect));
    painter.restore();
    QColor outline = palette().color(QPalette::Mid);
    outline.setAlphaF(outline.alphaF() * t);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(outline, 1));
    painter.drawRoundedRect(target.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
}
