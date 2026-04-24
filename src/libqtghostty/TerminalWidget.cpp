#include "TerminalWidget.h"

#include "PtySession.h"
#include "logging/Logging.h"

#include <QClipboard>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QInputMethod>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cstdio>

namespace {

constexpr int kInitialPtyCoalesceIntervalMs = 1;
constexpr int kBurstRenderIntervalMs = 8;
constexpr int kResizeCoalesceIntervalMs = 8;
constexpr int kImmediatePtyFlushBytes = 32 * 1024;

void appendCodepoint(QString &text, uint32_t codepoint) {
    if (codepoint <= 0xFFFF) {
        text.append(QChar(static_cast<ushort>(codepoint)));
        return;
    }

    if (codepoint <= 0x10FFFF) {
        text.append(QChar::fromUcs4(codepoint));
        return;
    }

    text.append(QChar::ReplacementCharacter);
}

} // namespace

// ---------------------------------------------------------------------------
// Effect callbacks (C-linkage friends of TerminalWidget)
// ---------------------------------------------------------------------------

void effectWritePty(GhosttyTerminal terminal, void *userdata, const uint8_t *data, size_t len) {
    (void)terminal;
    auto *widget = static_cast<TerminalWidget *>(userdata);
    if (widget->m_ptySession && len > 0) {
        widget->m_ptySession->write(QByteArray(reinterpret_cast<const char *>(data), static_cast<int>(len)));
    }
}

bool effectSize(GhosttyTerminal terminal, void *userdata, GhosttySizeReportSize *out_size) {
    (void)terminal;
    auto *widget = static_cast<TerminalWidget *>(userdata);
    out_size->rows = widget->m_rows;
    out_size->columns = widget->m_cols;
    out_size->cell_width = static_cast<uint32_t>(widget->m_cellWidth);
    out_size->cell_height = static_cast<uint32_t>(widget->m_cellHeight);
    return true;
}

bool effectDeviceAttributes(GhosttyTerminal terminal, void *userdata, GhosttyDeviceAttributes *out_attrs) {
    (void)terminal;
    (void)userdata;
    out_attrs->primary.conformance_level = GHOSTTY_DA_CONFORMANCE_VT220;
    out_attrs->primary.features[0] = GHOSTTY_DA_FEATURE_COLUMNS_132;
    out_attrs->primary.features[1] = GHOSTTY_DA_FEATURE_SELECTIVE_ERASE;
    out_attrs->primary.features[2] = GHOSTTY_DA_FEATURE_ANSI_COLOR;
    out_attrs->primary.num_features = 3;
    out_attrs->secondary.device_type = GHOSTTY_DA_DEVICE_TYPE_VT220;
    out_attrs->secondary.firmware_version = 1;
    out_attrs->secondary.rom_cartridge = 0;
    out_attrs->tertiary.unit_id = 0;
    return true;
}

GhosttyString effectXtversion(GhosttyTerminal terminal, void *userdata) {
    (void)terminal;
    (void)userdata;
    static const char name[] = "deepin-terminal-ghostty";
    return GhosttyString{
        .ptr = reinterpret_cast<const uint8_t *>(name),
        .len = sizeof(name) - 1,
    };
}

void effectTitleChanged(GhosttyTerminal terminal, void *userdata) {
    auto *widget = static_cast<TerminalWidget *>(userdata);
    GhosttyString title = {};
    if (ghostty_terminal_get(terminal, GHOSTTY_TERMINAL_DATA_TITLE, &title) == GHOSTTY_SUCCESS) {
        QString qTitle = QString::fromUtf8(reinterpret_cast<const char *>(title.ptr), static_cast<int>(title.len));
        widget->terminalTitleChanged(qTitle);
    }
}

bool effectColorScheme(GhosttyTerminal terminal, void *userdata, GhosttyColorScheme *out_scheme) {
    (void)terminal;
    (void)userdata;
    (void)out_scheme;
    return false;
}

// ---------------------------------------------------------------------------
// TerminalWidget
// ---------------------------------------------------------------------------

TerminalWidget::TerminalWidget(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled, true);

    m_font = QFont("Monospace", 11);
    m_font.setStyleHint(QFont::Monospace);
    m_font.setFixedPitch(true);

    QFontMetrics fm(m_font);
    m_cellWidth = fm.horizontalAdvance('M');
    m_cellHeight = fm.height();
    m_fontAscent = fm.ascent();
    updateCachedFonts();

    m_blinkTimer = new QTimer(this);
    m_blinkTimer->setInterval(500);
    connect(m_blinkTimer, &QTimer::timeout, this, [this]() {
        m_cursorBlinkVisible = !m_cursorBlinkVisible;
        update();
    });

    m_renderTimer = new QTimer(this);
    m_renderTimer->setSingleShot(true);
    connect(m_renderTimer, &QTimer::timeout, this, &TerminalWidget::onRenderTimerTimeout);

    m_resizeTimer = new QTimer(this);
    m_resizeTimer->setSingleShot(true);
    connect(m_resizeTimer, &QTimer::timeout, this, [this]() { applyPendingResize(); });
}

TerminalWidget::~TerminalWidget() {
    if (m_keyEvent)
        ghostty_key_event_free(m_keyEvent);
    if (m_keyEncoder)
        ghostty_key_encoder_free(m_keyEncoder);
    if (m_rowCells)
        ghostty_render_state_row_cells_free(m_rowCells);
    if (m_rowIter)
        ghostty_render_state_row_iterator_free(m_rowIter);
    if (m_renderState)
        ghostty_render_state_free(m_renderState);
    if (m_terminal)
        ghostty_terminal_free(m_terminal);
}

bool TerminalWidget::initialize() {
    updateGridSize();

    if (!setupTerminal()) {
        qCCritical(terminalLog) << "Failed to initialize Ghostty terminal";
        return false;
    }
    if (!setupRenderState()) {
        qCCritical(terminalLog) << "Failed to initialize Ghostty render state";
        return false;
    }
    if (!setupEncoders()) {
        qCCritical(terminalLog) << "Failed to initialize Ghostty key encoder state";
        return false;
    }

    m_ptySession = new PtySession(this);
    connect(m_ptySession, &PtySession::dataReceived, this, &TerminalWidget::onPtyDataReceived);
    connect(m_ptySession, &PtySession::childExited, this, &TerminalWidget::sessionExited);
    connect(m_ptySession, &PtySession::sessionClosed, this, &TerminalWidget::onPtySessionClosed);

    const PtySession::StartOptions options = m_hasStartOptions ? m_startOptions : PtySession::StartOptions{};
    if (!m_ptySession->start(m_cols, m_rows, options)) {
        qCCritical(terminalLog) << "Failed to start PTY session for terminal widget";
        return false;
    }

    qCInfo(terminalLog) << "Terminal widget initialized with size" << m_cols << "x" << m_rows;
    return true;
}

