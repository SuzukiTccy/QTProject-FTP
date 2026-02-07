#ifndef FTP_H
#define FTP_H
#include <QObject>
#include <ftplib.h>
#include <QFile>
#include <QMutex>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

// 自定义带时间戳的日志宏
#define qInfoTime() qInfo().noquote() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "[INFO] "

#define qDebugTime() qDebug().noquote() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "[DEBUG] "

#define qWarningTime() qWarning().noquote() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "[WARN] "

#define qCriticalTime() qCritical().noquote() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "[ERROR] "

struct FTP_DATA
{
    QString host;
    QString user;
    QString pass;
    int port = 21;               // 新增：FTP端口，默认为21
    bool useSSL = false;         // 新增：是否使用SSL/TLS
    QString sslCertPath;         // 新增：SSL证书路径（可选）
    bool verifyPeer = false;     // 新增：是否验证服务器证书（自签名证书设为false）
};

struct FTP_FILE_INFO
{
    QString access;             //权限
    QString link_cnt;           //链接数
    QString ower;               //属主
    QString group;              //属组
    QString size;               //大小
    QString date;               //日期(3)
    QString file_name;          //文件名(last)
    //    uintptr_t size;
    //    bool is_dir;
};

// 断点续传功能
struct FTP_TRANSFER_INFO {
    QString transferId;          // 传输唯一标识
    QString localPath;           // 本地文件路径
    QString remotePath;          // 远程文件路径
    qint64 fileSize;             // 文件总大小
    qint64 transferred;          // 已传输大小
    qint64 offset;               // 断点偏移量
    bool isUpload;               // true:上传, false:下载
    bool isResumeSupported;      // 是否支持续传
    QDateTime startTime;         // 开始时间
    QDateTime lastUpdate;        // 最后更新时间
    QString status;              // 状态: "waiting", "transferring", "paused", "completed", "failed"

    // 转换为JSON
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["transferId"] = transferId;
        obj["localPath"] = localPath;
        obj["remotePath"] = remotePath;
        obj["fileSize"] = QString::number(fileSize);
        obj["transferred"] = QString::number(transferred);
        obj["offset"] = QString::number(offset);
        obj["isUpload"] = isUpload;
        obj["isResumeSupported"] = isResumeSupported;
        obj["startTime"] = startTime.toString(Qt::ISODate);
        obj["lastUpdate"] = lastUpdate.toString(Qt::ISODate);
        obj["status"] = status;
        return obj;
    }

    // 从JSON解析
    static FTP_TRANSFER_INFO fromJson(const QJsonObject& obj) {
        FTP_TRANSFER_INFO info;
        info.transferId = obj["transferId"].toString();
        info.localPath = obj["localPath"].toString();
        info.remotePath = obj["remotePath"].toString();
        info.fileSize = obj["fileSize"].toString().toLongLong();
        info.transferred = obj["transferred"].toString().toLongLong();
        info.offset = obj["offset"].toString().toLongLong();
        info.isUpload = obj["isUpload"].toBool();
        info.isResumeSupported = obj["isResumeSupported"].toBool();
        info.startTime = QDateTime::fromString(obj["startTime"].toString(), Qt::ISODate);
        info.lastUpdate = QDateTime::fromString(obj["lastUpdate"].toString(), Qt::ISODate);
        info.status = obj["status"].toString();
        return info;
    }
};

class Ftp : public QObject
{
    Q_OBJECT
public:
    explicit Ftp(QObject* parent = nullptr);
    ~Ftp();
public:
    QString pwd();                                                     // 获取当前工作目录
    std::vector<FTP_FILE_INFO> dir();                                  // 列出当前目录内容
    bool login(const FTP_DATA& data);                                  // 登录FTP服务器
    bool cd(const QString& path);                                      // 改变当前工作目录
    bool cdup();                                                       // 返回上级目录
    bool put(const QString& put_file);                                 // 上传文件到FTP服务器
    bool get(const QString& get_file, const QString& local_file);      // 从FTP服务器下载文件
    bool del(const QString& file);                                     // 删除FTP服务器上的文件
    QString error();                                                   // 获取错误信息
    bool reconnect();                                                  // 重连函数

    // SSL相关方法
    bool setSSL(bool enable);                                          // 启用/禁用SSL
    bool setDataEncryption(bool enable);                               // 设置数据连接加密

    // 连接状态
    bool isConnected() const { return m_connected; }
    bool isSSLEnabled() const { return m_useSSL; }



    // 断点续传相关方法
    bool getResume(const QString& local_file, const QString& remote_file,
                   qint64 offset = 0);
    bool putResume(const QString& local_file, const QString& remote_file,
                   qint64 offset = 0);

    // 传输管理
    bool pauseTransfer(const QString& transferId);                     // 暂停传输
    bool resumeTransfer(const QString& transferId);                    // 继续传输
    bool cancelTransfer(const QString& transferId);                    // 取消传输
    QList<FTP_TRANSFER_INFO> getActiveTransfers() const;               // 获得正在传输的连接信息

    // 进度查询
    qint64 getRemoteFileSize(const QString& remote_file);              // 获得远程文件大小
    bool checkResumeSupported(const QString& remote_file);

signals:
    void connectionStatusChanged(bool connected);                      // 连接状态
    void sslStatusChanged(bool enabled);                               // SSL连接状态
    void errorOccurred(const QString& error);                          // 错误信号

    // 断点续传相关信号
    void transferProgress(const QString& transferId, qint64 bytesTransferred,
                          qint64 totalBytes);
    void transferStarted(const QString& transferId, const FTP_TRANSFER_INFO& info);
    void transferPaused(const QString& transferId, qint64 bytesTransferred);
    void transferResumed(const QString& transferId, qint64 offset);
    void transferCompleted(const QString& transferId);
    void transferFailed(const QString& transferId, const QString& error);

    void getOrPutResult(const QString& type, bool success, const QString& error);

private:
    // 禁用拷贝和赋值
    Ftp(const Ftp&) = delete;
    Ftp& operator=(const Ftp&) = delete;

    // SSL初始化
    bool initSSL();
    bool setupSSLSession();

    // 内部连接方法
    bool connectToHost(const QString& host, int port);
    bool performLogin(const QString& user, const QString& pass);

    // 传输状态管理
    void saveTransferState(const FTP_TRANSFER_INFO& info);
    FTP_TRANSFER_INFO loadTransferState(const QString& transferId);
    void removeTransferState(const QString& transferId);
    QString generateTransferId() const;

    // 文件操作辅助
    qint64 getLocalFileSize(const QString& filePath);
    bool validateFileForResume(const QString& filePath, qint64 expectedSize);

private:
    ftplib* ftp;                  // 指向ftplib库的FTP连接句柄，用于所有底层FTP操作
    std::string cur_path;         // 当前FTP服务器路径，缓存以减少频繁的PWD命令调用
    FTP_DATA savedFtpData;        // 保存连接信息

    // SSL相关
    bool m_useSSL = false;        // 是否使用SSL
    bool m_connected = false;     // 连接状态

    // 重连相关
    int maxRetryCount = 3;        // 最大重试次数
    int currentRetryCount = 0;    // 当前重试次数
    bool isConnecting = false;    // 正在连接标志

    // 断点续传相关
    QMap<QString, FTP_TRANSFER_INFO> activeTransfers;         // 活动传输
    QString transfersStateFile = "ftp_transfers_state.json";  // 状态文件
    mutable QMutex transfersMutex;                            // 传输状态互斥锁
};

#endif // FTP_H
