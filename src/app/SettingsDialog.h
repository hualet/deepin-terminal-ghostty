#pragma once

#include <DDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QSpinBox>

DWIDGET_USE_NAMESPACE

class SettingsDialog : public DDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void onFontChanged(const QFont &font);
    void onFontSizeChanged(int size);
    void onCursorShapeChanged(int index);
    void onCursorBlinkChanged(Qt::CheckState state);
    void onScrollbackChanged(int value);

private:
    void setupUI();
    void loadSettings();

    QFontComboBox *m_fontCombo = nullptr;
    QSpinBox *m_fontSizeSpin = nullptr;
    QComboBox *m_cursorShapeCombo = nullptr;
    QCheckBox *m_cursorBlinkCheck = nullptr;
    QSpinBox *m_scrollbackSpin = nullptr;
};
