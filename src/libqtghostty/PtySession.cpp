#include "PtySession.h"

#include <QDebug>
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
constexpr const char kTermEnv[] = "TERM=xterm-256color";

std::string resolveShellPath() {
    const QByteArray shellEnv = qgetenv("SHELL");
    if (!shellEnv.isEmpty()) {
        return shellEnv.constData();
    }

    struct passwd *userInfo = ::getpwuid(::getuid());
    if (userInfo != nullptr && userInfo->pw_shell != nullptr && userInfo->pw_shell[0] != '\0') {
        return userInfo->pw_shell;
    }

    return "/bin/sh";
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

bool PtySession::start(int cols, int rows) {
    if (m_masterFd >= 0 || m_childPid > 0) {
        return false;
    }

    destroyNotifiers();
    m_childShutdownRequested = false;
    m_childHupSent = false;
    m_childKillSent = false;
    m_childShutdownElapsedMs = 0;
    m_writeBufferOffset = 0;
    return spawn(cols, rows);
}

void PtySession::write(const QByteArray &data) {
    if (m_masterFd < 0 || data.isEmpty()) {
        return;
    }

    const int pendingBytes = pendingWriteBytes(m_writeBuffer, m_writeBufferOffset);
    const int capacityLeft = kMaxPendingWriteBytes - pendingBytes;
    if (capacityLeft <= 0) {
        qWarning() << "Dropping PTY write data because the pending buffer is full";
        return;
    }

    const int requestedBytes = static_cast<int>(data.size());
    const int bytesToAppend = qMin(capacityLeft, requestedBytes);
    if (bytesToAppend < requestedBytes) {
        qWarning() << "Dropping PTY write data because the pending buffer exceeded 1 MiB";
    }

    m_writeBuffer.append(data.constData(), bytesToAppend);
    (void)flushWriteBuffer();
}

void PtySession::resize(int cols, int rows, int cellWidthPx, int cellHeightPx) {
    applyWindowSize(m_masterFd, cols, rows, cellWidthPx, cellHeightPx);
}

void PtySession::handleMasterReadyRead() {
    if (m_masterFd < 0) {
        return;
    }

    char buffer[4096];
    while (true) {
        const ssize_t bytesRead = ::read(m_masterFd, buffer, sizeof(buffer));
        if (bytesRead > 0) {
            QPointer<PtySession> guard(this);
            emit dataReceived(QByteArray(buffer, static_cast<int>(bytesRead)));
            if (!guard) {
                return;
            }
            continue;
        }

        if (bytesRead == 0) {
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
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
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

bool PtySession::spawn(int cols, int rows) {
#if !defined(__linux__)
    Q_UNUSED(cols);
    Q_UNUSED(rows);
    return false;
#else
    int masterFd = -1;
    const std::string shellPath = resolveShellPath();
    std::vector<std::string> envStorage;
    std::vector<char *> envp;

    envStorage.reserve(1);
    envStorage.emplace_back(kTermEnv);
    envp.reserve(2);
    envp.push_back(envStorage.back().data());
    envp.push_back(nullptr);

    winsize ws{};
    ws.ws_col = cols > 0 ? static_cast<unsigned short>(cols) : 0;
    ws.ws_row = rows > 0 ? static_cast<unsigned short>(rows) : 0;

    const pid_t childPid = ::forkpty(&masterFd, nullptr, nullptr, &ws);
    if (childPid < 0) {
        return false;
    }

    if (childPid == 0) {
        char *const argv[] = {
            const_cast<char *>(shellPath.c_str()),
            nullptr,
        };
        ::execve(shellPath.c_str(), argv, envp.data());
        _exit(127);
    }

    m_masterFd = masterFd;
    m_childPid = childPid;
    m_sessionClosedEmitted = false;
    m_writeBuffer.clear();
    m_writeBufferOffset = 0;
    m_childShutdownRequested = false;
    m_childHupSent = false;
    m_childKillSent = false;
    m_childShutdownElapsedMs = 0;

    if (!setMasterNonBlocking()) {
        cleanupSynchronously(true);
        return false;
    }

    if (!setMasterCloseOnExec()) {
        cleanupSynchronously(true);
        return false;
    }

    destroyNotifiers();
    m_readNotifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated, this, &PtySession::handleMasterReadyRead);
    startChildPollTimer();

    applyWindowSize(m_masterFd, cols, rows, 0, 0);
    return true;
#endif
}

void PtySession::cleanup(bool signalChild) {
    destroyNotifiers();
    m_writeBuffer.clear();
    m_writeBufferOffset = 0;

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
    emit sessionClosed();
    return static_cast<bool>(guard);
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
            qWarning() << "Skipping forced PTY shutdown because the process group is not signalable";
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
        qWarning() << "Skipping SIGKILL for PTY shutdown because the process group is not signalable";
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

    qWarning() << "Timed out waiting for PTY child shutdown";
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
