#include "XFtpPORT.h"
#include "testUtil.h"

#include <iostream>
#include <string>
using namespace std;

void XFtpPORT::Parse(string cmd, string msg){
    cout << endl;
    Logger::info("XFtpPORT::Parse() -> msg: ", msg);

    // 1. 解析PORT命令
    // 格式：
    // PORT 127,0,0,1,70,96\r\n
	// PORT n1,n2,n3,n4,n5,n6\r\n
	// port = n5 * 256 + n6
    vector<string>vals;
    string tmp = "";
    msg = msg.substr(5, msg.size() - 2); // 去掉"PORT "和末尾的\r\n
    for (int i = 0; i < msg.size(); ++i){
        if(!isdigit(msg[i])){
            vals.push_back(tmp);
            tmp = "";
            continue;
        }
        tmp += msg[i];
    }
    vals.erase(std::remove_if(vals.begin(), vals.end(),
        [](const std::string& s) {
            return s.empty() && !isdigit(s[0]);
        }), vals.end());
    if(vals.size() != 6){
        Logger::error("XFtpPORT::Parse() invalid PORT command, not 6 values");
        ResCMD("501 Syntax error in parameters or arguments.\r\n");
        return;
    }

    // 2. 构建IP和计算端口
    ip = vals[0] + "." + vals[1] + "." + vals[2] + "." + vals[3];
    port = atoi(vals[4].c_str()) * 256 + atoi(vals[5].c_str());

    if(port < 1 || port > 65535){
        Logger::error("Client specified port " + to_string(port) + 
                " which may not be available");
        ResCMD("501 Syntax error in parameters or arguments.\r\n");
        return;
    }
    cmdTask->ip = ip;
    cmdTask->port = port;
    Logger::debug("XFtpPORT::Parse() ip: ", ip);
    Logger::debug("XFtpPORT::Parse() port: ", port);

    // 3. 返回响应
    ResCMD("200 Port command successful.\r\n");
}