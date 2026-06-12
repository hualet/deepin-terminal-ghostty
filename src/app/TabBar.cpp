#include "TabBar.h"

#include <QMouseEvent>

TabBar::TabBar(QWidget *parent) : DTabBar(parent) {
    installEventFilter(this);
}

bool TabBar::eventFilter(QObject *watched, QEvent *event) {
    Q_UNUSED(watched)

    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::MiddleButton)
            handleMiddleButtonClick(mouseEvent);
    }

    return false;
}

void TabBar::handleMiddleButtonClick(QMouseEvent *mouseEvent) {
    const QPoint position = mouseEvent->pos();

    for (int i = 0; i < count(); ++i) {
        if (tabRect(i).contains(position)) {
            Q_EMIT tabCloseRequested(i);
            break;
        }
    }
}
