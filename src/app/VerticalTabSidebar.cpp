#include "VerticalTabSidebar.h"

#include <DGuiApplicationHelper>
#include <QAbstractButton>
#include <QApplication>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

DGUI_USE_NAMESPACE

namespace {

constexpr int kProcessBadgeSize = 24;
constexpr int kProcessIconSize = 16;

QPushButton *createButton(const QString &objectName, const QString &text, QWidget *parent) {
    auto *button = new QPushButton(parent);
    button->setObjectName(objectName);
    button->setText(text);
    button->setFlat(true);
    button->setMinimumWidth(0);
    button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    button->setProperty("_fullText", text);
    return button;
}

void allowHorizontalShrink(QWidget *widget) {
    if (!widget)
        return;

    widget->setMinimumWidth(0);
    QSizePolicy policy = widget->sizePolicy();
    policy.setHorizontalPolicy(QSizePolicy::Ignored);
    widget->setSizePolicy(policy);
}

QLabel *createProcessBadge(const QString &objectName, QWidget *parent, const QString &iconName,
                           const QString &toneProperty) {
    auto *badge = new QLabel(parent);
    badge->setObjectName(objectName);
    badge->setProperty("tone", toneProperty);
    badge->setFixedSize(kProcessBadgeSize, kProcessBadgeSize);
    badge->setAlignment(Qt::AlignCenter);

    const QString resourcePath =
        QStringLiteral(":/icons/process/%1.svg").arg(iconName.isEmpty() ? QStringLiteral("terminal") : iconName);
    QIcon icon(resourcePath);
    if (icon.isNull())
        icon = QIcon(QStringLiteral(":/icons/process/terminal.svg"));
    badge->setPixmap(icon.pixmap(kProcessIconSize, kProcessIconSize));
    return badge;
}

} // namespace

VerticalTabSidebar::VerticalTabSidebar(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("verticalTabSidebar"));

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setObjectName(QStringLiteral("verticalTabSidebarScrollArea"));
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget(scrollArea);
    content->setObjectName(QStringLiteral("verticalTabSidebarContent"));
    allowHorizontalShrink(content);
    m_layout = new QVBoxLayout(content);
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->setSpacing(8);
    m_layout->addStretch(1);

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);

    applyStylesheet();
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::paletteTypeChanged, this,
            &VerticalTabSidebar::applyStylesheet);
}

static QColor blend(const QColor &a, const QColor &b, qreal ratio = 0.5) {
    return QColor(qMin(255, int(a.red() * (1 - ratio) + b.red() * ratio)),
                  qMin(255, int(a.green() * (1 - ratio) + b.green() * ratio)),
                  qMin(255, int(a.blue() * (1 - ratio) + b.blue() * ratio)));
}

