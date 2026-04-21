#pragma once

#include <DMainWindow>
#include <DTabBar>

class SettingsDialog;

DWIDGET_USE_NAMESPACE

class QStackedWidget;
class TermPane;
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
    void onTerminalSessionClosed();
    void onPaneTerminalChanged(TerminalWidget *term);
    void onSettingsTriggered();

private:
    void setupTitleBar();
    void addTab(bool activate = true);
    void closePane(TermPane *pane);
    TermPane *currentPane() const;
    TerminalWidget *currentTerminal() const;

    DTabBar *m_tabBar = nullptr;
    QStackedWidget *m_stackWidget = nullptr;
    SettingsDialog *m_settingsDialog = nullptr;
};
