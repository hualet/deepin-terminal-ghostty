# Qt libghostty-vt Minimal Terminal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a minimal Qt Widgets terminal emulator that embeds `libghostty-vt`, launches a shell in a PTY, renders terminal output, accepts basic keyboard input, and handles resize.

**Architecture:** Use a single-process Qt Widgets app with a `PtySession` object for PTY lifecycle and a `TerminalWidget` object for Ghostty terminal state, render-state extraction, and painting via `QPainter`. Keep the first implementation single-threaded and use `QSocketNotifier` to integrate the PTY fd into the Qt event loop.

**Tech Stack:** CMake, C++20, Qt6 Widgets, Linux PTY APIs (`forkpty`, `ioctl`, `fcntl`), local `libghostty-vt.so`, Ghostty C API headers from `/home/hualet/projects/g/ghostty/include`

---

### Task 1: Bootstrap the Qt project and build wiring

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/main.cpp`

- [ ] **Step 1: Write the failing build configuration**

```cmake
cmake_minimum_required(VERSION 3.21)
project(deepin_terminal_ghostty LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(deepin-terminal-ghostty
  src/main.cpp
)
target_link_libraries(deepin-terminal-ghostty PRIVATE Qt6::Widgets ghostty-vt)
```

```cpp
#include <QApplication>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    return app.exec();
}
```

- [ ] **Step 2: Run configure to verify it fails for the expected missing library wiring**

Run: `cmake -S . -B build -G Ninja`

Expected: configure or link setup is incomplete because the project does not yet tell CMake where `ghostty-vt` and Ghostty headers live.

- [ ] **Step 3: Write the minimal passing build wiring**

```cmake
cmake_minimum_required(VERSION 3.21)
project(deepin_terminal_ghostty LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(GHOSTTY_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../../g/ghostty/include" CACHE PATH
  "Path to the Ghostty headers")

add_library(ghostty-vt SHARED IMPORTED GLOBAL)
set_target_properties(ghostty-vt PROPERTIES
  IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/libghostty-vt.so"
)

add_executable(deepin-terminal-ghostty
  src/main.cpp
)

target_include_directories(deepin-terminal-ghostty PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${GHOSTTY_INCLUDE_DIR}"
)

target_link_libraries(deepin-terminal-ghostty PRIVATE
  Qt6::Widgets
  ghostty-vt
  util
)

set(CMAKE_BUILD_RPATH "$ORIGIN")
set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
set(CMAKE_INSTALL_RPATH "$ORIGIN")

add_custom_command(TARGET deepin-terminal-ghostty POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_FILE:ghostty-vt>"
          "$<TARGET_FILE_DIR:deepin-terminal-ghostty>/libghostty-vt.so.0"
  VERBATIM
)
```

```cpp
#include <QApplication>
#include <QMainWindow>
#include <QWidget>

#include <ghostty/vt/build_info.h>

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    GhosttyString version = {};
    ghostty_build_info(GHOSTTY_BUILD_INFO_VERSION_STRING, &version);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("deepin-terminal-ghostty"));
    window.setCentralWidget(new QWidget(&window));
    window.resize(960, 640);
    window.show();

    return app.exec();
}
```

- [ ] **Step 4: Run configure and build to verify the bootstrap passes**

Run: `cmake -S . -B build -G Ninja && cmake --build build`

Expected: configure succeeds and the executable `build/deepin-terminal-ghostty` is produced.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/main.cpp
git commit -m "build: bootstrap Qt terminal app"
```

### Task 2: Add PTY session ownership

**Files:**
- Create: `src/PtySession.h`
- Create: `src/PtySession.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing interface compile target**

```cpp
#pragma once

#include <QObject>
#include <QByteArray>

class PtySession final : public QObject {
    Q_OBJECT
public:
    explicit PtySession(QObject *parent = nullptr);
    ~PtySession() override;

