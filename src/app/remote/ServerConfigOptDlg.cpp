#include "ServerConfigOptDlg.h"

#include "ServerConfigManager.h"

#include <DFontSizeManager>
#include <DGuiApplicationHelper>
#include <DPaletteHelper>
#include <DSuggestButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QTimer>
#include <QVBoxLayout>

ServerConfigOptDlg::ServerConfigOptDlg(ServerConfigOptType type, const ServerConfig &curServer, QWidget *parent)
    : DAbstractDialog(true, parent),
      m_type(type),
      m_curServer(curServer),
      m_titleLabel(new DLabel(this)),
      m_iconLabel(new DLabel(this)),
      m_closeButton(new DWindowCloseButton(this)),
      m_serverName(new DLineEdit(this)),
      m_address(new DLineEdit(this)),
      m_port(new DSpinBox(this)),
      m_userName(new DLineEdit(this)),
      m_password(new DPasswordEdit(this)),
      m_privateKey(new DFileChooserEdit(this)),
      m_group(new DComboBox(this)),
      m_path(new DLineEdit(this)),
      m_command(new DLineEdit(this)),
      m_coding(new DComboBox(this)),
      m_backSpaceKey(new DComboBox(this)),
      m_deleteKey(new DComboBox(this)),
      m_advancedOptions(new DCommandLinkButton(tr("Advanced options"), this)),
      m_delServer(new DCommandLinkButton(tr("Delete server"), this)),
      m_cancelButton(new DPushButton(tr("Cancel"), this)),
      m_addSaveButton(new DSuggestButton(tr("Add"), this)) {
    setWindowModality(Qt::WindowModal);
    setAutoFillBackground(true);
    initUI();
    initData();
}

ServerConfigOptDlg::~ServerConfigOptDlg() = default;

