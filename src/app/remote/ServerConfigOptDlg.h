#pragma once

#include "ServerConfig.h"

#include <DAbstractDialog>
#include <DComboBox>
#include <DCommandLinkButton>
#include <DFileChooserEdit>
#include <DLabel>
#include <DLineEdit>
#include <DPasswordEdit>
#include <DPushButton>
#include <DSpinBox>
#include <DWindowCloseButton>

DWIDGET_USE_NAMESPACE

class ServerConfigOptDlg : public DAbstractDialog {
    Q_OBJECT

public:
    enum ServerConfigOptType {
        SCT_ADD,
        SCT_MODIFY,
    };

    explicit ServerConfigOptDlg(ServerConfigOptType type = SCT_ADD, const ServerConfig &curServer = ServerConfig(),
                                QWidget *parent = nullptr);
    ~ServerConfigOptDlg() override;

    ServerConfig getData() const;
    bool isDelServer() const { return m_bDelOpt; }

private slots:
    void slotAddSaveButtonClicked();
    void setAdvanceRegionVisible(bool isVisible);

private:
    void initUI();
    void initData();
    QList<QString> getTextCodec() const;
    QList<QString> getBackSpaceKey() const;
    QList<QString> getDeleteKey() const;

    ServerConfigOptType m_type;
    ServerConfig m_curServer;

    DLabel *m_titleLabel = nullptr;
    DLabel *m_iconLabel = nullptr;
    DWindowCloseButton *m_closeButton = nullptr;
    DLineEdit *m_serverName = nullptr;
    DLineEdit *m_address = nullptr;
    DSpinBox *m_port = nullptr;
    DLineEdit *m_userName = nullptr;
    DPasswordEdit *m_password = nullptr;
    DFileChooserEdit *m_privateKey = nullptr;
    DComboBox *m_group = nullptr;
    DLineEdit *m_path = nullptr;
    DLineEdit *m_command = nullptr;
    DComboBox *m_coding = nullptr;
    DComboBox *m_backSpaceKey = nullptr;
    DComboBox *m_deleteKey = nullptr;
    DCommandLinkButton *m_advancedOptions = nullptr;
    DCommandLinkButton *m_delServer = nullptr;
    DPushButton *m_cancelButton = nullptr;
    DPushButton *m_addSaveButton = nullptr;
    QGridLayout *m_gridLayout = nullptr;
    bool m_bDelOpt = false;
};
