// XFtpPROT.h
#pragma once
#include "XFtpTask.h"

class XFtpPROT : public XFtpTask {
public:
    virtual void Parse(std::string cmd, std::string param);
};