#include "TerminalWidget.h"
#include "PtySession.h"

#include <QPainter>
#include <QKeyEvent>
#include <QFontMetrics>
#include <QWheelEvent>

#include <algorithm>
#include <cstdio>

// ---------------------------------------------------------------------------
// Effect callbacks (C-linkage friends of TerminalWidget)
// ---------------------------------------------------------------------------

void effectWritePty(GhosttyTerminal terminal, void *userdata,
                           const uint8_t *data, size_t len)
{
    (void)terminal;
    auto *widget = static_cast<TerminalWidget *>(userdata);
    if (widget->m_ptySession && len > 0) {
        widget->m_ptySession->write(
            QByteArray(reinterpret_cast<const char *>(data), static_cast<int>(len)));
    }
}

bool effectSize(GhosttyTerminal terminal, void *userdata,
                       GhosttySizeReportSize *out_size)
{
    (void)terminal;
    auto *widget = static_cast<TerminalWidget *>(userdata);
    out_size->rows = widget->m_rows;
    out_size->columns = widget->m_cols;
    out_size->cell_width = static_cast<uint32_t>(widget->m_cellWidth);
    out_size->cell_height = static_cast<uint32_t>(widget->m_cellHeight);
    return true;
}

bool effectDeviceAttributes(GhosttyTerminal terminal, void *userdata,
                                   GhosttyDeviceAttributes *out_attrs)
{
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

GhosttyString effectXtversion(GhosttyTerminal terminal, void *userdata)
{
    (void)terminal;
    (void)userdata;
    static const char name[] = "deepin-terminal-ghostty";
    return GhosttyString{
        .ptr = reinterpret_cast<const uint8_t *>(name),
        .len = sizeof(name) - 1,
    };
}

void effectTitleChanged(GhosttyTerminal terminal, void *userdata)
{
    auto *widget = static_cast<TerminalWidget *>(userdata);
    GhosttyString title = {};
    if (ghostty_terminal_get(terminal, GHOSTTY_TERMINAL_DATA_TITLE, &title) == GHOSTTY_SUCCESS) {
        QString qTitle = QString::fromUtf8(reinterpret_cast<const char *>(title.ptr),
                                           static_cast<int>(title.len));
        widget->terminalTitleChanged(qTitle);
    }
}

bool effectColorScheme(GhosttyTerminal terminal, void *userdata,
                              GhosttyColorScheme *out_scheme)
{
    (void)terminal;
    (void)userdata;
    (void)out_scheme;
    return false;
}

// ---------------------------------------------------------------------------
// TerminalWidget
// ---------------------------------------------------------------------------

TerminalWidget::TerminalWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);

    m_font = QFont("Monospace", 11);
    m_font.setStyleHint(QFont::Monospace);
    m_font.setFixedPitch(true);

    QFontMetrics fm(m_font);
    m_cellWidth = fm.horizontalAdvance('M');
    m_cellHeight = fm.height();
    m_fontAscent = fm.ascent();
}

TerminalWidget::~TerminalWidget()
{
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

bool TerminalWidget::initialize()
{
    updateGridSize();

    if (!setupTerminal())
        return false;
    if (!setupRenderState())
        return false;
    if (!setupEncoders())
        return false;

    m_ptySession = new PtySession(this);
    connect(m_ptySession, &PtySession::dataReceived,
            this, &TerminalWidget::onPtyDataReceived);
    connect(m_ptySession, &PtySession::sessionClosed,
            this, &TerminalWidget::onPtySessionClosed);

    if (!m_ptySession->start(m_cols, m_rows))
        return false;

    return true;
}

bool TerminalWidget::setupTerminal()
{
    GhosttyTerminalOptions opts = {
        .cols = m_cols,
        .rows = m_rows,
        .max_scrollback = 1000,
    };

    GhosttyResult err = ghostty_terminal_new(nullptr, &m_terminal, opts);
    if (err != GHOSTTY_SUCCESS) {
        std::fprintf(stderr, "ghostty_terminal_new failed (%d)\n", err);
        return false;
    }

    ghostty_terminal_resize(m_terminal, m_cols, m_rows,
                            static_cast<uint32_t>(m_cellWidth),
                            static_cast<uint32_t>(m_cellHeight));

    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_USERDATA, this);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_WRITE_PTY,
                         reinterpret_cast<const void *>(effectWritePty));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_SIZE,
                         reinterpret_cast<const void *>(effectSize));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_DEVICE_ATTRIBUTES,
                         reinterpret_cast<const void *>(effectDeviceAttributes));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_XTVERSION,
                         reinterpret_cast<const void *>(effectXtversion));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_TITLE_CHANGED,
                         reinterpret_cast<const void *>(effectTitleChanged));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_SCHEME,
                         reinterpret_cast<const void *>(effectColorScheme));

    uint64_t kittyLimit = 64 * 1024 * 1024;
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_KITTY_IMAGE_STORAGE_LIMIT,
                         &kittyLimit);

    return true;
}

