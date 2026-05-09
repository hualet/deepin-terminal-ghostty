#include "AppSettings.h"

#include "ThemeLoader.h"

#include <DSettings>
#include <DSettingsOption>
#include <QFile>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <qsettingbackend.h>

AppSettings *AppSettings::s_instance = nullptr;

namespace {

constexpr auto kFontFamilyPath = "basic.interface.fontFamily";
constexpr auto kFontSizePath = "basic.interface.fontSize";
constexpr auto kCursorShapePath = "basic.cursor.cursorShape";
constexpr auto kCursorBlinkPath = "basic.cursor.cursorBlink";
constexpr auto kScrollbackLinesPath = "advanced.scrolling.scrollbackLines";
constexpr auto kVerticalTabsPath = "basic.layout.verticalTabs";
constexpr auto kColorSchemePath = "basic.interface.colorScheme";
constexpr auto kOpacityPath = "basic.interface.opacity";
constexpr auto kBackgroundBlurPath = "basic.interface.blurred_background";
constexpr auto kSessionRestorePath = "advanced.session.sessionRestore";
constexpr auto kSessionRestoreBehaviorPath = "advanced.session.sessionRestoreBehavior";

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
    auto fontOption = m_dsettings->option(kFontFamilyPath);
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
    auto shapeOption = m_dsettings->option(kCursorShapePath);
    if (shapeOption) {
        QMap<QString, QVariant> items;
        items.insert("keys", QStringList() << "0"
                                           << "1"
                                           << "2");
        items.insert("values", QStringList() << tr("Block") << tr("Bar") << tr("Underline"));
        shapeOption->setData("items", items);
    }

    auto themeOption = m_dsettings->option(kColorSchemePath);
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

    auto behaviorOption = m_dsettings->option(kSessionRestoreBehaviorPath);
    if (behaviorOption) {
        QMap<QString, QVariant> items;
        items.insert(QStringLiteral("keys"), QStringList() << QStringLiteral("ask") << QStringLiteral("auto"));
        items.insert(QStringLiteral("values"), QStringList() << tr("Ask") << tr("Restore automatically"));
        behaviorOption->setData(QStringLiteral("items"), items);
    }

    connect(m_dsettings, &Dtk::Core::DSettings::valueChanged, this, [this](const QString &key, const QVariant &) {
        if (key == kFontFamilyPath || key == kFontSizePath)
            emit terminalFontChanged(terminalFont());
        else if (key == kCursorShapePath)
            emit cursorShapeChanged(cursorShape());
        else if (key == kCursorBlinkPath)
            emit cursorBlinkChanged(cursorBlink());
        else if (key == kScrollbackLinesPath)
            emit scrollbackLinesChanged(scrollbackLines());
        else if (key == kVerticalTabsPath)
            emit verticalTabsEnabledChanged(verticalTabsEnabled());
        else if (key == kColorSchemePath)
            emit colorSchemeChanged(colorScheme());
        else if (key == kOpacityPath)
            emit opacityChanged(opacity());
        else if (key == kBackgroundBlurPath)
            emit backgroundBlurChanged(backgroundBlur());
    });
}

Dtk::Core::DSettings *AppSettings::dsettings() const {
    return m_dsettings;
}

QFont AppSettings::terminalFont() const {
    QFont font;
    font.setFamily(m_dsettings->value(kFontFamilyPath).toString());
    font.setPointSize(m_dsettings->value(kFontSizePath).toInt());
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    return font;
}

QFont AppSettings::defaultTerminalFont() const {
    QFont font = terminalFont();
    auto sizeOpt = m_dsettings->option(kFontSizePath);
    font.setPointSize(sizeOpt ? sizeOpt->defaultValue().toInt() : 11);
    return font;
}

void AppSettings::resetFontSize() {
    auto sizeOpt = m_dsettings->option(kFontSizePath);
    int defaultSize = sizeOpt ? sizeOpt->defaultValue().toInt() : 11;
    m_dsettings->setOption(kFontSizePath, defaultSize);
}

