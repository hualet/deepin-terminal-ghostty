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
#include <QUrl>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <mutex>
#include <optional>
#include <vector>

namespace {

constexpr int kInitialPtyCoalesceIntervalMs = 1;
constexpr int kBurstRenderIntervalMs = 8;
constexpr int kResizeCoalesceIntervalMs = 8;
constexpr int kImmediatePtyFlushBytes = 32 * 1024;
constexpr int kMaxOscScanBufferBytes = 16 * 1024;
constexpr uint32_t kMaxCellGraphemeCodepoints = 4096;
constexpr uint64_t kKittyImageStorageLimitBytes = 64 * 1024 * 1024;
constexpr size_t kKittyApcMaxBytes = 80 * 1024 * 1024;
constexpr size_t kBytesPerScrollbackLine = 20 * 1000;
constexpr size_t kMinimumScrollbackBytes = 100 * 1000 * 1000;

QColor faintForeground(const QColor &foreground, const QColor &background) {
    return QColor((foreground.red() + background.red()) / 2, (foreground.green() + background.green()) / 2,
                  (foreground.blue() + background.blue()) / 2);
}

enum BoxDrawingEdge {
    BoxDrawNone = 0,
    BoxDrawUp = 1 << 0,
    BoxDrawRight = 1 << 1,
    BoxDrawDown = 1 << 2,
    BoxDrawLeft = 1 << 3,
};

int boxDrawingEdges(uint32_t codepoint) {
    switch (codepoint) {
        case 0x2500: // BOX DRAWINGS LIGHT HORIZONTAL
        case 0x2501: // BOX DRAWINGS HEAVY HORIZONTAL
            return BoxDrawLeft | BoxDrawRight;
        case 0x2502: // BOX DRAWINGS LIGHT VERTICAL
        case 0x2503: // BOX DRAWINGS HEAVY VERTICAL
            return BoxDrawUp | BoxDrawDown;
        case 0x250C: // BOX DRAWINGS LIGHT DOWN AND RIGHT
        case 0x250F: // BOX DRAWINGS HEAVY DOWN AND RIGHT
        case 0x256D: // BOX DRAWINGS LIGHT ARC DOWN AND RIGHT
            return BoxDrawRight | BoxDrawDown;
        case 0x2510: // BOX DRAWINGS LIGHT DOWN AND LEFT
        case 0x2513: // BOX DRAWINGS HEAVY DOWN AND LEFT
        case 0x256E: // BOX DRAWINGS LIGHT ARC DOWN AND LEFT
            return BoxDrawDown | BoxDrawLeft;
        case 0x2514: // BOX DRAWINGS LIGHT UP AND RIGHT
        case 0x2517: // BOX DRAWINGS HEAVY UP AND RIGHT
        case 0x2570: // BOX DRAWINGS LIGHT ARC UP AND RIGHT
            return BoxDrawUp | BoxDrawRight;
        case 0x2518: // BOX DRAWINGS LIGHT UP AND LEFT
        case 0x251B: // BOX DRAWINGS HEAVY UP AND LEFT
        case 0x256F: // BOX DRAWINGS LIGHT ARC UP AND LEFT
            return BoxDrawUp | BoxDrawLeft;
        case 0x251C: // BOX DRAWINGS LIGHT VERTICAL AND RIGHT
        case 0x2523: // BOX DRAWINGS HEAVY VERTICAL AND RIGHT
            return BoxDrawUp | BoxDrawRight | BoxDrawDown;
        case 0x2524: // BOX DRAWINGS LIGHT VERTICAL AND LEFT
        case 0x252B: // BOX DRAWINGS HEAVY VERTICAL AND LEFT
            return BoxDrawUp | BoxDrawDown | BoxDrawLeft;
        case 0x252C: // BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
        case 0x2533: // BOX DRAWINGS HEAVY DOWN AND HORIZONTAL
            return BoxDrawRight | BoxDrawDown | BoxDrawLeft;
        case 0x2534: // BOX DRAWINGS LIGHT UP AND HORIZONTAL
        case 0x253B: // BOX DRAWINGS HEAVY UP AND HORIZONTAL
            return BoxDrawUp | BoxDrawRight | BoxDrawLeft;
        case 0x253C: // BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
        case 0x254B: // BOX DRAWINGS HEAVY VERTICAL AND HORIZONTAL
            return BoxDrawUp | BoxDrawRight | BoxDrawDown | BoxDrawLeft;
        default:
            return BoxDrawNone;
    }
}

bool renderBoxDrawingCodepoint(QPainter &painter, const QRect &cellRect, uint32_t codepoint, const QColor &color,
                               bool bold) {
    const int edges = boxDrawingEdges(codepoint);
    if (edges == BoxDrawNone)
        return false;

    const QPen previousPen = painter.pen();
    QPen pen(color);
    pen.setWidth(bold || codepoint == 0x2501 || codepoint == 0x2503 || codepoint == 0x250F || codepoint == 0x2513
                         || codepoint == 0x2517 || codepoint == 0x251B || codepoint == 0x2523 || codepoint == 0x252B
                         || codepoint == 0x2533 || codepoint == 0x253B || codepoint == 0x254B
                     ? 2
                     : 1);
    pen.setCapStyle(Qt::SquareCap);
    painter.setPen(pen);

    const int cx = cellRect.left() + cellRect.width() / 2;
    const int cy = cellRect.top() + cellRect.height() / 2;
    if (edges & BoxDrawUp)
        painter.drawLine(cx, cellRect.top(), cx, cy);
    if (edges & BoxDrawRight)
        painter.drawLine(cx, cy, cellRect.right(), cy);
    if (edges & BoxDrawDown)
        painter.drawLine(cx, cy, cx, cellRect.bottom());
    if (edges & BoxDrawLeft)
        painter.drawLine(cellRect.left(), cy, cx, cy);

    painter.setPen(previousPen);
    return true;
}

bool decodeKittyPng(void *userdata, const GhosttyAllocator *allocator, const uint8_t *data, size_t data_len,
                    GhosttySysImage *out) {
    (void)userdata;
    if (!data || data_len == 0 || !out)
        return false;

    QImage decoded = QImage::fromData(data, static_cast<int>(data_len), "PNG");
    if (decoded.isNull())
        return false;

    QImage rgba = decoded.convertToFormat(QImage::Format_RGBA8888);
    if (rgba.isNull() || rgba.width() <= 0 || rgba.height() <= 0)
        return false;

    const size_t rowBytes = static_cast<size_t>(rgba.width()) * 4;
    const size_t dataLen = rowBytes * static_cast<size_t>(rgba.height());
    uint8_t *pixels = ghostty_alloc(allocator, dataLen);
    if (!pixels)
        return false;

    for (int y = 0; y < rgba.height(); ++y)
        std::memcpy(pixels + rowBytes * static_cast<size_t>(y), rgba.constScanLine(y), rowBytes);

    out->width = static_cast<uint32_t>(rgba.width());
    out->height = static_cast<uint32_t>(rgba.height());
    out->data = pixels;
    out->data_len = dataLen;
    return true;
}

void ensureGhosttySysCallbacks() {
    static std::once_flag once;
    std::call_once(once, []() {
        const GhosttyResult result =
            ghostty_sys_set(GHOSTTY_SYS_OPT_DECODE_PNG, reinterpret_cast<const void *>(decodeKittyPng));
        if (result != GHOSTTY_SUCCESS)
            qCWarning(terminalLog) << "Failed to install Ghostty PNG decoder callback" << result;
    });
}

void appendCodepoint(QString &text, uint32_t codepoint) {
    if (codepoint <= 0xFFFF) {
        // Surrogate range (0xD800-0xDFFF) is not a valid Unicode scalar value.
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
            text.append(QChar::ReplacementCharacter);
            return;
        }
        text.append(QChar(static_cast<ushort>(codepoint)));
        return;
    }

    if (codepoint <= 0x10FFFF) {
        text.append(QChar::fromUcs4(codepoint));
        return;
    }

    text.append(QChar::ReplacementCharacter);
}

QString textFromRenderCellGraphemes(GhosttyRenderStateRowCells cells, uint32_t graphemeLen) {
    if (graphemeLen == 0)
        return {};

    if (graphemeLen > kMaxCellGraphemeCodepoints)
        return QString(QChar::ReplacementCharacter);

    std::array<uint32_t, 8> stackCodepoints = {};
    std::vector<uint32_t> heapCodepoints;
    uint32_t *codepoints = stackCodepoints.data();
    if (graphemeLen > stackCodepoints.size()) {
        heapCodepoints.resize(graphemeLen);
        codepoints = heapCodepoints.data();
    }

    if (ghostty_render_state_row_cells_get(cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_BUF, codepoints)
        != GHOSTTY_SUCCESS) {
        return {};
    }

    QString text;
    text.reserve(static_cast<int>(graphemeLen));
    for (uint32_t i = 0; i < graphemeLen; ++i)
        appendCodepoint(text, codepoints[i]);
    return text;
}

QString decodeVscodeOsc633Value(const QByteArray &encoded) {
    QString decoded;
    decoded.reserve(encoded.size());

    for (int i = 0; i < encoded.size(); ++i) {
        const char ch = encoded.at(i);
        if (ch != '\\') {
            decoded.append(QChar::fromLatin1(ch));
            continue;
        }

        if (i + 1 >= encoded.size()) {
            decoded.append(QChar::fromLatin1(ch));
            continue;
        }

        const char next = encoded.at(i + 1);
        if (next == '\\') {
            decoded.append(QChar::fromLatin1('\\'));
            ++i;
            continue;
        }

        if (next == 'x' && i + 3 < encoded.size()) {
            bool ok = false;
            const int code = encoded.mid(i + 2, 2).toInt(&ok, 16);
            if (ok) {
                decoded.append(QChar(static_cast<ushort>(code)));
                i += 3;
                continue;
            }
        }

        decoded.append(QChar::fromLatin1(ch));
    }

    return decoded;
}

QString commandFromVscodeOsc633(const QByteArray &payload) {
    static constexpr QByteArrayView kPrefix("633;E;");
    if (!payload.startsWith(kPrefix))
        return {};

    const QByteArray parameters = payload.mid(kPrefix.size());
    const int nonceSeparator = parameters.indexOf(';');
    const QByteArray encodedCommand = nonceSeparator < 0 ? parameters : parameters.left(nonceSeparator);
    return decodeVscodeOsc633Value(encodedCommand).trimmed();
}

QString commandFromWezTermUserVar(const QByteArray &payload) {
    static constexpr QByteArrayView kPrefix("1337;SetUserVar=WEZTERM_PROG=");
    if (!payload.startsWith(kPrefix))
        return {};

    return QString::fromUtf8(QByteArray::fromBase64(payload.mid(kPrefix.size()))).trimmed();
}

