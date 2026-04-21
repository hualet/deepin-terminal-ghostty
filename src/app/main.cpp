#include <DApplication>
#include <DWidgetUtil>

DWIDGET_USE_NAMESPACE

#include "MainWindow.h"

int main(int argc, char *argv[]) {
    DApplication app(argc, argv);
    app.setProductName("deepin-terminal-ghostty");
    app.setApplicationDisplayName("Deepin Terminal Ghostty");

    MainWindow window;
    window.show();

    Dtk::Widget::moveToCenter(&window);

    return app.exec();
}
