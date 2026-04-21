#pragma once

#include <DMainWindow>
#include <DTabBar>

DWIDGET_USE_NAMESPACE

class QStackedWidget;
class TerminalWidget;

class MainWindow : public DMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onTabAddRequested();
    void onTabCloseRequested(int index);
    void onTabCurrentChanged(int index);
    void onTerminalTitleChanged(const QString &title);

private:
    void setupTitleBar();
    void addTab(bool activate = true);
    TerminalWidget *currentTerminal() const;

    DTabBar *m_tabBar = nullptr;
    QStackedWidget *m_stackWidget = nullptr;
};