int TerminalWidget::terminalColumns() const {
    return m_cols;
}

int TerminalWidget::terminalRows() const {
    return m_rows;
}

void TerminalWidget::updateCachedFonts() {
    m_fontBold = m_font;
    m_fontBold.setBold(true);

    m_fontItalic = m_font;
    m_fontItalic.setItalic(true);

    m_fontBoldItalic = m_font;
    m_fontBoldItalic.setBold(true);
    m_fontBoldItalic.setItalic(true);
}

#ifdef QTGHOSTTY_TESTING
int TerminalWidget::debugLastFrameRenderedRowCount() const {
    return m_debugLastFrameRenderedRowCount;
}

int TerminalWidget::debugLastFrameDirtyRowCount() const {
    return m_debugLastFrameDirtyRowCount;
}

bool TerminalWidget::debugLastFrameWasFullRedraw() const {
    return m_debugLastFrameWasFullRedraw;
}

int TerminalWidget::debugResizeApplyCount() const {
    return m_debugResizeApplyCount;
}

int TerminalWidget::debugPtyFlushCount() const {
    return m_debugPtyFlushCount;
}
#endif

bool TerminalWidget::setupTerminal() {
    GhosttyTerminalOptions opts = {
        .cols = m_cols,
        .rows = m_rows,
        .max_scrollback = static_cast<uint32_t>(m_scrollbackLines),
    };

    GhosttyResult err = ghostty_terminal_new(nullptr, &m_terminal, opts);
    if (err != GHOSTTY_SUCCESS) {
        std::fprintf(stderr, "ghostty_terminal_new failed (%d)\n", err);
        return false;
    }

    ghostty_terminal_resize(m_terminal, m_cols, m_rows, static_cast<uint32_t>(m_cellWidth),
                            static_cast<uint32_t>(m_cellHeight));

    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_USERDATA, this);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_WRITE_PTY, reinterpret_cast<const void *>(effectWritePty));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_SIZE, reinterpret_cast<const void *>(effectSize));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_DEVICE_ATTRIBUTES,
                         reinterpret_cast<const void *>(effectDeviceAttributes));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_XTVERSION, reinterpret_cast<const void *>(effectXtversion));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_TITLE_CHANGED,
                         reinterpret_cast<const void *>(effectTitleChanged));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_SCHEME,
                         reinterpret_cast<const void *>(effectColorScheme));

    uint64_t kittyLimit = 64 * 1024 * 1024;
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_KITTY_IMAGE_STORAGE_LIMIT, &kittyLimit);

    return true;
}

bool TerminalWidget::setupRenderState() {
    GhosttyResult err = ghostty_render_state_new(nullptr, &m_renderState);
    if (err != GHOSTTY_SUCCESS) {
        std::fprintf(stderr, "ghostty_render_state_new failed (%d)\n", err);
        return false;
    }

    err = ghostty_render_state_row_iterator_new(nullptr, &m_rowIter);
    if (err != GHOSTTY_SUCCESS) {
        std::fprintf(stderr, "ghostty_render_state_row_iterator_new failed (%d)\n", err);
        return false;
    }

    err = ghostty_render_state_row_cells_new(nullptr, &m_rowCells);
    if (err != GHOSTTY_SUCCESS) {
        std::fprintf(stderr, "ghostty_render_state_row_cells_new failed (%d)\n", err);
        return false;
    }

    return true;
}

bool TerminalWidget::setupEncoders() {
    GhosttyResult err = ghostty_key_encoder_new(nullptr, &m_keyEncoder);
    if (err != GHOSTTY_SUCCESS) {
        std::fprintf(stderr, "ghostty_key_encoder_new failed (%d)\n", err);
        return false;
    }

    err = ghostty_key_event_new(nullptr, &m_keyEvent);
    if (err != GHOSTTY_SUCCESS) {
        std::fprintf(stderr, "ghostty_key_event_new failed (%d)\n", err);
        return false;
    }

    return true;
}

void TerminalWidget::updateGridSize() {
    int w = width();
    int h = height();
    if (m_cellWidth <= 0 || m_cellHeight <= 0)
        return;

    // Layout reparenting can transiently resize the terminal to 0x0 or 1x1.
    // Treat those geometries as unstable and keep the previous grid size so we
    // do not send a destructive PTY resize during layout switches.
    if (w < m_cellWidth || h < m_cellHeight)
        return;

    uint16_t cols = static_cast<uint16_t>(w / m_cellWidth);
    uint16_t rows = static_cast<uint16_t>(h / m_cellHeight);
    if (cols < 1)
        cols = 1;
    if (rows < 1)
        rows = 1;

    if (cols != m_cols || rows != m_rows) {
        m_pendingResizeCols = cols;
        m_pendingResizeRows = rows;
        if (!m_terminal && !m_ptySession) {
            m_cols = cols;
            m_rows = rows;
        } else if (m_resizeTimer) {
            m_resizeTimer->start(kResizeCoalesceIntervalMs);
        }
    }
}

void TerminalWidget::paintEvent(QPaintEvent *event) {
    (void)event;
    if (!m_terminal || !m_renderState)
        return;

    QPainter painter(this);
    painter.setFont(m_font);

    renderTerminal(painter);
    renderOverlays(painter);
    renderPreeditText(painter);
}

bool TerminalWidget::syncRenderState() const {
    if (!m_renderStateDirty) {
        return true;
    }

    GhosttyResult err = ghostty_render_state_update(m_renderState, m_terminal);
    if (err != GHOSTTY_SUCCESS) {
        return false;
    }

    m_renderStateDirty = false;
    return true;
}

void TerminalWidget::ensureBackBuffer() {
    const qreal devicePixelRatio = devicePixelRatioF();
    const QSize pixelSize(qMax(1, qRound(width() * devicePixelRatio)), qMax(1, qRound(height() * devicePixelRatio)));
    if (pixelSize.isEmpty()) {
        m_backBuffer = QImage();
        return;
    }

    if (m_backBuffer.size() == pixelSize && qFuzzyCompare(m_backBuffer.devicePixelRatio(), devicePixelRatio)) {
        return;
    }

    m_backBuffer = QImage(pixelSize, QImage::Format_ARGB32_Premultiplied);
    m_backBuffer.setDevicePixelRatio(devicePixelRatio);
    m_backBuffer.fill(Qt::transparent);
}

