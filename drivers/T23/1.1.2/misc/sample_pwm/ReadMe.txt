menuconfig配置如下
1.选择PWM依赖的TCU模块
Device Drivers → Multifunction device drivers
	<*> Support for XBurst TCU  //3.10 kernel
 或	[*] Ingenic tcu driver      //4.4 kernel

2.选择通道和使用的GPIO
Device Drivers → Pulse-Width Modulation (PWM) Support
	<*> Ingenic PWM support  //4.4 kernel
 或 <*> JZ PWM driver        //3.10 kernel
		[*] PWM channel0 Enable
				PWM channel0 GPIO select (PWM channel0 PB17)  --->
		[*] PWM channel1 Enable
				PWM channel1 GPIO select (PWM channel1 PB18)  --->
		[ ] PWM channel2 Enable
		[ ] PWM channel3 Enable

----------------------------------------------------------------
使用方法
1.导出 PWM 通道 N:
  echo N > /sys/class/pwm/pwmchip0/export

2.取消导出的 PWM 通道 N:
  echo N > /sys/class/pwm/pwmchip0/unexport

3.导出 PWM 通道 N 后，进入/sys/class/pwm/pwmchip0/pwmN 目录，可以看到一些属性文件，通过他们来控制PWM输出。

enable: 写入"0"表示禁止PWM输出；写入"1"表示使能PWM输出。读取该文件可获取 PWM 当前使能状态。
  echo 0 > enable #禁止 PWM 输出
  echo 1 > enable #使能 PWM 输出

period: 用于配置 PWM 周期。单位ns。
  配置 PWM 周期为 10us:
    echo 10000 > period

duty_cycle: 用于配置 PWM 的占空比(有效电平)时间。单位ns。
  配置 PWM 占空比为 5us：
    echo 5000 > duty_cycle

polarity: 用于设置极性，可读可写，可写入的值如下：
  echo normal > polarity   #默认极性
  echo inversed > polarity #极性反转


启动时PWM驱动会打印出周期(period)的取值范围(单位ns)，如下：
	pwm: period_ns:max=2730666,min=42

注意：
	TCU是硬件定时器模块，T21 T23 T31 T40的PWM或bypass或定时器功能与TCU共享通道，
	通道用作PWM功能后就不能再作为其他功能使用。
	T41的PWM是独立的，不依赖TCU。
