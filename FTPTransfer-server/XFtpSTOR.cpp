#include "XFtpSTOR.h"
#include "testUtil.h"
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <iostream>
#include <string>
#include <sys/stat.h>               // for stat()

// OpenSSL相关头文件
#ifndef OPENSSL_NO_SSL_INCLUDES
#include <openssl/ssl.h>               // SSL核心库
#include <openssl/err.h>               // SSL错误处理库
#include <event2/bufferevent_ssl.h>    // libevent与OpenSSL集成的缓冲事件
extern SSL_CTX *ssl_ctx;               // 声明外部SSL上下文变量
#endif

using namespace std;

void XFtpSTOR::Read(bufferevent *bev){
    Logger::debug("XFtpSTOR::Read() called, transfer_started=", transfer_started,
                  ", transfer_complete=", transfer_complete);
    
    // 增加安全检查
    if(!bev) {
        Logger::error("XFtpSTOR::Read() -> bev is null");
        return;
    }
    
    // 如果传输已完成，直接返回
    if(transfer_complete) {
        Logger::debug("XFtpSTOR::Read() -> Transfer already complete, ignoring");
        return;
    }
    
    // 1. 检查文件指针是否有效
    if(!fp){
        Logger::error("XFtpSTOR::Read() fp is null");
        ResCMD("550 Cannot create file.\r\n");
        ClosePORT();
        return;
    }

    #ifndef OPENSSL_NO_SSL_INCLUDES
    // 检查是否需要 SSL 握手
    if(cmdTask && cmdTask->use_ssl){
        SSL *ssl = bufferevent_openssl_get_ssl(bev);
        if(ssl && SSL_is_init_finished(ssl)){
            // SSL 连接已建立，可以读取数据
        } else {
            Logger::debug("XFtpSTOR::Read() -> SSL not ready, waiting for handshake");
            waiting_for_data = true;
            return;
        }
    }
    #endif

    // 标记传输已开始
    if(!transfer_started) {
        Logger::info("XFtpSTOR::Read() -> Starting file upload transfer");
        transfer_started = true;
        waiting_for_data = false;
    }
    
    bool data_was_read = false;
    
    // 循环读取直到缓冲区为空
    do {
        // 获取输入缓冲区大小
        struct evbuffer* input = bufferevent_get_input(bev);
        if(!input) {
            Logger::debug("XFtpSTOR::Read() -> No input buffer");
            break;
        }
        
        size_t available = evbuffer_get_length(input);
        if(available == 0) {
            Logger::debug("XFtpSTOR::Read() -> No data available");
            break;
        }
        
        // 计算本次读取的大小（不超过缓冲区大小）
        size_t to_read = std::min(available, sizeof(buf));
        Logger::debug("XFtpSTOR::Read() -> ", available, " bytes available, reading ", to_read);
        
        // 从数据连接读取数据
        int len = bufferevent_read(bev, buf, to_read);
        
        if(len == 0){
            // 没有更多数据可读（非错误）
            Logger::debug("XFtpSTOR::Read() -> bufferevent_read returned 0");
            break;
        }
        
        if(len < 0){
            // 读取错误
            Logger::error("XFtpSTOR::Read() -> bufferevent_read error");
            file_write_error = true;
            ResCMD("426 Connection closed; transfer aborted.\r\n");
            ClosePORT();
            return;
        }
        
        data_was_read = true;
        
        // 更新接收字节数
        bytes_received += len;
        Logger::info("XFtpSTOR::Read() -> Received ", len, " bytes, total: ", bytes_received);
        
        // 将数据写入文件
        size_t written = fwrite(buf, 1, len, fp);
        if(written != (size_t)len){
            // 写入错误
            int err = ferror(fp);
            Logger::error("XFtpSTOR::Read() -> fwrite error: ", err, 
                         ", expected ", len, " bytes, wrote ", written);
            file_write_error = true;
            ResCMD("552 Storage allocation exceeded or disk full.\r\n");
            ClosePORT();
            return;
        }
        
        // 每接收1MB数据flush一次
        if(bytes_received % (1024*1024) == 0) {
            fflush(fp);
            Logger::debug("XFtpSTOR::Read() -> Flushed at ", bytes_received, " bytes");
        }
        
    } while(false); // 只循环一次，避免阻塞事件循环
    
    // 如果读取了数据，立即检查是否还有更多数据
    if(data_was_read) {
        // 使用weak_ptr避免访问已释放的内存
        auto weak_bev = std::weak_ptr<bufferevent>(); // 需要先将bev包装为shared_ptr
        // 简化：直接再次触发读事件
        bufferevent_trigger(bev, EV_READ, 0);
    }
}