void VerticalTabSidebar::applyStylesheet() {
    const auto *helper = DGuiApplicationHelper::instance();
    const QPalette pal = helper->applicationPalette();
    const bool isDark = helper->themeType() == DGuiApplicationHelper::DarkType;

    const auto c = [](int r, int g, int b, int a) {
        return QStringLiteral("rgba(%1,%2,%3,%4)").arg(r).arg(g).arg(b).arg(a / 255.0, 0, 'f', 3);
    };
    QColor sidebarBg = pal.color(QPalette::Window);
    QColor sectionBg = pal.color(QPalette::Base);
    QColor text = pal.color(QPalette::WindowText);
    QColor highlight = pal.color(QPalette::Highlight);
    QColor sectionHover = isDark ? sectionBg.lighter(110) : sectionBg.darker(105);
    QColor sectionCurrent = blend(sectionBg, highlight, isDark ? 0.18 : 0.10);

    int ta = text.alpha();

    setStyleSheet(
        QStringLiteral(R"(
        #verticalTabSidebar {
            background: %1;
        }
        #verticalTabSidebarScrollArea {
            border: none;
            background: transparent;
        }
        #verticalTabSidebarContent {
            background: transparent;
        }
        #verticalTabSection {
            border: 1px solid %2;
            border-radius: 12px;
            background-color: %3;
        }
        #verticalTabSection:hover {
            background-color: %4;
        }
        #verticalTabSection[isCurrent="true"] {
            border: 1px solid %5;
            background-color: %6;
        }
        #verticalTabHeader {
            background: transparent;
        }
        #verticalTabSection[isCurrent="true"] #verticalTabButton {
            color: %7;
        }
        #verticalTabExpandButton {
            border: none;
            background: transparent;
            padding: 0px 1px;
            margin: 0px;
            color: %8;
        }
        #verticalTabExpandButton:hover {
            background-color: %9;
            border-radius: 6px;
        }
        #verticalTabButton {
            border: none;
            background: transparent;
            padding: 6px 0px;
            margin: 0px;
            color: %10;
            text-align: left;
            font-size: 15px;
        }
        #verticalTabButton:checked {
            font-weight: 600;
        }
        #verticalTabBadge,
        #verticalPaneBadge {
            border-radius: 12px;
            background-color: %11;
            color: %12;
        }
        #verticalTabSection[isCurrent="true"] #verticalTabBadge {
            background-color: %13;
        }
        #verticalPaneList {
            background: transparent;
        }
        #verticalPaneGuide {
            min-width: 1px;
            max-width: 1px;
            background-color: %14;
            margin-top: 2px;
            margin-bottom: 4px;
        }
        #verticalPaneButton {
            border: none;
            background: transparent;
            padding: 5px 0px;
            margin: 0px;
            color: %15;
            text-align: left;
            font-size: 14px;
        }
        #verticalPaneButton[active="true"] {
            color: %16;
        }
        #verticalPaneButton:checked {
            font-weight: 500;
        }
    )")
            .arg(c(sidebarBg.red(), sidebarBg.green(), sidebarBg.blue(), qRound(sidebarBg.alpha() * 0.72)),
                 c(text.red(), text.green(), text.blue(), qRound(ta * (isDark ? 0.05 : 0.08))),
                 c(sectionBg.red(), sectionBg.green(), sectionBg.blue(), qRound(sectionBg.alpha() * 0.96)),
                 c(sectionHover.red(), sectionHover.green(), sectionHover.blue(), qRound(sectionHover.alpha() * 0.98)),
                 c(highlight.red(), highlight.green(), highlight.blue(), qRound(highlight.alpha() * 0.45)),
                 c(sectionCurrent.red(), sectionCurrent.green(), sectionCurrent.blue(),
                   qRound(sectionCurrent.alpha() * 0.98)),
                 c(text.red(), text.green(), text.blue(), ta),
                 c(text.red(), text.green(), text.blue(), qRound(ta * 0.7)),
                 c(text.red(), text.green(), text.blue(), qRound(ta * 0.08)),
                 c(text.red(), text.green(), text.blue(), ta),
                 c(text.red(), text.green(), text.blue(), qRound(ta * 0.08)),
                 c(text.red(), text.green(), text.blue(), ta),
                 c(highlight.red(), highlight.green(), highlight.blue(), qRound(highlight.alpha() * 0.18)),
                 c(text.red(), text.green(), text.blue(), qRound(ta * (isDark ? 0.12 : 0.18))),
                 c(text.red(), text.green(), text.blue(), qRound(ta * 0.75)),
                 c(text.red(), text.green(), text.blue(), qRound(ta * 0.92))));
}

void VerticalTabSidebar::setItems(const QList<TabItem> &items) {
    m_items = items;
    rebuild();
}

QList<VerticalTabSidebar::TabItem> VerticalTabSidebar::items() const {
    return m_items;
}

void VerticalTabSidebar::updateButtonElisions() {
    for (auto *btn : findChildren<QAbstractButton *>()) {
        const QString fullText = btn->property("_fullText").toString();
        if (fullText.isEmpty())
            continue;
        QFontMetrics fm(btn->font());
        int pad = 2;
        const int available = qMax(btn->width() - pad, 20);
        btn->setText(fm.elidedText(fullText, Qt::ElideRight, available));
    }
}

void VerticalTabSidebar::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    QTimer::singleShot(0, this, &VerticalTabSidebar::updateButtonElisions);
}

