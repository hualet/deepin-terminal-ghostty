#pragma once

#include <QByteArray>
#include <QRect>
#include <QString>

class TerminalTrace {
public:
    static bool enable(const QString &path, QString *errorMessage = nullptr);
    static void disable();
    static bool isEnabled();

    static void logEvent(const char *event, const QString &detail = {});
    static void logBytes(const char *event, const QByteArray &data);
    static void logResize(const char *event, int cols, int rows, int cellWidth, int cellHeight, const QRect &rect = {});
};
