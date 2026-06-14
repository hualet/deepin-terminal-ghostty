#include "TerminalTheme.h"

#include <QApplication>
#include <QClipboard>
#include <QElapsedTimer>
#include <QEvent>
#include <QImage>
#include <QInputMethodEvent>
#include <QInputMethodQueryEvent>
#include <QMimeData>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>
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
    void testRendersLongGraphemeCells();
    void testRendersPreeditTextAcrossMultipleCells();
    void testFocusOutClearsPreeditText();
    void testRendersWideCharactersAcrossTwoCells();
    void testEmojiFallbackRendererCanBeForced();
    void testEmojiFallbackRendererShapesMultiCodepointEmoji();
    void testEmojiFallbackRendererShapesKeycapEmoji();
    void testEmojiFallbackRendererLeavesPlainDigitsAsText();
    void testEmojiFallbackRendererSizesVariationEmoji();
    void testEmojiFallbackRendererKeepsNarrowEmojiInOneCell();
    void testEmojiFallbackClusterPreservesBackgroundColor();
    void testEmojiFallbackClusterPreservesTextDecorations();
    void testSetTerminalFontInvalidatesEmojiModeDetection();
    void testFallbackGlyphDoesNotOverlapNextCell();
    void testSingleCodepointFallbackGlyphDoesNotClip();
    void testRendersAnsiForegroundColors();
    void testRendersInverseTextWithDefaultColors();
    void testRendersInlineKittyPngImage();
    void testRendersTextDecorations();
    void testStyledTextKeepsCharactersOnCellGrid();
    void testConcealedTextDoesNotRenderGlyphs();
    void testRendersBoxDrawingCharactersWithoutTextShaping();
    void testCoalescesPlainTextIntoRenderRuns();
    void testCursorOnlyUpdatesUseNarrowRepaint();
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

    void testDoubleClickSelectsWord();
    void testDoubleClickSelectsWordWithDots();
    void testDoubleClickSelectsWrappedFilename();
    void testDoubleClickSelectsWordWithHyphens();
    void testDoubleClickSelectsWordWithUnderscores();
    void testDoubleClickSelectsWordWithMixedPunctuation();
    void testDoubleClickStopsAtParentheses();
    void testDoubleClickStopsAtColon();
    void testDoubleClickStopsAtAmpersand();
    void testDoubleClickStopsAtPlus();
    void testDoubleClickStopsAtHash();
    void testDoubleClickStopsAtExclamation();
    void testDoubleClickStopsAtGlob();
    void testDoubleClickSelectsPath();
    void testSingleClickDoesNotSelectCell();
    void testDoubleClickSetsSelectionClipboard();
    void testTripleClickSetsSelectionClipboard();
    void testTripleClickSelectsLine();
    void testDoubleClickDragExtendsByWord();
    void testTripleClickDragExtendsByLine();
    void testSelectionDragAboveViewportAutoScrollsUp();
    void testSelectionDragBelowViewportAutoScrollsDown();

    void testZoomInIncreasesFontSize();
    void testZoomOutDecreasesFontSize();
    void testZoomInClampsAtMaximum();
    void testZoomOutClampsAtMinimum();
    void testSetScrollbackLines();
    void testScrollbackLineCountMapsToByteBudget();
    void testViewportScrollStateAndAbsoluteScroll();
    void testOutputDoesNotFollowBottomWhenViewportScrolledBack();
    void testOutputDoesNotRepaintScrolledBackViewportContent();
    void testContinuousOutputDoesNotMoveScrolledBackViewportContent();
    void testContinuousOutputDoesNotFollowBottomWhenScrollbackPrunes();
    void testOutputDoesNotFollowBottomAfterMouseWheelScrollback();
    void testPendingOutputDoesNotFollowBottomAfterMouseWheelScrollback();
    void testKeyInputFollowsBottomWhenViewportScrolledBack();
    void testInputMethodCommitFollowsBottomWhenViewportScrolledBack();
    void testPasteFollowsBottomWhenViewportScrolledBack();
    void testSetOpacity();
    void testSetOpacityRepaintsCachedBackground();
    void testSetOpacityFullDisablesTranslucentBackground();
    void testSetOpacityPartialEnablesTranslucentBackground();
    void testHasRunningProcessReturnsFalseForShell();
    void testSelectAllCreatesSelection();
    void testSelectAllThenCopyToClipboard();
    void testPasteFromClipboardSendsToPty();
    void testDropLocalFileWritesEscapedPathToPty();
    void testOsc52WritesClipboard();
    void testOsc52WritesClipboardAcrossChunks();
    void testOsc52ReadRequestDoesNotChangeClipboard();
    void testSearchFindsMatchInTerminalContent();
    void testSearchNoMatch();
    void testSearchEmptyQueryClears();
    void testClearSearch();
    void testFindNextCycles();
    void testFindPreviousCycles();
    void testFindNextOnEmptyMatchesIsNoop();
    void testFindPreviousOnEmptyMatchesIsNoop();
    void testImportVtContentDropsPendingPtyOutput();
    void testImportVtContentClearsStaleShellIntegrationState();
    void testImportVtContentKeepsFuturePtyOutputAfterRestoredScreen();

    void testBareLinkUriAtPositionDetectsHttp();
    void testHyperlinkUriAtPositionIgnoresBareLink();
    void testBareLinkCtrlClickActivatesLink();
    void testBareLinkUriAtPositionDetectsSupportedSchemes_data();
    void testBareLinkUriAtPositionDetectsSupportedSchemes();
    void testBareLinkTrimsTrailingPunctuationAndKeepsBalancedParentheses();
    void testBareLinkScanCacheReusesRowsAndPaintDoesNotScan();
    void testBareLinkHoverWorksWithoutMouseButton();
    void testBareLinkCachedRowDoesNotRefetchText();
    void testBareLinkHoverClearsOnScroll();
    void testSelectionDragAcrossBareLinkSelectsText();
    void testLinkUriAtPositionPrefersOsc8OverBareText();
    void testHyperlinkHoverDetection();
    void testHyperlinkCtrlClick();
    void testHyperlinkCursorChange();
    void testHyperlinkLeaveEvent();
    void testHyperlinkUnderlinePixels();
    void testHyperlinkHoverWithMouseTracking();
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

class RecordingTerminalWidget : public CountingTerminalWidget {
    Q_OBJECT

public:
    using CountingTerminalWidget::CountingTerminalWidget;

    QString capturedSelectionClipboardText() const { return m_capturedSelectionClipboardText; }

protected:
    void setSelectionClipboardText(const QString &text) override { m_capturedSelectionClipboardText = text; }

private:
    QString m_capturedSelectionClipboardText;
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

void feedTerminalOutput(CountingTerminalWidget &widget, const QByteArray &data) {
    const int previousFlushCount = widget.debugPtyFlushCount();
    const bool invoked =
        QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, data));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);
}

void populateAndScrollBack(CountingTerminalWidget &widget) {
    widget.resize(240, 80);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    widget.setFocus();
    QApplication::processEvents();

    feedTerminalOutput(widget, QByteArray("one\ntwo\nthree\nfour\nfive\nsix\n"));

    const auto bottomState = widget.viewportScrollState();
    QVERIFY(bottomState.totalRows > bottomState.visibleRows);
    QCOMPARE(bottomState.offset, bottomState.maximumOffset());

    widget.scrollViewportToOffset(0);
    QCOMPARE(widget.viewportScrollState().offset, 0);
}

int firstScreenRowContaining(const CountingTerminalWidget &widget, QStringView text) {
    for (int row = 0; row < 200; ++row) {
        if (widget.debugTextForScreenRow(row).contains(text))
            return row;
    }

    return -1;
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

int countRedPixels(const QImage &image, const QRect &rect) {
    int count = 0;
    const QRect bounded = rect.intersected(image.rect());
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            const QColor color = QColor::fromRgba(image.pixel(x, y));
            if (color.red() > 180 && color.green() < 80 && color.blue() < 80 && color.alpha() > 180)
                ++count;
        }
    }

    return count;
}

int countPixelsNear(const QImage &image, const QRect &rect, const QColor &target, int tolerance) {
    int count = 0;
    const QRect bounded = rect.intersected(image.rect());
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            const QColor color = QColor::fromRgba(image.pixel(x, y));
            if (qAbs(color.red() - target.red()) <= tolerance && qAbs(color.green() - target.green()) <= tolerance
                && qAbs(color.blue() - target.blue()) <= tolerance && color.alpha() > 180) {
                ++count;
            }
        }
    }

    return count;
}

int firstPaintedColumnInCell(const QImage &image, const QRect &cellRect, const QColor &background) {
    const QRect bounded = cellRect.intersected(image.rect());
    for (int x = bounded.left(); x <= bounded.right(); ++x) {
        for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
            const QColor pixel = QColor::fromRgba(image.pixel(x, y));
            if (qAbs(pixel.red() - background.red()) > 8 || qAbs(pixel.green() - background.green()) > 8
                || qAbs(pixel.blue() - background.blue()) > 8) {
                return x;
            }
        }
    }

    return -1;
}

int lastPaintedColumn(const QImage &image, const QRect &rect, const QColor &background) {
    const QRect bounded = rect.intersected(image.rect());
    for (int x = bounded.right(); x >= bounded.left(); --x) {
        for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
            const QColor pixel = QColor::fromRgba(image.pixel(x, y));
            if (qAbs(pixel.red() - background.red()) > 8 || qAbs(pixel.green() - background.green()) > 8
                || qAbs(pixel.blue() - background.blue()) > 8) {
                return x;
            }
        }
    }

    return -1;
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

void TestTerminalWidget::testRendersLongGraphemeCells() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    QString longGrapheme = QStringLiteral("a");
    for (int i = 0; i < 32; ++i)
        longGrapheme.append(QChar(0x0301));
    longGrapheme.append(QChar(u'\n'));

    const bool invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                                   Q_ARG(QByteArray, longGrapheme.toUtf8()));
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

void TestTerminalWidget::testFocusOutClearsPreeditText() {
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

    QFocusEvent focusOut(QEvent::FocusOut, Qt::OtherFocusReason);
    QApplication::sendEvent(&widget, &focusOut);
    QApplication::processEvents();

    const QImage afterFocusOut = renderWidgetImage(widget);
    const QRect diff = changedBounds(before, afterFocusOut);
    QVERIFY2(!diff.isValid() || diff.width() <= cursorRect.width(),
             "focus loss should remove preedit text instead of leaving multi-cell composition visible");
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

void TestTerminalWidget::testEmojiFallbackRendererCanBeForced() {
    CountingTerminalWidget widget;
    widget.debugSetEmojiRenderModeForTesting(TerminalWidget::EmojiRenderMode::CustomFallback);
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    int previousFlushCount = widget.debugPtyFlushCount();
    bool invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                             Q_ARG(QByteArray, QByteArray("\033[?25l")));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    QInputMethodQueryEvent queryEvent(Qt::ImCursorRectangle);
    QApplication::sendEvent(&widget, &queryEvent);
    const QRect cursorRect = queryEvent.value(Qt::ImCursorRectangle).toRect();
    QVERIFY(cursorRect.isValid());
    QVERIFY(cursorRect.width() > 0);

    const QImage before = renderWidgetImage(widget);
    previousFlushCount = widget.debugPtyFlushCount();
    invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                        Q_ARG(QByteArray, QStringLiteral("⚠\n").toUtf8()));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    const QImage after = renderWidgetImage(widget);
    const QRect firstCellRect(cursorRect.topLeft(), cursorRect.size());
    QVERIFY2(widget.debugLastFrameEmojiFallbackDrawCount() > 0, "forced fallback mode should use emoji fallback draws");
    QVERIFY2(countChangedPixels(before, after, firstCellRect) > 0, "emoji fallback should paint the emoji cell");
}

