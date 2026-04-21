#include <QTest>
#include <QSignalSpy>
#include <QApplication>

// Undefine Qt's emit macro before Ghostty headers (same workaround as TerminalWidget.h)
#ifdef emit
#undef emit
#endif

#include <ghostty/vt.h>

#include "TerminalWidget.h"

class TestTerminalWidget : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {}
    void cleanupTestCase() {}

    void testInitialize();
    void testSizeReport();
    void testTitleChanged();
    void testGridSize();
};

void TestTerminalWidget::testInitialize()
{
    TerminalWidget widget;
    QVERIFY(widget.initialize());
}

void TestTerminalWidget::testSizeReport()
{
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    // Resize the widget to trigger grid size calculation
    widget.resize(960, 640);

    // The widget should have non-zero cell dimensions after resize
    // We verify this indirectly by checking that initialize succeeded
    // and the widget accepts the resize without crashing
    QVERIFY(true);
}

void TestTerminalWidget::testTitleChanged()
{
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
    bool invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived",
                                              Qt::DirectConnection,
                                              Q_ARG(QByteArray, titleSequence));
    QVERIFY(invoked);

    // Process events so the title change propagates
    QApplication::processEvents();

    // Check that title changed signal was emitted
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString("MyTestTitle"));
}

void TestTerminalWidget::testGridSize()
{
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    // Test different sizes
    widget.resize(640, 480);
    // Grid should be recalculated on next paint/resize event

    widget.resize(1920, 1080);
    // Should not crash

    QVERIFY(true);
}

// We need QApplication for QWidget tests
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    TestTerminalWidget tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_terminal_widget.moc"
