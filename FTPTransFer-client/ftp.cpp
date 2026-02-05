#include "ftp.h"
#include <mainwindow.h>
#include <QFile>
#include <QDebug>
#include <QFileInfo>
#include <QSslSocket>
#include <QMessageBox>

Ftp::Ftp(QObject* parent) :
    QObject(parent)
{
    ftp = new ftplib();
    ftp->SetConnmode(ftplib::port);
}

Ftp::~Ftp()
{
    if(ftp){
        ftp->Quit();  // 关闭连接
        delete ftp;
        ftp = nullptr;
    }
}

bool Ftp::login(const FTP_DATA &data)
{
    savedFtpData = data;
    currentRetryCount = 0;
    isConnecting = false;
    m_useSSL = data.useSSL;

    qInfoTime() << "Ftp::login() -> Connecting to" << data.host << ":" << data.port
                << "SSL:" << (data.useSSL ? "Enabled" : "Disabled");

    // 1. 连接到服务端
    if(!connectToHost(data.host, data.port)){
        qCriticalTime() << "Ftp::login() -> Connection failed";
        emit errorOccurred("Connection failed: " + error());
        return false;
    }

    // 2. 如果需要SSL，进行SSL协商
    if(data.useSSL){
        if(!initSSL()){
            qCriticalTime() << "Ftp::login() -> SSL initialization failed";
            emit errorOccurred("SSL initialization failed");
            return false;
        }
    }

    // 3. 登录
    if(!performLogin(data.user, data.pass)){
        qCriticalTime() << "Ftp::login() -> Login failed";
        emit errorOccurred("Login failed: " + error());
        return false;
    }

    // 4. 如果使用SSL，设置数据连接加密
    if(data.useSSL){
        if(!setDataEncryption(true)) {
            qWarningTime() << "Ftp::login() -> Data encryption setup failed, continuing...";
            // 数据加密失败不一定致命，继续连接
        }
    }

    m_connected = true;
    emit connectionStatusChanged(true);
    qInfoTime() << "Ftp::login() -> Login successful";
    return true;
}

bool Ftp::connectToHost(const QString& host, int port){
    // 使用带端口号的Connect函数（需要修改ftplib或使用其他方法）
    // 注意：标准ftplib的Connect函数不支持端口参数，需要重写或使用其他方法

    // 临时方案：使用原始的Connect函数（默认端口21）
    // 如果需要支持非标准端口，需要修改ftplib或使用其他FTP库
    if (port != 21) {
        qWarningTime() << "Ftp::connectToHost() -> Non-standard port (" << port
                       << ") may not be supported by current ftplib implementation";
    }

    bool connected = ftp->Connect(host.toStdString().c_str());
    if(!connected){
        QString err = error();
        qCriticalTime() << "Ftp::connectToHost() -> Connect failed:" << err;

        // 检查是否SSL相关错误
        if(m_useSSL && err.contains("SSL", Qt::CaseInsensitive)){ // 不区分大小写
            qWarningTime() << "Ftp::connectToHost() -> SSL connection failed, trying non-SSL...";
            // 可以尝试非SSL连接，但这里我们直接失败
        }
    }

    return connected;
}


bool Ftp::initSSL()
{
    qInfoTime() << "Ftp::initSSL() -> Initializing SSL/TLS...";

    // 1. 尝试协商SSL加密
    if(!ftp->NegotiateEncryption()){
        qCriticalTime() << "Ftp::initSSL() -> SSL negotiation failed:" << error();

        // 检查是否服务器支持SSL
        QString resp = error();
        if(resp.contains("not supported", Qt::CaseInsensitive) ||
            resp.contains("not implemented", Qt::CaseInsensitive)) {
            qWarningTime() << "Ftp::initSSL() -> Server does not support SSL/TLS";

            // 询问用户是否继续非加密连接
            // 这里可以弹出对话框询问用户
            qInfoTime() << "Ftp::initSSL() -> Falling back to non-SSL connection";
            m_useSSL = false;
            emit sslStatusChanged(false);
            return true; // 允许继续非SSL连接
        }
        return false;
    }

    qInfoTime() << "Ftp::initSSL() -> SSL negotiation successful";

    // 2. 设置数据加密（如果需要）
    if(savedFtpData.useSSL){
        if(!setDataEncryption(true)){
            qWarningTime() << "Ftp::initSSL() -> Failed to set data encryption";
            // 数据加密失败不一定致命
        }
    }

    emit sslStatusChanged(true);
    return true;
}

