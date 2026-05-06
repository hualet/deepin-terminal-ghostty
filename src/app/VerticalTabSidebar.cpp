#include "VerticalTabSidebar.h"

#include <DGuiApplicationHelper>
#include <QAbstractButton>
#include <QApplication>
#include <QBitmap>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSet>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

DGUI_USE_NAMESPACE

namespace {

constexpr int kProcessBadgeSize = 24;
constexpr int kProcessIconSize = 16;
constexpr int kStatusDotSize = 8;

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

QString resolveBadgeName(const QString &iconName) {
    if (iconName == QStringLiteral("github-copilot"))
        return QStringLiteral("copilot");
    return iconName;
}

QString resolveBadgeResourcePath(const QString &rawName) {
    static const QSet<QString> kWebpIcons = {
        QStringLiteral("claude"),   QStringLiteral("gemini"), QStringLiteral("codex"),   QStringLiteral("qwen"),
        QStringLiteral("opencode"), QStringLiteral("goose"),  QStringLiteral("copilot"), QStringLiteral("kimi"),
    };
    const QString name = rawName.isEmpty() ? QStringLiteral("terminal") : resolveBadgeName(rawName);
    const QString ext = kWebpIcons.contains(name) ? QStringLiteral("webp") : QStringLiteral("svg");
    return QStringLiteral(":/badges/process/%1.%2").arg(name, ext);
}

QPixmap applyCircleMask(const QPixmap &src, int size) {
    if (src.isNull())
        return src;

    const qreal dpr = src.devicePixelRatio();
    QSize sz(size, size);
    QPixmap dst(sz * dpr);
    dst.setDevicePixelRatio(dpr);
    dst.fill(Qt::transparent);

    QPainter p(&dst);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath clip;
    clip.addEllipse(0, 0, size, size);
    p.setClipPath(clip);
    p.drawPixmap(0, 0, size, size, src);
    p.end();

    return dst;
}

QLabel *createProcessBadge(const QString &objectName, QWidget *parent, const QString &iconName,
                           const QString &toneProperty) {
    auto *badge = new QLabel(parent);
    badge->setObjectName(objectName);
    const QString name = iconName.isEmpty() ? QStringLiteral("terminal") : resolveBadgeName(iconName);
    badge->setAccessibleName(QObject::tr("Process badge: %1").arg(name));
    badge->setAccessibleDescription(QObject::tr("Icon showing the process associated with this terminal."));
    badge->setProperty("tone", toneProperty);
    badge->setFixedSize(kProcessBadgeSize, kProcessBadgeSize);
    badge->setAlignment(Qt::AlignCenter);

    const QString resourcePath = resolveBadgeResourcePath(iconName);
    QIcon icon(resourcePath);
    if (icon.isNull())
        icon = QIcon(QStringLiteral(":/badges/process/terminal.svg"));
    badge->setPixmap(applyCircleMask(icon.pixmap(kProcessIconSize, kProcessIconSize), kProcessIconSize));
    return badge;
}

QLabel *createCommandStatusDot(QWidget *parent, TerminalWidget::CommandState state, bool isActive) {
    auto *dot = new QLabel(parent);
    dot->setObjectName(QStringLiteral("commandStatusDot"));
    dot->setAccessibleName(QObject::tr("Command status"));
    dot->setAccessibleDescription(QObject::tr("Shows whether the last command succeeded or failed."));
    dot->setFixedSize(kStatusDotSize, kStatusDotSize);

    if (isActive || state == TerminalWidget::CommandState::Idle || state == TerminalWidget::CommandState::Running) {
        dot->setVisible(false);
        return dot;
    }

    const auto *helper = DGuiApplicationHelper::instance();
    const bool isDark = helper->themeType() == DGuiApplicationHelper::DarkType;

    QColor color;
    switch (state) {
        case TerminalWidget::CommandState::Succeeded:
            color = isDark ? QColor(46, 213, 115) : QColor(34, 170, 91);
            break;
        case TerminalWidget::CommandState::Failed:
            color = isDark ? QColor(255, 71, 87) : QColor(210, 55, 70);
            break;
        default:
            dot->setVisible(false);
            return dot;
    }

    const qreal dpr = dot->devicePixelRatioF();
    QPixmap pixmap(static_cast<int>(kStatusDotSize * dpr), static_cast<int>(kStatusDotSize * dpr));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(0, 0, kStatusDotSize, kStatusDotSize);
    painter.end();

    dot->setPixmap(pixmap);
    return dot;
}

} // namespace

