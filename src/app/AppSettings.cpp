#include "AppSettings.h"

AppSettings *AppSettings::s_instance = nullptr;

AppSettings *AppSettings::instance() {
    if (!s_instance)
        s_instance = new AppSettings();
    return s_instance;
}

void AppSettings::releaseInstance() {
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

AppSettings::AppSettings(QObject *parent)
    : QObject(parent), m_settings(new QSettings("deepin", "deepin-terminal-ghostty", this)) {}

QFont AppSettings::terminalFont() const {
    QFont font;
    font.setFamily(m_settings->value("basic/fontFamily", "Monospace").toString());
    font.setPointSize(m_settings->value("basic/fontSize", 11).toInt());
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    return font;
}

void AppSettings::setTerminalFont(const QFont &font) {
    m_settings->setValue("basic/fontFamily", font.family());
    m_settings->setValue("basic/fontSize", font.pointSize());
    emit terminalFontChanged(font);
}

int AppSettings::cursorShape() const {
    return m_settings->value("basic/cursorShape", 0).toInt();
}

void AppSettings::setCursorShape(int shape) {
    m_settings->setValue("basic/cursorShape", shape);
    emit cursorShapeChanged(shape);
}

bool AppSettings::cursorBlink() const {
    return m_settings->value("basic/cursorBlink", true).toBool();
}

void AppSettings::setCursorBlink(bool blink) {
    m_settings->setValue("basic/cursorBlink", blink);
    emit cursorBlinkChanged(blink);
}

int AppSettings::scrollbackLines() const {
    return m_settings->value("basic/scrollbackLines", 1000).toInt();
}

void AppSettings::setScrollbackLines(int lines) {
    m_settings->setValue("basic/scrollbackLines", lines);
    emit scrollbackLinesChanged(lines);
}
