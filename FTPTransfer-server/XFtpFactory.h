#pragma once
#include "XTask.h"

class XFtpFactory{
public:
    static XFtpFactory* Get(){   // 静态方法获取唯一实例
        static XFtpFactory f;    // 局部静态变量，C++11保证线程安全，这行代码只会执行一次
        return &f;
    }
    XTask* CreateTask();         // 工厂方法，创建任务对象
private:
    XFtpFactory(){};             // 构造函数私有化，防止外部创建
};