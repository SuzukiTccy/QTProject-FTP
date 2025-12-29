#pragma once
#include <vector>

#include "XThread.h"
#include "XTask.h"

class XThread;
class XTask;

class XThreadPool{
public:
    static XThreadPool* Get(){
        static XThreadPool instance;
        return &instance;
    }

    void Init(int threadNum);

    void Dispatch(XTask* task);
private:
    int threadCount;
    int lastThread = -1;
    std::vector<XThread*> threads;
    XThreadPool(){};
    ~XThreadPool();
};