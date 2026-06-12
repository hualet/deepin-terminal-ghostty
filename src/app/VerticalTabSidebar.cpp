#include "VerticalTabSidebar.h"

#include <DGuiApplicationHelper>
#include <DStyle>
#include <QAbstractAnimation>
#include <QAbstractButton>
#include <QAccessibleEvent>
#include <QApplication>
#include <QBitmap>
#include <QEasingCurve>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSet>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

DGUI_USE_NAMESPACE
DWIDGET_USE_NAMESPACE

namespace {

constexpr int kProcessBadgeSize = 24;
constexpr int kProcessIconSize = 16;
constexpr int kStatusDotSize = 8;
constexpr int kTabCloseButtonSize = 20;
constexpr int kTabCloseIconSize = 16;
constexpr int kReflowAnimationDurationMs = 120;

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

QHash<QString, QPixmap> &badgeCache() {
    static QHash<QString, QPixmap> cache;
    return cache;
}

QPixmap cachedBadgePixmap(const QString &iconName) {
    const QString name = iconName.isEmpty() ? QStringLiteral("terminal") : resolveBadgeName(iconName);
    auto &cache = badgeCache();
    if (cache.contains(name))
        return cache.value(name);

    const QString resourcePath = resolveBadgeResourcePath(iconName);
    QIcon icon(resourcePath);
    if (icon.isNull())
        icon = QIcon(QStringLiteral(":/badges/process/terminal.svg"));
    QPixmap pm = applyCircleMask(icon.pixmap(kProcessIconSize, kProcessIconSize), kProcessIconSize);
    cache.insert(name, pm);
    return pm;
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
    badge->setPixmap(cachedBadgePixmap(iconName));
    return badge;
}

using DotKey = QPair<TerminalWidget::CommandState, QPair<bool, int>>;

QHash<DotKey, QPixmap> &statusDotCache() {
    static QHash<DotKey, QPixmap> cache;
    return cache;
}

QPixmap cachedStatusDotPixmap(TerminalWidget::CommandState state, qreal dpr) {
    const auto *helper = DGuiApplicationHelper::instance();
    const bool isDark = helper->themeType() == DGuiApplicationHelper::DarkType;
    const int dprKey = qRound(dpr * 1000);
    DotKey key(state, qMakePair(isDark, dprKey));

    auto &cache = statusDotCache();
    if (cache.contains(key))
        return cache.value(key);

    QColor color;
    switch (state) {
        case TerminalWidget::CommandState::Succeeded:
            color = isDark ? QColor(46, 213, 115) : QColor(34, 170, 91);
            break;
        case TerminalWidget::CommandState::Failed:
            color = isDark ? QColor(255, 71, 87) : QColor(210, 55, 70);
            break;
        default:
            return {};
    }

    QPixmap pixmap(static_cast<int>(kStatusDotSize * dpr), static_cast<int>(kStatusDotSize * dpr));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(0, 0, kStatusDotSize, kStatusDotSize);
    painter.end();

    cache.insert(key, pixmap);
    return pixmap;
}

QLabel *createCommandStatusDot(QWidget *parent, TerminalWidget::CommandState state, bool isActive, bool hasPending) {
    auto *dot = new QLabel(parent);
    dot->setObjectName(QStringLiteral("commandStatusDot"));
    dot->setAccessibleName(QObject::tr("Command status"));
    dot->setAccessibleDescription(QObject::tr("Shows whether the last command succeeded or failed."));
    dot->setFixedSize(kStatusDotSize, kStatusDotSize);

    if (!hasPending || isActive || state == TerminalWidget::CommandState::Idle
        || state == TerminalWidget::CommandState::Running) {
        dot->setVisible(false);
        return dot;
    }

    if (state != TerminalWidget::CommandState::Succeeded && state != TerminalWidget::CommandState::Failed) {
        dot->setVisible(false);
        return dot;
    }

    dot->setPixmap(cachedStatusDotPixmap(state, dot->devicePixelRatioF()));
    return dot;
}

} // namespace

