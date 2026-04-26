#pragma once

#include "libqtghostty/TerminalTheme.h"

#include <QList>
#include <QString>

namespace ThemeLoader {
QList<TerminalTheme> loadThemes();
TerminalTheme findTheme(const QList<TerminalTheme> &themes, const QString &name);
} // namespace ThemeLoader
