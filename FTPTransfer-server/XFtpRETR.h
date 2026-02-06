#pragma once
#include "XFtpTask.h"
#include <string.h>

class XFtpRETR : public XFtpTask{
public:
    void Parse(std::string cmd, std::string msg);
    virtual void Event(bufferevent*, short);
    virtual void Write(bufferevent *);  // 数据连接写回调

    bool Init() {return true;};         // 初始化（空实现）

    // 重置传输状态
    void ResetTransferState() {
        transfer_started = false;
        transfer_complete = false;
        file_eof = false;
        file_read_error = false;
        file_pos = 0;
    }

private:
    char buf[1024*1024] = {0};           // 1MB文件读取缓冲区
    bool transfer_started = false;       // 传输已开始
    bool transfer_complete = false;      // 传输完成（包括缓冲区清空）
    bool file_eof = false;               // 文件已读到末尾
    bool file_read_error = false;        // 文件读取错误
    long file_pos = 0;                   // 文件读取位置（用于调试）
    long file_size = 0;               // 文件大小（用于调试）
};