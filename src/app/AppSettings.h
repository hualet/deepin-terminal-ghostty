#pragma once

#include <QFont>
#include <QKeySequence>
#include <QObject>
#include <QSize>
#include <QString>

class QTimer;

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
    QFont defaultTerminalFont() const;
    void resetFontSize();
    void setTerminalFont(const QFont &font);

    int cursorShape() const;
    void setCursorShape(int shape);

    bool cursorBlink() const;
    void setCursorBlink(bool blink);

    int scrollbackLines() const;
    void setScrollbackLines(int lines);

    bool verticalTabsEnabled() const;
    void setVerticalTabsEnabled(bool enabled);

    QString colorScheme() const;
    void setColorScheme(const QString &scheme);

    qreal opacity() const;
    bool backgroundBlur() const;
    void setOpacity(qreal opacity);

    bool sessionRestore() const;
    QString sessionRestoreBehavior() const;
    bool hideQuakeOnFocusLoss() const;

    QSize windowSize() const;
    void saveWindowSize(const QSize &size);

    QKeySequence shortcut(const QString &name) const;
    void setShortcut(const QString &name, const QKeySequence &seq);

signals:
    void terminalFontChanged(const QFont &font);
    void cursorShapeChanged(int shape);
    void cursorBlinkChanged(bool blink);
    void scrollbackLinesChanged(int lines);
    void verticalTabsEnabledChanged(bool enabled);
    void colorSchemeChanged(const QString &scheme);
    void opacityChanged(qreal opacity);
    void backgroundBlurChanged(bool enabled);

private:
    explicit AppSettings(QObject *parent = nullptr);
    void init();

    Dtk::Core::DSettings *m_dsettings = nullptr;
    Dtk::Core::QSettingBackend *m_backend = nullptr;
    QString m_configPath;
    QTimer *m_windowSizeTimer = nullptr;
    QSize m_pendingWindowSize;
    static AppSettings *s_instance;
};
