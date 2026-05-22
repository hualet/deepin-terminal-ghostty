#include "ApplicationMetadata.h"
#include "StartupOptions.h"
#include "logging/Logging.h"

#include <DApplication>
#include <DLog>
#include <DWidgetUtil>
#include <QCoreApplication>
#include <QLocale>
#include <QStringList>
#include <QTranslator>

#include <cstdio>
#include <memory>

DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE

#include "MainWindow.h"
#include "QuakeWindow.h"
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
