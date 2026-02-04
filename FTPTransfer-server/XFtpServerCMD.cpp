#include <string.h>                      // C语言字符串处理函数（如strlen、memcpy等）
#include <event2/bufferevent.h>          // libevent缓冲事件库，提供带缓冲的网络I/O操作
#include <event2/event.h>                // libevent核心事件库，提供事件循环和基础事件处理
#include <event2/util.h>                 // libevent工具库，提供跨平台的网络编程辅助函数

#include <string>                        // C++标准字符串库，提供std::string类
#include <thread>                        // C++标准线程库，提供std::thread类
using namespace std;                     // 使用std命名空间，简化代码编写

#include "XFtpServerCMD.h"               // 包含FTP服务器命令调度器的类定义
#include "testUtil.h"                    // 包含测试工具函数或调试辅助函数

#define BUFS 4096                        // 定义缓冲区大小为4096字节，用于网络数据读写



bool XFtpServerCMD::Init(){
    Logger::debug("XFtpServerCMD::Init()");

    bufferevent *bev = bufferevent_socket_new(base, sock, BEV_OPT_CLOSE_ON_FREE);
    if(!bev){
        Logger::error("XFtpServerCMD::Init() bufferevent_socket_new failed");
        return false;
    }

    timeval t = {300, 0};
    bufferevent_set_timeouts(bev, &t, 0);

    string msg = "220 FTP Server ready\r\n";
    bufferevent_write(bev, msg.c_str(), msg.size());

    this->cmdTask = this;
    this->bev = bev;
    Setcb(bev);

    Logger::info("XFtpServerCMD::Init() finished");
    return true;
}



void XFtpServerCMD::Event(bufferevent *bev, short events){
    Logger::debug("XFtpServerCMD::Event()");
    if (events & BEV_EVENT_CONNECTED) {
        Logger::info("XFtpServerCMD::Event() BEV_EVENT_CONNECTED");
        // 连接建立，可以开始发送数据（如果还没有开始的话）
        // 注意：在主动模式下，连接建立后可能已经发送了数据，所以这里可能不需要做任何事情
        return;
    }
    else if(events & BEV_EVENT_EOF){
        Logger::info("XFtpServerCMD::Event() BEV_EVENT_EOF");
    }
    else if(events & BEV_EVENT_ERROR){
        Logger::error("XFtpServerCMD::Event() connection error");
    }
    else if(events & BEV_EVENT_TIMEOUT){
        Logger::info("XFtpServerCMD::Event() connection timeout");
        
		// 发送超时响应
        string msg = "421 Service closing control connection due to timeout.\r\n";
        bufferevent_write(bev, msg.c_str(), msg.size());
    }
    // 关闭连接
    ClosePORT();
    this->thread->clearConnectedTasks(this);
    Logger::info("XFtpServerCMD::Event() close connection");
}