QString commandFromFinalTermOsc133(const QByteArray &payload) {
    if (!payload.startsWith("133;"))
        return {};

    const QList<QByteArray> parameters = payload.split(';');
    for (const QByteArray &parameter : parameters) {
        static constexpr QByteArrayView kPrefix("cmdline_url=");
        if (!parameter.startsWith(kPrefix))
            continue;

        const QByteArray encodedCommand = parameter.mid(kPrefix.size());
        const QUrl url(QString::fromUtf8(encodedCommand));
        if (url.isValid() && !url.scheme().isEmpty()) {
            const QString decodedUrl = url.toString(QUrl::FullyDecoded);
            return decodedUrl.isEmpty() ? QUrl::fromPercentEncoding(encodedCommand).trimmed() : decodedUrl.trimmed();
        }

        return QUrl::fromPercentEncoding(encodedCommand).trimmed();
    }

    return {};
}

std::optional<QString> commandFromQtGhosttyShellCommand(const QByteArray &payload) {
    static constexpr QByteArrayView kPrefix("777;ShellCommand=");
    if (!payload.startsWith(kPrefix))
        return std::nullopt;

    return QString::fromUtf8(QByteArray::fromBase64(payload.mid(kPrefix.size()))).trimmed();
}

std::optional<int> commandResultFromQtGhosttyShellCommandResult(const QByteArray &payload) {
    static constexpr QByteArrayView kPrefix("777;ShellCommandResult=");
    if (!payload.startsWith(kPrefix))
        return std::nullopt;

    const QByteArray value = payload.mid(kPrefix.size());
    bool ok = false;
    const int exitCode = value.toInt(&ok);
    if (!ok)
        return std::nullopt;
    return exitCode;
}

std::optional<QString> shellCommandFromOscPayload(const QByteArray &payload) {
    const std::optional<QString> qtGhosttyCommand = commandFromQtGhosttyShellCommand(payload);
    if (qtGhosttyCommand.has_value())
        return qtGhosttyCommand;

    const QString vscodeCommand = commandFromVscodeOsc633(payload);
    if (!vscodeCommand.isEmpty())
        return vscodeCommand;

    const QString wezTermCommand = commandFromWezTermUserVar(payload);
    if (!wezTermCommand.isEmpty())
        return wezTermCommand;

    const QString finalTermCommand = commandFromFinalTermOsc133(payload);
    if (!finalTermCommand.isEmpty())
        return finalTermCommand;

    return std::nullopt;
}

bool isCursorOnlyPtyData(const QByteArray &data) {
    for (int i = 0; i < data.size();) {
        const unsigned char ch = static_cast<unsigned char>(data.at(i));
        if (ch == '\r' || ch == '\b') {
            ++i;
            continue;
        }

        if (ch != 0x1B || i + 1 >= data.size() || data.at(i + 1) != '[')
            return false;

        i += 2;
        while (i < data.size()) {
            const unsigned char param = static_cast<unsigned char>(data.at(i));
            if ((param >= '0' && param <= '9') || param == ';' || param == '?' || param == ' ') {
                ++i;
                continue;
            }
            break;
        }

        if (i >= data.size())
            return false;

        const char final = data.at(i++);
        switch (final) {
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'G':
            case 'H':
            case 'f':
                break;
            default:
                return false;
        }
    }

    return !data.isEmpty();
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
    auto *widget = static_cast<TerminalWidget *>(userdata);
    *out_scheme = widget->m_isDark ? GHOSTTY_COLOR_SCHEME_DARK : GHOSTTY_COLOR_SCHEME_LIGHT;
    return true;
}

// ---------------------------------------------------------------------------
// TerminalWidget
// ---------------------------------------------------------------------------

TerminalWidget::TerminalWidget(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("terminalWidget"));
    setAccessibleName(tr("Terminal pane"));
    setAccessibleDescription(tr("Interactive terminal input and output area."));
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
        const QRect previousCursorRect = cursorPaintRect();
        m_cursorBlinkVisible = !m_cursorBlinkVisible;
        const QRect currentCursorRect = cursorPaintRect();
        update(previousCursorRect.united(currentCursorRect).adjusted(-1, -1, 1, 1));
    });

    m_renderTimer = new QTimer(this);
    m_renderTimer->setSingleShot(true);
    connect(m_renderTimer, &QTimer::timeout, this, &TerminalWidget::onRenderTimerTimeout);

    m_resizeTimer = new QTimer(this);
    m_resizeTimer->setSingleShot(true);
    connect(m_resizeTimer, &QTimer::timeout, this, [this]() { applyPendingResize(); });
}

TerminalWidget::~TerminalWidget() {
    if (m_mouseEvent)
        ghostty_mouse_event_free(m_mouseEvent);
    if (m_mouseEncoder)
        ghostty_mouse_encoder_free(m_mouseEncoder);
    if (m_keyEvent)
        ghostty_key_event_free(m_keyEvent);
    if (m_keyEncoder)
        ghostty_key_encoder_free(m_keyEncoder);
    if (m_rowCells)
        ghostty_render_state_row_cells_free(m_rowCells);
    if (m_rowIter)
        ghostty_render_state_row_iterator_free(m_rowIter);
    if (m_kittyPlacementIter)
        ghostty_kitty_graphics_placement_iterator_free(m_kittyPlacementIter);
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
        qCCritical(terminalLog) << "Failed to initialize Ghostty encoder state";
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

    updateViewportScrollState();
    qCInfo(terminalLog) << "Terminal widget initialized with size" << m_cols << "x" << m_rows;
    return true;
}

int TerminalWidget::terminalColumns() const {
    return m_cols;
}

int TerminalWidget::terminalRows() const {
    return m_rows;
}

bool TerminalWidget::hasRunningProcess() const {
    return m_ptySession && m_ptySession->hasRunningProcess();
}

TerminalWidget::ViewportScrollState TerminalWidget::viewportScrollState() const {
    return queryViewportScrollState();
}

void TerminalWidget::scrollViewportBy(int deltaRows) {
    if (!m_terminal || deltaRows == 0)
        return;

    GhosttyTerminalScrollViewport sv = {
        .tag = GHOSTTY_SCROLL_VIEWPORT_DELTA,
        .value = {.delta = static_cast<intptr_t>(deltaRows)},
    };
    ghostty_terminal_scroll_viewport(m_terminal, sv);
    m_renderStateDirty = true;
    updateViewportScrollState();
    update();
}

void TerminalWidget::scrollViewportToOffset(int offset) {
    const ViewportScrollState state = queryViewportScrollState();
    const int targetOffset = qBound(0, offset, state.maximumOffset());
    scrollViewportBy(targetOffset - state.offset);
}

QString TerminalWidget::workingDirectory() const {
    if (!m_ptySession)
        return {};
    return m_ptySession->workingDirectory();
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

int TerminalWidget::debugLastFrameTextRunCount() const {
    return m_debugLastFrameTextRunCount;
}

int TerminalWidget::debugLastFrameLineDrawCount() const {
    return m_debugLastFrameLineDrawCount;
}

int TerminalWidget::debugResizeApplyCount() const {
    return m_debugResizeApplyCount;
}

int TerminalWidget::debugPtyFlushCount() const {
    return m_debugPtyFlushCount;
}

int TerminalWidget::debugCursorOnlyRepaintCount() const {
    return m_debugCursorOnlyRepaintCount;
}

int TerminalWidget::debugScrollbackLines() const {
    return m_scrollbackLines;
}

size_t TerminalWidget::debugScrollbackByteBudget() const {
    return scrollbackByteBudget();
}

void TerminalWidget::debugSetSelection(int startRow, int startCol, int endRow, int endCol, bool active) {
    m_selection.startRow = startRow;
    m_selection.startCol = startCol;
    m_selection.endRow = endRow;
    m_selection.endCol = endCol;
    m_selection.active = active;
}

bool TerminalWidget::debugCellInSelection(int screenRow, int col) const {
    return cellInSelection(screenRow, col);
}

QString TerminalWidget::debugSelectedText() const {
    return selectedText();
}
#endif

bool TerminalWidget::setupTerminal() {
    ensureGhosttySysCallbacks();

    GhosttyTerminalOptions opts = {
        .cols = m_cols,
        .rows = m_rows,
        .max_scrollback = scrollbackByteBudget(),
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

    uint64_t kittyLimit = kKittyImageStorageLimitBytes;
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_KITTY_IMAGE_STORAGE_LIMIT, &kittyLimit);

    size_t kittyApcMaxBytes = kKittyApcMaxBytes;
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_APC_MAX_BYTES_KITTY, &kittyApcMaxBytes);

    bool fileMedium = false;
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_KITTY_IMAGE_MEDIUM_FILE, &fileMedium);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_KITTY_IMAGE_MEDIUM_TEMP_FILE, &fileMedium);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_KITTY_IMAGE_MEDIUM_SHARED_MEM, &fileMedium);

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

    err = ghostty_kitty_graphics_placement_iterator_new(nullptr, &m_kittyPlacementIter);
    if (err != GHOSTTY_SUCCESS) {
        std::fprintf(stderr, "ghostty_kitty_graphics_placement_iterator_new failed (%d)\n", err);
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

    err = ghostty_mouse_encoder_new(nullptr, &m_mouseEncoder);
    if (err != GHOSTTY_SUCCESS) {
        std::fprintf(stderr, "ghostty_mouse_encoder_new failed (%d)\n", err);
        return false;
    }

    err = ghostty_mouse_event_new(nullptr, &m_mouseEvent);
    if (err != GHOSTTY_SUCCESS) {
        std::fprintf(stderr, "ghostty_mouse_event_new failed (%d)\n", err);
        return false;
    }

    GhosttyMouseEncoderSize encoderSize = GHOSTTY_INIT_SIZED(GhosttyMouseEncoderSize);
    encoderSize.cell_width = static_cast<uint32_t>(m_cellWidth);
    encoderSize.cell_height = static_cast<uint32_t>(m_cellHeight);
    encoderSize.screen_width = static_cast<uint32_t>(m_cellWidth) * m_cols;
    encoderSize.screen_height = static_cast<uint32_t>(m_cellHeight) * m_rows;
    ghostty_mouse_encoder_setopt(m_mouseEncoder, GHOSTTY_MOUSE_ENCODER_OPT_SIZE, &encoderSize);

    bool trackLastCell = true;
    ghostty_mouse_encoder_setopt(m_mouseEncoder, GHOSTTY_MOUSE_ENCODER_OPT_TRACK_LAST_CELL, &trackLastCell);

    return true;
}

