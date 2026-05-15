
bus_monitor测试程序说明及使用


1、这个测试程序是进行bus_monitor的相关功能测试。

2、命令：./bus_monitor_test  1 2 3 4
    注：
    argv[1] 为使用的monitor的模式
        0   watch addr模式
        1   watch id模式
        2   monitor addr and id 模式
        3   monitor vals模式
        4   monitor valm模式

    argv[2] 为watch monitor的通道

    argv[3] 为使用的monitor ID


    argv[4] 为vals模式的功能


    bus_monitor_test    程序生成的可执行文件
    1                   monitor的模式，有0,1,2,3,4的值可以选择
    2                   对应的通道号，axi使用0,1,2,3,4,6通道，ahb使用channel 5
    3                   对应monitor功能的id号（十进制）
    4                   为vals模式的一些功能，不使用vlas可配置0

    测试代码包含一个驱动和测试程序
    测试程序test bus_monitor_test.c bus_monitor_test.h Makefile
    生成可执行文件    bus_monitor_test

#####################################################################################################
AXI (channel 0,1,2,3,4,6)
name （没有写id号的就说明此通道只有它一个主机，id号就是0）

格式：channel名+id中第几位+id号分配

ch0 [6:5]
0：
1：lcdc
2: ldc

ch1[4]
0：AIP

ch2 [5]
0:isp
1:ivdc

ch3 [7:5]
0:ipu
1:msc0
2:msc1
3:i2d
4:drawbox

ch4 [7:6]
0:jiloo
1:el
2:ivdc

ch6 [4]
0:cpu

##############################################################################################################################
