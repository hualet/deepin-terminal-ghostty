#include "AppSettings.h"

#include <QFont>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class TestAppSettings : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void cleanup() { AppSettings::releaseInstance(); }

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

    void testSignals() {
        auto *s = AppSettings::instance();
        QSignalSpy fontSpy(s, &AppSettings::terminalFontChanged);
        QSignalSpy shapeSpy(s, &AppSettings::cursorShapeChanged);
        QSignalSpy blinkSpy(s, &AppSettings::cursorBlinkChanged);
        QSignalSpy scrollSpy(s, &AppSettings::scrollbackLinesChanged);

        QVERIFY(fontSpy.isValid());
        QVERIFY(shapeSpy.isValid());
        QVERIFY(blinkSpy.isValid());
        QVERIFY(scrollSpy.isValid());

        s->setTerminalFont(QFont("DejaVu Sans Mono", 12));
        s->setCursorShape(1);
        s->setCursorBlink(false);
        s->setScrollbackLines(2000);

        QCOMPARE(fontSpy.count(), 1);
        QCOMPARE(shapeSpy.count(), 1);
        QCOMPARE(blinkSpy.count(), 1);
        QCOMPARE(scrollSpy.count(), 1);
    }
};

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    TestAppSettings tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_appsettings.moc"