void TerminalWidget::updateGridSize() {
    const QRect contentRect = terminalContentRect();
    int w = contentRect.width();
    int h = contentRect.height();
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

bool TerminalWidget::ensureBackBuffer() {
    const qreal devicePixelRatio = devicePixelRatioF();
    const QRect contentRect = terminalContentRect();
    const QSize pixelSize(qMax(1, qRound(contentRect.width() * devicePixelRatio)),
                          qMax(1, qRound(contentRect.height() * devicePixelRatio)));
    if (pixelSize.isEmpty()) {
        m_backBuffer = QImage();
        return false;
    }

    if (m_backBuffer.size() == pixelSize && qFuzzyCompare(m_backBuffer.devicePixelRatio(), devicePixelRatio)) {
        return false;
    }

    m_backBuffer = QImage(pixelSize, QImage::Format_ARGB32_Premultiplied);
    m_backBuffer.setDevicePixelRatio(devicePixelRatio);
    m_backBuffer.fill(Qt::transparent);
    return true;
}

void TerminalWidget::renderTerminal(QPainter &painter) {
    if (!syncRenderState())
        return;

    GhosttyResult err = GHOSTTY_SUCCESS;
    GhosttyRenderStateColors colors = GHOSTTY_INIT_SIZED(GhosttyRenderStateColors);
    err = ghostty_render_state_colors_get(m_renderState, &colors);
    if (err != GHOSTTY_SUCCESS)
        return;

    const QColor bgWithAlpha(colors.background.r, colors.background.g, colors.background.b, qRound(m_opacity * 255));

    const QRect contentRect = terminalContentRect();
    const QPoint contentOrigin = contentRect.topLeft();
    const QRect widgetRect = rect();
    if (contentRect != widgetRect) {
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        if (contentRect.top() > widgetRect.top())
            painter.fillRect(
                QRect(widgetRect.left(), widgetRect.top(), widgetRect.width(), contentRect.top() - widgetRect.top()),
                bgWithAlpha);
        if (contentRect.bottom() < widgetRect.bottom())
            painter.fillRect(QRect(widgetRect.left(), contentRect.bottom() + 1, widgetRect.width(),
                                   widgetRect.bottom() - contentRect.bottom()),
                             bgWithAlpha);
        if (contentRect.left() > widgetRect.left())
            painter.fillRect(QRect(widgetRect.left(), contentRect.top(), contentRect.left() - widgetRect.left(),
                                   contentRect.height()),
                             bgWithAlpha);
        if (contentRect.right() < widgetRect.right())
            painter.fillRect(QRect(contentRect.right() + 1, contentRect.top(), widgetRect.right() - contentRect.right(),
                                   contentRect.height()),
                             bgWithAlpha);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    err = ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &m_rowIter);
    if (err != GHOSTTY_SUCCESS)
        return;

    const bool bufferRecreated = ensureBackBuffer();
    if (m_backBuffer.isNull())
        return;

    GhosttyRenderStateDirty dirtyState = GHOSTTY_RENDER_STATE_DIRTY_FULL;
    ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_DIRTY, &dirtyState);

    const int currentViewportOffset = viewportScrollState().offset;
    const bool viewportChanged = currentViewportOffset != m_backBufferViewportOffset;
    const bool fullRedraw = bufferRecreated || viewportChanged || (dirtyState == GHOSTTY_RENDER_STATE_DIRTY_FULL);
    if (!fullRedraw && dirtyState == GHOSTTY_RENDER_STATE_DIRTY_FALSE) {
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(contentOrigin, m_backBuffer);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        return;
    }

    QPainter backPainter(&m_backBuffer);
    backPainter.setFont(m_font);
    if (fullRedraw) {
        backPainter.setCompositionMode(QPainter::CompositionMode_Source);
        backPainter.fillRect(QRect(QPoint(0, 0), contentRect.size()), bgWithAlpha);
        backPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

#ifdef QTGHOSTTY_TESTING
    m_debugLastFrameRenderedRowCount = 0;
    m_debugLastFrameDirtyRowCount = 0;
    m_debugLastFrameWasFullRedraw = fullRedraw;
    m_debugLastFrameTextRunCount = 0;
    m_debugLastFrameLineDrawCount = 0;
#endif

    renderKittyGraphicsLayer(backPainter, GHOSTTY_KITTY_PLACEMENT_LAYER_BELOW_BG);

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

        renderRow(backPainter, y, colors, RowRenderPass::Background);

        y += m_cellHeight;
    }

    renderKittyGraphicsLayer(backPainter, GHOSTTY_KITTY_PLACEMENT_LAYER_BELOW_TEXT);

    err = ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &m_rowIter);
    if (err != GHOSTTY_SUCCESS)
        return;

    y = 0;
    while (ghostty_render_state_row_iterator_next(m_rowIter)) {
        bool rowDirty = true;
        ghostty_render_state_row_get(m_rowIter, GHOSTTY_RENDER_STATE_ROW_DATA_DIRTY, &rowDirty);
        if (!fullRedraw && !rowDirty) {
            y += m_cellHeight;
            continue;
        }

        renderRow(backPainter, y, colors, RowRenderPass::Text);

        bool clean = false;
        ghostty_render_state_row_set(m_rowIter, GHOSTTY_RENDER_STATE_ROW_OPTION_DIRTY, &clean);

        y += m_cellHeight;
    }
    renderKittyGraphicsLayer(backPainter, GHOSTTY_KITTY_PLACEMENT_LAYER_ABOVE_TEXT);
    GhosttyRenderStateDirty cleanState = GHOSTTY_RENDER_STATE_DIRTY_FALSE;
    ghostty_render_state_set(m_renderState, GHOSTTY_RENDER_STATE_OPTION_DIRTY, &cleanState);
    m_backBufferViewportOffset = currentViewportOffset;
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(contentOrigin, m_backBuffer);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
}

bool TerminalWidget::renderKittyGraphicsLayer(QPainter &painter, GhosttyKittyPlacementLayer layer) {
    if (!m_terminal || !m_kittyPlacementIter)
        return false;

    GhosttyKittyGraphics graphics = nullptr;
    if (ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_KITTY_GRAPHICS, &graphics) != GHOSTTY_SUCCESS
        || !graphics) {
        return false;
    }

    if (ghostty_kitty_graphics_get(graphics, GHOSTTY_KITTY_GRAPHICS_DATA_PLACEMENT_ITERATOR, &m_kittyPlacementIter)
        != GHOSTTY_SUCCESS) {
        return false;
    }

    if (ghostty_kitty_graphics_placement_iterator_set(m_kittyPlacementIter,
                                                      GHOSTTY_KITTY_GRAPHICS_PLACEMENT_ITERATOR_OPTION_LAYER, &layer)
        != GHOSTTY_SUCCESS) {
        return false;
    }

    bool rendered = false;
    painter.save();
    painter.setClipRect(QRect(QPoint(0, 0), terminalContentRect().size()));
    while (ghostty_kitty_graphics_placement_next(m_kittyPlacementIter))
        rendered = renderKittyPlacement(painter, graphics) || rendered;
    painter.restore();
    return rendered;
}

bool TerminalWidget::renderKittyPlacement(QPainter &painter, GhosttyKittyGraphics graphics) {
    uint32_t imageId = 0;
    if (ghostty_kitty_graphics_placement_get(m_kittyPlacementIter, GHOSTTY_KITTY_GRAPHICS_PLACEMENT_DATA_IMAGE_ID,
                                             &imageId)
        != GHOSTTY_SUCCESS) {
        return false;
    }

    GhosttyKittyGraphicsImage image = ghostty_kitty_graphics_image(graphics, imageId);
    if (!image)
        return false;

    GhosttyKittyGraphicsPlacementRenderInfo renderInfo = GHOSTTY_INIT_SIZED(GhosttyKittyGraphicsPlacementRenderInfo);
    if (ghostty_kitty_graphics_placement_render_info(m_kittyPlacementIter, image, m_terminal, &renderInfo)
            != GHOSTTY_SUCCESS
        || !renderInfo.viewport_visible || renderInfo.pixel_width == 0 || renderInfo.pixel_height == 0
        || renderInfo.source_width == 0 || renderInfo.source_height == 0) {
        return false;
    }

    uint32_t xOffset = 0;
    uint32_t yOffset = 0;
    ghostty_kitty_graphics_placement_get(m_kittyPlacementIter, GHOSTTY_KITTY_GRAPHICS_PLACEMENT_DATA_X_OFFSET,
                                         &xOffset);
    ghostty_kitty_graphics_placement_get(m_kittyPlacementIter, GHOSTTY_KITTY_GRAPHICS_PLACEMENT_DATA_Y_OFFSET,
                                         &yOffset);

    const QImage qtImage = imageForKittyImage(image);
    if (qtImage.isNull())
        return false;

    const QRectF sourceRect(renderInfo.source_x, renderInfo.source_y, renderInfo.source_width,
                            renderInfo.source_height);
    const QRectF targetRect(static_cast<qreal>(renderInfo.viewport_col) * m_cellWidth + xOffset,
                            static_cast<qreal>(renderInfo.viewport_row) * m_cellHeight + yOffset,
                            renderInfo.pixel_width, renderInfo.pixel_height);
    painter.drawImage(targetRect, qtImage, sourceRect);
    return true;
}

QImage TerminalWidget::imageForKittyImage(GhosttyKittyGraphicsImage image) {
    uint32_t imageId = 0;
    if (ghostty_kitty_graphics_image_get(image, GHOSTTY_KITTY_IMAGE_DATA_ID, &imageId) != GHOSTTY_SUCCESS)
        return {};

    const auto cached = m_kittyImageCache.constFind(imageId);
    if (cached != m_kittyImageCache.constEnd())
        return cached.value();

    uint32_t width = 0;
    uint32_t height = 0;
    GhosttyKittyImageFormat format = GHOSTTY_KITTY_IMAGE_FORMAT_RGBA;
    GhosttyKittyImageCompression compression = GHOSTTY_KITTY_IMAGE_COMPRESSION_NONE;
    const uint8_t *data = nullptr;
    size_t dataLen = 0;
    constexpr GhosttyKittyGraphicsImageData kImageDataKeys[] = {
        GHOSTTY_KITTY_IMAGE_DATA_WIDTH,       GHOSTTY_KITTY_IMAGE_DATA_HEIGHT,   GHOSTTY_KITTY_IMAGE_DATA_FORMAT,
        GHOSTTY_KITTY_IMAGE_DATA_COMPRESSION, GHOSTTY_KITTY_IMAGE_DATA_DATA_PTR, GHOSTTY_KITTY_IMAGE_DATA_DATA_LEN,
    };
    void *imageDataValues[] = {&width, &height, &format, &compression, &data, &dataLen};
    size_t written = 0;
    if (ghostty_kitty_graphics_image_get_multi(image, std::size(kImageDataKeys), kImageDataKeys, imageDataValues,
                                               &written)
            != GHOSTTY_SUCCESS
        || written != std::size(kImageDataKeys) || width == 0 || height == 0 || !data
        || compression != GHOSTTY_KITTY_IMAGE_COMPRESSION_NONE) {
        return {};
    }

    QImage qtImage;
    const qsizetype w = static_cast<qsizetype>(width);
    const qsizetype h = static_cast<qsizetype>(height);
    switch (format) {
        case GHOSTTY_KITTY_IMAGE_FORMAT_RGB: {
            const qsizetype stride = w * 3;
            if (dataLen < static_cast<size_t>(stride * h))
                return {};
            qtImage = QImage(data, static_cast<int>(width), static_cast<int>(height), static_cast<int>(stride),
                             QImage::Format_RGB888)
                          .copy();
            break;
        }
        case GHOSTTY_KITTY_IMAGE_FORMAT_RGBA: {
            const qsizetype stride = w * 4;
            if (dataLen < static_cast<size_t>(stride * h))
                return {};
            qtImage = QImage(data, static_cast<int>(width), static_cast<int>(height), static_cast<int>(stride),
                             QImage::Format_RGBA8888)
                          .copy();
            break;
        }
        case GHOSTTY_KITTY_IMAGE_FORMAT_GRAY: {
            const qsizetype stride = w;
            if (dataLen < static_cast<size_t>(stride * h))
                return {};
            qtImage = QImage(data, static_cast<int>(width), static_cast<int>(height), static_cast<int>(stride),
                             QImage::Format_Grayscale8)
                          .convertToFormat(QImage::Format_RGBA8888);
            break;
        }
        case GHOSTTY_KITTY_IMAGE_FORMAT_GRAY_ALPHA: {
            const qsizetype stride = w * 2;
            if (dataLen < static_cast<size_t>(stride * h))
                return {};
            qtImage = QImage(static_cast<int>(width), static_cast<int>(height), QImage::Format_RGBA8888);
            for (uint32_t y = 0; y < height; ++y) {
                auto *dst = qtImage.scanLine(static_cast<int>(y));
                const uint8_t *src = data + static_cast<size_t>(y) * static_cast<size_t>(stride);
                for (uint32_t x = 0; x < width; ++x) {
                    dst[x * 4 + 0] = src[x * 2 + 0];
                    dst[x * 4 + 1] = src[x * 2 + 0];
                    dst[x * 4 + 2] = src[x * 2 + 0];
                    dst[x * 4 + 3] = src[x * 2 + 1];
                }
            }
            break;
        }
        default:
            return {};
    }

    if (!qtImage.isNull())
        m_kittyImageCache.insert(imageId, qtImage);
    return qtImage;
}

