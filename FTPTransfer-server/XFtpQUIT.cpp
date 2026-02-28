#include "XFtpQUIT.h"
#include "testUtil.h"

void XFtpQUIT::Parse(std::string cmd, std::string msg) {
    Logger::debug("XFtpQUIT::Parse()");
    // 发送标准响应
    ResCMD("221 Goodbye.\r\n");
    
    // 从线程活动任务列表中移除当前控制任务
    // 这将触发 XFtpServerCMD 对象的析构，自动释放 bufferevent 等资源
    // 不用处理，断开连接后，XFtpServerCMD 的析构函数会被调用，清理资源
}