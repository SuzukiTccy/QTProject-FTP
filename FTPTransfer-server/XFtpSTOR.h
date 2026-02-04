#pragma once
#include "XFtpTask.h"

class XFtpSTOR : public XFtpTask{
public:
    void Read(bufferevent *);
    void Event(bufferevent *, short);
    void Parse(string, string);

    // 重置传输状态
    void ResetTransferState() {
        transfer_started = false;
        transfer_complete = false;
        file_write_error = false;
        bytes_received = 0;
        waiting_for_data = false;
    }

private:
    char buf[1024*1024] = {0};
    bool transfer_started = false;       // 传输已开始
    bool transfer_complete = false;      // 传输完成
    bool file_write_error = false;       // 文件写入错误
    size_t bytes_received = 0;           // 已接收字节数
    bool waiting_for_data = false;       // 正在等待数据
};