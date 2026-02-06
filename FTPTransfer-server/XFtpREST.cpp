#include "XFtpREST.h"
#include "testUtil.h"
#include <string>
#include <cstdlib>
#include <sys/types.h>


using namespace std;

void XFtpREST::Parse(string cmd, string msg){
    Logger::debug("XFtpREST::Parse() -> cmd: ", cmd, " msg: ", msg);
    // REST命令格式：REST <偏移量>
    // 例如：REST 1024\r\n

    // 1. 提取偏移量, 去除文件名末尾的回车换行符
    int pos = msg.rfind(" ") + 1;
    string param = msg.substr(pos);
    while (!param.empty() && (param.back() == '\r' || param.back() == '\n')) {
        param.pop_back();
    }

    // 2. 解析偏移量
    char* endptr;   // 用于strtoll的错误检查，如果转换失败endptr会指向原字符串，如果成功则指向数字后第一个非数字字符
    off_t offset = strtoll(param.c_str(), &endptr, 10);  // 以10进制解析偏移量
    if (endptr == param.c_str() || *endptr != '\0' || offset < 0) {
        // 解析失败或偏移量无效
        Logger::error("XFtpREST::Parse() -> Invalid offset: ", param);
        ResCMD("501 Syntax error in parameters or arguments.\r\n");
        return;
    }

    if (offset < 0) {
        // 偏移量不能为负数
        Logger::error("XFtpREST::Parse() -> Negative offset: ", offset);
        ResCMD("501 Syntax error in parameters or arguments.\r\n");
        return;
    }

    // 3. 将偏移量保存到控制任务中
    if(cmdTask) {
        cmdTask->SetFileOffset(offset);
        Logger::debug("XFtpREST::Parse() -> Set file offset to ", offset);
        // 根据RFC 959，响应格式：350 Restarting at <offset>. Send STORE or RETRIEVE to initiate transfer
        ResCMD("350 Restarting at " + to_string(offset) + ". Send STORE or RETRIEVE to initiate transfer.\r\n");
    } else {
        Logger::error("XFtpREST::Parse() -> cmdTask is null");
        ResCMD("550 Internal server error.\r\n");
    }
}