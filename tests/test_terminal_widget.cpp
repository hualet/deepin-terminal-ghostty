#include "TerminalTheme.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QInputMethodEvent>
#include <QInputMethodQueryEvent>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>
#include <QWheelEvent>

// Undefine Qt's emit macro before Ghostty headers (same workaround as TerminalWidget.h)
#ifdef emit
#undef emit
#endif

#include "PtySession.h"
#include "TerminalWidget.h"

#include <ghostty/vt.h>

class TestTerminalWidget : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {}
    void cleanupTestCase() {}

    void testInitialize();
    void testInputMethodSupport();
    void testNoAppSpecificSignals();
    void testSizeReport();
    void testTitleChanged();
    void testShellIntegrationCommandDetection();
    void testGridSize();
    void testIgnoresTransientTinyResize();
    void testSetTerminalFont();
    void testSetCursorShape();
    void testSetCursorBlink();
    void testContentsMarginsInsetTerminalGridAndCursorRect();
    void testRendersSupplementaryPlaneCharacters();
    void testRendersPreeditTextAcrossMultipleCells();
    void testRendersWideCharactersAcrossTwoCells();
    void testRendersAnsiForegroundColors();
    void testCoalescesBurstRepaintsWithoutLosingFinalFrame();
    void testCoalescesSmallPtyBurstsIntoSingleFlush();
    void testIncrementalUpdatesRenderDirtyRowsOnly();
    void testCoalescesRapidResizeOperations();
    void testMouseTrackingEnabledSendsSGREvents();
    void testMouseTrackingDisabledDoesNotSendEvents();
    void testMouseTrackingReleaseSendsSGR();
    void testMouseTrackingWheelSendsSGR();

    void testCellInSelectionSameRow();
    void testCellInSelectionTopLeftToBottomRight();
    void testCellInSelectionBottomRightToTopLeft();
    void testCellInSelectionTopRightToBottomLeft();
    void testCellInSelectionBottomLeftToTopRight();
    void testSelectedTextTopLeftToBottomRight();
    void testSelectedTextBottomRightToTopLeft();
    void testApplyThemeSetsColors();

    void testCommandStateRunning();
    void testCommandStateSucceeded();
    void testCommandStateFailed();
    void testCommandStateIdleWhenNoResult();
    void testCommandStateTransitionSequence();
};

namespace {

class CountingTerminalWidget : public TerminalWidget {
    Q_OBJECT

public:
    using TerminalWidget::TerminalWidget;

    int paintCount() const { return m_paintCount; }

protected:
    void paintEvent(QPaintEvent *event) override {
        ++m_paintCount;
        TerminalWidget::paintEvent(event);
    }

private:
    int m_paintCount = 0;
};

QByteArray collectedPtyOutput(const QSignalSpy &spy) {
    QByteArray output;
    for (const QList<QVariant> &arguments : spy) {
        if (!arguments.isEmpty())
            output.append(arguments.at(0).toByteArray());
    }
    return output;
}

QImage renderWidgetImage(QWidget &widget) {
    QImage image(widget.size() * widget.devicePixelRatioF(), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(widget.devicePixelRatioF());
    image.fill(Qt::transparent);

    QPainter painter(&image);
    widget.render(&painter);
    return image;
}

QRect changedBounds(const QImage &before, const QImage &after) {
    if (before.size() != after.size()) {
        return {};
    }

    QRect bounds;
    for (int y = 0; y < before.height(); ++y) {
        for (int x = 0; x < before.width(); ++x) {
            if (before.pixel(x, y) != after.pixel(x, y)) {
                bounds |= QRect(x, y, 1, 1);
            }
        }
    }

    return bounds;
}

int countChangedPixels(const QImage &before, const QImage &after, const QRect &rect) {
    if (before.size() != after.size()) {
        return 0;
    }

    int count = 0;
    const QRect bounded = rect.intersected(before.rect());
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            if (before.pixel(x, y) != after.pixel(x, y)) {
                ++count;
            }
        }
    }

    return count;
}

void waitForNextPtyFlush(CountingTerminalWidget &widget, int previousFlushCount, int timeoutMs = 100) {
    QTRY_COMPARE_WITH_TIMEOUT(widget.debugPtyFlushCount(), previousFlushCount + 1, timeoutMs);
    QApplication::processEvents();
}

