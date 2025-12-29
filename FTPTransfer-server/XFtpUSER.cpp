#include "XFtpUSER.h"
#include "testUtil.h"

void XFtpUSER::Parse(std::string cmd, std::string username){
    // 1. 验证用户名格式
    // 2. 检查用户名是否有效
    // 3. 设置会话状态（等待PASS命令）
    // 4. 发送响应：
    //    - 成功："331 User name okay, need password."
    //    - 失败："530 Invalid username."

    if (is_valid_username(username)){
        ResCMD("331 User name okay, need password.\r\n");
    }
    else{
        ResCMD("530 Invalid username.\r\n");
    }
}

