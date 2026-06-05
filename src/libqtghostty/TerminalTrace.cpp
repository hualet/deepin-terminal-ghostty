#include "TerminalTrace.h"

#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include <memory>

namespace {

QMutex &traceMutex() {
    static QMutex mutex;
    return mutex;
}

std::unique_ptr<QFile> &traceFile() {
    static std::unique_ptr<QFile> file;
    return file;
}

QString escapedBytes(const QByteArray &data) {
    QString result;
    result.reserve(data.size() * 4);

    constexpr char kHex[] = "0123456789abcdef";
    for (unsigned char byte : data) {
        switch (byte) {
            case '\a':
                result += QStringLiteral("\\a");
                break;
            case '\b':
                result += QStringLiteral("\\b");
                break;
            case '\t':
                result += QStringLiteral("\\t");
                break;
            case '\n':
                result += QStringLiteral("\\n");
                break;
            case '\r':
                result += QStringLiteral("\\r");
                break;
            case 0x1b:
                result += QStringLiteral("\\e");
                break;
            case '\\':
                result += QStringLiteral("\\\\");
                break;
            default:
                if (byte >= 0x20 && byte <= 0x7e) {
                    result += QChar(static_cast<ushort>(byte));
                } else {
                    result += QStringLiteral("\\x");
                    result += QLatin1Char(kHex[(byte >> 4) & 0x0f]);
                    result += QLatin1Char(kHex[byte & 0x0f]);
                }
                break;
        }
    }

    return result;
}

void writeTraceLine(const QString &line) {
    QFile *file = traceFile().get();
    if (!file || !file->isOpen())
        return;

    QTextStream stream(file);
    stream << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) << ' ' << line << '\n';
    stream.flush();
}

} // namespace

bool TerminalTrace::enable(const QString &path, QString *errorMessage) {
    if (path.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("trace path is empty");
        return false;
    }

    QMutexLocker locker(&traceMutex());

    auto file = std::make_unique<QFile>(path);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = file->errorString();
        return false;
    }

    traceFile() = std::move(file);
    writeTraceLine(QStringLiteral("trace.enabled path=\"%1\"").arg(path));
    return true;
}

void TerminalTrace::disable() {
    QMutexLocker locker(&traceMutex());
    if (traceFile())
        writeTraceLine(QStringLiteral("trace.disabled"));
    traceFile().reset();
}

bool TerminalTrace::isEnabled() {
    QMutexLocker locker(&traceMutex());
    return traceFile() && traceFile()->isOpen();
}

void TerminalTrace::logEvent(const char *event, const QString &detail) {
    QMutexLocker locker(&traceMutex());
    if (!traceFile() || !traceFile()->isOpen())
        return;
    if (detail.isEmpty()) {
        writeTraceLine(QString::fromLatin1(event));
        return;
    }

    writeTraceLine(QStringLiteral("%1 %2").arg(QString::fromLatin1(event), detail));
}

void TerminalTrace::logBytes(const char *event, const QByteArray &data) {
    QMutexLocker locker(&traceMutex());
    if (!traceFile() || !traceFile()->isOpen())
        return;
    writeTraceLine(QStringLiteral("%1 bytes=%2 data=\"%3\"")
                       .arg(QString::fromLatin1(event))
                       .arg(data.size())
                       .arg(escapedBytes(data)));
}

void TerminalTrace::logResize(const char *event, int cols, int rows, int cellWidth, int cellHeight, const QRect &rect) {
    if (!isEnabled())
        return;

    QString detail = QStringLiteral("cols=%1 rows=%2 cell=%3x%4").arg(cols).arg(rows).arg(cellWidth).arg(cellHeight);
    if (rect.isValid()) {
        detail += QStringLiteral(" rect=%1,%2 %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
    }
    logEvent(event, detail);
}
