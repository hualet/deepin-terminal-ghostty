#include "TabTitleFormat.h"

#include <DLineEdit>
#include <DPushButton>
#include <QCoreApplication>
#include <QDir>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QSysInfo>

#include <pwd.h>
#include <unistd.h>

DWIDGET_USE_NAMESPACE

namespace {

QString shortDirectory(const QString &path) {
    if (path.isEmpty())
        return {};
    const QString cleaned = QDir::cleanPath(path);
    if (cleaned == QStringLiteral("/"))
        return cleaned;
    return cleaned.section(QLatin1Char('/'), -1);
}

} // namespace

QString formatTabTitle(const QString &format, const TabTitleContext &context) {
    // Single pass so substituted values (which may contain '%') are never
    // expanded again.
    QString result;
    result.reserve(format.size());
    for (qsizetype i = 0; i < format.size(); ++i) {
        const QChar ch = format.at(i);
        if (ch != QLatin1Char('%') || i + 1 >= format.size()) {
            result.append(ch);
            continue;
        }

        const QChar key = format.at(i + 1);
        switch (key.unicode()) {
            case 'n':
                result.append(context.programName);
                break;
            case 'd':
                result.append(shortDirectory(context.workingDirectory));
                break;
            case 'D':
                result.append(context.workingDirectory);
                break;
            case '#':
                if (context.sessionNumber > 0)
                    result.append(QString::number(context.sessionNumber));
                break;
            case 'u':
                result.append(context.userName);
                break;
            case 'U':
                if (!context.userName.isEmpty())
                    result.append(context.userName + QLatin1Char('@'));
                break;
            case 'h':
                result.append(context.hostName);
                break;
            case 'w':
                result.append(context.shellTitle);
                break;
            case '%':
                result.append(QLatin1Char('%'));
                break;
            default:
                result.append(ch);
                continue;
        }
        ++i;
    }
    return result;
}

std::optional<QString> tabTitleFormatOverride(const QString &text, const QString &globalFormat) {
    if (text.trimmed().isEmpty() || text == globalFormat)
        return std::nullopt;
    return text;
}

QList<TabTitleVariable> tabTitleVariables(bool remote) {
    if (remote) {
        return {
            {QStringLiteral("%u"), QCoreApplication::translate("TabTitleFormat", "username")},
            {QStringLiteral("%U"), QCoreApplication::translate("TabTitleFormat", "username@")},
            {QStringLiteral("%h"), QCoreApplication::translate("TabTitleFormat", "remote host")},
            {QStringLiteral("%#"), QCoreApplication::translate("TabTitleFormat", "session number")},
            {QStringLiteral("%w"), QCoreApplication::translate("TabTitleFormat", "title set by shell")},
        };
    }
    return {
        {QStringLiteral("%n"), QCoreApplication::translate("TabTitleFormat", "program name")},
        {QStringLiteral("%d"), QCoreApplication::translate("TabTitleFormat", "current directory (short)")},
        {QStringLiteral("%D"), QCoreApplication::translate("TabTitleFormat", "current directory (long)")},
        {QStringLiteral("%#"), QCoreApplication::translate("TabTitleFormat", "session number")},
        {QStringLiteral("%u"), QCoreApplication::translate("TabTitleFormat", "username")},
        {QStringLiteral("%h"), QCoreApplication::translate("TabTitleFormat", "local host")},
        {QStringLiteral("%w"), QCoreApplication::translate("TabTitleFormat", "title set by shell")},
    };
}

QString localUserName() {
    // The user cannot change for the lifetime of the process.
    static const QString userName = []() {
        if (const passwd *pw = ::getpwuid(::getuid()); pw && pw->pw_name)
            return QString::fromLocal8Bit(pw->pw_name);
        return qEnvironmentVariable("USER");
    }();
    return userName;
}

QString localHostName() {
    return QSysInfo::machineHostName();
}

TabTitleFormatEdit::TabTitleFormatEdit(bool remote, QWidget *parent) : QWidget(parent) {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_edit = new DLineEdit(this);
    m_edit->setClearButtonEnabled(true);
    layout->addWidget(m_edit, 1);

    auto *insertButton = new DPushButton(tr("Insert"), this);
    insertButton->setAccessibleName(tr("Insert tab title variable"));
    auto *menu = new QMenu(insertButton);
    for (const auto &variable : tabTitleVariables(remote)) {
        menu->addAction(QStringLiteral("%1: %2").arg(variable.description, variable.token), this,
                        [this, token = variable.token]() {
                            m_edit->lineEdit()->insert(token);
                            m_edit->lineEdit()->setFocus();
                            Q_EMIT editingFinished();
                        });
    }
    insertButton->setMenu(menu);
    layout->addWidget(insertButton);

    connect(m_edit, &DLineEdit::editingFinished, this, &TabTitleFormatEdit::editingFinished);
}

DLineEdit *TabTitleFormatEdit::lineEdit() const {
    return m_edit;
}

QString TabTitleFormatEdit::text() const {
    return m_edit->text();
}

void TabTitleFormatEdit::setText(const QString &text) {
    m_edit->setText(text);
}