void TerminalWidget::renderTerminal(QPainter &painter) {
    if (!syncRenderState())
        return;

    GhosttyResult err = GHOSTTY_SUCCESS;
    GhosttyRenderStateColors colors = GHOSTTY_INIT_SIZED(GhosttyRenderStateColors);
    err = ghostty_render_state_colors_get(m_renderState, &colors);
    if (err != GHOSTTY_SUCCESS)
        return;

    painter.fillRect(rect(), QColor(colors.background.r, colors.background.g, colors.background.b));

    err = ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &m_rowIter);
    if (err != GHOSTTY_SUCCESS)
        return;

    ensureBackBuffer();
    if (m_backBuffer.isNull())
        return;

    GhosttyRenderStateDirty dirtyState = GHOSTTY_RENDER_STATE_DIRTY_FULL;
    ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_DIRTY, &dirtyState);

    const bool fullRedraw = dirtyState == GHOSTTY_RENDER_STATE_DIRTY_FULL;
    if (!fullRedraw && dirtyState == GHOSTTY_RENDER_STATE_DIRTY_FALSE) {
        painter.drawImage(QPoint(0, 0), m_backBuffer);
        return;
    }

    QPainter backPainter(&m_backBuffer);
    backPainter.setFont(m_font);
    if (fullRedraw) {
        backPainter.fillRect(rect(), QColor(colors.background.r, colors.background.g, colors.background.b));
    }

#ifdef QTGHOSTTY_TESTING
    m_debugLastFrameRenderedRowCount = 0;
    m_debugLastFrameDirtyRowCount = 0;
    m_debugLastFrameWasFullRedraw = fullRedraw;
#endif

    int y = 0;
    while (ghostty_render_state_row_iterator_next(m_rowIter)) {
        bool rowDirty = true;
        ghostty_render_state_row_get(m_rowIter, GHOSTTY_RENDER_STATE_ROW_DATA_DIRTY, &rowDirty);
        if (!fullRedraw && !rowDirty) {
            y += m_cellHeight;
            continue;
        }
#ifdef QTGHOSTTY_TESTING
        if (rowDirty)
            ++m_debugLastFrameDirtyRowCount;
        ++m_debugLastFrameRenderedRowCount;
#endif

        renderRow(backPainter, y, colors);

        bool clean = false;
        ghostty_render_state_row_set(m_rowIter, GHOSTTY_RENDER_STATE_ROW_OPTION_DIRTY, &clean);

        y += m_cellHeight;
    }
    GhosttyRenderStateDirty cleanState = GHOSTTY_RENDER_STATE_DIRTY_FALSE;
    ghostty_render_state_set(m_renderState, GHOSTTY_RENDER_STATE_OPTION_DIRTY, &cleanState);
    painter.drawImage(QPoint(0, 0), m_backBuffer);
}

void TerminalWidget::renderRow(QPainter &painter, int y, const GhosttyRenderStateColors &colors) {
    if (ghostty_render_state_row_get(m_rowIter, GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &m_rowCells) != GHOSTTY_SUCCESS)
        return;

    const QColor defaultBackground(colors.background.r, colors.background.g, colors.background.b);
    const QColor defaultForeground(colors.foreground.r, colors.foreground.g, colors.foreground.b);
    painter.fillRect(0, y, width(), m_cellHeight, defaultBackground);

    constexpr GhosttyRenderStateRowCellsData kCellDataKeys[] = {
        GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_RAW,
        GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_STYLE,
        GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_LEN,
    };

    QString runText;
    const QFont *runFont = nullptr;
    QColor runForeground;
    int runStartX = 0;
    int runCellCount = 0;
    auto flushTextRun = [&]() {
        if (runCellCount == 0 || runText.isEmpty() || !runFont)
            return;

        painter.setFont(*runFont);
        painter.setPen(runForeground);
        painter.setLayoutDirection(Qt::LeftToRight);
        painter.drawText(runStartX, y + m_fontAscent, runText);

        runText.clear();
        runFont = nullptr;
        runCellCount = 0;
    };

    int x = 0;
    while (ghostty_render_state_row_cells_next(m_rowCells)) {
        GhosttyCell rawCell = 0;
        GhosttyStyle style = GHOSTTY_INIT_SIZED(GhosttyStyle);
        uint32_t graphemeLen = 0;
        void *cellDataValues[] = {&rawCell, &style, &graphemeLen};
        size_t written = 0;
        const GhosttyResult multiResult = ghostty_render_state_row_cells_get_multi(
            m_rowCells, std::size(kCellDataKeys), kCellDataKeys, cellDataValues, &written);
        if (multiResult != GHOSTTY_SUCCESS || written != std::size(kCellDataKeys))
            continue;

        GhosttyColorRgb bgColor = colors.background;
        const bool hasBg =
            ghostty_render_state_row_cells_get(m_rowCells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_BG_COLOR, &bgColor)
            == GHOSTTY_SUCCESS;

        GhosttyColorRgb fgColor = colors.foreground;
        const bool hasFg =
            ghostty_render_state_row_cells_get(m_rowCells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_FG_COLOR, &fgColor)
            == GHOSTTY_SUCCESS;

        GhosttyCellWide cellWide = GHOSTTY_CELL_WIDE_NARROW;
        ghostty_cell_get(rawCell, GHOSTTY_CELL_DATA_WIDE, &cellWide);
        const bool isWideHead = cellWide == GHOSTTY_CELL_WIDE_WIDE;
        const bool isWideTail = cellWide == GHOSTTY_CELL_WIDE_SPACER_TAIL || cellWide == GHOSTTY_CELL_WIDE_SPACER_HEAD;
        const int cellRenderWidth = isWideHead ? m_cellWidth * 2 : m_cellWidth;

        if (style.inverse) {
            std::swap(fgColor, bgColor);
            if (!isWideTail)
                painter.fillRect(x, y, cellRenderWidth, m_cellHeight, QColor(bgColor.r, bgColor.g, bgColor.b));
        } else if (hasBg && !isWideTail) {
            painter.fillRect(x, y, cellRenderWidth, m_cellHeight, QColor(bgColor.r, bgColor.g, bgColor.b));
        }

        const QFont *cachedFont = &m_font;
        if (style.bold && style.italic)
            cachedFont = &m_fontBoldItalic;
        else if (style.bold)
            cachedFont = &m_fontBold;
        else if (style.italic)
            cachedFont = &m_fontItalic;
        const QColor cellForeground = hasFg ? QColor(fgColor.r, fgColor.g, fgColor.b) : defaultForeground;

        if (!isWideHead && !isWideTail) {
            if (runCellCount == 0 || runFont != cachedFont || runForeground != cellForeground) {
                flushTextRun();
                runText.reserve(static_cast<int>(m_cols) + 1);
                runText.append(QChar(0x202D));
                runFont = cachedFont;
                runForeground = cellForeground;
                runStartX = x;
            }

            if (graphemeLen > 0) {
                uint32_t codepoints[16];
                const uint32_t len = graphemeLen < 16 ? graphemeLen : 16;
                ghostty_render_state_row_cells_get(m_rowCells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_BUF,
                                                   codepoints);
                for (uint32_t i = 0; i < len; ++i)
                    appendCodepoint(runText, codepoints[i]);
            } else {
                runText.append(QChar(u' '));
            }

            ++runCellCount;
            x += m_cellWidth;
            continue;
        }

        flushTextRun();

        if (graphemeLen > 0 && !isWideTail) {
            uint32_t codepoints[16];
            const uint32_t len = graphemeLen < 16 ? graphemeLen : 16;
            ghostty_render_state_row_cells_get(m_rowCells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_BUF,
                                               codepoints);

            QString text;
            text.reserve(static_cast<int>(len));
            for (uint32_t i = 0; i < len; ++i)
                appendCodepoint(text, codepoints[i]);

            QFont cellFont = *cachedFont;
            cellFont.setFixedPitch(false);

            painter.setFont(cellFont);
            painter.setPen(cellForeground);
            painter.setLayoutDirection(Qt::LeftToRight);
            painter.drawText(QRectF(x, y, cellRenderWidth, m_cellHeight), Qt::AlignCenter | Qt::TextSingleLine, text);
            painter.setFont(m_font);
        }

        x += m_cellWidth;
    }

    flushTextRun();
}

