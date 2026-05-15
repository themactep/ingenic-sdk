#ifndef __TMI8152_SPI_DEV_H__
#define __TMI8152_SPI_DEV_H__

#define MOTOR_DRV_OC    0x0 //通道输出开路
#define MOTOR_DRV_CW    0x5 //顺时针转
#define MOTOR_DRV_CCW   0xa //逆时针转
#define MOTOR_DRV_BRAKE 0xf //制动；停止；刹车

enum
{
    TMI8152_CDIV_1,   // 不分频
    TMI8152_CDIV_2,   // sysclk2 分频
    TMI8152_CDIV_4,   // sysclk4 分频
    TMI8152_CDIV_8,   // sysclk8 分频
    TMI8152_CDIV_16,  // sysclk16 分频
    TMI8152_CDIV_32,  // sysclk32 分频
    TMI8152_CDIV_64,  // sysclk64 分频
    TMI8152_CDIV_128, // sysclk128 分频
}clockchn_source;

enum //注： 安防IPC电机在PWM频率低于16kHz后， 电磁噪音会逐渐增大， 所以建议设置分频数小于等于7
{
    TMI8152_CSEL_1, // chdivclk
    TMI8152_CSEL_2, // chdivclk/2
    TMI8152_CSEL_3, // chdivclk/3
    TMI8152_CSEL_4, // chdivclk/4
    TMI8152_CSEL_5, // chdivclk/5
    TMI8152_CSEL_6, // chdivclk/6
    TMI8152_CSEL_7, // chdivclk/7
    TMI8152_CSEL_8, // chdivclk/8
    TMI8152_CSEL_9, // chdivclk/9
}clk_csel;

enum
{
    TMI8152_STEPMD_1,   // 4 步  1 细分
    TMI8152_STEPMD_2,   // 8 步  2 细分
    TMI8152_STEPMD_4,   // 16 步 4 细分
    TMI8152_STEPMD_8,   // 32 步 8 细分
    TMI8152_STEPMD_16,  // 64 步 16 细分
    TMI8152_STEPMD_32,  // 128 步 32 细分
    TMI8152_STEPMD_64,  // 256 步 64 细分
    TMI8152_STEPMD_128, // 512 步 128 细分
};

typedef enum motornum {
    MOTOR_DFT,
    MOTOR_1,
    MOTOR_2,
    MOTOR_BUTT,
}motor_num;

typedef enum {
    MOTOR_AUTO,
    MOTOR_MANUAL,
}motor_contrl;

struct stMotorConfig
{
    unsigned char ch_stepmd; //细分系数
    unsigned char rotation_dir; // 旋转方向;前进模式
    unsigned char sysclk_div; //通道时钟源分频
    unsigned char clk_csel;   //通道时钟分频
    unsigned char control_mode;   //控制模式,目前只支持手动模式
    unsigned char phset;       //相位设置
    unsigned short circle_set;    //要运行的圈数
};

struct spi_motor_platform_data
{
    char *name;
    int id;
};

//应用层设置参数
typedef struct spi_param
{
    motor_num motor_num; //哪一个电机，目前只支持两个电机
    struct stMotorConfig motor_config;
}spi_motor_param_t;

#define  SPI_MOTOR_STOP _IOWR('m', 1, spi_motor_param_t)
#define  SPI_MOTOR_RUN _IOWR('m', 2, spi_motor_param_t)
#define  SPI_MOTOR_PAUSE _IOWR('m', 3, spi_motor_param_t)
#define  SPI_MOTOR_RESET _IOWR('m', 4, spi_motor_param_t)
#define  SPI_MOTOR_IRCUT _IOWR('m', 5, spi_motor_param_t)

#endif /*__ingenic_SPI_DEV_H__*/
