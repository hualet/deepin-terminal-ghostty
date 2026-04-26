#pragma once

#include <QColor>
#include <QString>

struct TerminalTheme {
    QString name;
    QString displayName;
    bool isDark = true;
    QColor foreground;
    QColor background;
    QColor cursor;
    QColor ansi[16];
};