void TestTerminalWidget::testEmojiFallbackRendererShapesMultiCodepointEmoji() {
    CountingTerminalWidget widget;
    widget.debugSetEmojiRenderModeForTesting(TerminalWidget::EmojiRenderMode::CustomFallback);
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    int previousFlushCount = widget.debugPtyFlushCount();
    bool invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                             Q_ARG(QByteArray, QByteArray("\033[?25l")));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    QInputMethodQueryEvent queryEvent(Qt::ImCursorRectangle);
    QApplication::sendEvent(&widget, &queryEvent);
    const QRect cursorRect = queryEvent.value(Qt::ImCursorRectangle).toRect();
    QVERIFY(cursorRect.isValid());
    QVERIFY(cursorRect.width() > 0);

    const QImage before = renderWidgetImage(widget);
    previousFlushCount = widget.debugPtyFlushCount();
    invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                        Q_ARG(QByteArray, QStringLiteral("🇨🇳\n").toUtf8()));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    const QImage after = renderWidgetImage(widget);
    const QRect firstCellRect(cursorRect.topLeft(), QSize(cursorRect.width() * 2, cursorRect.height()));
    QCOMPARE(widget.debugLastFrameEmojiFallbackDrawCount(), 1);
    QVERIFY2(countChangedPixels(before, after, firstCellRect) > 0, "emoji fallback should paint the shaped emoji cell");
}

void TestTerminalWidget::testEmojiFallbackRendererShapesKeycapEmoji() {
    CountingTerminalWidget widget;
    widget.debugSetEmojiRenderModeForTesting(TerminalWidget::EmojiRenderMode::CustomFallback);
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    int previousFlushCount = widget.debugPtyFlushCount();
    bool invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                             Q_ARG(QByteArray, QByteArray("\033[?25l")));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    QInputMethodQueryEvent queryEvent(Qt::ImCursorRectangle);
    QApplication::sendEvent(&widget, &queryEvent);
    const QRect cursorRect = queryEvent.value(Qt::ImCursorRectangle).toRect();
    QVERIFY(cursorRect.isValid());

    const QImage before = renderWidgetImage(widget);
    previousFlushCount = widget.debugPtyFlushCount();
    invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                        Q_ARG(QByteArray, QStringLiteral("1️⃣\n").toUtf8()));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    const QImage after = renderWidgetImage(widget);
    const QRect firstCellRect(cursorRect.topLeft(), QSize(cursorRect.width() * 2, cursorRect.height()));
    QVERIFY2(widget.debugLastFrameEmojiFallbackDrawCount() > 0, "keycap emoji should use the fallback renderer");
    QVERIFY2(countChangedPixels(before, after, firstCellRect) > 0, "keycap emoji fallback should paint the cell");
}

void TestTerminalWidget::testEmojiFallbackRendererLeavesPlainDigitsAsText() {
    PtySession::StartOptions options;
    options.command = QStringLiteral("sleep 5");

    CountingTerminalWidget widget;
    widget.setStartOptions(options);
    widget.debugSetEmojiRenderModeForTesting(TerminalWidget::EmojiRenderMode::CustomFallback);
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    int previousFlushCount = widget.debugPtyFlushCount();
    bool invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                             Q_ARG(QByteArray, QByteArray("\033[?25l")));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    previousFlushCount = widget.debugPtyFlushCount();
    invoked = QMetaObject::invokeMethod(
        &widget, "onPtyDataReceived", Qt::DirectConnection,
        Q_ARG(QByteArray, QStringLiteral("K2.7 Code is ready higher end-to-end coding task success rates\n").toUtf8()));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    renderWidgetImage(widget);
    QCOMPARE(widget.visibleText().trimmed(),
             QStringLiteral("K2.7 Code is ready higher end-to-end coding task success rates"));
    QCOMPARE(widget.debugLastFrameEmojiFallbackDrawCount(), 0);
}

void TestTerminalWidget::testEmojiFallbackRendererSizesVariationEmoji() {
    CountingTerminalWidget widget;
    widget.debugSetEmojiRenderModeForTesting(TerminalWidget::EmojiRenderMode::CustomFallback);
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    int previousFlushCount = widget.debugPtyFlushCount();
    bool invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                             Q_ARG(QByteArray, QByteArray("\033[?25l")));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    QInputMethodQueryEvent queryEvent(Qt::ImCursorRectangle);
    QApplication::sendEvent(&widget, &queryEvent);
    const QRect cursorRect = queryEvent.value(Qt::ImCursorRectangle).toRect();
    QVERIFY(cursorRect.isValid());

    const QImage before = renderWidgetImage(widget);
    previousFlushCount = widget.debugPtyFlushCount();
    invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                        Q_ARG(QByteArray, QStringLiteral("✌️\n").toUtf8()));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    const QImage after = renderWidgetImage(widget);
    QVERIFY2(widget.debugLastFrameEmojiFallbackDrawCount() > 0, "variation emoji should use the fallback renderer");
    QVERIFY2(countChangedPixels(before, after, cursorRect) > 0,
             "variation emoji fallback should paint its terminal cell");
    QVERIFY2(!widget.visibleText().contains(QStringLiteral("<fe0f>"), Qt::CaseInsensitive),
             "variation selector placeholders should not leak into visible terminal text");
}

void TestTerminalWidget::testEmojiFallbackRendererKeepsNarrowEmojiInOneCell() {
    PtySession::StartOptions options;
    options.command = QStringLiteral("sleep 5");

    CountingTerminalWidget widget;
    widget.setStartOptions(options);
    widget.debugSetEmojiRenderModeForTesting(TerminalWidget::EmojiRenderMode::CustomFallback);
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    int previousFlushCount = widget.debugPtyFlushCount();
    bool invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                             Q_ARG(QByteArray, QByteArray("\033[?25l")));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    QInputMethodQueryEvent queryEvent(Qt::ImCursorRectangle);
    QApplication::sendEvent(&widget, &queryEvent);
    const QRect cursorRect = queryEvent.value(Qt::ImCursorRectangle).toRect();
    QVERIFY(cursorRect.isValid());

    const QImage before = renderWidgetImage(widget);
    previousFlushCount = widget.debugPtyFlushCount();
    invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                        Q_ARG(QByteArray, QStringLiteral("©️\n").toUtf8()));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    const QImage after = renderWidgetImage(widget);
    const QRect secondCellRect(cursorRect.topLeft() + QPoint(cursorRect.width(), 0), cursorRect.size());
    QVERIFY2(widget.debugLastFrameEmojiFallbackDrawCount() > 0, "copyright sign should use emoji fallback");
    QCOMPARE(countChangedPixels(before, after, secondCellRect), 0);
}

void TestTerminalWidget::testEmojiFallbackClusterPreservesBackgroundColor() {
    PtySession::StartOptions options;
    options.command = QStringLiteral("sleep 5");

    CountingTerminalWidget widget;
    widget.setStartOptions(options);
    widget.debugSetEmojiRenderModeForTesting(TerminalWidget::EmojiRenderMode::CustomFallback);
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    feedTerminalOutput(widget, QByteArray("\033[?25l"));
    feedTerminalOutput(widget, QStringLiteral("\033[48;2;40;90;160m👨‍👩‍👧‍👦\033[0m\n").toUtf8());

    const QImage frame = renderWidgetImage(widget);
    const QFontMetrics fm(widget.terminalFont());
    const QRect firstTwoCells(0, 0, fm.horizontalAdvance('M') * 2, fm.height());
    const QColor customBackground(40, 90, 160);
    QVERIFY2(countPixelsNear(frame, firstTwoCells, customBackground, 6) > firstTwoCells.width(),
             "emoji cluster overlay should preserve the cell background it clears before repainting");
}

void TestTerminalWidget::testEmojiFallbackClusterPreservesTextDecorations() {
    PtySession::StartOptions options;
    options.command = QStringLiteral("sleep 5");

    CountingTerminalWidget widget;
    widget.setStartOptions(options);
    widget.debugSetEmojiRenderModeForTesting(TerminalWidget::EmojiRenderMode::CustomFallback);
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    feedTerminalOutput(widget, QByteArray("\033[?25l"));
    feedTerminalOutput(widget, QStringLiteral("\033[4;58;2;255;0;0m👨‍👩‍👧‍👦\033[0m\n").toUtf8());

    const QImage frame = renderWidgetImage(widget);
    const QFontMetrics fm(widget.terminalFont());
    const QRect underlineBand(0, qMin(fm.height() - 1, fm.ascent() + 1), fm.horizontalAdvance('M') * 2, 3);
    QVERIFY2(countPixelsNear(frame, underlineBand, QColor(255, 0, 0), 12) > 0,
             "emoji cluster overlay should redraw underline decorations after clearing cells");
}

void TestTerminalWidget::testSetTerminalFontInvalidatesEmojiModeDetection() {
    CountingTerminalWidget widget;
    QVERIFY(widget.initialize());
    QVERIFY(!widget.debugHasDetectedEmojiRenderMode());

    (void)widget.debugEmojiRenderMode();
    QVERIFY(widget.debugHasDetectedEmojiRenderMode());

    QFont font = widget.terminalFont();
    font.setPointSize(font.pointSize() + 1);
    widget.setTerminalFont(font);
    QVERIFY(!widget.debugHasDetectedEmojiRenderMode());
}

void TestTerminalWidget::testFallbackGlyphDoesNotOverlapNextCell() {
    PtySession::StartOptions options;
    options.command = QStringLiteral("sleep 5");

    CountingTerminalWidget widget;
    widget.setStartOptions(options);
    widget.debugSetEmojiRenderModeForTesting(TerminalWidget::EmojiRenderMode::QtNative);
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    int previousFlushCount = widget.debugPtyFlushCount();
    bool invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                             Q_ARG(QByteArray, QByteArray("\033[?25l")));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    QInputMethodQueryEvent queryEvent(Qt::ImCursorRectangle);
    QApplication::sendEvent(&widget, &queryEvent);
    const QRect cursorRect = queryEvent.value(Qt::ImCursorRectangle).toRect();
    QVERIFY(cursorRect.isValid());
    QVERIFY(cursorRect.width() > 0);

    const QImage before = renderWidgetImage(widget);
    previousFlushCount = widget.debugPtyFlushCount();
    const QByteArray output = QStringLiteral("⚠️\n").toUtf8();
    invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, output));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    const QImage after = renderWidgetImage(widget);
    const QRect firstCellRect(cursorRect.topLeft(), cursorRect.size());
    const QRect nextCellRect(cursorRect.topLeft() + QPoint(cursorRect.width(), 0), cursorRect.size());
    QCOMPARE(countChangedPixels(before, after, nextCellRect), 0);
}

