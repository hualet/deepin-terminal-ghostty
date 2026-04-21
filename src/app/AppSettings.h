#pragma once

#include <QFont>
#include <QObject>
#include <QSettings>

class AppSettings : public QObject {
    Q_OBJECT

public:
    static AppSettings *instance();
    static void releaseInstance();

    QFont terminalFont() const;
    void setTerminalFont(const QFont &font);

    int cursorShape() const;
    void setCursorShape(int shape);

    bool cursorBlink() const;
    void setCursorBlink(bool blink);

    int scrollbackLines() const;
    void setScrollbackLines(int lines);

signals:
    void terminalFontChanged(const QFont &font);
    void cursorShapeChanged(int shape);
    void cursorBlinkChanged(bool blink);
    void scrollbackLinesChanged(int lines);

private:
    explicit AppSettings(QObject *parent = nullptr);

    QSettings *m_settings;
    static AppSettings *s_instance;
};
