#pragma once

#include <DFloatingWidget>
#include <DIconButton>
#include <DSearchEdit>
#include <QHBoxLayout>
#include <QKeyEvent>

DWIDGET_USE_NAMESPACE

class PageSearchBar : public DFloatingWidget {
    Q_OBJECT

public:
    explicit PageSearchBar(QWidget *parent = nullptr);

    QString searchText() const;
    void setFocusOnEdit();
    void setNoMatchAlert(bool alert);

signals:
    void findNext();
    void findPrev();
    void keywordChanged(const QString &keyword);
    void closeSearchBar();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void initFindPrevButton();
    void initFindNextButton();
    void initSearchEdit();

    DIconButton *m_findNextButton = nullptr;
    DIconButton *m_findPrevButton = nullptr;
    DSearchEdit *m_searchEdit = nullptr;

    static constexpr int kBarWidth = 382;
    static constexpr int kBarHeight = 62;
    static constexpr int kLayoutMargin = 7;
    static constexpr int kWidgetHeight = 36;
    static constexpr int kWidgetSpace = 10;
    static constexpr qreal kOpacity = 0.9;
};