void XFtpSTOR::Event(bufferevent* bev, short events) {
    Logger::debug("XFtpSTOR::Event() events: " + std::to_string(events));
    
    // 安全检查
    if(!bev) {
        Logger::error("XFtpSTOR::Event() -> bev is null");
        return;
    }
    
    #ifndef OPENSSL_NO_SSL_INCLUDES
    // 检查是否是SSL握手完成事件
    if (events & 0x4000) { // libevent的BEV_EVENT_SSL_READY标志
        Logger::info("XFtpSTOR::Event() -> SSL handshake completed on data connection");
        
        // 检查SSL状态
        if(cmdTask && cmdTask->use_ssl){
            SSL *ssl = bufferevent_openssl_get_ssl(bev);
            if(ssl && SSL_is_init_finished(ssl)){
                Logger::info("XFtpSTOR::Event() -> SSL ready, starting data reception");
                // 如果之前等待数据，现在可以开始读取了
                if(waiting_for_data) {
                    waiting_for_data = false;
                    // 触发读事件开始接收数据
                    bufferevent_trigger(bev, EV_READ, 0);
                }
            }
        }
        return;
    }
    #endif
    
    if (events & BEV_EVENT_CONNECTED) {
        Logger::info("XFtpSTOR::Event() BEV_EVENT_CONNECTED");
        
        #ifndef OPENSSL_NO_SSL_INCLUDES
        if (cmdTask && cmdTask->use_ssl) {
            SSL* ssl = bufferevent_openssl_get_ssl(bev);
            if (ssl) {
                if (SSL_is_init_finished(ssl)) {
                    Logger::info("XFtpSTOR::Event() -> SSL handshake already completed");
                } else {
                    Logger::info("XFtpSTOR::Event() -> SSL handshake in progress, waiting...");
                    // 等待SSL握手完成事件
                    return;
                }
            }
        }
        #endif
        
        // 连接建立成功，设置传输阶段的超时
        // 上传：主要关注读超时（接收数据）
        timeval read_timeout = {300, 0};    // 接收数据超时300秒
        timeval write_timeout = {30, 0};    // 发送响应超时30秒
        bufferevent_set_timeouts(bev, &read_timeout, &write_timeout);
        
        Logger::info("XFtpSTOR::Event() -> Ready to receive data");
        
        // 立即触发读事件开始接收数据
        bufferevent_trigger(bev, EV_READ, 0);
        
        return;
    }
    else if (events & BEV_EVENT_EOF) {
        Logger::info("XFtpSTOR::Event() BEV_EVENT_EOF");
        
        // 客户端关闭了连接，上传完成
        if(!transfer_complete && !file_write_error) {
            Logger::info("XFtpSTOR::Event() -> Client closed connection, upload complete");
            
            // 确保所有数据都已写入文件
            if(fp) {
                fflush(fp);
                
                // 获取文件大小验证
                fseek(fp, 0, SEEK_END);        // 移动到文件末尾
                long current_pos = ftell(fp);  // 当前文件大小
                Logger::info("XFtpSTOR::Event() -> Final file size: ", current_pos, 
                            " bytes, total received: ", bytes_received);
                
                // 检查文件大小是否合理
                if(current_pos != bytes_received) {
                    Logger::warning("XFtpSTOR::Event() -> File size mismatch! File: ", 
                                   current_pos, ", Received: ", bytes_received);
                }
            }
            
            // 发送成功响应
            ResCMD("226 Transfer complete.\r\n");
            transfer_complete = true;
        } else if(file_write_error) {
            ResCMD("550 File write error.\r\n");
        }

        // 传输完成，重置偏移量
        if (cmdTask) {
            cmdTask->SetFileOffset(0);
            Logger::debug("XFtpRETR::Event() -> Reset file offset to 0");
        }
        
        ClosePORT();
        Logger::info("XFtpSTOR::Event() close connection");
        return;
    }
    else if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
        // 同时发生错误和EOF的情况
        Logger::info("XFtpSTOR::Event() -> Connection closed with possible error");
        
        if(!transfer_complete && !file_write_error) {
            // 检查是否已接收部分数据
            if(bytes_received > 0) {
                Logger::info("XFtpSTOR::Event() -> Partial upload received: ", bytes_received, " bytes");
                
                // 保存已接收的数据
                if(fp) {
                    fflush(fp);
                    ResCMD("226 Partial transfer complete.\r\n");
                }
            }
            transfer_complete = true;
        }
        
        ClosePORT();
        return;
    }
    else if (events & BEV_EVENT_ERROR) {
        Logger::error("XFtpSTOR::Event() BEV_EVENT_ERROR");
        
        // 获取错误信息
        int err = EVUTIL_SOCKET_ERROR();
        std::string err_str = evutil_socket_error_to_string(err);
        Logger::error("XFtpSTOR::Event() -> Socket error: ", err_str);
        
        #ifndef OPENSSL_NO_SSL_INCLUDES
        // 检查SSL错误
        if(cmdTask && cmdTask->use_ssl && bev){
            SSL* ssl = bufferevent_openssl_get_ssl(bev);
            if(ssl){
                unsigned long ssl_err = ERR_get_error();
                if(ssl_err){
                    char err_buf[256];
                    ERR_error_string_n(ssl_err, err_buf, sizeof(err_buf));
                    Logger::error("XFtpSTOR::Event() -> SSL error: ", err_buf);
                }
            }
        }
        #endif
        
        // 检查是否是临时错误
        if (err == EINPROGRESS || err == EAGAIN) {
            Logger::info("XFtpSTOR::Event() -> Temporary error, waiting...");
            return; // 立即返回，不关闭连接
        }
        
        // 真正的错误，检查是否已接收部分数据
        if(!transfer_complete && !file_write_error) {
            if(bytes_received > 0) {
                Logger::warning("XFtpSTOR::Event() -> Connection error after receiving ", 
                               bytes_received, " bytes");
                ResCMD("426 Connection error; transfer aborted.\r\n");
            } else {
                ResCMD("425 Can't open data connection.\r\n");
            }
        }
        ClosePORT();
        return;
    }
    else if (events & BEV_EVENT_TIMEOUT) {
        Logger::info("XFtpSTOR::Event() BEV_EVENT_TIMEOUT");
        
        if(!transfer_complete && !file_write_error) {
            // 检查是否收到了任何数据
            if(bytes_received > 0) {
                Logger::warning("XFtpSTOR::Event() -> Timeout after receiving ", 
                               bytes_received, " bytes");
                
                // 尝试保存已接收的数据
                if(fp) {
                    fflush(fp);
                    Logger::info("XFtpSTOR::Event() -> Saved ", bytes_received, " bytes before timeout");
                }
                
                ResCMD("426 Transfer timeout; connection closed.\r\n");
            } else {
                ResCMD("421 Service not available, closing control connection.\r\n");
            }
        }
        ClosePORT();
        Logger::info("XFtpSTOR::Event() close connection");
        return;
    }
    // 其他事件暂时忽略
}



