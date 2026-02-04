// XFtpPROT.cpp
#include "XFtpPROT.h"
#include "testUtil.h"

void XFtpPROT::Parse(std::string cmd, std::string msg) {
    Logger::debug("XFtpPROT::Parse() -> cmd: ", cmd, " msg: ", msg);
    
    std::string prot_level = msg.substr(5); // 提取PROT命令的参数部分
    prot_level.erase(prot_level.find_last_not_of("\r\n") + 1); // 去除末尾的换行符
    // 转换为大写以便比较
    for (auto& c : prot_level) c = std::toupper(c);
    
    if (prot_level == "P") {
        // PROT P 表示数据通道需要加密
        ResCMD("200 Protection level set to Private\r\n");
    } else if (prot_level == "C") {
        // PROT C 表示数据通道不需要加密
        ResCMD("200 Protection level set to Clear\r\n");
    } else if (prot_level == "S" || prot_level == "E") {
        // PROT S 或 PROT E 表示安全/机密级别（较少使用）
        ResCMD("504 Unsupported protection level\r\n");
    } else {
        ResCMD("501 Syntax error in parameters or arguments\r\n");
    }
}