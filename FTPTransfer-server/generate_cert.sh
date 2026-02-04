#!/bin/bash

# 1. 生成CA私钥
openssl genrsa -out my_ca.key 2048
# 2. 生成CA根证书（有效期10年）
openssl req -x509 -new -nodes -key my_ca.key -sha256 -days 3650 -out my_ca.crt


# 生成私钥
openssl genrsa -out server.key 2048

# 生成证书签名请求（CSR）
openssl req -new -key server.key -out server.csr \
-subj "/C=US/ST=California/L=San Francisco/O=MyCompany/OU=IT Department/CN=localhost"

# 使用CA根证书签发服务器证书
openssl x509 -req -in server.csr -CA my_ca.crt -CAkey my_ca.key \
-CAcreateserial -out server.crt -days 365 -sha256

# 清理 CSR 文件
rm server.csr

echo "证书生成完成:"
echo "  - server.key: 私钥"
echo "  - server.crt: 证书"