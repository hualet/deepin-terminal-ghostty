#include "AppSettings.h"
#include "ApplicationMetadata.h"
#include "StartupOptions.h"
#include "TerminalTrace.h"
#include "logging/Logging.h"

#include <DApplication>
#include <DLog>
#include <DWidgetUtil>
#include <QCoreApplication>
#include <QDir>
#include <QLocale>
#include <QProcessEnvironment>
#include <QStringList>
#include <QTranslator>

#include <cstdio>
#include <memory>

DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE

#include "MainWindow.h"
#include "QuakeWindow.h"
#include "TerminalControlService.h"
#include "remote/ServerConfigManager.h"

namespace {

QStringList commandLineArguments(int argc, char *argv[]) {
    QStringList arguments;
    arguments.reserve(argc);
    for (int i = 0; i < argc; ++i)
        arguments.append(QString::fromLocal8Bit(argv[i]));
    return arguments;
}

QStringList translationSearchPaths() {
    const QString appDir = QCoreApplication::applicationDirPath();
    return {
        appDir + QStringLiteral("/translations"),
        appDir + QStringLiteral("/../share/deepin-terminal-ghostty/translations"),
    };
}

bool loadApplicationTranslation(QTranslator &translator) {
    const QLocale locale = QLocale::system();
    for (const QString &path : translationSearchPaths()) {
        if (translator.load(locale, QStringLiteral("deepin-terminal-ghostty"), QStringLiteral("_"), path))
            return true;
    }
    return false;
}

bool forwardStartupToExistingInstance(const StartupOptions &options) {
    // Quake launches manage their own window; embedders relying on
    // --wait-for-child/--propagate-exit-code need a real local session, and
    // --trace-vt expects the trace file from this very process.
    if (options.quakeMode || options.waitForChild || !options.traceVtPath.isEmpty())
        return false;
    if (!AppSettings::instance()->reuseWindow())
        return false;

    QString workingDirectory = options.workingDirectory;
    if (workingDirectory.isEmpty())
        workingDirectory = QDir::currentPath();
    else if (QDir::isRelativePath(workingDirectory))
        workingDirectory = QDir::current().absoluteFilePath(workingDirectory);

    QString error;
    const auto outcome = TerminalControlService::forwardOpenTab(
        workingDirectory, options.execute, QProcessEnvironment::systemEnvironment().toStringList(), 5000, &error);
    if (outcome == TerminalControlService::ForwardOutcome::Forwarded) {
        qCInfo(appLog) << "Opened a new tab in an existing terminal window";
        return true;
    }
    if (outcome == TerminalControlService::ForwardOutcome::Indeterminate) {
        // The request may still sit queued in the owner's blocked GUI thread,
        // so replaying it locally could run --execute twice. NameHasOwner is
        // answered by the bus daemon and stays reliable while the owner is
        // unresponsive: only a definitively absent owner proves non-delivery.
        if (TerminalControlService::serviceNameOwned()) {
            qCInfo(appLog) << "Startup request stays with the existing instance" << error;
            return true;
        }
        qCInfo(appLog) << "Existing instance is gone; starting an independent window:" << error;
        return false;
    }
    qCInfo(appLog) << "Existing instance rejected the startup request:" << error;
    return false;
}

} // namespace

int main(int argc, char *argv[]) {
    DApplication app(argc, argv);

    const StartupOptions startupOptions = parseStartupOptions(commandLineArguments(argc, argv));
    if (startupOptions.showHelp) {
        printf("%s", startupOptions.helpText.toLocal8Bit().constData());
        return 0;
    }
    if (!startupOptions.isValid) {
        fprintf(stderr, "Failed to parse command line: %s\n", startupOptions.error.toLocal8Bit().constData());
        return 2;
    }

    DLogManager::registerJournalAppender();
#ifdef QT_DEBUG
    DLogManager::registerConsoleAppender();
#endif

    qCInfo(appLog) << "Application startup";

    if (!startupOptions.traceVtPath.isEmpty()) {
        QString traceError;
        if (TerminalTrace::enable(startupOptions.traceVtPath, &traceError)) {
            qCInfo(appLog) << "VT trace enabled at" << startupOptions.traceVtPath;
        } else {
            qCWarning(appLog) << "Failed to enable VT trace at" << startupOptions.traceVtPath << traceError;
        }
    }

    applyApplicationMetadata(app);
    app.loadTranslator();

    QTranslator appTranslator;
    if (loadApplicationTranslation(appTranslator)) {
        app.installTranslator(&appTranslator);
        qCInfo(appLog) << "Loaded application translation for locale" << QLocale::system().name();
    } else {
        qCWarning(appLog) << "Failed to load application translation for locale" << QLocale::system().name();
    }

    ServerConfigManager::instance()->initServerConfig();

    TerminalControlService controlService([](const StartupOptions &options) -> MainWindow * {
        auto *window = new MainWindow(options);
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->show();
        Dtk::Widget::moveToCenter(window);
        qCInfo(appLog) << "Control service created main window";
        return window;
    });

    // Wait-mode sessions must neither own the reuse service name nor receive
    // forwarded tabs: the forwarded tab would outlive the embedder's
    // --wait-for-child expectation and get killed with the startup session.
    if (startupOptions.waitForChild) {
        qCInfo(appLog) << "Skipping terminal control service for wait-mode session";
    } else {
        QString controlError;
        if (!controlService.registerOnSessionBus(&controlError)) {
            if (forwardStartupToExistingInstance(startupOptions))
                return 0;
            qCWarning(appLog) << "Failed to register terminal control service:" << controlError;
        }
    }

    std::unique_ptr<MainWindow> window = startupOptions.quakeMode ? std::make_unique<QuakeWindow>(startupOptions)
                                                                  : std::make_unique<MainWindow>(startupOptions);
    int startupExitCode = 0;
    bool startupSessionFinished = false;
    QObject::connect(window.get(), &MainWindow::startupSessionFinished, &app, [&](int exitCode) {
        startupExitCode = exitCode;
        startupSessionFinished = true;
    });
    if (auto *quakeWindow = qobject_cast<QuakeWindow *>(window.get())) {
        quakeWindow->showQuake();
        qCInfo(appLog) << "Quake window shown";
    } else {
        window->show();
        Dtk::Widget::moveToCenter(window.get());
        qCInfo(appLog) << "Main window shown";
    }

    const int appExitCode = app.exec();
    if (startupOptions.propagateExitCode && startupSessionFinished)
        return startupExitCode;
    return appExitCode;
}
