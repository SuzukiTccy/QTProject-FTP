#include "XFtpSTOR.h"
#include "testUtil.h"
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <iostream>
#include <string>

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
    
    // 重置超时计时器（重要！每次读取数据后重置超时）
    timeval read_timeout = {60, 0};  // 60秒读取超时
    bufferevent_set_timeouts(bev, nullptr, &read_timeout);
    
    // 循环读取数据直到缓冲区为空或达到最大限制
    while(true) {
        // 从数据连接读取数据
        int len = bufferevent_read(bev, buf, sizeof(buf));
        
        if(len == 0){
            // 没有更多数据可读（非错误）
            Logger::debug("XFtpSTOR::Read() -> No more data available now");
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
        
        // 重要：对于大文件，不要每次都flush，影响性能
        // 每接收1MB数据flush一次
        if(bytes_received % (1024*1024) == 0) {
            fflush(fp);
            Logger::debug("XFtpSTOR::Read() -> Flushed at ", bytes_received, " bytes");
        }
        
        // 如果读取的数据少于缓冲区大小，说明本次读取已完成
        if(len < (int)sizeof(buf)) {
            Logger::debug("XFtpSTOR::Read() -> Partial read (", len, " < ", sizeof(buf), "), breaking loop");
            break;
        }
        
        // 检查是否应该继续读取（避免长时间占用CPU）
        struct evbuffer* input = bufferevent_get_input(bev);
        if(!input) break;
        
        size_t pending = evbuffer_get_length(input);
        if(pending < 1024) {  // 如果缓冲区数据很少，跳出循环等待下一次回调
            Logger::debug("XFtpSTOR::Read() -> Only ", pending, " bytes pending, breaking loop");
            break;
        }
    }
    
    // 检查是否还有待处理的数据
    struct evbuffer* input = bufferevent_get_input(bev);
    if(input) {
        size_t pending = evbuffer_get_length(input);
        if(pending > 0) {
            Logger::debug("XFtpSTOR::Read() -> Still ", pending, " bytes pending, will continue");
            
            // 如果还有数据，立即再次触发读事件
            // 这对于大文件传输很重要
            if(pending > 1024 * 1024) {  // 如果还有超过1MB数据
                bufferevent_trigger(bev, EV_READ, 0);
            }
        } else {
            Logger::debug("XFtpSTOR::Read() -> Input buffer is empty");
        }
    }
}



void XFtpSTOR::Event(bufferevent* bev, short events) {
    Logger::debug("XFtpSTOR::Event() events: " + std::to_string(events));
    
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
        
        // 重置超时计时器，开始传输
        timeval transfer_timeout = {300, 0};  // 300秒传输超时
        bufferevent_set_timeouts(bev, nullptr, &transfer_timeout);
        
        #ifndef OPENSSL_NO_SSL_INCLUDES
        if (cmdTask && cmdTask->use_ssl) {
            SSL* ssl = bufferevent_openssl_get_ssl(bev);
            if (ssl && SSL_is_init_finished(ssl)) {
                Logger::info("XFtpSTOR::Event() -> SSL handshake completed, ready to receive");
                // 立即触发读事件开始接收数据
                bufferevent_trigger(bev, EV_READ, 0);
            } else {
                Logger::info("XFtpSTOR::Event() -> SSL handshake in progress, waiting...");
                waiting_for_data = true;
                // 等待SSL握手完成事件
            }
        } else {
            // 非SSL连接，可以立即开始接收数据
            Logger::info("XFtpSTOR::Event() -> Non-SSL connection, starting receive");
            bufferevent_trigger(bev, EV_READ, 0);
        }
        #else
        Logger::info("XFtpSTOR::Event() -> Starting to receive data");
        bufferevent_trigger(bev, EV_READ, 0);
        #endif
        
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
                long current_pos = ftell(fp);
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
        
        ClosePORT();
        Logger::info("XFtpSTOR::Event() close connection");
        return;
    }
    else if (events & BEV_EVENT_ERROR) {
        Logger::error("XFtpSTOR::Event() BEV_EVENT_ERROR");
        
        // 获取错误信息
        int err = EVUTIL_SOCKET_ERROR();
        std::string err_str = evutil_socket_error_to_string(err);
        Logger::error("XFtpSTOR::Event() -> Socket error: " + err_str);
        
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
        
        // 真正的错误，关闭连接
        if(!transfer_complete && !file_write_error) {
            ResCMD("426 Connection error; transfer aborted.\r\n");
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
    
    // 3. 以二进制写模式打开文件
    fp = fopen(path.c_str(), "wb");  // 二进制流式传输
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

    // 4. 发送响应及建立数据连接
    Logger::info("XFtpSTOR::Parse() -> Ready to receive file upload");
    ResCMD("150 Opening data connection for file transfer.\r\n");
    
    // 建立数据连接
    ConnectoPORT();
    
    // 重要：立即设置一个定时器，防止连接建立后没有数据
    if(cmdTask && cmdTask->base && bev) {
        // 设置一个初始触发，确保读事件开始
        timeval immediate = {0, 50000};  // 50毫秒后触发
        event* trigger_event = event_new(cmdTask->base, -1, 0, [](evutil_socket_t fd, short event, void* arg){
            bufferevent* bev = (bufferevent*)arg;
            Logger::debug("XFtpSTOR -> Triggering initial read event");
            bufferevent_trigger(bev, EV_READ, 0);
        }, bev);
        event_add(trigger_event, &immediate);
    }
}