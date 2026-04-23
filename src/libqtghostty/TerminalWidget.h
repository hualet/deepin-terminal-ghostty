#pragma once

#include "PtySession.h"

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
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget() override;

    bool initialize();
    int terminalColumns() const;
    int terminalRows() const;

    void setTerminalFont(const QFont &font);
    void setCursorShape(int shape);
    void setCursorBlinkEnabled(bool blink);
    void setScrollbackLines(int lines);
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
    int debugResizeApplyCount() const;
    int debugPtyFlushCount() const;
#endif

signals:
    void terminalTitleChanged(const QString &title);
    void sessionExited(int exitCode);
    void sessionClosed();
    void focusGained();

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
    void ensureBackBuffer();
    void applyPendingResize();
    void updateCachedFonts();
    void updateGridSize();
    void renderTerminal(QPainter &painter);
    void renderOverlays(QPainter &painter) const;
    void renderRow(QPainter &painter, int y, const GhosttyRenderStateColors &colors);
    void renderPreeditText(QPainter &painter);
    void sendFocusEvent(bool gained);
    QRect inputMethodCursorRect() const;
    void notifyInputMethodCursorChange();
    void scheduleTerminalRepaint();
    void flushPendingPtyData();

    void updateSearchHighlight();
    QString textForScreenRow(int row) const;
    QString selectedText() const;
    void copyToClipboard();
    void pasteFromClipboard();
    bool hasSelection() const;

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
    mutable bool m_renderStateDirty = true;

    // PTY
    PtySession *m_ptySession = nullptr;
    PtySession::StartOptions m_startOptions;
    bool m_hasStartOptions = false;
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

#ifdef QTGHOSTTY_TESTING
    int m_debugLastFrameRenderedRowCount = 0;
    int m_debugLastFrameDirtyRowCount = 0;
    bool m_debugLastFrameWasFullRedraw = false;
    int m_debugResizeApplyCount = 0;
    int m_debugPtyFlushCount = 0;
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