void ClickableSection::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && sidebar) {
        m_dragStartPos = event->pos();
        m_leftButtonPressed = true;
        m_dragging = false;
        event->accept();
        return;
    }

    if (event->button() == Qt::MiddleButton && sidebar) {
        sidebar->requestTabClose(tabId);
        event->accept();
        return;
    }

    QFrame::mousePressEvent(event);
}

void ClickableSection::mouseMoveEvent(QMouseEvent *event) {
    if (m_leftButtonPressed && sidebar && (event->buttons() & Qt::LeftButton)) {
        const int dragStartDistance = qMax(QApplication::startDragDistance() * 2, 20);
        if (m_dragging || (event->pos() - m_dragStartPos).manhattanLength() >= dragStartDistance) {
            if (!m_dragging)
                sidebar->beginTabDrag(tabId, event->globalPosition().toPoint(), m_dragStartPos);
            m_dragging = true;
            sidebar->previewTabMove(tabId, event->globalPosition().toPoint());
        }

        event->accept();
        return;
    }
    QFrame::mouseMoveEvent(event);
}

void ClickableSection::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_leftButtonPressed && sidebar) {
        const bool wasDragging = m_dragging;
        m_leftButtonPressed = false;
        m_dragging = false;

        if (wasDragging) {
            const bool moved = sidebar->finishTabDrag(tabId, event->globalPosition().toPoint());
            if (!moved)
                Q_EMIT sidebar->tabActivated(tabId);
        } else {
            Q_EMIT sidebar->tabActivated(tabId);
        }

        event->accept();
        return;
    }
    QFrame::mouseReleaseEvent(event);
}

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
        #verticalTabSection[dragPlaceholder="true"] {
            background-color: transparent;
            border-color: transparent;
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
        #verticalTabCloseButton {
            border: none;
            background: transparent;
            padding: 0px;
            margin: 0px;
        }
        #verticalTabCloseButton:hover {
            background-color: %8;
            border-radius: 4px;
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
    m_dragTabId = 0;
    m_dragOriginalIndex = -1;
    m_dragMoved = false;
    rebuild();
}

QList<VerticalTabSidebar::TabItem> VerticalTabSidebar::items() const {
    return m_items;
}

bool VerticalTabSidebar::isTabDragActive() const {
    return m_dragTabId != 0;
}

void VerticalTabSidebar::requestTabMove(int tabId, const QPoint &globalPos) {
    auto *section = sectionForTabId(tabId);
    const QPoint hotSpot = section ? section->rect().center() : QPoint();
    beginTabDrag(tabId, globalPos, hotSpot);
    previewTabMove(tabId, globalPos);
    finishTabDrag(tabId, globalPos);
}

void VerticalTabSidebar::requestTabClose(int tabId) {
    Q_EMIT tabCloseRequested(tabId);
}

