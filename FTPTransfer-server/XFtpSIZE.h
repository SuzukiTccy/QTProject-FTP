// XFtpREST.h
#pragma once
#include "XFtpTask.h"

class XFtpSIZE : public XFtpTask {
public:
    virtual void Parse(std::string cmd, std::string param);
};