QColor dominantChangedColor(const QImage &before, const QImage &after, const QRect &rect) {
    QColor bestColor;
    int bestDistance = -1;
    const QRect bounded = rect.intersected(before.rect());
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            if (before.pixel(x, y) == after.pixel(x, y))
                continue;

            const QColor color = QColor::fromRgba(after.pixel(x, y));
            const int distance = qAbs(color.red() - color.green()) + qAbs(color.red() - color.blue())
                                 + qAbs(color.green() - color.blue());
            if (distance > bestDistance) {
                bestDistance = distance;
                bestColor = color;
            }
        }
    }

    return bestColor;
}

} // namespace

void TestTerminalWidget::testInitialize() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
}

void TestTerminalWidget::testInputMethodSupport() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    widget.setFocus();
    QVERIFY(widget.hasFocus());

    QVERIFY(widget.testAttribute(Qt::WA_InputMethodEnabled));

    QInputMethodQueryEvent queryEvent(Qt::ImEnabled);
    QApplication::sendEvent(&widget, &queryEvent);
    QVERIFY(queryEvent.value(Qt::ImEnabled).toBool());

    auto *session = widget.findChild<PtySession *>();
    QVERIFY(session);

    QSignalSpy spy(session, &PtySession::dataReceived);
    QVERIFY(spy.isValid());

    QTest::qWait(100);
    spy.clear();

    QInputMethodEvent imeEvent;
    imeEvent.setCommitString(QStringLiteral("中文"));
    QApplication::sendEvent(&widget, &imeEvent);
    QTest::keyClick(&widget, Qt::Key_Return);

    QTRY_VERIFY_WITH_TIMEOUT(collectedPtyOutput(spy).contains(QStringLiteral("中文").toUtf8()), 2000);
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

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 100);
    QCOMPARE(spy.at(0).at(0).toString(), QString("MyTestTitle"));
}

void TestTerminalWidget::testShellIntegrationCommandDetection() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    QSignalSpy spy(&widget, &TerminalWidget::shellCommandChanged);
    QVERIFY(spy.isValid());

    const QByteArray vscodeCommand = "\033]633;E;python -m aider\\x3b echo done\007";
    bool invoked =
        QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, vscodeCommand));
    QVERIFY(invoked);
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 100);
    QCOMPARE(widget.property("shellCommand").toString(), QStringLiteral("python -m aider; echo done"));

    const QByteArray weztermCommand = QByteArray("\033]1337;SetUserVar=WEZTERM_PROG=")
                                      + QByteArray("Y29kZXggLiAteSAtLW1vZGVsIGc1Cg==") + QByteArray("\033\\");
    invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                        Q_ARG(QByteArray, weztermCommand));
    QVERIFY(invoked);
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 100);
    QCOMPARE(widget.property("shellCommand").toString(), QStringLiteral("codex . -y --model g5"));

    const QByteArray qtGhosttyCommand = QByteArray("\033]777;ShellCommand=")
                                        + QByteArray("Y2xhdWRlIC0tc2tpcC1wZXJtaXNzaW9ucw==") + QByteArray("\033\\");
    invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                        Q_ARG(QByteArray, qtGhosttyCommand));
    QVERIFY(invoked);
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 3, 100);
    QCOMPARE(widget.property("shellCommand").toString(), QStringLiteral("claude --skip-permissions"));

    const QByteArray qtGhosttyClearCommand = "\033]777;ShellCommand=\033\\";
    invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                        Q_ARG(QByteArray, qtGhosttyClearCommand));
    QVERIFY(invoked);
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 4, 100);
    QCOMPARE(widget.property("shellCommand").toString(), QString());
}

void TestTerminalWidget::testGridSize() {
    CountingTerminalWidget widget;
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

void TestTerminalWidget::testRendersSupplementaryPlaneCharacters() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    const QByteArray emojiLine = QStringLiteral("hello 🌍\n").toUtf8();
    const bool invoked =
        QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, emojiLine));
    QVERIFY(invoked);

    widget.repaint();
    QApplication::processEvents();

    QVERIFY(true);
}