VerticalTabSidebar::VerticalTabSidebar(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("verticalTabSidebar"));
    setAccessibleName(tr("Vertical terminal tabs"));
    setAccessibleDescription(tr("Navigate terminal tabs and panes in the vertical sidebar."));

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
    m_layout->setContentsMargins(6, 6, 6, 6);
    m_layout->setSpacing(4);
    m_layout->addStretch(1);

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);

    auto *addTabButton = new QPushButton(this);
    addTabButton->setObjectName(QStringLiteral("verticalAddTabButton"));
    addTabButton->setAccessibleName(tr("New tab"));
    addTabButton->setAccessibleDescription(tr("Create a new terminal tab."));
    addTabButton->setText(QStringLiteral("+"));
    addTabButton->setFlat(true);
    addTabButton->setFixedHeight(32);
    connect(addTabButton, &QPushButton::clicked, this, &VerticalTabSidebar::addTabRequested);
    outerLayout->addWidget(addTabButton);

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
    QColor sectionBg = pal.color(QPalette::Button);
    QColor text = pal.color(QPalette::WindowText);
    QColor highlight = pal.color(QPalette::Highlight);
    QColor highlightedText = pal.color(QPalette::HighlightedText);
    QColor sectionHover = isDark ? sectionBg.lighter(110) : sectionBg.darker(110);
    QColor sectionBorder = isDark ? QColor(text.red(), text.green(), text.blue(), qRound(text.alpha() * 0.10))
                                  : QColor(text.red(), text.green(), text.blue(), qRound(text.alpha() * 0.05));

    int ta = text.alpha();

    setStyleSheet(
        QStringLiteral(R"(
        #verticalTabSidebar {
            background: transparent;
        }
        #verticalTabSidebarScrollArea {
            border: none;
            background: transparent;
        }
        #verticalTabSidebarContent {
            background: transparent;
        }
        #verticalTabSection {
            border: 1px solid %1;
            border-radius: 8px;
            background-color: %2;
        }
        #verticalTabSection:hover {
            background-color: %3;
        }
        #verticalTabSection[isCurrent="true"] {
            border: 1px solid %4;
            background-color: %5;
        }
        #verticalTabHeader {
            background: transparent;
        }
        #verticalTabSection[isCurrent="true"] #verticalTabButton {
            color: %6;
        }
        #verticalTabExpandButton {
            border: none;
            background: transparent;
            padding: 0px 1px;
            margin: 0px;
            color: %7;
        }
        #verticalTabExpandButton:hover {
            background-color: %8;
            border-radius: 6px;
        }
        #verticalTabButton {
            border: none;
            background: transparent;
            padding: 6px 0px;
            margin: 0px;
            color: %9;
            text-align: left;
            font-size: 15px;
        }
        #verticalTabButton:checked {
            font-weight: 600;
        }
        #verticalTabBadge,
        #verticalPaneBadge {
            border-radius: 12px;
            background-color: %10;
            color: %11;
        }
        #verticalTabSection[isCurrent="true"] #verticalTabBadge {
            background-color: %12;
        }
        #verticalPaneList {
            background: transparent;
        }
        #verticalPaneGuide {
            min-width: 1px;
            max-width: 1px;
            background-color: %13;
            margin-top: 2px;
            margin-bottom: 4px;
        }
        #verticalPaneButton {
            border: none;
            background: transparent;
            padding: 5px 0px;
            margin: 0px;
            color: %14;
            text-align: left;
            font-size: 14px;
        }
        #verticalPaneButton[active="true"] {
            color: %15;
        }
        #verticalPaneButton:checked {
            font-weight: 500;
        }
        #verticalAddTabButton {
            border: none;
            background: transparent;
            color: %14;
            font-size: 18px;
            text-align: center;
        }
        #verticalAddTabButton:hover {
            background-color: %8;
            border-radius: 6px;
        }
    )")
            .arg(c(sectionBorder.red(), sectionBorder.green(), sectionBorder.blue(), sectionBorder.alpha()),
                 c(sectionBg.red(), sectionBg.green(), sectionBg.blue(), sectionBg.alpha()),
                 c(sectionHover.red(), sectionHover.green(), sectionHover.blue(), sectionHover.alpha()),
                 c(sectionBorder.red(), sectionBorder.green(), sectionBorder.blue(), sectionBorder.alpha()),
                 c(highlight.red(), highlight.green(), highlight.blue(), highlight.alpha()),
                 c(highlightedText.red(), highlightedText.green(), highlightedText.blue(), highlightedText.alpha()),
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
        section->setAccessibleName(tr("Terminal tab section: %1").arg(tab.title));
        section->setAccessibleDescription(tr("A terminal tab and its panes."));
        section->setProperty("tabId", tab.id);
        section->setProperty("isCurrent", tab.isCurrent);
        allowHorizontalShrink(section);

        auto *sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(4, 2, 4, 2);
        sectionLayout->setSpacing(2);

        auto *header = new QWidget(section);
        header->setObjectName(QStringLiteral("verticalTabHeader"));
        header->setAccessibleName(tr("Terminal tab header: %1").arg(tab.title));
        header->setProperty("tabId", tab.id);
        allowHorizontalShrink(header);

        auto *headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(6, 4, 6, 4);
        headerLayout->setSpacing(6);

        const int paneCount = tab.panes.size();
        const bool isMultiPane = paneCount > 1;

        auto *expandButton = new QToolButton(header);
        expandButton->setObjectName(QStringLiteral("verticalTabExpandButton"));
        expandButton->setAccessibleName(tab.expanded ? tr("Collapse panes for terminal tab")
                                                     : tr("Expand panes for terminal tab"));
        expandButton->setAccessibleDescription(tr("Show or hide the pane list for this terminal tab."));
        expandButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
        expandButton->setArrowType(tab.expanded ? Qt::DownArrow : Qt::RightArrow);
        expandButton->setAutoRaise(true);
        expandButton->setFixedSize(16, 16);
        expandButton->setProperty("tabId", tab.id);
        expandButton->setProperty("expanded", tab.expanded);
        expandButton->setVisible(isMultiPane);
        connect(expandButton, &QToolButton::clicked, this, [this, tab]() { Q_EMIT tabExpansionToggled(tab.id); });

        auto *tabBadge =
            createProcessBadge(QStringLiteral("verticalTabBadge"), header,
                               isMultiPane ? QString() : (paneCount == 1 ? tab.panes.first().iconName : QString()),
                               tab.isCurrent ? QStringLiteral("bright") : QStringLiteral("muted"));
        tabBadge->setVisible(!isMultiPane);

        TerminalWidget::CommandState tabCommandState = TerminalWidget::CommandState::Idle;
        if (paneCount == 1)
            tabCommandState = tab.panes.first().commandState;
        auto *tabStatusDot = createCommandStatusDot(header, tabCommandState, tab.isCurrent);
        if (isMultiPane)
            tabStatusDot->setVisible(false);

        auto *tabButton = createButton(QStringLiteral("verticalTabButton"), tab.title, header);
        tabButton->setAccessibleName(tr("Terminal tab: %1").arg(tab.title));
        tabButton->setAccessibleDescription(tr("Activate this terminal tab."));
        tabButton->setCheckable(true);
        tabButton->setChecked(tab.isCurrent);
        tabButton->setProperty("tabId", tab.id);
        tabButton->setProperty("active", tab.isCurrent);
        connect(tabButton, &QPushButton::clicked, this, [this, tab]() { Q_EMIT tabActivated(tab.id); });

        headerLayout->addWidget(expandButton, 0, Qt::AlignVCenter);
        headerLayout->addWidget(tabBadge, 0, Qt::AlignVCenter);
        headerLayout->addWidget(tabButton, 1);
        headerLayout->addWidget(tabStatusDot, 0, Qt::AlignVCenter);

        sectionLayout->addWidget(header);

        if (tab.expanded && isMultiPane) {
            auto *paneList = new QWidget(section);
            paneList->setObjectName(QStringLiteral("verticalPaneList"));
            paneList->setAccessibleName(tr("Terminal panes"));
            paneList->setAccessibleDescription(tr("Panes inside the expanded terminal tab."));
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
                paneRowLayout->setContentsMargins(6, 3, 4, 3);
                paneRowLayout->setSpacing(6);

                auto *paneBadge =
                    createProcessBadge(QStringLiteral("verticalPaneBadge"), paneRow, pane.iconName,
                                       pane.isActive ? QStringLiteral("bright") : QStringLiteral("muted"));
                auto *paneButton = createButton(QStringLiteral("verticalPaneButton"), paneTitle, paneRow);
                paneButton->setAccessibleName(tr("Terminal pane: %1").arg(paneTitle));
                paneButton->setAccessibleDescription(tr("Activate this terminal pane."));
                paneButton->setCheckable(true);
                paneButton->setChecked(pane.isActive);
                paneButton->setProperty("tabId", tab.id);
                paneButton->setProperty("paneId", pane.id);
                paneButton->setProperty("active", pane.isActive);
                connect(paneButton, &QPushButton::clicked, this,
                        [this, tabId = tab.id, paneId = pane.id]() { Q_EMIT paneActivated(tabId, paneId); });

                auto *paneStatusDot = createCommandStatusDot(paneRow, pane.commandState, pane.isActive);

                paneRowLayout->addWidget(paneBadge, 0, Qt::AlignVCenter);
                paneRowLayout->addWidget(paneButton, 1);
                paneRowLayout->addWidget(paneStatusDot, 0, Qt::AlignVCenter);
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

void VerticalTabSidebar::setOpacity(qreal opacity) {
    m_opacity = opacity;
    setAttribute(Qt::WA_TranslucentBackground, m_opacity < 1.0);
    update();
}

void VerticalTabSidebar::paintEvent(QPaintEvent *) {
    const auto *helper = DGuiApplicationHelper::instance();
    QColor bg = helper->applicationPalette().color(QPalette::Window);
    bg.setAlpha(qRound(m_opacity * 255));

    QPainter painter(this);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), bg);
}