    bool start(int cols, int rows);
    void write(const QByteArray &data);
    void resize(int cols, int rows, int cellWidthPx, int cellHeightPx);

signals:
    void dataReceived(const QByteArray &data);
    void sessionClosed();
};
```

```cmake
add_executable(deepin-terminal-ghostty
  src/main.cpp
  src/PtySession.cpp
)
```

- [ ] **Step 2: Run build to verify it fails because the implementation does not exist yet**

Run: `cmake --build build`

Expected: compiler error for missing `src/PtySession.cpp` or unresolved `PtySession` symbols.

- [ ] **Step 3: Write the minimal PTY implementation**

```cpp
#pragma once

#include <QObject>
#include <QByteArray>

class QSocketNotifier;

class PtySession final : public QObject {
    Q_OBJECT
public:
    explicit PtySession(QObject *parent = nullptr);
    ~PtySession() override;

    bool start(int cols, int rows);
    void write(const QByteArray &data);
    void resize(int cols, int rows, int cellWidthPx, int cellHeightPx);

private slots:
    void handleReadable();

private:
    int m_fd = -1;
    pid_t m_childPid = -1;
    QSocketNotifier *m_notifier = nullptr;
};
```

```cpp
#include "PtySession.h"

#include <QSocketNotifier>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <pty.h>

PtySession::PtySession(QObject *parent) : QObject(parent) {}

PtySession::~PtySession() {
    if (m_notifier) m_notifier->deleteLater();
    if (m_fd >= 0) ::close(m_fd);
    if (m_childPid > 0) {
        ::kill(m_childPid, SIGHUP);
        ::waitpid(m_childPid, nullptr, 0);
    }
}

bool PtySession::start(int cols, int rows) {
    struct winsize ws {
        .ws_row = static_cast<unsigned short>(rows),
        .ws_col = static_cast<unsigned short>(cols),
        .ws_xpixel = 0,
        .ws_ypixel = 0,
    };

    const pid_t pid = ::forkpty(&m_fd, nullptr, nullptr, &ws);
    if (pid < 0) return false;

    if (pid == 0) {
        const char *shell = ::getenv("SHELL");
        if (!shell || !*shell) {
            if (passwd *pw = ::getpwuid(::getuid()); pw && pw->pw_shell && *pw->pw_shell) shell = pw->pw_shell;
            else shell = "/bin/sh";
        }
        ::setenv("TERM", "xterm-256color", 1);
        const char *shellName = std::strrchr(shell, '/');
        ::execl(shell, shellName ? shellName + 1 : shell, nullptr);
        ::_exit(127);
    }

    m_childPid = pid;
    const int flags = ::fcntl(m_fd, F_GETFL, 0);
    ::fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);

    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &PtySession::handleReadable);
    return true;
}

void PtySession::write(const QByteArray &data) {
    if (m_fd < 0 || data.isEmpty()) return;
    const char *ptr = data.constData();
    qsizetype remaining = data.size();
    while (remaining > 0) {
        const ssize_t written = ::write(m_fd, ptr, static_cast<size_t>(remaining));
        if (written > 0) {
            ptr += written;
            remaining -= written;
            continue;
        }
        if (written < 0 && errno == EINTR) continue;
        break;
    }
}

void PtySession::resize(int cols, int rows, int cellWidthPx, int cellHeightPx) {
    if (m_fd < 0) return;
    struct winsize ws {
        .ws_row = static_cast<unsigned short>(rows),
        .ws_col = static_cast<unsigned short>(cols),
        .ws_xpixel = static_cast<unsigned short>(cols * cellWidthPx),
        .ws_ypixel = static_cast<unsigned short>(rows * cellHeightPx),
    };
    ::ioctl(m_fd, TIOCSWINSZ, &ws);
}

void PtySession::handleReadable() {
    char buffer[4096];
    for (;;) {
        const ssize_t n = ::read(m_fd, buffer, sizeof(buffer));
        if (n > 0) {
            emit dataReceived(QByteArray(buffer, static_cast<qsizetype>(n)));
            continue;
        }
        if (n == 0 || (n < 0 && errno == EIO)) {
            emit sessionClosed();
            return;
        }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
        emit sessionClosed();
        return;
    }
}
```

- [ ] **Step 4: Run build to verify the PTY layer compiles**

Run: `cmake --build build`

Expected: the project builds with the new PTY session class.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/PtySession.h src/PtySession.cpp
git commit -m "feat: add PTY session wrapper"
```

