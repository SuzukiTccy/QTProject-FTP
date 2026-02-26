# C++ FTPS Server&Client(基于 libevent + OpenSSL)
<img src="https://github.com/SuzukiTccy/QTProject-FTP/blob/master/image/License-MIT-blue.svg" width="10%" alt="liense">
# C++ FTPS Server (基于 libevent + OpenSSL)

一个高性能、事件驱动的 FTPS (FTP over SSL/TLS) 服务器实现，完全由 C++ 编写。它利用 **libevent** 处理高并发网络 I/O，使用 **OpenSSL** 提供加密通道，并通过线程池模型实现多任务并行处理。

## 特性

- ✅ **完整 FTP 核心命令支持** (RFC 959)
- ✅ **加密控制通道** – `AUTH TLS` / `AUTH SSL`，强制使用 TLS 加密登录及命令传输
- ✅ **加密数据通道** – `PROT P` 保护数据传输，`PROT C` 可选明文
- ✅ **断点续传** – 支持 `REST` 命令，可从指定偏移量继续上传/下载
- ✅ **主动模式 (PORT)** 数据连接
- ✅ **文件列表 (LIST / PWD / CWD / CDUP)**
- ✅ **文件上传 (STOR) / 下载 (RETR)**
- ✅ **获取文件大小 (SIZE)**
- ✅ **多线程线程池** – 主线程负责监听，工作线程独立运行 libevent 事件循环，高效处理并发连接
- ✅ **模块化设计** – 新增 FTP 命令只需继承 `XFtpTask` 并注册即可

## 架构设计

### 整体模块

| 模块              | 职责                                                             |
| --------------- | -------------------------------------------------------------- |
| `main.cpp`      | 初始化 OpenSSL、线程池、创建 TCP 监听器，启动事件循环                              |
| `XThreadPool`   | 管理一组工作线程，采用轮询分发新连接                                             |
| `XThread`       | 每个工作线程拥有独立的 `event_base`，通过管道与主线程通信，处理分配到该线程的客户端连接             |
| `XFtpServerCMD` | 控制连接的任务对象，负责解析 FTP 命令并分发至具体命令处理器                               |
| `XFtpTask` 派生类  | 实现具体 FTP 命令，如 `XFtpLIST`, `XFtpRETR`, `XFtpSTOR`, `XFtpAUTH` 等 |
| `XFtpFactory`   | 工厂类，为每个新连接创建 `XFtpServerCMD` 对象并注册所有命令处理器                      |

### 流程图

#### 1. 服务器启动流程
<img src="https://github.com/SuzukiTccy/QTProject-FTP/blob/master/image/server1.png" width="20%" alt="服务器启动流程">

#### 2. 控制连接命令处理流程
<img src="https://github.com/SuzukiTccy/QTProject-FTP/blob/master/image/server2.svg" width="70%" alt="控制连接命令处理流程">

#### 3. 数据连接建立（PORT 主动模式）
<img src="https://github.com/SuzukiTccy/QTProject-FTP/blob/master/image/server3.svg" width="70%" alt="数据连接建立（PORT 主动模式）">

## 快速开始

### 依赖项

