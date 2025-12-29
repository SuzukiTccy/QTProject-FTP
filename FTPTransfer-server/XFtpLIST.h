#pragma once
#include "XFtpTask.h"
#include <string>
using namespace std;

class XFtpLIST : public XFtpTask{
public:
    virtual void Parse(string, string);       // 命令解析入口
    virtual void Event(bufferevent*, short);  // 事件回调函数
    virtual void Write(bufferevent*);         // 写入回调函数
private:
    string GetListData(string path);
    string listdata;                          // 文件列表数据
};