void XFtpServerCMD::Read(bufferevent *bev){
    cout << endl;
    Logger::debug("XFtpServerCMD::Read() called");
 
    char buf[BUFS] = {0};
    
    // 1. 读取新数据
    int len = bufferevent_read(bev, buf, BUFS);
    if(len <= 0){
        if(len == 0){
            Logger::info("XFtpServerCMD::Read() -> bufferevent_read EOF");
        }
        else {
            perror("XFtpServerCMD::Read() -> bufferevent_read failed");
        }
        return;
    }
    
    // 2. 将新数据追加到累积缓冲区
    read_buffer.append(buf, len);
    Logger::debug("XFtpServerCMD::Read() -> Appended ", len, " bytes to buffer. Buffer now (raw): \"", 
                    read_buffer, "\"");
    
    // 3. 循环处理缓冲区中所有完整的命令（以\r\n结尾）
    size_t start_pos = 0;
    while (true) {
        // 3.1 查找命令结束符 \r\n
        size_t crlf_pos = read_buffer.find("\r\n", start_pos);
        Logger::debug("XFtpServerCMD::Read() -> Looking for \\r\\n from pos ", start_pos, ", found at: ", 
                     (crlf_pos == std::string::npos ? "npos" : std::to_string(crlf_pos)));
        
        if (crlf_pos == std::string::npos) {
            // 没有找到完整的命令，跳出循环
            break;
        }
        
        // 3.2 提取一条完整命令（不包括\r\n）
        std::string cmd_line = read_buffer.substr(start_pos, crlf_pos - start_pos);
        Logger::debug("XFtpServerCMD::Read() -> Processing command line: \"", cmd_line, "\"");
        
        // 3.3 解析命令类型
        std::string type;
        for (size_t i = 0; i < cmd_line.size(); ++i) {
            if (cmd_line[i] == ' ' || cmd_line[i] == '\t') break;
            type += cmd_line[i];
        }
        std::transform(type.begin(), type.end(), type.begin(), ::toupper);
        Logger::info("XFtpServerCMD::Read() -> Recv CMD: ", type);
        
        // 3.4 处理命令
        auto it = calls_map.find(type);
        if (it != calls_map.end()) {
            XFtpTask *t = it->second;
            Logger::debug("XFtpServerCMD::Read() -> Found handler for command: ", type);
            // 确保传递完整的FTP格式
            t->Parse(type, cmd_line + "\r\n");
            Logger::info("XFtpServerCMD::Read() -> curDir: ", curDir);
        } else {
            ResCMD("500 Command not understood\r\n");
            Logger::warning("XFtpServerCMD::Read() -> Unknown CMD: ", type, ", available commands: ");
            // 打印所有可用命令以便调试
            for (const auto& pair : calls_map) {
                Logger::warning("  - ", pair.first);
            }
        }
        
        // 3.5 移动到下一条命令的开始位置（跳过\r\n）
        start_pos = crlf_pos + 2;
        Logger::debug("XFtpServerCMD::Read() -> Moving start_pos to: ", start_pos);
    }
    
    // 4. 处理完成后，清理已处理的数据
    if (start_pos > 0) {
        read_buffer.erase(0, start_pos);
        Logger::debug("XFtpServerCMD::Read() -> Cleared processed data. Remaining buffer: \"", 
                      read_buffer, "\"");
    }
    
    // 5. 可选：设置缓冲区大小上限，防止异常情况
    if (read_buffer.size() > BUFS * 4) {
        Logger::error("XFtpServerCMD::Read() -> Read buffer too large, possible protocol error. Clearing.");
        read_buffer.clear();
    }
}



void XFtpServerCMD::Reg(std::string cmd, XFtpTask *call){
    Logger::debug("XFtpServerCMD::Reg() -> cmd: " + cmd);
    if(!call){
        Logger::error("XFtpServerCMD::Reg() call is null");
        return;
    }
    if(cmd.empty()){
        Logger::error("XFtpServerCMD::Reg() cmd is null");
        return;
    }
    if(calls_map.find(cmd) != calls_map.end()){
        Logger::info("XFtpServerCMD::Reg() cmd already registered: ", cmd);
        return;
    }

    call->base = base;
    call->cmdTask = this;
    calls_map[cmd] = call;
    // callsDel_map[call] = 0;
}



XFtpServerCMD::~XFtpServerCMD(){
    Logger::debug("XFtpServerCMD::~XFtpServerCMD()");

    // 清理注册的命令处理器
    for(auto &pair : calls_map){
        Logger::info("XFtpServerCMD::~XFtpServerCMD() delete ", pair.second, " ", pair.first);
        if(pair.second != nullptr) continue;
        delete pair.second;
        pair.second = nullptr;
    }
    calls_map.clear();

    // 清理删除的命令处理器
    // for(auto &pair : callsDel_map){
    //     delete pair.first;
    //     Logger::info("XFtpServerCMD::~XFtpServerCMD() delete ", pair.first);
    // }
    // callsDel_map.clear();

    // 清理控制连接的bev
    if (bev && cmdTask == this){
        // 这是控制连接，确保正常关闭
        bufferevent_free(bev);
        bev = nullptr;
    }
    ClosePORT();
}