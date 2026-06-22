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

    const QString sample = QStringLiteral("1  今天是个好日子 ☀️，早上喝了杯咖啡 ☕，\r\n"
                                          "2  然后开始写代码 💻。中午和朋友约了饭 🍜，\r\n"
                                          "3  聊了很多有趣的话题 🗣️。下午去公园散步 🌳，\r\n"
                                          "4  看到一只可爱的狗狗 🐕，心情瞬间变好了 ❤️。\r\n"
                                          "5  晚上准备看部电影 🎬，再早点睡觉 🌙。\r\n"
                                          "6  记得明天要带伞 🌂，天气预报说有雨 🌧️。\r\n");
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
