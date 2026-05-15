#!/bin/sh

# 获取脚本所在目录的绝对路径
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")

# 检查hash_dev驱动是否已加载
if ! lsmod | grep -q "hash_dev"; then
    echo "hash_dev驱动未加载,正在安装..."

    # 尝试安装驱动
    if insmod $SCRIPT_DIR/hash_dev.ko; then
        echo "hash_dev驱动安装成功"
    else
        echo "hash_dev驱动安装失败" >&2
        exit 1
    fi
else
    echo "hash_dev驱动已加载"
fi

txt=$(date +%s%B%h%m%s%A)
str1=$(echo -n $txt | md5sum | awk '{print $1}')
str2=$($SCRIPT_DIR/hash_test md5 $txt)
if [ $str1 != $str2 ]
then
	echo "txt: $txt"
	echo "str1: $str1"
	echo "str2: $str2"
	echo "md5 error!!!!!!!!!!"
	exit 1
else
	echo "txt: $txt"
	echo "str1: $str1"
	echo "str2: $str2"
	echo "md5 pass!!!!!!!!!!"
fi
sleep 0.5
txt="helloWORLD1234567890"
str1="bb49a3f4c6dee893408fdf023132bf22d771e1ba"
str2=$($SCRIPT_DIR/hash_test sha1 $txt)
if [ $str1 != $str2 ]
then
	echo "txt: $txt"
	echo "str1: $str1"
	echo "str2: $str2"
	echo "sha1 error!!!!!!!!!!"
	exit 1
else
	echo "txt: $txt"
	echo "str1: $str1"
	echo "str2: $str2"
	echo "sha1 pass!!!!!!!!!!"
fi
sleep 0.5
txt="helloWORLD1234567890"
str1="cf0779a2503235f49a0514439985bd1117f941198e14a2b161c91ce9"
str2=$($SCRIPT_DIR/hash_test sha224 $txt)
if [ $str1 != $str2 ]
then
	echo "txt: $txt"
	echo "str1: $str1"
	echo "str2: $str2"
	echo "sha224 error!!!!!!!!!!"
	exit 1
else
	echo "txt: $txt"
	echo "str1: $str1"
	echo "str2: $str2"
	echo "sha224 pass!!!!!!!!!!"
fi
sleep 0.5

txt="hello"
str1="2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824"
str2=$($SCRIPT_DIR/hash_test sha256 $txt)
if [ $str1 != $str2 ]
then
	echo "txt: $txt"
	echo "str1: $str1"
	echo "str2: $str2"
	echo "sha256 error!!!!!!!!!!"
	exit 1
else
	echo "txt: $txt"
	echo "str1: $str1"
	echo "str2: $str2"
	echo "sha256 pass!!!!!!!!!!"
fi
sleep 0.5

txt="qwer"
str1="d0139fa4da4f145910b22e9965dba52fa3ddbf62859a229e18e2898a605eed626de5c2ede3018367c79c76efc283c5da"
str2=$($SCRIPT_DIR/hash_test sha384 $txt)
if [ $str1 != $str2 ]
then
	echo "txt: $txt"
	echo "str1: $str1"
	echo "str2: $str2"
	echo "sha384 error!!!!!!!!!!"
	exit 1
else
	echo "txt: $txt"
	echo "str1: $str1"
	echo "str2: $str2"
	echo "sha384 pass!!!!!!!!!!"
fi
sleep 0.5

txt="helloWORLD"
str1="79558c6e83aef11d1aec8e1e9161f8e126a60ecd3bc93a28e000f73e980e1f36662510b9c614e65ca3f1f4f27561d0ba38898cd4f940d24d404a1661cc27bd35"
str2=$($SCRIPT_DIR/hash_test sha512 $txt)
if [ $str1 != $str2 ]
then
	echo "txt: $txt"
	echo "str1: $str1"
	echo "str2: $str2"
	echo "sha512 error!!!!!!!!!!"
	exit 1
else
	echo "txt: $txt"
	echo "str1: $str1"
	echo "str2: $str2"
	echo "sha512 pass!!!!!!!!!!"
fi

exit 0