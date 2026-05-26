#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QStringList>
#include <QTextStream>

namespace {

constexpr auto kServiceName = "org.deepin.TerminalGhostty";
constexpr auto kObjectPath = "/org/deepin/TerminalGhostty/Control";
constexpr auto kInterfaceName = "org.deepin.TerminalGhostty.Control";

void printUsage(QTextStream &out) {
    out << "Usage:\n"
        << "  dtermctl list\n"
        << "  dtermctl new-window\n"
        << "  dtermctl new-tab [--window <window-id>]\n"
        << "  dtermctl split --pane <pane-id> (--horizontal|--vertical)\n"
        << "  dtermctl send --pane <pane-id> --text <text>\n"
        << "  dtermctl exec --pane <pane-id> -- <command>\n"
        << "\n"
        << "Notes:\n"
        << "  list reports each pane's current visible screen content, not full scrollback.\n";
}

QString takeOption(QStringList *arguments, const QString &name) {
    const int index = arguments->indexOf(name);
    if (index < 0 || index + 1 >= arguments->size())
        return {};
    const QString value = arguments->at(index + 1);
    arguments->removeAt(index + 1);
    arguments->removeAt(index);
    return value;
}

bool takeFlag(QStringList *arguments, const QString &name) {
    const int index = arguments->indexOf(name);
    if (index < 0)
        return false;
    arguments->removeAt(index);
    return true;
}

int callControl(const QString &method, const QVariantList &arguments) {
    QDBusInterface iface(QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath),
                         QString::fromLatin1(kInterfaceName), QDBusConnection::sessionBus());
    QTextStream err(stderr);
    if (!iface.isValid()) {
        err << "dtermctl: terminal control service is unavailable\n";
        return 1;
    }

    QDBusMessage reply = iface.callWithArgumentList(QDBus::Block, method, arguments);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        err << "dtermctl: " << reply.errorMessage() << '\n';
        return 1;
    }
    if (reply.arguments().isEmpty()) {
        err << "dtermctl: empty response\n";
        return 1;
    }

    QTextStream(stdout) << reply.arguments().first().toString() << '\n';
    return 0;
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QStringList arguments = app.arguments();
    arguments.removeFirst();

    QTextStream out(stdout);
    QTextStream err(stderr);
    if (arguments.isEmpty() || arguments.contains(QStringLiteral("--help"))
        || arguments.contains(QStringLiteral("-h"))) {
        printUsage(out);
        return arguments.isEmpty() ? 2 : 0;
    }

    const QString command = arguments.takeFirst();
    if (command == QStringLiteral("list"))
        return callControl(QStringLiteral("list"), {});
    if (command == QStringLiteral("new-window"))
        return callControl(QStringLiteral("newWindow"), {});
    if (command == QStringLiteral("new-tab")) {
        const QString windowId = takeOption(&arguments, QStringLiteral("--window"));
        return callControl(QStringLiteral("newTab"), {windowId});
    }
    if (command == QStringLiteral("split")) {
        const QString paneId = takeOption(&arguments, QStringLiteral("--pane"));
        QString orientation;
        if (takeFlag(&arguments, QStringLiteral("--horizontal")))
            orientation = QStringLiteral("horizontal");
        else if (takeFlag(&arguments, QStringLiteral("--vertical")))
            orientation = QStringLiteral("vertical");
        if (paneId.isEmpty() || orientation.isEmpty()) {
            err << "dtermctl: split requires --pane and --horizontal or --vertical\n";
            return 2;
        }
        return callControl(QStringLiteral("split"), {paneId, orientation});
    }
    if (command == QStringLiteral("send")) {
        const QString paneId = takeOption(&arguments, QStringLiteral("--pane"));
        const QString text = takeOption(&arguments, QStringLiteral("--text"));
        if (paneId.isEmpty() || text.isEmpty()) {
            err << "dtermctl: send requires --pane and --text\n";
            return 2;
        }
        return callControl(QStringLiteral("send"), {paneId, text});
    }
    if (command == QStringLiteral("exec")) {
        const QString paneId = takeOption(&arguments, QStringLiteral("--pane"));
        const int separator = arguments.indexOf(QStringLiteral("--"));
        if (separator >= 0)
            arguments = arguments.mid(separator + 1);
        const QString shellCommand = arguments.join(QLatin1Char(' '));
        if (paneId.isEmpty() || shellCommand.isEmpty()) {
            err << "dtermctl: exec requires --pane and a command after --\n";
            return 2;
        }
        return callControl(QStringLiteral("exec"), {paneId, shellCommand});
    }

    err << "dtermctl: unknown command: " << command << '\n';
    printUsage(err);
    return 2;
}
