#pragma once
#include "XFtpTask.h"
#include <string.h>

class XFtpRETR : public XFtpTask{
public:
    void Parse(std::string cmd, std::string msg);
    virtual void Event(bufferevent*, short);
    virtual void Write(bufferevent *);  // 数据连接写回调

    bool Init() {return true;};         // 初始化（空实现）
private:
    char buf[1024*1024] = {0};          // 1MB文件读取缓冲区
    bool transfer_complete = true;     // 传输完成标志
};