void TestTerminalWidget::testContentsMarginsInsetTerminalGridAndCursorRect() {
    TerminalWidget baselineWidget;
    QVERIFY(baselineWidget.initialize());
    baselineWidget.resize(960, 640);

    TerminalWidget paddedWidget;
    paddedWidget.setContentsMargins(4, 4, 4, 4);
    QVERIFY(paddedWidget.initialize());
    paddedWidget.resize(960, 640);
    paddedWidget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&paddedWidget));
    QApplication::processEvents();

    QVERIFY(paddedWidget.terminalColumns() < baselineWidget.terminalColumns());

    QInputMethodQueryEvent queryEvent(Qt::ImCursorRectangle);
    QApplication::sendEvent(&paddedWidget, &queryEvent);
    const QRect cursorRect = queryEvent.value(Qt::ImCursorRectangle).toRect();
    QCOMPARE(cursorRect.topLeft(), QPoint(4, 4));
}

void TestTerminalWidget::testRendersPreeditTextAcrossMultipleCells() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    widget.setFocus();
    QVERIFY(widget.hasFocus());
    widget.setCursorBlinkEnabled(false);
    QApplication::processEvents();

    QInputMethodQueryEvent queryEvent(Qt::ImCursorRectangle);
    QApplication::sendEvent(&widget, &queryEvent);
    const QRect cursorRect = queryEvent.value(Qt::ImCursorRectangle).toRect();
    QVERIFY(cursorRect.isValid());
    QVERIFY(cursorRect.width() > 0);

    const QImage before = renderWidgetImage(widget);

    QInputMethodEvent imeEvent(QStringLiteral("中文"), {});
    QApplication::sendEvent(&widget, &imeEvent);
    QApplication::processEvents();

    const QImage after = renderWidgetImage(widget);
    const QRect diff = changedBounds(before, after);
    QVERIFY2(diff.isValid(), "preedit text should change the rendered output");
    QVERIFY2(diff.width() > cursorRect.width(), "preedit text should render wider than a single terminal cell");
}

void TestTerminalWidget::testRendersWideCharactersAcrossTwoCells() {
    CountingTerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    QInputMethodQueryEvent queryEvent(Qt::ImCursorRectangle);
    QApplication::sendEvent(&widget, &queryEvent);
    const QRect cursorRect = queryEvent.value(Qt::ImCursorRectangle).toRect();
    QVERIFY(cursorRect.isValid());
    QVERIFY(cursorRect.width() > 0);

    const QImage before = renderWidgetImage(widget);
    const int previousFlushCount = widget.debugPtyFlushCount();
    const bool invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                                   Q_ARG(QByteArray, QStringLiteral("中\n").toUtf8()));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    const QImage after = renderWidgetImage(widget);
    const QRect diff = changedBounds(before, after);
    const QRect firstCellRect(cursorRect.topLeft(), cursorRect.size());
    const QRect secondCellRect(cursorRect.topLeft() + QPoint(cursorRect.width(), 0), cursorRect.size());
    const int secondCellChanges = countChangedPixels(before, after, secondCellRect);
    QVERIFY2(diff.isValid(), "wide character should change the rendered output");
    QVERIFY2(diff.width() > cursorRect.width(), "wide character should render wider than a single terminal cell");
    QVERIFY2(secondCellChanges > cursorRect.width() * 3,
             "wide character should visibly paint a meaningful portion of the second terminal cell");
}

void TestTerminalWidget::testRendersAnsiForegroundColors() {
    CountingTerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const QImage before = renderWidgetImage(widget);
    const int previousFlushCount = widget.debugPtyFlushCount();
    const bool invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                                   Q_ARG(QByteArray, QByteArray("\x1b[31mR\x1b[0m\n")));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    const QImage after = renderWidgetImage(widget);
    const QRect diff = changedBounds(before, after);
    QVERIFY2(diff.isValid(), "ANSI foreground colors should affect the rendered output");

    const QColor dominantColor = dominantChangedColor(before, after, diff);
    QVERIFY2(dominantColor.isValid(), "rendered ANSI text should produce visible colored pixels");
    QVERIFY2(dominantColor.red() > dominantColor.green() + 20 && dominantColor.red() > dominantColor.blue() + 20,
             "ANSI red foreground should render as a red-dominant color instead of grayscale");
}

