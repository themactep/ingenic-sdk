硬件定时器驱动。

T31 T23有4路，T40 T41有8路硬件定时器，具体使用那一路，配置下面字符串(tcu_chn*)：
	{ .compatible = "ingenic,tcu_chn3", } //4.4 kernel
	.name = "tcu_chn2",                   //3.10 kernel

应用层代码参考 jz_timer.c

编译驱动：make
编译timer_test：make sample_timer
