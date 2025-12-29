#ifndef FTPCONNECTDLG_H
#define FTPCONNECTDLG_H

#include <QDialog>
#include "ftp.h"
namespace Ui {
class FtpConnectDlg;
}



class FtpConnectDlg : public QDialog
{
    Q_OBJECT

public:
    explicit FtpConnectDlg(QWidget *parent = nullptr);
    ~FtpConnectDlg();
public:
    FTP_DATA ftp_data();
public slots:
    void onConnect();       // 连接槽函数
    void onCancel();        // 取消槽函数
private:
    Ui::FtpConnectDlg *ui;
};

#endif // FTPCONNECTDLG_H
