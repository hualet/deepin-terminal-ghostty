#pragma once

#include "PtySession.h"
#include "TerminalTheme.h"

#include <QElapsedTimer>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QInputMethodEvent>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include <optional>

// Qt defines 'emit' as a no-op macro; ghostty headers use 'emit' as a struct
// member name in formatter.h, so we must undefine it before inclusion.
#ifdef emit
#undef emit
#endif

#include <ghostty/vt.h>

class TerminalWidget : public QWidget {
    Q_OBJECT

public:
    enum class CommandState { Idle = 0, Running = 1, Succeeded = 2, Failed = 3 };
    enum class EmojiRenderMode { QtNative = 0, CustomFallback = 1 };
    enum class ProgressState { Remove = 0, Set = 1, Error = 2, Indeterminate = 3, Pause = 4 };
    Q_ENUM(ProgressState)
    struct ViewportScrollState {
        int offset = 0;
        int totalRows = 0;
        int visibleRows = 0;

        int maximumOffset() const { return qMax(0, totalRows - visibleRows); }
        bool canScroll() const { return maximumOffset() > 0; }
        bool operator==(const ViewportScrollState &other) const {
            return offset == other.offset && totalRows == other.totalRows && visibleRows == other.visibleRows;
        }
        bool operator!=(const ViewportScrollState &other) const { return !(*this == other); }
    };

    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget() override;

    bool initialize();
    int terminalColumns() const;
    int terminalRows() const;
    bool hasRunningProcess() const;
    QString workingDirectory() const;

    void setTerminalFont(const QFont &font);
    QFont terminalFont() const;
    void setCursorShape(int shape);
    void setCursorBlinkEnabled(bool blink);
    void setScrollbackLines(int lines);
    qreal opacity() const;
    void setOpacity(qreal opacity);
    void applyTheme(const TerminalTheme &theme);
    void setStartOptions(const PtySession::StartOptions &options);
    ViewportScrollState viewportScrollState() const;
    void scrollViewportBy(int deltaRows);
    void scrollViewportToOffset(int offset);

    QByteArray exportVtContent() const;
    void importVtContent(const QByteArray &data);
    QString visibleText() const;

    bool hasSelection() const;
    void selectAll();
    void copyToClipboard();
    void pasteFromClipboard();
    void writeUserInput(const QByteArray &data);
    void zoomIn();
    void zoomOut();
    void zoomReset();

    void performSearch(const QString &query);
    void clearSearch();
    void findNext();
    void findPrevious();
    bool hasSearchMatches() const;

    // Hyperlink query (public so app layer can use it without friend access)
    QString hyperlinkUriAtPosition(const QPoint &pos) const;
    QString linkUriAtPosition(const QPoint &pos) const;

#ifdef QTGHOSTTY_TESTING
    int debugLastFrameRenderedRowCount() const;
    int debugLastFrameDirtyRowCount() const;
    bool debugLastFrameWasFullRedraw() const;
    int debugLastFrameTextRunCount() const;
    int debugLastFrameLineDrawCount() const;
    int debugLastFrameEmojiFallbackDrawCount() const;
    int debugResizeApplyCount() const;
    int debugPtyFlushCount() const;
    int debugRenderStateUpdateCount() const;
    int debugPendingPtyDataSize() const;
    int debugBareLinkScanCount() const;
    int debugTextForScreenRowCount() const;
    EmojiRenderMode debugEmojiRenderMode() const;
    bool debugHasDetectedEmojiRenderMode() const;
    QString debugTextForScreenRow(int row) const;
    int debugHoverLinkStartCol() const;
    int debugHoverLinkEndCol() const;
    int debugCursorOnlyRepaintCount() const;
    int debugScrollbackLines() const;
    size_t debugScrollbackByteBudget() const;
    int debugUnicodeTextCellWidth(const QString &text) const;
    void debugSetSelection(int startRow, int startCol, int endRow, int endCol, bool active);
    bool debugCellInSelection(int screenRow, int col) const;
    QString debugSelectedText() const;
    bool debugAppliedIsDark() const;
    QColor debugAppliedForeground() const;
    QColor debugAppliedBackground() const;
    QColor debugAppliedPaletteColor(int index) const;
    void debugSetRawTerminalFont(const QFont &font);
    void debugSetEmojiRenderModeForTesting(EmojiRenderMode mode);
#endif

signals:
    void terminalTitleChanged(const QString &title);
    void workingDirectoryChanged(const QString &workingDirectory);
    void shellCommandChanged(const QString &command);
    void sessionExited(int exitCode);
    void sessionClosed();
    void focusGained();
    void commandStateChanged(CommandState state);
    void viewportScrollStateChanged();
    void hyperlinkHovered(const QString &uri);
    void hyperlinkActivated(const QString &uri);
    void linkHovered(const QString &uri);
    void linkActivated(const QString &uri);
    void desktopNotificationRequested(const QString &title, const QString &body);
    void progressChanged(ProgressState state, int progress);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool focusNextPrevChild(bool next) override;