void TerminalWidget::renderOverlays(QPainter &painter) const {
    if (!m_terminal || !m_renderState)
        return;

    size_t scrollOffset = 0;
    if (!m_searchMatches.isEmpty() || m_selection.active) {
        GhosttyTerminalScrollbar scrollbar = {};
        if (ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_SCROLLBAR, &scrollbar) == GHOSTTY_SUCCESS)
            scrollOffset = scrollbar.offset;
    }

    if (!m_searchMatches.isEmpty() || m_selection.active) {
        for (int viewportRow = 0; viewportRow < static_cast<int>(m_rows); ++viewportRow) {
            const int y = viewportRow * m_cellHeight;
            const int screenRow = static_cast<int>(viewportRow + scrollOffset);
            for (int col = 0; col < static_cast<int>(m_cols); ++col) {
                const int x = col * m_cellWidth;
                if (!m_searchMatches.isEmpty()) {
                    for (int i = 0; i < m_searchMatches.size(); ++i) {
                        const auto &match = m_searchMatches[i];
                        if (match.row == screenRow && col >= match.startCol && col < match.endCol) {
                            const QColor highlight =
                                (i == m_currentSearchIndex) ? QColor(255, 165, 0, 180) : QColor(255, 255, 0, 120);
                            painter.fillRect(x, y, m_cellWidth, m_cellHeight, highlight);
                            break;
                        }
                    }
                }

                if (cellInSelection(screenRow, col))
                    painter.fillRect(x, y, m_cellWidth, m_cellHeight, QColor(0, 120, 215, 180));
            }
        }
    }

    GhosttyRenderStateColors colors = GHOSTTY_INIT_SIZED(GhosttyRenderStateColors);
    if (ghostty_render_state_colors_get(m_renderState, &colors) != GHOSTTY_SUCCESS)
        return;

    bool cursorVisible = false;
    ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_VISIBLE, &cursorVisible);
    bool cursorInViewport = false;
    ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_HAS_VALUE, &cursorInViewport);

    if (cursorVisible && cursorInViewport && m_hasFocus) {
        uint16_t cx = 0, cy = 0;
        ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_X, &cx);
        ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_Y, &cy);

        GhosttyColorRgb curColor = colors.foreground;
        if (colors.cursor_has_value)
            curColor = colors.cursor;

        int curX = cx * m_cellWidth;
        int curY = cy * m_cellHeight;
        bool cursorBlinking = false;
        ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_BLINKING, &cursorBlinking);

        bool drawCursor = true;
        if (m_cursorBlinkEnabled && cursorBlinking)
            drawCursor = m_cursorBlinkVisible;

        if (drawCursor) {
            QColor cursorColor(curColor.r, curColor.g, curColor.b, 180);
            switch (m_cursorShape) {
                case 1:
                    painter.fillRect(curX, curY, 2, m_cellHeight, cursorColor);
                    break;
                case 2:
                    painter.fillRect(curX, curY + m_cellHeight - 2, m_cellWidth, 2, cursorColor);
                    break;
                default:
                    painter.fillRect(curX, curY, m_cellWidth, m_cellHeight, cursorColor);
                    break;
            }
        }
    }
}

void TerminalWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateGridSize();
    notifyInputMethodCursorChange();
}