void XFtpSTOR::Parse(string cmd, string msg){
    Logger::debug("XFtpSTOR::Parse() cmd: ", cmd, " msg: ", msg);
    
    // 重置传输状态
    ResetTransferState();
    
    // 1. 提取文件名, 去除文件名末尾的\r\n
    int pos = msg.rfind(" ") + 1;
    string filename = msg.substr(pos);
    while(!filename.empty() && (filename.back() == '\r' || filename.back() == '\n')){
        filename.pop_back();
    }
    
    // 2. 构建完整文件路径
    string path = cmdTask->rootDir + cmdTask->curDir + filename;
    Logger::info("XFtpSTOR::Parse() path: ", path);

    // 3. 获取偏移量
    off_t offset = cmdTask->GetFileOffset();
    Logger::info("XFtpSTOR::Parse() -> Starting upload with offset: ", offset);

    // 4. 检查文件是否存在及其大小
    struct stat fileStat;
    bool fileExists = (stat(path.c_str(), &fileStat) == 0);
    off_t existingSize = fileExists ? fileStat.st_size : 0;
    if(fileExists){
        if(offset != existingSize){
            Logger::warning("XFtpSTOR::Parse() -> Offset ", offset, \
                " does not match existing file size ", existingSize, \
                " for file", path, \
                ". Rejecting upload.");
            // 根据RFC 959，如果偏移量大于文件大小，应该返回错误
            ResCMD("554 Requested offset exceeds file size.\r\n");
            return;
        } else {
            Logger::info("XFtpSTOR::Parse() -> Existing file size: ", existingSize, " bytes");
        }
    }
    
    // 5. 以二进制写模式打开文件
    if (offset == 0) {
        // 从头开始，创建新文件或覆盖
        fp = fopen(path.c_str(), "wb");  // 二进制写入模式
    } else {
        // 续传，以读写模式打开
        fp = fopen(path.c_str(), "rb+");  // 二进制读写模式
        
        if (fp) {
            // 移动到偏移位置
            if (fseeko(fp, offset, SEEK_SET) != 0) {
                Logger::error("XFtpSTOR::Parse() -> fseeko64 failed, offset: ", offset, " error: ", strerror(errno));
                ResCMD("550 Cannot seek to specified offset.\r\n");
                fclose(fp);
                fp = nullptr;
                return;
            }
            bytes_received = offset; // 更新已接收字节数为偏移量
            Logger::info("XFtpSTOR::Parse() -> File pointer moved to offset: ", offset);
        } else {
            // 如果文件不存在但offset>0，返回错误
            if (offset > 0) {
                Logger::error("XFtpSTOR::Parse() -> File does not exist for resume, offset: ", offset);
                ResCMD("550 File does not exist for resume.\r\n");
                return;
            }
        }
    }
    if(!fp){
        int err = errno;
        string error_msg;
        
        if(err == EACCES || err == EPERM){
            error_msg = "550 Permission denied.\r\n";
        } else if(err == ENOENT){
            // 尝试创建目录
            error_msg = "550 Directory does not exist.\r\n";
        } else if(err == ENOSPC){
            error_msg = "552 Storage allocation exceeded.\r\n";
        } else {
            error_msg = "550 Cannot create file.\r\n";
        }
        Logger::error("XFtpSTOR::Parse() fopen failed: ", strerror(err));
        ResCMD(error_msg);
        return;
    }

    // 6. 发送响应及建立数据连接
    Logger::info("XFtpSTOR::Parse() -> Ready to receive file upload");
    ResCMD("150 Opening data connection for file transfer.\r\n");
    
    // 建立数据连接
    ConnectoPORT();
}