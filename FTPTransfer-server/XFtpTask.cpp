#include "XFtpTask.h"
#include "testUtil.h"

#include <event2/event.h>       // libevent基础事件处理：提供事件循环、基本事件（信号、定时器、文件描述符事件）管理
#include <event2/bufferevent.h> // libevent缓冲事件处理：提供带缓冲的网络IO高级接口，简化数据收发
#include <event2/buffer.h>      // libevent缓冲区操作：提供动态可扩展的内存缓冲区，用于数据存储和管理    

// OpenSSL相关头文件
#ifndef OPENSSL_NO_SSL_INCLUDES
#include <openssl/ssl.h>               // SSL核心库
#include <openssl/err.h>               // SSL错误处理库
#include <event2/bufferevent_ssl.h>    // libevent与OpenSSL集成的缓冲事件
extern SSL_CTX *ssl_ctx;               // 声明外部SSL上下文变量
#endif

#include <iostream>
#include <string.h>
using namespace std;

void XFtpTask::ResCMD(string msg){
	if(!cmdTask || !cmdTask->bev){
        Logger::error("XFtpTaskResCMD(): cmdTask or cmdTask->bev is null");
        return;
    }
	if(msg[msg.size() - 1] != '\n'){
		msg += "\r\n";
	}
	bufferevent_write(cmdTask->bev, msg.c_str(), msg.size());
    Logger::info("XFtpTaskResCMD(): Send Response: ", msg.substr(0, msg.size() - 2));
}

void XFtpTask::Setcb(bufferevent *bev){
    bufferevent_setcb(bev, ReadCB, WriteCB, EventCB, this);
    bufferevent_enable(bev, EV_READ | EV_WRITE);
}

void XFtpTask::ConnectoPORT(){
    cout << endl;
    Logger::info("XFtpTask::ConnectoPORT()");
    if(cmdTask->ip.empty() || cmdTask->port <= 0 || !cmdTask->base){
        Logger::error("XFtpTask::ConnectoPORT() cmdTask no ready");
        return;
    }
    if(bev){
        bufferevent_free(bev);
        bev = nullptr;
    }

    #ifndef OPENSSL_NO_SSL_INCLUDES
        if(cmdTask->use_ssl && cmdTask->ssl){
            // 为数据连接创建 SSL 对象
            Logger::info("XFtpTask::ConnectoPORT() -> Creating SSL for data connection");
            SSL *data_ssl = SSL_new(ssl_ctx);  // 需要访问全局 ssl_ctx
            if(!data_ssl){
                Logger::error("XFtpTask::ConnectoPORT() -> SSL_new failed for data connection");
                return;
            }

        // 关键修改：设置为服务器模式（即使我们是TCP连接的发起方）
        // 在FTP over TLS的PORT模式中，数据连接的SSL角色与控制连接相同
        SSL_set_accept_state(data_ssl);  // 改为accept，而不是connect
        Logger::info("XFtpTask::ConnectoPORT() -> SSL configured as server for data connection");

            Logger::info("XFtpTask::ConnectoPORT() -> Start bev openssl socket new");
            bev = bufferevent_openssl_socket_new(
                cmdTask->base,
                -1,  // 自动创建 socket
                data_ssl,
                BUFFEREVENT_SSL_ACCEPTING,  // ✅ 改为ACCEPTING状态
                BEV_OPT_CLOSE_ON_FREE
            );
        } else {
            bev = bufferevent_socket_new(cmdTask->base, -1, BEV_OPT_CLOSE_ON_FREE);
        }
    #else
        bev = bufferevent_socket_new(cmdTask->base, -1, BEV_OPT_CLOSE_ON_FREE);
    #endif

    if(!bev){
        Logger::error("XFtpTask::ConnectoPORT() -> bufferevent_socket_new error");
        return;
    }else{
        Logger::info("XFtpTask::ConnectoPORT() -> bufferevent_socket_new success");
    }

    sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(cmdTask->port);
    evutil_inet_pton(AF_INET, cmdTask->ip.c_str(), &sin.sin_addr);
    Logger::debug("XFtpTask::ConnectoPORT() ip: ", cmdTask->ip, " port: ", cmdTask->port);

    Setcb(bev); 

    timeval connect_phase_timeout = {30, 0};
    bufferevent_set_timeouts(bev, &connect_phase_timeout, &connect_phase_timeout);

    if(bufferevent_socket_connect(bev, (sockaddr*)&sin, sizeof(sin)) == -1){
        int err = evutil_socket_geterror(bufferevent_getfd(bev));
        // EINPROGRESS 是正常的，非阻塞连接会立即返回这个
        if (err != EINPROGRESS && err != EWOULDBLOCK) {
            Logger::error("XFtpTask::ConnectoPORT() -> Connection failed: ", 
                         evutil_socket_error_to_string(err));
            bufferevent_free(bev);
            bev = nullptr;
            ResCMD("425 Can't build data connection.\r\n");
        } else {
            Logger::info("XFtpTask::ConnectoPORT() -> Connection in progress (EINPROGRESS), waiting...");
        }
    } else {
        Logger::info("XFtpTask::ConnectoPORT() -> Connection initiated successfully");
    }
}


void XFtpTask::ClosePORT(){

    // 清理所有待处理事件
    ClearPendingEvents();
    
    if(bev){
        // 对于上传，需要确保所有数据都已处理
        struct evbuffer* input = bufferevent_get_input(bev);
        if(input) {
            size_t pending = evbuffer_get_length(input);
            if(pending > 0) {
                Logger::warning("XFtpTask::ClosePORT() -> Closing with ", 
                               pending, " bytes still in input buffer");
            }
        }
        
        bufferevent_free(bev);
        bev = nullptr;
    }
    
    if (fp){
        // 确保文件已刷新并关闭
        fflush(fp);
        fclose(fp);
        fp = nullptr;
        Logger::debug("XFtpTask::ClosePORT() -> File closed");
    }
    
    Logger::info("XFtpTask::ClosePORT() close");
}


