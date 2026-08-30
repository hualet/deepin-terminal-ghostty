#include "TerminalControlService.h"

#include "MainWindow.h"
#include "QuakeWindow.h"
#include "logging/Logging.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace {

constexpr auto kServiceName = "org.deepin.TerminalGhostty";
constexpr auto kObjectPath = "/org/deepin/TerminalGhostty/Control";
constexpr auto kInterfaceName = "org.deepin.TerminalGhostty.Control";

QString encodeJson(const QJsonObject &object) {
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QUuid uuidFromControlId(const QString &id) {
    if (id.isEmpty())
        return {};
    if (id.startsWith(QLatin1Char('{')))
        return QUuid::fromString(id);
    return QUuid::fromString(QStringLiteral("{%1}").arg(id));
}

Qt::Orientation orientationFromString(const QString &value, bool *ok) {
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("horizontal") || normalized == QStringLiteral("h")) {
        *ok = true;
        return Qt::Horizontal;
    }
    if (normalized == QStringLiteral("vertical") || normalized == QStringLiteral("v")) {
        *ok = true;
        return Qt::Vertical;
    }
    *ok = false;
    return Qt::Horizontal;
}

} // namespace

TerminalControlService::TerminalControlService(WindowFactory windowFactory, QObject *parent)
    : QObject(parent), m_windowFactory(std::move(windowFactory)) {}

bool TerminalControlService::registerOnSessionBus(QString *errorMessage) {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("session bus is not available");
        return false;
    }
    if (!bus.registerService(QString::fromLatin1(kServiceName))) {
        if (errorMessage)
            *errorMessage = bus.lastError().message();
        return false;
    }
    if (!bus.registerObject(QString::fromLatin1(kObjectPath), this, QDBusConnection::ExportNonScriptableSlots)) {
        if (errorMessage)
            *errorMessage = bus.lastError().message();
        bus.unregisterService(QString::fromLatin1(kServiceName));
        return false;
    }
    qCInfo(appLog) << "Registered terminal control D-Bus service" << kServiceName << kObjectPath;
    return true;
}

TerminalControlService::ForwardOutcome TerminalControlService::forwardOpenTab(const QString &workingDirectory,
                                                                              const QString &command,
                                                                              const QStringList &environment,
                                                                              int timeoutMs, QString *errorMessage) {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        // Without a session bus the request was never sent to any instance.
        if (errorMessage)
            *errorMessage = QStringLiteral("session bus is not available");
        return ForwardOutcome::Rejected;
    }

    QDBusMessage message =
        QDBusMessage::createMethodCall(QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath),
                                       QString::fromLatin1(kInterfaceName), QStringLiteral("openTab"));
    message << workingDirectory << command << environment;

    const QDBusMessage reply = bus.call(message, QDBus::Block, timeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        if (errorMessage)
            *errorMessage = reply.errorName().isEmpty() ? reply.errorMessage() : reply.errorName();
        // These failures mean the call was never executed by any instance.
        static const QStringList kDeterministicErrors = {
            QStringLiteral("org.freedesktop.DBus.Error.ServiceUnknown"),
            // The owner holds the name but has not exported the object yet
            // (simultaneous startup): provably nothing received the call.
            QStringLiteral("org.freedesktop.DBus.Error.UnknownObject"),
            QStringLiteral("org.freedesktop.DBus.Error.UnknownInterface"),
            QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
            QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"),
            QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
        };
        if (kDeterministicErrors.contains(reply.errorName()))
            return ForwardOutcome::Rejected;
        return ForwardOutcome::Indeterminate;
    }

    const QJsonObject response = QJsonDocument::fromJson(reply.arguments().value(0).toString().toUtf8()).object();
    if (!response.value(QStringLiteral("ok")).toBool()) {
        if (errorMessage)
            *errorMessage = response.value(QStringLiteral("error")).toString(QStringLiteral("request rejected"));
        return ForwardOutcome::Rejected;
    }
    return ForwardOutcome::Forwarded;
}

