#pragma once

#include <QFont>
#include <QKeySequence>
#include <QObject>
#include <QString>

namespace Dtk::Core {
class DSettings;
class QSettingBackend;
} // namespace Dtk::Core

class AppSettings : public QObject {
    Q_OBJECT

public:
    static AppSettings *instance();
    static void releaseInstance();

    Dtk::Core::DSettings *dsettings() const;

    QFont terminalFont() const;
    void setTerminalFont(const QFont &font);

    int cursorShape() const;
    void setCursorShape(int shape);

    bool cursorBlink() const;
    void setCursorBlink(bool blink);

    int scrollbackLines() const;
    void setScrollbackLines(int lines);

    bool verticalTabsEnabled() const;
    void setVerticalTabsEnabled(bool enabled);

    QKeySequence shortcut(const QString &name) const;
    void setShortcut(const QString &name, const QKeySequence &seq);

signals:
    void terminalFontChanged(const QFont &font);
    void cursorShapeChanged(int shape);
    void cursorBlinkChanged(bool blink);
    void scrollbackLinesChanged(int lines);
    void verticalTabsEnabledChanged(bool enabled);

private:
    explicit AppSettings(QObject *parent = nullptr);
    void init();

    Dtk::Core::DSettings *m_dsettings = nullptr;
    Dtk::Core::QSettingBackend *m_backend = nullptr;
    QString m_configPath;
    static AppSettings *s_instance;
};