void AppSettings::setTerminalFont(const QFont &font) {
    m_dsettings->blockSignals(true);
    m_dsettings->setOption(kFontFamilyPath, font.family());
    m_dsettings->setOption(kFontSizePath, font.pointSize());
    m_dsettings->blockSignals(false);
    emit terminalFontChanged(font);
}

int AppSettings::cursorShape() const {
    return m_dsettings->value(kCursorShapePath).toInt();
}

void AppSettings::setCursorShape(int shape) {
    m_dsettings->setOption(kCursorShapePath, shape);
}

bool AppSettings::cursorBlink() const {
    return m_dsettings->value(kCursorBlinkPath).toBool();
}

void AppSettings::setCursorBlink(bool blink) {
    m_dsettings->setOption(kCursorBlinkPath, blink);
}

int AppSettings::scrollbackLines() const {
    return m_dsettings->value(kScrollbackLinesPath).toInt();
}

void AppSettings::setScrollbackLines(int lines) {
    m_dsettings->setOption(kScrollbackLinesPath, lines);
}

bool AppSettings::verticalTabsEnabled() const {
    return m_dsettings->value(kVerticalTabsPath).toBool();
}

void AppSettings::setVerticalTabsEnabled(bool enabled) {
    if (verticalTabsEnabled() == enabled)
        return;

    m_dsettings->setOption(kVerticalTabsPath, enabled);
    m_dsettings->sync();
}

QSize AppSettings::windowSize() const {
    QSettings s(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                    + "/deepin/deepin-terminal-ghostty-state.conf",
                QSettings::IniFormat);
    int w = s.value("MainWindow/width", 0).toInt();
    int h = s.value("MainWindow/height", 0).toInt();
    return (w > 0 && h > 0) ? QSize(w, h) : QSize();
}

void AppSettings::saveWindowSize(const QSize &sz) {
    if (!m_windowSizeTimer) {
        m_windowSizeTimer = new QTimer(this);
        m_windowSizeTimer->setSingleShot(true);
        m_windowSizeTimer->setInterval(500);
        connect(m_windowSizeTimer, &QTimer::timeout, this, [this]() {
            QSettings s(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                            + "/deepin/deepin-terminal-ghostty-state.conf",
                        QSettings::IniFormat);
            s.setValue("MainWindow/width", m_pendingWindowSize.width());
            s.setValue("MainWindow/height", m_pendingWindowSize.height());
        });
    }
    m_pendingWindowSize = sz;
    m_windowSizeTimer->start();
}

QKeySequence AppSettings::shortcut(const QString &name) const {
    return QKeySequence(m_dsettings->value(shortcutPath(m_dsettings, name)).toString());
}

void AppSettings::setShortcut(const QString &name, const QKeySequence &seq) {
    m_dsettings->setOption(shortcutPath(m_dsettings, name), seq.toString());
}

QString AppSettings::colorScheme() const {
    return m_dsettings->value(kColorSchemePath).toString();
}

void AppSettings::setColorScheme(const QString &scheme) {
    m_dsettings->setOption(kColorSchemePath, scheme);
}

qreal AppSettings::opacity() const {
    return m_dsettings->value(kOpacityPath).toInt() / 100.0;
}

bool AppSettings::backgroundBlur() const {
    return m_dsettings->value(kBackgroundBlurPath).toBool();
}

void AppSettings::setOpacity(qreal opacity) {
    int val = qBound(20, qRound(opacity * 100.0), 100);
    m_dsettings->setOption(kOpacityPath, val);
}

bool AppSettings::sessionRestore() const {
    return m_dsettings->value(kSessionRestorePath).toBool();
}

QString AppSettings::sessionRestoreBehavior() const {
    return m_dsettings->value(kSessionRestoreBehaviorPath).toString();
}
