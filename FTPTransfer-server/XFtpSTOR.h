#pragma once
#include "XFtpTask.h"

class XFtpSTOR : public XFtpTask{
public:
    void Read(bufferevent *);
    void Event(bufferevent *, short);
    void Parse(string, string);

private:
    char buf[1024*1024] = {0};
};