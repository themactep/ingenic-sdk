
bus_monitor测试程序说明及使用


1、这个测试程序是进行bus_monitor的相关功能测试。

2、命令：./bus_monitor_test 2 2 64
    注：
    argv[1] 为使用的monitor的模式
       axi：
        0   watch addr模式
        1   watch id模式
        2   monitor addr 模式
        3   monitor id模式
      ahb:
        0   watch addr模式
        1   watch  id模式

    argv[2] 为watch或者monitor的通道

id 分配表(对应相应的寄存的ID位，有的通道只有一个function，不需要配置id)
#####################################################################################################
AHB:
    AHB2  (channel 5) (master号)
        name        master
        M_OTG       4'h0
        //M_UHC     4'h1  ("//的意思是定义的有参数，但是没有用到或者更改了总线")
        M_AES       4'h2
        M_SFC       4'h3
        M_ETH       4'h4
        M_CPU       4'h5
        M_HASH      4'h6
        //M_MSC0    4'h7
        //M_MSCX    4'h8
        M_PDMA      4'h9
        //M_MCU     4'ha

AXI:
AHB0 (channel 0,1,2,3,4,6)

    ch0
        tiziano

    ch1
        radix

    ch2 [6:4] （id 号）
        000 ipu
        001 msc0
        010 msc1
        011 i2d
        100 drawbox

    ch3 [5:4]
        00 bsclaer
        01 lcd
        10 vo
        11 ---

    ch4
        el150

    ch6
        cpu

##############################################################################################################################

    这上面的argv[1] argv[2]分别是代表monitor的模式和检测的通道。

    bus_monitor_test    程序生成的可执行文件
    2                   monitor的模式，有0,1,2,3的值可以选择
    2                   对应的通道号，axi使用0,1,2,3,4,6通道，ahb使用channel 5
    64                  对应monitor功能的id号（十进制）


    测试代码包含一个驱动和测试程序
    测试程序test bus_monitor_test.c bus_monitor_test.h Makefile
    生成可执行文件    bus_monitor_test