void VerticalTabSidebar::beginTabDrag(int tabId, const QPoint &globalPos, const QPoint &hotSpot) {
    if (m_dragTabId == tabId)
        return;

    m_dragTabId = tabId;
    m_dragOriginalIndex = indexOfItemId(tabId);
    m_dragHotSpot = hotSpot;
    m_dragMoved = false;

    auto *section = sectionForTabId(tabId);
    if (!section)
        return;

    QPixmap pixmap(section->size() * section->devicePixelRatioF());
    pixmap.setDevicePixelRatio(section->devicePixelRatioF());
    pixmap.fill(Qt::transparent);
    section->render(&pixmap);

    section->setProperty("dragPlaceholder", true);
    auto *placeholderEffect = new QGraphicsOpacityEffect(section);
    placeholderEffect->setOpacity(0.0);
    section->setGraphicsEffect(placeholderEffect);
    section->style()->unpolish(section);
    section->style()->polish(section);
    section->update();

    if (!m_dragProxy) {
        m_dragProxy = new QLabel(this);
        m_dragProxy->setObjectName(QStringLiteral("verticalTabDragProxy"));
        m_dragProxy->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    m_dragProxy->setPixmap(pixmap);
    m_dragProxy->resize(section->size());
    m_dragProxy->raise();
    m_dragProxy->show();
    moveDragProxy(globalPos);
}

void VerticalTabSidebar::previewTabMove(int tabId, const QPoint &globalPos) {
    if (m_dragTabId != tabId)
        beginTabDrag(tabId, globalPos, sectionForTabId(tabId) ? sectionForTabId(tabId)->rect().center() : QPoint());
    moveDragProxy(globalPos);

    int sourceIndex = -1;
    sourceIndex = indexOfItemId(tabId);
    if (sourceIndex < 0)
        return;

    const int targetIndex = targetIndexForPosition(tabId, globalPos);
    if (targetIndex < 0 || targetIndex == sourceIndex)
        return;

    QHash<ClickableSection *, QPoint> oldPositions;
    for (auto *section : findChildren<ClickableSection *>(QString(), Qt::FindChildrenRecursively))
        oldPositions.insert(section, section->pos());

    auto *section = sectionForTabId(tabId);
    m_items.move(sourceIndex, targetIndex);
    m_dragMoved = true;
    if (section && m_layout) {
        m_layout->removeWidget(section);
        m_layout->insertWidget(targetIndex, section);
        m_layout->invalidate();
        m_layout->activate();
        for (auto it = oldPositions.cbegin(); it != oldPositions.cend(); ++it) {
            auto *movedSection = it.key();
            const QPoint oldPos = it.value();
            const QPoint newPos = movedSection->pos();
            if (oldPos == newPos)
                continue;

            for (auto *animation :
                 movedSection->findChildren<QPropertyAnimation *>(QStringLiteral("verticalTabReflowAnimation"))) {
                animation->stop();
                animation->deleteLater();
            }

            auto *animation = new QPropertyAnimation(movedSection, "pos", movedSection);
            animation->setObjectName(QStringLiteral("verticalTabReflowAnimation"));
            animation->setDuration(kReflowAnimationDurationMs);
            animation->setEasingCurve(QEasingCurve::OutCubic);
            animation->setStartValue(oldPos);
            animation->setEndValue(newPos);
            animation->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }

    QTimer::singleShot(0, this, &VerticalTabSidebar::updateButtonElisions);
}

bool VerticalTabSidebar::finishTabDrag(int tabId, const QPoint &globalPos) {
    if (m_dragTabId != tabId)
        beginTabDrag(tabId, globalPos, sectionForTabId(tabId) ? sectionForTabId(tabId)->rect().center() : QPoint());

    moveDragProxy(globalPos);

    const int finalIndex = indexOfItemId(tabId);
    const int originalIndex = m_dragOriginalIndex;
    const bool moved = m_dragMoved && finalIndex >= 0 && originalIndex >= 0 && finalIndex != originalIndex;
    clearDragProxy();
    m_dragTabId = 0;
    m_dragOriginalIndex = -1;
    m_dragHotSpot = QPoint();
    m_dragMoved = false;

    if (moved)
        Q_EMIT tabMoveRequested(tabId, finalIndex);
    Q_EMIT tabDragFinished();
    return moved;
}

int VerticalTabSidebar::indexOfItemId(int tabId) const {
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == tabId)
            return i;
    }
    return -1;
}

int VerticalTabSidebar::targetIndexForPosition(int tabId, const QPoint &globalPos) const {
    const int sourceIndex = indexOfItemId(tabId);
    if (sourceIndex < 0)
        return -1;

    int insertionIndex = m_items.size();
    auto sections = findChildren<ClickableSection *>(QString(), Qt::FindChildrenRecursively);
    std::sort(sections.begin(), sections.end(), [](const ClickableSection *a, const ClickableSection *b) {
        return a->mapToGlobal(QPoint(0, 0)).y() < b->mapToGlobal(QPoint(0, 0)).y();
    });
    for (auto *section : sections) {
        const int sectionTabId = section->property("tabId").toInt();
        if (sectionTabId == tabId)
            continue;

        int sectionIndex = -1;
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items.at(i).id == sectionTabId) {
                sectionIndex = i;
                break;
            }
        }
        if (sectionIndex < 0)
            continue;

        const QPoint localPos = section->mapFromGlobal(globalPos);
        if (localPos.y() < section->height() / 2) {
            insertionIndex = sectionIndex;
            break;
        }
    }

    int targetIndex = insertionIndex;
    if (sourceIndex < targetIndex)
        --targetIndex;
    return qBound(0, targetIndex, m_items.size() - 1);
}

