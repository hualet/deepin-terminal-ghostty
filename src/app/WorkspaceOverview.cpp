#include "WorkspaceOverview.h"

#include <DStyle>
#include <QAbstractButton>
#include <QApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSet>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

class OverviewCard : public QAbstractButton {
public:
    explicit OverviewCard(QWidget *parent) : QAbstractButton(parent) {
        setObjectName(QStringLiteral("workspaceOverviewCard"));
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(220);
        setCheckable(true);
        m_close = new QToolButton(this);
        m_close->setObjectName(QStringLiteral("workspaceOverviewCloseTab"));
        m_close->setIcon(Dtk::Widget::DStyle::standardIcon(style(), Dtk::Widget::DStyle::SP_CloseButton));
        m_close->setIconSize(QSize(16, 16));
        m_close->setAutoRaise(true);
        m_close->setFixedSize(28, 28);
        m_close->setToolTip(WorkspaceOverview::tr("Close tab"));
        m_close->setAccessibleName(WorkspaceOverview::tr("Close tab"));
    }

    void setEntry(const WorkspaceOverview::Entry &entry) {
        m_preview = entry.preview;
        setIcon(QIcon(m_preview));
        setText(entry.title);
        setChecked(entry.current);
        setProperty("tabId", entry.id);
        setToolTip(entry.title);
        setAccessibleName(WorkspaceOverview::tr("Terminal tab: %1").arg(entry.title));
        m_close->setAccessibleName(WorkspaceOverview::tr("Close tab: %1").arg(entry.title));
        update();
    }

    QToolButton *closeButton() const { return m_close; }

    QRect previewRect() const {
        const QRect imageRect = rect().adjusted(8, 8, -8, -44);
        if (m_preview.isNull())
            return imageRect;
        const QSize scaled = m_preview.size().scaled(imageRect.size(), Qt::KeepAspectRatio);
        return QRect(imageRect.center().x() - scaled.width() / 2, imageRect.center().y() - scaled.height() / 2,
                     scaled.width(), scaled.height());
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QAbstractButton::resizeEvent(event);
        m_close->move(width() - 36, height() - 36);
    }

    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF frame = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
        QColor background = palette().color(QPalette::Base);
        if (underMouse())
            background = palette().color(QPalette::AlternateBase);
        painter.setBrush(background);
        const QColor border =
            (hasFocus() || isChecked()) ? palette().color(QPalette::Highlight) : palette().color(QPalette::Mid);
        painter.setPen(QPen(border, hasFocus() ? 3 : (isChecked() ? 2 : 1)));
        painter.drawRoundedRect(frame, 10, 10);

        const QRect imageRect = rect().adjusted(8, 8, -8, -44);
        if (!m_preview.isNull()) {
            const QRect target = previewRect();
            QPainterPath clip;
            clip.addRoundedRect(QRectF(imageRect), 6, 6);
            painter.save();
            painter.setClipPath(clip);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            painter.drawPixmap(target, m_preview);
            painter.restore();
        }
        painter.setPen(palette().color(QPalette::Text));
        QFont titleFont = font();
        titleFont.setBold(isChecked());
        painter.setFont(titleFont);
        const QRect titleRect(14, height() - 38, qMax(0, width() - 56), 30);
        painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                         painter.fontMetrics().elidedText(text(), Qt::ElideRight, titleRect.width()));
    }

private:
    QPixmap m_preview;
    QToolButton *m_close = nullptr;
};

} // namespace

WorkspaceOverview::WorkspaceOverview(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("workspaceOverview"));
    setAccessibleName(tr("Workspace overview"));
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Window);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(16);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(tr("Workspace overview"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setBold(true);
    title->setFont(titleFont);
    m_count = new QLabel(this);
    auto *done = new QPushButton(tr("Done"), this);
    done->setObjectName(QStringLiteral("workspaceOverviewDone"));
    done->setAccessibleName(tr("Exit workspace overview"));
    connect(done, &QPushButton::clicked, this, &WorkspaceOverview::dismissRequested);
    header->addWidget(title);
    header->addWidget(m_count);
    header->addStretch();
    header->addWidget(done);
    layout->addLayout(header);

    auto *tools = new QHBoxLayout;
    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("workspaceOverviewSearch"));
    m_search->setPlaceholderText(tr("Search tabs"));
    m_search->setAccessibleName(tr("Search tabs"));
    m_search->setClearButtonEnabled(true);
    auto *add = new QPushButton(tr("New tab"), this);
    add->setObjectName(QStringLiteral("workspaceOverviewNewTab"));
    add->setAccessibleName(tr("New tab"));
    add->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    connect(add, &QPushButton::clicked, this, &WorkspaceOverview::addTabRequested);
    tools->addWidget(m_search, 1);
    tools->addWidget(add);
    layout->addLayout(tools);

    m_empty = new QLabel(tr("No matching tabs"), this);
    m_empty->setObjectName(QStringLiteral("workspaceOverviewEmpty"));
    m_empty->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_empty);
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->viewport()->installEventFilter(this);
    m_scrollArea->installEventFilter(this);
    m_gridHost = new QWidget(m_scrollArea);
    m_grid = new QGridLayout(m_gridHost);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setSpacing(16);
    m_grid->setAlignment(Qt::AlignTop);
    m_scrollArea->setWidget(m_gridHost);
    layout->addWidget(m_scrollArea, 1);
    connect(m_search, &QLineEdit::textChanged, this, &WorkspaceOverview::reflow);
    for (auto *widget : {static_cast<QWidget *>(m_search), static_cast<QWidget *>(add), static_cast<QWidget *>(done)})
        widget->installEventFilter(this);
    m_empty->hide();
}

