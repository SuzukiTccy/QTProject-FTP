#include "ftpconnectdlg.h"
#include "ui_ftpconnectdlg.h"
#include <QCheckBox>
#include <QSpinBox>
#include <QMessageBox>

FtpConnectDlg::FtpConnectDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FtpConnectDlg)
{
    ui->setupUi(this);
    // 设置默认值
    ui->le_host->setText("127.0.0.1");
    ui->le_user->setText("xb1520");
    ui->le_pass->setText("xb1520");

    // 设置密码显示为圆点
    ui->le_pass->setEchoMode(QLineEdit::Password);

    // 连接信号槽
    connect(ui->pb_connect, &QPushButton::clicked, this, &FtpConnectDlg::onConnect);
    connect(ui->pb_cancel, &QPushButton::clicked, this, &FtpConnectDlg::onCancel);

    qInfoTime() << "FtpConnectDlg::FtpConnectDlg() -> finished";
}

FtpConnectDlg::~FtpConnectDlg()
{
    delete ui;
}

FTP_DATA FtpConnectDlg::ftp_data()
{
    FTP_DATA data;
    data.host = ui->le_host->text().trimmed();
    data.user = ui->le_user->text().trimmed();
    data.pass = ui->le_pass->text();

    // 从界面获取SSL设置（需要在UI中添加控件）
    data.useSSL = ui->cb_ssl->isChecked(); // 假设有一个名为cb_ssl的复选框

    // 临时：默认启用SSL（与服务端匹配）
    data.useSSL = true;
    data.port = 21;
    data.verifyPeer = false; // 对于自签名证书，不验证

    return data;
}

void FtpConnectDlg::onConnect()
{
    // 验证输入
    if (ui->le_host->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请输入服务器地址");
        return;
    }

    if (ui->le_user->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请输入用户名");
        return;
    }

    QDialog::accept();
}

void FtpConnectDlg::onCancel()
{
    QDialog::reject();
}

void FtpConnectDlg::onSSLStateChanged(int state)
{
    // 当SSL状态改变时，可以更新其他控件的状态
    // 例如：如果启用SSL，端口自动改为990（FTP over SSL的标准端口）
    // 但我们的服务端使用21端口进行显式SSL，所以保持21
}
