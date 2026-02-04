// XFtpAUTH.cpp
#include "XFtpAUTH.h"
#include "testUtil.h"
#include <event2/bufferevent.h>

#ifndef OPENSSL_NO_SSL_INCLUDES
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <event2/bufferevent_ssl.h>
extern SSL_CTX *ssl_ctx;  // 引用 main.cpp 中的全局 SSL 上下文
#endif

using namespace std;
void XFtpAUTH::Parse(string cmd, string msg){
    Logger::debug("XFtpAUTH::Parse() -> msg: ", msg);
    
    if(cmd == "AUTH"){
        if(msg.find("TLS") != string::npos || msg.find("SSL") != string::npos){
            // 1. 立即发送响应, 通知客户端可以开始 TLS 协商
            ResCMD("234 Proceed with negotiation.\r\n");
            
            #ifndef OPENSSL_NO_SSL_INCLUDES
                // 2. 关键：立即禁用原bufferevent的读取，防止它读到SSL数据
                bufferevent_disable(cmdTask->bev, EV_READ);

                // 3. 立即开始SSL初始化（同步进行）
                if(!InitSSL()) {
                    Logger::error("XFtpAUTH::Parse() -> SSL initialization failed");
                    // 可以发送错误响应或直接关闭连接
                    ResCMD("550 SSL/TLS initialization failed.\r\n");
                }
            #else
                ResCMD("550 SSL/TLS not supported.\r\n");
            #endif
        } 
        else{
            ResCMD("504 Unrecognized authentication type.\r\n");
        }
    }
}

#ifndef OPENSSL_NO_SSL_INCLUDES
bool XFtpAUTH::InitSSL(){
    Logger::info("XFtpAUTH::InitSSL() -> Starting SSL handshake");
    if(!ssl_ctx){ // 确保ssl_ctx是外部定义的全局或可访问的SSL_CTX*
        Logger::error("XFtpAUTH::InitSSL() -> SSL context not initialized");
        return false;
    }
    
    // 1. 创建新的 SSL 对象
    Logger::info("XFtpAUTH::InitSSL() -> Creating SSL object");
    SSL *ssl = SSL_new(ssl_ctx);
    if(!ssl){
        Logger::error("XFtpAUTH::InitSSL() -> SSL_new failed");
        return false;
    }
    
    // 2. 将 SSL 与现有的 socket 关联
    Logger::info("XFtpAUTH::InitSSL() -> Setting SSL fd");
    SSL_set_fd(ssl, cmdTask->sock);

    // 3. 设置为服务端模式并开始接受握手
    Logger::info("XFtpAUTH::InitSSL() -> Setting SSL to server mode");
    SSL_set_accept_state(ssl);
    
    // 4. 关键：创建“过滤器”式SSL bufferevent，包裹原有的控制连接bufferevent
    Logger::info("XFtpAUTH::InitSSL() -> Creating SSL bufferevent");
    bufferevent *ssl_bev = bufferevent_openssl_filter_new(
        cmdTask->base,
        cmdTask->bev,  // 原有的控制连接 bufferevent
        ssl,
        BUFFEREVENT_SSL_ACCEPTING,
        BEV_OPT_CLOSE_ON_FREE
    );
    
    if(!ssl_bev){
        Logger::error("XFtpAUTH::InitSSL() -> bufferevent_openssl_filter_new failed");
        SSL_free(ssl);
        return false;
    }
    
    // 5. 设置新bufferevent的回调（沿用原来XFtpServerCMD的回调）
    bufferevent_setcb(ssl_bev, 
                     XFtpTask::ReadCB,
                     XFtpTask::WriteCB,
                     XFtpTask::EventCB,
                     cmdTask);
    
    // 6. 启用新bufferevent的事件
    bufferevent_enable(ssl_bev, EV_READ | EV_WRITE);
    
    // 7. 用SSL bufferevent替换原来的控制连接bufferevent
    // 注意：不要释放原bev！bufferevent_openssl_filter_new已经接管了它。
    cmdTask->bev = ssl_bev;
    cmdTask->use_ssl = true;
    cmdTask->ssl = ssl;
    
    // 释放原有的 bufferevent（会自动关闭底层 socket）
    // bufferevent_free(old_bev);
    
    Logger::info("XFtpAUTH::InitSSL() -> SSL filter bufferevent created and swapped successfully");
    
    // 8. 触发一次读事件，让握手继续进行
    bufferevent_trigger(ssl_bev, EV_READ, 0);
    
    return true;
}
#endif