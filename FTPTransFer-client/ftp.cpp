#include "ftp.h"
#include <mainwindow.h>
#include <QFile>
#include <QDebug>
#include <QFileInfo>

Ftp::Ftp():
    ftp(new ftplib())
{
    ftp->SetConnmode(ftplib::port);
}

Ftp::~Ftp()
{
    ftp->Quit();  // 关闭连接
    delete ftp;
}

bool Ftp::login(const FTP_DATA &data)
{
    if(!ftp->Connect(data.host.toStdString().c_str())) return false;
    return ftp->Login(data.user.toStdString().c_str(), data.pass.toStdString().c_str());
}


QString Ftp::pwd()
{
    char remote_pwd[256];
    memset(remote_pwd, '\0', 256);
    ftp->Pwd(remote_pwd, 256);
    qDebugTime() << "Ftp::pwd() -> remote_pwd: " << remote_pwd;
    return QString(remote_pwd);
}

std::vector<FTP_FILE_INFO> Ftp::dir()
{
    // 1. 执行LIST命令，将结果保存到本地文件"dir.txt"
    if(!ftp->Dir("dir.txt",pwd().toStdString().c_str())){
        qWarningTime() << "Ftp::dir() -> ftp->Dir() error";
        perror("Ftp::dir() -> ftp->Dir() error");
        return {};
    }
    qInfoTime() << "Ftp::dir() -> ftp->Dir() success";

    // 2. 读取生成的临时文件
    QFile file(R"(dir.txt)");
    bool result = file.open(QFile::ReadOnly);
    if(!result){
        qWarningTime() << "Ftp::dir() : open file error!";
        return std::vector<FTP_FILE_INFO>();
    }
    QString list = file.readAll();
    file.close();

    // 3. 解析文件列表
    std::vector<FTP_FILE_INFO> _list;
    QStringList lines = list.split("\n");  // FTP使用CRLF换行
    for(auto& line : lines)
    {
        // 4. 按空格分割，移除空字符串
        auto args = line.split(" ");
        args.removeAll("");
        if(args.size() < 9)continue;  // 需要至少9个字段

        // 5. 构建日期字段（字段5-7）
        QString date = args[5] + " " + args[6] +  " " + args[7];

        // 6. 文件名可能包含空格，需要合并字段8及以后
        QString file_name = args[8];
        for(int i{9};i < args.size();i++) file_name +=  " " + args[i];

        // 7. 构建文件信息结构
        _list.push_back({
            .access = args[0]
            ,.link_cnt = args[1]
            ,.ower = args[2]
            ,.group = args[3]
            ,.size = args[4]
            ,.date = date
            ,.file_name = file_name
        });
    }
    return _list;
}

bool Ftp::cd(const QString &path)
{
    return ftp->Chdir(path.toStdString().c_str());
}

bool Ftp::cdup()
{
    return ftp->Cdup();
}

bool Ftp::put(const QString &put_file)
{
    // 使用本地文件名作为远程文件名
    return ftp->Put(put_file.toStdString().c_str(),    // 包括路径
                    QFileInfo(put_file).fileName().toStdString().c_str(),  // 不包括路径
                    ftplib::image);
}

bool Ftp::get(const QString &local_file,const QString& remote_file)
{
    return ftp->Get(local_file.toStdString().c_str(), remote_file.toStdString().c_str(),ftplib::image);
}

bool Ftp::del(const QString &file)
{
    bool ret = ftp->Delete(file.toStdString().c_str());
    if(ret) return true;
    return ftp->Rmdir(file.toStdString().c_str());
}

QString Ftp::error()
{
    return ftp->LastResponse();
}