    virtual void setSelectionClipboardText(const QString &text);

private slots:
    void onPtyDataReceived(const QByteArray &data);
    void onPtySessionClosed();
    void onRenderTimerTimeout();

private:
    bool setupTerminal();
    bool setupRenderState();
    bool setupEncoders();
    bool syncRenderState() const;
    bool ensureBackBuffer();
    void applyPendingResize();
    void updateCachedFonts();
    void updateGridSize();
    void renderTerminal(QPainter &painter);
    void renderOverlays(QPainter &painter) const;
    enum class RowRenderPass { Background, Text, Full };
    void renderRow(QPainter &painter, int y, const GhosttyRenderStateColors &colors, RowRenderPass pass);
    QImage emojiFallbackCellImage(QPainter &painter, const QRect &cellRect, const QString &text);
    bool drawEmojiFallback(QPainter &painter, const QRect &cellRect, const QString &text);
    EmojiRenderMode emojiRenderMode() const;
    QString hyperlinkUriAtViewportCell(int viewportRow, int col) const;
    QString hyperlinkUriForPoint(GhosttyPoint point) const;
    struct LinkRange {
        QString uri;
        int screenRow = 0;
        int startCol = 0;
        int endCol = 0;
        bool osc8 = false;
        bool isValid() const { return !uri.isEmpty() && startCol < endCol; }
    };
    struct LinkScanCacheEntry {
        QString text;
        QVector<int> cellOfChar;
        QVector<LinkRange> ranges;
        quint64 generation = 0;
    };
    LinkRange hyperlinkRangeAtPosition(const QPoint &pos) const;
    LinkRange linkRangeAtPosition(const QPoint &pos) const;
    LinkRange bareLinkRangeAtCell(int screenRow, int col) const;
    QVector<LinkRange> scanBareLinksInRow(int screenRow, const QString &text, const QVector<int> &cellOfChar) const;
    void touchLinkScanCacheRow(int screenRow) const;
    void clearLinkScanCache();
    void invalidateLinkScanCache();
    void clearHoverLink();
    void updateHyperlinkHoverState(const QPoint &pos, bool changeCursor = true);
    bool renderKittyGraphicsLayer(QPainter &painter, GhosttyKittyPlacementLayer layer);
    bool renderKittyPlacement(QPainter &painter, GhosttyKittyGraphics graphics);
    QImage imageForKittyImage(GhosttyKittyGraphicsImage image);
    void renderPreeditText(QPainter &painter);
    int unicodeTextCellWidth(const QString &text) const;
    void sendFocusEvent(bool gained);
    QRect inputMethodCursorRect() const;
    QRect terminalContentRect() const;
    QPoint terminalContentOrigin() const;
    QPoint terminalContentPosition(const QPoint &position) const;
    int viewportColumnForPosition(const QPoint &position) const;
    int viewportRowForPosition(const QPoint &position) const;
    GhosttyMousePosition ghosttyMousePositionForEvent(const QPointF &position) const;
    void notifyInputMethodCursorChange();
    void scheduleTerminalRepaint();
    bool flushPendingPtyData(QRect *repaintRegion = nullptr);
    void updateAfterPtyFlush(const QRect &repaintRegion);
    QRect cursorPaintRect() const;
    void clearRenderStateDirtyRows();
    size_t scrollbackByteBudget() const;
    bool applyScrollbackLimits();
    void scheduleScrollbackCompression();
    void runScrollbackCompressionStep();
    void scanShellIntegrationSequences(const QByteArray &data);
    void setShellCommand(const QString &command);
    void setShellCommandResult(int exitCode);
    void updateCommandState(CommandState newState);
    ViewportScrollState queryViewportScrollState() const;
    void updateViewportScrollState();
    void scrollViewportToBottom();