GhosttyKey TerminalWidget::mapQtKeyToGhostty(int key) const {
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return static_cast<GhosttyKey>(GHOSTTY_KEY_A + (key - Qt::Key_A));
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return static_cast<GhosttyKey>(GHOSTTY_KEY_DIGIT_0 + (key - Qt::Key_0));
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        return static_cast<GhosttyKey>(GHOSTTY_KEY_F1 + (key - Qt::Key_F1));
    }

    switch (key) {
        case Qt::Key_Space:
            return GHOSTTY_KEY_SPACE;
        case Qt::Key_Enter:
        case Qt::Key_Return:
            return GHOSTTY_KEY_ENTER;
        case Qt::Key_Tab:
            return GHOSTTY_KEY_TAB;
        case Qt::Key_Backspace:
            return GHOSTTY_KEY_BACKSPACE;
        case Qt::Key_Delete:
            return GHOSTTY_KEY_DELETE;
        case Qt::Key_Escape:
            return GHOSTTY_KEY_ESCAPE;
        case Qt::Key_Up:
            return GHOSTTY_KEY_ARROW_UP;
        case Qt::Key_Down:
            return GHOSTTY_KEY_ARROW_DOWN;
        case Qt::Key_Left:
            return GHOSTTY_KEY_ARROW_LEFT;
        case Qt::Key_Right:
            return GHOSTTY_KEY_ARROW_RIGHT;
        case Qt::Key_Home:
            return GHOSTTY_KEY_HOME;
        case Qt::Key_End:
            return GHOSTTY_KEY_END;
        case Qt::Key_PageUp:
            return GHOSTTY_KEY_PAGE_UP;
        case Qt::Key_PageDown:
            return GHOSTTY_KEY_PAGE_DOWN;
        case Qt::Key_Insert:
            return GHOSTTY_KEY_INSERT;
        case Qt::Key_Minus:
            return GHOSTTY_KEY_MINUS;
        case Qt::Key_Equal:
            return GHOSTTY_KEY_EQUAL;
        case Qt::Key_BracketLeft:
            return GHOSTTY_KEY_BRACKET_LEFT;
        case Qt::Key_BracketRight:
            return GHOSTTY_KEY_BRACKET_RIGHT;
        case Qt::Key_Backslash:
            return GHOSTTY_KEY_BACKSLASH;
        case Qt::Key_Semicolon:
            return GHOSTTY_KEY_SEMICOLON;
        case Qt::Key_Apostrophe:
            return GHOSTTY_KEY_QUOTE;
        case Qt::Key_Comma:
            return GHOSTTY_KEY_COMMA;
        case Qt::Key_Period:
            return GHOSTTY_KEY_PERIOD;
        case Qt::Key_Slash:
            return GHOSTTY_KEY_SLASH;
        case Qt::Key_QuoteLeft:
            return GHOSTTY_KEY_BACKQUOTE;
        default:
            return GHOSTTY_KEY_UNIDENTIFIED;
    }
}

uint32_t TerminalWidget::unshiftedCodepointForKey(int key) const {
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return 'a' + static_cast<uint32_t>(key - Qt::Key_A);
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return '0' + static_cast<uint32_t>(key - Qt::Key_0);
    }

    switch (key) {
        case Qt::Key_Space:
            return ' ';
        case Qt::Key_Minus:
            return '-';
        case Qt::Key_Equal:
            return '=';
        case Qt::Key_BracketLeft:
            return '[';
        case Qt::Key_BracketRight:
            return ']';
        case Qt::Key_Backslash:
            return '\\';
        case Qt::Key_Semicolon:
            return ';';
        case Qt::Key_Apostrophe:
            return '\'';
        case Qt::Key_Comma:
            return ',';
        case Qt::Key_Period:
            return '.';
        case Qt::Key_Slash:
            return '/';
        case Qt::Key_QuoteLeft:
            return '`';
        default:
            return 0;
    }
}

namespace {

// Standard C0 control character produced by Ctrl + printable key.
// Returns 0 when the combination does not map to a well-known control char.
char ctrlCharForKey(int key, Qt::KeyboardModifiers mods) {
    if (!(mods & Qt::ControlModifier))
        return 0;

    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return static_cast<char>(key - Qt::Key_A + 1); // Ctrl+A = 0x01 … Ctrl+Z = 0x1A

    switch (key) {
        case Qt::Key_2:
        case Qt::Key_At:
            return 0x00; // Ctrl+@ = NUL
        case Qt::Key_3:
        case Qt::Key_BracketLeft:
            return 0x1B; // Ctrl+[ = ESC
        case Qt::Key_4:
        case Qt::Key_Backslash:
            return 0x1C; // Ctrl+\ = FS
        case Qt::Key_5:
        case Qt::Key_BracketRight:
            return 0x1D; // Ctrl+] = GS
        case Qt::Key_6:
            return 0x1E; // Ctrl+^ = RS
        case Qt::Key_7:
        case Qt::Key_Minus:
        case Qt::Key_Underscore:
            return 0x1F; // Ctrl+_ = US
        case Qt::Key_8:
            return 0x7F; // Ctrl+? = DEL
        case Qt::Key_Space:
            return 0x00; // Ctrl+Space = NUL
        default:
            return 0;
    }
}

bool isC0ControlChar(const QByteArray &text) {
    if (text.size() != 1)
        return false;
    unsigned char c = static_cast<unsigned char>(text.at(0));
    return c <= 0x1F || c == 0x7F;
}

} // anonymous namespace

