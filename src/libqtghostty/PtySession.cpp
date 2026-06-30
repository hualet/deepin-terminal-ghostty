#include "PtySession.h"

#include "TerminalTrace.h"
#include "logging/Logging.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSocketNotifier>
#include <QTimer>
#include <QtGlobal>

#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(__linux__)
#include <pty.h>
#endif

namespace {

constexpr int kChildPollIntervalMs = 50;
constexpr int kChildShutdownGraceMs = 300;
constexpr int kChildDestructionGraceMs = 300;
constexpr int kMaxPendingWriteBytes = 1024 * 1024;
constexpr int kReadChunkBytes = 64 * 1024;
constexpr const char kTermEnv[] = "TERM=xterm-256color";

struct ShellIntegration {
    std::vector<std::string> shellArgs;
    std::vector<std::string> extraEnv;
    QString tempDir;
};

int normalizedExitCode(int status) {
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return 1;
}

std::string resolveShellPath() {
    const QByteArray shellEnv = qgetenv("SHELL");
    if (!shellEnv.isEmpty()) {
        qCDebug(ptyLog) << "Resolved shell from SHELL environment" << shellEnv;
        return shellEnv.constData();
    }

    struct passwd *userInfo = ::getpwuid(::getuid());
    if (userInfo != nullptr && userInfo->pw_shell != nullptr && userInfo->pw_shell[0] != '\0') {
        qCInfo(ptyLog) << "Resolved shell from passwd entry" << userInfo->pw_shell;
        return userInfo->pw_shell;
    }

    qCWarning(ptyLog) << "Falling back to /bin/sh because no user shell was available";
    return "/bin/sh";
}

QString createTempDir() {
    QByteArray templ = QFile::encodeName(QDir::tempPath() + QStringLiteral("/deepin-terminal-ghostty-shell-XXXXXX"));
    if (::mkdtemp(templ.data()) == nullptr) {
        qCWarning(ptyLog) << "Failed to create shell integration temp dir with errno" << errno;
        return {};
    }

    return QFile::decodeName(templ.constData());
}

bool writeTextFile(const QString &path, const QByteArray &content) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(ptyLog) << "Failed to open shell integration file" << path << file.errorString();
        return false;
    }

    if (file.write(content) != content.size()) {
        qCWarning(ptyLog) << "Failed to write shell integration file" << path << file.errorString();
        return false;
    }

    return true;
}

QByteArray shellIntegrationPrelude() {
    return R"SH(
__deepin_terminal_ghostty_emit_command() {
    printf '\033]777;ShellCommand=%s\033\\' "$(printf '%s' "$1" | base64 | tr -d '\n')"
}
__deepin_terminal_ghostty_clear_command() {
    printf '\033]777;ShellCommand=\033\\'
}
__deepin_terminal_ghostty_report_result() {
    printf '\033]777;ShellCommandResult=%s\033\\' "$?"
}
)SH";
}