    void updateSearchHighlight();
    void scrollToSearchMatch();
    QString textForScreenRow(int row) const;
    QString textForScreenRowWithCellMap(int row, QVector<int> &cellOfChar) const;
    QString selectedText() const;

    bool isWordBoundary(uint32_t codepoint) const;
    uint32_t codepointAtCell(int screenRow, int col) const;
    GhosttyCellWide cellWidthAtCell(int screenRow, int col) const;
    bool rowFlagAt(int screenRow, GhosttyRowData data) const;
    bool isRowWrapped(int screenRow) const;
    bool isRowContinuation(int screenRow) const;
    void wordBoundsAt(int screenRow, int col, int *startCol, int *endCol) const;
    void wordRangeAt(int screenRow, int col, int *startRow, int *startCol, int *endRow, int *endCol) const;
    void selectWordAt(int screenRow, int col);
    void selectLineAt(int screenRow);
    void extendWordSelection(int screenRow, int col);
    void extendLineSelection(int screenRow);
    void extendSelectionToPosition(const QPoint &position);
    void updateSelectionAutoScroll(const QPoint &position);
    void stopSelectionAutoScroll();
    void handleSelectionAutoScroll();

    int screenRowForViewportRow(int viewportRow) const;
    bool cellInSelection(int screenRow, int col) const;

    GhosttyKey mapQtKeyToGhostty(int key) const;
    uint32_t unshiftedCodepointForKey(int key) const;

    // Ghostty handles
    GhosttyTerminal m_terminal = nullptr;
    GhosttyRenderState m_renderState = nullptr;
    GhosttyRenderStateRowIterator m_rowIter = nullptr;
    GhosttyRenderStateRowCells m_rowCells = nullptr;
    GhosttyKittyGraphicsPlacementIterator m_kittyPlacementIter = nullptr;
    GhosttyKeyEncoder m_keyEncoder = nullptr;
    GhosttyKeyEvent m_keyEvent = nullptr;
    GhosttyMouseEncoder m_mouseEncoder = nullptr;
    GhosttyMouseEvent m_mouseEvent = nullptr;
    mutable bool m_renderStateDirty = true;

    // PTY
    PtySession *m_ptySession = nullptr;
    PtySession::StartOptions m_startOptions;
    bool m_hasStartOptions = false;
    QByteArray m_oscScanBuffer;
    QString m_terminalWorkingDirectory;
    int m_pendingExitCode = -1;
    CommandState m_commandState = CommandState::Idle;
    QByteArray m_pendingPtyData;
    QHash<uint32_t, QImage> m_kittyImageCache;
    std::optional<uint64_t> m_kittyGraphicsGeneration;
    QHash<QString, QImage> m_emojiImageCache;
    QStringList m_emojiImageCacheOrder;
    QTimer *m_renderTimer = nullptr;
    QTimer *m_synchronizedOutputTimeoutTimer = nullptr;
    QTimer *m_resizeTimer = nullptr;
    QTimer *m_selectionAutoScrollTimer = nullptr;
    QTimer *m_scrollbackCompressionTimer = nullptr;
    QElapsedTimer m_lastRenderTime;
    QImage m_backBuffer;
    int m_backBufferViewportOffset = 0;
    uint16_t m_pendingResizeCols = 80;
    uint16_t m_pendingResizeRows = 24;

