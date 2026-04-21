#pragma once

#include <QFont>
#include <QTimer>
#include <QWidget>

// Qt defines 'emit' as a no-op macro; ghostty headers use 'emit' as a struct
// member name in formatter.h, so we must undefine it before inclusion.
#ifdef emit
#undef emit
#endif

#include <ghostty/vt.h>

class PtySession;

class TerminalWidget : public QWidget {
    Q_OBJECT

public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget() override;

    bool initialize();

    void setTerminalFont(const QFont &font);
    void setCursorShape(int shape);
    void setCursorBlinkEnabled(bool blink);
    void setScrollbackLines(int lines);

signals:
    void terminalTitleChanged(const QString &title);
    void sessionClosed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool focusNextPrevChild(bool next) override;

private slots:
    void onPtyDataReceived(const QByteArray &data);
    void onPtySessionClosed();

private:
    bool setupTerminal();
    bool setupRenderState();
    bool setupEncoders();
    void updateGridSize();
    void renderTerminal(QPainter &painter);
    void sendFocusEvent(bool gained);

    GhosttyKey mapQtKeyToGhostty(int key) const;
    uint32_t unshiftedCodepointForKey(int key) const;

    // Ghostty handles
    GhosttyTerminal m_terminal = nullptr;
    GhosttyRenderState m_renderState = nullptr;
    GhosttyRenderStateRowIterator m_rowIter = nullptr;
    GhosttyRenderStateRowCells m_rowCells = nullptr;
    GhosttyKeyEncoder m_keyEncoder = nullptr;
    GhosttyKeyEvent m_keyEvent = nullptr;

    // PTY
    PtySession *m_ptySession = nullptr;

    // Font metrics
    QFont m_font;
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

    // Effects callbacks
    friend void effectWritePty(GhosttyTerminal terminal, void *userdata, const uint8_t *data, size_t len);
    friend bool effectSize(GhosttyTerminal terminal, void *userdata, GhosttySizeReportSize *out_size);
    friend bool effectDeviceAttributes(GhosttyTerminal terminal, void *userdata, GhosttyDeviceAttributes *out_attrs);
    friend GhosttyString effectXtversion(GhosttyTerminal terminal, void *userdata);
    friend void effectTitleChanged(GhosttyTerminal terminal, void *userdata);
    friend bool effectColorScheme(GhosttyTerminal terminal, void *userdata, GhosttyColorScheme *out_scheme);
};