void TerminalWidget::renderRow(QPainter &painter, int y, const GhosttyRenderStateColors &colors, RowRenderPass pass) {
    if (ghostty_render_state_row_get(m_rowIter, GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &m_rowCells) != GHOSTTY_SUCCESS)
        return;

    const bool drawBackground = pass == RowRenderPass::Background || pass == RowRenderPass::Full;
    const bool drawText = pass == RowRenderPass::Text || pass == RowRenderPass::Full;
    const QColor defaultBackground(colors.background.r, colors.background.g, colors.background.b,
                                   qRound(m_opacity * 255));
    const QColor defaultForeground(colors.foreground.r, colors.foreground.g, colors.foreground.b);
    if (drawBackground) {
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(0, y, terminalContentRect().width(), m_cellHeight, defaultBackground);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    constexpr GhosttyRenderStateRowCellsData kCellDataKeys[] = {
        GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_RAW,
        GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_STYLE,
        GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_LEN,
    };

    const QFont *activeFont = nullptr;
    QColor activeForeground;
    struct TextRun {
        QString text;
        QVector<QString> cellTexts;
        int x = 0;
        int cells = 0;
        const QFont *font = nullptr;
        QColor foreground;
        QColor decorationColor;
        int underline = GHOSTTY_SGR_UNDERLINE_NONE;
        bool strikethrough = false;
        bool overline = false;
    };
    const auto drawTextDecorations = [&](int x, int cells, const QColor &color, int underline, bool strikethrough,
                                         bool overline) {
        if (underline == GHOSTTY_SGR_UNDERLINE_NONE && !strikethrough && !overline)
            return;

        const QPen previousPen = painter.pen();
        QPen pen(color);
        switch (underline) {
            case GHOSTTY_SGR_UNDERLINE_DOTTED:
                pen.setStyle(Qt::DotLine);
                break;
            case GHOSTTY_SGR_UNDERLINE_DASHED:
                pen.setStyle(Qt::DashLine);
                break;
            default:
                pen.setStyle(Qt::SolidLine);
                break;
        }
        painter.setPen(pen);

        const int right = x + cells * m_cellWidth - 1;
        if (overline)
            painter.drawLine(x, y + 1, right, y + 1);
        if (strikethrough)
            painter.drawLine(x, y + m_cellHeight / 2, right, y + m_cellHeight / 2);
        if (underline != GHOSTTY_SGR_UNDERLINE_NONE) {
            const int underlineY = qMin(y + m_cellHeight - 1, y + m_fontAscent + 2);
            painter.drawLine(x, underlineY, right, underlineY);
            if (underline == GHOSTTY_SGR_UNDERLINE_DOUBLE)
                painter.drawLine(x, qMin(y + m_cellHeight - 1, underlineY + 2), right,
                                 qMin(y + m_cellHeight - 1, underlineY + 2));
        }
        painter.setPen(previousPen);
    };
    TextRun textRun;
    const auto flushTextRun = [&]() {
        if (textRun.text.isEmpty())
            return;

        if (activeFont != textRun.font) {
            painter.setFont(*textRun.font);
            activeFont = textRun.font;
        }
        if (activeForeground != textRun.foreground) {
            painter.setPen(textRun.foreground);
            activeForeground = textRun.foreground;
        }

        painter.setLayoutDirection(Qt::LeftToRight);
        int cellX = textRun.x;
        for (const QString &cellText : textRun.cellTexts) {
            painter.drawText(cellX, y + m_fontAscent, cellText);
            cellX += m_cellWidth;
        }
        drawTextDecorations(textRun.x, textRun.cells, textRun.decorationColor, textRun.underline, textRun.strikethrough,
                            textRun.overline);
#ifdef QTGHOSTTY_TESTING
        ++m_debugLastFrameTextRunCount;
#endif
        textRun = {};
    };
    const auto appendTextRun = [&](int runX, const QFont *font, const QColor &foreground, const QColor &decorationColor,
                                   int underline, bool strikethrough, bool overline, const QString &text) {
        if (textRun.font == font && textRun.foreground == foreground && textRun.decorationColor == decorationColor
            && textRun.underline == underline && textRun.strikethrough == strikethrough && textRun.overline == overline
            && textRun.x + textRun.cells * m_cellWidth == runX) {
            textRun.text.append(text);
            textRun.cellTexts.append(text);
            ++textRun.cells;
            return;
        }

        flushTextRun();
        textRun.x = runX;
        textRun.font = font;
        textRun.foreground = foreground;
        textRun.decorationColor = decorationColor;
        textRun.underline = underline;
        textRun.strikethrough = strikethrough;
        textRun.overline = overline;
        textRun.text = text;
        textRun.cellTexts = {text};
        textRun.cells = 1;
    };
    const auto appendTextRunCodepoint = [&](int runX, const QFont *font, const QColor &foreground,
                                            const QColor &decorationColor, int underline, bool strikethrough,
                                            bool overline, uint32_t codepoint) {
        if (textRun.font != font || textRun.foreground != foreground || textRun.decorationColor != decorationColor
            || textRun.underline != underline || textRun.strikethrough != strikethrough || textRun.overline != overline
            || textRun.x + textRun.cells * m_cellWidth != runX) {
            flushTextRun();
            textRun.x = runX;
            textRun.font = font;
            textRun.foreground = foreground;
            textRun.decorationColor = decorationColor;
            textRun.underline = underline;
            textRun.strikethrough = strikethrough;
            textRun.overline = overline;
        }

        QString cellText;
        appendCodepoint(cellText, codepoint);
        appendCodepoint(textRun.text, codepoint);
        textRun.cellTexts.append(cellText);
        ++textRun.cells;
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

        GhosttyCellWide cellWide = GHOSTTY_CELL_WIDE_NARROW;
        ghostty_cell_get(rawCell, GHOSTTY_CELL_DATA_WIDE, &cellWide);
        const bool isWideHead = cellWide == GHOSTTY_CELL_WIDE_WIDE;
        const bool isWideTail = cellWide == GHOSTTY_CELL_WIDE_SPACER_TAIL || cellWide == GHOSTTY_CELL_WIDE_SPACER_HEAD;
        const int cellRenderWidth = isWideHead ? m_cellWidth * 2 : m_cellWidth;

        GhosttyColorRgb bgColor = colors.background;
        const bool hasBg =
            ghostty_render_state_row_cells_get(m_rowCells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_BG_COLOR, &bgColor)
            == GHOSTTY_SUCCESS;

        GhosttyColorRgb fgColor = colors.foreground;
        const bool hasFg =
            ghostty_render_state_row_cells_get(m_rowCells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_FG_COLOR, &fgColor)
            == GHOSTTY_SUCCESS;

        if (style.inverse) {
            std::swap(fgColor, bgColor);
            if (drawText)
                flushTextRun();
            if (drawBackground && !isWideTail)
                painter.fillRect(x, y, cellRenderWidth, m_cellHeight, QColor(bgColor.r, bgColor.g, bgColor.b));
        } else if (drawBackground && hasBg && !isWideTail) {
            if (drawText)
                flushTextRun();
            painter.fillRect(x, y, cellRenderWidth, m_cellHeight, QColor(bgColor.r, bgColor.g, bgColor.b));
        }

        if (!drawText) {
            x += m_cellWidth;
            continue;
        }

        const QFont *cachedFont = &m_font;
        if (style.bold && style.italic)
            cachedFont = &m_fontBoldItalic;
        else if (style.bold)
            cachedFont = &m_fontBold;
        else if (style.italic)
            cachedFont = &m_fontItalic;
        QColor cellForeground = (style.inverse || hasFg) ? QColor(fgColor.r, fgColor.g, fgColor.b) : defaultForeground;
        if (style.faint)
            cellForeground = faintForeground(cellForeground, QColor(bgColor.r, bgColor.g, bgColor.b));
        QColor decorationColor = cellForeground;
        if (style.underline_color.tag == GHOSTTY_STYLE_COLOR_RGB) {
            const GhosttyColorRgb color = style.underline_color.value.rgb;
            decorationColor = QColor(color.r, color.g, color.b);
        }

        if (!isWideHead && !isWideTail) {
            if (graphemeLen == 0) {
                x += m_cellWidth;
                continue;
            }

            uint32_t codepoint = 0;
            const bool hasSingleCodepoint =
                graphemeLen == 1
                && ghostty_cell_get(rawCell, GHOSTTY_CELL_DATA_CODEPOINT, &codepoint) == GHOSTTY_SUCCESS
                && codepoint != 0;
            if (hasSingleCodepoint && !style.invisible && boxDrawingEdges(codepoint) != BoxDrawNone) {
                flushTextRun();
                renderBoxDrawingCodepoint(painter, QRect(x, y, m_cellWidth, m_cellHeight), codepoint, cellForeground,
                                          style.bold);
                drawTextDecorations(x, 1, decorationColor, style.underline, style.strikethrough, style.overline);
#ifdef QTGHOSTTY_TESTING
                ++m_debugLastFrameLineDrawCount;
#endif
                x += m_cellWidth;
                continue;
            }
            if (!style.inverse && !hasBg && hasSingleCodepoint) {
                if (!style.invisible)
                    appendTextRunCodepoint(x, cachedFont, cellForeground, decorationColor, style.underline,
                                           style.strikethrough, style.overline, codepoint);
                x += m_cellWidth;
                continue;
            }

            QString cellText;
            if (graphemeLen == 1) {
                if (hasSingleCodepoint) {
                    appendCodepoint(cellText, codepoint);
                } else {
                    cellText = textFromRenderCellGraphemes(m_rowCells, graphemeLen);
                }
            } else {
                cellText = textFromRenderCellGraphemes(m_rowCells, graphemeLen);
            }

            if (style.inverse || hasBg) {
                if (!style.invisible) {
                    if (activeFont != cachedFont) {
                        painter.setFont(*cachedFont);
                        activeFont = cachedFont;
                    }
                    if (activeForeground != cellForeground) {
                        painter.setPen(cellForeground);
                        activeForeground = cellForeground;
                    }

                    painter.setLayoutDirection(Qt::LeftToRight);
                    painter.drawText(x, y + m_fontAscent, cellText);
                    drawTextDecorations(x, 1, decorationColor, style.underline, style.strikethrough, style.overline);
#ifdef QTGHOSTTY_TESTING
                    ++m_debugLastFrameTextRunCount;
#endif
                }
            } else {
                if (!style.invisible)
                    appendTextRun(x, cachedFont, cellForeground, decorationColor, style.underline, style.strikethrough,
                                  style.overline, cellText);
            }
            x += m_cellWidth;
            continue;
        }

        flushTextRun();
        if (graphemeLen > 0 && !isWideTail && !style.invisible) {
            const QString text = textFromRenderCellGraphemes(m_rowCells, graphemeLen);

            QFont cellFont = *cachedFont;
            cellFont.setFixedPitch(false);

            painter.setFont(cellFont);
            painter.setPen(cellForeground);
            painter.setLayoutDirection(Qt::LeftToRight);
            painter.drawText(QRectF(x, y, cellRenderWidth, m_cellHeight), Qt::AlignCenter | Qt::TextSingleLine, text);
            drawTextDecorations(x, isWideHead ? 2 : 1, decorationColor, style.underline, style.strikethrough,
                                style.overline);
            activeFont = nullptr;
            activeForeground = QColor();
        }

        x += m_cellWidth;
    }
    flushTextRun();
}

void TerminalWidget::renderOverlays(QPainter &painter) const {
    if (!m_terminal || !m_renderState)
        return;

    const QPoint origin = terminalContentOrigin();
    size_t scrollOffset = 0;
    if (!m_searchMatches.isEmpty() || m_selection.active) {
        GhosttyTerminalScrollbar scrollbar = {};
        if (ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_SCROLLBAR, &scrollbar) == GHOSTTY_SUCCESS)
            scrollOffset = scrollbar.offset;
    }
    GhosttyColorRgb defaultFg = {};
    ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_COLOR_FOREGROUND, &defaultFg);
    QColor defaultForeground(defaultFg.r, defaultFg.g, defaultFg.b);

    if (!m_searchMatches.isEmpty() || m_selection.active) {
        for (int viewportRow = 0; viewportRow < static_cast<int>(m_rows); ++viewportRow) {
            const int y = origin.y() + viewportRow * m_cellHeight;
            const int screenRow = static_cast<int>(viewportRow + scrollOffset);
            for (int col = 0; col < static_cast<int>(m_cols); ++col) {
                const int x = origin.x() + col * m_cellWidth;
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

                if (cellInSelection(screenRow, col)) {
                    QColor selBg = m_selectionBackground.isValid()
                                       ? m_selectionBackground
                                       : QColor(defaultForeground.red(), defaultForeground.green(),
                                                defaultForeground.blue(), m_isDark ? 60 : 50);
                    painter.fillRect(x, y, m_cellWidth, m_cellHeight, selBg);
                }
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

        int curX = origin.x() + cx * m_cellWidth;
        int curY = origin.y() + cy * m_cellHeight;
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

GhosttyMods ghosttyMouseMods(Qt::KeyboardModifiers mods) {
    GhosttyMods result = 0;
    if (mods & Qt::ShiftModifier)
        result |= GHOSTTY_MODS_SHIFT;
    if (mods & Qt::ControlModifier)
        result |= GHOSTTY_MODS_CTRL;
    if (mods & Qt::AltModifier)
        result |= GHOSTTY_MODS_ALT;
    if (mods & Qt::MetaModifier)
        result |= GHOSTTY_MODS_SUPER;
    return result;
}

GhosttyMouseButton ghosttyMouseButtonForQt(Qt::MouseButton button) {
    switch (button) {
        case Qt::LeftButton:
            return GHOSTTY_MOUSE_BUTTON_LEFT;
        case Qt::RightButton:
            return GHOSTTY_MOUSE_BUTTON_RIGHT;
        case Qt::MiddleButton:
            return GHOSTTY_MOUSE_BUTTON_MIDDLE;
        default:
            return GHOSTTY_MOUSE_BUTTON_UNKNOWN;
    }
}

GhosttyMouseButton ghosttyMouseButtonForHeld(Qt::MouseButtons buttons) {
    if (buttons & Qt::LeftButton)
        return GHOSTTY_MOUSE_BUTTON_LEFT;
    if (buttons & Qt::MiddleButton)
        return GHOSTTY_MOUSE_BUTTON_MIDDLE;
    if (buttons & Qt::RightButton)
        return GHOSTTY_MOUSE_BUTTON_RIGHT;
    return GHOSTTY_MOUSE_BUTTON_UNKNOWN;
}

bool encodeAndSendMouseEvent(GhosttyMouseEncoder encoder, GhosttyMouseEvent event, PtySession *pty) {
    char buf[128];
    size_t written = 0;
    GhosttyResult err = ghostty_mouse_encoder_encode(encoder, event, buf, sizeof(buf), &written);
    if (err == GHOSTTY_SUCCESS && written > 0) {
        pty->write(QByteArray(buf, static_cast<int>(written)));
        return true;
    }
    return false;
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
    if (text.size() == 1 && !(event->modifiers() & Qt::AltModifier)) {
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
            return terminalContentRect();
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

QFont TerminalWidget::terminalFont() const {
    return m_font;
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
    updateViewportScrollState();
}

qreal TerminalWidget::opacity() const {
    return m_opacity;
}

void TerminalWidget::setOpacity(qreal opacity) {
    if (qFuzzyCompare(m_opacity, opacity))
        return;

    m_opacity = opacity;
    setAttribute(Qt::WA_TranslucentBackground, m_opacity < 1.0);
    m_backBuffer = QImage();
    update();
}

void TerminalWidget::applyTheme(const TerminalTheme &theme) {
    if (!m_terminal)
        return;

    auto toRgb = [](const QColor &c) -> GhosttyColorRgb {
        return {static_cast<uint8_t>(c.red()), static_cast<uint8_t>(c.green()), static_cast<uint8_t>(c.blue())};
    };

    auto fg = toRgb(theme.foreground);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_FOREGROUND, &fg);

    auto bg = toRgb(theme.background);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_BACKGROUND, &bg);

    auto cr = toRgb(theme.cursor);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_CURSOR, &cr);

    GhosttyColorRgb palette[256] = {};
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_COLOR_PALETTE, palette);
    for (int i = 0; i < 16; ++i)
        palette[i] = toRgb(theme.ansi[i]);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_PALETTE, palette);

    m_isDark = theme.isDark;
    m_selectionBackground = theme.selectionBackground;
    m_renderStateDirty = true;
    update();
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

    const int delta = event->angleDelta().y();
    if (delta == 0)
        return;

    bool mouseTracking = false;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_MOUSE_TRACKING, &mouseTracking);

    if (mouseTracking && m_mouseEncoder && m_mouseEvent && m_ptySession) {
        ghostty_mouse_encoder_setopt_from_terminal(m_mouseEncoder, m_terminal);
        GhosttyMouseButton button = delta > 0 ? GHOSTTY_MOUSE_BUTTON_FOUR : GHOSTTY_MOUSE_BUTTON_FIVE;
        ghostty_mouse_event_set_action(m_mouseEvent, GHOSTTY_MOUSE_ACTION_PRESS);
        ghostty_mouse_event_set_button(m_mouseEvent, button);
        ghostty_mouse_event_set_mods(m_mouseEvent, ghosttyMouseMods(event->modifiers()));
        GhosttyMousePosition pos = ghosttyMousePositionForEvent(event->position());
        ghostty_mouse_event_set_position(m_mouseEvent, pos);
        encodeAndSendMouseEvent(m_mouseEncoder, m_mouseEvent, m_ptySession);
        event->accept();
        return;
    }

    if (mouseTracking)
        return;

    int rows = delta > 0 ? -3 : 3;
    GhosttyTerminalScrollViewport sv = {
        .tag = GHOSTTY_SCROLL_VIEWPORT_DELTA,
        .value = {.delta = static_cast<intptr_t>(rows)},
    };
    ghostty_terminal_scroll_viewport(m_terminal, sv);
    m_renderStateDirty = true;
    updateViewportScrollState();
    update();
}