### Task 3: Add the terminal widget and Ghostty integration

**Files:**
- Create: `src/TerminalWidget.h`
- Create: `src/TerminalWidget.cpp`
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing widget interface and wire it into the app**

```cpp
#pragma once

#include <QWidget>

class PtySession;

class TerminalWidget final : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget() override;
};
```

```cpp
#include <QApplication>
#include <QMainWindow>

#include "TerminalWidget.h"

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("deepin-terminal-ghostty"));
    window.setCentralWidget(new TerminalWidget(&window));
    window.resize(960, 640);
    window.show();

    return app.exec();
}
```

```cmake
add_executable(deepin-terminal-ghostty
  src/main.cpp
  src/PtySession.cpp
  src/TerminalWidget.cpp
)
```

- [ ] **Step 2: Run build to verify it fails because the widget implementation is not present**

Run: `cmake --build build`

Expected: compiler or linker error for missing `TerminalWidget` implementation.

- [ ] **Step 3: Write the minimal Ghostty-backed widget**

```cpp
#pragma once

#include <QFont>
#include <QWidget>

#include <ghostty/vt.h>

class PtySession;

class TerminalWidget final : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget() override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void handlePtyData(const QByteArray &data);
    void handleSessionClosed();

private:
    static void writePtyCallback(GhosttyTerminal terminal, const uint8_t *data, size_t len, void *userdata);
    void initializeTerminal();
    void syncSize();
    QByteArray encodeKey(QKeyEvent *event);

    PtySession *m_session = nullptr;
    GhosttyTerminal m_terminal = nullptr;
    GhosttyRenderState m_renderState = nullptr;
    GhosttyRenderStateRowIterator m_rowIterator = nullptr;
    GhosttyRenderStateRowCells m_rowCells = nullptr;
    GhosttyKeyEncoder m_keyEncoder = nullptr;
    QFont m_font;
    int m_cellWidth = 0;
    int m_cellHeight = 0;
    bool m_closed = false;
};
```

```cpp
#include "TerminalWidget.h"
#include "PtySession.h"

#include <QKeyEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QFontDatabase>
#include <QFontMetrics>

namespace {
constexpr uint16_t kMinCols = 1;
constexpr uint16_t kMinRows = 1;
constexpr size_t kScrollback = 4000;
}

TerminalWidget::TerminalWidget(QWidget *parent) : QWidget(parent), m_session(new PtySession(this)) {
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled, false);

    m_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_font.setPointSize(12);
    QFontMetrics metrics(m_font);
    m_cellWidth = metrics.horizontalAdvance(QLatin1Char('M'));
    m_cellHeight = metrics.height();

    initializeTerminal();

    connect(m_session, &PtySession::dataReceived, this, &TerminalWidget::handlePtyData);
    connect(m_session, &PtySession::sessionClosed, this, &TerminalWidget::handleSessionClosed);

    m_session->start(80, 24);
}

TerminalWidget::~TerminalWidget() {
    if (m_keyEncoder) ghostty_key_encoder_free(m_keyEncoder);
    if (m_rowCells) ghostty_render_state_row_cells_free(m_rowCells);
    if (m_rowIterator) ghostty_render_state_row_iterator_free(m_rowIterator);
    if (m_renderState) ghostty_render_state_free(m_renderState);
    if (m_terminal) ghostty_terminal_free(m_terminal);
}

void TerminalWidget::initializeTerminal() {
    GhosttyTerminalOptions options{ .cols = 80, .rows = 24, .max_scrollback = kScrollback };
    ghostty_terminal_new(nullptr, &m_terminal, options);
    ghostty_render_state_new(nullptr, &m_renderState);
    ghostty_render_state_row_iterator_new(nullptr, &m_rowIterator);
    ghostty_render_state_row_cells_new(nullptr, &m_rowCells);
    ghostty_key_encoder_new(nullptr, &m_keyEncoder);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_USERDATA, this);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_WRITE_PTY, reinterpret_cast<const void *>(&TerminalWidget::writePtyCallback));
}

void TerminalWidget::writePtyCallback(GhosttyTerminal, const uint8_t *data, size_t len, void *userdata) {
    auto *self = static_cast<TerminalWidget *>(userdata);
    self->m_session->write(QByteArray(reinterpret_cast<const char *>(data), static_cast<qsizetype>(len)));
}
```

