#include "AppSettings.h"

#include <DSettings>
#include <DSettingsOption>
#include <QFile>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QStandardPaths>
#include <qsettingbackend.h>

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

AppSettings::AppSettings(QObject *parent) : QObject(parent) {
    init();
}

void AppSettings::init() {
    QString configPath =
        QString("%1/%2/%3/config.conf")
            .arg(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation), "deepin", "deepin-terminal-ghostty");
    m_backend = new Dtk::Core::QSettingBackend(configPath, this);

    QFile configFile(":/settings/default-config.json");
    if (!configFile.open(QFile::ReadOnly))
        qFatal("Failed to open default-config.json");
    QByteArray json = configFile.readAll();
    configFile.close();

    m_dsettings = Dtk::Core::DSettings::fromJson(json);
    m_dsettings->setBackend(m_backend);

    // Populate font family combobox items with system monospace fonts
    auto fontOption = m_dsettings->option("basic.interface.fontFamily");
    if (fontOption) {
        QMap<QString, QVariant> items;
        QStringList keys;
        QStringList values;
        for (const QString &family : QFontDatabase::families(QFontDatabase::Latin)) {
            if (QFontDatabase::isFixedPitch(family)) {
                keys.append(family);
                values.append(family);
            }
        }
        items.insert("keys", keys);
        items.insert("values", values);
        fontOption->setData("items", items);
    }

    // Populate cursor shape combobox items
    auto shapeOption = m_dsettings->option("basic.interface.cursorShape");
    if (shapeOption) {
        QMap<QString, QVariant> items;
        items.insert("keys", QStringList() << "0"
                                           << "1"
                                           << "2");
        items.insert("values", QStringList() << tr("Block") << tr("Bar") << tr("Underline"));
        shapeOption->setData("items", items);
    }

    connect(m_dsettings, &Dtk::Core::DSettings::valueChanged, this, [this](const QString &key, const QVariant &) {
        if (key == "basic.interface.fontFamily" || key == "basic.interface.fontSize")
            emit terminalFontChanged(terminalFont());
        else if (key == "basic.interface.cursorShape")
            emit cursorShapeChanged(cursorShape());
        else if (key == "basic.interface.cursorBlink")
            emit cursorBlinkChanged(cursorBlink());
        else if (key == "basic.interface.scrollbackLines")
            emit scrollbackLinesChanged(scrollbackLines());
    });
}

Dtk::Core::DSettings *AppSettings::dsettings() const {
    return m_dsettings;
}

QFont AppSettings::terminalFont() const {
    QFont font;
    font.setFamily(m_dsettings->value("basic.interface.fontFamily").toString());
    font.setPointSize(m_dsettings->value("basic.interface.fontSize").toInt());
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    return font;
}

void AppSettings::setTerminalFont(const QFont &font) {
    m_dsettings->blockSignals(true);
    m_dsettings->setOption("basic.interface.fontFamily", font.family());
    m_dsettings->setOption("basic.interface.fontSize", font.pointSize());
    m_dsettings->blockSignals(false);
    emit terminalFontChanged(font);
}

int AppSettings::cursorShape() const {
    return m_dsettings->value("basic.interface.cursorShape").toInt();
}

void AppSettings::setCursorShape(int shape) {
    m_dsettings->setOption("basic.interface.cursorShape", shape);
}

bool AppSettings::cursorBlink() const {
    return m_dsettings->value("basic.interface.cursorBlink").toBool();
}

void AppSettings::setCursorBlink(bool blink) {
    m_dsettings->setOption("basic.interface.cursorBlink", blink);
}

int AppSettings::scrollbackLines() const {
    return m_dsettings->value("basic.interface.scrollbackLines").toInt();
}

void AppSettings::setScrollbackLines(int lines) {
    m_dsettings->setOption("basic.interface.scrollbackLines", lines);
}