bool Ftp::setDataEncryption(bool enable){
    if (!m_useSSL) {
        qWarningTime() << "Ftp::setDataEncryption() -> SSL not enabled";
        return false;
    }

    ftplib::dataencryption mode = enable ? ftplib::secure : ftplib::unencrypted;

    if(!ftp->SetDataEncryption(mode)){
        qCriticalTime() << "Ftp::setDataEncryption() -> Failed to set data encryption:" << error();
        return false;
    }

    qInfoTime() << "Ftp::setDataEncryption() -> Data encryption" << (enable ? "enabled" : "disabled");
    return true;
}

bool Ftp::performLogin(const QString& user, const QString& pass){
    if(!ftp->Login(user.toStdString().c_str(), pass.toStdString().c_str())){
        qCriticalTime() << "Ftp::performLogin() -> Login failed:" << error();
        return false;
    }

    qInfoTime() << "Ftp::performLogin() -> Login successful";
    return true;
}


QString Ftp::pwd()
{
    char remote_pwd[256];
    memset(remote_pwd, '\0', 256);
    if(!ftp->Pwd(remote_pwd, 256)) {
        qWarningTime() << "Ftp::pwd() -> Failed to get current directory:" << error();
        return "";
    }
    qDebugTime() << "Ftp::pwd() -> remote_pwd: " << remote_pwd;
    return QString(remote_pwd);
}



