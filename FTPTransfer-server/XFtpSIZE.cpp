#include "XFtpSIZE.h"
#include "testUtil.h"
#include <string>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>               // for stat()


using namespace std;

void XFtpSIZE::Parse(string cmd, string msg){
    Logger::debug("XFtpSIZE::Parse() -> cmd: ", cmd, " msg: ", msg);
    // SIZE命令格式：SIZE <filename>
    // 例如：SIZE report.txt\r\n

    // 1. 提取文件名, 去除文件名末尾的回车换行符
    int pos = msg.rfind(" ") + 1;
    string file_name = msg.substr(pos);
    while (!file_name.empty() && (file_name.back() == '\r' || file_name.back() == '\n')) {
        file_name.pop_back();
    }

    // 2. 构建完整文件路径
    string path = cmdTask->rootDir + cmdTask->curDir + file_name;

    // 3. 获取文件大小
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        // 文件存在，返回文件大小
        string size_str = to_string(st.st_size);
        Logger::debug("XFtpSIZE::Parse() -> File size of ", path, ": ", size_str, " bytes");
        cmdTask->ResCMD("213 " + size_str + "\r\n");
    } else {
        Logger::debug("XFtpSIZE::Parse() -> File does not exist or is inaccessible: ", path);
        cmdTask->ResCMD("550 File not found or inaccessible\r\n");
    }
}