void ServerConfigOptDlg::initUI() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(0, 0, 0, 8);

    // Header
    m_iconLabel->setFixedSize(50, 50);
    m_iconLabel->setPixmap(QIcon::fromTheme("utilities-terminal").pixmap(36, 36));
    m_titleLabel->setText(tr("Add Server"));
    m_titleLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    DFontSizeManager::instance()->bind(m_titleLabel, DFontSizeManager::T5, QFont::DemiBold);

    auto *headLayout = new QHBoxLayout();
    headLayout->setSpacing(0);
    headLayout->setContentsMargins(12, 0, 0, 0);
    headLayout->addWidget(m_iconLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    headLayout->addWidget(m_titleLabel, 0, Qt::AlignHCenter);
    headLayout->addWidget(m_closeButton, 0, Qt::AlignRight | Qt::AlignTop);
    connect(m_closeButton, &DWindowCloseButton::clicked, this, &ServerConfigOptDlg::reject);
    mainLayout->addLayout(headLayout);

    // Form
    m_gridLayout = new QGridLayout();
    m_gridLayout->setContentsMargins(28, 0, 30, 0);
    m_gridLayout->setSpacing(8);

    auto addRow = [this](const QString &labelText, QWidget *widget, int row, int colSpan = 1) {
        auto *label = new DLabel(labelText);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        DFontSizeManager::instance()->bind(label, DFontSizeManager::T6);
        m_gridLayout->addWidget(label, row, 0);
        m_gridLayout->addWidget(widget, row, 1, 1, colSpan);
    };

    m_serverName->lineEdit()->setPlaceholderText(tr("Required"));
    addRow(tr("Server name:"), m_serverName, 0, 3);

    m_address->lineEdit()->setPlaceholderText(tr("Required"));
    addRow(tr("Address:"), m_address, 1, 2);

    auto *portLabel = new DLabel(tr("Port:"));
    portLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    DFontSizeManager::instance()->bind(portLabel, DFontSizeManager::T6);
    m_port->setRange(1, 65535);
    m_port->setValue(22);
    m_port->setSingleStep(1);
    m_port->setFixedWidth(70);
    m_port->setButtonSymbols(DSpinBox::NoButtons);
    m_port->setAttribute(Qt::WA_InputMethodEnabled, false);
    m_gridLayout->addWidget(portLabel, 1, 3);
    m_gridLayout->addWidget(m_port, 1, 4);

    m_userName->lineEdit()->setPlaceholderText(tr("Required"));
    addRow(tr("Username:"), m_userName, 2, 3);

    m_password->lineEdit()->setAttribute(Qt::WA_InputMethodEnabled, false);
    addRow(tr("Password:"), m_password, 3, 3);

    addRow(tr("Certificate:"), m_privateKey, 4, 3);

    DPalette advPalette = DPaletteHelper::instance()->palette(m_advancedOptions);
    advPalette.setColor(DPalette::ButtonText, advPalette.color(DPalette::Highlight));
    m_advancedOptions->setPalette(advPalette);
    m_advancedOptions->setFocusPolicy(Qt::TabFocus);
    m_gridLayout->addWidget(m_advancedOptions, 5, 0, 1, 5, Qt::AlignCenter);

    m_group->setEditable(true);
    m_group->lineEdit()->setPlaceholderText(tr("No Group"));
    auto *groupLabel = new DLabel(tr("Group:"));
    groupLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    DFontSizeManager::instance()->bind(groupLabel, DFontSizeManager::T6);
    m_gridLayout->addWidget(groupLabel, 6, 0);
    m_gridLayout->addWidget(m_group, 6, 1, 1, 3);

    auto *pathLabel = new DLabel(tr("Path:"));
    pathLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    DFontSizeManager::instance()->bind(pathLabel, DFontSizeManager::T6);
    m_gridLayout->addWidget(pathLabel, 7, 0);
    m_gridLayout->addWidget(m_path, 7, 1, 1, 3);

    auto *commandLabel = new DLabel(tr("Command:"));
    commandLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    DFontSizeManager::instance()->bind(commandLabel, DFontSizeManager::T6);
    m_gridLayout->addWidget(commandLabel, 8, 0);
    m_gridLayout->addWidget(m_command, 8, 1, 1, 3);

    auto *codingLabel = new DLabel(tr("Encoding:"));
    codingLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    DFontSizeManager::instance()->bind(codingLabel, DFontSizeManager::T6);
    m_gridLayout->addWidget(codingLabel, 9, 0);
    m_gridLayout->addWidget(m_coding, 9, 1, 1, 3);

    auto *backspaceLabel = new DLabel(tr("Backspace key:"));
    backspaceLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    DFontSizeManager::instance()->bind(backspaceLabel, DFontSizeManager::T6);
    m_gridLayout->addWidget(backspaceLabel, 10, 0);
    m_gridLayout->addWidget(m_backSpaceKey, 10, 1, 1, 3);

    auto *deleteLabel = new DLabel(tr("Delete key:"));
    deleteLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    DFontSizeManager::instance()->bind(deleteLabel, DFontSizeManager::T6);
    m_gridLayout->addWidget(deleteLabel, 11, 0);
    m_gridLayout->addWidget(m_deleteKey, 11, 1, 1, 3);

    m_delServer->setPalette(advPalette);
    m_gridLayout->addWidget(m_delServer, 12, 0, 1, 5, Qt::AlignCenter);

    mainLayout->addLayout(m_gridLayout, 1);

    // Hide advanced options by default
    setAdvanceRegionVisible(false);

    // Buttons
    m_addSaveButton->setDefault(true);
    if (m_type == SCT_MODIFY) {
        m_titleLabel->setText(tr("Edit Server"));
        m_addSaveButton->setText(tr("Save"));
    }

    auto *btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(10, 0, 10, 0);
    btnLayout->setSpacing(9);
    btnLayout->addWidget(m_cancelButton);
    btnLayout->addWidget(m_addSaveButton);
    mainLayout->addLayout(btnLayout);

    connect(m_cancelButton, &DPushButton::clicked, this, &ServerConfigOptDlg::reject);
    connect(m_addSaveButton, &DPushButton::clicked, this, &ServerConfigOptDlg::slotAddSaveButtonClicked);
    connect(m_advancedOptions, &DCommandLinkButton::clicked, this, [this]() { setAdvanceRegionVisible(true); });
    connect(m_delServer, &DCommandLinkButton::clicked, this, [this]() {
        m_bDelOpt = true;
        accept();
    });

    setLayout(mainLayout);
    setFixedSize(460, 390);
}