ClickableSection *VerticalTabSidebar::sectionForTabId(int tabId) const {
    for (auto *section : findChildren<ClickableSection *>(QString(), Qt::FindChildrenRecursively)) {
        if (section->property("tabId").toInt() == tabId)
            return section;
    }
    return nullptr;
}

void VerticalTabSidebar::moveDragProxy(const QPoint &globalPos) {
    if (!m_dragProxy)
        return;

    m_dragProxy->move(mapFromGlobal(globalPos) - m_dragHotSpot);
    m_dragProxy->raise();
}

void VerticalTabSidebar::clearDragProxy() {
    if (auto *section = sectionForTabId(m_dragTabId)) {
        section->setProperty("dragPlaceholder", false);
        section->setGraphicsEffect(nullptr);
        section->style()->unpolish(section);
        section->style()->polish(section);
        section->update();
    }

    if (m_dragProxy) {
        m_dragProxy->hide();
        m_dragProxy->deleteLater();
        m_dragProxy = nullptr;
    }
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

void removeOldPaneList(ClickableSection *section) {
    auto oldList = section->findChildren<QWidget *>(QStringLiteral("verticalPaneList"), Qt::FindDirectChildrenOnly);
    for (auto *w : oldList)
        w->deleteLater();
}

void buildPaneList(ClickableSection *section, const VerticalTabSidebar::TabItem &tab) {
    removeOldPaneList(section);

    const bool isMultiPane = tab.panes.size() > 1;
    if (!tab.expanded || !isMultiPane)
        return;

    auto *paneList = new QWidget(section);
    paneList->setObjectName(QStringLiteral("verticalPaneList"));
    paneList->setAccessibleName(VerticalTabSidebar::tr("Terminal panes"));
    paneList->setAccessibleDescription(VerticalTabSidebar::tr("Panes inside the expanded terminal tab."));
    paneList->setProperty("tabId", tab.id);
    paneList->setAttribute(Qt::WA_TransparentForMouseEvents, true);
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
        const QString paneTitle = pane.title.isEmpty() ? VerticalTabSidebar::tr("Terminal") : pane.title;
        auto *paneRow = new QWidget(paneColumn);
        paneRow->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        allowHorizontalShrink(paneRow);
        auto *paneRowLayout = new QHBoxLayout(paneRow);
        paneRowLayout->setContentsMargins(6, 3, 4, 3);
        paneRowLayout->setSpacing(6);

        auto *paneBadge = createProcessBadge(QStringLiteral("verticalPaneBadge"), paneRow, pane.iconName,
                                             pane.isActive ? QStringLiteral("bright") : QStringLiteral("muted"));
        paneBadge->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        auto *paneButton = createButton(QStringLiteral("verticalPaneButton"), paneTitle, paneRow);
        paneButton->setAccessibleName(VerticalTabSidebar::tr("Terminal pane: %1").arg(paneTitle));
        paneButton->setAccessibleDescription(VerticalTabSidebar::tr("Activate this terminal pane."));
        paneButton->setCheckable(true);
        paneButton->setChecked(pane.isActive);
        paneButton->setProperty("tabId", tab.id);
        paneButton->setProperty("paneId", pane.id);
        paneButton->setProperty("active", pane.isActive);
        paneButton->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        auto *paneStatusDot = createCommandStatusDot(paneRow, pane.commandState, tab.isCurrent,
                                                     !tab.isCurrent && tab.hasPendingCommandResult);
        paneStatusDot->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        paneRowLayout->addWidget(paneBadge, 0, Qt::AlignVCenter);
        paneRowLayout->addWidget(paneButton, 1);
        paneRowLayout->addWidget(paneStatusDot, 0, Qt::AlignVCenter);
        paneLayout->addWidget(paneRow);
    }

    paneListLayout->addWidget(paneColumn, 1);

    auto *sectionLayout = qobject_cast<QVBoxLayout *>(section->layout());
    if (sectionLayout)
        sectionLayout->addWidget(paneList);
}

