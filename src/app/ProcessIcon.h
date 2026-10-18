#pragma once

#include <QString>
#include <QStringList>

// Maps the terminal's foreground process to a badge icon name. `processName`
// is the kernel comm name and `arguments` the process argv, so the result
// follows the program actually running rather than the typed command line.
// Interpreters and package runners (node, python -m, npx, npm exec, ...) are
// looked through to the program they launch. Returns an empty string when the
// process is not recognized.
QString processIconName(const QString &processName, const QStringList &arguments);