ShellIntegration shellIntegrationFor(const std::string &shellPath) {
    ShellIntegration integration;
    integration.shellArgs.push_back(shellPath);

    const QString shell = QFileInfo(QString::fromStdString(shellPath)).fileName();
    if (shell == QStringLiteral("bash")) {
        const QString tempDir = createTempDir();
        if (tempDir.isEmpty())
            return integration;

        const QString rcPath = tempDir + QStringLiteral("/bashrc");
        const QByteArray content = QByteArray(R"SH(
if [ -r "$HOME/.bashrc" ]; then
    . "$HOME/.bashrc"
fi
)SH") + shellIntegrationPrelude() + QByteArray(R"SH(
__deepin_terminal_ghostty_inside_preexec=0
__deepin_terminal_ghostty_preexec() {
    local command="${BASH_COMMAND:-}"
    if [ "$__deepin_terminal_ghostty_inside_preexec" = 1 ] || [ -z "$command" ]; then
        return
    fi
    case "$command" in
        __deepin_terminal_ghostty_*|trap\ * )
            return
            ;;
    esac
    __deepin_terminal_ghostty_inside_preexec=1
    __deepin_terminal_ghostty_emit_command "$command"
    __deepin_terminal_ghostty_inside_preexec=0
}
trap '__deepin_terminal_ghostty_preexec' DEBUG
PROMPT_COMMAND="__deepin_terminal_ghostty_report_result;__deepin_terminal_ghostty_clear_command${PROMPT_COMMAND:+;$PROMPT_COMMAND}"
)SH");

        integration.tempDir = tempDir;

        if (!writeTextFile(rcPath, content))
            return integration;

        integration.shellArgs = {shellPath, "--rcfile", QFile::encodeName(rcPath).constData(), "-i"};
        return integration;
    }

    if (shell == QStringLiteral("zsh")) {
        const QString tempDir = createTempDir();
        if (tempDir.isEmpty())
            return integration;

        const QString rcPath = tempDir + QStringLiteral("/.zshrc");
        const QByteArray content = QByteArray(R"SH(
if [ -r "$HOME/.zshrc" ]; then
    source "$HOME/.zshrc"
fi
)SH") + shellIntegrationPrelude() + QByteArray(R"SH(
__deepin_terminal_ghostty_preexec() {
    emulate -L zsh
    __deepin_terminal_ghostty_emit_command "$1"
}
__deepin_terminal_ghostty_precmd() {
    emulate -L zsh
    __deepin_terminal_ghostty_report_result
    __deepin_terminal_ghostty_clear_command
}
autoload -Uz add-zsh-hook 2>/dev/null
if (( $+functions[add-zsh-hook] )); then
    add-zsh-hook preexec __deepin_terminal_ghostty_preexec
    add-zsh-hook precmd __deepin_terminal_ghostty_precmd
else
    preexec_functions+=(__deepin_terminal_ghostty_preexec)
    precmd_functions+=(__deepin_terminal_ghostty_precmd)
fi
)SH");

        integration.tempDir = tempDir;

        if (!writeTextFile(rcPath, content))
            return integration;

        integration.shellArgs = {shellPath, "-i"};
        integration.extraEnv.push_back(("ZDOTDIR=" + QFile::encodeName(tempDir)).constData());
        return integration;
    }

    return integration;
}

void applyWindowSize(int fd, int cols, int rows, int cellWidthPx, int cellHeightPx) {
    if (fd < 0) {
        return;
    }

    winsize ws{};
    ws.ws_col = cols > 0 ? static_cast<unsigned short>(cols) : 0;
    ws.ws_row = rows > 0 ? static_cast<unsigned short>(rows) : 0;

    if (cols > 0 && cellWidthPx > 0) {
        const long long pixelWidth = static_cast<long long>(cols) * static_cast<long long>(cellWidthPx);
        ws.ws_xpixel = pixelWidth > 0xFFFF ? 0xFFFF : static_cast<unsigned short>(pixelWidth);
    }

    if (rows > 0 && cellHeightPx > 0) {
        const long long pixelHeight = static_cast<long long>(rows) * static_cast<long long>(cellHeightPx);
        ws.ws_ypixel = pixelHeight > 0xFFFF ? 0xFFFF : static_cast<unsigned short>(pixelHeight);
    }

    ::ioctl(fd, TIOCSWINSZ, &ws);
}

void signalProcessGroup(pid_t pid, int signalNumber) {
    if (pid > 0) {
        (void)::kill(-pid, signalNumber);
    }
}

int pendingWriteBytes(const QByteArray &buffer, int offset) {
    return buffer.size() - offset;
}

bool isSignalablePid(pid_t pid) {
    if (pid <= 0) {
        return false;
    }

    if (::kill(-pid, 0) == 0) {
        return true;
    }

    return errno != ESRCH;
}

} // namespace

PtySession::PtySession(QObject *parent) : QObject(parent) {
    m_childPollTimer = new QTimer(this);
    m_childPollTimer->setInterval(kChildPollIntervalMs);
    m_childPollTimer->setSingleShot(false);
    connect(m_childPollTimer, &QTimer::timeout, this, &PtySession::handleChildPollTimeout);
}

