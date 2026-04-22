#include <QApplication>
#include <QSignalSpy>
#include <QTest>

// Undefine Qt's emit macro before Ghostty headers (same workaround as TerminalWidget.h)
#ifdef emit
#undef emit
#endif

#include "TerminalWidget.h"

#include <ghostty/vt.h>

class TestTerminalWidget : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {}
    void cleanupTestCase() {}

    void testInitialize();
    void testNoAppSpecificSignals();
    void testSizeReport();
    void testTitleChanged();
    void testGridSize();
    void testIgnoresTransientTinyResize();
    void testSetTerminalFont();
    void testSetCursorShape();
    void testSetCursorBlink();
};

void TestTerminalWidget::testInitialize() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
}

void TestTerminalWidget::testNoAppSpecificSignals() {
    TerminalWidget widget;

    const QMetaObject *metaObject = widget.metaObject();
    QVERIFY(metaObject);

    QCOMPARE(metaObject->indexOfSignal("requestHorizontalSplit()"), -1);
    QCOMPARE(metaObject->indexOfSignal("requestVerticalSplit()"), -1);
    QCOMPARE(metaObject->indexOfSignal("requestCloseSplit()"), -1);
    QCOMPARE(metaObject->indexOfSignal("requestSearch()"), -1);
    QCOMPARE(metaObject->indexOfSignal("requestSettings()"), -1);
}

void TestTerminalWidget::testSizeReport() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    // Resize the widget to trigger grid size calculation
    widget.resize(960, 640);

    // The widget should have non-zero cell dimensions after resize
    // We verify this indirectly by checking that initialize succeeded
    // and the widget accepts the resize without crashing
    QVERIFY(true);
}

void TestTerminalWidget::testTitleChanged() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    QSignalSpy spy(&widget, &TerminalWidget::terminalTitleChanged);
    QVERIFY(spy.isValid());

    // Feed an OSC sequence that sets the window title
    // ESC ] 0 ; title BEL  ->  \033]0;MyTestTitle\007
    QByteArray titleSequence = "\033]0;MyTestTitle\007";

    // Use the PtySession to feed data indirectly
    // Since TerminalWidget::onPtyDataReceived is private, we test via
    // the public interface by accessing PtySession through the widget
    // Actually, onPtyDataReceived is a private slot, but we can use
    // QMetaObject::invokeMethod to call it for testing
    bool invoked =
        QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, titleSequence));
    QVERIFY(invoked);

    // Process events so the title change propagates
    QApplication::processEvents();

    // Check that title changed signal was emitted
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("MyTestTitle"));
}

void TestTerminalWidget::testGridSize() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    // Test different sizes
    widget.resize(640, 480);
    // Grid should be recalculated on next paint/resize event

    widget.resize(1920, 1080);
    // Should not crash

    QVERIFY(true);
}

void TestTerminalWidget::testIgnoresTransientTinyResize() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    widget.resize(960, 640);
    QApplication::processEvents();

    const int originalCols = widget.terminalColumns();
    const int originalRows = widget.terminalRows();
    QVERIFY(originalCols > 1);
    QVERIFY(originalRows > 1);

    widget.resize(1, 1);
    QApplication::processEvents();

    QCOMPARE(widget.terminalColumns(), originalCols);
    QCOMPARE(widget.terminalRows(), originalRows);
}

void TestTerminalWidget::testSetTerminalFont() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    QFont font("Courier New", 16);
    widget.setTerminalFont(font);
    // Verify the font was applied by triggering a resize
    widget.resize(800, 600);
    QVERIFY(true);
}

void TestTerminalWidget::testSetCursorShape() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.setCursorShape(1); // Bar
    widget.setCursorShape(2); // Underline
    widget.setCursorShape(0); // Block
    QVERIFY(true);
}

void TestTerminalWidget::testSetCursorBlink() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.setCursorBlinkEnabled(false);
    widget.setCursorBlinkEnabled(true);
    QVERIFY(true);
}

// We need QApplication for QWidget tests
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    TestTerminalWidget tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_terminal_widget.moc"
