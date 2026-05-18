#include "StartupOptions.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>

StartupOptions parseStartupOptions(const QStringList &arguments) {
    QCoreApplication::setApplicationName(QStringLiteral("deepin-terminal-ghostty"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Deepin Terminal Ghostty"));
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    const QCommandLineOption helpOption = parser.addHelpOption();

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
    const QCommandLineOption quakeModeOption(QStringLiteral("quake-mode"), QStringLiteral("Run in quake mode"));

    parser.addOption(executeOption);
    parser.addOption(workingDirectoryOption);
    parser.addOption(waitForChildOption);
    parser.addOption(propagateExitCodeOption);
    parser.addOption(quakeModeOption);

    StartupOptions options;
    if (!parser.parse(arguments)) {
        options.isValid = false;
        options.error = parser.errorText();
        return options;
    }

    if (parser.isSet(helpOption)) {
        options.showHelp = true;
        options.helpText = parser.helpText();
        if (!arguments.isEmpty())
            options.helpText.replace(QStringLiteral("<executable_name>"), QFileInfo(arguments.first()).fileName());
        return options;
    }

    options.execute = parser.value(executeOption);
    options.workingDirectory = parser.value(workingDirectoryOption);
    options.propagateExitCode = parser.isSet(propagateExitCodeOption);
    options.waitForChild = parser.isSet(waitForChildOption) || options.propagateExitCode;
    options.quakeMode = parser.isSet(quakeModeOption);
    return options;
}