    // Font metrics
    QFont m_font;
    QFont m_fontBold;
    QFont m_fontItalic;
    QFont m_fontBoldItalic;
    int m_cellWidth = 0;
    int m_cellHeight = 0;
    int m_fontAscent = 0;
    mutable std::optional<EmojiRenderMode> m_detectedEmojiRenderMode;
#ifdef QTGHOSTTY_TESTING
    std::optional<EmojiRenderMode> m_forcedEmojiRenderMode;
#endif

    // Grid size
    uint16_t m_cols = 80;
    uint16_t m_rows = 24;

    // Focus tracking
    bool m_hasFocus = false;

    // Cursor settings
    int m_cursorShape = 0;
    bool m_cursorBlinkEnabled = true;
    bool m_cursorBlinkVisible = true;
    QTimer *m_blinkTimer = nullptr;

    // Scrollback
    int m_scrollbackLines = 5000;
    ViewportScrollState m_viewportScrollState;
    std::optional<uint64_t> m_scrollbackCompressionActivity;
    bool m_scrollbackCompressionAvailable = true;
    bool m_isDark = true;
    qreal m_opacity = 1.0;

    // Selection colors (from theme)
    QColor m_selectionBackground;

    // Input method composition
    QString m_preeditText;

    // Search
    struct SearchMatch {
        int row = 0;
        int startCol = 0;
        int endCol = 0;
    };
    QVector<SearchMatch> m_searchMatches;
    int m_currentSearchIndex = -1;

    // Selection (screen coordinates)
    struct Selection {
        int startRow = 0;
        int startCol = 0;
        int endRow = 0;
        int endCol = 0;
        bool active = false;
    };
    Selection m_selection;
    QString m_hoverHyperlinkUri;
    LinkRange m_hoverLink;
    bool m_hoverLinkActive = false;
    mutable QHash<int, LinkScanCacheEntry> m_linkScanCache;
    mutable QVector<int> m_linkScanLru;
    quint64 m_linkScanGeneration = 1;
    QPoint m_lastMousePos;
    QPoint m_selectionDragStartPos;
    bool m_selectionDragActive = false;

    // Multi-click detection
    enum class ClickMode { Single, Word, Line };
    ClickMode m_clickMode = ClickMode::Single;
    int m_clickCount = 0;
    QPoint m_lastClickPos;
    QElapsedTimer m_clickTimer;
    int m_clickAnchorRow = 0;
    int m_clickAnchorCol = 0;

#ifdef QTGHOSTTY_TESTING
    int m_debugLastFrameRenderedRowCount = 0;
    int m_debugLastFrameDirtyRowCount = 0;
    bool m_debugLastFrameWasFullRedraw = false;
    int m_debugLastFrameTextRunCount = 0;
    int m_debugLastFrameLineDrawCount = 0;
    int m_debugLastFrameEmojiFallbackDrawCount = 0;
    int m_debugResizeApplyCount = 0;
    int m_debugPtyFlushCount = 0;
    mutable int m_debugRenderStateUpdateCount = 0;
    int m_debugCursorOnlyRepaintCount = 0;
    mutable int m_debugBareLinkScanCount = 0;
    mutable int m_debugTextForScreenRowCount = 0;
    int m_debugScrollbackCompressionStepCount = 0;
    int m_debugKittyImageConversionCount = 0;
#endif

    // Effects callbacks
    friend void effectWritePty(GhosttyTerminal terminal, void *userdata, const uint8_t *data, size_t len);
    friend bool effectSize(GhosttyTerminal terminal, void *userdata, GhosttySizeReportSize *out_size);
    friend bool effectDeviceAttributes(GhosttyTerminal terminal, void *userdata, GhosttyDeviceAttributes *out_attrs);
    friend GhosttyString effectXtversion(GhosttyTerminal terminal, void *userdata);
    friend void effectTitleChanged(GhosttyTerminal terminal, void *userdata);
    friend void effectPwdChanged(GhosttyTerminal terminal, void *userdata);
    friend bool effectColorScheme(GhosttyTerminal terminal, void *userdata, GhosttyColorScheme *out_scheme);
};
