#include "XFtpSTOR.h"
#include "testUtil.h"
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <iostream>
#include <string>
using namespace std;

void XFtpSTOR::Read(bufferevent *bev){
    Logger::debug("XFtpSTOR::Read()");
    // 1. 检查文件指针是否有效
    if(!fp){
        int err = errno;
        string error_msg;
        
        if(err == EACCES || err == EPERM){
            error_msg = "550 Permission denied.\r\n";
        } else if(err == ENOENT){
            error_msg = "550 File not found.\r\n";
        } else if(err == ENOSPC){
            error_msg = "552 Storage allocation exceeded.\r\n";
        } else {
            error_msg = "550 Cannot create file.\r\n";
        }
        
        Logger::error("XFtpSTOR::Parse() fopen failed: " + string(strerror(err)));
        ResCMD(error_msg);
        ClosePORT();
        return;
    }
    // 2. 循环读取数据并写入文件
    while(true){
        // 从数据连接读取数据
        int len = bufferevent_read(bev, buf, sizeof(buf));
        if(len == 0){
            // 读取到0字节，表示客户端完成发送
            Logger::info("XFtpSTOR::Read() read 0 bytes");
            ResCMD("226 Transfer complete.\r\n");
            return;
        }
        if(len < 0){
            // 读取错误
            Logger::error("XFtpSTOR::Read() read error");
            ResCMD("550 Transfer error.");
            return;
        }
        // 3. 将数据写入文件
        fwrite(buf, 1, len, fp);
    }
    ClosePORT();
}



void XFtpSTOR::Event(bufferevent* bev, short events) {
    Logger::debug("XFtpSTOR::Event() events: " + std::to_string(events));
    
    if (events & BEV_EVENT_CONNECTED) {
        Logger::info("XFtpSTOR::Event() BEV_EVENT_CONNECTED");
        // 连接建立，可以开始发送数据（如果还没有开始的话）
        return;
    }
    else if (events & BEV_EVENT_EOF) {
        Logger::info("XFtpSTOR::Event() BEV_EVENT_EOF");
        // 对方关闭了连接，我们也要关闭连接
    }
    else if (events & BEV_EVENT_ERROR) {
        Logger::error("XFtpSTOR::Event() BEV_EVENT_ERROR");
        // 发生错误，关闭连接
    }
    else if (events & BEV_EVENT_TIMEOUT) {
        Logger::info("XFtpSTOR::Event() BEV_EVENT_TIMEOUT");
        // 超时，关闭连接
    }
    // 其他事件，暂时忽略

    // 关闭连接
    ClosePORT();
    Logger::info("XFtpSTOR::Event() close connection");
}



void XFtpSTOR::Parse(string cmd, string msg){
    #ifdef DEBUG
    Logger::debug("XFtpSTOR::Parse() cmd: " + cmd + " msg: " + msg);
    #endif
    // 1. 提取文件名, 去除文件名末尾的\r\n
    int pos = msg.rfind(" ") + 1;
    string filename = msg.substr(pos);
    while(!filename.empty() && (filename.back() == '\r' || filename.back() == '\n')){
        filename.pop_back();
    }
    // 2. 构建完整文件路径
    string path = cmdTask->rootDir + cmdTask->curDir + filename;
    Logger::info("XFtpSTOR::Parse() path: " + path);
    // 3. 以二进制写模式打开文件
    fp = fopen(path.c_str(), "wb");  // 二进制流式传输
    if(!fp){
        int err = errno;
        string error_msg;
        
        if(err == EACCES || err == EPERM){
            error_msg = "550 Permission denied.\r\n";
        } else if(err == ENOENT){
            error_msg = "550 File not found.\r\n";
        } else if(err == ENOSPC){
            error_msg = "552 Storage allocation exceeded.\r\n";
        } else {
            error_msg = "550 Cannot create file.\r\n";
        }
        Logger::error("XFtpSTOR::Parse() fopen failed: " + string(strerror(err)));
        ResCMD(error_msg);
        return;
    }

    // 4. 发送响应及建立数据连接
    ResCMD("150 Opening data connection for file transfer.\r\n");
    ConnectoPORT();
}