ClickableSection *buildSection(VerticalTabSidebar *sidebar, const VerticalTabSidebar::TabItem &tab) {
    auto *section = new ClickableSection(sidebar);
    section->setObjectName(QStringLiteral("verticalTabSection"));
    section->setAccessibleName(VerticalTabSidebar::tr("Terminal tab section: %1").arg(tab.title));
    section->setAccessibleDescription(VerticalTabSidebar::tr("A terminal tab and its panes."));
    section->setProperty("tabId", tab.id);
    section->setProperty("isCurrent", tab.isCurrent);
    section->tabId = tab.id;
    section->sidebar = sidebar;
    allowHorizontalShrink(section);

    auto *sectionLayout = new QVBoxLayout(section);
    sectionLayout->setContentsMargins(4, 2, 4, 2);
    sectionLayout->setSpacing(2);

    auto *header = new QWidget(section);
    header->setObjectName(QStringLiteral("verticalTabHeader"));
    header->setAccessibleName(VerticalTabSidebar::tr("Terminal tab header: %1").arg(tab.title));
    header->setProperty("tabId", tab.id);
    allowHorizontalShrink(header);

    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(6, 4, 6, 4);
    headerLayout->setSpacing(6);

    const int paneCount = tab.panes.size();
    const bool isMultiPane = paneCount > 1;

    auto *expandButton = new QToolButton(section);
    expandButton->setObjectName(QStringLiteral("verticalTabExpandButton"));
    expandButton->setAccessibleName(tab.expanded ? VerticalTabSidebar::tr("Collapse panes for terminal tab")
                                                 : VerticalTabSidebar::tr("Expand panes for terminal tab"));
    expandButton->setAccessibleDescription(VerticalTabSidebar::tr("Show or hide the pane list for this terminal tab."));
    expandButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    expandButton->setArrowType(tab.expanded ? Qt::DownArrow : Qt::RightArrow);
    expandButton->setAutoRaise(true);
    expandButton->setFixedSize(16, 16);
    expandButton->setProperty("tabId", tab.id);
    expandButton->setProperty("expanded", tab.expanded);
    expandButton->setVisible(isMultiPane);
    QObject::connect(expandButton, &QToolButton::clicked, sidebar,
                     [sidebar, tab]() { Q_EMIT sidebar->tabExpansionToggled(tab.id); });

    auto *tabBadge =
        createProcessBadge(QStringLiteral("verticalTabBadge"), header,
                           isMultiPane ? QString() : (paneCount == 1 ? tab.panes.first().iconName : QString()),
                           tab.isCurrent ? QStringLiteral("bright") : QStringLiteral("muted"));
    tabBadge->setVisible(!isMultiPane);
    tabBadge->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    TerminalWidget::CommandState tabCommandState = TerminalWidget::CommandState::Idle;
    if (paneCount == 1)
        tabCommandState = tab.panes.first().commandState;
    auto *tabStatusDot = createCommandStatusDot(header, tabCommandState, tab.isCurrent, tab.hasPendingCommandResult);
    if (isMultiPane)
        tabStatusDot->setVisible(false);
    tabStatusDot->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    auto *tabButton = createButton(QStringLiteral("verticalTabButton"), tab.title, header);
    tabButton->setAccessibleName(VerticalTabSidebar::tr("Terminal tab: %1").arg(tab.title));
    tabButton->setAccessibleDescription(VerticalTabSidebar::tr("Activate this terminal tab."));
    tabButton->setCheckable(true);
    tabButton->setChecked(tab.isCurrent);
    tabButton->setProperty("tabId", tab.id);
    tabButton->setProperty("active", tab.isCurrent);
    QObject::connect(tabButton, &QPushButton::clicked, sidebar,
                     [sidebar, tab]() { Q_EMIT sidebar->tabActivated(tab.id); });

    auto *closeButton = new QToolButton(header);
    closeButton->setObjectName(QStringLiteral("verticalTabCloseButton"));
    closeButton->setAccessibleName(VerticalTabSidebar::tr("Close tab"));
    closeButton->setAccessibleDescription(VerticalTabSidebar::tr("Close this terminal tab."));
    closeButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    closeButton->setIcon(DStyle::standardIcon(sidebar->style(), DStyle::SP_CloseButton));
    closeButton->setIconSize(QSize(kTabCloseIconSize, kTabCloseIconSize));
    closeButton->setAutoRaise(true);
    closeButton->setFixedSize(kTabCloseButtonSize, kTabCloseButtonSize);
    closeButton->setProperty("tabId", tab.id);
    closeButton->setVisible(tab.isCurrent);
    QObject::connect(closeButton, &QToolButton::clicked, sidebar,
                     [sidebar, tab]() { Q_EMIT sidebar->tabCloseRequested(tab.id); });

    headerLayout->addWidget(expandButton, 0, Qt::AlignVCenter);
    headerLayout->addWidget(tabBadge, 0, Qt::AlignVCenter);
    headerLayout->addWidget(tabButton, 1);
    headerLayout->addWidget(tabStatusDot, 0, Qt::AlignVCenter);
    headerLayout->addWidget(closeButton, 0, Qt::AlignVCenter);

    sectionLayout->addWidget(header);

    buildPaneList(section, tab);

    return section;
}

