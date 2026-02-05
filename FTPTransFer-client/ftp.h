#ifndef FTP_H
#define FTP_H
#include <QObject>
#include <ftplib.h>

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

signals:
    void connectionStatusChanged(bool connected);                      //
    void sslStatusChanged(bool enabled);                               //
    void errorOccurred(const QString& error);                          //

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
};

#endif // FTP_H
