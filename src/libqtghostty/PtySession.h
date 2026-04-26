#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <sys/types.h>

class QSocketNotifier;
class QTimer;

class PtySession : public QObject {
    Q_OBJECT

public:
    struct StartOptions {
        QString command;
        QString workingDirectory;
    };

    explicit PtySession(QObject *parent = nullptr);
    ~PtySession() override;

    bool start(int cols, int rows, const StartOptions &options = {});
    void write(const QByteArray &data);
    void resize(int cols, int rows, int cellWidthPx, int cellHeightPx);

signals:
    void dataReceived(const QByteArray &data);
    void dataWritten(const QByteArray &data);
    void childExited(int exitCode);
    void sessionClosed();

private slots:
    void handleMasterReadyRead();
    void handleMasterWritable();
    void handleChildPollTimeout();

private:
    bool spawn(int cols, int rows, const StartOptions &options);
    void cleanup(bool signalChild);
    void cleanupForDestruction();
    void cleanupSynchronously(bool signalChild);
    bool closeMaster();
    void destroyNotifiers();
    bool maybeEmitSessionClosed();
    void startChildPollTimer();
    bool flushWriteBuffer();
    void shutdownChild(bool signalChild);
    void shutdownChildBlocking(bool signalChild);
    bool emitSessionClosedOnce();
    void emitChildExitedOnce(int exitCode);
    void reapChildNonBlocking();
    bool setMasterNonBlocking();
    bool setMasterCloseOnExec();
    void removeShellIntegrationTempDirs();

    int m_masterFd{-1};
    pid_t m_childPid{-1};
    QSocketNotifier *m_readNotifier{nullptr};
    QSocketNotifier *m_writeNotifier{nullptr};
    QTimer *m_childPollTimer{nullptr};
    QByteArray m_writeBuffer;
    int m_writeBufferOffset{0};
    bool m_sessionClosedEmitted{false};
    bool m_childExitedEmitted{false};
    bool m_childShutdownRequested{false};
    bool m_childHupSent{false};
    bool m_childKillSent{false};
    int m_childShutdownElapsedMs{0};
    QStringList m_shellIntegrationTempDirs;
};
