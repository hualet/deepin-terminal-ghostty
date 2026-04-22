#include "ApplicationMetadata.h"
#include "logging/Logging.h"

#include <DApplication>
#include <DLog>
#include <DWidgetUtil>
#include <QCoreApplication>
#include <QLocale>
#include <QStringList>
#include <QTranslator>

DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE

#include "MainWindow.h"
#include "remote/ServerConfigManager.h"

namespace {

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
    DLogManager::registerJournalAppender();
#ifdef QT_DEBUG
    DLogManager::registerConsoleAppender();
#endif

    qCInfo(appLog) << "Application startup";

    DApplication app(argc, argv);
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

    MainWindow window;
    window.show();
    qCInfo(appLog) << "Main window shown";

    Dtk::Widget::moveToCenter(&window);

    return app.exec();
}