int XFtpTask::Send(const char* data, size_t datasize){
    if(datasize == 0) return 0;
    if(!bev){
        Logger::error("XFtpTask::Send() bev is null");
        return -2;
    }

    #ifndef OPENSSL_NO_SSL_INCLUDES
        // 检查是否需要 SSL 握手
        if(cmdTask && cmdTask->use_ssl){
            SSL *ssl = bufferevent_openssl_get_ssl(bev);
            if(ssl && SSL_is_init_finished(ssl)){
                // SSL 连接已建立，可以发送数据
            } else {
                Logger::debug("XFtpTask::Send() -> SSL not ready, waiting for handshake");
                return 0;
            }
        }
    #endif


    int result = bufferevent_write(bev, data, datasize);
    if(result == -1){
        // 发送失败
        Logger::error("XFtpTask::Send() bufferevent_write error");
        ResCMD("426 Connection closed; transfer aborted.");
        ClosePORT();
    }
    // Logger::debug("XFtpTask::Send() data: ", data); // 打印发送的数据
    return result;
}


int XFtpTask::Send(const string &data){
    int result = Send(data.c_str(), data.size());
    return result;
}


void XFtpTask::EventCB(bufferevent *bev, short events, void *arg){
    XFtpTask *t = (XFtpTask*)arg;
    t->Event(bev, events);
}

void XFtpTask::ReadCB(bufferevent *bev, void *arg){
    XFtpTask *t = (XFtpTask*)arg;
    t->Read(bev);
}

void XFtpTask::WriteCB(bufferevent *bev, void *arg){
    XFtpTask *t = (XFtpTask*)arg;
    t->Write(bev);
}


XFtpTask::~XFtpTask(){
    ClosePORT();
}

void XFtpTask::Event(bufferevent* bev, short events) {
    Logger::debug("XFtpLIST::Event() events: " + std::to_string(events));
    // 检查是否是连接建立和错误同时发生
    if ((events & BEV_EVENT_CONNECTED) && (events & BEV_EVENT_ERROR)) {
        Logger::info("XFtpLIST::Event() -> CONNECTED+ERROR (likely SSL handshake in progress)");
        
        // TCP连接已建立，但SSL握手可能还在进行或有临时错误
        int err = EVUTIL_SOCKET_ERROR();
        if (err == EINPROGRESS || err == EAGAIN) {
            Logger::info("XFtpLIST::Event() -> SSL handshake in progress, waiting...");
            
            // 等待SSL握手完成，不要触发写入
            // SSL握手成功后会再次触发事件
            return;
        }
    }
    else if (events & BEV_EVENT_CONNECTED) {
        Logger::info("XFtpLIST::Event() BEV_EVENT_CONNECTED");
        // 检查SSL握手是否完成（对于SSL连接）
        #ifndef OPENSSL_NO_SSL_INCLUDES
            if (cmdTask && cmdTask->use_ssl) {
                SSL* ssl = bufferevent_openssl_get_ssl(bev);
                if (ssl && SSL_is_init_finished(ssl)) {
                    Logger::info("XFtpLIST::Event() -> SSL handshake completed, triggering write");
                } else {
                    Logger::info("XFtpLIST::Event() -> SSL handshake in progress, waiting...");
                    // SSL握手未完成，等待握手完成事件
                    // 打印SSL错误队列
                    unsigned long ssl_err;
                    while ((ssl_err = ERR_get_error()) != 0) {
                        char err_buf[256];
                        ERR_error_string_n(ssl_err, err_buf, sizeof(err_buf));
                        Logger::error("SSL Error: " + std::string(err_buf));
                    }
                }
            }
        #endif
        return;
    }
    else if (events & BEV_EVENT_ERROR) {
        Logger::error("XFtpLIST::Event() BEV_EVENT_ERROR");
        
        // 获取错误信息
        int err = EVUTIL_SOCKET_ERROR();
        std::string err_str = evutil_socket_error_to_string(err);
        Logger::error("XFtpLIST::Event() -> Socket error: " + err_str);
        
        // 关键修正：正确检查 EINPROGRESS
        // 比较错误码
        if (err == EINPROGRESS) {
            Logger::info("XFtpLIST::Event() -> EINPROGRESS detected (via errno), this is normal. Waiting...");
            return; // ✅ 立即返回，不关闭连接！
        }
        
        if (err == EAGAIN) {
            Logger::info("XFtpLIST::Event() -> EAGAIN detected, this is normal. Waiting...");
            return; // ✅ 立即返回，不关闭连接！
        }
        
        // 如果是真正的错误，才关闭连接
        Logger::error("XFtpLIST::Event() -> Real connection error, closing.");
        ClosePORT();
        Logger::info("XFtpLIST::Event() close connection");
        return;
    }
    else if (events & BEV_EVENT_EOF) {
        Logger::info("XFtpLIST::Event() BEV_EVENT_EOF");
        ClosePORT();
        Logger::info("XFtpLIST::Event() close connection");
    }
    else if (events & BEV_EVENT_TIMEOUT) {
        Logger::info("XFtpLIST::Event() BEV_EVENT_TIMEOUT");
        ClosePORT();
        Logger::info("XFtpLIST::Event() close connection");
    }
    // 其他事件暂时忽略
}