#include "VerticalTabSidebar.h"

#include <QAbstractButton>
#include <QApplication>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QPushButton *createButton(const QString &objectName, const QString &text, QWidget *parent) {
    auto *button = new QPushButton(parent);
    button->setObjectName(objectName);
    button->setText(text);
    button->setFlat(true);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    button->setProperty("_fullText", text);
    return button;
}

QLabel *createBadge(const QString &objectName, QWidget *parent, QStyle::StandardPixmap iconType,
                    const QString &toneProperty) {
    auto *badge = new QLabel(parent);
    badge->setObjectName(objectName);
    badge->setProperty("tone", toneProperty);
    badge->setFixedSize(24, 24);
    badge->setAlignment(Qt::AlignCenter);
    badge->setPixmap(qApp->style()->standardIcon(iconType).pixmap(14, 14));
    return badge;
}

QStyle::StandardPixmap tabBadgeIcon(int index) {
    static const QStyle::StandardPixmap icons[] = {
        QStyle::SP_ComputerIcon,
        QStyle::SP_DriveHDIcon,
        QStyle::SP_FileDialogContentsView,
        QStyle::SP_DirHomeIcon,
    };

    return icons[index % (sizeof(icons) / sizeof(icons[0]))];
}

QStyle::StandardPixmap paneBadgeIcon(int index) {
    static const QStyle::StandardPixmap icons[] = {
        QStyle::SP_FileIcon,
        QStyle::SP_DialogResetButton,
        QStyle::SP_CommandLink,
        QStyle::SP_BrowserReload,
    };

    return icons[index % (sizeof(icons) / sizeof(icons[0]))];
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

    auto *content = new QWidget(scrollArea);
    content->setObjectName(QStringLiteral("verticalTabSidebarContent"));
    m_layout = new QVBoxLayout(content);
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->setSpacing(8);
    m_layout->addStretch(1);

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);

    setStyleSheet(QStringLiteral(R"(
        #verticalTabSidebar {
            background: rgba(12, 15, 22, 0.72);
        }
        #verticalTabSidebarScrollArea {
            border: none;
            background: transparent;
        }
        #verticalTabSidebarContent {
            background: transparent;
        }
        #verticalTabSection {
            border: 1px solid rgba(255, 255, 255, 0.05);
            border-radius: 12px;
            background-color: rgba(29, 32, 40, 0.96);
        }
        #verticalTabSection:hover {
            background-color: rgba(37, 41, 51, 0.98);
        }
        #verticalTabSection[isCurrent="true"] {
            border: 1px solid rgba(83, 143, 255, 0.45);
            background-color: rgba(30, 45, 74, 0.98);
        }
        #verticalTabHeader {
            background: transparent;
        }
        #verticalTabSection[isCurrent="true"] #verticalTabButton {
            color: rgb(242, 247, 255);
        }
        #verticalTabExpandButton {
            border: none;
            background: transparent;
            padding: 0px 1px;
            margin: 0px;
            color: rgba(210, 218, 232, 0.7);
        }
        #verticalTabExpandButton:hover {
            background-color: rgba(255, 255, 255, 0.08);
            border-radius: 6px;
        }
        #verticalTabButton {
            border: none;
            background: transparent;
            padding: 6px 0px;
            margin: 0px;
            color: rgb(220, 225, 236);
            text-align: left;
            font-size: 15px;
        }
        #verticalTabButton:checked {
            font-weight: 600;
        }
        #verticalTabBadge,
        #verticalPaneBadge {
            border-radius: 12px;
            background-color: rgba(255, 255, 255, 0.08);
            color: rgb(220, 225, 236);
        }
        #verticalTabSection[isCurrent="true"] #verticalTabBadge {
            background-color: rgba(83, 143, 255, 0.18);
        }
        #verticalPaneList {
            background: transparent;
        }
        #verticalPaneGuide {
            min-width: 1px;
            max-width: 1px;
            background-color: rgba(255, 255, 255, 0.12);
            margin-top: 2px;
            margin-bottom: 4px;
        }
        #verticalPaneButton {
            border: none;
            background: transparent;
            padding: 5px 0px;
            margin: 0px;
            color: rgb(183, 191, 204);
            text-align: left;
            font-size: 14px;
        }
        #verticalPaneButton[active="true"] {
            color: rgb(229, 235, 245);
        }
        #verticalPaneButton:checked {
            font-weight: 500;
        }
    )"));
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
    updateButtonElisions();
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

    for (int tabIndex = 0; tabIndex < m_items.size(); ++tabIndex) {
        const auto &tab = m_items.at(tabIndex);
        auto *section = new QFrame(this);
        section->setObjectName(QStringLiteral("verticalTabSection"));
        section->setProperty("tabId", tab.id);
        section->setProperty("isCurrent", tab.isCurrent);

        auto *sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(6, 6, 6, 6);
        sectionLayout->setSpacing(4);

        auto *header = new QWidget(section);
        header->setObjectName(QStringLiteral("verticalTabHeader"));
        header->setProperty("tabId", tab.id);

        auto *headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(8, 6, 8, 6);
        headerLayout->setSpacing(6);

        auto *expandButton = new QToolButton(header);
        expandButton->setObjectName(QStringLiteral("verticalTabExpandButton"));
        expandButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
        expandButton->setArrowType(tab.expanded ? Qt::DownArrow : Qt::RightArrow);
        expandButton->setAutoRaise(true);
        expandButton->setFixedSize(16, 16);
        expandButton->setProperty("tabId", tab.id);
        expandButton->setProperty("expanded", tab.expanded);
        connect(expandButton, &QToolButton::clicked, this, [this, tab]() { emit tabExpansionToggled(tab.id); });

        auto *tabBadge = createBadge(QStringLiteral("verticalTabBadge"), header, tabBadgeIcon(tabIndex),
                                     tab.isCurrent ? QStringLiteral("bright") : QStringLiteral("muted"));
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

        if (tab.expanded) {
            auto *paneList = new QWidget(section);
            paneList->setObjectName(QStringLiteral("verticalPaneList"));
            paneList->setProperty("tabId", tab.id);

            auto *paneListLayout = new QHBoxLayout(paneList);
            paneListLayout->setContentsMargins(10, 0, 4, 0);
            paneListLayout->setSpacing(8);

            auto *paneGuide = new QFrame(paneList);
            paneGuide->setObjectName(QStringLiteral("verticalPaneGuide"));
            paneGuide->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
            paneListLayout->addWidget(paneGuide);

            auto *paneColumn = new QWidget(paneList);
            auto *paneLayout = new QVBoxLayout(paneColumn);
            paneLayout->setContentsMargins(0, 0, 0, 0);
            paneLayout->setSpacing(2);

            for (int paneIndex = 0; paneIndex < tab.panes.size(); ++paneIndex) {
                const auto &pane = tab.panes.at(paneIndex);
                const QString paneTitle = pane.title.isEmpty() ? tr("Terminal") : pane.title;
                auto *paneRow = new QWidget(paneColumn);
                auto *paneRowLayout = new QHBoxLayout(paneRow);
                paneRowLayout->setContentsMargins(8, 2, 4, 2);
                paneRowLayout->setSpacing(6);

                auto *paneBadge = createBadge(QStringLiteral("verticalPaneBadge"), paneRow, paneBadgeIcon(paneIndex),
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
