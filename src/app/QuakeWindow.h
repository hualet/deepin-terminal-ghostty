#pragma once

#include "MainWindow.h"
#include "StartupOptions.h"

#include <QPointer>
#include <QRect>

class QPropertyAnimation;

class QuakeWindow : public MainWindow {
    Q_OBJECT

public:
    explicit QuakeWindow(const StartupOptions &startupOptions = {}, QWidget *parent = nullptr);

    bool isQuakeMode() const;
    QRect targetGeometry() const;
    void showQuake();
    void hideQuake();

#ifdef QTGHOSTTY_TESTING
    void debugSetAnimationDuration(int durationMs);
    void debugHandleActivationChange(bool active);
#endif

protected:
    void changeEvent(QEvent *event) override;

private:
    void applyQuakePresentation();
    QRect calculateTargetGeometry() const;
    void handleActivationChange(bool active);
    bool hasActiveOwnedDialog() const;
    void startGeometryAnimation(const QRect &endGeometry, bool hideWhenFinished);

    QPointer<QPropertyAnimation> m_geometryAnimation;
    QRect m_targetGeometry;
    int m_animationDurationMs = 160;
};
