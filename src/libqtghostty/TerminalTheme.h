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
    QColor selectionBackground;
    QColor selectionForeground;
    QColor ansi[16];
};
