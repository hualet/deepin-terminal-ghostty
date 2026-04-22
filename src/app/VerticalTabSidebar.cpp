#include "VerticalTabSidebar.h"

#include <QAbstractButton>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
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
    m_layout->setContentsMargins(6, 8, 6, 8);
    m_layout->setSpacing(6);
    m_layout->addStretch(1);

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);

    setStyleSheet(QStringLiteral(R"(
        #verticalTabSidebarScrollArea {
            border: none;
            background: transparent;
        }
        #verticalTabSection {
            border: 1px solid transparent;
            border-radius: 6px;
            background-color: transparent;
        }
        #verticalTabSection:hover {
            background-color: palette(alternate-base);
        }
        #verticalTabSection[isCurrent="true"] {
            background-color: palette(highlight);
        }
        #verticalTabSection[isCurrent="true"] #verticalTabButton,
        #verticalTabSection[isCurrent="true"] #verticalPaneButton,
        #verticalTabSection[isCurrent="true"] #verticalTabExpandButton {
            color: palette(highlighted-text);
        }
        #verticalTabExpandButton {
            border: none;
            background: transparent;
            padding: 0px;
            margin: 0px;
        }
        #verticalTabExpandButton:hover {
            background-color: rgba(0, 0, 0, 0.1);
            border-radius: 3px;
        }
        #verticalTabButton {
            border: none;
            background: transparent;
            padding: 2px 0px;
            margin: 0px;
            color: palette(text);
            text-align: left;
        }
        #verticalTabButton:checked {
            font-weight: bold;
        }
        #verticalPaneList {
            background: transparent;
        }
        #verticalPaneButton {
            border: none;
            background: transparent;
            padding: 2px 0px;
            padding-left: 20px;
            margin: 0px;
            color: palette(text);
            text-align: left;
        }
        #verticalPaneButton:hover {
            background-color: rgba(0, 0, 0, 0.05);
            border-radius: 4px;
        }
        #verticalPaneButton:checked {
            font-weight: bold;
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
        int pad = 8;
        if (btn->objectName() == QStringLiteral("verticalPaneButton"))
            pad = 24;
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

    for (const auto &tab : m_items) {
        auto *section = new QFrame(this);
        section->setObjectName(QStringLiteral("verticalTabSection"));
        section->setProperty("tabId", tab.id);
        section->setProperty("isCurrent", tab.isCurrent);

        auto *sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(8, 6, 8, 6);
        sectionLayout->setSpacing(2);

        auto *header = new QWidget(section);
        header->setObjectName(QStringLiteral("verticalTabHeader"));
        header->setProperty("tabId", tab.id);

        auto *headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(4);

        auto *expandButton = new QToolButton(header);
        expandButton->setObjectName(QStringLiteral("verticalTabExpandButton"));
        expandButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
        expandButton->setArrowType(tab.expanded ? Qt::DownArrow : Qt::RightArrow);
        expandButton->setAutoRaise(true);
        expandButton->setFixedSize(16, 16);
        expandButton->setProperty("tabId", tab.id);
        expandButton->setProperty("expanded", tab.expanded);
        connect(expandButton, &QToolButton::clicked, this, [this, tab]() { emit tabExpansionToggled(tab.id); });

        auto *tabButton = createButton(QStringLiteral("verticalTabButton"), tab.title, header);
        tabButton->setCheckable(true);
        tabButton->setChecked(tab.isCurrent);
        tabButton->setProperty("tabId", tab.id);
        tabButton->setProperty("active", tab.isCurrent);
        connect(tabButton, &QPushButton::clicked, this, [this, tab]() { emit tabActivated(tab.id); });

        headerLayout->addWidget(expandButton, 0, Qt::AlignVCenter);
        headerLayout->addWidget(tabButton, 1);

        sectionLayout->addWidget(header);

        if (tab.expanded) {
            auto *paneList = new QWidget(section);
            paneList->setObjectName(QStringLiteral("verticalPaneList"));
            paneList->setProperty("tabId", tab.id);

            auto *paneLayout = new QVBoxLayout(paneList);
            paneLayout->setContentsMargins(0, 2, 0, 0);
            paneLayout->setSpacing(0);

            for (const auto &pane : tab.panes) {
                const QString paneTitle = pane.title.isEmpty() ? tr("Terminal") : pane.title;
                auto *paneButton = createButton(QStringLiteral("verticalPaneButton"), paneTitle, paneList);
                paneButton->setCheckable(true);
                paneButton->setChecked(pane.isActive);
                paneButton->setProperty("tabId", tab.id);
                paneButton->setProperty("paneId", pane.id);
                paneButton->setProperty("active", pane.isActive);
                connect(paneButton, &QPushButton::clicked, this,
                        [this, tabId = tab.id, paneId = pane.id]() { emit paneActivated(tabId, paneId); });
                paneLayout->addWidget(paneButton);
            }

            sectionLayout->addWidget(paneList);
        }

        m_layout->addWidget(section);
    }

    m_layout->addStretch(1);

    QTimer::singleShot(0, this, &VerticalTabSidebar::updateButtonElisions);
}
