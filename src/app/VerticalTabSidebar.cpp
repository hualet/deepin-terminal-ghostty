#include "VerticalTabSidebar.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QToolButton *createButton(const QString &objectName, const QString &text, QWidget *parent) {
    auto *button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setText(text);
    button->setAutoRaise(true);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
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
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->setSpacing(4);
    m_layout->addStretch(1);

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);
}

void VerticalTabSidebar::setItems(const QList<TabItem> &items) {
    m_items = items;
    rebuild();
}

QList<VerticalTabSidebar::TabItem> VerticalTabSidebar::items() const {
    return m_items;
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
        auto *section = new QWidget(this);
        section->setObjectName(QStringLiteral("verticalTabSection"));
        section->setProperty("tabId", tab.id);
        section->setProperty("expanded", tab.expanded);

        auto *sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
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
        expandButton->setProperty("tabId", tab.id);
        expandButton->setProperty("expanded", tab.expanded);
        connect(expandButton, &QToolButton::clicked, this, [this, tab]() { emit tabExpansionToggled(tab.id); });

        auto *tabButton = createButton(QStringLiteral("verticalTabButton"), tab.title, header);
        tabButton->setCheckable(true);
        tabButton->setChecked(tab.isCurrent);
        tabButton->setProperty("tabId", tab.id);
        tabButton->setProperty("active", tab.isCurrent);
        connect(tabButton, &QToolButton::clicked, this, [this, tab]() { emit tabActivated(tab.id); });

        headerLayout->addWidget(expandButton, 0, Qt::AlignTop);
        headerLayout->addWidget(tabButton, 1);

        sectionLayout->addWidget(header);

        if (tab.expanded) {
            auto *paneList = new QWidget(section);
            paneList->setObjectName(QStringLiteral("verticalPaneList"));
            paneList->setProperty("tabId", tab.id);

            auto *paneLayout = new QVBoxLayout(paneList);
            paneLayout->setContentsMargins(20, 0, 0, 0);
            paneLayout->setSpacing(2);

            for (const auto &pane : tab.panes) {
                const QString paneTitle = pane.title.isEmpty() ? tr("Terminal") : pane.title;
                auto *paneButton = createButton(QStringLiteral("verticalPaneButton"), paneTitle, paneList);
                paneButton->setCheckable(true);
                paneButton->setChecked(pane.isActive);
                paneButton->setProperty("tabId", tab.id);
                paneButton->setProperty("paneId", pane.id);
                paneButton->setProperty("active", pane.isActive);
                connect(paneButton, &QToolButton::clicked, this,
                        [this, tabId = tab.id, paneId = pane.id]() { emit paneActivated(tabId, paneId); });
                paneLayout->addWidget(paneButton);
            }

            sectionLayout->addWidget(paneList);
        }

        m_layout->addWidget(section);
    }

    m_layout->addStretch(1);
}
