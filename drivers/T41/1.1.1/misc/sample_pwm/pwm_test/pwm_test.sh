#!/bin/sh

#使用前请先在内核中配置好PWM支持，并打开要测试的通道
#使用方法：./pwm_test.sh 0 10000 5000 0
#0表示PWM通道 10000表示周期  5000表示占空比 0表示默认极性(1极性反转) 可以自行更改


N=$1
TIME=$2
Duty=$3
Polarity=$4

echo "PWM test start!"
#导出通道，默认导出通道0
if [ "$1" ]; then
	echo "PWM $N Test !"
	echo $N > /sys/class/pwm/pwmchip0/export
	cd /sys/class/pwm/pwmchip0/pwm$N
else
	echo "PWM 0 Test !"
	echo 0 > /sys/class/pwm/pwmchip0/export
	cd /sys/class/pwm/pwmchip0/pwm0
fi

#PWM通道选择 可选1-7，默认使用0
if [ "$1" ]; then
	echo "PWM $N enable!"
	echo 1 > enable
else
	echo "PWM 0 enable!"
	echo 1 > enable
fi

#极性选择
if [ "$1" ]; then
	if [ "$4" = "0" ]; then
	echo "polarity is normal "
	echo normal > polarity
	elif [ "$4" = "1"]; then
	echo "polarity is inversed"
	echo inversed > polarity
	else
	echo "Use last configuration "
	fi
else
	echo "polarity default"
	echo normal > polarity
fi

#周期选择，默认使用10000（10us）
if [ "$2" ]; then
	echo "PWM cycle -> $TIME"
	echo $TIME > period
else
	echo "PWM cycle -> 10000"
	echo 10000 > period
fi

#占空比输入，默认使用5000
if [ "$3" ]; then
	echo "PWM Duty -> $Duty"
	echo $Duty > duty_cycle
else
	echo "PWM Duty -> 5000"
	echo 5000 > duty_cycle
fi
























