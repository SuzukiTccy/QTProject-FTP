#pragma once
#include "XFtpTask.h"

class XFtpUSER : public XFtpTask{
public:
    virtual void Parse(std::string, std::string);
    bool is_valid_username(std::string) {return true;};
};