PtySession::~PtySession() {
    cleanupForDestruction();
}

bool PtySession::start(int cols, int rows, const StartOptions &options) {
    if (m_masterFd >= 0 || m_childPid > 0) {
        qCWarning(ptyLog) << "Refusing to start PTY session because one is already active";
        return false;
    }

    destroyNotifiers();
    m_childShutdownRequested = false;
    m_childHupSent = false;
    m_childKillSent = false;
    m_childShutdownElapsedMs = 0;
    m_writeBufferOffset = 0;
    m_childExitedEmitted = false;
    qCInfo(ptyLog) << "Starting PTY session with size" << cols << "x" << rows;
    TerminalTrace::logResize("pty.start", cols, rows, 0, 0);
    return spawn(cols, rows, options);
}

bool PtySession::hasRunningProcess() const {
    if (m_masterFd < 0 || m_childPid <= 0)
        return false;

    // The foreground process group is the ground truth for "something is
    // running": unlike shell-integration OSC sequences it cannot be emitted by
    // the program inside the terminal, so it is always safe to trust for the
    // close-confirmation check. A single tcgetpgrp() is one cheap ioctl
    // (microseconds), so it stays non-blocking even when close handling walks
    // every pane, and it detects a foreground process the instant it is
    // queried — no debounce window that could silently miss a freshly started
    // command.
    const pid_t fgPgid = ::tcgetpgrp(m_masterFd);
    if (fgPgid <= 0)
        return false;

    return fgPgid != m_childPid;
}

QString PtySession::workingDirectory() const {
    if (m_childPid <= 0)
        return {};

    QByteArray link = "/proc/" + QByteArray::number(m_childPid) + "/cwd";
    char buf[PATH_MAX];
    ssize_t len = ::readlink(link.constData(), buf, sizeof(buf) - 1);
    if (len <= 0)
        return {};

    buf[len] = '\0';
    return QString::fromUtf8(buf);
}

void PtySession::write(const QByteArray &data) {
    if (m_masterFd < 0 || data.isEmpty()) {
        return;
    }

    const int pendingBytes = pendingWriteBytes(m_writeBuffer, m_writeBufferOffset);
    const int capacityLeft = kMaxPendingWriteBytes - pendingBytes;
    if (capacityLeft <= 0) {
        qCWarning(ptyLog) << "Dropping PTY write data because the pending buffer is full";
        return;
    }

    const int requestedBytes = static_cast<int>(data.size());
    const int bytesToAppend = qMin(capacityLeft, requestedBytes);
    if (bytesToAppend < requestedBytes) {
        qCWarning(ptyLog) << "Dropping PTY write data because the pending buffer exceeded 1 MiB";
    }

    m_writeBuffer.append(data.constData(), bytesToAppend);
    TerminalTrace::logBytes("pty.write", data.left(bytesToAppend));
    (void)flushWriteBuffer();

    Q_EMIT dataWritten(data.left(bytesToAppend));
}

void PtySession::resize(int cols, int rows, int cellWidthPx, int cellHeightPx) {
    TerminalTrace::logResize("pty.resize", cols, rows, cellWidthPx, cellHeightPx);
    applyWindowSize(m_masterFd, cols, rows, cellWidthPx, cellHeightPx);
}

