#include "SettingsDialog.h"

#include "AppSettings.h"

SettingsDialog::SettingsDialog(QWidget *parent) : DSettingsDialog(parent) {
    setWindowTitle(tr("Settings"));
    updateSettings(AppSettings::instance()->dsettings());
}
