#pragma once

#include "StartupOptions.h"

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class MainWindow;

class TerminalControlService : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.TerminalGhostty.Control")

public:
    enum class ForwardOutcome {
        Forwarded,
        // Definitive: nothing was delivered or the request definitively failed,
        // so falling back to a local window cannot duplicate it.
        Rejected,
        // Indeterminate: the request may already be queued in the owner.
        Indeterminate,
    };

    using WindowFactory = std::function<MainWindow *(const StartupOptions &options)>;

    explicit TerminalControlService(WindowFactory windowFactory = {}, QObject *parent = nullptr);

    bool registerOnSessionBus(QString *errorMessage = nullptr);

    static ForwardOutcome forwardOpenTab(const QString &workingDirectory, const QString &command,
                                         const QStringList &environment, int timeoutMs = 5000,
                                         QString *errorMessage = nullptr);
    // Asked to the D-Bus daemon, so it stays reliable while the owning
    // process is unresponsive. Returns false when the session bus or daemon
    // is unavailable, since then nothing can own the name and startup must
    // fall back to an independent window.
    static bool serviceNameOwned();

public slots:
    QString list() const;
    QString newWindow();
    QString newTab(const QString &windowId = QString());
    QString openTab(const QString &workingDirectory, const QString &command,
                    const QStringList &environment = QStringList());
    QString split(const QString &paneId, const QString &orientation);
    QString send(const QString &paneId, const QString &text);
    QString exec(const QString &paneId, const QString &command);

private:
    MainWindow *windowById(const QString &windowId) const;
    MainWindow *windowForPane(const QString &paneId) const;
    MainWindow *tabTargetWindow() const;
    QList<MainWindow *> windows() const;
    QString okResponse(const QJsonObject &payload = {}) const;
    QString errorResponse(const QString &message) const;

    WindowFactory m_windowFactory;
};