void PtySession::handleMasterReadyRead() {
    if (m_masterFd < 0) {
        return;
    }

    QByteArray aggregatedData;
    aggregatedData.reserve(kReadChunkBytes);

    while (true) {
        const qsizetype offset = aggregatedData.size();
        aggregatedData.resize(offset + kReadChunkBytes);
        const ssize_t bytesRead = ::read(m_masterFd, aggregatedData.data() + offset, kReadChunkBytes);
        if (bytesRead > 0) {
            aggregatedData.resize(offset + static_cast<qsizetype>(bytesRead));
            continue;
        }

        if (bytesRead == 0) {
            aggregatedData.resize(offset);
            if (!aggregatedData.isEmpty()) {
                TerminalTrace::logBytes("pty.read", aggregatedData);
                QPointer<PtySession> guard(this);
                emit dataReceived(aggregatedData);
                if (!guard) {
                    return;
                }
            }
            if (!closeMaster()) {
                return;
            }
            shutdownChild(true);
            if (!maybeEmitSessionClosed()) {
                return;
            }
            return;
        }

        if (errno == EINTR) {
            aggregatedData.resize(offset);
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            aggregatedData.resize(offset);
            if (!aggregatedData.isEmpty()) {
                TerminalTrace::logBytes("pty.read", aggregatedData);
                QPointer<PtySession> guard(this);
                emit dataReceived(aggregatedData);
                if (!guard) {
                    return;
                }
            }
            return;
        }

        aggregatedData.resize(offset);
        if (!aggregatedData.isEmpty()) {
            TerminalTrace::logBytes("pty.read", aggregatedData);
            QPointer<PtySession> guard(this);
            emit dataReceived(aggregatedData);
            if (!guard) {
                return;
            }
        }
        if (!closeMaster()) {
            return;
        }
        shutdownChild(true);
        if (!maybeEmitSessionClosed()) {
            return;
        }
        return;
    }
}

void PtySession::handleMasterWritable() {
    (void)flushWriteBuffer();
}

void PtySession::handleChildPollTimeout() {
    if (m_childPid <= 0) {
        if (m_childPollTimer != nullptr) {
            m_childPollTimer->stop();
        }
        if (!maybeEmitSessionClosed()) {
            return;
        }
        return;
    }

    int status = 0;
    while (true) {
        const pid_t result = ::waitpid(m_childPid, &status, WNOHANG);
        if (result == m_childPid) {
            emitChildExitedOnce(normalizedExitCode(status));
            m_childPid = -1;
            m_childShutdownRequested = false;
            m_childHupSent = false;
            m_childKillSent = false;
            m_childShutdownElapsedMs = 0;
            if (m_childPollTimer != nullptr) {
                m_childPollTimer->stop();
            }
            if (!maybeEmitSessionClosed()) {
                return;
            }
            return;
        }
        if (result == 0) {
            if (m_childShutdownRequested) {
                m_childShutdownElapsedMs += kChildPollIntervalMs;
                if (!m_childKillSent && m_childShutdownElapsedMs >= kChildShutdownGraceMs) {
                    signalProcessGroup(m_childPid, SIGKILL);
                    m_childKillSent = true;
                }
            }
            return;
        }
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == ECHILD) {
                m_childPid = -1;
                m_childShutdownRequested = false;
                m_childHupSent = false;
                m_childKillSent = false;
                m_childShutdownElapsedMs = 0;
                if (m_childPollTimer != nullptr) {
                    m_childPollTimer->stop();
                }
                if (!maybeEmitSessionClosed()) {
                    return;
                }
            }
            return;
        }
    }
}

