#ifndef FTPCONNECTDLG_H
#define FTPCONNECTDLG_H

#include <QDialog>
#include <QDateTime>
#include "ftp.h"
namespace Ui {
class FtpConnectDlg;
}

// 自定义带时间戳的日志宏
#define qInfoTime() qInfo().noquote() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "[INFO] "

#define qDebugTime() qDebug().noquote() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "[DEBUG] "

#define qWarningTime() qWarning().noquote() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "[WARN] "

#define qCriticalTime() qCritical().noquote() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "[ERROR] "



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
private slots:
    void onSSLStateChanged(int state);  // SSL复选框状态改变
private:
    Ui::FtpConnectDlg *ui;
};

#endif // FTPCONNECTDLG_H