std::vector<FTP_FILE_INFO> Ftp::dir()
{
    // 1. 执行LIST命令，将结果保存到本地文件"dir.txt"
    if(!ftp->Dir("dir.txt", pwd().toStdString().c_str())){
        qWarningTime() << "Ftp::dir() -> ftp->Dir() error:" << error();
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

    // 删除临时文件
    file.remove();

    // 3. 解析文件列表
    std::vector<FTP_FILE_INFO> _list;
    QStringList lines = list.split("\n", Qt::SkipEmptyParts);
    for(auto& line : lines)
    {
        line = line.trimmed();
        if(line.isEmpty()) continue;

        // 4. 按空格分割，移除空字符串
        auto args = line.split(" ", Qt::SkipEmptyParts);
        if(args.size() < 9) continue;  // 需要至少9个字段

        // 5. 构建日期字段（字段5-7）
        QString date = args[5] + " " + args[6] + " " + args[7];

        // 6. 文件名可能包含空格，需要合并字段8及以后
        QString file_name = args[8];
        for(int i{9}; i < args.size(); i++)
            file_name += " " + args[i];

        // 7. 构建文件信息结构
        _list.push_back({
            .access = args[0],
            .link_cnt = args[1],
            .ower = args[2],
            .group = args[3],
            .size = args[4],
            .date = date,
            .file_name = file_name
        });
    }
    return _list;
}

bool Ftp::cd(const QString &path)
{
    qInfoTime() << "Ftp::cd() path: " << path;
    bool success = ftp->Chdir(path.toStdString().c_str());
    if(success) return success;

    QString res = this->error();

    // 如果是超时错误，尝试重连
    if(res.contains("timeout", Qt::CaseInsensitive) ||
        res.contains("timed out", Qt::CaseInsensitive)) {

        qInfoTime() << "cd命令超时，尝试重连后重试...";

        if(this->reconnect()) {
            // 重连成功，重新尝试cd命令
            success = ftp->Chdir(path.toStdString().c_str());
            if(success) {
                qInfoTime() << "重连后cd命令成功";
                return true;
            } else {
                qCriticalTime() << "重连后cd命令仍然失败: " << this->error();
                return false;
            }
        } else {
            qCriticalTime() << "重连失败";
            return false;
        }
    }

    // 非超时错误，直接返回失败
    qCriticalTime() << "cd命令失败: " << res;
    return false;
}

bool Ftp::cdup()
{
    qInfoTime() << "Ftp::cdup() called";
    bool success = ftp->Cdup();
    if(success) return success;

    QString res = this->error();

    // 如果是超时错误，尝试重连
    if(res.contains("timeout", Qt::CaseInsensitive) ||
        res.contains("timed out", Qt::CaseInsensitive)) {

        qInfoTime() << "cdup命令超时，尝试重连后重试...";

        if(this->reconnect()) {
            // 重连成功，重新尝试cd命令
            success = ftp->Cdup();
            if(success) {
                qInfoTime() << "重连后cdup命令成功";
                return true;
            } else {
                qCriticalTime() << "重连后cdup命令仍然失败: " << this->error();
                return false;
            }
        } else {
            qCriticalTime() << "重连失败";
            return false;
        }
    }

    // 非超时错误，直接返回失败
    qCriticalTime() << "cd命令失败: " <<  res;
    return false;
}


bool Ftp::put(const QString &put_file)
{
    // 1. 检查文件是否存在
    if(!QFile::exists(put_file)){
        qCriticalTime() << "Ftp::put() -> Local file does not exist:" << put_file;
        emit errorOccurred("Local file does not exist: " + put_file);
        return false;
    }

    QFileInfo fileInfo(put_file);
    QString remote_file = fileInfo.fileName();

    qInfoTime() << "Ftp::put() -> Uploading" << put_file << "to" << remote_file;

    // 2. 检查SSL状态
    if(m_useSSL){
        qInfoTime() << "Ftp::put() -> SSL enabled for upload";
    }

    bool success = ftp->Put(put_file.toStdString().c_str(),
                            remote_file.toStdString().c_str(),
                            ftplib::image);

    if(!success) {
        qCriticalTime() << "Ftp::put() -> Upload failed:" << error();
        emit errorOccurred("Upload failed: " + error());
    } else {
        qInfoTime() << "Ftp::put() -> Upload successful";
    }

    return success;
}


bool Ftp::get(const QString &local_file,const QString& remote_file)
{
    qInfoTime() << "Ftp::get() -> Downloading" << remote_file << "to" << local_file;

    // 检查SSL状态
    if(m_useSSL) {
        qDebugTime() << "Ftp::get() -> SSL enabled for download";
    }

    bool success = ftp->Get(local_file.toStdString().c_str(),
                            remote_file.toStdString().c_str(),
                            ftplib::image);

    if(!success) {
        qCriticalTime() << "Ftp::get() -> Download failed:" << error();
        emit errorOccurred("Download failed: " + error());
    } else {
        qInfoTime() << "Ftp::get() -> Download successful";
    }

    return success;
}


bool Ftp::del(const QString &file)
{
    qInfoTime() << "Ftp::del() -> Deleting" << file;

    bool ret = ftp->Delete(file.toStdString().c_str());
    if(ret) return true;

    // 如果删除文件失败，尝试删除目录
    ret = ftp->Rmdir(file.toStdString().c_str());

    if(!ret){
        qCriticalTime() << "Ftp::del() -> Delete failed:" << error();
        emit errorOccurred("Delete failed: " + error());
    }
    return ret;
}

QString Ftp::error()
{
    const char* response = ftp->LastResponse();
    return response ? QString(response) : "Unknown error";
}

bool Ftp::reconnect(){

    // 关闭旧连接
    if (ftp) {
        ftp->Quit();
        delete ftp;
        ftp = nullptr;
    }

    if (isConnecting) {
        qWarningTime() << "已在重连中，跳过";
        return false;
    }

    if (currentRetryCount >= maxRetryCount) {
        qCriticalTime() << "已达到最大重连次数: " << maxRetryCount;
        return false;
    }

    if (savedFtpData.host.isEmpty() || savedFtpData.user.isEmpty()) {
        qCriticalTime() << "无保存的连接信息";
        return false;
    }

    isConnecting = true;
    currentRetryCount++;

    qInfoTime() << "尝试第 " << currentRetryCount << " 次重连...";

    // 创建新连接
    ftp = new ftplib();
    bool success = this->login(savedFtpData);

    isConnecting = false;

    if(success){
        qInfoTime() << "重连成功";
        currentRetryCount = 0;  // 重置重试计数
        return true;
    } else {
        qCriticalTime() << "重连失败";
        return false;
    }
}

bool Ftp::setSSL(bool enable){
    // 只能在连接前设置
    if(m_connected){
        qWarningTime() << "Ftp::setSSL() -> Cannot change SSL setting while connected";
        return false;
    }

    m_useSSL = enable;
    savedFtpData.useSSL = enable;
    qInfoTime() << "Ftp::setSSL() -> SSL set to" << (enable ? "enabled" : "disabled");
    return true;
}







