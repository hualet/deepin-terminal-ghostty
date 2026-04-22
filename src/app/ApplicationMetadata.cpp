#include "ApplicationMetadata.h"

#include <QIcon>

void applyApplicationMetadata(Dtk::Widget::DApplication &app) {
    app.setApplicationName(QStringLiteral("deepin-terminal-ghostty"));
    app.setProductName(QStringLiteral("deepin-terminal-ghostty"));
    app.setApplicationDisplayName(QObject::tr("Deepin Terminal Ghostty"));
    app.setOrganizationName(QStringLiteral("deepin"));
    app.setApplicationVersion(QStringLiteral(PROJECT_VERSION_STR));
    app.setProductIcon(QIcon::fromTheme("utilities-terminal"));
    app.setApplicationDescription(
        QObject::tr("A feature-rich terminal emulator for Linux built with Qt6, DTK6, and libghostty-vt."));
    app.setApplicationLicense(QStringLiteral("LGPL-2.1"));
    app.setApplicationHomePage(QStringLiteral("https://github.com/linuxdeepin/deepin-terminal-ghostty"));
    app.setApplicationAcknowledgementVisible(false);
}
