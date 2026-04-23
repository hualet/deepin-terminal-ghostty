#pragma once

#include <QString>
#include <QStringList>

struct StartupOptions {
    bool isValid = true;
    QString error;
    QString execute;
    QString workingDirectory;
    bool waitForChild = false;
    bool propagateExitCode = false;
};

StartupOptions parseStartupOptions(const QStringList &arguments);