void VerticalTabSidebar::rebuild() {
    if (!m_layout)
        return;

    while (m_layout->count() > 0) {
        QLayoutItem *item = m_layout->takeAt(0);
        if (auto *widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    for (const auto &tab : m_items) {
        auto *section = new QFrame(this);
        section->setObjectName(QStringLiteral("verticalTabSection"));
        section->setProperty("tabId", tab.id);
        section->setProperty("isCurrent", tab.isCurrent);
        allowHorizontalShrink(section);

        auto *sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(6, 6, 6, 6);
        sectionLayout->setSpacing(4);

        auto *header = new QWidget(section);
        header->setObjectName(QStringLiteral("verticalTabHeader"));
        header->setProperty("tabId", tab.id);
        allowHorizontalShrink(header);

        auto *headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(8, 6, 8, 6);
        headerLayout->setSpacing(6);

        const int paneCount = tab.panes.size();
        const bool isMultiPane = paneCount > 1;

        auto *expandButton = new QToolButton(header);
        expandButton->setObjectName(QStringLiteral("verticalTabExpandButton"));
        expandButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
        expandButton->setArrowType(tab.expanded ? Qt::DownArrow : Qt::RightArrow);
        expandButton->setAutoRaise(true);
        expandButton->setFixedSize(16, 16);
        expandButton->setProperty("tabId", tab.id);
        expandButton->setProperty("expanded", tab.expanded);
        expandButton->setVisible(isMultiPane);
        connect(expandButton, &QToolButton::clicked, this, [this, tab]() { emit tabExpansionToggled(tab.id); });

        auto *tabBadge =
            createProcessBadge(QStringLiteral("verticalTabBadge"), header,
                               isMultiPane ? QString() : (paneCount == 1 ? tab.panes.first().iconName : QString()),
                               tab.isCurrent ? QStringLiteral("bright") : QStringLiteral("muted"));
        tabBadge->setVisible(!isMultiPane);

        auto *tabButton = createButton(QStringLiteral("verticalTabButton"), tab.title, header);
        tabButton->setCheckable(true);
        tabButton->setChecked(tab.isCurrent);
        tabButton->setProperty("tabId", tab.id);
        tabButton->setProperty("active", tab.isCurrent);
        connect(tabButton, &QPushButton::clicked, this, [this, tab]() { emit tabActivated(tab.id); });

        headerLayout->addWidget(expandButton, 0, Qt::AlignVCenter);
        headerLayout->addWidget(tabBadge, 0, Qt::AlignVCenter);
        headerLayout->addWidget(tabButton, 1);

        sectionLayout->addWidget(header);

        if (tab.expanded && isMultiPane) {
            auto *paneList = new QWidget(section);
            paneList->setObjectName(QStringLiteral("verticalPaneList"));
            paneList->setProperty("tabId", tab.id);
            allowHorizontalShrink(paneList);

            auto *paneListLayout = new QHBoxLayout(paneList);
            paneListLayout->setContentsMargins(10, 0, 4, 0);
            paneListLayout->setSpacing(8);

            auto *paneGuide = new QFrame(paneList);
            paneGuide->setObjectName(QStringLiteral("verticalPaneGuide"));
            paneGuide->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
            paneListLayout->addWidget(paneGuide);

            auto *paneColumn = new QWidget(paneList);
            allowHorizontalShrink(paneColumn);
            auto *paneLayout = new QVBoxLayout(paneColumn);
            paneLayout->setContentsMargins(0, 0, 0, 0);
            paneLayout->setSpacing(2);

            for (int paneIndex = 0; paneIndex < tab.panes.size(); ++paneIndex) {
                const auto &pane = tab.panes.at(paneIndex);
                const QString paneTitle = pane.title.isEmpty() ? tr("Terminal") : pane.title;
                auto *paneRow = new QWidget(paneColumn);
                allowHorizontalShrink(paneRow);
                auto *paneRowLayout = new QHBoxLayout(paneRow);
                paneRowLayout->setContentsMargins(8, 2, 4, 2);
                paneRowLayout->setSpacing(6);

                auto *paneBadge =
                    createProcessBadge(QStringLiteral("verticalPaneBadge"), paneRow, pane.iconName,
                                       pane.isActive ? QStringLiteral("bright") : QStringLiteral("muted"));
                auto *paneButton = createButton(QStringLiteral("verticalPaneButton"), paneTitle, paneRow);
                paneButton->setCheckable(true);
                paneButton->setChecked(pane.isActive);
                paneButton->setProperty("tabId", tab.id);
                paneButton->setProperty("paneId", pane.id);
                paneButton->setProperty("active", pane.isActive);
                connect(paneButton, &QPushButton::clicked, this,
                        [this, tabId = tab.id, paneId = pane.id]() { emit paneActivated(tabId, paneId); });

                paneRowLayout->addWidget(paneBadge, 0, Qt::AlignVCenter);
                paneRowLayout->addWidget(paneButton, 1);
                paneLayout->addWidget(paneRow);
            }

            paneListLayout->addWidget(paneColumn, 1);
            sectionLayout->addWidget(paneList);
        }

        m_layout->addWidget(section);
    }

    m_layout->addStretch(1);

    QTimer::singleShot(0, this, &VerticalTabSidebar::updateButtonElisions);
}
