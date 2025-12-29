#include "ftpconnectdlg.h"
#include "ui_ftpconnectdlg.h"

FtpConnectDlg::FtpConnectDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FtpConnectDlg)
{
    ui->setupUi(this);
    //    ui->le_host->setText("127.0.0.1");//192.168.217.214
    ui->le_host->setText("127.0.0.1");//192.168.217.214
    ui->le_user->setText("xb1520");
    ui->le_pass->setText("xb1520");
    connect(ui->pb_connect,&QPushButton::clicked,this,&FtpConnectDlg::onConnect);
    connect(ui->pb_cancel,&QPushButton::clicked,this,&FtpConnectDlg::onCancel);
}

FtpConnectDlg::~FtpConnectDlg()
{
    delete ui;
}

FTP_DATA FtpConnectDlg::ftp_data()
{
    return
    {
        .host = ui->le_host->text()
        ,.user = ui->le_user->text()
        ,.pass = ui->le_pass->text()
    };
}

void FtpConnectDlg::onConnect()
{
    QDialog::accept();
}

void FtpConnectDlg::onCancel(){
    QDialog::reject();
}
