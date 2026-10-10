#pragma once

#include <QList>
#include <QString>
#include <QWidget>

namespace Dtk::Widget {
class DLineEdit;
}

// Values substituted into a tab title format string. Mirrors the variables
// supported by deepin-terminal:
//   local:  %n program, %d short dir, %D long dir, %# session number,
//           %u user, %h local host, %w shell title
//   remote: %u user, %U user@, %h remote host, %# session number,
//           %w shell title
struct TabTitleContext {
    QString programName;
    QString workingDirectory;
    QString shellTitle;
    QString userName;
    QString hostName;
    int sessionNumber = 0;
};

struct TabTitleVariable {
    QString token;
    QString description;
};

QString formatTabTitle(const QString &format, const TabTitleContext &context);
QList<TabTitleVariable> tabTitleVariables(bool remote);
QString localUserName();
QString localHostName();

// Line edit with an "Insert" button that offers the format variables.
class TabTitleFormatEdit : public QWidget {
    Q_OBJECT

public:
    explicit TabTitleFormatEdit(bool remote, QWidget *parent = nullptr);

    Dtk::Widget::DLineEdit *lineEdit() const;
    QString text() const;
    void setText(const QString &text);

signals:
    void editingFinished();

private:
    Dtk::Widget::DLineEdit *m_edit = nullptr;
};
