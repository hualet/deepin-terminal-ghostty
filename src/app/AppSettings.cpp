#include "AppSettings.h"

#include "ThemeLoader.h"

#include <DSettings>
#include <DSettingsOption>
#include <QFile>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QStandardPaths>
#include <qsettingbackend.h>

AppSettings *AppSettings::s_instance = nullptr;

namespace {

QString shortcutPath(Dtk::Core::DSettings *settings, const QString &name) {
    static const QStringList kGroups = {"terminal", "tab", "advanced"};

    if (!settings)
        return QString("shortcuts.terminal.%1").arg(name);

    for (const QString &group : kGroups) {
        const QString path = QString("shortcuts.%1.%2").arg(group, name);
        if (settings->option(path))
            return path;
    }

    return QString("shortcuts.terminal.%1").arg(name);
}

} // namespace

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
    m_configPath =
        QString("%1/%2/%3.conf")
            .arg(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation), "deepin", "deepin-terminal-ghostty");
    m_backend = new Dtk::Core::QSettingBackend(m_configPath, this);

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

    auto themeOption = m_dsettings->option("basic.interface.colorScheme");
    if (themeOption) {
        QMap<QString, QVariant> items;
        QStringList keys;
        QStringList values;
        keys.append(QStringLiteral("system"));
        values.append(tr("System"));
        auto themes = ThemeLoader::loadThemes();
        for (const auto &t : themes) {
            keys.append(t.name);
            values.append(t.displayName);
        }
        items.insert(QStringLiteral("keys"), keys);
        items.insert(QStringLiteral("values"), values);
        themeOption->setData(QStringLiteral("items"), items);
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
        else if (key == "basic.interface.verticalTabs")
            emit verticalTabsEnabledChanged(verticalTabsEnabled());
        else if (key == "basic.interface.colorScheme")
            emit colorSchemeChanged(colorScheme());
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

bool AppSettings::verticalTabsEnabled() const {
    return m_dsettings->value("basic.interface.verticalTabs").toBool();
}

void AppSettings::setVerticalTabsEnabled(bool enabled) {
    if (verticalTabsEnabled() == enabled)
        return;

    m_dsettings->setOption("basic.interface.verticalTabs", enabled);
    m_dsettings->sync();
}

QKeySequence AppSettings::shortcut(const QString &name) const {
    return QKeySequence(m_dsettings->value(shortcutPath(m_dsettings, name)).toString());
}

void AppSettings::setShortcut(const QString &name, const QKeySequence &seq) {
    m_dsettings->setOption(shortcutPath(m_dsettings, name), seq.toString());
}

QString AppSettings::colorScheme() const {
    return m_dsettings->value("basic.interface.colorScheme").toString();
}

void AppSettings::setColorScheme(const QString &scheme) {
    m_dsettings->setOption("basic.interface.colorScheme", scheme);
}
