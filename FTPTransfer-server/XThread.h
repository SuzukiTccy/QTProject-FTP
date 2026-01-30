#pragma once
#include <event2/util.h>      // libevent工具头文件，提供跨平台的socket类型定义
#include <event2/event.h>     // libevent核心头文件，提供事件循环和事件管理功能
#include <list>               // C++标准库双向链表，用于存储任务队列
#include <mutex>              // C++标准库互斥锁，用于线程同步
#include <thread>             // C++标准库线程，用于多线程编程

#include "XTask.h"
#include "XFtpServerCMD.h"

class XFtpServerCMD;                  // 前向声明，避免循环依赖
struct event_base;            // libevent事件循环前向声明

/**
 * @class XThread
 * @brief 工作线程类，管理一个独立的libevent事件循环和任务队列
 * 
 * 这个类封装了工作线程的核心逻辑，包括：
 * 1. 创建和管理事件循环(event_base)
 * 2. 通过管道通知机制唤醒线程
 * 3. 管理任务队列的线程安全访问
 * 4. 分配任务给对应的XTask对象处理
 * 
 * 每个XThread对象对应一个工作线程，用于处理FTP客户端连接和命令执行
 */
class XThread{
public:
    /**
     * @brief 启动工作线程
     * 创建新线程并执行Main()函数
     */
    bool Start();

    /**
     * @brief 线程主函数
     * 运行事件循环，等待任务通知和事件触发
     */
    void Main();

    /**
     * @brief 线程初始化
     * 创建事件循环和通知机制
     * @return 初始化是否成功
     */
    bool Setup();

    /**
     * @brief 通知回调函数
     * 当线程收到通知时被libevent调用
     * @param socket 通知管道socket
     * @param event_type 事件类型
     */
    void Notify(evutil_socket_t socket, short event_type);

    /**
     * @brief 激活线程处理任务
     * 从任务队列中取出任务并启动处理
     */
    void Activate();

    /**
     * @brief 添加任务到线程队列
     * 线程安全的方法，用于向任务队列添加新任务
     * @param task 要添加的任务指针
     */
    void AddTask(std::shared_ptr<XFtpServerCMD> task);

    /**
     * @brief 停止线程
     * 写入管道停止标志，通知线程退出事件循环
     * @note 线程在处理完当前任务后才会退出
     * */
    void Stop();

    /**
     * @brief 清理已完成的任务
     * 从活动任务映射中移除已断开连接的任务
     */
    void clearConnectedTasks(XFtpServerCMD* task);

    /**
     * @brief 构造函数
     */
    XThread(){};
    
    /**
     * @brief 析构函数
     * 清理资源
     */
    ~XThread();

    int id = 0;                        ///< 线程唯一标识符，用于调试和追踪

private:
    std::thread *pthread = nullptr;           //< 线程对象，用于管理工作线程
    evutil_socket_t notify_send_fd = -1;      //< 通知管道的发送端文件描述符，用于唤醒事件循环
    evutil_socket_t notify_recv_fd = -1;      // 管道读端
    event_base* base = nullptr;               //< libevent事件循环基座，管理所有事件和回调
    std::list<std::shared_ptr<XFtpServerCMD>> active_tasks;             //< 任务队列，存储待处理的XTask对象指针
    std::mutex tasks_mutex;                   //< 任务队列互斥锁，保证线程安全访问
    struct event *notify_event;               // 通知事件对象
};