void WorkspaceOverview::setEntries(const QList<Entry> &entries) {
    m_entries = entries;
    QSet<int> liveIds;
    for (const auto &entry : entries) {
        liveIds.insert(entry.id);
        auto *card = static_cast<OverviewCard *>(m_cards.value(entry.id));
        if (!card) {
            card = new OverviewCard(m_gridHost);
            m_cards.insert(entry.id, card);
            card->installEventFilter(this);
            card->closeButton()->installEventFilter(this);
            connect(card, &QAbstractButton::clicked, this, [this, id = entry.id]() { Q_EMIT tabActivated(id); });
            connect(card->closeButton(), &QToolButton::clicked, this,
                    [this, id = entry.id]() { Q_EMIT tabCloseRequested(id); });
        }
        card->setEntry(entry);
    }
    for (int id : m_cards.keys()) {
        if (!liveIds.contains(id)) {
            auto *card = m_cards.take(id);
            const bool hadFocus = card->hasFocus() || card->isAncestorOf(QApplication::focusWidget());
            m_grid->removeWidget(card);
            card->hide();
            card->setObjectName(QString());
            card->deleteLater();
            if (hadFocus)
                focusSearch();
        }
    }
    m_count->setText(tr("%n tab(s)", nullptr, entries.size()));
    reflow();
}

void WorkspaceOverview::focusSearch() {
    m_search->setFocus(Qt::OtherFocusReason);
}

void WorkspaceOverview::ensureTabVisible(int tabId) {
    if (auto *card = m_cards.value(tabId); card && !card->isHidden())
        m_scrollArea->ensureWidgetVisible(card);
}

QRect WorkspaceOverview::previewRect(int tabId) const {
    auto *card = static_cast<OverviewCard *>(m_cards.value(tabId));
    if (!card || card->isHidden())
        return {};
    const QRect rect(card->mapTo(this, card->previewRect().topLeft()), card->previewRect().size());
    const QRect viewport(m_scrollArea->viewport()->mapTo(this, QPoint()), m_scrollArea->viewport()->size());
    return viewport.contains(rect) ? rect : QRect();
}

QList<QAbstractButton *> WorkspaceOverview::matchingCards() const {
    QList<QAbstractButton *> cards;
    for (const auto &entry : m_entries) {
        if (entry.title.contains(m_search->text(), Qt::CaseInsensitive))
            cards.append(m_cards.value(entry.id));
    }
    return cards;
}

void WorkspaceOverview::reflow() {
    while (auto *item = m_grid->takeAt(0))
        delete item;
    const int previousColumns = m_columns;
    m_columns = qMax(1, (m_scrollArea->viewport()->width() + 16) / (260 + 16));
    for (int i = 0; i < qMax(previousColumns, m_columns); ++i)
        m_grid->setColumnStretch(i, i < m_columns ? 1 : 0);
    const auto matches = matchingCards();
    const QSet<QAbstractButton *> visible(matches.begin(), matches.end());
    for (auto *card : m_cards) {
        card->setVisible(visible.contains(card));
    }
    for (int i = 0; i < matches.size(); ++i)
        m_grid->addWidget(matches[i], i / m_columns, i % m_columns);
    m_empty->setVisible(matches.isEmpty());
}

void WorkspaceOverview::navigate(int key) {
    const auto cards = matchingCards();
    if (cards.isEmpty())
        return;
    int index = cards.indexOf(qobject_cast<QAbstractButton *>(QApplication::focusWidget()));
    if (index < 0) {
        index = 0;
        for (int i = 0; i < cards.size(); ++i) {
            if (cards[i]->isChecked()) {
                index = i;
                break;
            }
        }
    } else {
        const int delta = key == Qt::Key_Left    ? -1
                          : key == Qt::Key_Right ? 1
                          : key == Qt::Key_Up    ? -m_columns
                                                 : m_columns;
        index = qBound(0, index + delta, int(cards.size()) - 1);
    }
    cards[index]->setFocus(Qt::OtherFocusReason);
    m_scrollArea->ensureWidgetVisible(cards[index]);
}

bool WorkspaceOverview::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_scrollArea->viewport() && event->type() == QEvent::Resize) {
        reflow();
    } else if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            Q_EMIT dismissRequested();
            return true;
        }
        if (keyEvent->modifiers() == Qt::NoModifier) {
            const int key = keyEvent->key();
            const bool inSearch = watched == m_search;
            if (key == Qt::Key_Up || key == Qt::Key_Down
                || (!inSearch && (key == Qt::Key_Left || key == Qt::Key_Right))) {
                navigate(key);
                return true;
            }
            if (key == Qt::Key_Return || key == Qt::Key_Enter) {
                if (inSearch) {
                    const auto cards = matchingCards();
                    if (!cards.isEmpty())
                        cards.first()->click();
                } else if (auto *button = qobject_cast<QAbstractButton *>(watched)) {
                    button->click();
                }
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool WorkspaceOverview::focusNextPrevChild(bool next) {
    QWidget *start = QApplication::focusWidget();
    if (!start || !isAncestorOf(start))
        start = this;
    QWidget *candidate = start;
    do {
        candidate = next ? candidate->nextInFocusChain() : candidate->previousInFocusChain();
        if (isAncestorOf(candidate) && candidate->isVisibleTo(this) && candidate->isEnabled()
            && (candidate->focusPolicy() & Qt::TabFocus)) {
            candidate->setFocus(next ? Qt::TabFocusReason : Qt::BacktabFocusReason);
            return true;
        }
    } while (candidate != start);
    return true;
}

void WorkspaceOverview::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    reflow();
}