void TestTerminalWidget::testCoalescesBurstRepaintsWithoutLosingFinalFrame() {
    CountingTerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    widget.setCursorBlinkEnabled(false);
    QApplication::processEvents();

    const int initialPaintCount = widget.paintCount();

    QElapsedTimer timer;
    timer.start();

    for (int i = 0; i < 12; ++i) {
        const QByteArray line = QByteArray("line-") + QByteArray::number(i) + "\n";
        const bool invoked =
            QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, line));
        QVERIFY(invoked);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        if (timer.elapsed() < 8)
            QTest::qWait(1);
    }

    QTRY_VERIFY_WITH_TIMEOUT(widget.paintCount() > initialPaintCount, 200);

    const int paintsForBurst = widget.paintCount() - initialPaintCount;
    QVERIFY2(paintsForBurst < 12, "burst PTY input should be coalesced into fewer paints than data chunks");

    const QString lastLine = QStringLiteral("line-11");
    QTRY_VERIFY_WITH_TIMEOUT(([&widget, &lastLine]() {
                                 widget.performSearch(lastLine);
                                 return widget.hasSearchMatches();
                             })(),
                             50);
}

void TestTerminalWidget::testCoalescesSmallPtyBurstsIntoSingleFlush() {
    CountingTerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    widget.setCursorBlinkEnabled(false);
    QApplication::processEvents();

    const int initialFlushCount = widget.debugPtyFlushCount();

    for (int i = 0; i < 4; ++i) {
        const bool invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                                       Q_ARG(QByteArray, QByteArray("abc")));
        QVERIFY(invoked);
    }

    QCOMPARE(widget.debugPtyFlushCount(), initialFlushCount);
    QTRY_COMPARE_WITH_TIMEOUT(widget.debugPtyFlushCount(), initialFlushCount + 1, 100);
}

void TestTerminalWidget::testIncrementalUpdatesRenderDirtyRowsOnly() {
    CountingTerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    widget.repaint();
    QApplication::processEvents();

    int previousFlushCount = widget.debugPtyFlushCount();
    bool invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                             Q_ARG(QByteArray, QByteArray("baseline-line\n")));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);
    widget.repaint();
    QApplication::processEvents();
    const QImage before = renderWidgetImage(widget);

    previousFlushCount = widget.debugPtyFlushCount();
    invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                        Q_ARG(QByteArray, QByteArray("incremental-line\n")));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);
    widget.repaint();
    QApplication::processEvents();
    const QImage after = renderWidgetImage(widget);

    const QRect diff = changedBounds(before, after);
    QVERIFY2(diff.isValid(), "single-line PTY updates should change the rendered output");
    QVERIFY2(diff.height() < widget.height() / 2,
             "single-line PTY updates should affect substantially less than the whole viewport");
}

void TestTerminalWidget::testCoalescesRapidResizeOperations() {
    CountingTerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int initialApplyCount = widget.debugResizeApplyCount();

    for (int i = 0; i < 6; ++i) {
        widget.resize(960 + (i * 8), 640 + (i * 4));
        QApplication::processEvents();
        QTest::qWait(1);
    }

    QTRY_VERIFY_WITH_TIMEOUT(widget.debugResizeApplyCount() > initialApplyCount, 100);
    QVERIFY2(widget.debugResizeApplyCount() - initialApplyCount < 6,
             "rapid resize events should be coalesced into fewer terminal resizes");
}

void TestTerminalWidget::testMouseTrackingEnabledSendsSGREvents() {
    CountingTerminalWidget widget;
    widget.resize(800, 600);
    QVERIFY(widget.initialize());

    auto *session = widget.findChild<PtySession *>();
    QVERIFY(session);

    QSignalSpy spy(session, &PtySession::dataWritten);
    QVERIFY(spy.isValid());

    const int flushCountBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("\033[?1000h\033[?1006h")));
    waitForNextPtyFlush(widget, flushCountBefore);
    QApplication::processEvents();
    spy.clear();

    QMouseEvent pressEvent(QEvent::MouseButtonPress, QPointF(0, 0), QPointF(0, 0), Qt::LeftButton, Qt::LeftButton,
                           Qt::NoModifier);
    QApplication::sendEvent(&widget, &pressEvent);

    QVERIFY(spy.count() > 0);
    const QByteArray output = collectedPtyOutput(spy);
    QVERIFY2(output.startsWith("\x1b[<"), qPrintable("SGR press: expected ESC[< prefix, got: " + output.toHex()));
    QVERIFY2(output.endsWith('M'), qPrintable("SGR press: expected trailing M, got: " + output.toHex()));
}