- C++17 编译器
- [libevent](https://libevent.org/) (>= 2.1)
- [OpenSSL](https://www.openssl.org/) (>= 1.1.1)

### 编译

使用Makefile构建：
```bash
git clone https://github.com/SuzukiTccy/QTProject-FTP.git
cd FTPTransfer-server
make -j
```

### 生成自签名证书

FTPS 需要服务器证书和私钥（PEM 格式）。可使用 OpenSSL 快速生成
项目自带generate_cert.sh，可生成证书和私钥
```bash
sh generate_cert.sh
```
将生成的 `server.crt` 和 `server.key` 放置于工作目录。

### 运行
```bash
./ftps_server
```
默认监听 **21** 端口。可通过修改 `main.cpp` 中的 `SPORT` 宏更改。

### 测试

使用支持 TLS 的 FTP 客户端（如 FileZilla、lftp）连接：
```text
主机: 127.0.0.1
端口: 21
协议: FTP over TLS (显式加密)
用户名: 任意 (目前未校验)
密码: 任意
```

使用lftp进行测试：
```bash
# 上传下载文件
lftp -u xb1520 -e 'debug; set ftp:ssl-force true; set ftp:passive-mode off; get 文件名' localhost

lftp -u xb1520 -e 'debug; set ftp:ssl-force true; set ftp:passive-mode off; put 本地文件名' localhost

lftp -u xb1520 -e 'debug; set ftp:ssl-force true; set ssl:verify-certificate true; set ftp:passive-mode off; get 摄影师之眼.pdf' localhost

lftp -u xb1520 -e 'debug; set ftp:ssl-force true; set ssl:verify-certificate true; set ftp:passive-mode off; put 摄影师之眼.pdf' localhost
```

## 已实现的 FTP 命令
| 命令     | 功能       | 备注                          |
| ------ | -------- | --------------------------- |
| `USER` | 用户名      | 框架实现，可扩展认证                  |
| `PASS` | 密码       | 总是成功                        |
| `TYPE` | 传输类型     | 总是成功                        |
| `PORT` | 主动模式端口   | 解析 IP 和端口                   |
| `LIST` | 列表目录     | 支持 `PWD`、`CWD`、`CDUP` 共享处理器 |
| `RETR` | 下载文件     | 支持断点续传                      |
| `STOR` | 上传文件     | 支持断点续传                      |
| `AUTH` | 认证机制     | 支持 `TLS` / `SSL`，切换控制连接到加密  |
| `PBSZ` | 保护缓冲区大小  | 固定响应 `200 PBSZ=0`           |
| `PROT` | 数据通道保护级别 | 支持 `P` (私有) / `C` (明文)      |
| `REST` | 断点续传偏移量  | 设置偏移量，用于后续 `RETR` / `STOR`  |
| `SIZE` | 获取文件大小   | 返回 `213` 响应                 |
| `PWD`  | 打印当前目录   | 由 `XFtpLIST` 处理             |
| `CWD`  | 改变目录     | 由 `XFtpLIST` 处理             |
| `CDUP` | 返回上级目录   | 由 `XFtpLIST` 处理             |

## 配置说明

- **根目录**：默认限制在 `/Users/username/`，可在 `XFtpTask.h` 中修改 `rootDir` 变量。
    
- **线程数**：在 `main.cpp` 中 `XThreadPoolGet->Init(10)` 可调整工作线程数量。
    
- **证书路径**：目前硬编码为 `server.crt` 和 `server.key`，可根据需要修改 `main.cpp` 中的文件名

## 待办 / 已知问题

- 被动模式 (PASV) 尚未实现。
- 目录列表格式为 `ls -la` 输出，不完全符合 FTP 标准格式（但大多数客户端兼容）。
- 未实现 `ABOR` 命令中断传输
- 内存管理可进一步优化（智能指针已部分使用，但仍有原始指针）


# Qt FTPS Client (基于 C++ Qt + ftplib)
一个功能完整的 FTPS (FTP over SSL/TLS) 图形化客户端，使用 **Qt** 框架构建界面，通过 **ftplib** 库进行底层 FTP 通信，支持显式 SSL/TLS 加密、断点续传、多线程传输管理。

## 特性

- ✅ **图形化界面** – 基于 Qt Widgets，操作直观
- ✅ **FTPS 支持** – 显式 TLS/SSL 加密（`AUTH TLS`），兼容自签名证书
- ✅ **主动模式 (PORT)** – 支持标准的主动模式数据连接
- ✅ **文件浏览** – 类似资源管理器的远程文件列表，支持双击进入目录、返回上级
- ✅ **上传/下载** – 支持单个文件上传和下载，支持断点续传（`REST` 命令）
- ✅ **断点续传** – 自动检测远程文件大小，从中断处继续传输；支持暂停/恢复/取消传输任务
- ✅ **多线程传输** – 使用 `QtConcurrent` 实现后台传输，不阻塞 UI
- ✅ **传输队列管理** – 实时显示正在进行的传输及其进度
- ✅ **标准 FTP 命令** – 支持 `LIST`, `CWD`, `CDUP`, `PWD`, `DELE`, `RMD`, `MKD` 等
- ✅ **日志输出** – 控制台带时间戳的日志，便于调试

## 架构设计

### 整体模块

|模块|职责|
|---|---|
|`main.cpp`|应用程序入口，初始化翻译、主窗口|
|`MainWindow`|主界面，负责文件列表显示、菜单交互、传输列表管理|
|`FtpConnectDlg`|连接对话框，收集服务器地址、用户名、密码及 SSL 选项|
|`Ftp`|FTP 核心封装类，基于 `ftplib` 提供高级 API，处理 SSL 协商、断点续传、传输状态回调|
|`ftplib`|第三方 C++ FTP 库，支持 SSL 扩展，提供底层 socket 操作和 FTP 命令实现|

### 流程图

#### 1. 连接与登录流程
<img src="https://github.com/SuzukiTccy/QTProject-FTP/blob/master/image/client1.svg" width="70%" alt="连接与登录流程">

#### 2. 文件下载（支持断点续传）
<img src="https://github.com/SuzukiTccy/QTProject-FTP/blob/master/image/client2.svg" width="70%" alt="文件下载（支持断点续传）">

#### 3. 暂停/恢复传输
<img src="https://github.com/SuzukiTccy/QTProject-FTP/blob/master/image/client3.svg" width="70%" alt="暂停/恢复传输">


## 快速开始

### 依赖项

- Qt 5.15 或更高版本（推荐 Qt 6）
- OpenSSL 1.1.1 或更高版本（用于 SSL 支持）
- 编译器：支持 C++17 的编译器（GCC, Clang, MSVC）

### 编译

项目使用 qmake 构建，若使用 Qt Creator，直接打开 `.pro` 文件并构建即可。

### 运行

确保 OpenSSL 库在系统路径中。在 macOS/Linux 上可能需要设置 `LD_LIBRARY_PATH` 或安装到标准位置。
```bash
./FTPTransFer-client
```

## 使用指南

1. **连接服务器**  
    点击菜单栏“连接” -> “连接”，在弹出的对话框中输入：
    
    - Host: 服务器地址（例如 `127.0.0.1`）
        
    - User / Pass: 用户名和密码（服务端暂未验证）
        
    - SSL 连接：勾选以启用 FTPS（显式 TLS）
        
2. **浏览文件**  
    成功连接后，远程文件列表将显示在“文件列表”区域。双击目录可进入，双击文件无操作（暂未实现直接打开）。
    
3. **上传文件**  
    在文件列表空白处右键 -> “上传”，选择本地文件，文件将上传到当前远程目录。
    
4. **下载文件**  
    选中一个文件，右键 -> “下载”，选择本地保存路径，文件即开始下载。
    
5. **断点续传**  
    如果下载/上传过程中中断（网络断开或手动暂停），重新执行相同操作，客户端会自动检测本地/远程文件大小，并尝试从断点继续（需服务器支持 `REST` 命令）。
    
6. **删除文件/目录**  
    选中文件或目录，右键 -> “删除”。目录非空时可能删除失败。
    
7. **查看传输状态**  
    下方的“传输列表”会显示正在进行的任务，包括文件名、进度和状态（传输中/完成）。


## 项目结构
```text
├── ftp.h / ftp.cpp          # FTP 核心封装类，处理 SSL、断点续传、传输状态
├── ftpconnectdlg.h / .cpp   # 连接对话框
├── ftplib.h / ftplib.cpp    # 第三方 ftplib 库（含 SSL 扩展）
├── main.cpp                 # 应用程序入口
├── mainwindow.h / .cpp      # 主窗口，UI 逻辑
├── mainwindow.ui             # 主窗口 UI 设计文件
├── FTPTransFer-client.pro    # qmake 项目文件
└── README.md                 # 本文档
```

## 待办 / 已知问题

- **被动模式 (PASV)** 尚未实现，目前仅支持主动模式（PORT）。
- **暂停/恢复功能** 目前仅为状态标记，实际中断传输需修改 ftplib 以支持可中断的 I/O。
- **多文件传输**（队列）尚未实现，目前一次只能上传/下载一个文件。
- **目录上传/下载** 暂不支持
- **UI 细节**：传输列表的进度显示尚不完善；右键菜单可能缺少某些操作
- **证书验证**：目前不对服务器证书进行验证（适用于自签名证书），生产环境应增强验证

# 贡献

欢迎任何形式的贡献！无论是新功能、bug 修复、文档改进，都请提交 issue 或 pull request。在开发前请确保代码遵循现有风格，并通过基础测试。

1. Fork 本仓库
2. 创建您的特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交您的更改 (`git commit -m 'Add some amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 打开一个 Pull Request

# 许可证

本项目基于 MIT 许可证开源，详情请见 [LICENSE](https://license/) 文件。

---

**注意**：本项目为学习与实验目的编写，生产环境使用前请进行充分测试和安全加固
