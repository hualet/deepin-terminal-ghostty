#include <QApplication>
#include <QMainWindow>

#include "TerminalWidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("deepin-terminal-ghostty");
    window.resize(960, 640);

    auto *terminal = new TerminalWidget(&window);
    if (!terminal->initialize()) {
        return 1;
    }

    QObject::connect(terminal, &TerminalWidget::terminalTitleChanged,
                     &window, &QMainWindow::setWindowTitle);

    window.setCentralWidget(terminal);
    window.show();

    return app.exec();
}