void TerminalWidget::onPtyDataReceived(const QByteArray &data) {
    if (!m_terminal || data.isEmpty())
        return;

    scanShellIntegrationSequences(data);
    m_pendingPtyData.append(data);
    scheduleTerminalRepaint();
}

void TerminalWidget::scanShellIntegrationSequences(const QByteArray &data) {
    m_oscScanBuffer.append(data);
    if (m_oscScanBuffer.size() > kMaxOscScanBufferBytes)
        m_oscScanBuffer = m_oscScanBuffer.right(kMaxOscScanBufferBytes);

    while (true) {
        const int oscStart = m_oscScanBuffer.indexOf("\033]");
        if (oscStart < 0) {
            m_oscScanBuffer = m_oscScanBuffer.endsWith('\033') ? QByteArray("\033") : QByteArray();
            return;
        }

        if (oscStart > 0)
            m_oscScanBuffer.remove(0, oscStart);

        const int belEnd = m_oscScanBuffer.indexOf('\a', 2);
        const int stEnd = m_oscScanBuffer.indexOf("\033\\", 2);
        if (belEnd < 0 && stEnd < 0)
            return;

        const bool useBel = belEnd >= 0 && (stEnd < 0 || belEnd < stEnd);
        const int payloadEnd = useBel ? belEnd : stEnd;
        const int sequenceEnd = useBel ? belEnd + 1 : stEnd + 2;
        const QByteArray payload = m_oscScanBuffer.mid(2, payloadEnd - 2);

        const std::optional<int> result = commandResultFromQtGhosttyShellCommandResult(payload);
        if (result.has_value()) {
            setShellCommandResult(result.value());
        } else {
            const std::optional<QString> command = shellCommandFromOscPayload(payload);
            if (command.has_value())
                setShellCommand(command.value());
        }

        m_oscScanBuffer.remove(0, sequenceEnd);
    }
}

void TerminalWidget::setShellCommand(const QString &command) {
    const QString trimmed = command.trimmed();
    if (property("shellCommand").toString() == trimmed)
        return;

    setProperty("shellCommand", trimmed);

    if (!trimmed.isEmpty()) {
        m_pendingExitCode = -1;
        updateCommandState(CommandState::Running);
    } else {
        if (m_pendingExitCode >= 0) {
            updateCommandState(m_pendingExitCode == 0 ? CommandState::Succeeded : CommandState::Failed);
            m_pendingExitCode = -1;
        } else {
            updateCommandState(CommandState::Idle);
        }
    }

    Q_EMIT shellCommandChanged(trimmed);
}

