#include "AppSettings.h"

#include <QCoreApplication>
#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

namespace {

QString settingsStorePath() {
    return QString("%1/%2/%3.conf")
        .arg(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation), "deepin", "deepin-terminal-ghostty");
}

void clearVerticalTabsSetting() {
    QFile::remove(settingsStorePath());
}

} // namespace

class TestAppSettings : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        clearVerticalTabsSetting();
    }

    void cleanup() {
        AppSettings::releaseInstance();
        clearVerticalTabsSetting();
    }

    void testFont() {
        auto *s = AppSettings::instance();
        QFont font("Courier New", 14);
        s->setTerminalFont(font);
        QCOMPARE(s->terminalFont().family(), QString("Courier New"));
        QCOMPARE(s->terminalFont().pointSize(), 14);
    }

    void testCursorShape() {
        auto *s = AppSettings::instance();
        s->setCursorShape(2);
        QCOMPARE(s->cursorShape(), 2);
        s->setCursorShape(0);
        QCOMPARE(s->cursorShape(), 0);
    }

    void testCursorBlink() {
        auto *s = AppSettings::instance();
        s->setCursorBlink(false);
        QCOMPARE(s->cursorBlink(), false);
        s->setCursorBlink(true);
        QCOMPARE(s->cursorBlink(), true);
    }

    void testScrollbackLines() {
        auto *s = AppSettings::instance();
        s->setScrollbackLines(5000);
        QCOMPARE(s->scrollbackLines(), 5000);
    }

    void testVerticalTabsEnabled() {
        auto *s = AppSettings::instance();
        QCOMPARE(s->verticalTabsEnabled(), false);

        s->setVerticalTabsEnabled(true);
        QCoreApplication::processEvents();
        QCOMPARE(s->verticalTabsEnabled(), true);

        AppSettings::releaseInstance();
        QCoreApplication::processEvents();
        s = AppSettings::instance();
        QCOMPARE(s->verticalTabsEnabled(), true);
    }

    void testShortcutsAcrossGroups() {
        auto *s = AppSettings::instance();
        QCOMPARE(s->shortcut("copy"), QKeySequence(QStringLiteral("Ctrl+Shift+C")));
        QCOMPARE(s->shortcut("select_left_workspace"), QKeySequence(QStringLiteral("Alt+Left")));
        QCOMPARE(s->shortcut("switch_fullscreen"), QKeySequence(QStringLiteral("F11")));
    }

    void testSignals() {
        auto *s = AppSettings::instance();
        QSignalSpy fontSpy(s, &AppSettings::terminalFontChanged);
        QSignalSpy shapeSpy(s, &AppSettings::cursorShapeChanged);
        QSignalSpy blinkSpy(s, &AppSettings::cursorBlinkChanged);
        QSignalSpy scrollSpy(s, &AppSettings::scrollbackLinesChanged);
        QSignalSpy verticalTabsSpy(s, &AppSettings::verticalTabsEnabledChanged);

        QVERIFY(fontSpy.isValid());
        QVERIFY(shapeSpy.isValid());
        QVERIFY(blinkSpy.isValid());
        QVERIFY(scrollSpy.isValid());
        QVERIFY(verticalTabsSpy.isValid());

        s->setVerticalTabsEnabled(false);
        verticalTabsSpy.clear();
        s->setTerminalFont(QFont("DejaVu Sans Mono", 12));
        s->setCursorShape(1);
        s->setCursorBlink(false);
        s->setScrollbackLines(2000);
        s->setVerticalTabsEnabled(true);

        QCOMPARE(fontSpy.count(), 1);
        QCOMPARE(shapeSpy.count(), 1);
        QCOMPARE(blinkSpy.count(), 1);
        QCOMPARE(scrollSpy.count(), 1);
        QCOMPARE(verticalTabsSpy.count(), 1);
    }

    void testColorScheme() {
        auto *s = AppSettings::instance();
        s->setColorScheme(QStringLiteral("Dracula"));
        QCOMPARE(s->colorScheme(), QStringLiteral("Dracula"));
    }

    void testOpacity() {
        auto *s = AppSettings::instance();
        s->setOpacity(0.8);
        QCOMPARE(s->opacity(), 0.8);
    }

    void testOpacityClampsToMinimum() {
        auto *s = AppSettings::instance();
        s->setOpacity(0.1);
        QCOMPARE(s->opacity(), 0.2);
    }

    void testOpacityClampsToMaximum() {
        auto *s = AppSettings::instance();
        s->setOpacity(1.5);
        QCOMPARE(s->opacity(), 1.0);
    }

    void testBackgroundBlur() {
        auto *s = AppSettings::instance();
        QCOMPARE(s->backgroundBlur(), false);
    }

    void testDefaultTerminalFont() {
        auto *s = AppSettings::instance();
        QFont defaultFont = s->defaultTerminalFont();
        QCOMPARE(defaultFont.pointSize(), 11);
    }

    void testResetFontSize() {
        auto *s = AppSettings::instance();
        QFont bigFont(s->terminalFont().family(), 20);
        s->setTerminalFont(bigFont);
        QCOMPARE(s->terminalFont().pointSize(), 20);

        s->resetFontSize();
        QCOMPARE(s->terminalFont().pointSize(), 11);
    }

    void testWindowSizeSaveAndLoad() {
        auto *s = AppSettings::instance();
        s->saveWindowSize(QSize(800, 600));
        QTest::qWait(600);

        AppSettings::releaseInstance();
        QCoreApplication::processEvents();

        auto *s2 = AppSettings::instance();
        QCOMPARE(s2->windowSize(), QSize(800, 600));
    }
};

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    TestAppSettings tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_appsettings.moc"
