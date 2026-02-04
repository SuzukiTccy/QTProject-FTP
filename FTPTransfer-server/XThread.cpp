#include <thread>
#include <iostream>

#include <unistd.h>           // POSIX API
#include <event2/event.h>

#include "XThread.h"
#include "XTask.h"
#include "testUtil.h"




static void Notify_cb(evutil_socket_t fd, short event, void *arg){
    XThread *t = (XThread *)arg;
    t->Notify(fd, event);
}


void XThread::Notify(evutil_socket_t fd, short event){
    Logger::info("XThread::Notify() -> Thread_id ", id);

    // 1. 读取管道中的数据
    char buf[1] = {0};
    int re = read(fd, buf, 1);
    if(re < 0){
        Logger::error("XThread::Notify() -> Thread_id ", id, " read() error");
        return;
    }
    if(re == 0){
        Logger::info("XThread::Notify() -> Thread_id ", id,  " read() return 0");
        return;
    }
    Logger::info("XThread::Notify() -> Thread_id ", id, " recv: ", buf);
    if(buf[0] == 's'){
        Logger::info("XThread::Notify() -> Thread_id ", id, " : stop");
        event_base_loopbreak(base);  // 停止事件循环
        return;
    }

    // 2. 取出任务并执行
    std::shared_ptr<XFtpServerCMD> t = nullptr;
    t = connect_tasks.front();
    connect_tasks.pop();
    tasks_mutex.unlock();
    t->Init();
}


bool XThread::Start(){
    Logger::info("XThread::Start() -> Thread_id ", id);
    if(!Setup()){
	    Logger::error("XThread::Start() Setup failed");
	    return false;
	}
    pthread = new std::thread(&XThread::Main, this);
    if(pthread == nullptr){
        Logger::error("XThread::Start() -> Thread_id ", id, ": pthread is nullptr");
        return false;
    }
    return true;
}



void XThread::Main(){
    Logger::info("XThread::Main() -> Thread_id ", id);
    int ret = event_base_dispatch(base);
	if(ret == -1){
	    Logger::error("XThread::Main() -> Thread_id ", id, ": event_base_dispatch failed");
	}
    event_free(notify_event);
    event_base_free(base);
    Logger::info("XThread::Main() -> Thread_id ", id, " exit");
}


bool XThread::Setup(){
    Logger::info("XThread::Setup() -> Thread_id ", id);
    
    int fds[2];
    if(pipe(fds)){
        Logger::error("XThread::Setup() -> Thread_id ", id, ": pipe() error");
        return false;
    }

    notify_recv_fd = fds[0];
    notify_send_fd = fds[1];

    event_config *ev_conf = event_config_new();
    event_config_set_flag(ev_conf, EVENT_BASE_FLAG_NOLOCK);
    this->base = event_base_new_with_config(ev_conf);
    event_config_free(ev_conf);
    if(!base){
        Logger::error("XThread::Setup() -> Thread_id ", id, ": event_base_new_with_config() error");
        return false;
    }
    // 创建持久化的读事件，用于监听通知管道，EV_PERSIST表示事件触发后不自动删除
    notify_event = event_new(base, notify_recv_fd, EV_READ | EV_PERSIST, Notify_cb, this);
    event_add(notify_event, NULL);

    return true;
}



void XThread::Activate(){
    Logger::info("XThread::Activate() -> Thread_id ", id);

    // 1. 检查任务队列是否为空
    tasks_mutex.lock();
    if(active_tasks.empty()){
        tasks_mutex.unlock();
        return;
    }
    tasks_mutex.unlock();

    // 2. 写管道，激活线程
    int re = write(notify_send_fd, "c", 1);
    if(re <= 0){
        Logger::error("XThread::Activate() -> Thread_id ", id, ": write() error");
        return;
    }
}


void XThread::AddTask(std::shared_ptr<XFtpServerCMD> t){
    Logger::info("XThread::AddTask() -> Thread_id ", id);

    t->base = this->base;
    t->thread = this;
    if(t == nullptr){
        Logger::error("XThread::AddTask() -> Thread_id ", id, ": XTask is nullptr");
        return;
    }
    tasks_mutex.lock();
    active_tasks.push_back(t);
    connect_tasks.push(t);
    tasks_mutex.unlock();

    Activate(); // 唤醒线程
}


void XThread::Stop(){
    Logger::info("XThread::Stop() -> Thread_id ", id);

    // 1. 写管道，停止线程
    int re = write(notify_send_fd, "s", 1);
    if(re <= 0){
        Logger::error("XThread::Stop() -> Thread_id ", id, 
                    ": write() error. Detail: ", strerror(errno));
    }
}


void XThread::clearConnectedTasks(XFtpServerCMD* task){
    Logger::info("XThread::clearConnectedTasks() -> Thread_id ", id);
    if(!task){
        Logger::error("XThread::clearConnectedTasks() -> Thread_id ", id, ": task is nullptr");
        return;
    }
    
    for(auto it = active_tasks.begin(); it != active_tasks.end(); it++){
        if(it->get()->cmdTask == task){
            active_tasks.erase(it);
            Logger::info("XThread::clearConnectedTasks() -> Thread_id ", id, 
                ": XFtpServerCMD ", task, " ip :", task->ip,
                "removed from active_tasks");
            return;
        }
    }
}


XThread::~XThread(){
    Stop();

    // 等待线程结束
    if(pthread){
        if(pthread->joinable()) pthread->join();
        delete pthread;
        pthread = nullptr;
        Logger::info("XThread::~XThread() -> Thread_id ", id, " delete pthread");
    }

    close(notify_recv_fd);
    close(notify_send_fd);

}