bool PtySession::spawn(int cols, int rows, const StartOptions &options) {
#if !defined(__linux__)
    Q_UNUSED(cols);
    Q_UNUSED(rows);
    Q_UNUSED(options);
    return false;
#else
    int masterFd = -1;
    const std::string shellPath = resolveShellPath();
    const QByteArray command = options.command.toUtf8();
    const QByteArray workingDirectory = options.workingDirectory.toUtf8();
    ShellIntegration shellIntegration;
    if (command.isEmpty())
        shellIntegration = shellIntegrationFor(shellPath);
    std::vector<std::string> envStorage;
    std::vector<char *> envp;

    for (char **entry = environ; entry != nullptr && *entry != nullptr; ++entry) {
        if (std::strncmp(*entry, "TERM=", 5) == 0)
            continue;
        if (!shellIntegration.extraEnv.empty() && std::strncmp(*entry, "ZDOTDIR=", 7) == 0)
            continue;
        envStorage.emplace_back(*entry);
    }
    envStorage.emplace_back(kTermEnv);
    for (const std::string &entry : shellIntegration.extraEnv)
        envStorage.emplace_back(entry);

    envp.reserve(static_cast<size_t>(envStorage.size()) + 1);
    for (std::string &entry : envStorage)
        envp.push_back(entry.data());
    envp.push_back(nullptr);

    winsize ws{};
    ws.ws_col = cols > 0 ? static_cast<unsigned short>(cols) : 0;
    ws.ws_row = rows > 0 ? static_cast<unsigned short>(rows) : 0;

    const pid_t childPid = ::forkpty(&masterFd, nullptr, nullptr, &ws);
    if (childPid < 0) {
        qCWarning(ptyLog) << "forkpty failed with errno" << errno;
        return false;
    }

    if (childPid == 0) {
        if (!workingDirectory.isEmpty() && ::chdir(workingDirectory.constData()) != 0)
            _exit(127);

        if (!command.isEmpty()) {
            static constexpr char kCommandShell[] = "/bin/sh";
            char *const argv[] = {
                const_cast<char *>(kCommandShell),
                const_cast<char *>("-c"),
                const_cast<char *>(command.constData()),
                nullptr,
            };
            ::execve(kCommandShell, argv, envp.data());
            _exit(127);
        }

        std::vector<char *> argv;
        argv.reserve(shellIntegration.shellArgs.size() + 1);
        for (std::string &arg : shellIntegration.shellArgs)
            argv.push_back(arg.data());
        argv.push_back(nullptr);

        ::execve(shellPath.c_str(), argv.data(), envp.data());
        _exit(127);
    }

    m_masterFd = masterFd;
    m_childPid = childPid;
    m_sessionClosedEmitted = false;
    m_childExitedEmitted = false;
    m_writeBuffer.clear();
    m_writeBufferOffset = 0;
    m_childShutdownRequested = false;
    m_childHupSent = false;
    m_childKillSent = false;
    m_childShutdownElapsedMs = 0;
    if (!shellIntegration.tempDir.isEmpty())
        m_shellIntegrationTempDirs.append(shellIntegration.tempDir);

    if (!setMasterNonBlocking()) {
        qCWarning(ptyLog) << "Failed to enable non-blocking mode on PTY master";
        cleanupSynchronously(true);
        return false;
    }

    if (!setMasterCloseOnExec()) {
        qCWarning(ptyLog) << "Failed to mark PTY master close-on-exec";
        cleanupSynchronously(true);
        return false;
    }

    destroyNotifiers();
    m_readNotifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated, this, &PtySession::handleMasterReadyRead);
    startChildPollTimer();

    applyWindowSize(m_masterFd, cols, rows, 0, 0);
    qCInfo(ptyLog) << "PTY session started for child pid" << m_childPid;
    return true;
#endif
}

void PtySession::cleanup(bool signalChild) {
    destroyNotifiers();
    m_writeBuffer.clear();
    m_writeBufferOffset = 0;
    removeShellIntegrationTempDirs();

    if (!closeMaster()) {
        return;
    }

    shutdownChild(signalChild);
}

void PtySession::cleanupForDestruction() {
    cleanupSynchronously(true);
}

void PtySession::cleanupSynchronously(bool signalChild) {
    destroyNotifiers();
    if (m_childPollTimer != nullptr) {
        m_childPollTimer->stop();
    }
    m_writeBuffer.clear();
    m_writeBufferOffset = 0;
    removeShellIntegrationTempDirs();

    if (m_masterFd >= 0) {
        ::close(m_masterFd);
        m_masterFd = -1;
    }

    shutdownChildBlocking(signalChild);
}

bool PtySession::closeMaster() {
    destroyNotifiers();
    m_writeBuffer.clear();
    m_writeBufferOffset = 0;
    removeShellIntegrationTempDirs();

    if (m_masterFd >= 0) {
        ::close(m_masterFd);
        m_masterFd = -1;
    }

    return maybeEmitSessionClosed();
}

