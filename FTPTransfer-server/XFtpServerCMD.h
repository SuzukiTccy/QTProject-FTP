#pragma once
#include "XFtpTask.h"
#include "XThread.h"

class XThread;  // 前向声明，避免循环依赖

#include <map>

class XFtpServerCMD : public XFtpTask{
public:
    virtual bool Init();                                  // 初始化 FTP 命令服务器

    virtual void Event(bufferevent *bev, short events);   // 处理控制连接的事件回调

    virtual void Read(bufferevent *bev);                  // 核心命令解析和分发函数，处理客户端发送的命令

    void Reg(std::string, XFtpTask *call);                // 命令注册函数

    XFtpServerCMD(){};
    virtual ~XFtpServerCMD();

    XThread* thread = nullptr;                            // 命令服务器线程

private:
    std::map<std::string, XFtpTask*> calls_map;  // 命令注册表
    // std::map<XFtpTask*, int> callsDel_map;    // 任务删除标记表
    std::string read_buffer; // 新增：用于累积未处理完的数据
};