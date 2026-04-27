#include "ThemeLoader.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace ThemeLoader {

QList<TerminalTheme> loadThemes() {
    QList<TerminalTheme> themes;
    QDir themesDir(QStringLiteral(":/themes"));
    const QStringList files = themesDir.entryList(QStringList() << QStringLiteral("*.json"), QDir::Files);
    for (const QString &fileName : files) {
        QFile file(QStringLiteral(":/themes/") + fileName);
        if (!file.open(QFile::ReadOnly))
            continue;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isObject())
            continue;
        QJsonObject obj = doc.object();

        auto parseColor = [](const QJsonArray &arr) -> QColor {
            if (arr.size() != 3)
                return QColor();
            return QColor(arr[0].toInt(), arr[1].toInt(), arr[2].toInt());
        };

        TerminalTheme theme;
        theme.name = obj[QStringLiteral("name")].toString();
        theme.displayName = obj[QStringLiteral("displayName")].toString();
        theme.isDark = obj[QStringLiteral("isDark")].toBool(true);
        theme.foreground = parseColor(obj[QStringLiteral("foreground")].toArray());
        theme.background = parseColor(obj[QStringLiteral("background")].toArray());
        theme.cursor = parseColor(obj[QStringLiteral("cursor")].toArray());
        if (obj.contains(QStringLiteral("selectionBackground")))
            theme.selectionBackground = parseColor(obj[QStringLiteral("selectionBackground")].toArray());
        else
            theme.selectionBackground = QColor(theme.foreground.red(), theme.foreground.green(),
                                               theme.foreground.blue(), theme.isDark ? 60 : 50);
        if (obj.contains(QStringLiteral("selectionForeground")))
            theme.selectionForeground = parseColor(obj[QStringLiteral("selectionForeground")].toArray());

        QJsonArray ansiArr = obj[QStringLiteral("ansi")].toArray();
        for (int i = 0; i < 16 && i < ansiArr.size(); ++i)
            theme.ansi[i] = parseColor(ansiArr[i].toArray());

        themes.append(theme);
    }
    return themes;
}

TerminalTheme findTheme(const QList<TerminalTheme> &themes, const QString &name) {
    for (const auto &t : themes) {
        if (t.name == name)
            return t;
    }
    for (const auto &t : themes) {
        if (t.name == QStringLiteral("dark"))
            return t;
    }
    return themes.isEmpty() ? TerminalTheme() : themes.first();
}

} // namespace ThemeLoader
