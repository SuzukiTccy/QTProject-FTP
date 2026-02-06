#include "ftp.h"
#include <mainwindow.h>
#include <QFile>
#include <QDebug>
#include <QFileInfo>
#include <QSslSocket>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QCryptographicHash>
#include <QDir>
#include <QTimer>

Ftp::Ftp(QObject* parent) :
    QObject(parent)
{
    ftp = new ftplib();
    ftp->SetConnmode(ftplib::port);
}

Ftp::~Ftp()
{
    qInfoTime() << "Ftp::~Ftp() called";
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

    // 检查是否支持断点续传
    bool resumeSupported = checkResumeSupported(remote_file);

    if (resumeSupported) {
        // 使用断点续传功能
        return putResume(put_file, remote_file, 0);
    }

    // 使用普通上传
    qInfoTime() << "Ftp::put() -> Server does not support resume, using normal upload";
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

    // 检查是否支持断点续传
    bool resumeSupported = checkResumeSupported(remote_file);

    if (resumeSupported) {
        // 使用断点续传功能
        return getResume(local_file, remote_file, 0);
    }

    // 使用普通下载
    qInfoTime() << "Ftp::get() -> Server does not support resume, using normal download";
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


// 断点续传相关

qint64 Ftp::getRemoteFileSize(const QString& remote_file){
    if (!ftp) {
        qCriticalTime() << "Ftp::getRemoteFileSize() -> Not connected";
        return -1;
    }

    int size = 0;
    if (!ftp->Size(remote_file.toStdString().c_str(), &size, ftplib::image)) {
        qWarningTime() << "Ftp::getRemoteFileSize() -> Failed to get size for" << remote_file
                       << "Error:" << error();
        return -1;
    }

    return static_cast<qint64>(size);
}

bool Ftp::checkResumeSupported(const QString& remote_file){

    // 尝试获取文件大小来检查服务器是否支持SIZE命令
    return getRemoteFileSize(remote_file) >= 0;
}

bool Ftp::getResume(const QString& local_file, const QString& remote_file, qint64 offset){
    qInfoTime() << "Ftp::getResume() -> Downloading" << remote_file << "to" << local_file;
    if (!ftp) {
        qCriticalTime() << "Ftp::getResume() -> Not connected";
        emit errorOccurred("Not connected");
        return false;
    }

    // 生成传输ID
    QString transferId = generateTransferId();

    // 获取远程文件总大小
    qint64 totalSize = getRemoteFileSize(remote_file);
    if(totalSize < 0){
        qCriticalTime() << "Ftp::getResume() -> Cannot get remote file size";
        emit errorOccurred("Cannot get remote file size");
        return false;
    }

    // 检查本地文件状态
    QFile localFile(local_file);
    qint64 localSize = 0;

    if(localFile.exists()){
        if(offset == 0){
            // 如果offset为0，使用本地文件大小作为offset
            offset = localFile.size();
            qInfoTime() << "Ftp::getResume() -> Using local file size as offset:" << offset;
        }

        // 验证本地文件, 如果offset > 0, 且offset > local_file的大小
        if(offset > 0 && !validateFileForResume(local_file, offset)){
            qWarningTime() << "Ftp::getResume() -> Local file validation failed, starting from beginning";
            offset = 0;
        }
    }

    // 创建传输信息
    FTP_TRANSFER_INFO transferInfo;
    transferInfo.transferId = transferId;
    transferInfo.localPath = local_file;
    transferInfo.remotePath = remote_file;
    transferInfo.fileSize = totalSize;
    transferInfo.transferred = offset;
    transferInfo.offset = offset;
    transferInfo.isUpload = false;
    transferInfo.isResumeSupported = true;
    transferInfo.startTime = QDateTime::currentDateTime();
    transferInfo.lastUpdate = QDateTime::currentDateTime();
    transferInfo.status = "transferring";

    // 保存传输状态
    saveTransferState(transferInfo);

    // 添加活动传输列表
    {
        QMutexLocker locker(&transfersMutex);
        activeTransfers[transferId] = transferInfo;
    }

    emit transferStarted(transferId, transferInfo);

    // 执行带偏移的下载
    qInfoTime() << "Ftp::getResume() -> Starting download with offset" << offset;

    bool success = ftp->Get(local_file.toStdString().c_str(),
                            remote_file.toStdString().c_str(),
                            ftplib::image,
                            static_cast<off64_t>(offset));

    if(success){
        qInfoTime() << "Ftp::getResume() -> Download completed successfully";
        transferInfo.status = "completed";
        transferInfo.transferred = totalSize;
        transferInfo.lastUpdate = QDateTime::currentDateTime();

        // 更新状态
        {
            QMutexLocker locker(&transfersMutex);
            activeTransfers[transferId] = transferInfo;
        }

        saveTransferState(transferInfo);
        emit transferCompleted(transferId);

        // 清理完成的状态
        QTimer::singleShot(5000, this, [this, transferId](){
            removeTransferState(transferId);
            {
                QMutexLocker locker(&transfersMutex);
                activeTransfers.remove(transferId);
            }
        });

    }else{
        qCriticalTime() << "Ftp::getResume() -> Download failed:" << error();

        transferInfo.status = "failed";
        transferInfo.lastUpdate = QDateTime::currentDateTime();

        // 更新状态
        {
            QMutexLocker locker(&transfersMutex);
            activeTransfers[transferId] = transferInfo;
        }

        saveTransferState(transferInfo);
        emit transferFailed(transferId, error());
    }
    return success;

}


bool Ftp::putResume(const QString& local_file, const QString& remote_file, qint64 offset){
    qInfoTime() << "Ftp::putResume() -> Uploading" << local_file << "to" << remote_file;

    if (!ftp) {
        qCriticalTime() << "Ftp::putResume() -> Not connected";
        emit errorOccurred("Not connected");
        return false;
    }

    // 检查本地文件
    QFile localFile(local_file);
    if (!localFile.exists()) {
        qCriticalTime() << "Ftp::putResume() -> Local file does not exist:" << local_file;
        emit errorOccurred("Local file does not exist");
        return false;
    }

    qint64 totalSize = localFile.size();
    if (totalSize <= 0) {
        qCriticalTime() << "Ftp::putResume() -> Local file is empty";
        emit errorOccurred("Local file is empty");
        return false;
    }

    // 如果offset = 0, 检查远程文件大小
    if(offset == 0){
        qint64 remoteSize = getRemoteFileSize(remote_file);
        if(remoteSize > 0){
            offset = remoteSize;
            qInfoTime() << "Ftp::putResume() -> Remote file exists, using size as offset:" << offset;
        }
    }

    // 验证偏移量
    if(offset >= totalSize){
        qWarningTime() << "Ftp::putResume() -> Offset >= file size, file may already be complete";

        // 检查文件是否完整
        if(offset == totalSize){
            qInfoTime() << "Ftp::putResume() -> File already fully uploaded";
            return true;
        }else{
            qCriticalTime() << "Ftp::putResume() -> Invalid offset, starting from beginning";
            offset = 0;
        }
    }

    // 生成传输ID
    QString transferId = generateTransferId();

    // 创建传输信息
    FTP_TRANSFER_INFO transferInfo;
    transferInfo.transferId = transferId;
    transferInfo.localPath = local_file;
    transferInfo.remotePath = remote_file;
    transferInfo.fileSize = totalSize;
    transferInfo.transferred = offset;
    transferInfo.offset = offset;
    transferInfo.isUpload = true;
    transferInfo.isResumeSupported = checkResumeSupported(remote_file);
    transferInfo.startTime = QDateTime::currentDateTime();
    transferInfo.lastUpdate = QDateTime::currentDateTime();
    transferInfo.status = "transferring";

    // 保存传输状态
    saveTransferState(transferInfo);

    // 添加到活动传输列表
    {
        QMutexLocker locker(&transfersMutex);
        activeTransfers[transferId] = transferInfo;
    }

    emit transferStarted(transferId, transferInfo);

    // 执行带偏移的上传
    qInfoTime() << "Ftp::putResume() -> Starting upload with offset" << offset;

    bool success = ftp->Put(local_file.toStdString().c_str(),
                            remote_file.toStdString().c_str(),
                            ftplib::image,
                            static_cast<off64_t>(offset));

    if (success) {
        qInfoTime() << "Ftp::putResume() -> Upload completed successfully";
        transferInfo.status = "completed";
        transferInfo.transferred = totalSize;
        transferInfo.lastUpdate = QDateTime::currentDateTime();

        // 更新状态
        {
            QMutexLocker locker(&transfersMutex);
            activeTransfers[transferId] = transferInfo;
        }

        saveTransferState(transferInfo);
        emit transferCompleted(transferId);

        // 清理完成的状态
        QTimer::singleShot(5000, this, [this, transferId]() {
            removeTransferState(transferId);
            {
                QMutexLocker locker(&transfersMutex);
                activeTransfers.remove(transferId);
            }
        });

    } else {
        qCriticalTime() << "Ftp::putResume() -> Upload failed:" << error();

        transferInfo.status = "failed";
        transferInfo.lastUpdate = QDateTime::currentDateTime();

        // 更新状态
        {
            QMutexLocker locker(&transfersMutex);
            activeTransfers[transferId] = transferInfo;
        }
        saveTransferState(transferInfo);
        emit transferFailed(transferId, error());
    }

    return success;
}


bool Ftp::pauseTransfer(const QString& transferId){
    QMutexLocker locker(&transfersMutex);

    if(!activeTransfers.contains(transferId)){
        qWarningTime() << "Ftp::pauseTransfer() -> Transfer not found:" << transferId;
        return false;
    }

    FTP_TRANSFER_INFO& info = activeTransfers[transferId];
    info.status = "paused";
    info.lastUpdate = QDateTime::currentDateTime();

    // 保持状态
    saveTransferState(info);

    qInfoTime() << "Ftp::pauseTransfer() -> Transfer paused:" << transferId;
    emit transferPaused(transferId, info.transferred);

    // 注意：实际暂停传输需要中断FTP命令，这需要更复杂的实现
    // 目前我们只是标记状态，真正的暂停需要在传输过程中实现

    return true;
}


bool Ftp::resumeTransfer(const QString& transferId){
    QMutexLocker locker(&transfersMutex);

    if(!activeTransfers.contains(transferId)){
        // 尝试从保存的状态恢复
        FTP_TRANSFER_INFO info = loadTransferState(transferId);
        if(info.transferId.isEmpty() || info.status != "paused"){
            qWarningTime() << "Ftp::resumeTransfer() -> Transfer not found or not paused:" << transferId;
            return false;
        }
        activeTransfers[transferId] = info;
    }

    FTP_TRANSFER_INFO& info = activeTransfers[transferId];

    // 检查文件状态
    if(info.isUpload){
        // 上传续传
        QFile localFile(info.localPath);
        if(!localFile.exists()){
            qCriticalTime() << "Ftp::resumeTransfer() -> Local file not found:" << info.localPath;
            info.status = "failed";
            saveTransferState(info);
            return false;
        }

        qint64 currentSize = localFile.size();
        if(currentSize < info.transferred){
            qWarningTime() << "Ftp::resumeTransfer() -> Local file smaller than recorded transfer, adjusting offset";
            info.transferred = currentSize;
            info.offset = currentSize;
        }

        // 继续上传
        info.status = "transferring";
        info.lastUpdate = QDateTime::currentDateTime();
        saveTransferState(info);

        // 在下一事件循环中执行续传
        QTimer::singleShot(0, this, [this, info](){
            getResume(info.localPath, info.remotePath, info.offset);
        });

    }
    emit transferResumed(transferId, info.offset);
    return true;
}


bool Ftp::cancelTransfer(const QString& transferId){
    QMutexLocker locker(&transfersMutex);

    if (!activeTransfers.contains(transferId)) {
        qWarningTime() << "Ftp::cancelTransfer() -> Transfer not found:" << transferId;
        return false;
    }

    // 更新状态
    FTP_TRANSFER_INFO& info = activeTransfers[transferId];
    info.status = "cancelled";
    info.lastUpdate = QDateTime::currentDateTime();
    saveTransferState(info);

    // 从活动列表移除
    activeTransfers.remove(transferId);

    // 删除状态文件
    removeTransferState(transferId);

    qInfoTime() << "Ftp::cancelTransfer() -> Transfer cancelled:" << transferId;

    // 注意：实际取消传输需要中断FTP命令
    // 这需要更复杂的实现，目前我们只是标记状态

    return true;
}

QList<FTP_TRANSFER_INFO> Ftp::getActiveTransfers() const {
    QMutexLocker locker(&transfersMutex);

    QList<FTP_TRANSFER_INFO> transfers;
    for(const auto& info : activeTransfers){
        transfers.append(info);
    }

    // 也从保存的文件中加载暂停的传输
    QFile stateFile(transfersStateFile);
    if(stateFile.exists() && stateFile.open(QIODevice::ReadOnly)){
        QJsonDocument doc = QJsonDocument::fromJson(stateFile.readAll());
        stateFile.close();

        if(doc.isObject()){
            QJsonObject root = doc.object();
            QJsonArray transfersArray = root["transfers"].toArray();

            for(const QJsonValue& value : transfersArray){
                FTP_TRANSFER_INFO info = FTP_TRANSFER_INFO::fromJson(value.toObject());
                if(info.status == "paused" && !activeTransfers.contains(info.transferId)){
                    transfers.append(info);
                }
            }
        }
    }
    return transfers;
}

// 辅助方法实现

void Ftp::saveTransferState(const FTP_TRANSFER_INFO& info){
    QMutexLocker locker(&transfersMutex);

    QJsonDocument doc;
    QJsonObject root;
    QJsonArray transfersArray;

    // 读取现有状态
    QFile stateFile(transfersStateFile);
    if (stateFile.exists() && stateFile.open(QIODevice::ReadOnly)) {
        doc = QJsonDocument::fromJson(stateFile.readAll());
        stateFile.close();

        if (doc.isObject()) {
            root = doc.object();
            transfersArray = root["transfers"].toArray();
        }
    }

    // 更新或添加传输状态
    bool found = false;
    for(int i = 0; i < transfersArray.size(); ++i){
        QJsonObject obj = transfersArray[i].toObject();
        if(obj["transferId"].toString() == info.transferId){
            transfersArray[i] = info.toJson();
            found = true;
            break;
        }
    }

    if(!found){
        transfersArray.append(info.toJson());
    }

    root["transfers"] = transfersArray;
    doc = QJsonDocument(root);

    // 保存到文件
    if(stateFile.open(QIODevice::WriteOnly | QIODevice::Truncate)){        // QIODevice::Truncate：清空/截断模式
        stateFile.write(doc.toJson());
        stateFile.close();
        qInfoTime() << "Ftp::saveTransferState() -> State saved for transfer:" << info.transferId;
    } else {
        qWarningTime() << "Ftp::saveTransferState() -> Failed to save state file";
    }
}


FTP_TRANSFER_INFO Ftp::loadTransferState(const QString& transferId){
    QMutexLocker locker(&transfersMutex);

    FTP_TRANSFER_INFO info;

    QFile stateFile(transfersStateFile);
    if (!stateFile.exists()) {
        qWarningTime() << "Ftp::loadTransferState() -> transfersStateFile is not exist :" << transferId;
        return info;
    }

    if(!stateFile.open(QIODevice::ReadOnly)){
        qWarningTime() << "Ftp::loadTransferState() -> Failed to open state file";
        return info;
    }

    QJsonDocument doc = QJsonDocument::fromJson(stateFile.readAll());
    stateFile.close();

    if (!doc.isObject()) {
        qWarningTime() << "Ftp::loadTransferState() -> !doc.isObject()";
        return info;
    }

    QJsonObject root = doc.object();
    QJsonArray transfersArray = root["transfers"].toArray();

    for (const QJsonValue& value : transfersArray) {
        QJsonObject obj = value.toObject();
        if (obj["transferId"].toString() == transferId) {
            info = FTP_TRANSFER_INFO::fromJson(obj);
            break;
        }
    }

    return info;

}


void Ftp::removeTransferState(const QString& transferId){
    QMutexLocker locker(&transfersMutex);

    QJsonDocument doc;
    QJsonObject root;
    QJsonArray transfersArray;

    // 读取现有状态
    QFile stateFile(transfersStateFile);
    if (stateFile.exists() && stateFile.open(QIODevice::ReadOnly)) {
        doc = QJsonDocument::fromJson(stateFile.readAll());
        stateFile.close();

        if (doc.isObject()) {
            root = doc.object();
            transfersArray = root["transfers"].toArray();
        }
    } else {
        qWarningTime() << "Ftp::removeTransferState() -> transfersStateFile is not exist or cant read";
        return;
    }

    // 删除指定传输
    QJsonArray newArray;
    for (const QJsonValue& value : transfersArray) {
        QJsonObject obj = value.toObject();
        if (obj["transferId"].toString() != transferId) {
            newArray.append(obj);
        }
    }

    root["transfers"] = newArray;
    doc.setObject(root);

    // 保存到文件
    if (stateFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        stateFile.write(doc.toJson());
        stateFile.close();
        qInfoTime() << "Ftp::removeTransferState() -> State removed for transfer:" << transferId;
    }
}


QString Ftp::generateTransferId() const{
    // 使用时间戳和随机数生成唯一ID
    QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch(), 16);
    QString random = QString::number(QRandomGenerator::global()->generate(), 16);

    // 使用MD5哈希确保唯一性
    QString input = timestamp + random + QString::number(QRandomGenerator::global()->generate());
    QByteArray hash = QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Md5);

    return hash.toHex().left(16); // 取前16个字符
}

qint64 Ftp::getLocalFileSize(const QString& filePath){
    QFile file(filePath);
    if(file.exists()){
        return file.size();
    }
    qWarningTime() << "Ftp::getLocalFileSize() -> filePath : " << filePath << "is not exist";
    return 0;
}

bool Ftp::validateFileForResume(const QString& filePath, qint64 expectedSize){
    QFile file(filePath);
    if(!file.exists()){
        qWarningTime() << "Ftp::validateFileForResume -> filePath : " << filePath << "is not exist";
        return false;
    }

    qint64 actualSize = file.size();

    // 简单验证：实际大小应该等于或者小于期望大小
    // 在实际应用中，可能需要更复杂的校验（如校验和）
    if(actualSize <= expectedSize){
        return true;
    }

    qWarningTime() << "Ftp::validateFileForResume() -> File size mismatch. Actual:"
                   << actualSize << "Expected:" << expectedSize;
    return false;
}


























