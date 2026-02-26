# C++ FTPS Server (基于 libevent + OpenSSL)

[](https://license/)[https://img.shields.io/badge/License-MIT-blue.svg](https://img.shields.io/badge/License-MIT-blue.svg)  
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
```graph TD
    A[main] --> B[初始化 OpenSSL]
    B --> C[创建线程池 XThreadPool::Init]
    C --> D[创建监听器 evconnlistener]
    D --> E[进入事件循环 event_base_dispatch]
    E --> F{收到新连接?}
    F -->|是| G[调用 listen_cb]
    G --> H[通过工厂创建 XFtpServerCMD 任务]
    H --> I[线程池 Dispatch 到某个工作线程]
    I --> J[工作线程的管道写 'c' 唤醒]
    J --> K[工作线程执行任务 Init]
```

#### 2. 控制连接命令处理流程
```cpp
sequenceDiagram
    participant Client
    participant WorkerThread
    participant XFtpServerCMD
    participant XFtpTask

    Client->>WorkerThread: 发送命令 (如 "LIST\r\n")
    WorkerThread->>XFtpServerCMD: Read 回调
    XFtpServerCMD->>XFtpServerCMD: 累积数据，提取完整命令
    XFtpServerCMD->>XftpTask: 根据命令类型查找处理器
    XftpTask->>XftpTask: 执行 Parse 逻辑
    XftpTask->>XFtpServerCMD: 调用 ResCMD 发送响应
    XFtpServerCMD-->>Client: 响应
```

#### 3. 数据连接建立（PORT 主动模式）
```
sequenceDiagram
    participant Client
    participant XFtpTask (e.g. LIST)
    participant DataConnection

    Client->>XFtpTask: PORT 命令 (包含IP和端口)
    XFtpTask->>XFtpTask: 解析IP和端口
    XFtpTask->>DataConnection: 调用 ConnectoPORT()
    DataConnection->>DataConnection: 创建新 socket 连接客户端指定端口
    DataConnection-->>Client: 建立 TCP 连接
    Note over DataConnection,Client: 若启用 SSL，此时进行握手
    DataConnection->>DataConnection: 触发 CONNECTED 事件
    DataConnection->>Client: 发送数据 (或接收)
    DataConnection-->>XFtpTask: 传输完成，关闭
```

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

## 贡献

欢迎任何形式的贡献！无论是新功能、bug 修复、文档改进，都请提交 issue 或 pull request。在开发前请确保代码遵循现有风格，并通过基础测试。

1. Fork 本仓库
2. 创建您的特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交您的更改 (`git commit -m 'Add some amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 打开一个 Pull Request

## 许可证

本项目基于 MIT 许可证开源，详情请见 [LICENSE](https://license/) 文件。

---

**注意**：本项目为学习与实验目的编写，生产环境使用前请进行充分测试和安全加固