bool TerminalWidget::setupRenderState()
{
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

bool TerminalWidget::setupEncoders()
{
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

void TerminalWidget::updateGridSize()
{
    int w = width();
    int h = height();
    if (m_cellWidth <= 0 || m_cellHeight <= 0)
        return;

    uint16_t cols = static_cast<uint16_t>(w / m_cellWidth);
    uint16_t rows = static_cast<uint16_t>(h / m_cellHeight);
    if (cols < 1)
        cols = 1;
    if (rows < 1)
        rows = 1;

    if (cols != m_cols || rows != m_rows) {
        m_cols = cols;
        m_rows = rows;

        if (m_terminal) {
            ghostty_terminal_resize(m_terminal, m_cols, m_rows,
                                    static_cast<uint32_t>(m_cellWidth),
                                    static_cast<uint32_t>(m_cellHeight));
        }
        if (m_ptySession) {
            m_ptySession->resize(m_cols, m_rows, m_cellWidth, m_cellHeight);
        }
    }
}

void TerminalWidget::paintEvent(QPaintEvent *event)
{
    (void)event;
    if (!m_terminal || !m_renderState)
        return;

    QPainter painter(this);
    painter.setFont(m_font);

    renderTerminal(painter);
}

void TerminalWidget::renderTerminal(QPainter &painter)
{
    GhosttyResult err = ghostty_render_state_update(m_renderState, m_terminal);
    if (err != GHOSTTY_SUCCESS)
        return;

    GhosttyRenderStateColors colors = GHOSTTY_INIT_SIZED(GhosttyRenderStateColors);
    err = ghostty_render_state_colors_get(m_renderState, &colors);
    if (err != GHOSTTY_SUCCESS)
        return;

    painter.fillRect(rect(), QColor(colors.background.r, colors.background.g,
                                    colors.background.b));

    err = ghostty_render_state_get(m_renderState,
                                   GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR,
                                   &m_rowIter);
    if (err != GHOSTTY_SUCCESS)
        return;

    int y = 0;
    while (ghostty_render_state_row_iterator_next(m_rowIter)) {
        err = ghostty_render_state_row_get(m_rowIter,
                                           GHOSTTY_RENDER_STATE_ROW_DATA_CELLS,
                                           &m_rowCells);
        if (err != GHOSTTY_SUCCESS)
            continue;

        int x = 0;
        while (ghostty_render_state_row_cells_next(m_rowCells)) {
            uint32_t graphemeLen = 0;
            ghostty_render_state_row_cells_get(
                m_rowCells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_LEN,
                &graphemeLen);

            GhosttyColorRgb bgColor = colors.background;
            bool hasBg =
                (ghostty_render_state_row_cells_get(
                     m_rowCells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_BG_COLOR,
                     &bgColor) == GHOSTTY_SUCCESS);

            GhosttyColorRgb fgColor = colors.foreground;
            ghostty_render_state_row_cells_get(
                m_rowCells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_FG_COLOR,
                &fgColor);

            GhosttyStyle style = GHOSTTY_INIT_SIZED(GhosttyStyle);
            ghostty_render_state_row_cells_get(
                m_rowCells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_STYLE, &style);

            if (style.inverse) {
                std::swap(fgColor, bgColor);
                hasBg = true;
            }

            if (hasBg) {
                painter.fillRect(x, y, m_cellWidth, m_cellHeight,
                                 QColor(bgColor.r, bgColor.g, bgColor.b));
            }

            if (graphemeLen > 0) {
                uint32_t codepoints[16];
                uint32_t len = graphemeLen < 16 ? graphemeLen : 16;
                ghostty_render_state_row_cells_get(
                    m_rowCells,
                    GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_BUF,
                    codepoints);

                QString text;
                for (uint32_t i = 0; i < len; ++i) {
                    text.append(QChar(codepoints[i]));
                }

                QFont cellFont = m_font;
                if (style.bold)
                    cellFont.setBold(true);
                if (style.italic)
                    cellFont.setItalic(true);
                painter.setFont(cellFont);

                painter.setPen(QColor(fgColor.r, fgColor.g, fgColor.b));
                painter.drawText(x, y + m_fontAscent, text);
                painter.setFont(m_font);
            }

            x += m_cellWidth;
        }

        bool clean = false;
        ghostty_render_state_row_set(m_rowIter,
                                     GHOSTTY_RENDER_STATE_ROW_OPTION_DIRTY,
                                     &clean);

        y += m_cellHeight;
    }

    bool cursorVisible = false;
    ghostty_render_state_get(m_renderState,
                             GHOSTTY_RENDER_STATE_DATA_CURSOR_VISIBLE,
                             &cursorVisible);
    bool cursorInViewport = false;
    ghostty_render_state_get(
        m_renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_HAS_VALUE,
        &cursorInViewport);

    if (cursorVisible && cursorInViewport && m_hasFocus) {
        uint16_t cx = 0, cy = 0;
        ghostty_render_state_get(m_renderState,
                                 GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_X,
                                 &cx);
        ghostty_render_state_get(m_renderState,
                                 GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_Y,
                                 &cy);

        GhosttyColorRgb curColor = colors.foreground;
        if (colors.cursor_has_value)
            curColor = colors.cursor;

        int curX = cx * m_cellWidth;
        int curY = cy * m_cellHeight;
        painter.fillRect(
            curX, curY, m_cellWidth, m_cellHeight,
            QColor(curColor.r, curColor.g, curColor.b, 128));
    }

    GhosttyRenderStateDirty cleanState = GHOSTTY_RENDER_STATE_DIRTY_FALSE;
    ghostty_render_state_set(m_renderState, GHOSTTY_RENDER_STATE_OPTION_DIRTY,
                             &cleanState);
}

void TerminalWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateGridSize();
}

GhosttyKey TerminalWidget::mapQtKeyToGhostty(int key) const
{
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

uint32_t TerminalWidget::unshiftedCodepointForKey(int key) const
{
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
char ctrlCharForKey(int key, Qt::KeyboardModifiers mods)
{
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

bool isC0ControlChar(const QByteArray &text)
{
    if (text.size() != 1)
        return false;
    unsigned char c = static_cast<unsigned char>(text.at(0));
    return c <= 0x1F || c == 0x7F;
}

} // anonymous namespace

void TerminalWidget::keyPressEvent(QKeyEvent *event)
{
    if (!m_terminal || !m_keyEncoder || !m_keyEvent || !m_ptySession) {
        QWidget::keyPressEvent(event);
        return;
    }

    ghostty_key_encoder_setopt_from_terminal(m_keyEncoder, m_terminal);

    GhosttyKey gkey = mapQtKeyToGhostty(event->key());
    if (gkey == GHOSTTY_KEY_UNIDENTIFIED && event->text().isEmpty()) {
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
        ghostty_key_event_set_utf8(m_keyEvent, textUtf8.constData(),
                                   static_cast<size_t>(textUtf8.size()));
    } else {
        ghostty_key_event_set_utf8(m_keyEvent, nullptr, 0);
    }

    char buf[128];
    size_t written = 0;
    GhosttyResult err = ghostty_key_encoder_encode(m_keyEncoder, m_keyEvent,
                                                    buf, sizeof(buf), &written);
    if (err == GHOSTTY_SUCCESS && written > 0) {
        m_ptySession->write(QByteArray(buf, static_cast<int>(written)));
    } else if (!textUtf8.isEmpty() && !isC0ControlChar(textUtf8) &&
               (err != GHOSTTY_SUCCESS || written == 0)) {
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

bool TerminalWidget::focusNextPrevChild(bool next)
{
    (void)next;
    // Prevent Tab / Shift+Tab from moving focus out of the terminal.
    // The Tab key is sent to the PTY as a normal keypress for shell
    // completion, so we must keep focus here.
    return false;
}

void TerminalWidget::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    m_hasFocus = true;
    sendFocusEvent(true);
    update();
}

void TerminalWidget::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    m_hasFocus = false;
    sendFocusEvent(false);
    update();
}

void TerminalWidget::sendFocusEvent(bool gained)
{
    if (!m_terminal || !m_ptySession)
        return;

    bool focusMode = false;
    if (ghostty_terminal_mode_get(m_terminal, GHOSTTY_MODE_FOCUS_EVENT,
                                   &focusMode) != GHOSTTY_SUCCESS)
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

void TerminalWidget::wheelEvent(QWheelEvent *event)
{
    if (!m_terminal)
        return;

    bool mouseTracking = false;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_MOUSE_TRACKING,
                         &mouseTracking);
    if (mouseTracking)
        return; // minimal implementation: ignore wheel in mouse-tracking mode

    int delta = event->angleDelta().y();
    if (delta != 0) {
        int rows = delta > 0 ? -3 : 3;
        GhosttyTerminalScrollViewport sv = {
            .tag = GHOSTTY_SCROLL_VIEWPORT_DELTA,
            .value = { .delta = static_cast<intptr_t>(rows) },
        };
        ghostty_terminal_scroll_viewport(m_terminal, sv);
        update();
    }
}

void TerminalWidget::onPtyDataReceived(const QByteArray &data)
{
    if (!m_terminal)
        return;
    ghostty_terminal_vt_write(
        m_terminal, reinterpret_cast<const uint8_t *>(data.constData()),
        static_cast<size_t>(data.size()));
    update();
}

void TerminalWidget::onPtySessionClosed()
{
    // Minimal implementation: nothing special on session close
}