void PtySession::destroyNotifiers() {
    if (m_readNotifier != nullptr) {
        delete m_readNotifier;
        m_readNotifier = nullptr;
    }

    if (m_writeNotifier != nullptr) {
        delete m_writeNotifier;
        m_writeNotifier = nullptr;
    }
}

void PtySession::removeShellIntegrationTempDirs() {
    for (const QString &path : std::as_const(m_shellIntegrationTempDirs))
        QDir(path).removeRecursively();
    m_shellIntegrationTempDirs.clear();
}

bool PtySession::maybeEmitSessionClosed() {
    if (m_masterFd >= 0 || m_childPid > 0) {
        return true;
    }

    return emitSessionClosedOnce();
}

void PtySession::startChildPollTimer() {
    if (m_childPollTimer == nullptr || m_childPid <= 0) {
        return;
    }

    m_childPollTimer->start();
}

bool PtySession::emitSessionClosedOnce() {
    if (m_sessionClosedEmitted) {
        return true;
    }

    QPointer<PtySession> guard(this);
    m_sessionClosedEmitted = true;
    qCInfo(ptyLog) << "PTY session closed";
    emit sessionClosed();
    return static_cast<bool>(guard);
}

void PtySession::emitChildExitedOnce(int exitCode) {
    if (m_childExitedEmitted)
        return;

    m_childExitedEmitted = true;
    emit childExited(exitCode);
}

bool PtySession::flushWriteBuffer() {
    if (m_masterFd < 0) {
        m_writeBuffer.clear();
        m_writeBufferOffset = 0;
        if (m_writeNotifier != nullptr) {
            m_writeNotifier->setEnabled(false);
        }
        return false;
    }

    if (m_writeNotifier != nullptr) {
        m_writeNotifier->setEnabled(false);
    }

    while (!m_writeBuffer.isEmpty()) {
        const int bytesPending = pendingWriteBytes(m_writeBuffer, m_writeBufferOffset);
        if (bytesPending <= 0) {
            m_writeBuffer.clear();
            m_writeBufferOffset = 0;
            break;
        }

        const ssize_t written =
            ::write(m_masterFd, m_writeBuffer.constData() + m_writeBufferOffset, static_cast<size_t>(bytesPending));
        if (written > 0) {
            m_writeBufferOffset += static_cast<int>(written);
            if (m_writeBufferOffset >= m_writeBuffer.size()) {
                m_writeBuffer.clear();
                m_writeBufferOffset = 0;
            } else if (m_writeBufferOffset > kMaxPendingWriteBytes / 2) {
                m_writeBuffer.remove(0, m_writeBufferOffset);
                m_writeBufferOffset = 0;
            }
            continue;
        }

        if (written < 0 && errno == EINTR) {
            continue;
        }

        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (m_writeNotifier == nullptr) {
                m_writeNotifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Write, this);
                connect(m_writeNotifier, &QSocketNotifier::activated, this, &PtySession::handleMasterWritable);
            }
            m_writeNotifier->setEnabled(true);
            return false;
        }

        qCWarning(ptyLog) << "Write to PTY master failed with errno" << errno;
        m_writeBuffer.clear();
        m_writeBufferOffset = 0;
        if (!closeMaster()) {
            return false;
        }
        shutdownChild(true);
        return false;
    }

    if (m_writeNotifier != nullptr) {
        m_writeNotifier->setEnabled(false);
    }
    return true;
}

void PtySession::shutdownChild(bool signalChild) {
    if (m_childPid <= 0) {
        if (!maybeEmitSessionClosed()) {
            return;
        }
        return;
    }

    if (signalChild && !m_childHupSent) {
        signalProcessGroup(m_childPid, SIGHUP);
        m_childHupSent = true;
    }

    m_childShutdownRequested = true;
    m_childShutdownElapsedMs = 0;

    startChildPollTimer();
}

