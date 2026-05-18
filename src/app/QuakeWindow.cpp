#include "QuakeWindow.h"

#include "AppSettings.h"

#include <DTitlebar>
#include <QApplication>
#include <QCursor>
#include <QDialog>
#include <QEvent>
#include <QGuiApplication>
#include <QPropertyAnimation>
#include <QScreen>

DWIDGET_USE_NAMESPACE

namespace {

constexpr int kQuakeHeightNumerator = 2;
constexpr int kQuakeHeightDenominator = 5;

QScreen *screenForQuakeWindow() {
    if (QScreen *screen = QGuiApplication::screenAt(QCursor::pos()))
        return screen;
    return QGuiApplication::primaryScreen();
}

} // namespace

QuakeWindow::QuakeWindow(const StartupOptions &startupOptions, QWidget *parent) : MainWindow(startupOptions, parent) {
    setObjectName(QStringLiteral("quakeWindow"));
    applyQuakePresentation();
}

bool QuakeWindow::isQuakeMode() const {
    return true;
}

QRect QuakeWindow::targetGeometry() const {
    return m_targetGeometry.isValid() ? m_targetGeometry : calculateTargetGeometry();
}

QRect QuakeWindow::calculateTargetGeometry() const {
    const QScreen *screen = screenForQuakeWindow();
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 800, 600);
    return QRect(available.x(), available.y(), available.width(),
                 qMax(1, available.height() * kQuakeHeightNumerator / kQuakeHeightDenominator));
}

void QuakeWindow::applyQuakePresentation() {
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    setWindowFlag(Qt::FramelessWindowHint, true);
    if (DTitlebar *bar = titlebar())
        bar->setFixedHeight(0);
    m_targetGeometry = calculateTargetGeometry();
    setGeometry(targetGeometry());
}

void QuakeWindow::showQuake() {
    m_targetGeometry = calculateTargetGeometry();
    const QRect start(m_targetGeometry.x(), m_targetGeometry.y(), m_targetGeometry.width(), 0);
    setGeometry(start);
    show();
    raise();
    activateWindow();
    startGeometryAnimation(m_targetGeometry, false);
}

void QuakeWindow::hideQuake() {
    if (!isVisible())
        return;

    const QRect current = geometry();
    const QRect target(current.x(), current.y(), current.width(), 0);
    startGeometryAnimation(target, true);
}

#ifdef QTGHOSTTY_TESTING
void QuakeWindow::debugSetAnimationDuration(int durationMs) {
    m_animationDurationMs = qMax(0, durationMs);
}

void QuakeWindow::debugHandleActivationChange(bool active) {
    handleActivationChange(active);
}
#endif

void QuakeWindow::changeEvent(QEvent *event) {
    MainWindow::changeEvent(event);
    if (event && event->type() == QEvent::ActivationChange)
        handleActivationChange(isActiveWindow());
}

void QuakeWindow::handleActivationChange(bool active) {
    if (active || !isVisible() || !AppSettings::instance()->hideQuakeOnFocusLoss() || hasActiveOwnedDialog())
        return;

    hideQuake();
}

bool QuakeWindow::hasActiveOwnedDialog() const {
    QWidget *active = QApplication::activeModalWidget();
    if (!active)
        active = QApplication::activePopupWidget();
    if (!active)
        active = QApplication::activeWindow();

    while (active) {
        if (active == this)
            return false;
        if (active->window() == this || active->parentWidget() == this)
            return qobject_cast<QDialog *>(active) || active->isWindow();
        active = active->parentWidget();
    }

    return false;
}

void QuakeWindow::startGeometryAnimation(const QRect &endGeometry, bool hideWhenFinished) {
    if (m_geometryAnimation) {
        QPropertyAnimation *oldAnimation = m_geometryAnimation;
        m_geometryAnimation = nullptr;
        oldAnimation->stop();
    }

    if (m_animationDurationMs == 0) {
        setGeometry(endGeometry);
        if (hideWhenFinished)
            hide();
        return;
    }

    m_geometryAnimation = new QPropertyAnimation(this, "geometry", this);
    m_geometryAnimation->setDuration(m_animationDurationMs);
    m_geometryAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_geometryAnimation->setStartValue(geometry());
    m_geometryAnimation->setEndValue(endGeometry);
    QPointer<QPropertyAnimation> currentAnimation = m_geometryAnimation;
    connect(m_geometryAnimation, &QPropertyAnimation::finished, this, [this, currentAnimation, hideWhenFinished]() {
        if (hideWhenFinished)
            hide();
        if (m_geometryAnimation == currentAnimation)
            m_geometryAnimation = nullptr;
    });
    m_geometryAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}