void TestTerminalWidget::testSingleCodepointFallbackGlyphDoesNotClip() {
    PtySession::StartOptions options;
    options.command = QStringLiteral("sleep 5");

    CountingTerminalWidget widget;
    widget.setStartOptions(options);
    widget.debugSetEmojiRenderModeForTesting(TerminalWidget::EmojiRenderMode::QtNative);
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    int previousFlushCount = widget.debugPtyFlushCount();
    bool invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                             Q_ARG(QByteArray, QByteArray("\033[?25l")));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    QInputMethodQueryEvent queryEvent(Qt::ImCursorRectangle);
    QApplication::sendEvent(&widget, &queryEvent);
    const QRect cursorRect = queryEvent.value(Qt::ImCursorRectangle).toRect();
    QVERIFY(cursorRect.isValid());
    QVERIFY(cursorRect.width() > 0);

    const QImage before = renderWidgetImage(widget);
    previousFlushCount = widget.debugPtyFlushCount();
    const QByteArray output = QStringLiteral("⚠\n").toUtf8();
    invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, output));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    const QImage after = renderWidgetImage(widget);
    const QRect firstCellRect(cursorRect.topLeft(), cursorRect.size());
    const QRect nextCellRect(cursorRect.topLeft() + QPoint(cursorRect.width(), 0), cursorRect.size());
    const QRect rightEdgeRect(firstCellRect.right(), firstCellRect.top(), 1, firstCellRect.height());
    QVERIFY2(countChangedPixels(before, after, rightEdgeRect) <= 1,
             "single-codepoint fallback glyph should be fitted inside the cell instead of being cut");
    QCOMPARE(countChangedPixels(before, after, nextCellRect), 0);
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

void TestTerminalWidget::testRendersInverseTextWithDefaultColors() {
    CountingTerminalWidget widget;
    QVERIFY(widget.initialize());

    TerminalTheme theme;
    theme.name = QStringLiteral("inverse-test");
    theme.displayName = QStringLiteral("Inverse Test");
    theme.isDark = true;
    theme.foreground = QColor(240, 240, 240);
    theme.background = QColor(5, 5, 5);
    theme.cursor = QColor(240, 240, 240);
    for (QColor &color : theme.ansi)
        color = QColor(128, 128, 128);
    widget.applyTheme(theme);

    widget.resize(240, 120);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    feedTerminalOutput(widget, QByteArray("\x1b[7mX\x1b[0m"));
    const QImage frame = renderWidgetImage(widget);

    const QFontMetrics fm(widget.terminalFont());
    const QRect firstCell(0, 0, fm.horizontalAdvance('M'), fm.height());
    QVERIFY2(countPixelsNear(frame, firstCell, theme.foreground, 12) > firstCell.width() * firstCell.height() / 2,
             "inverse cell should paint the default foreground as the background");
    QVERIFY2(countPixelsNear(frame, firstCell, theme.background, 24) > 0,
             "inverse text without an explicit foreground should use the default background color");
}

void TestTerminalWidget::testRendersInlineKittyPngImage() {
    CountingTerminalWidget widget;
    widget.resize(640, 400);
    QVERIFY(widget.initialize());

    static constexpr const char *kOneByOneRedPngBase64 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAA"
                                                         "DUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==";
    const QByteArray kittyImage =
        QByteArray("\033_Ga=T,f=100,q=2,c=4,r=2;") + kOneByOneRedPngBase64 + QByteArray("\033\\");

    feedTerminalOutput(widget, kittyImage);
    const QImage frame = renderWidgetImage(widget);

    const QFontMetrics fm(widget.terminalFont());
    const QRect imageArea(0, 0, fm.horizontalAdvance('M') * 4, fm.height() * 2);
    QVERIFY2(countRedPixels(frame, imageArea) > imageArea.width() * imageArea.height() / 2,
             "inline Kitty PNG image should render into its placement cells");
}

void TestTerminalWidget::testRendersTextDecorations() {
    PtySession::StartOptions options;
    options.command = QStringLiteral("sleep 5");

    CountingTerminalWidget plainWidget;
    plainWidget.setStartOptions(options);
    QVERIFY(plainWidget.initialize());
    plainWidget.resize(240, 120);
    plainWidget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&plainWidget));

    CountingTerminalWidget decoratedWidget;
    decoratedWidget.setStartOptions(options);
    QVERIFY(decoratedWidget.initialize());
    decoratedWidget.resize(240, 120);
    decoratedWidget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&decoratedWidget));
    QApplication::processEvents();

    feedTerminalOutput(plainWidget, QByteArray("x\n"));
    feedTerminalOutput(decoratedWidget, QByteArray("\x1b[4;9;53mx\x1b[0m\n"));

    const QImage plain = renderWidgetImage(plainWidget);
    const QImage decorated = renderWidgetImage(decoratedWidget);
    const QRect diff = changedBounds(plain, decorated);
    QVERIFY2(diff.isValid(), "SGR underline, strikethrough, and overline should affect rendered text");
}

void TestTerminalWidget::testStyledTextKeepsCharactersOnCellGrid() {
    PtySession::StartOptions options;
    options.command = QStringLiteral("sleep 5");

    CountingTerminalWidget widget;
    widget.setStartOptions(options);
    QFont proportionalFont(QStringLiteral("DejaVu Sans"), 14);
    proportionalFont.setStyleHint(QFont::SansSerif);
    widget.debugSetRawTerminalFont(proportionalFont);
    QVERIFY(widget.initialize());
    widget.resize(360, 120);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const QFontMetrics fm(widget.terminalFont());
    const int cellWidth = fm.horizontalAdvance(QLatin1Char('M'));
    const int cellHeight = fm.height();
    QVERIFY2(fm.horizontalAdvance(QLatin1Char('i')) < cellWidth / 2,
             "test font should simulate a fallback font with non-cell-sized glyph advances");
    const QColor background = widget.debugAppliedBackground();

    feedTerminalOutput(widget, QByteArray("\r\033[2Kiiiiiii"));
    const QImage plainFrame = renderWidgetImage(widget);
    const QRect seventhCell(cellWidth * 6, 0, cellWidth, cellHeight);
    QVERIFY2(firstPaintedColumnInCell(plainFrame, seventhCell, background) >= 0,
             "each character in a render run should be anchored to its logical terminal cell");

    feedTerminalOutput(widget, QByteArray("\r\033[2K\033[1mW\033[0morking"));
    const QImage firstFrame = renderWidgetImage(widget);

    feedTerminalOutput(widget, QByteArray("\r\033[2KWo\033[1mr\033[0mking"));
    const QImage secondFrame = renderWidgetImage(widget);

    const QRect firstKCell(cellWidth * 3, 0, cellWidth, cellHeight);
    const QRect secondKCell(cellWidth * 3, 0, cellWidth, cellHeight);
    const int firstKLeft = firstPaintedColumnInCell(firstFrame, firstKCell, background);
    const int secondKLeft = firstPaintedColumnInCell(secondFrame, secondKCell, background);
    const QRect lineRect(0, 0, cellWidth * 7, cellHeight);
    const int firstRight = lastPaintedColumn(firstFrame, lineRect, background);
    const int secondRight = lastPaintedColumn(secondFrame, lineRect, background);

    QVERIFY2(firstKLeft >= 0, "first styled frame should paint the fourth cell");
    QVERIFY2(secondKLeft >= 0, "second styled frame should paint the fourth cell");
    QCOMPARE(firstKLeft, secondKLeft);
    QCOMPARE(firstRight, secondRight);
}

void TestTerminalWidget::testConcealedTextDoesNotRenderGlyphs() {
    PtySession::StartOptions options;
    options.command = QStringLiteral("sleep 5");

    CountingTerminalWidget emptyWidget;
    emptyWidget.setStartOptions(options);
    QVERIFY(emptyWidget.initialize());
    emptyWidget.resize(240, 120);
    emptyWidget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&emptyWidget));

    CountingTerminalWidget concealedWidget;
    concealedWidget.setStartOptions(options);
    QVERIFY(concealedWidget.initialize());
    concealedWidget.resize(240, 120);
    concealedWidget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&concealedWidget));
    QApplication::processEvents();

    feedTerminalOutput(concealedWidget, QByteArray("\x1b[8mx\x1b[0m\n"));

    const QImage empty = renderWidgetImage(emptyWidget);
    const QImage concealed = renderWidgetImage(concealedWidget);
    const QRect diff = changedBounds(empty, concealed);
    QVERIFY2(!diff.isValid(), "SGR conceal should advance the terminal state without painting the glyph");
}

void TestTerminalWidget::testRendersBoxDrawingCharactersWithoutTextShaping() {
    CountingTerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.resize(240, 120);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    feedTerminalOutput(widget, QStringLiteral("┌─┐\n").toUtf8());

    renderWidgetImage(widget);

    QCOMPARE(widget.debugLastFrameTextRunCount(), 0);
    QCOMPARE(widget.debugLastFrameLineDrawCount(), 3);
}

void TestTerminalWidget::testCoalescesPlainTextIntoRenderRuns() {
    CountingTerminalWidget widget;
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    widget.repaint();
    QApplication::processEvents();

    const int previousFlushCount = widget.debugPtyFlushCount();
    const QByteArray line("abcdefghijklmnopqrstuvwxyz0123456789\n");
    const bool invoked =
        QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, line));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    renderWidgetImage(widget);

    const int perCellDrawUpperBound = widget.debugLastFrameRenderedRowCount() * widget.terminalColumns();
    QVERIFY2(widget.debugLastFrameTextRunCount() < perCellDrawUpperBound / 4,
             "plain text with one style should be rendered as a small number of text runs, not one draw per cell");
}

void TestTerminalWidget::testCursorOnlyUpdatesUseNarrowRepaint() {
    CountingTerminalWidget widget;
    PtySession::StartOptions options;
    options.command = QStringLiteral("sleep 5");
    widget.setStartOptions(options);
    QVERIFY(widget.initialize());

    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    widget.setCursorBlinkEnabled(false);
    widget.setFocus();
    QApplication::processEvents();

    int previousFlushCount = widget.debugPtyFlushCount();
    bool invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                             Q_ARG(QByteArray, QByteArray("abc")));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);
    renderWidgetImage(widget);

    const int previousCursorOnlyRepaintCount = widget.debugCursorOnlyRepaintCount();
    previousFlushCount = widget.debugPtyFlushCount();
    invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                        Q_ARG(QByteArray, QByteArray("\r")));
    QVERIFY(invoked);
    waitForNextPtyFlush(widget, previousFlushCount);

    QVERIFY2(widget.debugCursorOnlyRepaintCount() > previousCursorOnlyRepaintCount,
             "cursor-only PTY updates should repaint only the old and new cursor rectangles");
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

