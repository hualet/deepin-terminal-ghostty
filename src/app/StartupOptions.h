#pragma once

#include <QString>
#include <QStringList>

struct StartupOptions {
    bool isValid = true;
    QString error;
    bool showHelp = false;
    QString helpText;
    QString execute;
    QString workingDirectory;
    QString traceVtPath;
    bool waitForChild = false;
    bool propagateExitCode = false;
    bool quakeMode = false;
};

StartupOptions parseStartupOptions(const QStringList &arguments);
