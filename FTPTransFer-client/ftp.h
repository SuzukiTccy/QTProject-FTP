#ifndef FTP_H
#define FTP_H
#include <QObject>
#include <ftplib.h>

struct FTP_DATA
{
    QString host;
    QString user;
    QString pass;
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

class Ftp
{
public:
    Ftp();
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
private:
    // 禁用拷贝和赋值
    Ftp(const Ftp&) = delete;
    Ftp& operator=(const Ftp&) = delete;

private:
    ftplib* ftp;             // 指向ftplib库的FTP连接句柄，用于所有底层FTP操作
    std::string cur_path;    // 当前FTP服务器路径，缓存以减少频繁的PWD命令调用
};

#endif // FTP_H
