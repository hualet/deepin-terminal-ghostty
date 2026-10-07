#pragma once

#include <QHash>
#include <QList>
#include <QPixmap>
#include <QWidget>

class QAbstractButton;
class QGridLayout;
class QLabel;
class QLineEdit;
class QScrollArea;

class WorkspaceOverview : public QWidget {
    Q_OBJECT

public:
    struct Entry {
        int id = 0;
        QString title;
        QPixmap preview;
        bool current = false;
    };

    explicit WorkspaceOverview(QWidget *parent = nullptr);
    void setEntries(const QList<Entry> &entries);
    void focusSearch();

signals:
    void tabActivated(int tabId);
    void tabCloseRequested(int tabId);
    void addTabRequested();
    void dismissRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool focusNextPrevChild(bool next) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void reflow();
    void navigate(int key);
    QList<QAbstractButton *> matchingCards() const;

    QList<Entry> m_entries;
    QHash<int, QAbstractButton *> m_cards;
    QLineEdit *m_search = nullptr;
    QLabel *m_count = nullptr;
    QLabel *m_empty = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_gridHost = nullptr;
    QGridLayout *m_grid = nullptr;
    int m_columns = 1;
};
