#include "XFtpLIST.h"
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <sys/stat.h> 
#include "testUtil.h"
using namespace std;



void XFtpLIST::Write(bufferevent* bev) {
    Logger::debug("XFtpLIST::Write()");

    // 发送目录列表数据
    int result = Send(listdata);
    if(result <= 0){
        if(result == 0){
            Logger::info("XFtpLIST::Write() Send complete");
            ResCMD("226 Transfer complete\r\n");
        }
        else{
            Logger::error("XFtpLIST::Parse() Send failed");
            ResCMD("426 Connection closed; transfer aborted.\r\n");
        }
        ClosePORT();
        return;
    }
    
    // 检查输出缓冲区是否还有数据
    struct evbuffer* output = bufferevent_get_output(bev);
    if (evbuffer_get_length(output) == 0) {
        // 所有数据已发送完成
        ResCMD("226 Transfer complete\r\n");
        ClosePORT();
        Logger::info("XFtpLIST::Write() close connection");
    } else {
        // 还有数据待发送，继续等待下一次Write回调
        Logger::debug("XFtpLIST::Write() data remaining, continue sending");
    }
}


void XFtpLIST::Event(bufferevent* bev, short events) {
    Logger::debug("XFtpLIST::Event() events: " + std::to_string(events));
    
    if (events & BEV_EVENT_CONNECTED) {
        Logger::info("XFtpLIST::Event() BEV_EVENT_CONNECTED");
        // 连接建立，可以开始发送数据（如果还没有开始的话）
        bufferevent_trigger(bev, EV_WRITE, 0);
        return;
    }
    else if (events & BEV_EVENT_EOF) {
        Logger::info("XFtpLIST::Event() BEV_EVENT_EOF");
        // 对方关闭了连接，我们也要关闭连接
    }
    else if (events & BEV_EVENT_ERROR) {
        Logger::error("XFtpLIST::Event() BEV_EVENT_ERROR");
        // 发生错误，关闭连接
        // 获取并打印错误信息
        int err = EVUTIL_SOCKET_ERROR();
        Logger::error("XFtpLIST::Event() -> Socket error: " + std::string(evutil_socket_error_to_string(err)));
    }
    else if (events & BEV_EVENT_TIMEOUT) {
        Logger::info("XFtpLIST::Event() BEV_EVENT_TIMEOUT");
        // 超时，关闭连接
    }
    // 其他事件，暂时忽略

    // 关闭连接
    ClosePORT();
    Logger::info("XFtpLIST::Event() close connection");
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

