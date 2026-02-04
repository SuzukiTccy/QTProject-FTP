#pragma once
#include <event2/bufferevent.h>
#include "XTask.h"
#include <string>
using namespace std;

struct bufferevent;

class XFtpTask : public XTask
{
public:
    // FTP会话状态信息
    string curDir = "Desktop/";             // 当前工作目录（客户端所在目录）
    string rootDir = "/Users/ccy/";            // 根目录（限制用户访问的文件系统范围）
    string ip = "";                  // 客户端IP地址（用于数据连接）
    int port = 0;                    // 数据连接端口号（PORT模式使用）
    XFtpTask *cmdTask = nullptr;     // 指向控制连接的FTP任务对象（用于响应命令）

    // 解析FTP命令（纯虚函数，子类需实现具体命令解析）
    // 参数：cmd-命令字，param-命令参数
    virtual void Parse(std::string cmd, std::string param) {}

    // 向控制连接发送FTP响应消息
    // 参数：msg-响应消息（需包含FTP响应码）
    void ResCMD(string msg);

	// 设置bufferevent的回调函数（将静态回调绑定到当前对象）
    // 参数：bev-要设置回调的bufferevent指针
    void Setcb(struct bufferevent* bev);

    // 建立PORT模式数据连接（客户端监听，服务器主动连接）
    void ConnectoPORT();

    // 关闭数据连接和释放相关资源
    void ClosePORT();

    // 通过数据连接发送数据（字符串版本）
    // 参数：data-要发送的字符串数据
    int Send(const string& data);

    // 通过数据连接发送数据（二进制数据版本）
    // 参数：data-数据指针，datasize-数据大小
    int Send(const char *data, size_t datasize);

    // 事件回调虚函数（子类可覆盖处理特定事件）
    // 参数：bev-触发事件的bufferevent，what-事件类型
    virtual void Event(bufferevent *bev, short what);

    // 读事件回调虚函数（子类可覆盖处理数据接收）
    // 参数：bev-触发读事件的bufferevent
    virtual void Read(bufferevent *bev) {}

    // 写事件回调虚函数（子类可覆盖处理数据发送）
    // 参数：bev-触发写事件的bufferevent
    virtual void Write(bufferevent *bev) {}

    // 初始化函数（基类简单实现，子类可覆盖）
    // 返回值：总是返回true表示初始化成功
    bool Init() { return true; }

    // 析构函数（清理资源）
    virtual ~XFtpTask();

    // 数据连接相关的bufferevent对象（用于文件传输、目录列表等数据操作）
    bufferevent *bev = 0;

protected:
    // 静态事件回调函数（libevent C风格回调）
    // 参数：bev-触发事件的bufferevent，what-事件类型，arg-用户数据（指向XFtpTask对象）
    static void EventCB(bufferevent *bev, short what, void *arg);

    // 静态读回调函数（libevent C风格回调）
    // 参数：bev-触发读事件的bufferevent，arg-用户数据（指向XFtpTask对象）
    static void ReadCB(bufferevent *bev, void *arg);

    // 静态写回调函数（libevent C风格回调）
    // 参数：bev-触发写事件的bufferevent，arg-用户数据（指向XFtpTask对象）
    static void WriteCB(bufferevent *bev, void *arg);

    // 文件指针（用于文件上传/下载操作时打开的文件）
    FILE *fp = 0;
};