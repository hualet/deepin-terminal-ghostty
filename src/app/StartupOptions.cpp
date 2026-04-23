#include "StartupOptions.h"

#include <QCommandLineOption>
#include <QCommandLineParser>

StartupOptions parseStartupOptions(const QStringList &arguments) {
    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);

    const QCommandLineOption executeOption(QStringLiteral("execute"), QStringLiteral("Execute command in startup tab"),
                                           QStringLiteral("command"));
    const QCommandLineOption workingDirectoryOption(QStringLiteral("working-directory"),
                                                    QStringLiteral("Run the startup session in this directory"),
                                                    QStringLiteral("directory"));
    const QCommandLineOption waitForChildOption(QStringLiteral("wait-for-child"),
                                                QStringLiteral("Close the application when the startup session exits"));
    const QCommandLineOption propagateExitCodeOption(
        QStringLiteral("propagate-exit-code"),
        QStringLiteral("Return the startup session exit code from the application process"));

    parser.addOption(executeOption);
    parser.addOption(workingDirectoryOption);
    parser.addOption(waitForChildOption);
    parser.addOption(propagateExitCodeOption);

    StartupOptions options;
    if (!parser.parse(arguments)) {
        options.isValid = false;
        options.error = parser.errorText();
        return options;
    }

    options.execute = parser.value(executeOption);
    options.workingDirectory = parser.value(workingDirectoryOption);
    options.propagateExitCode = parser.isSet(propagateExitCodeOption);
    options.waitForChild = parser.isSet(waitForChildOption) || options.propagateExitCode;
    return options;
}
