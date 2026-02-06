// XFtpREST.h
#pragma once
#include "XFtpTask.h"

class XFtpREST : public XFtpTask {
public:
    virtual void Parse(std::string cmd, std::string msg);
};