void TerminalWidget::setShellCommandResult(int exitCode) {
    m_pendingExitCode = exitCode;
}

void TerminalWidget::updateCommandState(CommandState newState) {
    if (m_commandState == newState)
        return;

    m_commandState = newState;
    setProperty("commandState", static_cast<int>(newState));
    Q_EMIT commandStateChanged(newState);
}

void TerminalWidget::onPtySessionClosed() {
    flushPendingPtyData();
    m_oscScanBuffer.clear();
    if (m_renderTimer)
        m_renderTimer->stop();
    Q_EMIT sessionClosed();
}

QRect TerminalWidget::inputMethodCursorRect() const {
    QRect cursorRect(terminalContentOrigin(), QSize(qMax(m_cellWidth, 1), qMax(m_cellHeight, 1)));
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
    cursorRect.moveTo(terminalContentOrigin()
                      + QPoint(static_cast<int>(cursorX) * m_cellWidth, static_cast<int>(cursorY) * m_cellHeight));
    return cursorRect;
}

QRect TerminalWidget::terminalContentRect() const {
    return contentsRect();
}

QPoint TerminalWidget::terminalContentOrigin() const {
    return terminalContentRect().topLeft();
}

QPoint TerminalWidget::terminalContentPosition(const QPoint &position) const {
    const QRect contentRect = terminalContentRect();
    if (contentRect.isEmpty())
        return {};

    const int maxX = qMax(0, contentRect.width() - 1);
    const int maxY = qMax(0, contentRect.height() - 1);
    return QPoint(qBound(0, position.x() - contentRect.x(), maxX), qBound(0, position.y() - contentRect.y(), maxY));
}

int TerminalWidget::viewportColumnForPosition(const QPoint &position) const {
    if (m_cellWidth <= 0 || m_cols == 0)
        return 0;
    return qBound(0, terminalContentPosition(position).x() / m_cellWidth, static_cast<int>(m_cols) - 1);
}

int TerminalWidget::viewportRowForPosition(const QPoint &position) const {
    if (m_cellHeight <= 0 || m_rows == 0)
        return 0;
    return qBound(0, terminalContentPosition(position).y() / m_cellHeight, static_cast<int>(m_rows) - 1);
}

GhosttyMousePosition TerminalWidget::ghosttyMousePositionForEvent(const QPointF &position) const {
    const QPoint localPosition = terminalContentPosition(position.toPoint());
    return {static_cast<float>(localPosition.x()), static_cast<float>(localPosition.y())};
}

QRect TerminalWidget::cursorPaintRect() const {
    if (!m_renderState || m_renderStateDirty || !m_hasFocus)
        return {};

    bool cursorVisible = false;
    if (ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_VISIBLE, &cursorVisible)
            != GHOSTTY_SUCCESS
        || !cursorVisible) {
        return {};
    }

    bool cursorInViewport = false;
    if (ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_HAS_VALUE, &cursorInViewport)
            != GHOSTTY_SUCCESS
        || !cursorInViewport) {
        return {};
    }

    bool cursorBlinking = false;
    ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_BLINKING, &cursorBlinking);
    if (m_cursorBlinkEnabled && cursorBlinking && !m_cursorBlinkVisible)
        return {};

    uint16_t cx = 0;
    uint16_t cy = 0;
    ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_X, &cx);
    ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_Y, &cy);

    const QPoint origin = terminalContentOrigin();
    const int x = origin.x() + static_cast<int>(cx) * m_cellWidth;
    const int y = origin.y() + static_cast<int>(cy) * m_cellHeight;

    switch (m_cursorShape) {
        case 1:
            return QRect(x, y, 2, m_cellHeight);
        case 2:
            return QRect(x, y + m_cellHeight - 2, m_cellWidth, 2);
        default:
            return QRect(x, y, m_cellWidth, m_cellHeight);
    }
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
        QRect repaintRegion;
        flushPendingPtyData(&repaintRegion);
        updateAfterPtyFlush(repaintRegion);
        m_lastRenderTime.restart();
        return;
    }

    if (!m_lastRenderTime.isValid()) {
        if (!m_renderTimer->isActive())
            m_renderTimer->start(kInitialPtyCoalesceIntervalMs);
        return;
    }

    if (m_lastRenderTime.elapsed() >= kBurstRenderIntervalMs) {
        QRect repaintRegion;
        flushPendingPtyData(&repaintRegion);
        updateAfterPtyFlush(repaintRegion);
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
        updateViewportScrollState();
    }

    if (m_ptySession)
        m_ptySession->resize(m_cols, m_rows, m_cellWidth, m_cellHeight);

    if (m_mouseEncoder) {
        GhosttyMouseEncoderSize encoderSize = GHOSTTY_INIT_SIZED(GhosttyMouseEncoderSize);
        encoderSize.cell_width = static_cast<uint32_t>(m_cellWidth);
        encoderSize.cell_height = static_cast<uint32_t>(m_cellHeight);
        encoderSize.screen_width = static_cast<uint32_t>(m_cellWidth) * m_cols;
        encoderSize.screen_height = static_cast<uint32_t>(m_cellHeight) * m_rows;
        ghostty_mouse_encoder_setopt(m_mouseEncoder, GHOSTTY_MOUSE_ENCODER_OPT_SIZE, &encoderSize);
    }

#ifdef QTGHOSTTY_TESTING
    ++m_debugResizeApplyCount;
#endif

    update();
}

void TerminalWidget::updateAfterPtyFlush(const QRect &repaintRegion) {
    if (repaintRegion.isValid()) {
        update(repaintRegion.adjusted(-1, -1, 1, 1));
        return;
    }

    update();
}

void TerminalWidget::clearRenderStateDirtyRows() {
    if (ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &m_rowIter) != GHOSTTY_SUCCESS)
        return;

    bool clean = false;
    while (ghostty_render_state_row_iterator_next(m_rowIter))
        ghostty_render_state_row_set(m_rowIter, GHOSTTY_RENDER_STATE_ROW_OPTION_DIRTY, &clean);

    GhosttyRenderStateDirty cleanState = GHOSTTY_RENDER_STATE_DIRTY_FALSE;
    ghostty_render_state_set(m_renderState, GHOSTTY_RENDER_STATE_OPTION_DIRTY, &cleanState);
}

size_t TerminalWidget::scrollbackByteBudget() const {
    return std::max<size_t>(kMinimumScrollbackBytes, static_cast<size_t>(m_scrollbackLines) * kBytesPerScrollbackLine);
}

bool TerminalWidget::flushPendingPtyData(QRect *repaintRegion) {
    if (!m_terminal || m_pendingPtyData.isEmpty()) {
        return false;
    }

    const bool canDetectCursorOnly = repaintRegion && m_renderState && !m_renderStateDirty && !m_backBuffer.isNull()
                                     && m_preeditText.isEmpty() && isCursorOnlyPtyData(m_pendingPtyData);
    const QRect previousCursorRect = canDetectCursorOnly ? cursorPaintRect() : QRect();

#ifdef QTGHOSTTY_TESTING
    ++m_debugPtyFlushCount;
#endif
    m_kittyImageCache.clear();
    ghostty_terminal_vt_write(m_terminal, reinterpret_cast<const uint8_t *>(m_pendingPtyData.constData()),
                              static_cast<size_t>(m_pendingPtyData.size()));
    m_pendingPtyData.clear();
    m_renderStateDirty = true;
    updateViewportScrollState();

    if (canDetectCursorOnly && ghostty_render_state_update(m_renderState, m_terminal) == GHOSTTY_SUCCESS) {
        m_renderStateDirty = false;

        GhosttyRenderStateDirty dirtyState = GHOSTTY_RENDER_STATE_DIRTY_FULL;
        ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_DIRTY, &dirtyState);
        if (dirtyState == GHOSTTY_RENDER_STATE_DIRTY_FALSE || dirtyState == GHOSTTY_RENDER_STATE_DIRTY_PARTIAL) {
            if (dirtyState == GHOSTTY_RENDER_STATE_DIRTY_PARTIAL)
                clearRenderStateDirtyRows();
            const QRect currentCursorRect = cursorPaintRect();
            *repaintRegion = previousCursorRect.united(currentCursorRect);
#ifdef QTGHOSTTY_TESTING
            ++m_debugCursorOnlyRepaintCount;
#endif
            notifyInputMethodCursorChange();
            return true;
        }
    }

    notifyInputMethodCursorChange();
    return false;
}

TerminalWidget::ViewportScrollState TerminalWidget::queryViewportScrollState() const {
    ViewportScrollState state;
    if (!m_terminal)
        return state;

    GhosttyTerminalScrollbar scrollbar = {};
    if (ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_SCROLLBAR, &scrollbar) != GHOSTTY_SUCCESS)
        return state;

    constexpr uint64_t kMaxInt = static_cast<uint64_t>(std::numeric_limits<int>::max());
    state.offset = static_cast<int>(qMin(scrollbar.offset, kMaxInt));
    state.totalRows = static_cast<int>(qMin(scrollbar.total, kMaxInt));
    state.visibleRows = static_cast<int>(qMin(scrollbar.len, kMaxInt));
    return state;
}

void TerminalWidget::updateViewportScrollState() {
    const ViewportScrollState state = queryViewportScrollState();
    if (state == m_viewportScrollState)
        return;

    m_viewportScrollState = state;
    Q_EMIT viewportScrollStateChanged();
}

void TerminalWidget::onRenderTimerTimeout() {
    if (m_pendingPtyData.isEmpty()) {
        return;
    }

    QRect repaintRegion;
    flushPendingPtyData(&repaintRegion);
    updateAfterPtyFlush(repaintRegion);
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
    if (!m_terminal || !m_ptySession)
        return;

    bool mouseTracking = false;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_MOUSE_TRACKING, &mouseTracking);

    if (mouseTracking && m_mouseEncoder && m_mouseEvent) {
        ghostty_mouse_encoder_setopt_from_terminal(m_mouseEncoder, m_terminal);
        ghostty_mouse_event_set_action(m_mouseEvent, GHOSTTY_MOUSE_ACTION_PRESS);
        ghostty_mouse_event_set_button(m_mouseEvent, ghosttyMouseButtonForQt(event->button()));
        ghostty_mouse_event_set_mods(m_mouseEvent, ghosttyMouseMods(event->modifiers()));
        GhosttyMousePosition pos = ghosttyMousePositionForEvent(event->position());
        ghostty_mouse_event_set_position(m_mouseEvent, pos);

        bool anyButton = event->buttons() != Qt::NoButton;
        ghostty_mouse_encoder_setopt(m_mouseEncoder, GHOSTTY_MOUSE_ENCODER_OPT_ANY_BUTTON_PRESSED, &anyButton);

        encodeAndSendMouseEvent(m_mouseEncoder, m_mouseEvent, m_ptySession);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const int col = viewportColumnForPosition(event->pos());
        const int viewportRow = viewportRowForPosition(event->pos());
        const int screenRow = screenRowForViewportRow(viewportRow);

        // Detect multi-click
        constexpr int kMultiClickIntervalMs = 400;
        constexpr int kMultiClickDistancePx = 5;
        if (m_clickTimer.isValid() && m_clickTimer.elapsed() < kMultiClickIntervalMs
            && (event->pos() - m_lastClickPos).manhattanLength() < kMultiClickDistancePx) {
            m_clickCount = qMin(m_clickCount + 1, 3);
        } else {
            m_clickCount = 1;
        }
        m_clickTimer.start();
        m_lastClickPos = event->pos();

        m_clickAnchorRow = screenRow;
        m_clickAnchorCol = col;

        switch (m_clickCount) {
            case 1:
                m_clickMode = ClickMode::Single;
                m_selection.active = false;
                m_selection.startCol = col;
                m_selection.startRow = screenRow;
                m_selection.endCol = col;
                m_selection.endRow = screenRow;
                break;
            case 2:
                m_clickMode = ClickMode::Word;
                selectWordAt(screenRow, col);
                break;
            case 3:
                m_clickMode = ClickMode::Line;
                selectLineAt(screenRow);
                break;
        }
        update();
    }
}

