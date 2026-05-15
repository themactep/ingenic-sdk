#!/bin/sh

#使用前请先在内核中配置好PWM支持，并打开要测试的通道
#使用方法：./pwm_test.sh 0 10000 5000 0
#0表示PWM通道 10000表示周期  5000表示占空比 0表示默认极性(1极性反转) 可以自行更改
#Please configure PWM support in the kernel and open the channel to be tested before use
#Usage:/ pwm_test.sh 0 10000 5000 0
#0 represents PWM channel, 10000 represents cycle, 5000 represents duty cycle, 0 represents default polarity (1 polarity reversal), can be changed by oneself

N=$1
TIME=$2
Duty=$3
Polarity=$4

echo "PWM test start!"
#Export channel, default export channel 0
if [ "$1" ]; then
	echo "PWM $N Test !"
	echo $N > /sys/class/pwm/pwmchip0/export
	cd /sys/class/pwm/pwmchip0/pwm$N
else
	echo "PWM 0 Test !"
	echo 0 > /sys/class/pwm/pwmchip0/export
	cd /sys/class/pwm/pwmchip0/pwm0
fi

#PWM channel selection can be selected from 0-3, default is 0
if [ "$1" ]; then
	echo "PWM $N enable!"
	echo 1 > enable
else
	echo "PWM 0 enable!"
	echo 1 > enable
fi

#POLARITY
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

#Cycle selection, default use 10000 (10us)
if [ "$2" ]; then
	echo "PWM cycle -> $TIME"
	echo $TIME > period
else
	echo "PWM cycle -> 10000"
	echo 10000 > period
fi

#Duty cycle input, default is 5000
if [ "$3" ]; then
	echo "PWM Duty -> $Duty"
	echo $Duty > duty_cycle
else
	echo "PWM Duty -> 5000"
	echo 5000 > duty_cycle
fi

