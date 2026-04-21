#include <DApplication>
#include <DWidgetUtil>
#include <QLocale>
#include <QStringList>
#include <QTranslator>

DWIDGET_USE_NAMESPACE

#include "MainWindow.h"

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
    DApplication app(argc, argv);
    app.setProductName("deepin-terminal-ghostty");
    app.setApplicationDisplayName("Deepin Terminal Ghostty");

    QTranslator appTranslator;
    if (loadApplicationTranslation(appTranslator))
        app.installTranslator(&appTranslator);

    MainWindow window;
    window.show();

    Dtk::Widget::moveToCenter(&window);

    return app.exec();
}
