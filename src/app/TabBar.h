#pragma once

#include <DTabBar>

DWIDGET_USE_NAMESPACE

class TabBar : public DTabBar {
    Q_OBJECT

public:
    explicit TabBar(QWidget *parent = nullptr);

signals:
    void tabMenuRequested(int index);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void handleMiddleButtonClick(QMouseEvent *mouseEvent);
    bool handleRightButtonClick(QMouseEvent *mouseEvent);
};