void TerminalWidget::keyPressEvent(QKeyEvent *event) {
    if (!m_terminal || !m_keyEncoder || !m_keyEvent || !m_ptySession) {
        QWidget::keyPressEvent(event);
        return;
    }

    // Fast path: if Qt already translated the key into a single C0 control
    // character (common on some platforms for Ctrl+letter), send it directly
    // without going through the encoder. This also handles the case where
    // Qt reports the key as Qt::Key_unknown but the text is a control char.
    QByteArray text = event->text().toUtf8();
    if (text.size() == 1) {
        unsigned char c = static_cast<unsigned char>(text.at(0));
        if (c <= 0x1F || c == 0x7F) {
            m_ptySession->write(text);
            return;
        }
    }

    ghostty_key_encoder_setopt_from_terminal(m_keyEncoder, m_terminal);

    GhosttyKey gkey = mapQtKeyToGhostty(event->key());
    if (gkey == GHOSTTY_KEY_UNIDENTIFIED && text.isEmpty()) {
        QWidget::keyPressEvent(event);
        return;
    }

    GhosttyMods mods = 0;
    if (event->modifiers() & Qt::ShiftModifier)
        mods |= GHOSTTY_MODS_SHIFT;
    if (event->modifiers() & Qt::ControlModifier)
        mods |= GHOSTTY_MODS_CTRL;
    if (event->modifiers() & Qt::AltModifier)
        mods |= GHOSTTY_MODS_ALT;
    if (event->modifiers() & Qt::MetaModifier)
        mods |= GHOSTTY_MODS_SUPER;

    ghostty_key_event_set_key(m_keyEvent, gkey);
    ghostty_key_event_set_action(m_keyEvent, GHOSTTY_KEY_ACTION_PRESS);
    ghostty_key_event_set_mods(m_keyEvent, mods);

    uint32_t ucp = unshiftedCodepointForKey(event->key());
    ghostty_key_event_set_unshifted_codepoint(m_keyEvent, ucp);

    // consumed_mods: modifiers that the platform text input already accounted
    // for when producing the UTF-8 text. For printable keys, shift/ctrl/alt
    // are consumed because they change the resulting character.
    GhosttyMods consumed = 0;
    if (ucp != 0) {
        if (mods & GHOSTTY_MODS_SHIFT)
            consumed |= GHOSTTY_MODS_SHIFT;
        if (mods & GHOSTTY_MODS_CTRL)
            consumed |= GHOSTTY_MODS_CTRL;
        if (mods & GHOSTTY_MODS_ALT)
            consumed |= GHOSTTY_MODS_ALT;
    }
    ghostty_key_event_set_consumed_mods(m_keyEvent, consumed);

    QByteArray textUtf8 = event->text().toUtf8();
    // Do not pass C0 control characters as utf8 text; let the encoder
    // derive them from the logical key + mods (Ghostty docs explicitly
    // warn against passing U+0000–U+001F or U+007F here).
    if (!textUtf8.isEmpty() && !isC0ControlChar(textUtf8)) {
        ghostty_key_event_set_utf8(m_keyEvent, textUtf8.constData(), static_cast<size_t>(textUtf8.size()));
    } else {
        ghostty_key_event_set_utf8(m_keyEvent, nullptr, 0);
    }

    char buf[128];
    size_t written = 0;
    GhosttyResult err = ghostty_key_encoder_encode(m_keyEncoder, m_keyEvent, buf, sizeof(buf), &written);
    if (err == GHOSTTY_SUCCESS && written > 0) {
        m_ptySession->write(QByteArray(buf, static_cast<int>(written)));
    } else if (!textUtf8.isEmpty() && !isC0ControlChar(textUtf8) && (err != GHOSTTY_SUCCESS || written == 0)) {
        m_ptySession->write(textUtf8);
    } else {
        // Fallback: for standard Ctrl+letter/symbol combos, send the
        // corresponding C0 control character directly when the encoder
        // does not produce output.
        char c0 = ctrlCharForKey(event->key(), event->modifiers());
        if (c0 != 0) {
            m_ptySession->write(QByteArray(1, c0));
        } else if (gkey == GHOSTTY_KEY_UNIDENTIFIED) {
            QWidget::keyPressEvent(event);
            return;
        }
    }
}

void TerminalWidget::inputMethodEvent(QInputMethodEvent *event) {
    if (!event) {
        return;
    }

    if (!m_ptySession) {
        event->ignore();
        return;
    }

    const QString commitString = event->commitString();
    if (!commitString.isEmpty()) {
        m_ptySession->write(commitString.toUtf8());
        m_preeditText.clear();
    }

    if (commitString.isEmpty() || !event->preeditString().isEmpty()) {
        m_preeditText = event->preeditString();
    }

    event->accept();
    update();
    notifyInputMethodCursorChange();
}

QVariant TerminalWidget::inputMethodQuery(Qt::InputMethodQuery query) const {
    switch (query) {
        case Qt::ImEnabled:
            return true;
        case Qt::ImCursorRectangle:
            return inputMethodCursorRect();
        case Qt::ImFont:
            return m_font;
        case Qt::ImAnchorRectangle:
            return inputMethodCursorRect();
        case Qt::ImInputItemClipRectangle:
            return rect();
        case Qt::ImCursorPosition:
        case Qt::ImAnchorPosition:
            return 0;
        case Qt::ImSurroundingText:
        case Qt::ImCurrentSelection:
        case Qt::ImTextBeforeCursor:
        case Qt::ImTextAfterCursor:
            return QString();
        case Qt::ImReadOnly:
            return false;
        default:
            return QWidget::inputMethodQuery(query);
    }
}

bool TerminalWidget::focusNextPrevChild(bool next) {
    (void)next;
    // Prevent Tab / Shift+Tab from moving focus out of the terminal.
    // The Tab key is sent to the PTY as a normal keypress for shell
    // completion, so we must keep focus here.
    return false;
}

void TerminalWidget::setTerminalFont(const QFont &font) {
    m_font = font;
    m_font.setStyleHint(QFont::Monospace);
    m_font.setFixedPitch(true);

    QFontMetrics fm(m_font);
    m_cellWidth = fm.horizontalAdvance('M');
    m_cellHeight = fm.height();
    m_fontAscent = fm.ascent();
    updateCachedFonts();

    updateGridSize();
    update();
}

void TerminalWidget::setCursorShape(int shape) {
    m_cursorShape = shape;
    update();
}

void TerminalWidget::setCursorBlinkEnabled(bool blink) {
    m_cursorBlinkEnabled = blink;
    if (m_hasFocus) {
        if (blink)
            m_blinkTimer->start();
        else
            m_blinkTimer->stop();
        m_cursorBlinkVisible = true;
        update();
    }
}

void TerminalWidget::setStartOptions(const PtySession::StartOptions &options) {
    m_startOptions = options;
    m_hasStartOptions = !options.command.isEmpty() || !options.workingDirectory.isEmpty();
}

void TerminalWidget::setScrollbackLines(int lines) {
    m_scrollbackLines = lines;
}

void TerminalWidget::focusInEvent(QFocusEvent *event) {
    QWidget::focusInEvent(event);
    m_hasFocus = true;
    sendFocusEvent(true);
    if (m_cursorBlinkEnabled)
        m_blinkTimer->start();
    update();
    notifyInputMethodCursorChange();
    Q_EMIT focusGained();
}

void TerminalWidget::focusOutEvent(QFocusEvent *event) {
    QWidget::focusOutEvent(event);
    m_hasFocus = false;
    sendFocusEvent(false);
    m_blinkTimer->stop();
    m_cursorBlinkVisible = true;
    update();
    notifyInputMethodCursorChange();
}

void TerminalWidget::sendFocusEvent(bool gained) {
    if (!m_terminal || !m_ptySession)
        return;

    bool focusMode = false;
    if (ghostty_terminal_mode_get(m_terminal, GHOSTTY_MODE_FOCUS_EVENT, &focusMode) != GHOSTTY_SUCCESS)
        return;
    if (!focusMode)
        return;

    GhosttyFocusEvent event = gained ? GHOSTTY_FOCUS_GAINED : GHOSTTY_FOCUS_LOST;
    char buf[8];
    size_t written = 0;
    GhosttyResult err = ghostty_focus_encode(event, buf, sizeof(buf), &written);
    if (err == GHOSTTY_SUCCESS && written > 0) {
        m_ptySession->write(QByteArray(buf, static_cast<int>(written)));
    }
}