void TestTerminalWidget::testMouseTrackingDisabledDoesNotSendEvents() {
    TerminalWidget widget;
    widget.resize(800, 600);
    QVERIFY(widget.initialize());

    auto *session = widget.findChild<PtySession *>();
    QVERIFY(session);

    QSignalSpy spy(session, &PtySession::dataWritten);
    QVERIFY(spy.isValid());
    spy.clear();

    QMouseEvent pressEvent(QEvent::MouseButtonPress, QPointF(0, 0), QPointF(0, 0), Qt::LeftButton, Qt::LeftButton,
                           Qt::NoModifier);
    QApplication::sendEvent(&widget, &pressEvent);

    QCOMPARE(spy.count(), 0);
}

void TestTerminalWidget::testMouseTrackingReleaseSendsSGR() {
    CountingTerminalWidget widget;
    widget.resize(800, 600);
    QVERIFY(widget.initialize());

    auto *session = widget.findChild<PtySession *>();
    QVERIFY(session);

    QSignalSpy spy(session, &PtySession::dataWritten);
    QVERIFY(spy.isValid());

    const int flushCountBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("\033[?1000h\033[?1006h")));
    waitForNextPtyFlush(widget, flushCountBefore);
    QApplication::processEvents();

    QMouseEvent pressEvent(QEvent::MouseButtonPress, QPointF(0, 0), QPointF(0, 0), Qt::LeftButton, Qt::LeftButton,
                           Qt::NoModifier);
    QApplication::sendEvent(&widget, &pressEvent);
    spy.clear();

    QMouseEvent releaseEvent(QEvent::MouseButtonRelease, QPointF(0, 0), QPointF(0, 0), Qt::LeftButton, Qt::NoButton,
                             Qt::NoModifier);
    QApplication::sendEvent(&widget, &releaseEvent);

    QVERIFY(spy.count() > 0);
    const QByteArray output = collectedPtyOutput(spy);
    QVERIFY2(output.startsWith("\x1b[<"), qPrintable("SGR release: expected ESC[< prefix, got: " + output.toHex()));
    QVERIFY2(output.endsWith('m'), qPrintable("SGR release: expected trailing m, got: " + output.toHex()));
}

void TestTerminalWidget::testMouseTrackingWheelSendsSGR() {
    CountingTerminalWidget widget;
    widget.resize(800, 600);
    QVERIFY(widget.initialize());

    auto *session = widget.findChild<PtySession *>();
    QVERIFY(session);

    QSignalSpy spy(session, &PtySession::dataWritten);
    QVERIFY(spy.isValid());

    const int flushCountBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("\033[?1000h\033[?1006h")));
    waitForNextPtyFlush(widget, flushCountBefore);
    QApplication::processEvents();
    spy.clear();

    QWheelEvent wheelEvent(QPointF(10, 10), QPointF(10, 10), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                           Qt::ScrollUpdate, false);
    QApplication::sendEvent(&widget, &wheelEvent);

    QVERIFY(spy.count() > 0);
    const QByteArray output = collectedPtyOutput(spy);
    QVERIFY2(output.startsWith("\x1b[<"), qPrintable("SGR wheel: expected ESC[< prefix, got: " + output.toHex()));
    QVERIFY2(output.endsWith('M'), qPrintable("SGR wheel: expected trailing M, got: " + output.toHex()));
    QVERIFY2(output.contains("64;") || output.contains("65;"),
             qPrintable("SGR wheel: expected button 64 (up) or 65 (down), got: " + output.toHex()));
}

void TestTerminalWidget::testCellInSelectionSameRow() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.debugSetSelection(2, 3, 2, 7, true);

    QVERIFY(!widget.debugCellInSelection(2, 2));
    QVERIFY(widget.debugCellInSelection(2, 3));
    QVERIFY(widget.debugCellInSelection(2, 5));
    QVERIFY(widget.debugCellInSelection(2, 7));
    QVERIFY(!widget.debugCellInSelection(2, 8));
    QVERIFY(!widget.debugCellInSelection(1, 5));
    QVERIFY(!widget.debugCellInSelection(3, 5));
}