namespace {

QPoint cellCenterForPos(const TerminalWidget &widget, int col, int row) {
    const QFont font = widget.terminalFont();
    const QFontMetrics fm(font);
    const int cellWidth = fm.horizontalAdvance('M');
    const int cellHeight = fm.height();
    return QPoint(col * cellWidth + cellWidth / 2, row * cellHeight + cellHeight / 2);
}

QMouseEvent mouseMoveEventFor(QWidget &widget, const QPoint &pos, Qt::MouseButtons buttons = Qt::NoButton,
                              Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    const QPointF localPos(pos);
    const QPointF globalPos(widget.mapToGlobal(pos));
    return QMouseEvent(QEvent::MouseMove, localPos, localPos, globalPos, Qt::NoButton, buttons, modifiers);
}

QMouseEvent mousePressEventFor(QWidget &widget, const QPoint &pos, Qt::MouseButton button, Qt::MouseButtons buttons,
                               Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    const QPointF localPos(pos);
    const QPointF globalPos(widget.mapToGlobal(pos));
    return QMouseEvent(QEvent::MouseButtonPress, localPos, localPos, globalPos, button, buttons, modifiers);
}

} // namespace

void TestTerminalWidget::testDoubleClickSelectsWord() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("hello world test\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    // Double-click on "world" (starts at column 6)
    const QPoint pos = cellCenterForPos(widget, 6, 0);
    QMouseEvent press1(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press1);
    QMouseEvent release1(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release1);

    QMouseEvent press2(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press2);

    const QString text = widget.debugSelectedText();
    QCOMPARE(text, QStringLiteral("world"));
}

void TestTerminalWidget::testDoubleClickSelectsWordWithDots() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("linux-image-6.18.19-amd64-desktop-rolling\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    const QPoint pos = cellCenterForPos(widget, 18, 0);
    QMouseEvent press1(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press1);
    QMouseEvent release1(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release1);

    QMouseEvent press2(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press2);

    const QString text = widget.debugSelectedText();
    QCOMPARE(text, QStringLiteral("linux-image-6.18.19-amd64-desktop-rolling"));
}

void TestTerminalWidget::testDoubleClickSelectsWrappedFilename() {
    CountingTerminalWidget widget;
    widget.resize(120, 120);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const QString filename = QStringLiteral("linux-image-6.18.19-amd64-desktop-rolling-wrapped-target-package.deb");
    feedTerminalOutput(widget, filename.toUtf8());
    widget.repaint();
    QApplication::processEvents();

    const int continuationRow = firstScreenRowContaining(widget, QStringLiteral("amd64"));
    QVERIFY2(continuationRow > 0, qPrintable(widget.visibleText()));
    const int continuationCol = widget.debugTextForScreenRow(continuationRow).indexOf(QStringLiteral("amd64"));
    QVERIFY(continuationCol >= 0);

    const QPoint pos = cellCenterForPos(widget, continuationCol, continuationRow);
    QMouseEvent press1(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press1);
    QMouseEvent release1(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release1);
    QMouseEvent press2(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press2);
    QMouseEvent release2(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release2);

    const QString text = widget.debugSelectedText();
    QCOMPARE(text, filename);
}

void TestTerminalWidget::testDoubleClickSelectsWordWithHyphens() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("apt-get install foo-bar-baz\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    const QPoint pos = cellCenterForPos(widget, 3, 0);
    QMouseEvent press1(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press1);
    QMouseEvent release1(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release1);

    QMouseEvent press2(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press2);

    const QString text = widget.debugSelectedText();
    QCOMPARE(text, QStringLiteral("apt-get"));
}

void TestTerminalWidget::testDoubleClickSelectsWordWithUnderscores() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("MY_ENV_VAR=/usr/local/bin\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    const QPoint pos = cellCenterForPos(widget, 3, 0);
    QMouseEvent press1(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press1);
    QMouseEvent release1(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release1);

    QMouseEvent press2(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press2);

    const QString text = widget.debugSelectedText();
    QCOMPARE(text, QStringLiteral("MY_ENV_VAR"));
}

void TestTerminalWidget::testDoubleClickSelectsWordWithMixedPunctuation() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("config.json.backup saved.ok\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    const QPoint pos = cellCenterForPos(widget, 3, 0);
    QMouseEvent press1(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press1);
    QMouseEvent release1(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release1);

    QMouseEvent press2(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press2);

    const QString text = widget.debugSelectedText();
    QCOMPARE(text, QStringLiteral("config.json.backup"));
}

void TestTerminalWidget::testDoubleClickStopsAtParentheses() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("foo(bar)baz\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    const QPoint pos = cellCenterForPos(widget, 1, 0);
    QMouseEvent press1(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press1);
    QMouseEvent release1(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release1);

    QMouseEvent press2(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press2);

    const QString text = widget.debugSelectedText();
    QCOMPARE(text, QStringLiteral("foo"));
}

void TestTerminalWidget::testDoubleClickStopsAtColon() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("error: something went wrong\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    const QPoint pos = cellCenterForPos(widget, 2, 0);
    QMouseEvent press1(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press1);
    QMouseEvent release1(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release1);

    QMouseEvent press2(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press2);

    const QString text = widget.debugSelectedText();
    QCOMPARE(text, QStringLiteral("error"));
}

void TestTerminalWidget::testDoubleClickStopsAtAmpersand() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("cmd1&&cmd2\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    const QPoint pos = cellCenterForPos(widget, 1, 0);
    QMouseEvent press1(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press1);
    QMouseEvent release1(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release1);
    QMouseEvent press2(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press2);

    const QString text = widget.debugSelectedText();
    QCOMPARE(text, QStringLiteral("cmd1"));
}

void TestTerminalWidget::testDoubleClickStopsAtPlus() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("foo+bar\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    const QPoint pos = cellCenterForPos(widget, 1, 0);
    QMouseEvent press1(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press1);
    QMouseEvent release1(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release1);
    QMouseEvent press2(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press2);

    const QString text = widget.debugSelectedText();
    QCOMPARE(text, QStringLiteral("foo"));
}

void TestTerminalWidget::testDoubleClickStopsAtHash() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("value # comment\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    const QPoint pos = cellCenterForPos(widget, 1, 0);
    QMouseEvent press1(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press1);
    QMouseEvent release1(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release1);
    QMouseEvent press2(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press2);

    const QString text = widget.debugSelectedText();
    QCOMPARE(text, QStringLiteral("value"));
}

void TestTerminalWidget::testDoubleClickStopsAtExclamation() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("done!next\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    const QPoint pos = cellCenterForPos(widget, 1, 0);
    QMouseEvent press1(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press1);
    QMouseEvent release1(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release1);
    QMouseEvent press2(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press2);

    const QString text = widget.debugSelectedText();
    QCOMPARE(text, QStringLiteral("done"));
}

void TestTerminalWidget::testDoubleClickStopsAtGlob() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("file*.txt\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    const QPoint pos = cellCenterForPos(widget, 1, 0);
    QMouseEvent press1(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press1);
    QMouseEvent release1(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release1);
    QMouseEvent press2(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press2);

    const QString text = widget.debugSelectedText();
    QCOMPARE(text, QStringLiteral("file"));
}

void TestTerminalWidget::testDoubleClickSelectsPath() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("cp /usr/local/bin/app /opt/\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    const QPoint pos = cellCenterForPos(widget, 7, 0);
    QMouseEvent press1(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press1);
    QMouseEvent release1(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release1);
    QMouseEvent press2(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press2);

    const QString text = widget.debugSelectedText();
    QCOMPARE(text, QStringLiteral("/usr/local/bin/app"));
}

void TestTerminalWidget::testSingleClickDoesNotSelectCell() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("hello world\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    const QPoint clickPos = cellCenterForPos(widget, 1, 0);
    QMouseEvent pressEvent = mousePressEventFor(widget, clickPos, Qt::LeftButton, Qt::LeftButton);
    QApplication::sendEvent(&widget, &pressEvent);
    QMouseEvent releaseEvent(QEvent::MouseButtonRelease, clickPos, clickPos, Qt::LeftButton, Qt::NoButton,
                             Qt::NoModifier);
    QApplication::sendEvent(&widget, &releaseEvent);
    QApplication::processEvents();

    QVERIFY(!widget.debugCellInSelection(0, 1));
    QVERIFY(widget.debugSelectedText().isEmpty());
}

