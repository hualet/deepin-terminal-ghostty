#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>

class MainWindow;

class TerminalControlService : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.TerminalGhostty.Control")

public:
    using WindowFactory = std::function<MainWindow *()>;

    explicit TerminalControlService(WindowFactory windowFactory = {}, QObject *parent = nullptr);

    bool registerOnSessionBus(QString *errorMessage = nullptr);

public slots:
    QString list() const;
    QString newWindow();
    QString newTab(const QString &windowId = QString());
    QString split(const QString &paneId, const QString &orientation);
    QString send(const QString &paneId, const QString &text);
    QString exec(const QString &paneId, const QString &command);

private:
    MainWindow *windowById(const QString &windowId) const;
    MainWindow *windowForPane(const QString &paneId) const;
    QList<MainWindow *> windows() const;
    QString okResponse(const QJsonObject &payload = {}) const;
    QString errorResponse(const QString &message) const;

    WindowFactory m_windowFactory;
};
