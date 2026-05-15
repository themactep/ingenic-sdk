#!/bin/sh

# 获取脚本所在目录的绝对路径
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")

# 运行gpio_check程序
$SCRIPT_DIR/rsa_test RSA1024

# 检查上一个命令的返回值
if [ $? -eq 0 ]; then
    exit 0
else
    exit 1
fi

$SCRIPT_DIR/rsa_test RSA2048

# 检查上一个命令的返回值
if [ $? -eq 0 ]; then
    exit 0
else
    exit 1
fi
