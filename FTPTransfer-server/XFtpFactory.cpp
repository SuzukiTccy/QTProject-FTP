#include "XFtpFactory.h"
#include "XFtpServerCMD.h"
#include "XFtpUSER.h"
#include "XFtpLIST.h"
#include "XFtpPORT.h"
#include "XFtpRETR.h"
#include "XFtpSTOR.h"
#include "XFtpPASS.h"
#include "XFtpTYPE.h"
#include "XFtpAUTH.h"
#include "XFtpPBSZ.h"
#include "XFtpPROT.h"
#include "XFtpREST.h"
#include "XFtpSIZE.h"
#include "testUtil.h"
#include <memory>           // 智能指针

std::shared_ptr<XFtpServerCMD> XFtpFactory::CreateTask(){
    Logger::debug("XFtpFactory::CreateTask()");
    // XFtpServerCMD *cmd = new XFtpServerCMD();
    std::shared_ptr<XFtpServerCMD> cmd = std::make_shared<XFtpServerCMD>();

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

    // SSL相关命令注册
    #ifndef OPENSSL_NO_SSL_INCLUDES
    cmd->Reg("AUTH", new XFtpAUTH());
    cmd->Reg("PBSZ", new XFtpPBSZ());
    cmd->Reg("PROT", new XFtpPROT());
    #endif

    // 断点续传命令注册
    cmd->Reg("REST", new XFtpREST());
    cmd->Reg("SIZE", new XFtpSIZE());

    return cmd;
}
