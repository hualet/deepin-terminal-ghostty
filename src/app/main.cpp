#include <DApplication>
#include <DMainWindow>
#include <DWidgetUtil>

DWIDGET_USE_NAMESPACE

#include "TerminalWidget.h"

int main(int argc, char *argv[])
{
    DApplication app(argc, argv);
    app.setProductName("deepin-terminal-ghostty");
    app.setApplicationDisplayName("Deepin Terminal Ghostty");

    DMainWindow window;
    window.setWindowTitle("deepin-terminal-ghostty");
    window.resize(960, 640);

    auto *terminal = new TerminalWidget(&window);
    if (!terminal->initialize()) {
        return 1;
    }

    QObject::connect(terminal, &TerminalWidget::terminalTitleChanged,
                     &window, &DMainWindow::setWindowTitle);

    window.setCentralWidget(terminal);
    window.show();

    Dtk::Widget::moveToCenter(&window);

    return app.exec();
}