```cpp
void TerminalWidget::handlePtyData(const QByteArray &data) {
    ghostty_terminal_vt_write(m_terminal, reinterpret_cast<const uint8_t *>(data.constData()), static_cast<size_t>(data.size()));
    update();
}

void TerminalWidget::handleSessionClosed() {
    m_closed = true;
    update();
}

void TerminalWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    syncSize();
}

void TerminalWidget::syncSize() {
    const uint16_t cols = static_cast<uint16_t>(std::max<int>(kMinCols, width() / std::max(1, m_cellWidth)));
    const uint16_t rows = static_cast<uint16_t>(std::max<int>(kMinRows, height() / std::max(1, m_cellHeight)));
    ghostty_terminal_resize(m_terminal, cols, rows, static_cast<uint32_t>(m_cellWidth), static_cast<uint32_t>(m_cellHeight));
    ghostty_key_encoder_setopt_from_terminal(m_keyEncoder, m_terminal);
    m_session->resize(cols, rows, m_cellWidth, m_cellHeight);
}
```

```cpp
void TerminalWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setFont(m_font);

    ghostty_render_state_update(m_renderState, m_terminal);
    GhosttyRenderStateColors colors = GHOSTTY_INIT_SIZED(GhosttyRenderStateColors);
    ghostty_render_state_colors_get(m_renderState, &colors);

    painter.fillRect(rect(), QColor(colors.background.r, colors.background.g, colors.background.b));

    if (ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &m_rowIterator) != GHOSTTY_SUCCESS) return;

    int y = m_cellHeight;
    while (ghostty_render_state_row_iterator_next(m_rowIterator)) {
        if (ghostty_render_state_row_get(m_rowIterator, GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &m_rowCells) != GHOSTTY_SUCCESS) {
            y += m_cellHeight;
            continue;
        }
        int x = 0;
        while (ghostty_render_state_row_cells_next(m_rowCells)) {
            uint32_t len = 0;
            ghostty_render_state_row_cells_get(m_rowCells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_LEN, &len);
            GhosttyColorRgb fg = colors.foreground;
            ghostty_render_state_row_cells_get(m_rowCells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_FG_COLOR, &fg);
            GhosttyColorRgb bg{};
            const bool hasBg = ghostty_render_state_row_cells_get(m_rowCells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_BG_COLOR, &bg) == GHOSTTY_SUCCESS;
            if (hasBg) painter.fillRect(x, y - m_cellHeight, m_cellWidth, m_cellHeight, QColor(bg.r, bg.g, bg.b));
            if (len > 0) {
                uint32_t codepoints[16] = {};
                ghostty_render_state_row_cells_get(m_rowCells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_BUF, codepoints);
                QString text;
                for (uint32_t i = 0; i < len && i < 16; ++i) text.append(QChar::fromUcs4(codepoints[i]));
                painter.setPen(QColor(fg.r, fg.g, fg.b));
                painter.drawText(x, y - QFontMetrics(m_font).descent(), text);
            }
            x += m_cellWidth;
        }
        y += m_cellHeight;
    }
}
```

```cpp
QByteArray TerminalWidget::encodeKey(QKeyEvent *event) {
    if (!event->text().isEmpty() && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        return event->text().toUtf8();
    }
    return {};
}

void TerminalWidget::keyPressEvent(QKeyEvent *event) {
    const QByteArray text = encodeKey(event);
    if (!text.isEmpty()) {
        m_session->write(text);
        return;
    }
    QWidget::keyPressEvent(event);
}
```

- [ ] **Step 4: Extend the key path to cover basic special keys**