void TerminalWidget::wheelEvent(QWheelEvent *event) {
    if (!m_terminal)
        return;

    bool mouseTracking = false;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_MOUSE_TRACKING, &mouseTracking);
    if (mouseTracking)
        return; // minimal implementation: ignore wheel in mouse-tracking mode

    int delta = event->angleDelta().y();
    if (delta != 0) {
        int rows = delta > 0 ? -3 : 3;
        GhosttyTerminalScrollViewport sv = {
            .tag = GHOSTTY_SCROLL_VIEWPORT_DELTA,
            .value = {.delta = static_cast<intptr_t>(rows)},
        };
        ghostty_terminal_scroll_viewport(m_terminal, sv);
        m_renderStateDirty = true;
        update();
    }
}

void TerminalWidget::onPtyDataReceived(const QByteArray &data) {
    if (!m_terminal || data.isEmpty())
        return;

    m_pendingPtyData.append(data);
    scheduleTerminalRepaint();
}

void TerminalWidget::onPtySessionClosed() {
    flushPendingPtyData();
    if (m_renderTimer)
        m_renderTimer->stop();
    Q_EMIT sessionClosed();
}

QRect TerminalWidget::inputMethodCursorRect() const {
    QRect cursorRect(0, 0, qMax(m_cellWidth, 1), qMax(m_cellHeight, 1));
    if (!m_terminal || !m_renderState) {
        return cursorRect;
    }

    if (!syncRenderState()) {
        return cursorRect;
    }

    bool cursorInViewport = false;
    if (ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_HAS_VALUE, &cursorInViewport)
            != GHOSTTY_SUCCESS
        || !cursorInViewport) {
        return cursorRect;
    }

    uint16_t cursorX = 0;
    uint16_t cursorY = 0;
    ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_X, &cursorX);
    ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_Y, &cursorY);
    cursorRect.moveTo(static_cast<int>(cursorX) * m_cellWidth, static_cast<int>(cursorY) * m_cellHeight);
    return cursorRect;
}

void TerminalWidget::notifyInputMethodCursorChange() {
    if (QInputMethod *inputMethod = QGuiApplication::inputMethod()) {
        inputMethod->update(Qt::ImEnabled | Qt::ImCursorRectangle | Qt::ImAnchorRectangle
                            | Qt::ImInputItemClipRectangle);
    }
}

void TerminalWidget::scheduleTerminalRepaint() {
    if (!m_terminal || m_pendingPtyData.isEmpty()) {
        return;
    }

    if (m_pendingPtyData.size() >= kImmediatePtyFlushBytes) {
        flushPendingPtyData();
        update();
        m_lastRenderTime.restart();
        return;
    }

    if (!m_lastRenderTime.isValid()) {
        if (!m_renderTimer->isActive())
            m_renderTimer->start(kInitialPtyCoalesceIntervalMs);
        return;
    }

    if (m_lastRenderTime.elapsed() >= kBurstRenderIntervalMs) {
        flushPendingPtyData();
        update();
        m_lastRenderTime.restart();
        return;
    }

    if (!m_renderTimer->isActive()) {
        const int remaining = qMax(0, kBurstRenderIntervalMs - static_cast<int>(m_lastRenderTime.elapsed()));
        m_renderTimer->start(remaining);
    }
}

void TerminalWidget::applyPendingResize() {
    if (m_pendingResizeCols == m_cols && m_pendingResizeRows == m_rows)
        return;

    m_cols = m_pendingResizeCols;
    m_rows = m_pendingResizeRows;

    if (m_terminal) {
        ghostty_terminal_resize(m_terminal, m_cols, m_rows, static_cast<uint32_t>(m_cellWidth),
                                static_cast<uint32_t>(m_cellHeight));
        m_renderStateDirty = true;
    }

    if (m_ptySession)
        m_ptySession->resize(m_cols, m_rows, m_cellWidth, m_cellHeight);

#ifdef QTGHOSTTY_TESTING
    ++m_debugResizeApplyCount;
#endif

    update();
}

void TerminalWidget::flushPendingPtyData() {
    if (!m_terminal || m_pendingPtyData.isEmpty()) {
        return;
    }

#ifdef QTGHOSTTY_TESTING
    ++m_debugPtyFlushCount;
#endif
    ghostty_terminal_vt_write(m_terminal, reinterpret_cast<const uint8_t *>(m_pendingPtyData.constData()),
                              static_cast<size_t>(m_pendingPtyData.size()));
    m_pendingPtyData.clear();
    m_renderStateDirty = true;
    notifyInputMethodCursorChange();
}

void TerminalWidget::onRenderTimerTimeout() {
    if (m_pendingPtyData.isEmpty()) {
        return;
    }

    flushPendingPtyData();
    update();
    m_lastRenderTime.restart();
}

void TerminalWidget::renderPreeditText(QPainter &painter) {
    if (m_preeditText.isEmpty()) {
        return;
    }

    const QRect cursorRect = inputMethodCursorRect();
    QFontMetrics fm(m_font);
    const int baseline = cursorRect.y() + m_fontAscent;
    const int preeditWidth = fm.horizontalAdvance(m_preeditText);
    const QRect backgroundRect(cursorRect.x(), cursorRect.y(), qMax(preeditWidth, cursorRect.width()), m_cellHeight);

    painter.fillRect(backgroundRect, palette().base());
    painter.setPen(palette().text().color());
    painter.setFont(m_font);
    painter.drawText(cursorRect.x(), baseline, m_preeditText);
    painter.drawLine(cursorRect.x(), cursorRect.y() + m_cellHeight - 1, cursorRect.x() + qMax(preeditWidth, 1),
                     cursorRect.y() + m_cellHeight - 1);
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

QString TerminalWidget::textForScreenRow(int row) const {
    if (!m_terminal)
        return QString();

    QString result;
    for (int col = 0; col < m_cols; ++col) {
        GhosttyPoint point = {
            .tag = GHOSTTY_POINT_TAG_SCREEN,
            .value = {.coordinate = {.x = static_cast<uint16_t>(col), .y = static_cast<uint32_t>(row)}}};
        GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        if (ghostty_terminal_grid_ref(m_terminal, point, &ref) != GHOSTTY_SUCCESS)
            continue;

        uint32_t graphemes[16];
        size_t len = 0;
        if (ghostty_grid_ref_graphemes(&ref, graphemes, 16, &len) == GHOSTTY_SUCCESS && len > 0) {
            for (size_t i = 0; i < len; ++i)
                appendCodepoint(result, graphemes[i]);
        }
    }
    return result;
}

void TerminalWidget::performSearch(const QString &query) {
    m_searchMatches.clear();
    m_currentSearchIndex = -1;

    if (query.isEmpty() || !m_terminal) {
        update();
        return;
    }

    size_t totalRows = 0;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_TOTAL_ROWS, &totalRows);

    for (size_t row = 0; row < totalRows; ++row) {
        QString line = textForScreenRow(static_cast<int>(row));
        int pos = 0;
        while ((pos = line.indexOf(query, pos, Qt::CaseInsensitive)) != -1) {
            m_searchMatches.append({static_cast<int>(row), pos, static_cast<int>(pos + query.length())});
            pos += query.length();
        }
    }

    if (!m_searchMatches.isEmpty())
        m_currentSearchIndex = 0;

    update();
}