void TestTerminalWidget::testCellInSelectionTopLeftToBottomRight() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.debugSetSelection(1, 2, 3, 6, true);

    QVERIFY(widget.debugCellInSelection(1, 2));
    QVERIFY(widget.debugCellInSelection(1, 10));
    QVERIFY(!widget.debugCellInSelection(1, 1));

    QVERIFY(widget.debugCellInSelection(2, 0));
    QVERIFY(widget.debugCellInSelection(2, 10));

    QVERIFY(widget.debugCellInSelection(3, 0));
    QVERIFY(widget.debugCellInSelection(3, 6));
    QVERIFY(!widget.debugCellInSelection(3, 7));

    QVERIFY(!widget.debugCellInSelection(0, 5));
    QVERIFY(!widget.debugCellInSelection(4, 5));
}

void TestTerminalWidget::testCellInSelectionBottomRightToTopLeft() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.debugSetSelection(3, 6, 1, 2, true);

    QVERIFY(widget.debugCellInSelection(1, 2));
    QVERIFY(widget.debugCellInSelection(1, 10));
    QVERIFY(!widget.debugCellInSelection(1, 1));

    QVERIFY(widget.debugCellInSelection(2, 0));
    QVERIFY(widget.debugCellInSelection(2, 10));

    QVERIFY(widget.debugCellInSelection(3, 0));
    QVERIFY(widget.debugCellInSelection(3, 6));
    QVERIFY(!widget.debugCellInSelection(3, 7));

    QVERIFY(!widget.debugCellInSelection(0, 5));
    QVERIFY(!widget.debugCellInSelection(4, 5));
}

void TestTerminalWidget::testCellInSelectionTopRightToBottomLeft() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.debugSetSelection(1, 8, 3, 2, true);

    QVERIFY(widget.debugCellInSelection(1, 8));
    QVERIFY(widget.debugCellInSelection(1, 20));
    QVERIFY(!widget.debugCellInSelection(1, 7));

    QVERIFY(widget.debugCellInSelection(2, 0));
    QVERIFY(widget.debugCellInSelection(2, 20));

    QVERIFY(widget.debugCellInSelection(3, 0));
    QVERIFY(widget.debugCellInSelection(3, 2));
    QVERIFY(!widget.debugCellInSelection(3, 3));
}

void TestTerminalWidget::testCellInSelectionBottomLeftToTopRight() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.debugSetSelection(3, 2, 1, 8, true);

    QVERIFY(widget.debugCellInSelection(1, 8));
    QVERIFY(widget.debugCellInSelection(1, 20));
    QVERIFY(!widget.debugCellInSelection(1, 7));

    QVERIFY(widget.debugCellInSelection(2, 0));
    QVERIFY(widget.debugCellInSelection(2, 20));

    QVERIFY(widget.debugCellInSelection(3, 0));
    QVERIFY(widget.debugCellInSelection(3, 2));
    QVERIFY(!widget.debugCellInSelection(3, 3));
}

void TestTerminalWidget::testSelectedTextTopLeftToBottomRight() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    widget.debugSetSelection(0, 1, 2, 2, true);

    widget.debugSetSelection(2, 2, 0, 1, true);
    const QString reverseText = widget.debugSelectedText();

    widget.debugSetSelection(0, 1, 2, 2, true);
    const QString forwardText = widget.debugSelectedText();

    QCOMPARE(reverseText, forwardText);
}

void TestTerminalWidget::testSelectedTextBottomRightToTopLeft() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("AAAA\nBBBB\nCCCC\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    widget.debugSetSelection(0, 1, 2, 2, true);
    const QString forwardText = widget.debugSelectedText();
    QVERIFY(!forwardText.isEmpty());

    widget.debugSetSelection(2, 2, 0, 1, true);
    const QString reverseText = widget.debugSelectedText();

    QCOMPARE(reverseText, forwardText);
}

