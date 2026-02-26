#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <QLabel>
#include <ftpconnectdlg.h>
#include "ftp.h"



// 自定义带时间戳的日志宏
#define qInfoTime() qInfo().noquote() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "[INFO] "

#define qDebugTime() qDebug().noquote() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "[DEBUG] "

#define qWarningTime() qWarning().noquote() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "[WARN] "

#define qCriticalTime() qCritical().noquote() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "[ERROR] "



QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    void init_Flist_menu();

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public:
    void timerEvent(QTimerEvent *e);
    void closeEvent(QCloseEvent* e);

public:
    static QString cur_time();

public slots:
    void onConnect();
    void cellDoubleClicked(int row);
    void cdupAction();
    void refAction();
    void putAction();
    void getAction();
    void delAction();

    void onFtpError(const QString& error);
    void onFtpSSLStatusChanged(bool enabled);

    void onGetPutResult(const QString& type, bool success, const QString& error);
    void onProgressUpdate(const QString& transferId, qint64 bytesTransferred,
                          qint64 totalBytes);
    void OnTransferInsertRow(const QString& transferId, const FTP_TRANSFER_INFO& info);
    void onTransferCompleted(const QString& transferId);
private:
    QString get_file_name(int row);
    void set_pwd();
    void clear_ui_list();
    void insert_list(const std::vector<FTP_FILE_INFO>& list);
    void insert_row(int row,const FTP_FILE_INFO& info);
    void insert_item(int row,int idx,QString item);

private:
    Ui::MainWindow *ui;
    QLabel ui_connect_status; // 连接状态
    QLabel ui_pwd;            // 当前文件夹
    QLabel ui_cur_time;       // 当前时间
    int timerId;              // 定时器ID
    Ftp ftp;
    QLabel ui_ssl_status;     // SSL状态显示
};
#endif // MAINWINDOW_H
