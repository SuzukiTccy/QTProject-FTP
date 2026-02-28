#include <stdlib.h>         // C标准库：malloc/free、exit等
#include <signal.h>         // 信号处理：signal()函数
#include <string.h>         // 字符串操作：memset等
#include <string>           // C++字符串类
#include <fstream>          // 文件流（当前未使用）
#include <unistd.h>         // POSIX API：getpid()等

// libevent相关头文件
#include <event2/event.h>           // 核心事件处理
#include <event2/listener.h>        // TCP监听器
#include <event2/bufferevent.h>     // 缓冲事件
#include <event2/buffer.h>          // 缓冲区操作
#include <event2/util.h>            // 工具函数

// OpenSSL相关头文件
#ifndef OPENSSL_NO_SSL_INCLUDES
#include <openssl/ssl.h>            // SSL核心库
#include <openssl/err.h>            // SSL错误处理
#include <openssl/rand.h>           // SSL随机数生成
#endif

// 在全局变量处添加
#ifndef OPENSSL_NO_SSL_INCLUDES
SSL_CTX *ssl_ctx = nullptr;   // SSL上下文
#endif

// 项目自定义头文件
#include "XThreadPool.h"
#include "XThread.h"
#include "XTask.h"
#include "XFtpFactory.h"
#include "testUtil.h"

#define SPORT 21            // FTP默认控制端口
#define BUFS 1024           // 缓冲区大小（当前未使用）
#define XThreadPoolGet XThreadPool::Get()  // 单例获取宏


static event_base *global_base = nullptr;      // 静态全局事件循环对象指针，用于退出程序时终止事件循环
void signal_handler(int sig){
    if(sig == SIGINT || sig == SIGTERM){
        Logger::info("Main Thread: 收到退出信号, 开始关闭服务器...");
        event_base_loopbreak(global_base);

        #ifndef OPENSSL_NO_SSL_INCLUDES
        if(ssl_ctx){
            SSL_CTX_free(ssl_ctx);
            ssl_ctx = nullptr;
        }
        EVP_cleanup();
        #endif
        exit(0);
    }
}


void listen_cb(struct evconnlistener *evl, evutil_socket_t fd, 
                struct sockaddr *addr, int socklen, void *arg)
{
    Logger::info("Main Thread: New connection");
    // sockaddr_in *sin = (sockaddr_in *)addr; // 当前未使用
    std::shared_ptr<XFtpServerCMD>cmdTask = XFtpFactory::Get()->CreateTask();
    cmdTask->sock = fd;

    XThreadPoolGet->Dispatch(cmdTask);
}

void clear(event_base *base, evconnlistener *evl){
    if(evl) evconnlistener_free(evl);
    if(base) event_base_free(base);
}


int main(){
    // 初始化OpenSSL
    #ifndef OPENSSL_NO_SSL_INCLUDES
    SSL_library_init();            // 初始化OpenSSL库
    SSL_load_error_strings();      // 加载错误信息
    OpenSSL_add_all_algorithms();  // 加载所有加密算法

    // 创建SSL上下文时，设置明确的协议版本和选项
    ssl_ctx = SSL_CTX_new(TLS_server_method());
    if(!ssl_ctx) {
        Logger::error("Failed to create SSL context");
        return -1;
    }

    // 禁用不安全的旧协议（SSLv2, SSLv3, TLSv1.0）
    SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION);
    // SSL_CTX_set_max_proto_version(ssl_ctx, TLS1_3_VERSION); // 如果支持TLS1.3

    // 设置常用的安全选项
    SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);

    // 加载证书和私钥
    // 自签名证书
    if(SSL_CTX_use_certificate_file(ssl_ctx, "server.crt", SSL_FILETYPE_PEM) <= 0){
        Logger::error("Main Thread -> SSL_CTX_use_certificate_file error");
        SSL_CTX_free(ssl_ctx);
        return -1;
    }

    if(SSL_CTX_use_PrivateKey_file(ssl_ctx, "server.key", SSL_FILETYPE_PEM) <= 0){
        Logger::error("Main Thread -> SSL_CTX_use_PrivateKey_file error");
        SSL_CTX_free(ssl_ctx);
        return -1;
    }

    // 检查证书和私钥是否匹配
    if(!SSL_CTX_check_private_key(ssl_ctx)){
        Logger::error("Main Thread -> SSL_CTX_check_private_key error");
        SSL_CTX_free(ssl_ctx);
        return -1;
    }

    Logger::info("Main Thread -> OpenSSL initialized successfully");
    #endif


    // 忽略SIGPIPE信号
    if(signal(SIGPIPE, SIG_IGN) == SIG_ERR){
        Logger::error("Main Thread -> signal error");
        return -1;
    }
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 1. 初始化线程池
    XThreadPoolGet->Init(20);

    // 2. 初始化libevent事件循环基座
    event_base *base = event_base_new();
    if(!base){
        Logger::error("Main Thread -> event_base_new error");
        return -1;
    }
    global_base = base;

    // 3. 网络地址配置
    sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(SPORT);

    // 4. 创建监听器
    evconnlistener *evl = evconnlistener_new_bind(
        base,                                        // libevent事件循环基座
        listen_cb,                                   // 接收到连接的回调函数
        base,                                        // 回调函数的参数arg
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,   // 监听器选项：监听器关闭时释放资源，端口可重用
        10,                                          // 监听器队列大小
        (struct sockaddr *)&sin,                     // 监听地址
        sizeof(sin)
    );
    if(evl == NULL){
        Logger::error("Main Thread -> evconnlistener_new_bind error");
        clear(base, evl);
        return -1;
    }
    Logger::info("Main Thread -> Create evconnlistener evl: ", evl);

    // 5. 开始事件循环
    Logger::info("Main Thread -> event_base_dispatch begin");
    Logger::info("Main Thread -> Begin to listen...");
    Logger::info("Main Thread -> Listen port: ", SPORT);
    Logger::info("Main Thread -> Thread_id: ", getpid());
    event_base_dispatch(base);
    Logger::info("Main Thread -> event_base_dispatch exit");
    SSL_CTX_free(ssl_ctx);
    Logger::info("Main Thread -> SSL_CTX_free called");
    clear(base, evl);
    Logger::info("Main Thread -> exit");
    return 0;
}

