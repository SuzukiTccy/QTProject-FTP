#include "XFtpFactory.h"
#include "XFtpServerCMD.h"
#include "XFtpUSER.h"
#include "XFtpLIST.h"
#include "XFtpPORT.h"
#include "XFtpRETR.h"
#include "XFtpSTOR.h"
#include "XFtpPASS.h"
#include "XFtpTYPE.h"
#include "testUtil.h"
#include <memory>           // 智能指针

XTask *XFtpFactory::CreateTask(){
    Logger::debug("XFtpFactory::CreateTask()");
    XFtpServerCMD *cmd = new XFtpServerCMD();

    cmd->Reg("USER", new XFtpUSER());
    cmd->Reg("PORT", new XFtpPORT());
    cmd->Reg("RETR", new XFtpRETR());
    cmd->Reg("STOR", new XFtpSTOR());
    cmd->Reg("PASS", new XFtpPASS());
    cmd->Reg("TYPE", new XFtpTYPE());

    XFtpTask *xftplist = new XFtpLIST();
    cmd->Reg("LIST", xftplist);
    cmd->Reg("PWD", xftplist);
    cmd->Reg("CWD", xftplist);
    cmd->Reg("CDUP", xftplist);

    return cmd;
}
