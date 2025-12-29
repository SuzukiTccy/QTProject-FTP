#include "XFtpTask.h"
#include "testUtil.h"

#include <event2/event.h>       // libevent基础事件处理：提供事件循环、基本事件（信号、定时器、文件描述符事件）管理
#include <event2/bufferevent.h> // libevent缓冲事件处理：提供带缓冲的网络IO高级接口，简化数据收发

#include <iostream>
#include <string.h>
using namespace std;

void XFtpTask::ResCMD(string msg){
	if(!cmdTask || !cmdTask->bev){
        Logger::error("XFtpTaskResCMD(): cmdTask or cmdTask->bev is null");
        return;
    }
	if(msg[msg.size() - 1] != '\n'){
		msg += "\r\n";
	}
	bufferevent_write(cmdTask->bev, msg.c_str(), msg.size());
    Logger::info("XFtpTaskResCMD(): Send Response: ", msg.substr(0, msg.size() - 2));
}

void XFtpTask::Setcb(bufferevent *bev){
    bufferevent_setcb(bev, ReadCB, WriteCB, EventCB, this);
    bufferevent_enable(bev, EV_READ | EV_WRITE);
}

void XFtpTask::ConnectoPORT(){
    cout << endl;
    Logger::info("XFtpTask::ConnectoPORT()");
    if(cmdTask->ip.empty() || cmdTask->port <= 0 || !cmdTask->base){
        Logger::error("XFtpTask::ConnectoPORT() cmdTask no ready");
        return;
    }
    if(bev){
        bufferevent_free(bev);
        bev = nullptr;
    }
    bev = bufferevent_socket_new(cmdTask->base, -1, BEV_OPT_CLOSE_ON_FREE);
    if(!bev){
        Logger::error("XFtpTask::ConnectoPORT() -> bufferevent_socket_new error");
        return;
    }
    sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(cmdTask->port);
    evutil_inet_pton(AF_INET, cmdTask->ip.c_str(), &sin.sin_addr);
    Logger::debug("XFtpTask::ConnectoPORT() ip: ", cmdTask->ip, " port: ", cmdTask->port);

    Setcb(bev); 

    timeval t = {30, 0};
    bufferevent_set_timeouts(bev, &t, 0);

    if(bufferevent_socket_connect(bev, (sockaddr*)&sin, sizeof(sin)) == -1){
        Logger::error("XFtpTask::ConnectoPORT() -> bufferevent_socket_connect error");
        cout << "[ERROR] XFtpTask::ConnectoPORT() -> ";
        perror("bufferevent_socket_connect failed");
        return;
    }
}


void XFtpTask::ClosePORT(){
    if(bev){
        bufferevent_free(bev);
        bev = nullptr;
    }
    if (fp){
        fclose(fp);
        fp = nullptr;
    }
    Logger::info("XFtpTask::ClosePORT() close");
}


int XFtpTask::Send(const char* data, size_t datasize){
    if(datasize == 0) return 0;
    if(!bev){
        Logger::error("XFtpTask::Send() bev is null");
        return -2;
    }
    int result = bufferevent_write(bev, data, datasize);
    if(result == -1){
        // 发送失败
        Logger::error("XFtpTask::Send() bufferevent_write error");
        ResCMD("426 Connection closed; transfer aborted.");
        ClosePORT();
    }
    Logger::info("XFtpTask::Send() data: ", data);
    return result;
}


int XFtpTask::Send(const string &data){
    int result = Send(data.c_str(), data.size());
    return result;
}


void XFtpTask::EventCB(bufferevent *bev, short events, void *arg){
    XFtpTask *t = (XFtpTask*)arg;
    t->Event(bev, events);
}

void XFtpTask::ReadCB(bufferevent *bev, void *arg){
    XFtpTask *t = (XFtpTask*)arg;
    t->Read(bev);
}

void XFtpTask::WriteCB(bufferevent *bev, void *arg){
    XFtpTask *t = (XFtpTask*)arg;
    t->Write(bev);
}


XFtpTask::~XFtpTask(){
    ClosePORT();
}

void XFtpTask::Event(bufferevent* bev, short events) {
    Logger::debug("XFtpTask::Event() events: " + std::to_string(events));
    
    if (events & BEV_EVENT_CONNECTED) {
        Logger::info("XFtpTask::Event() BEV_EVENT_CONNECTED");
        // 连接建立，可以开始发送数据（如果还没有开始的话）
        // 注意：在主动模式下，连接建立后可能已经发送了数据，所以这里可能不需要做任何事情
        return;
    }
    else if (events & BEV_EVENT_EOF) {
        Logger::info("XFtpTask::Event() BEV_EVENT_EOF");
        // 对方关闭了连接，我们也要关闭连接
    }
    else if (events & BEV_EVENT_ERROR) {
        Logger::error("XFtpTask::Event() BEV_EVENT_ERROR");
        // 获取并打印错误信息
        int err = EVUTIL_SOCKET_ERROR();
        Logger::error("Socket error: " + std::string(evutil_socket_error_to_string(err)));
    }
    else if (events & BEV_EVENT_TIMEOUT) {
        Logger::info("XFtpTask::Event() BEV_EVENT_TIMEOUT");
        // 超时，关闭连接
    }
    // 其他事件，暂时忽略

    // 关闭连接
    ClosePORT();
    Logger::info("XFtpTask::Event() close connection");
}