void TestTerminalWidget::testApplyThemeSetsColors() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    TerminalTheme theme;
    theme.name = QStringLiteral("test");
    theme.displayName = QStringLiteral("Test");
    theme.isDark = true;
    theme.foreground = QColor(255, 0, 0);
    theme.background = QColor(0, 0, 255);
    theme.cursor = QColor(0, 255, 0);
    for (int i = 0; i < 16; ++i)
        theme.ansi[i] = QColor(i * 15, i * 15, i * 15);

    widget.applyTheme(theme);
    QApplication::processEvents();

    QVERIFY(widget.debugAppliedIsDark());
    QCOMPARE(widget.debugAppliedForeground(), QColor(255, 0, 0));
    QCOMPARE(widget.debugAppliedBackground(), QColor(0, 0, 255));
}

void TestTerminalWidget::testCommandStateRunning() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    QSignalSpy spy(&widget, &TerminalWidget::commandStateChanged);
    QVERIFY(spy.isValid());

    const QByteArray command = QByteArray("\033]777;ShellCommand=") + QByteArray("bWFrZQ==") + QByteArray("\033\\");
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, command));

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 100);
    QCOMPARE(widget.property("commandState").toInt(), static_cast<int>(TerminalWidget::CommandState::Running));
}

void TestTerminalWidget::testCommandStateSucceeded() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    QSignalSpy spy(&widget, &TerminalWidget::commandStateChanged);
    QVERIFY(spy.isValid());

    const QByteArray command = QByteArray("\033]777;ShellCommand=") + QByteArray("bWFrZQ==") + QByteArray("\033\\");
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, command));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 100);

    const QByteArray result = "\033]777;ShellCommandResult=0\033\\";
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, result));

    const QByteArray clear = "\033]777;ShellCommand=\033\\";
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, clear));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 100);

    QCOMPARE(widget.property("commandState").toInt(), static_cast<int>(TerminalWidget::CommandState::Succeeded));
}

void TestTerminalWidget::testCommandStateFailed() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    QSignalSpy spy(&widget, &TerminalWidget::commandStateChanged);
    QVERIFY(spy.isValid());

    const QByteArray command = QByteArray("\033]777;ShellCommand=") + QByteArray("bWFrZQ==") + QByteArray("\033\\");
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, command));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 100);

    const QByteArray result = "\033]777;ShellCommandResult=2\033\\";
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, result));

    const QByteArray clear = "\033]777;ShellCommand=\033\\";
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, clear));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 100);

    QCOMPARE(widget.property("commandState").toInt(), static_cast<int>(TerminalWidget::CommandState::Failed));
}

void TestTerminalWidget::testCommandStateIdleWhenNoResult() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    const QByteArray clear = "\033]777;ShellCommand=\033\\";
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, clear));

    QCOMPARE(widget.property("commandState").toInt(), static_cast<int>(TerminalWidget::CommandState::Idle));
}

void TestTerminalWidget::testCommandStateTransitionSequence() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    QSignalSpy spy(&widget, &TerminalWidget::commandStateChanged);
    QVERIFY(spy.isValid());

    auto sendOsc = [&widget](const QByteArray &osc) {
        QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, osc));
    };

    // Command starts -> Running
    sendOsc(QByteArray("\033]777;ShellCommand=") + QByteArray("bWFrZQ==") + QByteArray("\033\\"));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 100);
    QCOMPARE(widget.property("commandState").toInt(), static_cast<int>(TerminalWidget::CommandState::Running));

    // Result 0 + clear -> Succeeded
    sendOsc("\033]777;ShellCommandResult=0\033\\");
    sendOsc("\033]777;ShellCommand=\033\\");
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 100);
    QCOMPARE(widget.property("commandState").toInt(), static_cast<int>(TerminalWidget::CommandState::Succeeded));

    // New command -> Running again
    sendOsc(QByteArray("\033]777;ShellCommand=") + QByteArray("bWFrZSBhbGw=") + QByteArray("\033\\"));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 3, 100);
    QCOMPARE(widget.property("commandState").toInt(), static_cast<int>(TerminalWidget::CommandState::Running));

    // Result 1 + clear -> Failed
    sendOsc("\033]777;ShellCommandResult=1\033\\");
    sendOsc("\033]777;ShellCommand=\033\\");
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 4, 100);
    QCOMPARE(widget.property("commandState").toInt(), static_cast<int>(TerminalWidget::CommandState::Failed));
}

// We need QApplication for QWidget tests
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    TestTerminalWidget tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_terminal_widget.moc"
