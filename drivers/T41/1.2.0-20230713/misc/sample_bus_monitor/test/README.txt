bus_monitor测试程序说明

1、这个测试程序是进行bus_monitor的相关功能测试，在使用前需要将对应的.ko文件进行insmod。
2、命令：./bus_monitor_test  1 2 3 4
    注：
    argv[1] 为使用的monitor的模式，有0,1,2,3,4的值可以选择。
        0   watch addr模式
        1   watch id模式
        2   monitor addr and id 模式
        3   monitor vals模式
        4   monitor valm模式

    argv[2] 为watch monitor的通道，axi使用0,1,2,3,4,6通道，ahb使用channel 5。

    argv[3] 为使用的monitor ID，对应monitor功能的id号（十进制），在.h文件中可以找到。

    argv[4] 为vals模式的功能，为vals模式的一些功能，不使用vlas可配置0。

####################################################################################
AXI 有0,1,2,3,4,6的channel
获取name的格式：channel名+id中第几位+id号分配
（没有写id号的就说明此通道只有它一个主机，id号就是0）

ch0 [6:5]
0：ov    1：lcdc    2: ldc

ch1[4]
0：AIP

ch2 [5]
0:isp    1:ivdc

ch3 [7:5]
0:ipu    1:msc0    2:msc1    3:i2d    4:drawbox

ch4 [7:6]
0:jilo   1:el      2:ivdc

ch6 [4]
0:cpu

AXI 有5,7的channel
ch5:
0：usb  1：dmic  2：aes  3：sfc0  4：gmac  5：ahb_bri  6：hash  7：sfc1  8：pwm  9：pdma

ch7:
Cpu_lep


####################################################################################