void ServerConfigOptDlg::initData() {
    m_coding->addItems(getTextCodec());
    m_backSpaceKey->addItems(getBackSpaceKey());
    m_deleteKey->addItems(getDeleteKey());

    QStringList groupList = ServerConfigManager::instance()->groups();
    groupList.removeDuplicates();
    m_group->addItems(groupList);
    m_group->setCurrentIndex(-1);

    if (m_type == SCT_MODIFY) {
        m_serverName->setText(m_curServer.m_serverName);
        m_address->setText(m_curServer.m_address);
        m_port->setValue(m_curServer.m_port.toInt());
        m_userName->setText(m_curServer.m_userName);
        m_password->setText(m_curServer.m_password);
        m_privateKey->setText(m_curServer.m_privateKey);
        m_path->setText(m_curServer.m_path);
        m_command->setText(m_curServer.m_command);
        m_group->setCurrentText(m_curServer.m_group);

        if (!m_curServer.m_encoding.isEmpty())
            m_coding->setCurrentText(m_curServer.m_encoding);
        if (!m_curServer.m_backspaceKey.isEmpty())
            m_backSpaceKey->setCurrentText(m_curServer.m_backspaceKey);
        if (!m_curServer.m_deleteKey.isEmpty())
            m_deleteKey->setCurrentText(m_curServer.m_deleteKey);
    }
}

QList<QString> ServerConfigOptDlg::getTextCodec() const {
    return QList<QString>{"UTF-8", "GB18030", "GBK", "GB2312", "BIG5", "ISO-8859-1"};
}

QList<QString> ServerConfigOptDlg::getBackSpaceKey() const {
    return QList<QString>{"ascii-del", "auto", "control-h", "escape-sequence", "tty"};
}

QList<QString> ServerConfigOptDlg::getDeleteKey() const {
    return QList<QString>{"escape-sequence", "ascii-del", "auto", "control-h", "tty"};
}

void ServerConfigOptDlg::setAdvanceRegionVisible(bool isVisible) {
    m_advancedOptions->setVisible(!isVisible);

    // Hide/show all advanced option rows (row 6-12 in grid)
    for (int row = 6; row <= 12; ++row) {
        for (int col = 0; col < m_gridLayout->columnCount(); ++col) {
            QLayoutItem *item = m_gridLayout->itemAtPosition(row, col);
            if (item && item->widget())
                item->widget()->setVisible(isVisible);
        }
    }

    if (isVisible) {
        m_delServer->setVisible(m_type == SCT_MODIFY);
        setFixedSize(460, 640);
    } else {
        setFixedSize(460, 390);
    }
}

void ServerConfigOptDlg::slotAddSaveButtonClicked() {
    if (m_serverName->text().trimmed().isEmpty()) {
        m_serverName->showAlertMessage(tr("Please enter a server name"), m_serverName);
        return;
    }
    if (m_address->text().trimmed().isEmpty()) {
        m_address->showAlertMessage(tr("Please enter an address"), m_address);
        return;
    }
    if (m_userName->text().trimmed().isEmpty()) {
        m_userName->showAlertMessage(tr("Please enter a username"), m_userName);
        return;
    }

    // Check duplicate name on add or when name changed
    QString newName = m_serverName->text().trimmed();
    if (m_type == SCT_ADD || (m_type == SCT_MODIFY && m_curServer.m_serverName != newName)) {
        auto allConfigs = ServerConfigManager::instance()->getServerConfigs();
        for (auto it = allConfigs.cbegin(); it != allConfigs.cend(); ++it) {
            for (const ServerConfig &cfg : it.value()) {
                if (cfg.m_serverName == newName) {
                    m_serverName->showAlertMessage(tr("Server name already exists"), m_serverName);
                    return;
                }
            }
        }
    }

    accept();
}

ServerConfig ServerConfigOptDlg::getData() const {
    ServerConfig config;
    config.m_serverName = m_serverName->text().trimmed();
    config.m_address = m_address->text().trimmed();
    config.m_userName = m_userName->text().trimmed();
    config.m_password = m_password->text();
    config.m_privateKey = m_privateKey->text();
    config.m_port = m_port->text();
    config.m_group = m_group->currentText().trimmed();
    config.m_path = m_path->text();
    config.m_command = m_command->text();
    config.m_encoding = m_coding->currentText();
    config.m_backspaceKey = m_backSpaceKey->currentText();
    config.m_deleteKey = m_deleteKey->currentText();
    return config;
}