void TerminalWidget::mouseMoveEvent(QMouseEvent *event) {
    if (!m_terminal || !m_ptySession)
        return;

    bool mouseTracking = false;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_MOUSE_TRACKING, &mouseTracking);

    if (mouseTracking && m_mouseEncoder && m_mouseEvent) {
        ghostty_mouse_encoder_setopt_from_terminal(m_mouseEncoder, m_terminal);
        ghostty_mouse_event_set_action(m_mouseEvent, GHOSTTY_MOUSE_ACTION_MOTION);

        GhosttyMouseButton heldButton = ghosttyMouseButtonForHeld(event->buttons());
        if (heldButton == GHOSTTY_MOUSE_BUTTON_UNKNOWN)
            ghostty_mouse_event_clear_button(m_mouseEvent);
        else
            ghostty_mouse_event_set_button(m_mouseEvent, heldButton);

        ghostty_mouse_event_set_mods(m_mouseEvent, ghosttyMouseMods(event->modifiers()));
        GhosttyMousePosition pos = ghosttyMousePositionForEvent(event->position());
        ghostty_mouse_event_set_position(m_mouseEvent, pos);

        bool anyButton = event->buttons() != Qt::NoButton;
        ghostty_mouse_encoder_setopt(m_mouseEncoder, GHOSTTY_MOUSE_ENCODER_OPT_ANY_BUTTON_PRESSED, &anyButton);

        encodeAndSendMouseEvent(m_mouseEncoder, m_mouseEvent, m_ptySession);
        event->accept();
        return;
    }

    if (event->buttons() & Qt::LeftButton) {
        const int col = viewportColumnForPosition(event->pos());
        const int screenRow = screenRowForViewportRow(viewportRowForPosition(event->pos()));

        switch (m_clickMode) {
            case ClickMode::Single:
                m_selection.endCol = col;
                m_selection.endRow = screenRow;
                m_selection.active = true;
                break;
            case ClickMode::Word:
                extendWordSelection(screenRow, col);
                break;
            case ClickMode::Line:
                extendLineSelection(screenRow);
                break;
        }
        update();
    }
}

void TerminalWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (!m_terminal || !m_ptySession)
        return;

    bool mouseTracking = false;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_MOUSE_TRACKING, &mouseTracking);

    if (mouseTracking && m_mouseEncoder && m_mouseEvent) {
        ghostty_mouse_encoder_setopt_from_terminal(m_mouseEncoder, m_terminal);
        ghostty_mouse_event_set_action(m_mouseEvent, GHOSTTY_MOUSE_ACTION_RELEASE);
        ghostty_mouse_event_set_button(m_mouseEvent, ghosttyMouseButtonForQt(event->button()));
        ghostty_mouse_event_set_mods(m_mouseEvent, ghosttyMouseMods(event->modifiers()));
        GhosttyMousePosition pos = ghosttyMousePositionForEvent(event->position());
        ghostty_mouse_event_set_position(m_mouseEvent, pos);

        bool anyButton = event->buttons() != Qt::NoButton;
        ghostty_mouse_encoder_setopt(m_mouseEncoder, GHOSTTY_MOUSE_ENCODER_OPT_ANY_BUTTON_PRESSED, &anyButton);

        encodeAndSendMouseEvent(m_mouseEncoder, m_mouseEvent, m_ptySession);

        if (!anyButton)
            ghostty_mouse_encoder_reset(m_mouseEncoder);

        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const int col = viewportColumnForPosition(event->pos());
        const int screenRow = screenRowForViewportRow(viewportRowForPosition(event->pos()));

        switch (m_clickMode) {
            case ClickMode::Single:
                m_selection.endCol = col;
                m_selection.endRow = screenRow;
                m_selection.active = true;
                break;
            case ClickMode::Word:
                extendWordSelection(screenRow, col);
                break;
            case ClickMode::Line:
                extendLineSelection(screenRow);
                break;
        }
        update();

        QString text = selectedText();
        if (!text.isEmpty())
            QGuiApplication::clipboard()->setText(text, QClipboard::Selection);
    }

    if (event->button() == Qt::MiddleButton) {
        if (!m_ptySession)
            return;
        QString text = QGuiApplication::clipboard()->text(QClipboard::Selection);
        if (!text.isEmpty()) {
            QByteArray data = text.toUtf8();
            bool bracketed = false;
            ghostty_terminal_mode_get(m_terminal, GHOSTTY_MODE_BRACKETED_PASTE, &bracketed);
            if (bracketed) {
                size_t required = 0;
                QByteArray mutableData(data);
                GhosttyResult err = ghostty_paste_encode(mutableData.data(), static_cast<size_t>(mutableData.size()),
                                                         true, nullptr, 0, &required);
                if (err == GHOSTTY_OUT_OF_SPACE && required > 0) {
                    QByteArray buf(static_cast<int>(required), '\0');
                    size_t written = 0;
                    if (ghostty_paste_encode(mutableData.data(), static_cast<size_t>(mutableData.size()), true,
                                             buf.data(), static_cast<size_t>(buf.size()), &written)
                        == GHOSTTY_SUCCESS) {
                        m_ptySession->write(QByteArray(buf.constData(), static_cast<int>(written)));
                        event->accept();
                        return;
                    }
                }
            }
            m_ptySession->write(data);
        }
        event->accept();
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

    int topCol = (m_selection.startRow <= m_selection.endRow) ? m_selection.startCol : m_selection.endCol;
    int bottomCol = (m_selection.startRow <= m_selection.endRow) ? m_selection.endCol : m_selection.startCol;
    int left = qMin(topCol, bottomCol);
    int right = qMax(topCol, bottomCol);

    if (top == bottom) {
        return col >= left && col <= right;
    }

    if (screenRow == top)
        return col >= topCol;
    if (screenRow == bottom)
        return col <= bottomCol;
    return true;
}

