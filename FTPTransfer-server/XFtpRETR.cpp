#include "XFtpRETR.h"
#include "testUtil.h"
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/event.h>
#include <iostream>
#include <string>

// OpenSSL相关头文件
#ifndef OPENSSL_NO_SSL_INCLUDES
#include <openssl/ssl.h>               // SSL核心库
#include <event2/bufferevent_ssl.h>    // libevent与OpenSSL集成的缓冲事件
extern SSL_CTX *ssl_ctx;               // 声明外部SSL上下文变量
#endif

using namespace std;

void XFtpRETR::Write(bufferevent *bev){
    Logger::debug("XFtpRETR::Write() called, transfer_started=", transfer_started, 
                  ", file_eof=", file_eof, ", transfer_complete=", transfer_complete);
    
    // 如果传输已完成，直接返回
    if(transfer_complete) {
        Logger::debug("XFtpRETR::Write() -> Transfer already complete, ignoring");
        return;
    }
    
    // 1. 检查文件指针是否有效
    if(!fp){
        Logger::error("XFtpRETR::Write() fp is null");
        ResCMD("550 Cannot access file.\r\n");
        ClosePORT();
        return;
    }

    #ifndef OPENSSL_NO_SSL_INCLUDES
        // 检查是否需要 SSL 握手
        if(cmdTask && cmdTask->use_ssl){
            SSL *ssl = bufferevent_openssl_get_ssl(bev);
            if(ssl && SSL_is_init_finished(ssl)){
                // SSL 连接已建立，可以发送数据
            } else {
                Logger::debug("XFtpRETR::Write() -> SSL not ready, waiting for handshake");
                return;
            }
        }
    #endif

    // 如果传输还未开始，启动传输
    if(!transfer_started) {
        Logger::info("XFtpRETR::Write() -> Starting file transfer");
        transfer_started = true;
    }
    
    // 检查是否已经读取到文件末尾且缓冲区已空
    if(file_eof) {
        struct evbuffer* output = bufferevent_get_output(bev);
        if(output && evbuffer_get_length(output) == 0) {
            // 文件已读完且缓冲区已空，传输完成
            Logger::info("XFtpRETR::Write() -> File transfer complete");
            ResCMD("226 Transfer complete.\r\n");
            transfer_complete = true;
            ClosePORT();
        } else {
            // 缓冲区还有数据，等待下一次回调
            size_t remaining = output ? evbuffer_get_length(output) : 0;
            Logger::debug("XFtpRETR::Write() -> Waiting for buffer to flush: ", 
                         remaining, " bytes remaining");
        }
        return;
    }
    
    // 检查文件读取错误
    if(file_read_error) {
        Logger::error("XFtpRETR::Write() -> File read error occurred earlier");
        ResCMD("550 File read error.\r\n");
        ClosePORT();
        return;
    }

    // 从文件读取数据（1MB块）
    int len = fread(buf, 1, sizeof(buf), fp);
    file_pos += len;
    Logger::debug("XFtpRETR::Write() -> Read ", len, " bytes from file, total: ", file_pos);

    // 处理读取结果
    if(len == 0){
        // 文件结束
        Logger::info("XFtpRETR::Write() -> End of file reached, total bytes: ", file_pos);
        file_eof = true;
        
        // 检查错误
        if(ferror(fp)) {
            int err = ferror(fp);
            Logger::error("XFtpRETR::Write() -> File read error: ", err);
            ResCMD("550 File read error.\r\n");
            ClosePORT();
            return;
        }
        
        // 文件正常结束，现在检查缓冲区是否为空
        struct evbuffer* output = bufferevent_get_output(bev);
        if(output && evbuffer_get_length(output) == 0) {
            // 缓冲区已空，立即完成
            Logger::info("XFtpRETR::Write() -> Buffer empty, completing transfer");
            ResCMD("226 Transfer complete.\r\n");
            transfer_complete = true;
            ClosePORT();
        } else {
            // 缓冲区还有数据，等待下一次 Write() 回调
            size_t remaining = output ? evbuffer_get_length(output) : 0;
            Logger::debug("XFtpRETR::Write() -> Buffer not empty, waiting: ", 
                         remaining, " bytes remaining");
        }
        return;
    } 
    else if(len < 0){
        // 读取错误
        int err = ferror(fp);
        Logger::error("XFtpRETR::Write() -> fread failed, error: ", err);
        file_read_error = true;
        ResCMD("550 File read error.\r\n");
        ClosePORT();
        return;
    }

    // 发送数据
    Logger::info("XFtpRETR::Write() -> Sending ", len, " bytes");
    int result = Send(buf, len);
    
    if(result < 0){
        // 发送失败
        Logger::error("XFtpRETR::Write() -> Send failed, error: ", result);
        ResCMD("426 Connection closed; transfer aborted.\r\n");
        ClosePORT();
        return;
    } 
    else if(result == 0) {
        // 数据成功排队，等待缓冲区刷新
        Logger::debug("XFtpRETR::Write() -> Data queued, waiting for buffer");
        // 不立即读取下一块，等待下一次 Write() 回调
    }
    else {
        // 数据已部分或全部发送，可以立即尝试读取下一块
        Logger::debug("XFtpRETR::Write() -> ", result, " bytes sent immediately");
        // 注意：这里可以立即读取下一块，但为了简单，我们还是等待下一次回调
    }
    
    // 检查是否需要继续读取（如果缓冲区还有空间）
    struct evbuffer* output = bufferevent_get_output(bev);
    if(output) {
        size_t buffer_size = evbuffer_get_length(output);
        // 如果缓冲区很小，可以继续读取下一块
        if(buffer_size < 1024 * 512) { // 小于512KB
            Logger::debug("XFtpRETR::Write() -> Buffer has space, will continue");
            // 可以立即触发下一次写事件
            bufferevent_trigger(bev, EV_WRITE, 0);
        }
    }
}