bool TerminalControlService::serviceNameOwned() {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return false;

    QDBusMessage message =
        QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                                       QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameHasOwner"));
    message << QString::fromLatin1(kServiceName);

    const QDBusMessage reply = bus.call(message, QDBus::Block, 2000);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        // A daemon that cannot answer NameHasOwner could not have routed a
        // queued openTab either; treat bus and daemon failures as "no owner"
        // so startup falls back to an independent window instead of exiting.
        return false;
    }
    return reply.arguments().value(0).toBool();
}

QString TerminalControlService::list() const {
    QJsonArray windowArray;
    for (auto *window : windows())
        windowArray.append(window->controlSnapshot());

    QJsonObject payload;
    payload.insert(QStringLiteral("windows"), windowArray);
    return okResponse(payload);
}

QString TerminalControlService::newWindow() {
    if (!m_windowFactory)
        return errorResponse(QStringLiteral("new-window is not available in this process"));

    auto *window = m_windowFactory(StartupOptions{});
    if (!window)
        return errorResponse(QStringLiteral("failed to create window"));

    QJsonObject payload;
    payload.insert(QStringLiteral("window"), window->controlSnapshot());
    return okResponse(payload);
}

QString TerminalControlService::newTab(const QString &windowId) {
    auto *window = windowById(windowId);
    if (!window)
        return errorResponse(QStringLiteral("window not found"));

    QString paneId;
    if (!window->controlNewTab(&paneId))
        return errorResponse(QStringLiteral("failed to create tab"));

    QJsonObject payload;
    payload.insert(QStringLiteral("windowId"), window->controlWindowId());
    payload.insert(QStringLiteral("paneId"), paneId);
    return okResponse(payload);
}

QString TerminalControlService::openTab(const QString &workingDirectory, const QString &command,
                                        const QStringList &environment) {
    auto *window = tabTargetWindow();
    bool createdWindow = false;
    if (!window && m_windowFactory) {
        // A quake-only (or windowless) owner must not wedge the reuse service:
        // host a new normal window in this process and put the requested tab
        // in it as the window's initial session.
        StartupOptions options;
        options.workingDirectory = workingDirectory;
        options.execute = command;
        options.environment = environment;
        window = m_windowFactory(options);
        createdWindow = window != nullptr;
    }
    if (!window)
        return errorResponse(QStringLiteral("no terminal window available"));

    QString paneId;
    if (createdWindow) {
        // The factory put the requested session in the initial tab already.
        const QJsonArray tabs = window->controlSnapshot().value(QStringLiteral("tabs")).toArray();
        if (!tabs.isEmpty()) {
            const QJsonArray panes = tabs.first().toObject().value(QStringLiteral("panes")).toArray();
            if (!panes.isEmpty())
                paneId = panes.first().toObject().value(QStringLiteral("id")).toString();
        }
    } else if (!window->controlOpenTab(workingDirectory, command, environment, &paneId)) {
        return errorResponse(QStringLiteral("failed to create tab"));
    }

    window->show();
    window->raise();
    window->activateWindow();

    QJsonObject payload;
    payload.insert(QStringLiteral("windowId"), window->controlWindowId());
    payload.insert(QStringLiteral("paneId"), paneId);
    return okResponse(payload);
}

QString TerminalControlService::split(const QString &paneId, const QString &orientation) {
    if (uuidFromControlId(paneId).isNull())
        return errorResponse(QStringLiteral("invalid pane id"));

    bool ok = false;
    const Qt::Orientation parsedOrientation = orientationFromString(orientation, &ok);
    if (!ok)
        return errorResponse(QStringLiteral("orientation must be horizontal or vertical"));

    auto *window = windowForPane(paneId);
    if (!window)
        return errorResponse(QStringLiteral("pane not found"));

    QString newPaneId;
    if (!window->controlSplitPane(uuidFromControlId(paneId), parsedOrientation, &newPaneId))
        return errorResponse(QStringLiteral("failed to split pane"));

    QJsonObject payload;
    payload.insert(QStringLiteral("windowId"), window->controlWindowId());
    payload.insert(QStringLiteral("paneId"), newPaneId);
    return okResponse(payload);
}

