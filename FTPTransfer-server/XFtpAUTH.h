// XFtpAUTH.h
#pragma once
#include "XFtpTask.h"
#include <string>

class XFtpAUTH : public XFtpTask
{
public:
    virtual void Parse(std::string cmd, std::string param);
    
#ifndef OPENSSL_NO_SSL_INCLUDES
    bool InitSSL();  // 初始化 SSL
#endif
};