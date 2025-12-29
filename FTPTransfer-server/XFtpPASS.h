#pragma once
#include "XFtpTask.h"

class XFtpPASS : public XFtpTask{
public:
    virtual void Parse(std::string, std::string);
};