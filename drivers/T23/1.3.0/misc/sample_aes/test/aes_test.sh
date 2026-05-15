#!/bin/sh
# 获取脚本所在目录的绝对路径
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")

# 检查aes_dev驱动是否已加载
if ! lsmod | grep -q "aes_dev"; then
    echo "aes_dev驱动未加载,正在安装..."
    # 尝试安装驱动
    if insmod $SCRIPT_DIR/aes_dev.ko; then
        echo "aes_dev驱动安装成功"
    else
        echo "aes_dev驱动安装失败" >&2
        exit 1
    fi
else
    echo "aes_dev驱动已加载"
fi


# 运行aes_test程序
$SCRIPT_DIR/aes_test

# 检查上一个命令的返回值
if [ $? -eq 0 ]; then
    exit 0
else
    exit 1
fi

exit 0