#include <thread>
#include <iostream>
#include <chrono>
#include "XThreadPool.h"
#include "XFtpServerCMD.h"
#include "testUtil.h"


void XThreadPool::Init(int threadNum){
    Logger::info("XThreadPool::Init()");
    this->threadCount = threadNum;
    this->lastThread = -1;
    for(int i = 0; i < threadNum; i++){
        Logger::info("XThreadPool::Init() create thread ", i);
        XThread* t = new XThread();
        t->id = i;
        if(!t->Start()){
            Logger::error("XThreadPool::Init() create thread failed");
            delete t;
            t = nullptr;
            continue;
        }
        threads.push_back(t);
        this_thread::sleep_for(chrono::milliseconds(10));
    }
}


void XThreadPool::Dispatch(std::shared_ptr<XFtpServerCMD> task){
    Logger::info("XThreadPool::Dispatch()");

    if(task == nullptr){
        Logger::warning("XThreadPool::Dispatch() task is nullptr");
        return;
    }

    int tid = (lastThread + 1) % threadCount;
    lastThread = tid;
    XThread* t = threads[tid];
    t->AddTask(task);
}


XThreadPool::~XThreadPool(){
    Logger::info("XThreadPool::~XThreadPool()");
    for(auto t : threads){
        t->Stop();
        delete t;
        t = nullptr;
    }
}