void TestTerminalWidget::testDoubleClickSetsSelectionClipboard() {
    RecordingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("hello world test\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    const QPoint pos = cellCenterForPos(widget, 6, 0);
    QMouseEvent press1(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press1);
    QMouseEvent release1(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release1);
    QMouseEvent press2(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press2);
    QMouseEvent release2(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release2);
    QApplication::processEvents();

    QCOMPARE(widget.capturedSelectionClipboardText(), QStringLiteral("world"));
}

void TestTerminalWidget::testTripleClickSetsSelectionClipboard() {
    RecordingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("hello world test\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    const QPoint pos = cellCenterForPos(widget, 6, 0);
    for (int i = 0; i < 3; ++i) {
        QMouseEvent press(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&widget, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&widget, &release);
    }
    QApplication::processEvents();

    QVERIFY(!widget.capturedSelectionClipboardText().isEmpty());
    QVERIFY(widget.capturedSelectionClipboardText().startsWith(QStringLiteral("hello")));
}

void TestTerminalWidget::testTripleClickSelectsLine() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("hello world test\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    // Triple-click on the line
    const QPoint pos = cellCenterForPos(widget, 3, 0);
    for (int i = 0; i < 3; ++i) {
        QMouseEvent press(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&widget, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&widget, &release);
    }

    const QString text = widget.debugSelectedText();
    // Triple-click should select the entire line (all columns)
    QVERIFY(!text.isEmpty());
    QVERIFY(text.startsWith(QStringLiteral("hello")));
}

void TestTerminalWidget::testDoubleClickDragExtendsByWord() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("hello world test\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    // Double-click on "hello"
    const QPoint startPos = cellCenterForPos(widget, 1, 0);
    QMouseEvent press1(QEvent::MouseButtonPress, startPos, startPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press1);
    QMouseEvent release1(QEvent::MouseButtonRelease, startPos, startPos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release1);
    QMouseEvent press2(QEvent::MouseButtonPress, startPos, startPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press2);

    // Drag to "test"
    const QPoint endPos = cellCenterForPos(widget, 12, 0);
    QMouseEvent moveEvent(QEvent::MouseMove, endPos, endPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &moveEvent);

    const QString text = widget.debugSelectedText();
    QCOMPARE(text, QStringLiteral("hello world test"));
}

void TestTerminalWidget::testTripleClickDragExtendsByLine() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("line one\nline two\nline three\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    // Triple-click on "line one"
    const QPoint startPos = cellCenterForPos(widget, 2, 0);
    for (int i = 0; i < 3; ++i) {
        QMouseEvent press(QEvent::MouseButtonPress, startPos, startPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&widget, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, startPos, startPos, Qt::LeftButton, Qt::NoButton,
                            Qt::NoModifier);
        QApplication::sendEvent(&widget, &release);
    }

    // Drag to "line three"
    const QPoint endPos = cellCenterForPos(widget, 2, 2);
    QMouseEvent moveEvent(QEvent::MouseMove, endPos, endPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &moveEvent);

    const QString text = widget.debugSelectedText();
    QVERIFY(!text.isEmpty());
    QVERIFY(text.contains(QStringLiteral("line one")));
    QVERIFY(text.contains(QStringLiteral("line three")));
}

void TestTerminalWidget::testSelectionDragAboveViewportAutoScrollsUp() {
    CountingTerminalWidget widget;
    widget.resize(240, 80);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    QByteArray lines;
    for (int i = 0; i < 30; ++i)
        lines += QByteArray("line ") + QByteArray::number(i) + '\n';
    feedTerminalOutput(widget, lines);

    const auto bottomState = widget.viewportScrollState();
    QVERIFY(bottomState.offset > 0);

    const QPoint startPos = cellCenterForPos(widget, 2, 1);
    QMouseEvent press(QEvent::MouseButtonPress, startPos, startPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press);

    const QPoint outsideTop(widget.width() / 2, -20);
    QMouseEvent move(QEvent::MouseMove, outsideTop, outsideTop, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &move);

    QTRY_VERIFY_WITH_TIMEOUT(widget.viewportScrollState().offset < bottomState.offset, 500);

    QMouseEvent release(QEvent::MouseButtonRelease, outsideTop, outsideTop, Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    QApplication::sendEvent(&widget, &release);
}

void TestTerminalWidget::testSelectionDragBelowViewportAutoScrollsDown() {
    CountingTerminalWidget widget;
    widget.resize(240, 80);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    QByteArray lines;
    for (int i = 0; i < 30; ++i)
        lines += QByteArray("line ") + QByteArray::number(i) + '\n';
    feedTerminalOutput(widget, lines);
    widget.scrollViewportToOffset(0);
    QCOMPARE(widget.viewportScrollState().offset, 0);

    const QPoint startPos = cellCenterForPos(widget, 2, widget.terminalRows() - 2);
    QMouseEvent press(QEvent::MouseButtonPress, startPos, startPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press);

    const QPoint outsideBottom(widget.width() / 2, widget.height() + 20);
    QMouseEvent move(QEvent::MouseMove, outsideBottom, outsideBottom, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &move);

    QTRY_VERIFY_WITH_TIMEOUT(widget.viewportScrollState().offset > 0, 500);

    QMouseEvent release(QEvent::MouseButtonRelease, outsideBottom, outsideBottom, Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    QApplication::sendEvent(&widget, &release);
}

void TestTerminalWidget::testZoomInIncreasesFontSize() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    const int before = widget.terminalFont().pointSize();
    widget.zoomIn();
    QCOMPARE(widget.terminalFont().pointSize(), before + 1);
}

void TestTerminalWidget::testZoomOutDecreasesFontSize() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    const int before = widget.terminalFont().pointSize();
    widget.zoomOut();
    QCOMPARE(widget.terminalFont().pointSize(), before - 1);
}

void TestTerminalWidget::testZoomInClampsAtMaximum() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    QFont maxFont = widget.terminalFont();
    maxFont.setPointSize(72);
    widget.setTerminalFont(maxFont);
    QCOMPARE(widget.terminalFont().pointSize(), 72);
    widget.zoomIn();
    QCOMPARE(widget.terminalFont().pointSize(), 72);
}

void TestTerminalWidget::testZoomOutClampsAtMinimum() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    QFont minFont = widget.terminalFont();
    minFont.setPointSize(5);
    widget.setTerminalFont(minFont);
    QCOMPARE(widget.terminalFont().pointSize(), 5);
    widget.zoomOut();
    QCOMPARE(widget.terminalFont().pointSize(), 5);
}

void TestTerminalWidget::testSetScrollbackLines() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    widget.setScrollbackLines(5000);
    QVERIFY(true);
}

void TestTerminalWidget::testScrollbackLineCountMapsToByteBudget() {
    TerminalWidget widget;

    QCOMPARE(widget.debugScrollbackLines(), 5000);
    QCOMPARE(widget.debugScrollbackByteBudget(), size_t(100 * 1000 * 1000));

    widget.setScrollbackLines(20000);
    QCOMPARE(widget.debugScrollbackLines(), 20000);
    QCOMPARE(widget.debugScrollbackByteBudget(), size_t(400 * 1000 * 1000));
}

void TestTerminalWidget::testViewportScrollStateAndAbsoluteScroll() {
    CountingTerminalWidget widget;
    widget.resize(240, 80);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    feedTerminalOutput(widget, QByteArray("one\ntwo\nthree\nfour\nfive\nsix\n"));

    const auto bottomState = widget.viewportScrollState();
    QVERIFY(bottomState.totalRows > bottomState.visibleRows);
    QCOMPARE(bottomState.offset, bottomState.maximumOffset());

    QSignalSpy spy(&widget, &TerminalWidget::viewportScrollStateChanged);
    QVERIFY(spy.isValid());

    widget.scrollViewportToOffset(0);
    QTRY_VERIFY(spy.count() > 0);
    QCOMPARE(widget.viewportScrollState().offset, 0);

    widget.scrollViewportToOffset(widget.viewportScrollState().maximumOffset());
    QTRY_COMPARE(widget.viewportScrollState().offset, bottomState.maximumOffset());
}

void TestTerminalWidget::testOutputDoesNotFollowBottomWhenViewportScrolledBack() {
    CountingTerminalWidget widget;
    widget.resize(240, 80);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    feedTerminalOutput(widget, QByteArray("one\ntwo\nthree\nfour\nfive\nsix\n"));

    const auto bottomState = widget.viewportScrollState();
    QVERIFY(bottomState.totalRows > bottomState.visibleRows);
    QCOMPARE(bottomState.offset, bottomState.maximumOffset());

    widget.scrollViewportToOffset(0);
    QCOMPARE(widget.viewportScrollState().offset, 0);

    feedTerminalOutput(widget, QByteArray("seven\n"));

    const auto afterOutput = widget.viewportScrollState();
    QCOMPARE(afterOutput.offset, 0);
    QVERIFY(afterOutput.maximumOffset() > bottomState.maximumOffset());
}

void TestTerminalWidget::testOutputDoesNotRepaintScrolledBackViewportContent() {
    CountingTerminalWidget widget;
    widget.resize(240, 80);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    QByteArray lines;
    for (int i = 0; i < 40; ++i)
        lines += QByteArray("line ") + QByteArray::number(i) + '\n';
    feedTerminalOutput(widget, lines);

    const auto bottomState = widget.viewportScrollState();
    QVERIFY(bottomState.totalRows > bottomState.visibleRows);
    QCOMPARE(bottomState.offset, bottomState.maximumOffset());

    const int scrolledOffset = qMax(0, bottomState.maximumOffset() - 3);
    QVERIFY(scrolledOffset < bottomState.maximumOffset());
    widget.scrollViewportToOffset(scrolledOffset);
    QCOMPARE(widget.viewportScrollState().offset, scrolledOffset);
    QApplication::processEvents();

    const QImage before = renderWidgetImage(widget);
    feedTerminalOutput(widget, QByteArray("tail\n"));
    const QImage after = renderWidgetImage(widget);

    QCOMPARE(widget.viewportScrollState().offset, scrolledOffset);
    QCOMPARE(changedBounds(before, after), QRect());
}

void TestTerminalWidget::testContinuousOutputDoesNotMoveScrolledBackViewportContent() {
    CountingTerminalWidget widget;
    widget.resize(240, 80);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    QByteArray lines;
    for (int i = 0; i < 120; ++i)
        lines += QByteArray("line ") + QByteArray::number(i) + '\n';
    feedTerminalOutput(widget, lines);

    const auto bottomState = widget.viewportScrollState();
    QVERIFY(bottomState.totalRows > bottomState.visibleRows);

    const int scrolledOffset = qMax(0, bottomState.maximumOffset() - 3);
    QVERIFY(scrolledOffset < bottomState.maximumOffset());
    widget.scrollViewportToOffset(scrolledOffset);
    QCOMPARE(widget.viewportScrollState().offset, scrolledOffset);
    QApplication::processEvents();

    const QImage before = renderWidgetImage(widget);
    for (int i = 0; i < 30; ++i)
        feedTerminalOutput(widget, QByteArray("tail ") + QByteArray::number(i) + '\n');
    const QImage after = renderWidgetImage(widget);

    QCOMPARE(widget.viewportScrollState().offset, scrolledOffset);
    QCOMPARE(changedBounds(before, after), QRect());
}

void TestTerminalWidget::testContinuousOutputDoesNotFollowBottomWhenScrollbackPrunes() {
    CountingTerminalWidget widget;
    widget.resize(240, 80);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    QByteArray lines;
    const QByteArray payload(512, 'x');
    for (int i = 0; i < 22000; ++i)
        lines += QByteArray("line ") + QByteArray::number(i) + ' ' + payload + '\n';
    feedTerminalOutput(widget, lines);

    const auto bottomState = widget.viewportScrollState();
    QVERIFY(bottomState.totalRows > bottomState.visibleRows);

    const int scrolledOffset = qMax(0, bottomState.maximumOffset() - 3);
    QVERIFY(scrolledOffset < bottomState.maximumOffset());
    widget.scrollViewportToOffset(scrolledOffset);
    QCOMPARE(widget.viewportScrollState().offset, scrolledOffset);
    QApplication::processEvents();

    for (int i = 0; i < 200; ++i)
        feedTerminalOutput(widget, QByteArray("tail ") + QByteArray::number(i) + ' ' + payload + '\n');

    const auto afterOutput = widget.viewportScrollState();
    QVERIFY(afterOutput.offset < afterOutput.maximumOffset());
}

void TestTerminalWidget::testOutputDoesNotFollowBottomAfterMouseWheelScrollback() {
    CountingTerminalWidget widget;
    widget.resize(240, 80);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    feedTerminalOutput(widget, QByteArray("one\ntwo\nthree\nfour\nfive\nsix\n"));

    const auto bottomState = widget.viewportScrollState();
    QVERIFY(bottomState.totalRows > bottomState.visibleRows);
    QCOMPARE(bottomState.offset, bottomState.maximumOffset());

    QWheelEvent wheelEvent(QPointF(10, 10), QPointF(10, 10), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                           Qt::ScrollUpdate, false);
    QApplication::sendEvent(&widget, &wheelEvent);
    const auto scrolledState = widget.viewportScrollState();
    QVERIFY(scrolledState.offset < bottomState.offset);

    feedTerminalOutput(widget, QByteArray("seven\n"));

    const auto afterOutput = widget.viewportScrollState();
    QCOMPARE(afterOutput.offset, scrolledState.offset);
    QVERIFY(afterOutput.offset < afterOutput.maximumOffset());
}