void PtySession::shutdownChildBlocking(bool signalChild) {
    if (m_childPid <= 0) {
        return;
    }

    if (signalChild) {
        signalProcessGroup(m_childPid, SIGHUP);
    }

    int status = 0;
    bool signalable = !signalChild || isSignalablePid(m_childPid);
    for (int elapsedMs = 0; elapsedMs < kChildDestructionGraceMs; elapsedMs += kChildPollIntervalMs) {
        while (true) {
            const pid_t result = ::waitpid(m_childPid, &status, WNOHANG);
            if (result == m_childPid) {
                m_childPid = -1;
                m_childShutdownRequested = false;
                m_childHupSent = false;
                m_childKillSent = false;
                m_childShutdownElapsedMs = 0;
                return;
            }
            if (result == 0) {
                break;
            }
            if (result < 0) {
                if (errno == EINTR) {
                    continue;
                }
                if (errno == ECHILD) {
                    m_childPid = -1;
                    m_childShutdownRequested = false;
                    m_childHupSent = false;
                    m_childKillSent = false;
                    m_childShutdownElapsedMs = 0;
                }
                return;
            }
        }

        if (!signalable) {
            qCWarning(ptyLog) << "Skipping forced PTY shutdown because the process group is not signalable";
            m_childPid = -1;
            m_childShutdownRequested = false;
            m_childHupSent = false;
            m_childKillSent = false;
            m_childShutdownElapsedMs = 0;
            return;
        }

        ::usleep(static_cast<useconds_t>(kChildPollIntervalMs * 1000));
    }

    if (isSignalablePid(m_childPid)) {
        signalProcessGroup(m_childPid, SIGKILL);
    } else {
        qCWarning(ptyLog) << "Skipping SIGKILL for PTY shutdown because the process group is not signalable";
        m_childPid = -1;
        m_childShutdownRequested = false;
        m_childHupSent = false;
        m_childKillSent = false;
        m_childShutdownElapsedMs = 0;
        return;
    }

    for (int elapsedMs = 0; elapsedMs < kChildDestructionGraceMs; elapsedMs += kChildPollIntervalMs) {
        while (true) {
            const pid_t result = ::waitpid(m_childPid, &status, WNOHANG);
            if (result == m_childPid) {
                m_childPid = -1;
                m_childShutdownRequested = false;
                m_childHupSent = false;
                m_childKillSent = false;
                m_childShutdownElapsedMs = 0;
                return;
            }
            if (result == 0) {
                break;
            }
            if (result < 0) {
                if (errno == EINTR) {
                    continue;
                }
                if (errno == ECHILD) {
                    m_childPid = -1;
                    m_childShutdownRequested = false;
                    m_childHupSent = false;
                    m_childKillSent = false;
                    m_childShutdownElapsedMs = 0;
                }
                return;
            }
        }

        ::usleep(static_cast<useconds_t>(kChildPollIntervalMs * 1000));
    }

    qCWarning(ptyLog) << "Timed out waiting for PTY child shutdown";
    m_childPid = -1;
    m_childShutdownRequested = false;
    m_childHupSent = false;
    m_childKillSent = false;
    m_childShutdownElapsedMs = 0;
}

void PtySession::reapChildNonBlocking() {
    if (m_childPid <= 0) {
        return;
    }

    int status = 0;
    while (true) {
        const pid_t result = ::waitpid(m_childPid, &status, WNOHANG);
        if (result == m_childPid) {
            m_childPid = -1;
            return;
        }
        if (result == 0) {
            return;
        }
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == ECHILD) {
                m_childPid = -1;
            }
            return;
        }
    }
}

bool PtySession::setMasterNonBlocking() {
    if (m_masterFd < 0) {
        return false;
    }

    const int flags = ::fcntl(m_masterFd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }

    if (::fcntl(m_masterFd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return false;
    }

    return true;
}

bool PtySession::setMasterCloseOnExec() {
    if (m_masterFd < 0) {
        return false;
    }

    const int flags = ::fcntl(m_masterFd, F_GETFD, 0);
    if (flags < 0) {
        return false;
    }

    if (::fcntl(m_masterFd, F_SETFD, flags | FD_CLOEXEC) < 0) {
        return false;
    }

    return true;
}
