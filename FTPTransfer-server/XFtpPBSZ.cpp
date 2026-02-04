// XFtpPBSZ.cpp
#include "XFtpPBSZ.h"
#include "testUtil.h"

void XFtpPBSZ::Parse(std::string cmd, std::string param) {
    Logger::debug("XFtpPBSZ::Parse() -> cmd: ", cmd, " param: ", param);
    // PBSZ 0 是FTP over TLS必需的命令，表示保护缓冲区大小
    ResCMD("200 PBSZ=0\r\n");  // RFC 4217要求格式
}