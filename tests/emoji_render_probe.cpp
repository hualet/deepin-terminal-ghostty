#include "PtySession.h"
#include "TerminalWidget.h"

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QInputMethodQueryEvent>
#include <QPainter>
#include <QThread>
#include <QTimer>

#include <cstdio>

namespace {

QImage renderWidgetImage(QWidget &widget) {
    QImage image(widget.size() * widget.devicePixelRatioF(), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(widget.devicePixelRatioF());
    image.fill(Qt::transparent);

    QPainter painter(&image);
    widget.render(&painter);
    return image;
}

bool feedTerminalOutput(TerminalWidget &widget, const QByteArray &data) {
    const int previousFlushCount = widget.debugPtyFlushCount();
    const bool invoked =
        QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, data));
    if (!invoked)
        return false;

    for (int i = 0; i < 100 && widget.debugPtyFlushCount() == previousFlushCount; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(10);
    }
    return widget.debugPtyFlushCount() > previousFlushCount;
}

int countColorPixels(const QImage &image) {
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            const int maxChannel = qMax(color.red(), qMax(color.green(), color.blue()));
            const int minChannel = qMin(color.red(), qMin(color.green(), color.blue()));
            if (color.alpha() > 20 && maxChannel - minChannel > 30)
                ++count;
        }
    }
    return count;
}

bool renderProbe(const QString &name, std::optional<TerminalWidget::EmojiRenderMode> mode) {
    PtySession::StartOptions options;
    options.command = QStringLiteral("sleep 5");

    TerminalWidget widget;
    widget.setStartOptions(options);
    if (mode)
        widget.debugSetEmojiRenderModeForTesting(*mode);
    if (!widget.initialize())
        return false;

    widget.resize(760, 720);
    widget.show();
    for (int i = 0; i < 20; ++i)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    const QString sample = QStringLiteral("01 👋 wave\r\n"
                                          "02 🤚 raised-back-hand\r\n"
                                          "03 🖐 hand-splayed\r\n"
                                          "04 ✋ raised-hand\r\n"
                                          "05 🖖 vulcan\r\n"
                                          "06 👌 ok\r\n"
                                          "07 🤌 pinched-fingers\r\n"
                                          "08 🤏 pinching-hand\r\n"
                                          "09 ✌️ victory\r\n"
                                          "10 🤞 crossed-fingers\r\n"
                                          "11 🫰 finger-heart\r\n"
                                          "12 🤟 love-you\r\n"
                                          "13 🤘 horns\r\n"
                                          "14 🤙 call-me\r\n"
                                          "15 🫵 point-at-viewer\r\n"
                                          "16 🫱 rightwards-hand\r\n"
                                          "17 🫲 leftwards-hand\r\n");
    if (!feedTerminalOutput(widget, QByteArray("\033[?25l")))
        return false;
    if (!feedTerminalOutput(widget, sample.toUtf8()))
        return false;

    const QImage image = renderWidgetImage(widget);
    const QString path = QStringLiteral("/tmp/deepin-terminal-emoji-%1.png").arg(name);
    if (!image.save(path))
        return false;

    std::printf("%s path=%s fallback_draws=%d color_pixels=%d visible_text=%s\n", name.toUtf8().constData(),
                path.toUtf8().constData(), widget.debugLastFrameEmojiFallbackDrawCount(), countColorPixels(image),
                widget.visibleText().toUtf8().constData());
    return true;
}

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QDir().mkpath(QStringLiteral("/tmp"));

    const bool autoOk = renderProbe(QStringLiteral("auto"), std::nullopt);
    const bool fallbackOk = renderProbe(QStringLiteral("fallback"), TerminalWidget::EmojiRenderMode::CustomFallback);
    return autoOk && fallbackOk ? 0 : 1;
}
