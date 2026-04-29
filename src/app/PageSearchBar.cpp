#include "PageSearchBar.h"

#include <QGraphicsOpacityEffect>
#include <QShortcut>
#include <QToolButton>

PageSearchBar::PageSearchBar(QWidget *parent) : DFloatingWidget(parent) {
    setObjectName(QStringLiteral("pageSearchBar"));
    setAccessibleName(tr("Terminal search"));
    setAccessibleDescription(tr("Search text in the active terminal."));
    hide();
    setFixedSize(kBarWidth, kBarHeight);

    auto *opacityEffect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(opacityEffect);
    opacityEffect->setOpacity(kOpacity);

    initSearchEdit();
    initFindNextButton();
    initFindPrevButton();

    auto *layout = new QHBoxLayout();
    layout->setSpacing(kWidgetSpace);
    layout->setContentsMargins(kLayoutMargin, kLayoutMargin, kLayoutMargin, kLayoutMargin);
    layout->setAlignment(Qt::AlignVCenter);
    layout->addWidget(m_searchEdit);
    layout->addWidget(m_findPrevButton);
    layout->addWidget(m_findNextButton);
    setLayout(layout);

    auto *escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escShortcut, &QShortcut::activated, this, &PageSearchBar::closeSearchBar);
}

QString PageSearchBar::searchText() const {
    return m_searchEdit->text();
}

void PageSearchBar::setFocusOnEdit() {
    m_searchEdit->lineEdit()->setFocus();
    m_searchEdit->lineEdit()->selectAll();
}

void PageSearchBar::setNoMatchAlert(bool alert) {
    m_searchEdit->setAlert(alert);
}

void PageSearchBar::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            if (m_searchEdit->lineEdit()->hasFocus()) {
                auto mods = event->modifiers();
                if (mods == Qt::ShiftModifier || mods == (Qt::ShiftModifier | Qt::KeypadModifier)) {
                    Q_EMIT findPrev();
                } else if (mods == Qt::NoModifier || mods == Qt::KeypadModifier) {
                    Q_EMIT findNext();
                }
            }
            break;
        default:
            DFloatingWidget::keyPressEvent(event);
            break;
    }
}

void PageSearchBar::initFindPrevButton() {
    m_findPrevButton = new DIconButton(QStyle::SP_ArrowUp);
    m_findPrevButton->setObjectName(QStringLiteral("findPreviousButton"));
    m_findPrevButton->setAccessibleName(tr("Find previous"));
    m_findPrevButton->setAccessibleDescription(tr("Find the previous search match."));
    m_findPrevButton->setFixedSize(kWidgetHeight, kWidgetHeight);
    m_findPrevButton->setFocusPolicy(Qt::TabFocus);
    connect(m_findPrevButton, &DIconButton::clicked, this, [this]() {
        if (!m_searchEdit->lineEdit()->text().isEmpty())
            Q_EMIT findPrev();
    });
}

void PageSearchBar::initFindNextButton() {
    m_findNextButton = new DIconButton(QStyle::SP_ArrowDown);
    m_findNextButton->setObjectName(QStringLiteral("findNextButton"));
    m_findNextButton->setAccessibleName(tr("Find next"));
    m_findNextButton->setAccessibleDescription(tr("Find the next search match."));
    m_findNextButton->setFixedSize(kWidgetHeight, kWidgetHeight);
    m_findNextButton->setFocusPolicy(Qt::TabFocus);
    connect(m_findNextButton, &DIconButton::clicked, this, [this]() {
        if (!m_searchEdit->lineEdit()->text().isEmpty())
            Q_EMIT findNext();
    });
}

void PageSearchBar::initSearchEdit() {
    m_searchEdit = new DSearchEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("terminalSearchEdit"));
    m_searchEdit->setAccessibleName(tr("Search text"));
    m_searchEdit->setAccessibleDescription(tr("Enter text to search in the active terminal."));
    m_searchEdit->setFocusPolicy(Qt::StrongFocus);
    m_searchEdit->setFocusProxy(m_searchEdit->lineEdit());

    // Make the clear button only empty text, not close the bar
    QList<QToolButton *> list = m_searchEdit->lineEdit()->findChildren<QToolButton *>();
    for (QToolButton *btn : list) {
        btn->disconnect(SIGNAL(clicked()));
        connect(btn, &QToolButton::clicked, this, [this]() {
            m_searchEdit->lineEdit()->setText("");
            Q_EMIT keywordChanged("");
        });
    }

    connect(m_searchEdit, &DSearchEdit::textChanged, this,
            [this]() { Q_EMIT keywordChanged(m_searchEdit->lineEdit()->text()); });
}