QString TerminalControlService::send(const QString &paneId, const QString &text) {
    if (uuidFromControlId(paneId).isNull())
        return errorResponse(QStringLiteral("invalid pane id"));
    if (text.isEmpty())
        return errorResponse(QStringLiteral("text is empty"));

    auto *window = windowForPane(paneId);
    if (!window)
        return errorResponse(QStringLiteral("pane not found"));
    if (!window->controlSendText(uuidFromControlId(paneId), text))
        return errorResponse(QStringLiteral("failed to send text"));

    return okResponse();
}

QString TerminalControlService::exec(const QString &paneId, const QString &command) {
    if (uuidFromControlId(paneId).isNull())
        return errorResponse(QStringLiteral("invalid pane id"));
    if (command.isEmpty())
        return errorResponse(QStringLiteral("command is empty"));

    auto *window = windowForPane(paneId);
    if (!window)
        return errorResponse(QStringLiteral("pane not found"));
    if (!window->controlExecuteCommand(uuidFromControlId(paneId), command))
        return errorResponse(QStringLiteral("failed to execute command"));

    return okResponse();
}

MainWindow *TerminalControlService::windowById(const QString &windowId) const {
    const auto candidates = windows();
    if (candidates.isEmpty())
        return nullptr;
    if (windowId.isEmpty())
        return candidates.first();
    for (auto *window : candidates) {
        if (window->controlWindowId() == windowId)
            return window;
    }
    return nullptr;
}

MainWindow *TerminalControlService::windowForPane(const QString &paneId) const {
    const QString normalizedPaneId = uuidFromControlId(paneId).toString(QUuid::WithoutBraces);
    for (auto *window : windows()) {
        const QJsonArray tabs = window->controlSnapshot().value(QStringLiteral("tabs")).toArray();
        for (const auto &tabValue : tabs) {
            const QJsonArray panes = tabValue.toObject().value(QStringLiteral("panes")).toArray();
            for (const auto &paneValue : panes) {
                if (paneValue.toObject().value(QStringLiteral("id")).toString() == normalizedPaneId)
                    return window;
            }
        }
    }
    return nullptr;
}

MainWindow *TerminalControlService::tabTargetWindow() const {
    const auto candidates = windows();
    if (candidates.isEmpty())
        return nullptr;

    // Never inject tabs into a quake window; it is a transient drop-down surface.
    const auto isQuake = [](MainWindow *window) { return qobject_cast<QuakeWindow *>(window) != nullptr; };

    if (auto *active = qobject_cast<MainWindow *>(QApplication::activeWindow())) {
        if (!isQuake(active) && candidates.contains(active))
            return active;
    }

    for (auto *window : candidates) {
        if (!isQuake(window))
            return window;
    }
    return nullptr;
}

QList<MainWindow *> TerminalControlService::windows() const {
    QList<MainWindow *> result;
    for (auto *widget : QApplication::topLevelWidgets()) {
        auto *window = qobject_cast<MainWindow *>(widget);
        if (window && !window->controlWindowId().isEmpty())
            result.append(window);
    }
    return result;
}

QString TerminalControlService::okResponse(const QJsonObject &payload) const {
    QJsonObject response = payload;
    response.insert(QStringLiteral("ok"), true);
    return encodeJson(response);
}

QString TerminalControlService::errorResponse(const QString &message) const {
    QJsonObject response;
    response.insert(QStringLiteral("ok"), false);
    response.insert(QStringLiteral("error"), message);
    return encodeJson(response);
}
