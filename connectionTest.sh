#!/bin/bash
# 文件名: test_concurrency.sh
# 用法: ./test_concurrency.sh [并发数] [超时秒数]

# 默认参数
CONCURRENCY=${1:-50}      # 默认并发50个连接
TIMEOUT=${2:-10}           # 默认每个连接超时10秒

echo "开始测试 $CONCURRENCY 个并发 FTPS 连接，每个连接超时 $TIMEOUT 秒..."

# 循环启动并发连接
for ((i=1; i<=CONCURRENCY; i++)); do
    lftp \
        -u xb1520,xb1520 \
        -e "set ftp:ssl-force true; \
            set ftp:passive-mode off; \
            set ssl:verify-certificate false; \
            ls; sleep $TIMEOUT; quit" \
        localhost > /dev/null 2>&1 &
    echo "启动第 $i 个连接"
done

# 等待所有后台进程结束（包括被 timeout 强制终止的）
wait

echo "测试完成，所有连接已退出。"