QByteArray paneListFingerprint(const QList<TermPane::PaneInfo> &panes, bool expanded, bool isCurrent, bool hasPending) {
    QByteArray fp;
    fp.reserve(panes.size() * 64);
    for (const auto &p : panes) {
        fp.append(p.id.toByteArray());
        fp.append(p.title.toUtf8());
        fp.append(p.iconName.toUtf8());
        fp.append(reinterpret_cast<const char *>(&p.commandState), sizeof(p.commandState));
        fp.append(static_cast<char>(p.isActive ? 1 : 0));
    }
    fp.append(static_cast<char>(expanded ? 1 : 0));
    fp.append(static_cast<char>(isCurrent ? 1 : 0));
    fp.append(static_cast<char>(hasPending ? 1 : 0));
    return fp;
}

void updateSectionHeader(ClickableSection *section, const VerticalTabSidebar::TabItem &tab) {
    const int paneCount = tab.panes.size();
    const bool isMultiPane = paneCount > 1;

    const bool wasCurrent = section->property("isCurrent").toBool();
    section->setProperty("isCurrent", tab.isCurrent);
    if (wasCurrent != tab.isCurrent) {
        section->style()->unpolish(section);
        section->style()->polish(section);
    }

    auto expandButtons =
        section->findChildren<QToolButton *>(QStringLiteral("verticalTabExpandButton"), Qt::FindChildrenRecursively);
    if (!expandButtons.isEmpty()) {
        auto *btn = expandButtons.first();
        btn->setAccessibleName(tab.expanded ? VerticalTabSidebar::tr("Collapse panes for terminal tab")
                                            : VerticalTabSidebar::tr("Expand panes for terminal tab"));
        QAccessibleEvent nameEvent(btn, QAccessible::NameChanged);
        QAccessible::updateAccessibility(&nameEvent);
        btn->setArrowType(tab.expanded ? Qt::DownArrow : Qt::RightArrow);
        btn->setProperty("expanded", tab.expanded);
        btn->setVisible(isMultiPane);
    }

    auto badges = section->findChildren<QLabel *>(QStringLiteral("verticalTabBadge"), Qt::FindChildrenRecursively);
    if (!badges.isEmpty()) {
        auto *badge = badges.first();
        const QString iconName = isMultiPane ? QString() : (paneCount == 1 ? tab.panes.first().iconName : QString());
        badge->setPixmap(cachedBadgePixmap(iconName));
        badge->setProperty("tone", tab.isCurrent ? QStringLiteral("bright") : QStringLiteral("muted"));
        badge->setVisible(!isMultiPane);
    }

    auto dots = section->findChildren<QLabel *>(QStringLiteral("commandStatusDot"), Qt::FindChildrenRecursively);
    if (!dots.isEmpty()) {
        auto *dot = dots.first();
        TerminalWidget::CommandState cmdState = TerminalWidget::CommandState::Idle;
        if (paneCount == 1)
            cmdState = tab.panes.first().commandState;
        bool shouldShow = tab.hasPendingCommandResult && !tab.isCurrent
                          && cmdState != TerminalWidget::CommandState::Idle
                          && cmdState != TerminalWidget::CommandState::Running;
        if (shouldShow
            && (cmdState == TerminalWidget::CommandState::Succeeded
                || cmdState == TerminalWidget::CommandState::Failed)) {
            dot->setPixmap(cachedStatusDotPixmap(cmdState, dot->devicePixelRatioF()));
            dot->setVisible(!isMultiPane);
        } else {
            dot->setVisible(false);
        }
    }

    auto buttons =
        section->findChildren<QPushButton *>(QStringLiteral("verticalTabButton"), Qt::FindChildrenRecursively);
    if (!buttons.isEmpty()) {
        auto *btn = buttons.first();
        btn->setAccessibleName(VerticalTabSidebar::tr("Terminal tab: %1").arg(tab.title));
        QAccessibleEvent nameEvent(btn, QAccessible::NameChanged);
        QAccessible::updateAccessibility(&nameEvent);
        btn->setProperty("_fullText", tab.title);
        btn->setChecked(tab.isCurrent);
        btn->setProperty("active", tab.isCurrent);
    }

    auto closeButtons =
        section->findChildren<QToolButton *>(QStringLiteral("verticalTabCloseButton"), Qt::FindChildrenRecursively);
    if (!closeButtons.isEmpty()) {
        auto *btn = closeButtons.first();
        btn->setVisible(tab.isCurrent);
        btn->setIcon(DStyle::standardIcon(section->style(), DStyle::SP_CloseButton));
    }
}

