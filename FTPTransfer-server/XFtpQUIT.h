#pragma once
#include "XFtpTask.h"

class XFtpQUIT : public XFtpTask {
public:
    virtual void Parse(std::string cmd, std::string msg) override;
};