void XFtpRETR::Event(bufferevent* bev, short events) {
    Logger::debug("XFtpRETR::Event() events: " + std::to_string(events));
    
    if (events & BEV_EVENT_CONNECTED) {
        Logger::info("XFtpRETR::Event() -> Connection established");
        
        #ifndef OPENSSL_NO_SSL_INCLUDES
        if(cmdTask && cmdTask->use_ssl) {
            SSL* ssl = bufferevent_openssl_get_ssl(bev);
            if(ssl && SSL_is_init_finished(ssl)) {
                Logger::info("XFtpRETR::Event() -> SSL ready, starting transfer");
                // 连接建立成功，设置传输阶段的超时
                // 下载：主要关注写超时（发送数据），读超时可以设置短一些
                timeval write_timeout = {300, 0};   // 发送数据超时300秒
                timeval read_timeout = {30, 0};     // 读取响应超时30秒
                bufferevent_set_timeouts(bev, &read_timeout, &write_timeout);

                bufferevent_trigger(bev, EV_WRITE, 0);
            } else {
                Logger::info("XFtpRETR::Event() -> SSL handshake in progress");
            }
        } else {
            bufferevent_trigger(bev, EV_WRITE, 0);
        }
        #else
        bufferevent_trigger(bev, EV_WRITE, 0);
        #endif
        
        return;
    }
    else if (events & BEV_EVENT_EOF) {
        Logger::info("XFtpRETR::Event() BEV_EVENT_EOF");
        if(!transfer_complete) {
            Logger::warning("XFtpRETR::Event() -> Unexpected EOF before transfer complete");
            ResCMD("426 Connection closed unexpectedly.\r\n");
        }
        ClosePORT();
    }
    else if (events & BEV_EVENT_ERROR) {
        Logger::error("XFtpRETR::Event() BEV_EVENT_ERROR");
        ClosePORT();
    }
    else if (events & BEV_EVENT_TIMEOUT) {
        Logger::info("XFtpRETR::Event() BEV_EVENT_TIMEOUT");
        ClosePORT();
    }
    // 其他事件暂时忽略
}




void XFtpRETR::Parse(string cmd, string msg){
    Logger::debug("XFtpRETR::Parse() -> msg = ", msg);

    // 重置传输状态
    ResetTransferState();

    // 1. 提取文件名, 去除文件名末尾的回车换行符
    int pos = msg.rfind(" ") + 1;
    string filename = msg.substr(pos);
    while (!filename.empty() && (filename.back() == '\r' || filename.back() == '\n')) {
        filename.pop_back();
    }

    // 2. 构建完整文件路径
    string path = cmdTask->rootDir + cmdTask->curDir + filename;
    Logger::debug("XFtpRETR::Parse() path: ", path);

    // 3. 打开文件（二进制读取模式）
    fp = fopen(path.c_str(), "rb");  // 二进制流式传输
    if (!fp) {
        // 检查具体错误类型
        if (errno == ENOENT) {
            ResCMD("550 File not found.\r\n");
        } else if (errno == EACCES) {
            ResCMD("550 Permission denied.\r\n");
        } else if (errno == EISDIR) {
            ResCMD("550 Is a directory.\r\n");
        } else {
            ResCMD("550 Cannot open file.\r\n");
        }
        return;
    }

    // 4. 发送开始传输响应
    ResCMD("150 File status okay; about to open data connection.\r\n");
    transfer_complete = false;
    // 5. 建立数据连接
    ConnectoPORT();
}