void TestTerminalWidget::testPendingOutputDoesNotFollowBottomAfterMouseWheelScrollback() {
    CountingTerminalWidget widget;
    widget.resize(240, 80);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    feedTerminalOutput(widget, QByteArray("one\ntwo\nthree\nfour\nfive\nsix\n"));

    const auto bottomState = widget.viewportScrollState();
    QVERIFY(bottomState.totalRows > bottomState.visibleRows);
    QCOMPARE(bottomState.offset, bottomState.maximumOffset());

    const int flushBefore = widget.debugPtyFlushCount();
    const bool invoked = QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                                                   Q_ARG(QByteArray, QByteArray("seven\n")));
    QVERIFY(invoked);
    QCOMPARE(widget.debugPtyFlushCount(), flushBefore);

    QWheelEvent wheelEvent(QPointF(10, 10), QPointF(10, 10), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                           Qt::ScrollUpdate, false);
    QApplication::sendEvent(&widget, &wheelEvent);
    const auto scrolledState = widget.viewportScrollState();
    QVERIFY(scrolledState.offset < bottomState.offset);

    waitForNextPtyFlush(widget, flushBefore);

    const auto afterOutput = widget.viewportScrollState();
    QCOMPARE(afterOutput.offset, scrolledState.offset);
    QVERIFY(afterOutput.offset < afterOutput.maximumOffset());
}

void TestTerminalWidget::testKeyInputFollowsBottomWhenViewportScrolledBack() {
    CountingTerminalWidget widget;
    populateAndScrollBack(widget);

    auto *session = widget.findChild<PtySession *>();
    QVERIFY(session);
    QSignalSpy spy(session, &PtySession::dataWritten);
    QVERIFY(spy.isValid());

    QTest::keyClick(&widget, Qt::Key_A);

    QTRY_VERIFY_WITH_TIMEOUT(spy.count() > 0, 500);
    const auto afterInput = widget.viewportScrollState();
    QCOMPARE(afterInput.offset, afterInput.maximumOffset());
}

void TestTerminalWidget::testInputMethodCommitFollowsBottomWhenViewportScrolledBack() {
    CountingTerminalWidget widget;
    populateAndScrollBack(widget);

    auto *session = widget.findChild<PtySession *>();
    QVERIFY(session);
    QSignalSpy spy(session, &PtySession::dataWritten);
    QVERIFY(spy.isValid());

    QInputMethodEvent imeEvent;
    imeEvent.setCommitString(QStringLiteral("中文"));
    QApplication::sendEvent(&widget, &imeEvent);

    QTRY_VERIFY_WITH_TIMEOUT(collectedPtyOutput(spy).contains(QStringLiteral("中文").toUtf8()), 500);
    const auto afterInput = widget.viewportScrollState();
    QCOMPARE(afterInput.offset, afterInput.maximumOffset());
}

void TestTerminalWidget::testPasteFollowsBottomWhenViewportScrolledBack() {
    CountingTerminalWidget widget;
    populateAndScrollBack(widget);

    auto *session = widget.findChild<PtySession *>();
    QVERIFY(session);
    QSignalSpy spy(session, &PtySession::dataWritten);
    QVERIFY(spy.isValid());

    QGuiApplication::clipboard()->setText(QStringLiteral("pasted text"));
    QTest::qWait(50);
    widget.pasteFromClipboard();

    QTRY_VERIFY_WITH_TIMEOUT(collectedPtyOutput(spy).contains("pasted text"), 500);
    const auto afterInput = widget.viewportScrollState();
    QCOMPARE(afterInput.offset, afterInput.maximumOffset());
}

void TestTerminalWidget::testSetOpacity() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    widget.setOpacity(0.5);
    QCOMPARE(widget.opacity(), qreal(0.5));
}

void TestTerminalWidget::testSetOpacityRepaintsCachedBackground() {
    CountingTerminalWidget widget;
    QVERIFY(widget.initialize());
    widget.resize(640, 400);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    widget.setCursorBlinkEnabled(false);
    QApplication::processEvents();

    const QImage opaqueFrame = renderWidgetImage(widget);
    const QColor opaquePixel = QColor::fromRgba(opaqueFrame.pixel(opaqueFrame.width() / 2, opaqueFrame.height() / 2));
    QVERIFY(opaquePixel.alpha() > 240);

    widget.setOpacity(0.5);
    const QImage translucentFrame = renderWidgetImage(widget);
    const QColor translucentPixel =
        QColor::fromRgba(translucentFrame.pixel(translucentFrame.width() / 2, translucentFrame.height() / 2));

    QVERIFY2(translucentPixel.alpha() < opaquePixel.alpha(),
             "opacity changes should repaint the cached terminal background with the new alpha");
}

void TestTerminalWidget::testSetOpacityFullDisablesTranslucentBackground() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    widget.setOpacity(0.5);
    QVERIFY(widget.testAttribute(Qt::WA_TranslucentBackground));
    widget.setOpacity(1.0);
    QVERIFY(!widget.testAttribute(Qt::WA_TranslucentBackground));
}

void TestTerminalWidget::testSetOpacityPartialEnablesTranslucentBackground() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    QVERIFY(!widget.testAttribute(Qt::WA_TranslucentBackground));
    widget.setOpacity(0.8);
    QVERIFY(widget.testAttribute(Qt::WA_TranslucentBackground));
}

void TestTerminalWidget::testHasRunningProcessReturnsFalseForShell() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    auto *session = widget.findChild<PtySession *>();
    QVERIFY(session);
    QSignalSpy spy(session, &PtySession::dataReceived);
    QVERIFY(spy.isValid());
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() > 0, 3000);
    QVERIFY(!widget.hasRunningProcess());
}

void TestTerminalWidget::testSelectAllCreatesSelection() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("hello terminal\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    widget.selectAll();
    const QString text = widget.debugSelectedText();
    QVERIFY(!text.isEmpty());
}

void TestTerminalWidget::testSelectAllThenCopyToClipboard() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("copy test content\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    QGuiApplication::clipboard()->clear();
    widget.selectAll();
    widget.copyToClipboard();
    const QString clipboardText = QGuiApplication::clipboard()->text();
    QVERIFY(!clipboardText.isEmpty());
}

void TestTerminalWidget::testPasteFromClipboardSendsToPty() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    widget.setFocus();

    auto *session = widget.findChild<PtySession *>();
    QVERIFY(session);

    QSignalSpy spy(session, &PtySession::dataWritten);
    QVERIFY(spy.isValid());

    QGuiApplication::clipboard()->setText(QStringLiteral("pasted text"));
    QTest::qWait(50);

    spy.clear();
    widget.pasteFromClipboard();

    QTRY_VERIFY_WITH_TIMEOUT(spy.count() > 0, 500);
    const QByteArray written = collectedPtyOutput(spy);
    QVERIFY(written.contains("pasted text"));
}

void TestTerminalWidget::testDropLocalFileWritesEscapedPathToPty() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    widget.resize(960, 640);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    auto *session = widget.findChild<PtySession *>();
    QVERIFY(session);
    QSignalSpy spy(session, &PtySession::dataWritten);
    QVERIFY(spy.isValid());

    auto *mimeData = new QMimeData;
    mimeData->setUrls({QUrl::fromLocalFile(QStringLiteral("/tmp/drop test/it's here.txt"))});

    QDragEnterEvent dragEnter(QPoint(10, 10), Qt::CopyAction, mimeData, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &dragEnter);
    QVERIFY(dragEnter.isAccepted());

    QDropEvent drop(QPointF(10, 10), Qt::CopyAction, mimeData, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &drop);

    QTRY_VERIFY_WITH_TIMEOUT(spy.count() > 0, 500);
    const QByteArray written = collectedPtyOutput(spy);
    QVERIFY(written.contains("'/tmp/drop test/it'\\''s here.txt'"));
}

void TestTerminalWidget::testOsc52WritesClipboard() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    QGuiApplication::clipboard()->clear();
    const QByteArray encoded = QByteArray("copied from osc52").toBase64();
    const QByteArray sequence = QByteArray("\033]52;c;") + encoded + QByteArray("\a");

    const bool invoked =
        QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, sequence));
    QVERIFY(invoked);

    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("copied from osc52"));
}

void TestTerminalWidget::testOsc52WritesClipboardAcrossChunks() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    QGuiApplication::clipboard()->clear();
    const QByteArray encoded = QByteArray("chunked osc52").toBase64();
    const QByteArray firstChunk = QByteArray("\033]52;c;") + encoded.left(encoded.size() / 2);
    const QByteArray secondChunk = encoded.mid(encoded.size() / 2) + QByteArray("\033\\");

    QVERIFY(
        QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, firstChunk)));
    QCOMPARE(QGuiApplication::clipboard()->text(), QString());

    QVERIFY(
        QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, secondChunk)));
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("chunked osc52"));
}

void TestTerminalWidget::testOsc52ReadRequestDoesNotChangeClipboard() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());

    QGuiApplication::clipboard()->setText(QStringLiteral("existing"));
    const QByteArray sequence = QByteArray("\033]52;c;?\a");

    QVERIFY(QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, sequence)));
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("existing"));
}

void TestTerminalWidget::testSearchFindsMatchInTerminalContent() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("hello world\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    widget.performSearch(QStringLiteral("hello"));
    QVERIFY(widget.hasSearchMatches());
}

void TestTerminalWidget::testSearchNoMatch() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    widget.performSearch(QStringLiteral("zzznotfound"));
    QVERIFY(!widget.hasSearchMatches());
}

void TestTerminalWidget::testSearchEmptyQueryClears() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("hello world\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    widget.performSearch(QStringLiteral("hello"));
    QVERIFY(widget.hasSearchMatches());
    widget.performSearch(QString());
    QVERIFY(!widget.hasSearchMatches());
}

void TestTerminalWidget::testClearSearch() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("hello world\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    widget.performSearch(QStringLiteral("hello"));
    QVERIFY(widget.hasSearchMatches());
    widget.clearSearch();
    QVERIFY(!widget.hasSearchMatches());
}

void TestTerminalWidget::testFindNextCycles() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("abc hello def hello ghi\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    widget.performSearch(QStringLiteral("hello"));
    QVERIFY(widget.hasSearchMatches());
    widget.findNext();
    QVERIFY(widget.hasSearchMatches());
    widget.findNext();
    QVERIFY(widget.hasSearchMatches());
}

void TestTerminalWidget::testFindPreviousCycles() {
    CountingTerminalWidget widget;
    widget.resize(960, 640);
    QVERIFY(widget.initialize());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    const int flushBefore = widget.debugPtyFlushCount();
    QMetaObject::invokeMethod(&widget, "onPtyDataReceived", Qt::DirectConnection,
                              Q_ARG(QByteArray, QByteArray("abc hello def hello ghi\n")));
    waitForNextPtyFlush(widget, flushBefore);
    widget.repaint();
    QApplication::processEvents();

    widget.performSearch(QStringLiteral("hello"));
    QVERIFY(widget.hasSearchMatches());
    widget.findPrevious();
    QVERIFY(widget.hasSearchMatches());
    widget.findPrevious();
    QVERIFY(widget.hasSearchMatches());
}