void VerticalTabSidebar::rebuild() {
    if (!m_layout)
        return;

    QSet<int> liveIds;
    for (const auto &tab : m_items)
        liveIds.insert(tab.id);

    QSet<int> existingIds;
    auto sections = findChildren<ClickableSection *>(QString(), Qt::FindChildrenRecursively);
    for (auto *section : sections)
        existingIds.insert(section->tabId);

    for (auto *section : sections) {
        if (!liveIds.contains(section->tabId)) {
            m_layout->removeWidget(section);
            section->deleteLater();
        }
    }

    for (int i = 0; i < m_items.size(); ++i) {
        const auto &tab = m_items.at(i);
        auto *existing = sectionForTabId(tab.id);

        if (existing) {
            updateSectionHeader(existing, tab);

            QByteArray newFp = paneListFingerprint(tab.panes, tab.expanded, tab.isCurrent, tab.hasPendingCommandResult);
            QByteArray oldFp = existing->property("_paneFp").toByteArray();
            if (newFp != oldFp) {
                buildPaneList(existing, tab);
                existing->setProperty("_paneFp", newFp);
            }

            int currentIdx = -1;
            for (int j = 0; j < m_layout->count(); ++j) {
                if (auto *w = m_layout->itemAt(j)->widget()) {
                    if (auto *cs = qobject_cast<ClickableSection *>(w)) {
                        if (cs->tabId == tab.id) {
                            currentIdx = j;
                            break;
                        }
                    }
                }
            }
            if (currentIdx >= 0 && currentIdx != i) {
                QLayoutItem *item = m_layout->takeAt(currentIdx);
                m_layout->insertWidget(i, item->widget());
                delete item;
            }

        } else {
            auto *section = buildSection(this, tab);
            QByteArray fp = paneListFingerprint(tab.panes, tab.expanded, tab.isCurrent, tab.hasPendingCommandResult);
            section->setProperty("_paneFp", fp);
            m_layout->insertWidget(i, section);
        }
    }

    bool hasStretch = false;
    for (int i = 0; i < m_layout->count(); ++i) {
        if (m_layout->itemAt(i)->spacerItem()) {
            if (!hasStretch) {
                hasStretch = true;
            } else {
                QLayoutItem *item = m_layout->takeAt(i);
                delete item;
                --i;
            }
        }
    }
    if (!hasStretch)
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
