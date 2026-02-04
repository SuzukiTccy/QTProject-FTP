// XFtpPBSZ.h
#pragma once
#include "XFtpTask.h"

class XFtpPBSZ : public XFtpTask {
public:
    virtual void Parse(std::string cmd, std::string param);
};