void TestTerminalWidget::testFindNextOnEmptyMatchesIsNoop() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    widget.findNext();
    QVERIFY(!widget.hasSearchMatches());
}

void TestTerminalWidget::testFindPreviousOnEmptyMatchesIsNoop() {
    TerminalWidget widget;
    QVERIFY(widget.initialize());
    widget.findPrevious();
    QVERIFY(!widget.hasSearchMatches());
}

void TestTerminalWidget::testImportVtContentDropsPendingPtyOutput() {
    CountingTerminalWidget saved;
    saved.resize(960, 640);
    QVERIFY(saved.initialize());
    saved.show();
    QVERIFY(QTest::qWaitForWindowExposed(&saved));

    feedTerminalOutput(saved, QByteArray("RESTORED_CONTENT\n"));
    const QByteArray vtData = saved.exportVtContent();
    QVERIFY(!vtData.isEmpty());

    CountingTerminalWidget restored;
    restored.resize(960, 640);
    QVERIFY(restored.initialize());
    restored.show();
    QVERIFY(QTest::qWaitForWindowExposed(&restored));

    const int flushBefore = restored.debugPtyFlushCount();
    const bool invoked = QMetaObject::invokeMethod(&restored, "onPtyDataReceived", Qt::DirectConnection,
                                                   Q_ARG(QByteArray, QByteArray("STALE_PROMPT_MARKER\n")));
    QVERIFY(invoked);
    QCOMPARE(restored.debugPtyFlushCount(), flushBefore);
    QVERIFY(restored.debugPendingPtyDataSize() >= QByteArray("STALE_PROMPT_MARKER\n").size());

    restored.importVtContent(vtData);

    QCOMPARE(restored.debugPendingPtyDataSize(), 0);
    restored.performSearch(QStringLiteral("RESTORED_CONTENT"));
    QVERIFY(restored.hasSearchMatches());
    restored.performSearch(QStringLiteral("STALE_PROMPT_MARKER"));
    QVERIFY(!restored.hasSearchMatches());
}

void TestTerminalWidget::testImportVtContentClearsStaleShellIntegrationState() {
    CountingTerminalWidget saved;
    saved.resize(960, 640);
    QVERIFY(saved.initialize());
    saved.show();
    QVERIFY(QTest::qWaitForWindowExposed(&saved));

    feedTerminalOutput(saved, QByteArray("RESTORED_CONTENT\n"));
    const QByteArray vtData = saved.exportVtContent();
    QVERIFY(!vtData.isEmpty());

    CountingTerminalWidget restored;
    restored.resize(960, 640);
    QVERIFY(restored.initialize());
    restored.show();
    QVERIFY(QTest::qWaitForWindowExposed(&restored));

    const QByteArray command = QByteArray("\033]777;ShellCommand=") + QByteArray("enNo") + QByteArray("\033\\");
    const bool invoked =
        QMetaObject::invokeMethod(&restored, "onPtyDataReceived", Qt::DirectConnection, Q_ARG(QByteArray, command));
    QVERIFY(invoked);
    QCOMPARE(restored.property("shellCommand").toString(), QStringLiteral("zsh"));
    QCOMPARE(restored.property("commandState").toInt(), static_cast<int>(TerminalWidget::CommandState::Running));

    restored.importVtContent(vtData);

    QCOMPARE(restored.property("shellCommand").toString(), QString());
    QCOMPARE(restored.property("commandState").toInt(), static_cast<int>(TerminalWidget::CommandState::Idle));
}

void TestTerminalWidget::testImportVtContentKeepsFuturePtyOutputAfterRestoredScreen() {
    CountingTerminalWidget restored;
    restored.resize(960, 640);
    QVERIFY(restored.initialize());
    restored.show();
    QVERIFY(QTest::qWaitForWindowExposed(&restored));

    restored.importVtContent(QByteArray("RESTORED_LINE\033[1;1H"));
    const int restoredRow = firstScreenRowContaining(restored, QStringLiteral("RESTORED_LINE"));
    QVERIFY(restoredRow >= 0);

    const int flushBefore = restored.debugPtyFlushCount();
    const bool invoked = QMetaObject::invokeMethod(&restored, "onPtyDataReceived", Qt::DirectConnection,
                                                   Q_ARG(QByteArray, QByteArray("LIVE_PROMPT")));
    QVERIFY(invoked);
    QTRY_VERIFY_WITH_TIMEOUT(restored.debugPtyFlushCount() > flushBefore, 100);
    QApplication::processEvents();

    const QString restoredLine = restored.debugTextForScreenRow(restoredRow);
    QVERIFY(restoredLine.contains(QStringLiteral("RESTORED_LINE")));
    QVERIFY(!restoredLine.contains(QStringLiteral("LIVE_PROMPT")));
}

void TestTerminalWidget::testBareLinkUriAtPositionDetectsHttp() {
    TerminalWidget widget;
    widget.resize(400, 300);
    QVERIFY(widget.initialize());
    widget.importVtContent(QByteArray("Visit https://example.com/path\n"));
    QApplication::processEvents();
    widget.scrollViewportToOffset(0);
    QApplication::processEvents();

    const QPoint linkPos = cellCenterForPos(widget, 8, 0);
    QCOMPARE(widget.linkUriAtPosition(linkPos), QStringLiteral("https://example.com/path"));
}

void TestTerminalWidget::testHyperlinkUriAtPositionIgnoresBareLink() {
    TerminalWidget widget;
    widget.resize(400, 300);
    QVERIFY(widget.initialize());
    widget.importVtContent(QByteArray("Visit https://example.com/path\n"));
    QApplication::processEvents();
    widget.scrollViewportToOffset(0);
    QApplication::processEvents();

    const QPoint linkPos = cellCenterForPos(widget, 8, 0);
    QCOMPARE(widget.hyperlinkUriAtPosition(linkPos), QString());
}

void TestTerminalWidget::testBareLinkCtrlClickActivatesLink() {
    TerminalWidget widget;
    widget.resize(400, 300);
    QVERIFY(widget.initialize());
    widget.setMouseTracking(true);
    widget.show();
    QApplication::processEvents();

    widget.importVtContent(QByteArray("Visit https://example.com/path\n"));
    QApplication::processEvents();
    widget.scrollViewportToOffset(0);
    QApplication::processEvents();

    QSignalSpy activateSpy(&widget, &TerminalWidget::linkActivated);
    QVERIFY(activateSpy.isValid());

    const QPoint clickPos = cellCenterForPos(widget, 8, 0);
    QMouseEvent pressEvent = mousePressEventFor(widget, clickPos, Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier);
    QApplication::sendEvent(&widget, &pressEvent);
    QApplication::processEvents();

    QCOMPARE(activateSpy.count(), 1);
    QCOMPARE(activateSpy.first().first().toString(), QStringLiteral("https://example.com/path"));
}

void TestTerminalWidget::testBareLinkUriAtPositionDetectsSupportedSchemes_data() {
    QTest::addColumn<QByteArray>("content");
    QTest::addColumn<int>("col");
    QTest::addColumn<QString>("expected");

    QTest::newRow("http") << QByteArray("x http://example.com\n") << 3 << QStringLiteral("http://example.com");
    QTest::newRow("https") << QByteArray("x https://example.com\n") << 3 << QStringLiteral("https://example.com");
    QTest::newRow("ssh") << QByteArray("x ssh://git@example.com/repo\n") << 3
                         << QStringLiteral("ssh://git@example.com/repo");
    QTest::newRow("mailto") << QByteArray("x mailto:user@example.com\n") << 3
                            << QStringLiteral("mailto:user@example.com");
    QTest::newRow("file") << QByteArray("x file:///tmp/example.txt\n") << 3
                          << QStringLiteral("file:///tmp/example.txt");
}

void TestTerminalWidget::testBareLinkUriAtPositionDetectsSupportedSchemes() {
    QFETCH(QByteArray, content);
    QFETCH(int, col);
    QFETCH(QString, expected);

    TerminalWidget widget;
    widget.resize(400, 300);
    QVERIFY(widget.initialize());
    widget.importVtContent(content);
    QApplication::processEvents();
    widget.scrollViewportToOffset(0);
    QApplication::processEvents();

    QCOMPARE(widget.linkUriAtPosition(cellCenterForPos(widget, col, 0)), expected);
}

void TestTerminalWidget::testBareLinkTrimsTrailingPunctuationAndKeepsBalancedParentheses() {
    TerminalWidget widget;
    widget.resize(640, 300);
    QVERIFY(widget.initialize());
    widget.importVtContent(QByteArray("x `https://example.com/a(b)`).\n"));
    QApplication::processEvents();
    widget.scrollViewportToOffset(0);
    QApplication::processEvents();

    QCOMPARE(widget.linkUriAtPosition(cellCenterForPos(widget, 5, 0)), QStringLiteral("https://example.com/a(b)"));
}

void TestTerminalWidget::testBareLinkScanCacheReusesRowsAndPaintDoesNotScan() {
    TerminalWidget widget;
    widget.resize(400, 300);
    QVERIFY(widget.initialize());
    widget.importVtContent(QByteArray("x https://example.com\n"));
    QApplication::processEvents();
    widget.scrollViewportToOffset(0);
    QApplication::processEvents();

    const int scansBeforePaint = widget.debugBareLinkScanCount();
    renderWidgetImage(widget);
    QCOMPARE(widget.debugBareLinkScanCount(), scansBeforePaint);

    QCOMPARE(widget.linkUriAtPosition(cellCenterForPos(widget, 3, 0)), QStringLiteral("https://example.com"));
    const int scansAfterFirstQuery = widget.debugBareLinkScanCount();
    QCOMPARE(scansAfterFirstQuery, scansBeforePaint + 1);

    QCOMPARE(widget.linkUriAtPosition(cellCenterForPos(widget, 4, 0)), QStringLiteral("https://example.com"));
    QCOMPARE(widget.debugBareLinkScanCount(), scansAfterFirstQuery);
}

void TestTerminalWidget::testBareLinkHoverWorksWithoutMouseButton() {
    TerminalWidget widget;
    widget.resize(400, 300);
    QVERIFY(widget.initialize());
    widget.show();
    QApplication::processEvents();
    widget.importVtContent(QByteArray("x https://example.com\n"));
    QApplication::processEvents();
    widget.scrollViewportToOffset(0);
    QApplication::processEvents();

    QSignalSpy hoverSpy(&widget, &TerminalWidget::linkHovered);
    QVERIFY(hoverSpy.isValid());

    QMouseEvent moveEvent = mouseMoveEventFor(widget, cellCenterForPos(widget, 3, 0));
    QApplication::sendEvent(&widget, &moveEvent);
    QApplication::processEvents();

    QTRY_VERIFY(hoverSpy.count() > 0);
    QCOMPARE(hoverSpy.last().first().toString(), QStringLiteral("https://example.com"));
}