```cpp
QByteArray TerminalWidget::encodeKey(QKeyEvent *event) {
    if (!event->text().isEmpty() && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        return event->text().toUtf8();
    }

    GhosttyKey key = GHOSTTY_KEY_UNIDENTIFIED;
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter: key = GHOSTTY_KEY_ENTER; break;
    case Qt::Key_Backspace: key = GHOSTTY_KEY_BACKSPACE; break;
    case Qt::Key_Tab: key = GHOSTTY_KEY_TAB; break;
    case Qt::Key_Left: key = GHOSTTY_KEY_ARROW_LEFT; break;
    case Qt::Key_Right: key = GHOSTTY_KEY_ARROW_RIGHT; break;
    case Qt::Key_Up: key = GHOSTTY_KEY_ARROW_UP; break;
    case Qt::Key_Down: key = GHOSTTY_KEY_ARROW_DOWN; break;
    default: return {};
    }

    GhosttyMods mods = 0;
    if (event->modifiers() & Qt::ShiftModifier) mods |= GHOSTTY_MODS_SHIFT;
    if (event->modifiers() & Qt::ControlModifier) mods |= GHOSTTY_MODS_CTRL;
    if (event->modifiers() & Qt::AltModifier) mods |= GHOSTTY_MODS_ALT;
    if (event->modifiers() & Qt::MetaModifier) mods |= GHOSTTY_MODS_SUPER;

    GhosttyKeyEvent keyEvent = nullptr;
    ghostty_key_event_new(nullptr, &keyEvent);
    ghostty_key_event_set_key(keyEvent, key);
    ghostty_key_event_set_mods(keyEvent, mods);
    ghostty_key_event_set_action(keyEvent, GHOSTTY_KEY_ACTION_PRESS);

    uint8_t buffer[64] = {};
    size_t written = 0;
    ghostty_key_encoder_setopt_from_terminal(m_keyEncoder, m_terminal);
    const GhosttyResult result = ghostty_key_encoder_encode(m_keyEncoder, keyEvent, buffer, sizeof(buffer), &written);
    ghostty_key_event_free(keyEvent);
    if (result != GHOSTTY_SUCCESS || written == 0) return {};

    return QByteArray(reinterpret_cast<const char *>(buffer), static_cast<qsizetype>(written));
}
```

- [ ] **Step 5: Run build to verify the integrated widget compiles**

Run: `cmake --build build`

Expected: build succeeds with the terminal widget, PTY session, and Ghostty integration linked together.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt src/main.cpp src/TerminalWidget.h src/TerminalWidget.cpp
git commit -m "feat: add minimal Ghostty terminal widget"
```

### Task 4: Add a startup smoke test and final verification

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing smoke-test registration**

```cmake
enable_testing()
add_test(NAME app_starts COMMAND deepin-terminal-ghostty)
```

- [ ] **Step 2: Run the test to verify it fails because the app does not exit on its own**

Run: `ctest --test-dir build --output-on-failure`

Expected: the test hangs or fails because the GUI app needs a controlled auto-exit path for CI-style smoke checks.

- [ ] **Step 3: Add a minimal auto-exit path for test mode**

```cpp
#include <QTimer>

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("deepin-terminal-ghostty"));
    window.setCentralWidget(new TerminalWidget(&window));
    window.resize(960, 640);
    window.show();

    if (qEnvironmentVariableIntValue("GHOSTTY_QT_SMOKE") == 1) {
        QTimer::singleShot(250, &app, &QCoreApplication::quit);
    }

    return app.exec();
}
```

```cmake
enable_testing()
add_test(NAME app_starts COMMAND ${CMAKE_COMMAND} -E env GHOSTTY_QT_SMOKE=1 $<TARGET_FILE:deepin-terminal-ghostty>)
```

- [ ] **Step 4: Run build and tests to verify the smoke path passes**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: build passes and `app_starts` exits successfully.

- [ ] **Step 5: Run manual verification**

Run: `./build/deepin-terminal-ghostty`

Expected:
- A window opens
- A shell prompt appears
- Typing printable text echoes correctly
- Enter runs commands
- Backspace edits the command line
- Arrow keys move through shell line editing/history
- Window resize keeps the app running and updates terminal dimensions

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt src/main.cpp
git commit -m "test: add Qt terminal smoke test"
```
