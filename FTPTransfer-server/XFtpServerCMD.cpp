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
    Logger::debug("XFtpServerCMD::Read()");
 
    char buf[BUFS] = {0};
    
    // 1. 一次性读取所有数据
    int total_len = bufferevent_read(bev, buf, BUFS);
    if(total_len <= 0){
        if(total_len == 0){
            Logger::info("XFtpServerCMD::Read() -> bufferevent_read EOF");
        }
        else if(total_len < 0){
            perror("XFtpServerCMD::Read() -> bufferevent_read failed");
        }
        return;
    }
    
    // 2. 按行分割并处理
    int pos = 0;  // 当前处理位置
    while(pos < total_len){
        // 2.1 查找命令结束符 \r\n
        int cmd_end = pos;
        while(cmd_end < total_len && 
              !(buf[cmd_end] == '\r' && cmd_end+1 < total_len && buf[cmd_end+1] == '\n')){
            cmd_end++;
        }
        
        // 2.2 如果没有找到完整命令，跳出循环
        if(cmd_end >= total_len || buf[cmd_end] != '\r'){
            Logger::debug("XFtpServerCMD::Read() -> Incomplete command, waiting for more data");
            break;
        }
        
        // 2.3 提取单条命令（不包括\r\n）
        string cmd_line(buf + pos, cmd_end - pos);
        Logger::debug("XFtpServerCMD::Read() -> Processing command line: ", cmd_line);
        
        // 2.4 解析命令类型
        string type = "";
        for(size_t i = 0; i < cmd_line.size(); ++i){
            if(cmd_line[i] == ' ' || cmd_line[i] == '\t') break;
            type += cmd_line[i];
        }
        transform(type.begin(), type.end(), type.begin(), ::toupper);
        Logger::info("XFtpServerCMD::Read() -> Recv CMD: ", type);
        
        // 2.5 处理命令
        if (calls_map.find(type) != calls_map.end()){
            XFtpTask *t = calls_map[type];
            if(cmd_line.find('\r') == string::npos){
                cmd_line += "\r\n";
            }
            t->Parse(type, cmd_line);  // 传递完整的命令行
            Logger::info("XFtpServerCMD::Read() -> curDir : ", curDir);
            // 注意：不要在这里发送通用响应，让每个命令自己发送响应
        }
        else{
            ResCMD("500 Command not understood\r\n");
            Logger::warning("XFtpServerCMD::Read() -> Unknown CMD: ", type);
        }
        
        // 2.6 移动到下一命令（跳过\r\n）
        pos = cmd_end + 2;  // \r\n 占2个字节
    }
    
    // 3. 检查是否有剩余数据（不完整的命令）
    if(pos < total_len){
        Logger::debug("XFtpServerCMD::Read() -> Remaining data: ", string(buf+pos, total_len-pos));
        // 注意：这里应该保存剩余数据，下次读取时合并
        // 但为了简化，我们先不处理
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