#pragma once

#include <DSettingsDialog>

DWIDGET_USE_NAMESPACE

class SettingsDialog : public DSettingsDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
};
