#include "XFtpLIST.h"
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <sys/stat.h> 
#include "testUtil.h"

// OpenSSL相关头文件
#ifndef OPENSSL_NO_SSL_INCLUDES
#include <openssl/ssl.h>               // SSL核心库
#include <openssl/err.h>               // SSL错误处理库
#include <event2/bufferevent_ssl.h>    // libevent与OpenSSL集成的缓冲事件
extern SSL_CTX *ssl_ctx;               // 声明外部SSL上下文变量
#endif

using namespace std;



void XFtpLIST::Write(bufferevent* bev) {
    Logger::debug("XFtpLIST::Write()");
    
    static bool data_queued = false;
    
    if(!data_queued) {
        // 第一次：发送数据
        int result = Send(listdata);
        if(result <= 0){
            if(result == 0){
                Logger::info("XFtpLIST::Write() -> Data queued for sending");
                data_queued = true;
                
                // 关键：不要在这里发送 226
                // 等待下一次 Write() 回调来检查缓冲区
            }
            else{
                Logger::error("XFtpLIST::Parse() Send failed");
                ResCMD("426 Connection closed; transfer aborted.\r\n");
                ClosePORT();
            }
        }
        return;
    }
    
    // 数据已经排队，检查缓冲区
    struct evbuffer* output = bufferevent_get_output(bev);
    if (evbuffer_get_length(output) == 0) {
        // 缓冲区已空，传输完成
        Logger::info("XFtpLIST::Write() -> Buffer empty, transfer complete");
        ResCMD("226 Transfer complete\r\n");
        ClosePORT();
        data_queued = false; // 重置状态
        Logger::info("XFtpLIST::Write() close connection");
    } else {
        // 缓冲区还有数据，继续等待
        Logger::debug("XFtpLIST::Write() -> ", evbuffer_get_length(output), 
                     " bytes remaining in buffer");
    }
}