void TestTerminalWidget::testBareLinkCachedRowDoesNotRefetchText() {
    TerminalWidget widget;
    widget.resize(400, 300);
    QVERIFY(widget.initialize());
    widget.importVtContent(QByteArray("x https://example.com\n"));
    QApplication::processEvents();
    widget.scrollViewportToOffset(0);
    QApplication::processEvents();

    QCOMPARE(widget.linkUriAtPosition(cellCenterForPos(widget, 3, 0)), QStringLiteral("https://example.com"));
    const int textFetchesAfterFirstQuery = widget.debugTextForScreenRowCount();

    QCOMPARE(widget.linkUriAtPosition(cellCenterForPos(widget, 4, 0)), QStringLiteral("https://example.com"));
    QCOMPARE(widget.debugTextForScreenRowCount(), textFetchesAfterFirstQuery);
}

void TestTerminalWidget::testBareLinkHoverClearsOnScroll() {
    TerminalWidget widget;
    widget.resize(400, 60);
    QVERIFY(widget.initialize());
    widget.setMouseTracking(true);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QApplication::processEvents();

    widget.importVtContent(QByteArray("https://example.com\nline2\nline3\nline4\nline5\nline6\n"));
    QApplication::processEvents();
    widget.scrollViewportToOffset(0);
    QApplication::processEvents();
    QCOMPARE(widget.linkUriAtPosition(cellCenterForPos(widget, 3, 0)), QStringLiteral("https://example.com"));

    QSignalSpy hoverSpy(&widget, &TerminalWidget::linkHovered);
    QVERIFY(hoverSpy.isValid());

    QMouseEvent moveEvent = mouseMoveEventFor(widget, cellCenterForPos(widget, 3, 0));
    QApplication::sendEvent(&widget, &moveEvent);
    QApplication::processEvents();
    QTRY_VERIFY(hoverSpy.count() > 0);
    QCOMPARE(hoverSpy.last().first().toString(), QStringLiteral("https://example.com"));

    widget.scrollViewportBy(1);
    QApplication::processEvents();
    QTRY_VERIFY(hoverSpy.count() > 1);
    QCOMPARE(hoverSpy.last().first().toString(), QString());
}

void TestTerminalWidget::testSelectionDragAcrossBareLinkSelectsText() {
    TerminalWidget widget;
    widget.resize(640, 300);
    QVERIFY(widget.initialize());
    widget.show();
    QApplication::processEvents();
    widget.importVtContent(QByteArray("x https://example.com\n"));
    QApplication::processEvents();
    widget.scrollViewportToOffset(0);
    QApplication::processEvents();

    const QPoint startPos = cellCenterForPos(widget, 0, 0);
    QMouseEvent press(QEvent::MouseButtonPress, startPos, startPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press);

    const QPoint endPos = cellCenterForPos(widget, 21, 0);
    QMouseEvent move = mouseMoveEventFor(widget, endPos, Qt::LeftButton);
    QApplication::sendEvent(&widget, &move);

    QMouseEvent release(QEvent::MouseButtonRelease, endPos, endPos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release);
    QApplication::processEvents();

    QVERIFY(widget.debugSelectedText().contains(QStringLiteral("https://example.com")));
}

void TestTerminalWidget::testLinkUriAtPositionPrefersOsc8OverBareText() {
    TerminalWidget widget;
    widget.resize(640, 300);
    QVERIFY(widget.initialize());
    widget.importVtContent(QByteArray("\033]8;;https://osc.example\ahttps://bare.example\033]8;;\a\n"));
    QApplication::processEvents();
    widget.scrollViewportToOffset(0);
    QApplication::processEvents();

    const QPoint linkPos = cellCenterForPos(widget, 8, 0);
    QCOMPARE(widget.hyperlinkUriAtPosition(linkPos), QStringLiteral("https://osc.example"));
    QCOMPARE(widget.linkUriAtPosition(linkPos), QStringLiteral("https://osc.example"));
}

void TestTerminalWidget::testHyperlinkHoverDetection() {
    TerminalWidget widget;
    widget.resize(400, 300);
    QVERIFY(widget.initialize());
    widget.setMouseTracking(true);
    widget.show();
    QApplication::processEvents();
    widget.setFocus();
    QApplication::processEvents();

    QSignalSpy hoverSpy(&widget, &TerminalWidget::hyperlinkHovered);

    // Write OSC 8 hyperlink sequence
    QByteArray osc8("\033]8;;https://example.com\aLink\033]8;;\a\n");
    widget.importVtContent(osc8);
    QApplication::processEvents();
    widget.scrollViewportToOffset(0);
    QApplication::processEvents();

    // Move mouse over hyperlink
    QPoint hoverPos(5, 5);
    QMouseEvent moveEvent = mouseMoveEventFor(widget, hoverPos);
    QApplication::sendEvent(&widget, &moveEvent);
    QApplication::processEvents();

    QTRY_VERIFY(hoverSpy.count() > 0);
    QCOMPARE(hoverSpy.last().first().toString(), QString("https://example.com"));
}

void TestTerminalWidget::testHyperlinkCtrlClick() {
    TerminalWidget widget;
    widget.resize(400, 300);
    QVERIFY(widget.initialize());
    widget.setMouseTracking(true);
    widget.show();
    QApplication::processEvents();

    // Write OSC 8 hyperlink sequence
    QByteArray osc8("\033]8;;https://example.com\aLink\033]8;;\a\n");
    widget.importVtContent(osc8);
    QApplication::processEvents();
    widget.scrollViewportToOffset(0);
    QApplication::processEvents();

    QSignalSpy activateSpy(&widget, &TerminalWidget::hyperlinkActivated);

    // Simulate Ctrl+LeftButton click over hyperlink
    QPoint clickPos(5, 5);
    QMouseEvent pressEvent = mousePressEventFor(widget, clickPos, Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier);
    QApplication::sendEvent(&widget, &pressEvent);
    QApplication::processEvents();

    QCOMPARE(activateSpy.count(), 1);
    QCOMPARE(activateSpy.first().first().toString(), QString("https://example.com"));
}

void TestTerminalWidget::testHyperlinkCursorChange() {
    TerminalWidget widget;
    widget.resize(400, 300);
    QVERIFY(widget.initialize());
    widget.setMouseTracking(true);
    widget.show();
    QApplication::processEvents();

    // Write OSC 8 hyperlink sequence
    QByteArray osc8("\033]8;;https://example.com\aLink\033]8;;\a\n");
    widget.importVtContent(osc8);
    QApplication::processEvents();
    widget.scrollViewportToOffset(0);
    QApplication::processEvents();

    // Move mouse over hyperlink
    QPoint hoverPos(5, 5);
    QMouseEvent moveEvent = mouseMoveEventFor(widget, hoverPos);
    QApplication::sendEvent(&widget, &moveEvent);
    QApplication::processEvents();

    QCOMPARE(widget.cursor().shape(), Qt::PointingHandCursor);

    // Send leave event to reset cursor
    QEvent leaveEvent(QEvent::Leave);
    QApplication::sendEvent(&widget, &leaveEvent);
    QApplication::processEvents();

    QCOMPARE(widget.cursor().shape(), Qt::ArrowCursor);
}

void TestTerminalWidget::testHyperlinkLeaveEvent() {
    TerminalWidget widget;
    widget.resize(400, 300);
    QVERIFY(widget.initialize());
    widget.setMouseTracking(true);
    widget.show();
    QApplication::processEvents();

    QSignalSpy hoverSpy(&widget, &TerminalWidget::hyperlinkHovered);

    // Write OSC 8 hyperlink sequence
    QByteArray osc8("\033]8;;https://example.com\aLink\033]8;;\a\n");
    widget.importVtContent(osc8);
    QApplication::processEvents();
    widget.scrollViewportToOffset(0);
    QApplication::processEvents();

    // Move mouse over hyperlink
    QPoint hoverPos(5, 5);
    QMouseEvent moveEvent = mouseMoveEventFor(widget, hoverPos);
    QApplication::sendEvent(&widget, &moveEvent);
    QApplication::processEvents();

    QVERIFY(hoverSpy.count() > 0);

    // Send leave event directly
    QEvent leaveEvent(QEvent::Leave);
    QApplication::sendEvent(&widget, &leaveEvent);
    QApplication::processEvents();

    QCOMPARE(hoverSpy.last().first().toString(), QString());
}

void TestTerminalWidget::testHyperlinkUnderlinePixels() {
    TerminalWidget widget;
    widget.resize(400, 300);
    QVERIFY(widget.initialize());
    widget.setMouseTracking(true);
    widget.show();
    QApplication::processEvents();

    // Write OSC 8 hyperlink sequence
    QByteArray osc8("\033]8;;https://example.com\aLink\033]8;;\a\n");
    QSignalSpy hoverSpy(&widget, &TerminalWidget::hyperlinkHovered);

    widget.importVtContent(osc8);
    QApplication::processEvents();
    widget.scrollViewportToOffset(0);
    QApplication::processEvents();

    // Render before hover
    QImage before = renderWidgetImage(widget);

    // Move mouse over hyperlink
    QPoint hoverPos(5, 5);
    QMouseEvent moveEvent = mouseMoveEventFor(widget, hoverPos);
    QApplication::sendEvent(&widget, &moveEvent);
    QApplication::processEvents();

    // Verify hover signal was emitted
    QTRY_VERIFY(hoverSpy.count() > 0);
    QCOMPARE(hoverSpy.last().first().toString(), QString("https://example.com"));

    // Render after hover
    QImage after = renderWidgetImage(widget);

    // There should be pixel differences (underline drawn)
    QVERIFY(changedBounds(before, after).isValid());

    // Move mouse out
    QEvent leaveEvent(QEvent::Leave);
    QApplication::sendEvent(&widget, &leaveEvent);
    QApplication::processEvents();

    // Render after leave
    QImage afterLeave = renderWidgetImage(widget);

    // Should return to original state (no underline)
    QVERIFY(changedBounds(after, afterLeave).isValid());
}

void TestTerminalWidget::testHyperlinkHoverWithMouseTracking() {
    TerminalWidget widget;
    widget.resize(400, 300);
    QVERIFY(widget.initialize());
    widget.setMouseTracking(true);
    widget.show();
    QApplication::processEvents();

    QSignalSpy hoverSpy(&widget, &TerminalWidget::hyperlinkHovered);

    // Combine mouse tracking enable + hyperlink into one import
    // (importVtContent resets the terminal, so both must be in one call)
    QByteArray content("\033[?1002h\033]8;;https://example.com\aLink\033]8;;\a\n");
    widget.importVtContent(content);
    QApplication::processEvents();
    widget.scrollViewportToOffset(0);
    QApplication::processEvents();

    // Move mouse over hyperlink — should still trigger hover signal
    // even though mouse tracking is enabled
    QPoint hoverPos(5, 5);
    QMouseEvent moveEvent = mouseMoveEventFor(widget, hoverPos);
    QApplication::sendEvent(&widget, &moveEvent);
    QApplication::processEvents();

    QTRY_VERIFY(hoverSpy.count() > 0);
    QCOMPARE(hoverSpy.last().first().toString(), QString("https://example.com"));
}

// We need QApplication for QWidget tests
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    TestTerminalWidget tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_terminal_widget.moc"