QString TerminalWidget::selectedText() const {
    if (!m_selection.active || !m_terminal)
        return QString();

    int top = qMin(m_selection.startRow, m_selection.endRow);
    int bottom = qMax(m_selection.startRow, m_selection.endRow);
    int topCol = (m_selection.startRow <= m_selection.endRow) ? m_selection.startCol : m_selection.endCol;
    int bottomCol = (m_selection.startRow <= m_selection.endRow) ? m_selection.endCol : m_selection.startCol;

    auto isRowWrapped = [&](int row) -> bool {
        GhosttyPoint wp = {.tag = GHOSTTY_POINT_TAG_SCREEN,
                           .value = {.coordinate = {.x = 0, .y = static_cast<uint32_t>(row)}}};
        GhosttyGridRef wref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        if (ghostty_terminal_grid_ref(m_terminal, wp, &wref) != GHOSTTY_SUCCESS)
            return false;
        GhosttyRow row_ = 0;
        if (ghostty_grid_ref_row(&wref, &row_) != GHOSTTY_SUCCESS || row_ == 0)
            return false;
        bool wrapped = false;
        ghostty_row_get(row_, GHOSTTY_ROW_DATA_WRAP, &wrapped);
        return wrapped;
    };

    QStringList lines;
    for (int row = top; row <= bottom; ++row) {
        QString line;
        int left = 0;
        int right = m_cols - 1;

        if (row == top && row == bottom) {
            left = qMin(topCol, bottomCol);
            right = qMax(topCol, bottomCol);
        } else if (row == top) {
            left = topCol;
        } else if (row == bottom) {
            right = bottomCol;
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
        if (row > top && isRowWrapped(row - 1)) {
            if (!lines.isEmpty())
                lines.last().append(line);
            else
                lines.append(line);
        } else {
            lines.append(line);
        }
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
    if (!m_ptySession || !m_terminal)
        return;
    QString text = QGuiApplication::clipboard()->text();
    if (text.isEmpty())
        return;
    QByteArray data = text.toUtf8();

    bool bracketed = false;
    ghostty_terminal_mode_get(m_terminal, GHOSTTY_MODE_BRACKETED_PASTE, &bracketed);

    if (bracketed) {
        size_t required = 0;
        QByteArray mutableData(data);
        GhosttyResult err = ghostty_paste_encode(mutableData.data(), static_cast<size_t>(mutableData.size()), true,
                                                 nullptr, 0, &required);
        if (err == GHOSTTY_OUT_OF_SPACE && required > 0) {
            QByteArray buf(static_cast<int>(required), '\0');
            size_t written = 0;
            if (ghostty_paste_encode(mutableData.data(), static_cast<size_t>(mutableData.size()), true, buf.data(),
                                     static_cast<size_t>(buf.size()), &written)
                == GHOSTTY_SUCCESS) {
                m_ptySession->write(QByteArray(buf.constData(), static_cast<int>(written)));
                return;
            }
        }
    }

    m_ptySession->write(data);
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

bool TerminalWidget::isWordChar(uint32_t codepoint) const {
    // Common terminal word characters: letters, digits, underscore, hyphen
    if (codepoint >= 'a' && codepoint <= 'z')
        return true;
    if (codepoint >= 'A' && codepoint <= 'Z')
        return true;
    if (codepoint >= '0' && codepoint <= '9')
        return true;
    if (codepoint == '_')
        return true;
    if (codepoint == '-')
        return true;
    return false;
}

void TerminalWidget::wordBoundsAt(int screenRow, int col, int *startCol, int *endCol) const {
    *startCol = col;
    *endCol = col;
    if (!m_terminal || col < 0 || col >= static_cast<int>(m_cols))
        return;

    auto getCodepointAt = [&](int c) -> uint32_t {
        GhosttyPoint point = {
            .tag = GHOSTTY_POINT_TAG_SCREEN,
            .value = {.coordinate = {.x = static_cast<uint16_t>(c), .y = static_cast<uint32_t>(screenRow)}}};
        GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        if (ghostty_terminal_grid_ref(m_terminal, point, &ref) != GHOSTTY_SUCCESS)
            return 0;
        uint32_t graphemes[16];
        size_t len = 0;
        if (ghostty_grid_ref_graphemes(&ref, graphemes, 16, &len) == GHOSTTY_SUCCESS && len > 0)
            return graphemes[0];
        return 0;
    };

    auto cellWidthAt = [&](int c) -> GhosttyCellWide {
        GhosttyPoint point = {
            .tag = GHOSTTY_POINT_TAG_SCREEN,
            .value = {.coordinate = {.x = static_cast<uint16_t>(c), .y = static_cast<uint32_t>(screenRow)}}};
        GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        if (ghostty_terminal_grid_ref(m_terminal, point, &ref) != GHOSTTY_SUCCESS)
            return GHOSTTY_CELL_WIDE_NARROW;
        GhosttyCell cell = 0;
        if (ghostty_grid_ref_cell(&ref, &cell) != GHOSTTY_SUCCESS)
            return GHOSTTY_CELL_WIDE_NARROW;
        GhosttyCellWide wide = GHOSTTY_CELL_WIDE_NARROW;
        ghostty_cell_get(cell, GHOSTTY_CELL_DATA_WIDE, &wide);
        return wide;
    };

    int adjustedCol = col;
    GhosttyCellWide clickedWide = cellWidthAt(col);
    if (clickedWide == GHOSTTY_CELL_WIDE_SPACER_TAIL || clickedWide == GHOSTTY_CELL_WIDE_SPACER_HEAD) {
        adjustedCol = qMax(0, col - 1);
    }

    uint32_t anchorCp = getCodepointAt(adjustedCol);
    const bool expectBoundary = !isWordChar(anchorCp);

    for (int c = adjustedCol; c >= 0; --c) {
        GhosttyCellWide cw = cellWidthAt(c);
        if (cw == GHOSTTY_CELL_WIDE_SPACER_TAIL || cw == GHOSTTY_CELL_WIDE_SPACER_HEAD)
            continue;
        uint32_t cp = getCodepointAt(c);
        if ((expectBoundary && isWordChar(cp)) || (!expectBoundary && !isWordChar(cp))) {
            *startCol = c + 1;
            break;
        }
        *startCol = c;
    }

    for (int c = adjustedCol; c < static_cast<int>(m_cols); ++c) {
        GhosttyCellWide cw = cellWidthAt(c);
        if (cw == GHOSTTY_CELL_WIDE_SPACER_TAIL || cw == GHOSTTY_CELL_WIDE_SPACER_HEAD)
            continue;
        uint32_t cp = getCodepointAt(c);
        if ((expectBoundary && isWordChar(cp)) || (!expectBoundary && !isWordChar(cp))) {
            *endCol = c - 1;
            break;
        }
        int advance = (cw == GHOSTTY_CELL_WIDE_WIDE) ? 2 : 1;
        *endCol = c + advance - 1;
        if (cw == GHOSTTY_CELL_WIDE_WIDE)
            ++c;
    }
}

void TerminalWidget::selectWordAt(int screenRow, int col) {
    int startCol = col;
    int endCol = col;
    wordBoundsAt(screenRow, col, &startCol, &endCol);
    m_selection.active = true;
    m_selection.startRow = screenRow;
    m_selection.startCol = startCol;
    m_selection.endRow = screenRow;
    m_selection.endCol = endCol;
}

void TerminalWidget::selectLineAt(int screenRow) {
    if (!m_terminal) {
        m_selection.active = true;
        m_selection.startRow = screenRow;
        m_selection.startCol = 0;
        m_selection.endRow = screenRow;
        m_selection.endCol = m_cols - 1;
        return;
    }

    size_t totalRows = 0;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_TOTAL_ROWS, &totalRows);

    auto isRowWrapped = [&](int row) -> bool {
        if (row < 0 || row >= static_cast<int>(totalRows))
            return false;
        GhosttyPoint point = {.tag = GHOSTTY_POINT_TAG_SCREEN,
                              .value = {.coordinate = {.x = 0, .y = static_cast<uint32_t>(row)}}};
        GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        if (ghostty_terminal_grid_ref(m_terminal, point, &ref) != GHOSTTY_SUCCESS)
            return false;
        GhosttyRow row_ = 0;
        if (ghostty_grid_ref_row(&ref, &row_) != GHOSTTY_SUCCESS || row_ == 0)
            return false;
        bool wrapped = false;
        ghostty_row_get(row_, GHOSTTY_ROW_DATA_WRAP, &wrapped);
        return wrapped;
    };

    auto isRowContinuation = [&](int row) -> bool {
        if (row < 0 || row >= static_cast<int>(totalRows))
            return false;
        GhosttyPoint point = {.tag = GHOSTTY_POINT_TAG_SCREEN,
                              .value = {.coordinate = {.x = 0, .y = static_cast<uint32_t>(row)}}};
        GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        if (ghostty_terminal_grid_ref(m_terminal, point, &ref) != GHOSTTY_SUCCESS)
            return false;
        GhosttyRow row_ = 0;
        if (ghostty_grid_ref_row(&ref, &row_) != GHOSTTY_SUCCESS || row_ == 0)
            return false;
        bool cont = false;
        ghostty_row_get(row_, GHOSTTY_ROW_DATA_WRAP_CONTINUATION, &cont);
        return cont;
    };

    int startRow = screenRow;
    while (startRow > 0 && isRowContinuation(startRow))
        --startRow;

    int endRow = screenRow;
    while (isRowWrapped(endRow) && endRow < static_cast<int>(totalRows) - 1)
        ++endRow;

    m_selection.active = true;
    m_selection.startRow = startRow;
    m_selection.startCol = 0;
    m_selection.endRow = endRow;
    m_selection.endCol = m_cols - 1;
}

void TerminalWidget::extendWordSelection(int screenRow, int col) {
    int anchorWordStart = 0, anchorWordEnd = 0;
    wordBoundsAt(m_clickAnchorRow, m_clickAnchorCol, &anchorWordStart, &anchorWordEnd);
    int currentWordStart = 0, currentWordEnd = 0;
    wordBoundsAt(screenRow, col, &currentWordStart, &currentWordEnd);
    if (screenRow < m_clickAnchorRow || (screenRow == m_clickAnchorRow && col < m_clickAnchorCol)) {
        m_selection.startRow = screenRow;
        m_selection.startCol = currentWordStart;
        m_selection.endRow = m_clickAnchorRow;
        m_selection.endCol = anchorWordEnd;
    } else {
        m_selection.startRow = m_clickAnchorRow;
        m_selection.startCol = anchorWordStart;
        m_selection.endRow = screenRow;
        m_selection.endCol = currentWordEnd;
    }
    m_selection.active = true;
}

void TerminalWidget::extendLineSelection(int screenRow) {
    if (!m_terminal) {
        if (screenRow < m_clickAnchorRow) {
            m_selection.startRow = screenRow;
            m_selection.startCol = 0;
            m_selection.endRow = m_clickAnchorRow;
            m_selection.endCol = m_cols - 1;
        } else {
            m_selection.startRow = m_clickAnchorRow;
            m_selection.startCol = 0;
            m_selection.endRow = screenRow;
            m_selection.endCol = m_cols - 1;
        }
        m_selection.active = true;
        return;
    }

    size_t totalRows = 0;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_TOTAL_ROWS, &totalRows);

    auto isRowWrapped = [&](int row) -> bool {
        if (row < 0 || row >= static_cast<int>(totalRows))
            return false;
        GhosttyPoint point = {.tag = GHOSTTY_POINT_TAG_SCREEN,
                              .value = {.coordinate = {.x = 0, .y = static_cast<uint32_t>(row)}}};
        GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        if (ghostty_terminal_grid_ref(m_terminal, point, &ref) != GHOSTTY_SUCCESS)
            return false;
        GhosttyRow row_ = 0;
        if (ghostty_grid_ref_row(&ref, &row_) != GHOSTTY_SUCCESS || row_ == 0)
            return false;
        bool wrapped = false;
        ghostty_row_get(row_, GHOSTTY_ROW_DATA_WRAP, &wrapped);
        return wrapped;
    };

    auto isRowContinuation = [&](int row) -> bool {
        if (row < 0 || row >= static_cast<int>(totalRows))
            return false;
        GhosttyPoint point = {.tag = GHOSTTY_POINT_TAG_SCREEN,
                              .value = {.coordinate = {.x = 0, .y = static_cast<uint32_t>(row)}}};
        GhosttyGridRef ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
        if (ghostty_terminal_grid_ref(m_terminal, point, &ref) != GHOSTTY_SUCCESS)
            return false;
        GhosttyRow row_ = 0;
        if (ghostty_grid_ref_row(&ref, &row_) != GHOSTTY_SUCCESS || row_ == 0)
            return false;
        bool cont = false;
        ghostty_row_get(row_, GHOSTTY_ROW_DATA_WRAP_CONTINUATION, &cont);
        return cont;
    };

    auto logicalLineStart = [&](int row) -> int {
        int r = row;
        while (r > 0 && isRowContinuation(r))
            --r;
        return r;
    };

    auto logicalLineEnd = [&](int row) -> int {
        int r = row;
        while (isRowWrapped(r) && r < static_cast<int>(totalRows) - 1)
            ++r;
        return r;
    };

    int anchorStart = logicalLineStart(m_clickAnchorRow);
    int anchorEnd = logicalLineEnd(m_clickAnchorRow);
    int dragStart = logicalLineStart(screenRow);
    int dragEnd = logicalLineEnd(screenRow);

    if (dragStart < anchorStart) {
        m_selection.startRow = dragStart;
        m_selection.startCol = 0;
        m_selection.endRow = anchorEnd;
        m_selection.endCol = m_cols - 1;
    } else {
        m_selection.startRow = anchorStart;
        m_selection.startCol = 0;
        m_selection.endRow = dragEnd;
        m_selection.endCol = m_cols - 1;
    }
    m_selection.active = true;
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

#ifdef QTGHOSTTY_TESTING
bool TerminalWidget::debugAppliedIsDark() const {
    return m_isDark;
}

QColor TerminalWidget::debugAppliedForeground() const {
    if (!m_terminal)
        return QColor();
    GhosttyColorRgb rgb;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_COLOR_FOREGROUND, &rgb);
    return QColor(rgb.r, rgb.g, rgb.b);
}

QColor TerminalWidget::debugAppliedBackground() const {
    if (!m_terminal)
        return QColor();
    GhosttyColorRgb rgb;
    ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_COLOR_BACKGROUND, &rgb);
    return QColor(rgb.r, rgb.g, rgb.b);
}

void TerminalWidget::debugSetRawTerminalFont(const QFont &font) {
    m_font = font;

    QFontMetrics fm(m_font);
    m_cellWidth = fm.horizontalAdvance('M');
    m_cellHeight = fm.height();
    m_fontAscent = fm.ascent();
    updateCachedFonts();

    updateGridSize();
    update();
}
#endif
