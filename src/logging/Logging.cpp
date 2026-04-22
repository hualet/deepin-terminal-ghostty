#include "logging/Logging.h"

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(appLog, "org.deepin_terminal_ghostty.app")
Q_LOGGING_CATEGORY(ptyLog, "org.deepin_terminal_ghostty.pty")
Q_LOGGING_CATEGORY(terminalLog, "org.deepin_terminal_ghostty.terminal")
#else
Q_LOGGING_CATEGORY(appLog, "org.deepin_terminal_ghostty.app", QtInfoMsg)
Q_LOGGING_CATEGORY(ptyLog, "org.deepin_terminal_ghostty.pty", QtInfoMsg)
Q_LOGGING_CATEGORY(terminalLog, "org.deepin_terminal_ghostty.terminal", QtInfoMsg)
#endif