void TerminalWidget::clearSearch() {
    m_searchMatches.clear();
    m_currentSearchIndex = -1;
    update();
}

void TerminalWidget::findNext() {
    if (m_searchMatches.isEmpty())
        return;
    m_currentSearchIndex = (m_currentSearchIndex + 1) % m_searchMatches.size();
    update();
}

void TerminalWidget::findPrevious() {
    if (m_searchMatches.isEmpty())
        return;
    m_currentSearchIndex = m_currentSearchIndex - 1;
    if (m_currentSearchIndex < 0)
        m_currentSearchIndex = m_searchMatches.size() - 1;
    update();
}

bool TerminalWidget::hasSearchMatches() const {
    return !m_searchMatches.isEmpty();
}

// ---------------------------------------------------------------------------
// Mouse selection
// ---------------------------------------------------------------------------

void TerminalWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_selection.active = false;
        m_selection.startCol = event->pos().x() / m_cellWidth;
        m_selection.startRow = screenRowForViewportRow(event->pos().y() / m_cellHeight);
        m_selection.endCol = m_selection.startCol;
        m_selection.endRow = m_selection.startRow;
        update();
    }
}

void TerminalWidget::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        m_selection.endCol = event->pos().x() / m_cellWidth;
        m_selection.endRow = screenRowForViewportRow(event->pos().y() / m_cellHeight);
        m_selection.active = true;
        update();
    }
}

void TerminalWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_selection.endCol = event->pos().x() / m_cellWidth;
        m_selection.endRow = screenRowForViewportRow(event->pos().y() / m_cellHeight);
        m_selection.active = true;
        update();
    }
}

int TerminalWidget::screenRowForViewportRow(int viewportRow) const {
    if (!m_terminal)
        return viewportRow;
    GhosttyTerminalScrollbar scrollbar = {};
    if (ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_SCROLLBAR, &scrollbar) != GHOSTTY_SUCCESS)
        return viewportRow;
    return static_cast<int>(viewportRow + scrollbar.offset);
}

bool TerminalWidget::cellInSelection(int screenRow, int col) const {
    if (!m_selection.active)
        return false;

    int top = qMin(m_selection.startRow, m_selection.endRow);
    int bottom = qMax(m_selection.startRow, m_selection.endRow);
    if (screenRow < top || screenRow > bottom)
        return false;

    int left = qMin(m_selection.startCol, m_selection.endCol);
    int right = qMax(m_selection.startCol, m_selection.endCol);

    if (m_selection.startRow == m_selection.endRow) {
        return screenRow == top && col >= left && col <= right;
    }

    if (screenRow == top)
        return col >= m_selection.startCol;
    if (screenRow == bottom)
        return col <= m_selection.endCol;
    return col >= left && col <= right;
}

QString TerminalWidget::selectedText() const {
    if (!m_selection.active || !m_terminal)
        return QString();

    int top = qMin(m_selection.startRow, m_selection.endRow);
    int bottom = qMax(m_selection.startRow, m_selection.endRow);

    QStringList lines;
    for (int row = top; row <= bottom; ++row) {
        QString line;
        int left = 0;
        int right = m_cols - 1;

        if (row == top && row == bottom) {
            left = qMin(m_selection.startCol, m_selection.endCol);
            right = qMax(m_selection.startCol, m_selection.endCol);
        } else if (row == top) {
            left = m_selection.startCol;
        } else if (row == bottom) {
            right = m_selection.endCol;
        }

        for (int col = left; col <= right; ++col) {
            GhosttyPoint point = {
                .tag = GHOSTTY_POINT_TAG_SCREEN,
                .value = {.coordinate = {.x = static_cast<uint16_t>(col), .y = static_cast<uint32_t>(row)}}};
            GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
            if (ghostty_terminal_grid_ref(m_terminal, point, &ref) != GHOSTTY_SUCCESS)
                continue;

            uint32_t graphemes[16];
            size_t len = 0;
            if (ghostty_grid_ref_graphemes(&ref, graphemes, 16, &len) == GHOSTTY_SUCCESS && len > 0) {
                for (size_t i = 0; i < len; ++i)
                    appendCodepoint(line, graphemes[i]);
            }
        }
        lines.append(line);
    }
    return lines.join("\n");
}

bool TerminalWidget::hasSelection() const {
    return m_selection.active;
}

void TerminalWidget::copyToClipboard() {
    QString text = selectedText();
    if (!text.isEmpty())
        QGuiApplication::clipboard()->setText(text);
}

void TerminalWidget::pasteFromClipboard() {
    if (!m_ptySession)
        return;
    QString text = QGuiApplication::clipboard()->text();
    if (!text.isEmpty())
        m_ptySession->write(text.toUtf8());
}

void TerminalWidget::selectAll() {
    if (!m_terminal)
        return;
    size_t totalRows = 0;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_TOTAL_ROWS, &totalRows);
    m_selection.active = true;
    m_selection.startRow = 0;
    m_selection.startCol = 0;
    m_selection.endRow = static_cast<int>(totalRows) - 1;
    m_selection.endCol = m_cols - 1;
    update();
}

void TerminalWidget::zoomIn() {
    QFont f = m_font;
    f.setPointSize(qMin(f.pointSize() + 1, 72));
    setTerminalFont(f);
}

void TerminalWidget::zoomOut() {
    QFont f = m_font;
    f.setPointSize(qMax(f.pointSize() - 1, 5));
    setTerminalFont(f);
}
