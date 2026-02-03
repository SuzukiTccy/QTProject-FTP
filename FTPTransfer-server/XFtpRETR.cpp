#include "XFtpRETR.h"
#include "testUtil.h"
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <iostream>
#include <string>
using namespace std;



void XFtpRETR::Write(bufferevent *bev){
    Logger::debug("XFtpRETR::Write()");
    if(transfer_complete) return;
    // 1. 检查文件指针是否有效
    if(!fp){
        Logger::error("XFtpRETR::Write() fp is null");
        ClosePORT();
        return;
    }

    // 2. 从文件读取数据（1MB块）
    int len = fread(buf, 1, sizeof(buf), fp);

    // 3. 处理读取结果
    if(len == 0){
        // 文件结束
        Logger::info("XFtpRETR::Write() end of file reached");
        ResCMD("226 Transfer complete.\r\n");
        ClosePORT();
        return;
    } else if(len < 0){
        // 读取错误
        int err = ferror(fp);
        Logger::error("XFtpRETR::Write() fread failed, error: " + to_string(err));
        ResCMD("426 Connection closed; transfer aborted.\r\n");
        ClosePORT();
        return;
    }
    Logger::info("XFtpRETR::Write() read len = ", len);

    // 4. 发送数据
    int result = Send(buf, len);
    if(result == -1){
        Logger::error("XFtpRETR::Write() Send failed");
        transfer_complete = true;
        ClosePORT();
        return;
    }
}



void XFtpRETR::Event(bufferevent* bev, short events) {
    Logger::debug("XFtpRETR::Event() events: " + std::to_string(events));
    
    if (events & BEV_EVENT_CONNECTED) {
        Logger::info("XFtpRETR::Event() BEV_EVENT_CONNECTED");
        // 连接建立，可以开始发送数据（如果还没有开始的话）
        // 触发第一次写事件
        bufferevent_trigger(bev, EV_WRITE, 0);
        return;
    }
    else if (events & BEV_EVENT_EOF) {
        Logger::info("XFtpRETR::Event() BEV_EVENT_EOF");
        // 对方关闭了连接，我们也要关闭连接
    }
    else if (events & BEV_EVENT_ERROR) {
        Logger::error("XFtpRETR::Event() BEV_EVENT_ERROR");
        // 发生错误，关闭连接
    }
    else if (events & BEV_EVENT_TIMEOUT) {
        Logger::info("XFtpRETR::Event() BEV_EVENT_TIMEOUT");
        // 超时，关闭连接
    }
    // 其他事件，暂时忽略

    // 关闭连接
    ClosePORT();
    Logger::info("XFtpRETR::Event() close connection");
}




void XFtpRETR::Parse(string cmd, string msg){
    Logger::debug("XFtpRETR::Parse() -> msg = ", msg);

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