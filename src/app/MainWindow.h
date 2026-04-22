#pragma once

#include <DMainWindow>
#include <DTabBar>
#include <QList>
#include <QPointer>
#include <QString>

class QShortcut;
class SettingsDialog;
class QAction;
class QSplitter;
class QWidget;

DWIDGET_USE_NAMESPACE

class QStackedWidget;
class TermPane;
class TerminalWidget;
class VerticalTabSidebar;

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
    struct TabRecord {
        int id = 0;
        TermPane *pane = nullptr;
        QString title;
        bool expanded = true;
    };

    DTabBar *ensureTabBar();
    QWidget *ensureTabTitlebarWidget();
    QWidget *ensureCompactTitlebarWidget();
    void detachTabBarFromTitlebarWidget();
    void setupTitleBar();
    void addTab(bool activate = true);
    void closePane(TermPane *pane);
    TermPane *currentPane() const;
    TerminalWidget *currentTerminal() const;
    int indexOfTabId(int tabId) const;
    TabRecord *tabRecordForPane(TermPane *pane);
    void refreshTabRecord(TabRecord &record);
    void refreshTabRecords();
    void syncTabWidgetsFromRecords();
    void refreshSidebar();
    void setVerticalTabsEnabled(bool enabled);
    void rebuildCentralLayout();
    void updateTitlebarPresentation();
    void setupShortcuts();
    void updateShortcut(QShortcut *shortcut, const QString &name);
    void closeOtherTabs();
    void gotoTab(int index);
    void onShortcutRenameTitle();
    void onShortcutDisplayShortcuts();
    void onShortcutCustomCommand();
    void onShortcutRemoteManagement();

    QPointer<DTabBar> m_tabBar;
    QStackedWidget *m_stackWidget = nullptr;
    QWidget *m_contentHost = nullptr;
    SettingsDialog *m_settingsDialog = nullptr;
    VerticalTabSidebar *m_verticalSidebar = nullptr;
    QSplitter *m_mainSplitter = nullptr;
    QAction *m_verticalTabsAction = nullptr;
    bool m_verticalTabsEnabled = false;
    int m_nextTabId = 1;
    QList<TabRecord> m_tabs;
    QPointer<QWidget> m_tabTitlebarWidget;
    QPointer<QWidget> m_compactTitlebarWidget;

    QShortcut *m_scNewTab = nullptr;
    QShortcut *m_scCloseTab = nullptr;
    QShortcut *m_scCloseOtherTabs = nullptr;
    QShortcut *m_scPrevTab = nullptr;
    QShortcut *m_scNextTab = nullptr;
    QShortcut *m_scVSplit = nullptr;
    QShortcut *m_scHSplit = nullptr;
    QShortcut *m_scCloseWorkspace = nullptr;
    QShortcut *m_scCloseOtherWorkspaces = nullptr;
    QShortcut *m_scSelectUpper = nullptr;
    QShortcut *m_scSelectLower = nullptr;
    QShortcut *m_scSelectLeft = nullptr;
    QShortcut *m_scSelectRight = nullptr;
    QShortcut *m_scFullscreen = nullptr;
    QShortcut *m_scRenameTitle = nullptr;
    QShortcut *m_scDisplayShortcuts = nullptr;
    QShortcut *m_scCustomCommand = nullptr;
    QShortcut *m_scRemoteManagement = nullptr;
};
