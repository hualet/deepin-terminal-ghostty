#pragma once

#include "PtySession.h"
#include "TerminalTheme.h"

#include <QElapsedTimer>
#include <QFont>
#include <QImage>
#include <QInputMethodEvent>
#include <QTimer>
#include <QWidget>

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

    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget() override;

    bool initialize();
    int terminalColumns() const;
    int terminalRows() const;
    bool hasRunningProcess() const;

    void setTerminalFont(const QFont &font);
    QFont terminalFont() const;
    void setCursorShape(int shape);
    void setCursorBlinkEnabled(bool blink);
    void setScrollbackLines(int lines);
    qreal opacity() const;
    void setOpacity(qreal opacity);
    void applyTheme(const TerminalTheme &theme);
    void setStartOptions(const PtySession::StartOptions &options);

    void selectAll();
    void zoomIn();
    void zoomOut();
    void zoomReset();

    void performSearch(const QString &query);
    void clearSearch();
    void findNext();
    void findPrevious();
    bool hasSearchMatches() const;

#ifdef QTGHOSTTY_TESTING
    int debugLastFrameRenderedRowCount() const;
    int debugLastFrameDirtyRowCount() const;
    bool debugLastFrameWasFullRedraw() const;
    int debugLastFrameTextRunCount() const;
    int debugResizeApplyCount() const;
    int debugPtyFlushCount() const;
    int debugCursorOnlyRepaintCount() const;
    void debugSetSelection(int startRow, int startCol, int endRow, int endCol, bool active);
    bool debugCellInSelection(int screenRow, int col) const;
    QString debugSelectedText() const;
    bool debugAppliedIsDark() const;
    QColor debugAppliedForeground() const;
    QColor debugAppliedBackground() const;
#endif

signals:
    void terminalTitleChanged(const QString &title);
    void shellCommandChanged(const QString &command);
    void sessionExited(int exitCode);
    void sessionClosed();
    void focusGained();
    void commandStateChanged(CommandState state);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool focusNextPrevChild(bool next) override;

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
    void renderRow(QPainter &painter, int y, const GhosttyRenderStateColors &colors);
    void renderPreeditText(QPainter &painter);
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
    void scanShellIntegrationSequences(const QByteArray &data);
    void setShellCommand(const QString &command);
    void setShellCommandResult(int exitCode);
    void updateCommandState(CommandState newState);

    void updateSearchHighlight();
    QString textForScreenRow(int row) const;
    QString selectedText() const;
    void copyToClipboard();
    void pasteFromClipboard();
    bool hasSelection() const;

    bool isWordChar(uint32_t codepoint) const;
    void wordBoundsAt(int screenRow, int col, int *startCol, int *endCol) const;
    void selectWordAt(int screenRow, int col);
    void selectLineAt(int screenRow);
    void extendWordSelection(int screenRow, int col);
    void extendLineSelection(int screenRow);

    int screenRowForViewportRow(int viewportRow) const;
    bool cellInSelection(int screenRow, int col) const;

    GhosttyKey mapQtKeyToGhostty(int key) const;
    uint32_t unshiftedCodepointForKey(int key) const;

    // Ghostty handles
    GhosttyTerminal m_terminal = nullptr;
    GhosttyRenderState m_renderState = nullptr;
    GhosttyRenderStateRowIterator m_rowIter = nullptr;
    GhosttyRenderStateRowCells m_rowCells = nullptr;
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
    int m_pendingExitCode = -1;
    CommandState m_commandState = CommandState::Idle;
    QByteArray m_pendingPtyData;
    QTimer *m_renderTimer = nullptr;
    QTimer *m_resizeTimer = nullptr;
    QElapsedTimer m_lastRenderTime;
    QImage m_backBuffer;
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
    int m_scrollbackLines = 1000;
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
    int m_debugResizeApplyCount = 0;
    int m_debugPtyFlushCount = 0;
    int m_debugCursorOnlyRepaintCount = 0;
#endif

    friend class TermPane;

    // Effects callbacks
    friend void effectWritePty(GhosttyTerminal terminal, void *userdata, const uint8_t *data, size_t len);
    friend bool effectSize(GhosttyTerminal terminal, void *userdata, GhosttySizeReportSize *out_size);
    friend bool effectDeviceAttributes(GhosttyTerminal terminal, void *userdata, GhosttyDeviceAttributes *out_attrs);
    friend GhosttyString effectXtversion(GhosttyTerminal terminal, void *userdata);
    friend void effectTitleChanged(GhosttyTerminal terminal, void *userdata);
    friend bool effectColorScheme(GhosttyTerminal terminal, void *userdata, GhosttyColorScheme *out_scheme);
};
