#include "ServerListItem.h"

#include <DFontSizeManager>
#include <DPaletteHelper>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

ServerListItem::ServerListItem(const QString &name, const QString &subtitle, const QString &key, QWidget *parent)
    : QWidget(parent), m_key(key) {
    setFixedHeight(56);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 4, 8, 4);
    mainLayout->setSpacing(8);

    m_iconButton = new DIconButton(this);
    m_iconButton->setIcon(QIcon::fromTheme("utilities-terminal"));
    m_iconButton->setIconSize(QSize(36, 36));
    m_iconButton->setFlat(true);
    m_iconButton->setFocusPolicy(Qt::NoFocus);
    m_iconButton->setEnabled(false);
    mainLayout->addWidget(m_iconButton, 0, Qt::AlignVCenter);

    auto *textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);

    m_nameLabel = new DLabel(name, this);
    DFontSizeManager::instance()->bind(m_nameLabel, DFontSizeManager::T7);
    textLayout->addWidget(m_nameLabel);

    m_subtitleLabel = new DLabel(subtitle, this);
    DFontSizeManager::instance()->bind(m_subtitleLabel, DFontSizeManager::T8);
    QPalette subPalette = m_subtitleLabel->palette();
    subPalette.setColor(QPalette::WindowText, QColor(128, 128, 128));
    m_subtitleLabel->setPalette(subPalette);
    textLayout->addWidget(m_subtitleLabel);

    mainLayout->addLayout(textLayout, 1);
    mainLayout->addStretch();

    setLayout(mainLayout);
    updateBackground();
}

ServerListItem::~ServerListItem() = default;

void ServerListItem::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(rect().adjusted(2, 1, -2, -1), 8, 8);

    DPalette palette = DPaletteHelper::instance()->palette(this);
    QColor bgColor;
    if (m_isPressed) {
        bgColor = palette.color(DPalette::ObviousBackground);
    } else if (m_isFocus) {
        bgColor = palette.color(DPalette::ObviousBackground);
    } else if (m_isHover) {
        bgColor = palette.color(DPalette::ItemBackground);
    } else {
        bgColor = Qt::transparent;
    }

    painter.fillPath(path, bgColor);
}

void ServerListItem::enterEvent(QEnterEvent *event) {
    QWidget::enterEvent(event);
    m_isHover = true;
    update();
}

void ServerListItem::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    m_isHover = false;
    m_isPressed = false;
    update();
}

void ServerListItem::mousePressEvent(QMouseEvent *event) {
    QWidget::mousePressEvent(event);
    if (event->button() == Qt::LeftButton) {
        m_isPressed = true;
        update();
    }
}

void ServerListItem::mouseReleaseEvent(QMouseEvent *event) {
    QWidget::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton && m_isPressed) {
        m_isPressed = false;
        update();
        if (rect().contains(event->pos()))
            emit itemClicked(m_key);
    }
}

void ServerListItem::mouseDoubleClickEvent(QMouseEvent *event) {
    QWidget::mouseDoubleClickEvent(event);
    if (event->button() == Qt::LeftButton)
        emit itemDoubleClicked(m_key);
}

void ServerListItem::focusInEvent(QFocusEvent *event) {
    QWidget::focusInEvent(event);
    m_isFocus = true;
    update();
}

void ServerListItem::focusOutEvent(QFocusEvent *event) {
    QWidget::focusOutEvent(event);
    m_isFocus = false;
    update();
}

void ServerListItem::updateBackground() {
    update();
}
