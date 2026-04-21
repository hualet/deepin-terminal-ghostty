#include "SettingsDialog.h"

#include "AppSettings.h"

#include <DWidget>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent) : DDialog(parent) {
    setWindowTitle(tr("Settings"));
    setFixedSize(560, 380);
    setupUI();
    loadSettings();
}

void SettingsDialog::setupUI() {
    auto *content = new QWidget(this);
    auto *layout = new QFormLayout(content);
    layout->setSpacing(16);
    layout->setContentsMargins(20, 20, 20, 20);

    // Font family
    m_fontCombo = new QFontComboBox(this);
    m_fontCombo->setFontFilters(QFontComboBox::MonospacedFonts);
    layout->addRow(tr("Font:"), m_fontCombo);

    // Font size
    m_fontSizeSpin = new QSpinBox(this);
    m_fontSizeSpin->setRange(5, 72);
    m_fontSizeSpin->setSuffix(" pt");
    layout->addRow(tr("Font size:"), m_fontSizeSpin);

    // Cursor shape
    m_cursorShapeCombo = new QComboBox(this);
    m_cursorShapeCombo->addItem(tr("Block"), 0);
    m_cursorShapeCombo->addItem(tr("Bar"), 1);
    m_cursorShapeCombo->addItem(tr("Underline"), 2);
    layout->addRow(tr("Cursor shape:"), m_cursorShapeCombo);

    // Cursor blink
    m_cursorBlinkCheck = new QCheckBox(tr("Enable cursor blink"), this);
    layout->addRow(QString(), m_cursorBlinkCheck);

    // Scrollback lines
    m_scrollbackSpin = new QSpinBox(this);
    m_scrollbackSpin->setRange(100, 50000);
    m_scrollbackSpin->setSingleStep(100);
    m_scrollbackSpin->setSuffix(" lines");
    layout->addRow(tr("Scrollback lines:"), m_scrollbackSpin);

    addContent(content);

    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, &SettingsDialog::onFontChanged);
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsDialog::onFontSizeChanged);
    connect(m_cursorShapeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SettingsDialog::onCursorShapeChanged);
    connect(m_cursorBlinkCheck, &QCheckBox::checkStateChanged, this, &SettingsDialog::onCursorBlinkChanged);
    connect(m_scrollbackSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsDialog::onScrollbackChanged);
}

void SettingsDialog::loadSettings() {
    auto *s = AppSettings::instance();
    QFont font = s->terminalFont();
    m_fontCombo->setCurrentFont(font);
    m_fontSizeSpin->setValue(font.pointSize());
    m_cursorShapeCombo->setCurrentIndex(m_cursorShapeCombo->findData(s->cursorShape()));
    m_cursorBlinkCheck->setChecked(s->cursorBlink());
    m_scrollbackSpin->setValue(s->scrollbackLines());
}

void SettingsDialog::onFontChanged(const QFont &font) {
    QFont f = font;
    f.setPointSize(m_fontSizeSpin->value());
    AppSettings::instance()->setTerminalFont(f);
}

void SettingsDialog::onFontSizeChanged(int size) {
    QFont f = m_fontCombo->currentFont();
    f.setPointSize(size);
    AppSettings::instance()->setTerminalFont(f);
}

void SettingsDialog::onCursorShapeChanged(int index) {
    int shape = m_cursorShapeCombo->itemData(index).toInt();
    AppSettings::instance()->setCursorShape(shape);
}

void SettingsDialog::onCursorBlinkChanged(Qt::CheckState state) {
    AppSettings::instance()->setCursorBlink(state == Qt::Checked);
}

void SettingsDialog::onScrollbackChanged(int value) {
    AppSettings::instance()->setScrollbackLines(value);
}
