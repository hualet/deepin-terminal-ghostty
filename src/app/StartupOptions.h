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
    // Not parsed from the command line; set programmatically when a startup
    // request is forwarded from another process.
    QStringList environment;
    QString traceVtPath;
    bool waitForChild = false;
    bool propagateExitCode = false;
    bool quakeMode = false;
};

StartupOptions parseStartupOptions(const QStringList &arguments);
