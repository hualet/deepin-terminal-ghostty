#pragma once

#include <DIconButton>
#include <DLabel>
#include <QWidget>

DWIDGET_USE_NAMESPACE

class ServerListItem : public QWidget {
    Q_OBJECT

public:
    explicit ServerListItem(const QString &name, const QString &subtitle, const QString &key,
                            QWidget *parent = nullptr);
    ~ServerListItem() override;

    QString key() const { return m_key; }

signals:
    void itemClicked(const QString &key);
    void itemDoubleClicked(const QString &key);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void updateBackground();

    DIconButton *m_iconButton = nullptr;
    DLabel *m_nameLabel = nullptr;
    DLabel *m_subtitleLabel = nullptr;
    QString m_key;
    bool m_isHover = false;
    bool m_isFocus = false;
    bool m_isPressed = false;
};