void XFtpLIST::Event(bufferevent* bev, short events) {
    Logger::debug("XFtpLIST::Event() events: " + std::to_string(events));
    // 检查是否是连接建立和错误同时发生
    #ifndef OPENSSL_NO_SSL_INCLUDES
        // 检查是否是SSL握手完成事件
        if (events & 0x4000) { // libevent的BEV_EVENT_SSL_READY标志
            Logger::info("XFtpLIST::Event() -> SSL handshake completed on data connection");
            
            // 检查SSL状态
            if(cmdTask && cmdTask->use_ssl){
                SSL *ssl = bufferevent_openssl_get_ssl(bev);
                if(ssl && SSL_is_init_finished(ssl)){
                    Logger::info("XFtpLIST::Event() -> SSL ready, starting data transfer");
                    // 开始发送数据
                    bufferevent_trigger(bev, EV_WRITE, 0);
                }
            }
            return;
        }
    #endif

    if (events & BEV_EVENT_CONNECTED) {
        Logger::info("XFtpLIST::Event() BEV_EVENT_CONNECTED");
        // 检查SSL握手是否完成（对于SSL连接）
        #ifndef OPENSSL_NO_SSL_INCLUDES
            if(events & BEV_EVENT_ERROR){
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
            if (cmdTask && cmdTask->use_ssl) {
                SSL* ssl = bufferevent_openssl_get_ssl(bev);
                if (ssl && SSL_is_init_finished(ssl)) {
                    Logger::info("XFtpLIST::Event() -> SSL handshake completed, triggering write");
                    bufferevent_trigger(bev, EV_WRITE, 0);
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
            } else {
                // 非SSL连接，直接开始发送数据
                bufferevent_trigger(bev, EV_WRITE, 0);
            }
        #else
            bufferevent_trigger(bev, EV_WRITE, 0);
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
    #ifndef OPENSSL_NO_SSL_INCLUDES
        else if (events & 0x4000) { // 有些libevent版本用这个标志表示SSL握手完成
            Logger::info("XFtpLIST::Event() -> SSL handshake completed event");
            if (listdata.empty()) {
                Logger::debug("XFtpLIST::Event() -> No data to send yet");
            } else {
                bufferevent_trigger(bev, EV_WRITE, 0);
            }
        }
    #endif
    // 其他事件暂时忽略
}



// 函数作用：获取指定路径的目录列表数据，通过执行系统ls命令并捕获输出
// 参数：path - 要列出内容的目录路径
// 返回值：包含目录详细列表的字符串（类似ls -l格式）
// 安全警告：直接使用popen执行系统命令存在安全风险，特别是当path来自不可信输入时
string XFtpLIST::GetListData(string path){
    Logger::debug("XFtpLIST::GetListData()");
    // 安全性检查
    if(path.find("rm") != string::npos){
        Logger::warning("XFtpLIST::GetListData() path is not safe");
        ResCMD("550 Not allowed.\r\n");
        return "";
    }
    
    // 初始化返回数据为空字符串
    string data = "";
    
    // 构建系统命令："ls -l [path]"
    string cmd = "ls -la ";
    cmd += path;  // 直接将路径拼接到命令中，存在安全风险
    Logger::debug("XFtpLIST::GetListData() cmd: ", cmd);
    
    // 使用popen执行系统命令并获取输出
    // popen()会创建一个管道，fork一个子进程，然后执行shell命令
    // "r"表示从命令输出读取数据
    FILE *f = popen(cmd.c_str(), "r");
    
    // 检查命令执行是否成功
    if (f == nullptr){
        Logger::warning("XFtpLIST::GetListData() popen failed");
        return data;  // 返回空字符串表示失败
    }

    // 缓冲区用于读取命令输出
    char buf[1024] = {0};
    
    // 循环读取命令输出
    while (true){
        // 每次读取最多1023字节（留1字节给字符串结束符）
        int len = fread(buf, 1, sizeof(buf) - 1, f);
        
        // 读取结束或出错时跳出循环
        if(len <= 0) break;
        
        // 确保字符串正确终止
        buf[len] = '\0';
        
        // 将读取到的数据追加到结果字符串
        data += buf;
    }
    
    // 关闭命令管道，等待子进程结束
    pclose(f);
    
    // 返回收集到的目录列表数据
    return data;
}



// 函数作用：解析和处理多个FTP目录相关命令（PWD、LIST、CWD、CDUP）
// 参数：
//   cmd - FTP命令字（"PWD"、"LIST"、"CWD"、"CDUP"）
//   msg - 完整的FTP命令字符串（包含命令和参数）
// 注意：这个类名为XFtpLIST，但实际处理多个目录相关命令，设计上存在责任混淆
void XFtpLIST::Parse(string cmd, string msg){
    cout << endl;
    Logger::debug("XFtpLIST::Parse()");
    Logger::debug("XFtpLIST::Parse() msg: ", msg);
    
    string resmsg = "";
    
    // PWD命令：打印当前工作目录
    if (cmd == "PWD"){
        // 构建FTP响应：257 "目录" is the current directory.
        resmsg = "257 \"";
        resmsg += cmdTask->curDir;  // 从控制连接任务获取当前目录
        resmsg += "\" is the current directory.\r\n";
        ResCMD(resmsg);  // 发送响应到控制连接
    }
    // LIST命令：列出目录内容
    else if (cmd == "LIST"){
        // 构建完整路径：根目录 + 当前目录
        string path = "";
        if(cmdTask->curDir[0] != '/'){ // 如果当前目录不以/开头
            path = cmdTask->curDir;
        }
        else{
            path = cmdTask->curDir.substr(1);
        }
        path = cmdTask->rootDir + path; // 拼接根目录和当前目录
        Logger::debug("XFtpLIST::Parse() path: ", path);
        
        // 获取目录列表数据
        listdata = GetListData(path);
        
        // 发送开始传输响应
        ResCMD("150 Here comes the directory listing.\r\n");
        // 建立数据连接
        ConnectoPORT();
    }
    // CWD命令：改变工作目录
    else if (cmd == "CWD"){
        Logger::debug("XFtpLIST::Parse() CWD");
        
        // 更健壮的解析: 
        string path = "";
        size_t space_pos = msg.find(' ');
        if (space_pos == string::npos) {
            // 没有参数，使用当前目录
            path = ".";
            Logger::info("XFtpLIST::Parse() no parameter, use current directory");
        } else {
            // 提取从空格后到结尾（去除\r\n）
            path = msg.substr(space_pos + 1);
            // 去除末尾的\r\n
            while (!path.empty() && (path.back() == '\r' || path.back() == '\n')) {
                path.pop_back();
            }
        }
        
        if (path == "."){
            ResCMD("250 Directory successfully changed.\r\n");
            return;
        }
        
        // 获取当前目录
        string curDir = cmdTask->curDir;
        
        // 处理绝对路径和相对路径
        if(path[0] == '/'){  // 绝对路径
            curDir = path;
        }
        else{  // 相对路径
            // 确保当前目录以/结尾
            if(curDir != "/" && curDir[curDir.size() - 1] != '/'){
                curDir += "/";
            }
            curDir += path + "/";
        }
        
        // 确保目录路径以/结尾
        if(curDir[curDir.size() - 1] != '/'){
            curDir += "/";
        }
        path = cmdTask->rootDir + curDir;
        Logger::debug("XFtpLIST::Parse() curDir1: ", curDir);
        Logger::debug("XFtpLIST::Parse() path: ", path);
        // 检查目录是否存在
        struct stat s_buf;                    // 定义文件状态结构体
        int result = stat(path.c_str(), &s_buf);         // 获取目录状态信息
        if (result != 0) {
            if (errno == ENOENT) {
                ResCMD("550 Directory does not exist.\r\n");
            } else if (errno == EACCES) {
                ResCMD("550 Permission denied.\r\n");
            } else if (errno == ENOTDIR) {
                ResCMD("550 Not a directory.\r\n");
            } else {
                ResCMD("550 Failed to change directory.\r\n");
            }
            return;
        }

        // 如果是目录，则更新当前目录
        if(S_ISDIR(s_buf.st_mode)){           // 判断是否为目录
            cmdTask->curDir = curDir;         // 更新当前目录
            ResCMD("250 Directory successfully changed.\r\n");  // 发送成功响应
        }
        else{                                 // 不是目录或不存在
            ResCMD("501 Failed to change directory: Directory is not exist.\r\n");  // 发送失败响应
        }
    }
    // CDUP命令：返回上级目录
    else if(cmd == "CDUP"){
        Logger::debug("XFtpLIST::Parse() CDUP");
        Logger::info("XFtpLIST::Parse() msg:", msg);
        Logger::info("XFtpLIST::Parse() cmdTask->curDir:", cmdTask->curDir);

        if(cmdTask->curDir == "/"){
            ResCMD("550 Failed to change directory: No parent directory.\r\n");
            return;
        }

        // 获取当前目录
        string path = cmdTask->curDir;
        
        // 去除末尾的斜杠（如果有）
        if(path[path.size() - 1] == '/'){
            path = path.substr(0, path.size() - 1);
        }
        
        // 找到最后一个斜杠的位置
        int pos = path.rfind("/");
        
        // 提取上级目录路径
        path = path.substr(0, pos);
        
        // 更新当前目录
        cmdTask->curDir = path;
        
        // 确保目录路径以/结尾
        if(cmdTask->curDir[cmdTask->curDir.size() - 1] != '/'){
            cmdTask->curDir += "/";
        }
        
        // 发送成功响应
        ResCMD("250 Directory successfully changed.\r\n");
    }
}

