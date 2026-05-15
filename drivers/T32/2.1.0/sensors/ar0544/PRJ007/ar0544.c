/*
 * ar0544.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps		dvdd
 *  0            2592*1944      30        mipi_2lane    linear  10        30
 *
 * @I2C addr:0x30, 0x32
 *
 * @FSync:none
 *
 */
#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <soc/gpio.h>

#include <tx-isp-common.h>
#include <sensor-common.h>
#include <txx-funcs.h>
#include <tx-isp-fast.h>

#define TVERSION "V20241105a"
#define SENSOR_VERSION  "H20260127a"

/* #define SENSOR_TEST */

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
/* #define SENSOR_HCG */
#define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x0481)
#define SENSOR_OUTPUT_MIN_FPS 1
#define SENSOR_MCLK 24000000

#ifdef CONFIG_ZERATUL
#define ZRT_SENSOR_WITHOUT_INIT
#endif

#ifdef SENSOR_WDR_2_FRAME
static int wdr_line = xxx;
#endif

#ifndef SENSOR_I2C_REG_8BIT
#define SENSOR_I2C_REG_16BIT
#endif /* SENSOR_I2C_REG_8BIT */
#ifdef SENSOR_I2C_REG_8BIT
#define SENSOR_REG_END    0xff
#define SENSOR_REG_DELAY  0xfe
#endif /* SENSOR_I2C_REG_8BIT */
#ifdef SENSOR_I2C_REG_16BIT
#define SENSOR_REG_END    0xffff
#define SENSOR_REG_DELAY  0xfffe
#endif /* SENSOR_I2C_REG_16BIT */

struct regval_list {
#ifdef SENSOR_I2C_REG_8BIT
    uint8_t reg_num;
#endif /* SENSOR_I2C_REG_8BIT */
#ifdef SENSOR_I2C_REG_16BIT
    uint16_t reg_num;
#endif /* SENSOR_I2C_REG_16BIT */
    uint16_t value;
    int len;
};

struct tx_isp_sensor_attribute ar0544_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
    unsigned int value;
    unsigned int gain;
};

//todo
struct again_lut ar0544_again_lut[] = {
	{0,0},
	{1,4082},
	{2,8164},
	{3,12246},
	{4,16328},
	{5,20410},
	{6,24492},
	{7,28574},
	{8,32612},
	{9,36694},
	{10,40776},
	{11,44858},
	{12,48940},
	{13,53022},
	{14,57104},
	{15,61186},
	{16,65225},
	{17,69307},
	{18,73389},
	{19,77471},
	{20,81553},
	{21,85635},
	{22,89717},
	{23,93799},
	{24,97619},
	{25,101701},
	{26,105783},
	{27,109865},
	{28,113947},
	{29,118029},
	{30,122111},
	{31,126193},
	{32,130406},
	{33,134488},
	{34,138570},
	{35,142652},
	{36,146734},
	{37,150816},
	{38,154898},
	{39,158980},
	{40,163236},
	{41,167318},
	{42,171400},
	{43,175482},
	{44,179564},
	{45,183646},
	{46,187728},
	{47,191810},
	{48,195641},
	{49,199723},
	{50,203805},
	{51,207887},
	{52,211969},
	{53,216051},
	{54,220133},
	{55,224215},
	{56,228341},
	{57,232423},
	{58,236505},
	{59,240587},
	{60,244669},
	{61,248751},
	{62,252833},
	{63,256915},
	{64,261040},
	{65,265122},
	{66,269204},
	{67,273286},
	{68,277368},
	{69,281450},
	{70,285532},
	{71,289614},
	{72,293598},
	{73,297680},
	{74,301762},
	{75,305844},
	{76,309926},
	{77,314008},
	{78,318090},
	{79,322172},
	{80,326363},
	{81,330445},
	{82,334527},
	{83,338609},
	{84,342691},
	{85,346773},
	{86,350855},
	{87,354937},
	{88,359073},
	{89,363155},
	{90,367237},
	{91,371319},
	{92,375401},
	{93,379483},
	{94,383565},
	{95,387647},
	{96,391762},
	{97,395844},
	{98,399926},
	{99,404008},
	{100,408090},
	{101,412172},
	{102,416254},
	{103,420336},
	{104,424516},
	{105,428598},
	{106,432680},
	{107,436762},
	{108,440844},
	{109,444926},
	{110,449007},
	{111,453089},
	{112,457161},
	{113,461243},
	{114,465325},
	{115,469407},
	{116,473489},
	{117,477571},
	{118,481652},
	{119,485734},
	{120,489806},
	{121,493888},
	{122,497970},
	{123,502052},
	{124,506134},
	{125,510215},
	{126,514297},
	{127,518379},
	{128,522461},
	{129,526543},
	{130,530625},
	{131,534707},
	{132,538789},
	{133,542871},
	{134,546953},
	{135,551035},
	{136,555150},
	{137,559232},
	{138,563314},
	{139,567396},
	{140,571478},
	{141,575560},
	{142,579642},
	{143,583724},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int ar0544_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
    /* Analog gain table */
    struct again_lut *lut = ar0544_again_lut;
    while (lut->gain <= ar0544_attr.again.max) {
        if (isp_gain == 0) {
            *sensor_again = lut[0].value;
            return 0;
        } else if (isp_gain < lut->gain) {
            *sensor_again = (lut - 1)->value;
            return (lut - 1)->gain;
        } else {
            if ((lut->gain == ar0544_attr.again.max) && (isp_gain >= lut->gain)) {
                *sensor_again = lut->value;
                return lut->gain;
            }
        }

        lut++;
    }

#else
    /* Non analog gain table */
    ...;
#endif  /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

    return isp_gain;
}

#ifdef SENSOR_WDR_2_FRAME
unsigned int ar0544_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
    /* Analog gain table */
    struct again_lut *lut = ar0544_again_lut;
    while(lut->gain <= ar0544_attr.again_short.max) {
        if(isp_gain == 0) {
            *sensor_again = 0;
            return 0;
        }
        else if(isp_gain < lut->gain) {
            *sensor_again = (lut - 1)->value;
            return (lut - 1)->gain;
        }
        else{
            if((lut->gain == ar0544_attr.again_short.max) && (isp_gain >= lut->gain)) {
                *sensor_again = lut->value;
                return lut->gain;
            }
        }

        lut++;
    }

    ...;
#else
    /* Non analog gain table */

    ...;
#endif /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

    return isp_gain;
}
#endif /* SENSOR_WDR_2_FRAME */

unsigned int ar0544_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
    return 0;
}

#ifdef SENSOR_HCG
unsigned int ar0544_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

    return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int ar0544_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

    return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus ar0544_mipi_linear = {
    .mode = SENSOR_MIPI_OTHER_MODE,
    .clk = 800,
    .lans = 2,
    .image_twidth = 2592,
    .image_theight = 1944,
    .mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
    .mipi_sc.hcrop_diff_en = 0,
    .mipi_sc.mipi_vcomp_en = 0,
    .mipi_sc.mipi_hcomp_en = 0,
    .mipi_sc.mipi_crop_start0x = 0,
    .mipi_sc.mipi_crop_start0y = 0,
    .mipi_sc.mipi_crop_start1x = 0,
    .mipi_sc.mipi_crop_start1y = 0,
    .mipi_sc.mipi_crop_start2x = 0,
    .mipi_sc.mipi_crop_start2y = 0,
    .mipi_sc.mipi_crop_start3x = 0,
    .mipi_sc.mipi_crop_start3y = 0,
    .mipi_sc.line_sync_mode = 0,
    .mipi_sc.work_start_flag = 0,
    .mipi_sc.data_type_en = 0,
    .mipi_sc.data_type_value = RAW10,
    .mipi_sc.del_start = 0,
    .mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
    .mipi_sc.sensor_fid_mode = 0,
    .mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_dvp_bus ar0544_dvp = {
    .gpio = DVP_PA_LOW_10BIT,
    .mode = SENSOR_DVP_HREF_MODE,
    .blanking = {
        .hblanking = 0,
        .vblanking = 0,
    },
    .polar = {
        .hsync_polar = 0,
        .vsync_polar = 0,
        .pclk_polar = 0,
    },
    .dvp_hcomp_en = 0,
};

struct tx_isp_sensor_attribute ar0544_attr = {
    .name = "ar0544",
    .chip_id = 0x4010,//todo
    .cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
    .cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
    .cbus_device = 0x10,//todo
    .sensor_ctrl.alloc_again = ar0544_alloc_again,
    .sensor_ctrl.alloc_dgain = ar0544_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
    .sensor_ctrl.alloc_again_short = ar0544_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

    .dgain = {0},
    .dgain_short = {0},
};

static struct regval_list ar0544_init_regs_2592_1944_30fps_mipi[] = {
    {0x0103, 0x01, 1},
    {0x0304, 0x0003, 2},
    {0x0306, 0x0113, 2},
    {0x0302, 0x0002, 2},
    {0x0300, 0x000A, 2},
    {0x030C, 0x000C, 2},
    {0x030E, 0x01F9, 2},
    {0x030A, 0x0002, 2},
    {0x0308, 0x0005, 2},
    {0x0112, 0x0A0A, 2},
    {0x0114, 0x01, 1},
    {0x0344, 0x000A, 2},
    {0x0348, 0x0A25, 2},
    {0x0346, 0x0008, 2},
    {0x034A, 0x079F, 2},
    {0x0380, 0x0001, 2},
    {0x0382, 0x0001, 2},
    {0x0384, 0x0001, 2},
    {0x0386, 0x0001, 2},
    {0x0900, 0x00, 1},
    {0x0901, 0x11, 1},
    {0x3040, 0x4010, 2},
    {0x034C, 0x0A20, 2},
    {0x034E, 0x0798, 2},
    {0x0800, 0x0C, 1},
    {0x0801, 0x06, 1},
    {0x0802, 0x0A, 1},
    {0x0803, 0x08, 1},
    {0x0804, 0x09, 1},
    {0x0805, 0x05, 1},
    {0x0806, 0x1F, 1},
    {0x0807, 0x07, 1},
    {0x082A, 0x0D, 1},
    {0x082B, 0x0A, 1},
    {0x082C, 0x0C, 1},
    {0x3F06, 0x00C0, 2},
    {0x3F0A, 0xA000, 2},
    {0x3F0A, 0xA000, 2},
    {0x3F0A, 0x2000, 2},
    {0x3F0C, 0x0008, 2},
    {0x3F1E, 0x200E, 2},
    {0x3F20, 0x09C0, 2},
    {0x3F3A, 0x0000, 2},
    {0x3F3C, 0x0000, 2},
    {0x0220, 0x30, 1},
    {0x0342, 0x1884, 2},
    {0x0340, 0x0920, 2},
    {0x0202, 0x0118, 2},
    {0x3100, 0x0000, 2},
    {0x4000, 0x011A, 2},
    {0x4002, 0x2028, 2},
    {0x4004, 0x42FF, 2},
    {0x4006, 0xFFFF, 2},
    {0x4008, 0x1300, 2},
    {0x400A, 0x9FB7, 2},
    {0x400C, 0xF000, 2},
    {0x400E, 0x9192, 2},
    {0x4010, 0xF006, 2},
    {0x4012, 0x1300, 2},
    {0x4014, 0xF012, 2},
    {0x4016, 0x9FF0, 2},
    {0x4018, 0x0EB7, 2},
    {0x401A, 0x0810, 2},
    {0x401C, 0x0225, 2},
    {0x401E, 0x108F, 2},
    {0x4020, 0x3003, 2},
    {0x4022, 0x97F0, 2},
    {0x4024, 0x0030, 2},
    {0x4026, 0xD8F0, 2},
    {0x4028, 0x059A, 2},
    {0x402A, 0xF001, 2},
    {0x402C, 0x99F0, 2},
    {0x402E, 0x0085, 2},
    {0x4030, 0xF003, 2},
    {0x4032, 0x8B89, 2},
    {0x4034, 0xF001, 2},
    {0x4036, 0x30C0, 2},
    {0x4038, 0xF002, 2},
    {0x403A, 0x9C82, 2},
    {0x403C, 0x3018, 2},
    {0x403E, 0x8BB1, 2},
    {0x4040, 0xB6F0, 2},
    {0x4042, 0x0121, 2},
    {0x4044, 0x58F0, 2},
    {0x4046, 0x0A99, 2},
    {0x4048, 0xF001, 2},
    {0x404A, 0x98F0, 2},
    {0x404C, 0x00A2, 2},
    {0x404E, 0xF002, 2},
    {0x4050, 0xA296, 2},
    {0x4052, 0xB4F0, 2},
    {0x4054, 0x009D, 2},
    {0x4056, 0xF002, 2},
    {0x4058, 0xA1F0, 2},
    {0x405A, 0x18A1, 2},
    {0x405C, 0xF003, 2},
    {0x405E, 0x9D8B, 2},
    {0x4060, 0x1009, 2},
    {0x4062, 0x83F0, 2},
    {0x4064, 0x0088, 2},
    {0x4066, 0xF001, 2},
    {0x4068, 0x3600, 2},
    {0x406A, 0xF001, 2},
    {0x406C, 0x9088, 2},
    {0x406E, 0xF003, 2},
    {0x4070, 0x3600, 2},
    {0x4072, 0x83F0, 2},
    {0x4074, 0x0D8B, 2},
    {0x4076, 0xF00E, 2},
    {0x4078, 0xA3F0, 2},
    {0x407A, 0x02A3, 2},
    {0x407C, 0xF002, 2},
    {0x407E, 0x9DF0, 2},
    {0x4080, 0x03A1, 2},
    {0x4082, 0xF018, 2},
    {0x4084, 0xA1F0, 2},
    {0x4086, 0x3621, 2},
    {0x4088, 0xED40, 2},
    {0x408A, 0xD21F, 2},
    {0x408C, 0xF6F0, 2},
    {0x408E, 0x0030, 2},
    {0x4090, 0x03F0, 2},
    {0x4092, 0x0084, 2},
    {0x4094, 0x8BF0, 2},
    {0x4096, 0x0486, 2},
    {0x4098, 0xF000, 2},
    {0x409A, 0x86F0, 2},
    {0x409C, 0x0380, 2},
    {0x409E, 0x8082, 2},
    {0x40A0, 0x0288, 2},
    {0x40A2, 0x30C0, 2},
    {0x40A4, 0x3600, 2},
    {0x40A6, 0xF00C, 2},
    {0x40A8, 0x3600, 2},
    {0x40AA, 0x30C0, 2},
    {0x40AC, 0x0288, 2},
    {0x40AE, 0x8280, 2},
    {0x40B0, 0x8082, 2},
    {0x40B2, 0x0288, 2},
    {0x40B4, 0x30C0, 2},
    {0x40B6, 0x3600, 2},
    {0x40B8, 0xF00C, 2},
    {0x40BA, 0x3600, 2},
    {0x40BC, 0x30C0, 2},
    {0x40BE, 0x0288, 2},
    {0x40C0, 0x8280, 2},
    {0x40C2, 0xF04D, 2},
    {0x40C4, 0x1300, 2},
    {0x40C6, 0x9FB7, 2},
    {0x40C8, 0xE0E0, 2},
    {0x40CA, 0xE0E0, 2},
    {0x40CC, 0xE0E0, 2},
    {0x40CE, 0xE0E0, 2},
    {0x40D0, 0xF000, 2},
    {0x40D2, 0x0401, 2},
    {0x40D4, 0xF001, 2},
    {0x40D6, 0x82F0, 2},
    {0x40D8, 0x0283, 2},
    {0x40DA, 0x85F0, 2},
    {0x40DC, 0x0E85, 2},
    {0x40DE, 0x87F0, 2},
    {0x40E0, 0x2587, 2},
    {0x40E2, 0xF11D, 2},
    {0x40E4, 0x88F0, 2},
    {0x40E6, 0x0288, 2},
    {0x40E8, 0xF002, 2},
    {0x40EA, 0x0048, 2},
    {0x40EC, 0xF001, 2},
    {0x40EE, 0x86F0, 2},
    {0x40F0, 0x0282, 2},
    {0x40F2, 0xF011, 2},
    {0x40F4, 0x8AF0, 2},
    {0x40F6, 0x1180, 2},
    {0x40F8, 0xF022, 2},
    {0x40FA, 0xE0E0, 2},
    {0x40FC, 0xE0E0, 2},
    {0x40FE, 0xE0E0, 2},
    {0x4100, 0xF000, 2},
    {0x4102, 0x0401, 2},
    {0x4104, 0xF00F, 2},
    {0x4106, 0x020C, 2},
    {0x4108, 0xF00F, 2},
    {0x410A, 0x87F0, 2},
    {0x410C, 0x0287, 2},
    {0x410E, 0xF045, 2},
    {0x4110, 0xE839, 2},
    {0x4112, 0x20F0, 2},
    {0x4114, 0x0434, 2},
    {0x4116, 0x9032, 2},
    {0x4118, 0x48F0, 2},
    {0x411A, 0x0039, 2},
    {0x411C, 0x20F0, 2},
    {0x411E, 0x0939, 2},
    {0x4120, 0x20F0, 2},
    {0x4122, 0x0032, 2},
    {0x4124, 0x4834, 2},
    {0x4126, 0x90F0, 2},
    {0x4128, 0x04C1, 2},
    {0x412A, 0x13F0, 2},
    {0x412C, 0x0239, 2},
    {0x412E, 0x20F0, 2},
    {0x4130, 0x02B0, 2},
    {0x4132, 0x0208, 2},
    {0x4134, 0xF07D, 2},
    {0x4136, 0xB0F0, 2},
    {0x4138, 0x14E9, 2},
    {0x413A, 0x0405, 2},
    {0x413C, 0xF08C, 2},
    {0x413E, 0xE0E0, 2},
    {0x4140, 0xF047, 2},
    {0x4142, 0x9192, 2},
    {0x4144, 0xF023, 2},
    {0x4146, 0x0810, 2},
    {0x4148, 0x0225, 2},
    {0x414A, 0x108F, 2},
    {0x414C, 0x3003, 2},
    {0x414E, 0x97F0, 2},
    {0x4150, 0x0030, 2},
    {0x4152, 0xD8F0, 2},
    {0x4154, 0x059A, 2},
    {0x4156, 0xF001, 2},
    {0x4158, 0x99F0, 2},
    {0x415A, 0x0085, 2},
    {0x415C, 0xF003, 2},
    {0x415E, 0x8B89, 2},
    {0x4160, 0xF001, 2},
    {0x4162, 0x30C0, 2},
    {0x4164, 0xF002, 2},
    {0x4166, 0x9C82, 2},
    {0x4168, 0x3018, 2},
    {0x416A, 0x8BB1, 2},
    {0x416C, 0xB6F0, 2},
    {0x416E, 0x0121, 2},
    {0x4170, 0x58F0, 2},
    {0x4172, 0x0A99, 2},
    {0x4174, 0xF001, 2},
    {0x4176, 0x98F0, 2},
    {0x4178, 0x00A2, 2},
    {0x417A, 0xF002, 2},
    {0x417C, 0xA296, 2},
    {0x417E, 0xB4F0, 2},
    {0x4180, 0x009D, 2},
    {0x4182, 0xF002, 2},
    {0x4184, 0xA1F0, 2},
    {0x4186, 0x18A1, 2},
    {0x4188, 0xF003, 2},
    {0x418A, 0x9D8B, 2},
    {0x418C, 0x1009, 2},
    {0x418E, 0x83F0, 2},
    {0x4190, 0x0088, 2},
    {0x4192, 0xF001, 2},
    {0x4194, 0x3600, 2},
    {0x4196, 0xF001, 2},
    {0x4198, 0x9088, 2},
    {0x419A, 0xF003, 2},
    {0x419C, 0x3600, 2},
    {0x419E, 0x83F0, 2},
    {0x41A0, 0x0D8B, 2},
    {0x41A2, 0xF00E, 2},
    {0x41A4, 0xA3F0, 2},
    {0x41A6, 0x02A3, 2},
    {0x41A8, 0xF002, 2},
    {0x41AA, 0x9DF0, 2},
    {0x41AC, 0x03A1, 2},
    {0x41AE, 0xF018, 2},
    {0x41B0, 0xA1F0, 2},
    {0x41B2, 0x3621, 2},
    {0x41B4, 0xED40, 2},
    {0x41B6, 0xD21F, 2},
    {0x41B8, 0xF684, 2},
    {0x41BA, 0x3003, 2},
    {0x41BC, 0x0840, 2},
    {0x41BE, 0xF000, 2},
    {0x41C0, 0x0041, 2},
    {0x41C2, 0x8082, 2},
    {0x41C4, 0x0208, 2},
    {0x41C6, 0x8736, 2},
    {0x41C8, 0xC0F0, 2},
    {0x41CA, 0x0636, 2},
    {0x41CC, 0xC087, 2},
    {0x41CE, 0x0208, 2},
    {0x41D0, 0x8280, 2},
    {0x41D2, 0x8082, 2},
    {0x41D4, 0x0208, 2},
    {0x41D6, 0x8736, 2},
    {0x41D8, 0xC0F0, 2},
    {0x41DA, 0x0636, 2},
    {0x41DC, 0xC087, 2},
    {0x41DE, 0x0208, 2},
    {0x41E0, 0x8280, 2},
    {0x41E2, 0x8082, 2},
    {0x41E4, 0x0208, 2},
    {0x41E6, 0x8736, 2},
    {0x41E8, 0xC0F0, 2},
    {0x41EA, 0x0636, 2},
    {0x41EC, 0xC087, 2},
    {0x41EE, 0x0208, 2},
    {0x41F0, 0x8280, 2},
    {0x41F2, 0x8082, 2},
    {0x41F4, 0x0208, 2},
    {0x41F6, 0x8736, 2},
    {0x41F8, 0xC0F0, 2},
    {0x41FA, 0x0636, 2},
    {0x41FC, 0xC087, 2},
    {0x41FE, 0x0208, 2},
    {0x4200, 0x8280, 2},
    {0x4202, 0x9FF0, 2},
    {0x4204, 0x0313, 2},
    {0x4206, 0x00F0, 2},
    {0x4208, 0x04B7, 2},
    {0x420A, 0xE0E0, 2},
    {0x420C, 0xE0E0, 2},
    {0x420E, 0xE0E0, 2},
    {0x4210, 0xF00B, 2},
    {0x4212, 0x9192, 2},
    {0x4214, 0xF000, 2},
    {0x4216, 0x80F0, 2},
    {0x4218, 0x230A, 2},
    {0x421A, 0x3410, 2},
    {0x421C, 0x8F30, 2},
    {0x421E, 0x03B2, 2},
    {0x4220, 0xF000, 2},
    {0x4222, 0x30C0, 2},
    {0x4224, 0x3018, 2},
    {0x4226, 0x97B5, 2},
    {0x4228, 0xF002, 2},
    {0x422A, 0x9AF0, 2},
    {0x422C, 0x0099, 2},
    {0x422E, 0xF002, 2},
    {0x4230, 0x3018, 2},
    {0x4232, 0x8530, 2},
    {0x4234, 0xC09E, 2},
    {0x4236, 0x4042, 2},
    {0x4238, 0x2018, 2},
    {0x423A, 0x8941, 2},
    {0x423C, 0x0482, 2},
    {0x423E, 0xA0F0, 2},
    {0x4240, 0x019C, 2},
    {0x4242, 0xF00B, 2},
    {0x4244, 0x99F0, 2},
    {0x4246, 0x0198, 2},
    {0x4248, 0xF000, 2},
    {0x424A, 0xA296, 2},
    {0x424C, 0xF000, 2},
    {0x424E, 0xB4A2, 2},
    {0x4250, 0xF002, 2},
    {0x4252, 0x9DF0, 2},
    {0x4254, 0x02A1, 2},
    {0x4256, 0xF01D, 2},
    {0x4258, 0x8BA1, 2},
    {0x425A, 0x1009, 2},
    {0x425C, 0x83F0, 2},
    {0x425E, 0x0036, 2},
    {0x4260, 0x00F0, 2},
    {0x4262, 0x009D, 2},
    {0x4264, 0x88F0, 2},
    {0x4266, 0x0588, 2},
    {0x4268, 0xF000, 2},
    {0x426A, 0x3600, 2},
    {0x426C, 0x9083, 2},
    {0x426E, 0xF014, 2},
    {0x4270, 0x8BF0, 2},
    {0x4272, 0x61A3, 2},
    {0x4274, 0xF002, 2},
    {0x4276, 0xA3F0, 2},
    {0x4278, 0x029D, 2},
    {0x427A, 0xF002, 2},
    {0x427C, 0xA1F0, 2},
    {0x427E, 0x18A1, 2},
    {0x4280, 0xF035, 2},
    {0x4282, 0x9D41, 2},
    {0x4284, 0x10B9, 2},
    {0x4286, 0xF003, 2},
    {0x4288, 0xB2F0, 2},
    {0x428A, 0x098B, 2},
    {0x428C, 0x9184, 2},
    {0x428E, 0x8EF0, 2},
    {0x4290, 0x38A6, 2},
    {0x4292, 0x848E, 2},
    {0x4294, 0xF002, 2},
    {0x4296, 0x0202, 2},
    {0x4298, 0xF003, 2},
    {0x429A, 0x91F0, 2},
    {0x429C, 0x0FB2, 2},
    {0x429E, 0xF000, 2},
    {0x42A0, 0x83F0, 2},
    {0x42A2, 0x0036, 2},
    {0x42A4, 0x00B8, 2},
    {0x42A6, 0xF008, 2},
    {0x42A8, 0x3600, 2},
    {0x42AA, 0xF001, 2},
    {0x42AC, 0x839C, 2},
    {0x42AE, 0xF005, 2},
    {0x42B0, 0x9CF0, 2},
    {0x42B2, 0x0D8B, 2},
    {0x42B4, 0xF004, 2},
    {0x42B6, 0x3018, 2},
    {0x42B8, 0xA3F0, 2},
    {0x42BA, 0x02A3, 2},
    {0x42BC, 0xF002, 2},
    {0x42BE, 0x9DF0, 2},
    {0x42C0, 0x27B9, 2},
    {0x42C2, 0xF029, 2},
    {0x42C4, 0x3018, 2},
    {0x42C6, 0x9DF0, 2},
    {0x42C8, 0x0282, 2},
    {0x42CA, 0xF003, 2},
    {0x42CC, 0x30C0, 2},
    {0x42CE, 0xF00E, 2},
    {0x42D0, 0x30C0, 2},
    {0x42D2, 0xF002, 2},
    {0x42D4, 0x82F0, 2},
    {0x42D6, 0x0990, 2},
    {0x42D8, 0xF003, 2},
    {0x42DA, 0x8C8F, 2},
    {0x42DC, 0xF02D, 2},
    {0x42DE, 0x3018, 2},
    {0x42E0, 0xA2F0, 2},
    {0x42E2, 0x02A2, 2},
    {0x42E4, 0xF002, 2},
    {0x42E6, 0x9DF0, 2},
    {0x42E8, 0x289D, 2},
    {0x42EA, 0xF007, 2},
    {0x42EC, 0x3018, 2},
    {0x42EE, 0x89B5, 2},
    {0x42F0, 0xF000, 2},
    {0x42F2, 0x8BF0, 2},
    {0x42F4, 0x0097, 2},
    {0x42F6, 0xF000, 2},
    {0x42F8, 0x17A6, 2},
    {0x42FA, 0x21CD, 2},
    {0x42FC, 0x40C2, 2},
    {0x42FE, 0x1049, 2},
    {0x4300, 0xF000, 2},
    {0x4302, 0x3007, 2},
    {0x4304, 0x84F0, 2},
    {0x4306, 0x0680, 2},
    {0x4308, 0xF00E, 2},
    {0x430A, 0x86F0, 2},
    {0x430C, 0x0086, 2},
    {0x430E, 0xF008, 2},
    {0x4310, 0x8082, 2},
    {0x4312, 0x0088, 2},
    {0x4314, 0x30C0, 2},
    {0x4316, 0x3600, 2},
    {0x4318, 0xF00B, 2},
    {0x431A, 0x36C0, 2},
    {0x431C, 0x8783, 2},
    {0x431E, 0x0005, 2},
    {0x4320, 0x80F0, 2},
    {0x4322, 0x0082, 2},
    {0x4324, 0x0088, 2},
    {0x4326, 0x30C0, 2},
    {0x4328, 0x3600, 2},
    {0x432A, 0xF00A, 2},
    {0x432C, 0x30C0, 2},
    {0x432E, 0x3600, 2},
    {0x4330, 0x8783, 2},
    {0x4332, 0x8280, 2},
    {0x4334, 0xF017, 2},
    {0x4336, 0x1300, 2},
    {0x4338, 0xF000, 2},
    {0x433A, 0xB8B7, 2},
    {0x433C, 0x9FF0, 2},
    {0x433E, 0x0081, 2},
    {0x4340, 0xE0E0, 2},
    {0x4342, 0xE0E0, 2},
    {0x4344, 0xE0E0, 2},
    {0x4346, 0xE0E0, 2},
    {0x4348, 0xE0E0, 2},
    {0x434A, 0xE0E0, 2},
    {0x434C, 0xE0E0, 2},
    {0x434E, 0xE0E0, 2},
    {0x5500, 0x0000, 2},
    {0x5502, 0x0001, 2},
    {0x5504, 0x0002, 2},
    {0x5506, 0x0006, 2},
    {0x5508, 0x0008, 2},
    {0x550A, 0x0010, 2},
    {0x550C, 0x0011, 2},
    {0x550E, 0x0012, 2},
    {0x5510, 0x0018, 2},
    {0x5512, 0x0019, 2},
    {0x5514, 0x0020, 2},
    {0x5516, 0x0021, 2},
    {0x5518, 0x0027, 2},
    {0x551A, 0x002E, 2},
    {0x551C, 0x002F, 2},
    {0x551E, 0x0030, 2},
    {0x5400, 0x0100, 2},
    {0x5402, 0x3108, 2},
    {0x5404, 0x010F, 2},
    {0x5406, 0x4100, 2},
    {0x5408, 0x510E, 2},
    {0x540A, 0x8101, 2},
    {0x540C, 0xA10A, 2},
    {0x540E, 0xC108, 2},
    {0x5410, 0xF10B, 2},
    {0x5412, 0xD107, 2},
    {0x5414, 0xF15A, 2},
    {0x5416, 0xF1E9, 2},
    {0x5418, 0xF2B3, 2},
    {0x541A, 0xF3D1, 2},
    {0x541C, 0xF564, 2},
    {0x541E, 0xF79D, 2},
    {0x5420, 0xFAC1, 2},
    {0x5422, 0xFF32, 2},
    {0x5424, 0xFFFA, 2},
    {0x5426, 0x5557, 2},
    {0x5428, 0x0005, 2},
    {0x542A, 0xA550, 2},
    {0x542C, 0xAAAA, 2},
    {0x542E, 0x000A, 2},
    {0x5460, 0x2269, 2},
    {0x5462, 0x0B8B, 2},
    {0x5464, 0x0B8B, 2},
    {0x5466, 0x098B, 2},
    {0x5498, 0x0000, 2},
    {0x549A, 0x16E2, 2},
    {0x549C, 0x16E3, 2},
    {0x549E, 0x16E3, 2},
    {0x3060, 0xFF01, 2},
    {0x44BA, 0x3155, 2},
    {0x44BC, 0x99AA, 2},
    {0x44BE, 0x86E0, 2},
    {0x44C0, 0x4080, 2},
    {0x44C4, 0x0FD0, 2},
    {0x44C6, 0x16E2, 2},
    {0x44C8, 0x6343, 2},
    {0x44CC, 0x7777, 2},
    {0x44CA, 0x800E, 2},
    {0x44CE, 0x8B74, 2},
    {0x44D0, 0x171D, 2},
    {0x44D2, 0x0B8B, 2},
    {0x44D6, 0xB206, 2},
    {0x44D8, 0xAAFA, 2},
    {0x44DA, 0xC001, 2},
    {0x340E, 0xA18B, 2},
    {0x44E4, 0xC000, 2},
    {0x44E6, 0xA03F, 2},
    {0x44DE, 0x2FB9, 2},
    {0x44E0, 0x3939, 2},
    {0x44E2, 0x3921, 2},
    {0x32A4, 0x0000, 2},
    {0x3098, 0x0001, 2},
    {0x3060, 0xFF01, 2},
    {0x36C0, 0x0001, 2},
    {0x3600, 0x94DF, 2},
    {0x3040, 0x4010, 2},
    {0x328E, 0x0004, 2},
    {0x3340, 0x0C60, 2},
    {0x3340, 0x1C60, 2},
    {0x33C2, 0x0001, 2},
    {0x0138, 0x01, 1},
    {0x33C0, 0x0002, 2},
    {0x0100, 0x01, 1},
    {0x44D6, 0xB206, 2},

    {SENSOR_REG_DELAY, 0x20,1},
    {SENSOR_REG_END, 0x00,1},/* END MARKER */
};

/*
 * the order of the ar0544_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting ar0544_win_sizes[] = {
    {
        .width          = 2592,
        .height         = 1944,
        .fps            = 30 << 16 | 1,
        .mbus_code      = TISP_VI_FMT_SGRBG10_1X10,
        .colorspace     = TISP_COLORSPACE_SRGB,
        .regs           = ar0544_init_regs_2592_1944_30fps_mipi,
    },
};

static struct tx_isp_sensor_win_setting *wsize = &ar0544_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list ar0544_stream_on_mipi[] = {
    // {0x0100, 0x0001},
    { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list ar0544_stream_off_mipi[] = {
    // {0x0100, 0x0000},
    { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int ar0544_read(struct tx_isp_subdev *sd, unsigned char reg,
                 unsigned char *value)
{
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    struct i2c_msg msg[2] = {
        [0] = {
            .addr	= client->addr,
            .flags	= 0,
            .len	= 1,
            .buf	= &reg,
        },
        [1] = {
            .addr	= client->addr,
            .flags	= I2C_M_RD,
            .len	= 1,
            .buf	= value,
        }
    };
    int ret;
    ret = private_i2c_transfer(client->adapter, msg, 2);
    if (ret > 0)
        ret = 0;

    return ret;
}

int ar0544_write(struct tx_isp_subdev *sd, unsigned char reg,
                  unsigned char value)
{
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    unsigned char buf[2] = {reg, value};
    struct i2c_msg msg = {
        .addr	= client->addr,
        .flags	= 0,
        .len	= 2,
        .buf	= buf,
    };
    int ret;
    ret = private_i2c_transfer(client->adapter, &msg, 1);
    if (ret > 0)
        ret = 0;

    return ret;
}

#if 0
static int ar0544_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    unsigned char val;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = ar0544_read(sd, vals->reg_num, &val);
            /* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif

static int ar0544_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = ar0544_write(sd, vals->reg_num, vals->value);
            printk("{0x%04x,0x%04x},ret = %d\n", vals->reg_num, vals->value, ret);
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int ar0544_read(struct tx_isp_subdev *sd, uint16_t reg,
                 unsigned char *value) {
    int ret;
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
    struct i2c_msg msg[2] = {
        [0] = {
            .addr   = client->addr,
            .flags  = 0,
            .len    = 2,
            .buf    = buf,
        },
        [1] = {
            .addr   = client->addr,
            .flags  = I2C_M_RD,
            .len    = 1,
            .buf    = value,
        }
    };

    ret = private_i2c_transfer(client->adapter, msg, 2);
    if (ret > 0)
        ret = 0;

    return ret;
}

int ar0544_read16(struct tx_isp_subdev *sd, uint16_t reg,
                 uint16_t *value) {
    int ret;
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
    uint8_t data[2];
    struct i2c_msg msg[2] = {
        [0] = {
            .addr   = client->addr,
            .flags  = 0,
            .len    = 2,
            .buf    = buf,
        },
        [1] = {
            .addr   = client->addr,
            .flags  = I2C_M_RD,
            .len    = 2,
            .buf    = data,
        }
    };

    ret = private_i2c_transfer(client->adapter, msg, 2);
    if (ret > 0)
        ret = 0;
    *value = (data[0] << 8) | data[1];

    return ret;
}

int ar0544_write(struct tx_isp_subdev *sd, uint16_t reg,
                  uint16_t value, int len)
{
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    int ret;
    uint8_t buf[4];
    if(len == 1) {
        buf[0] = (reg >> 8) & 0xff;
        buf[1] = reg & 0xff;
        buf[2] = value & 0xff;
    }else if(len == 2 ) {
        buf[0] = (reg >> 8) & 0xff;
        buf[1] = reg & 0xff;
        buf[2] = (value >> 8) & 0xff;
        buf[3] = value & 0xff;
    }else{
        printk("ar0544_write error: len = %d\n", len);
        return -1;
    }

    struct i2c_msg msg = {
        .addr   = client->addr,
        .flags  = 0,
        .len    = len+2,
        .buf    = buf,
    };

    ret = private_i2c_transfer(client->adapter, &msg, 1);
    if (ret > 0)
        ret = 0;

    return ret;
}

#if 0
static int ar0544_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    unsigned char val;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = ar0544_read(sd, vals->reg_num, &val);
            if (ret < 0)
                return ret;
        }
        vals++;
    }
    return 0;
}
#endif

static int ar0544_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = ar0544_write(sd, vals->reg_num, vals->value, vals->len);
            printk("{%04x,%04x},ret=%d\n", vals->reg_num, vals->value, ret);
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int ar0544_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    unsigned long rate;
    int ret = ISP_SUCCESS;

    rate = private_clk_get_rate(sensor->mclk);
    if(((rate / 1000) % (mclk / 1000)) != 0) {
        ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
        sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
        if (IS_ERR(sclka)) {
            pr_err("get sclka failed\n");
        } else {
            rate = private_clk_get_rate(sclka);
            if(((rate / 1000) % (mclk / 1000)) != 0) {
                switch(mclk) {
                case 24000000:
                    private_clk_set_rate(sclka, 1200000000);
                    break;
                case 27000000:
                case 37125000:
                    private_clk_set_rate(sclka, 1188000000);
                    break;
                default:
                    ret = -1;
                    goto error;
                }
            } else {
                if ((mclk != 24000000) && (mclk != 27000000) && (mclk != 37125000)) {
                    ret = -1;
                    goto error;
                }
            }
        }
    }

    private_clk_set_rate(sensor->mclk, mclk);
    private_clk_prepare_enable(sensor->mclk);

error:
    return ret;
}

static int ar0544_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    int ret = ISP_SUCCESS;

    sensor->video.vi_max_width = wsize->width;
    sensor->video.vi_max_height = wsize->height;

    sensor->video.mbus.width = wsize->width;
    sensor->video.mbus.height = wsize->height;
    sensor->video.mbus.field = TISP_FIELD_NONE;
    sensor->video.mbus.colorspace = wsize->colorspace;

    sensor->video.fps.min = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

#ifdef SENSOR_WDR_2_FRAME
    sensor->video.wdr = 1;
#else
    sensor->video.wdr = 0;
#endif

    ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

    return ret;
}

static int ar0544_setting_select(struct tx_isp_subdev *sd, int deboot)
{
    int ret = ISP_SUCCESS;

    switch (deboot) {
    case 0:
        wsize = &ar0544_win_sizes[0];
        ar0544_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

        ar0544_attr.again.value = 65536;
        ar0544_attr.again.max = 583724;
        ar0544_attr.again.min = 0;
        ar0544_attr.again.apply_delay = 2;

        ar0544_attr.integration_time.value = 0xb60;
        ar0544_attr.integration_time.max = 0x7b8 - 8;
        ar0544_attr.integration_time.min = 1;
        ar0544_attr.integration_time.apply_delay = 2;

       ar0544_attr.total_width = 0x1d0c;
        ar0544_attr.total_height = 0x7b8;

        ar0544_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
        ar0544_attr.hcg.base_gain = ;
        ar0544_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
        ar0544_attr.again_short.value = ;
        ar0544_attr.again_short.max = ;
        ar0544_attr.again_short.min = ;
        ar0544_attr.again_short.apply_delay = ;

        ar0544_attr.integration_time_short.value = ;
        ar0544_attr.integration_time_short.max = ;
        ar0544_attr.integration_time_short.min = ;
        ar0544_attr.integration_time_short.apply_delay = ;

        ar0544_attr.wdr_cache = wdr_line * ar0544_attr.total_width;

#ifdef SENSOR_HCG
        ar0544_attr.hcg_short.base_gain = ;
        ar0544_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

        memcpy((void *)(&(ar0544_attr.mipi)), (void *)(&ar0544_mipi_linear), sizeof(ar0544_attr.mipi));
        break;
    default:
        ISP_ERROR("Have no this Setting Source!!!\n");
        break;
    }

    return ret;
}

static int ar0544_attr_check(struct tx_isp_subdev *sd)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    struct clk *sclka;
    int ret = ISP_SUCCESS;

    ar0544_setting_select(sd, info->default_boot);

    switch (info->video_interface) {
    case TISP_SENSOR_VI_MIPI_CSI0:
        ar0544_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
        ar0544_attr.mipi.index = 0;
        break;
    case TISP_SENSOR_VI_MIPI_CSI1:
        ar0544_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
        ar0544_attr.mipi.index = 1;
        break;
    case TISP_SENSOR_VI_DVP:
        ar0544_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
        break;
    default:
        ISP_ERROR("Have no this interface!!!\n");
    }

#ifndef ZRT_SENSOR_WITHOUT_INIT
    switch (info->mclk) {
    case TISP_SENSOR_MCLK0:
    case TISP_SENSOR_MCLK1:
    case TISP_SENSOR_MCLK2:
        sclka = private_devm_clk_get(&client->dev, SEN_MCLK);
        sensor->mclk = private_devm_clk_get(sensor->dev, SEN_BCLK);
        set_sensor_mclk_function(0);
        break;
    default:
        ISP_ERROR("Have no this MCLK Source!!!\n");
    }

    if (IS_ERR(sensor->mclk)) {
        ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
        goto err_get_mclk;
    }

    ret = ar0544_clk_set(sd, sclka, SENSOR_MCLK);
    if (ret) {
        ISP_ERROR("MCLK configuration failed!!!\n");
    }
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
    ar0544_attr_set(sd, wsize);
    sensor->priv = wsize;

    return 0;

err_get_mclk:
    return -1;
}

static int ar0544_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
    uint16_t v;
    int ret;

    ret = ar0544_read16(sd, 0x0022, &v);
    pr_debug("-----%s: %d ret = %d, v = 0x%04x\n", __func__, __LINE__, ret, v);
    printk("-----%s: %d ret = %d, v = 0x%04x\n", __func__, __LINE__, ret, v);
    if (ret < 0)
        return ret;
    if (v != SENSOR_CHIP_ID_H)
         return -ENODEV;
    ident = v;


    return 0;
}

static int ar0544_g_chip_ident(struct tx_isp_subdev *sd,
                                struct tx_isp_chip_ident *chip)
{
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    unsigned int ident = 0;
    int ret = ISP_SUCCESS;

    ar0544_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
    if (info->rst_gpio != -1) {
        ret = private_gpio_request(info->rst_gpio, "ar0544_reset");
        if (!ret) {
            private_gpio_direction_output(info->rst_gpio, 1);
            private_msleep(100);
            private_gpio_direction_output(info->rst_gpio, 0);
            private_msleep(100);
            private_gpio_direction_output(info->rst_gpio, 1);
            private_msleep(100);
        } else {
            ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
        }
    }
    if (info->pwdn_gpio != -1) {
        ret = private_gpio_request(info->pwdn_gpio, "ar0544_pwdn");
        if (!ret) {
            private_gpio_direction_output(info->pwdn_gpio, 0);
            private_msleep(5);
            private_gpio_direction_output(info->pwdn_gpio, 1);
            private_msleep(5);
        } else {
            ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
        }
    }
    ret = ar0544_detect(sd, &ident);
    if (ret) {
        ISP_ERROR("chip found @ 0x%x (%s) is not an ar0544 chip.\n",
                  client->addr, client->adapter->name);
        return ret;
    }
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    ISP_WARNING("===================================================\n");
    ISP_WARNING("Template version is %s\n", TVERSION);
    ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
    ISP_WARNING("Sensor name is %s\n", ar0544_attr.name);
    ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
    ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
    ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
                info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
    ISP_WARNING("===================================================\n");

    if (chip) {
        memcpy(chip->name, "ar0544", sizeof("ar0544"));
        chip->ident = ident;
        chip->revision = SENSOR_VERSION;
    }

    return 0;
}

static int ar0544_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
    return 0;
}

static int ar0544_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    int ret = ISP_SUCCESS;

    if (!init->enable) {
        sensor->video.state = TX_ISP_MODULE_DEINIT;
        return ISP_SUCCESS;
    } else {
        sensor->video.mbus.code = wsize->mbus_code;
        sensor->video.fps.value = wsize->fps;
        sensor->video.fps.max = wsize->fps;
        sensor->video.fps.apply_delay = 1;
        ret = ar0544_attr_set(sd, wsize);
        sensor->video.state = TX_ISP_MODULE_DEINIT;
    }

    return ret;
}

static int ar0544_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
    uint16_t val = 0;
    int len = 0;
    int ret = ISP_SUCCESS;

    len = strlen(sd->chip.name);
    if (len && strncmp(sd->chip.name, reg->name, len)) {
        return -EINVAL;
    }
    if (!private_capable(CAP_SYS_ADMIN))
        return -EPERM;
    ret = ar0544_read16(sd, reg->reg & 0xffff, &val);
    reg->val = val;
    reg->size = 2;

    return ret;
}

static int ar0544_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
    int len = 0;

    len = strlen(sd->chip.name);
    if (len && strncmp(sd->chip.name, reg->name, len)) {
        return -EINVAL;
    }
    if (!private_capable(CAP_SYS_ADMIN))
        return -EPERM;
    ar0544_write(sd, reg->reg & 0xffff, reg->val & 0xffff, 2);

    return 0;
}

static int ar0544_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    int ret = ISP_SUCCESS;

    if (init->enable) {
        if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
            ret = ar0544_write_array(sd, wsize->regs);
            printk("%s %d ret = %d\n", __func__, __LINE__, ret);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
            if (ret)
                return ret;
            sensor->video.state = TX_ISP_MODULE_INIT;
        }
        if (sensor->video.state == TX_ISP_MODULE_INIT) {
            ret = ar0544_write_array(sd, ar0544_stream_on_mipi);
            sensor->video.state = TX_ISP_MODULE_RUNNING;
            pr_debug("ar0544 stream on\n");
        }

    } else {
        ret = ar0544_write_array(sd, ar0544_stream_off_mipi);
        sensor->video.state = TX_ISP_MODULE_INIT;
        pr_debug("ar0544 stream off\n");
    }

    return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int ar0544_set_expo(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;
    int it = value & 0xffff;
    int again = (value & 0xffff0000) >> 16;
    printk("it =  %d,again = %d \n", it, again);

    ret += ar0544_write(sd, 0x0202, (unsigned char)it,2);

    ret += ar0544_write(sd, 0x3062, (unsigned char)again,2);

    return ret;
}
#else
static int ar0544_set_integration_time(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    value = (value << 1) - 1;
    ret += ar0544_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
    ret += ar0544_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
    ret += ar0544_write(sd, 0x3e02, (unsigned char)((value & 0xf) << 4));

    return ret;
}

static int ar0544_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ret += ar0544_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
    ret += ar0544_write(sd, 0x3e09, (unsigned char)(value & 0xff));

    return ret;
}
#endif /* SENSOR_EXPO */

static int ar0544_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
    return 0;
}

static int ar0544_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
    return 0;
}

static int ar0544_set_mode(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    if (wsize) {
        ret = ar0544_attr_set(sd, wsize);
    }

    return ret;
}

static int ar0544_set_fps(struct tx_isp_subdev *sd, int fps)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    unsigned int sclk = 0;
    unsigned int hts = 0;
    unsigned int vts = 0;
    unsigned char val = 0;
    unsigned int newformat = 0; //the format is 24.8
    unsigned int max_fps = 0;
    int ret = ISP_SUCCESS;

    switch (info->default_boot) {
    case 0:
        sclk = 0x1884 * 0x920 * 30;  /**< HTS * VTS * FPS */
        max_fps = 30;
        break;
    default:
        ret = -1;
        ISP_ERROR("Now we do not support this framerate!!!\n");
    }

    /* max_fps = wsize->fps; */
    newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
    if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
        ISP_ERROR("warn: fps(%d/%d) no in range\n", fps >> 16, fps & 0xffff);
        return -1;
    }

    val = 0;
    ret += ar0544_read(sd, 0x0342, &val);
    hts = val << 8;
    val = 0;
    ret += ar0544_read(sd, 0x0343, &val);
    hts |= val;
    if (0 != ret) {
        ISP_ERROR("err: ar0544 read err\n");
        return ret;
    }

    vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

    printk("hts=%d,vts=%d,fps=%d\n", hts, vts, ((fps >> 16) & 0xffff)/(fps & 0xffff));
    ar0544_write(sd, 0x3001, 0x0001, 2);
    ar0544_write(sd, 0x0340, (uint16_t)vts, 2);
    ar0544_write(sd, 0x3001, 0x0000, 2);

    if (0 != ret) {
        ISP_ERROR("err: ar0544_write err\n");
        return ret;
    }

    /* sensor->video.fps = fps; */
    sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
    /* Linear mode */
    sensor->video.attr->total_height = vts;
    sensor->video.attr->integration_time.max = vts - 8;
#else
    /* WDR mode */
    switch (info->default_boot) {
    case 0:
        sensor->video.attr->total_height = vts;
        sensor->video.attr->integration_time.max = vts - 4;
        break;
    case 1:
        sensor->video.attr->total_height = vts;
        sensor->video.attr->integration_time.max = vts - 4;
        sensor->video.attr->integration_time_short.max = vts - 4;
        break;
    default:
        ret = -1;
        ISP_ERROR("Now we do not support this framerate!!!\n");
    }
#endif /* SENSOR_WDR_2_FRAME */

    ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
    if (ret) {
        ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
    }

    return ret;
}

static int ar0544_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
    struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
    int ret = ISP_SUCCESS;
    uint16_t mode;
    unsigned char val = 0;

    par->drop_frame = 0;
    par->reset = 0;

    /* 2'b01:mirror,2'b10:filp */
    ret = ar0544_read(sd, 0x0101, &val);
    mode = val << 8;
    ret = ar0544_read(sd, 0x0101, &val);
    mode += val;
    switch(par->hvflip) {
    case TX_ISP_SENSOR_HVFLIP_NOMAL:
        mode &= 0xf0ff;
        sensor->video.mbus.code = TISP_VI_FMT_SGRBG10_1X10;
        break;
    case TX_ISP_SENSOR_HVFLIP_HFLIP:
        mode &= 0xf0ff;
        mode |= 0x0100;
        sensor->video.mbus.code = TISP_VI_FMT_SRGGB10_1X10;
        break;
    case TX_ISP_SENSOR_HVFLIP_VFLIP:
        mode &= 0xf0ff;
        mode |= 0x0200;
        sensor->video.mbus.code = TISP_VI_FMT_SBGGR10_1X10;
        break;
    case TX_ISP_SENSOR_HVFLIP_HVFLIP:
        mode |= 0x0300;
        sensor->video.mbus.code = TISP_VI_FMT_SGBRG10_1X10;
        break;
    }
    ret += ar0544_write(sd, 0x0101, mode, 2);

    sensor->video.hvflip_mode = par->hvflip;
    sensor->video.mbus_change = 1;
    ar0544_attr_set(sd, wsize);

    return ret;
#else
    return -1;
#endif /* SENSOR_MIR_FLIP */
}

static int ar0544_pm_s_stream_ctrl(struct tx_isp_subdev *sd, void *arg)
{
    int ret = ISP_SUCCESS;
    IMPISPUserCtl *data = (IMPISPUserCtl *)arg;

    if(data->cmd == TX_SENSOR_PM_RESUME){

        // ret = ar0544_write_array(sd, wsize->regs);
        // if (ret < 0){
        //     printk("ar0544 resume setting failed\n");
        //     return ret;
        // }
        // ret = ar0544_resume(sd);
        // if(ret != 0){
        //     printk("ar0544 resume attr failed\n");
        //     return ret;
        // }

        ret = ar0544_write_array(sd, ar0544_stream_on_mipi);
        if(ret != 0){
            printk("ar0544 streamon failed\n");
            return ret;
        }

        printk("ar0544 TX_SENSOR_PM_RESUME\n");

    }else if(data->cmd == TX_SENSOR_PM_SUSPEND) {
        ret = ar0544_write_array(sd, ar0544_stream_off_mipi);
        if (ret < 0) {
            printk("\n TX_SENSOR_PM_SUSPEND error !!!\n");
            return ret;
        }
        printk("ar0544 TX_SENSOR_PM_SUSPEND !!!\n");
    }else if(data->cmd == TX_SENSOR_PM_PREPARE) {
        printk("\n ar0544 TX_SENSOR_PM_PREPARE !!!\n");
    }else if(data->cmd == TX_SENSOR_PM_COMPLETE) {
        printk("\n ar0544 TX_SENSOR_PM_COMPLETE !!!\n");
    }else if(data->cmd == TX_SENSOR_PM_STREAMON) {
        ret = ar0544_write_array(sd, ar0544_stream_on_mipi);
        if (ret < 0) {
            printk("\n TX_SENSOR_PM_STREAMON error !!!\n");
            return ret;
        }
        printk("\n ar0544 TX_SENSOR_PM_STREAMON !!!\n");
    }else if(data->cmd == TX_SENSOR_PM_STREAMOFF) {
        ret = ar0544_write_array(sd, ar0544_stream_off_mipi);
        if (ret < 0) {
            printk("\n [%s %d] TX_SENSOR_PM_STREAMOFF error !!!\n", __func__, __LINE__);
            return ret;
        }
        printk("\n ar0544 TX_SENSOR_PM_STREAMOFF !!!\n");
    }else {
        printk("\n==> Don't Support this function !!! \n");
        return -EINVAL;
    }

    return ret;
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int ar0544_set_expo_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}
#else
static int ar0544_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}

static int ar0544_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}
#endif /* SENSOR_EXPO */

static int ar0544_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
    struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
    int ret = ISP_SUCCESS;

    ret = ar0544_write_array(sd, ar0544_stream_off_mipi);
    if (par->wdr_en == 1) {
        if (par->boot == -1) {
            info->default_boot = 1;
        }
        ar0544_setting_select(sd, 1);
        ar0544_attr_set(sd, wsize);
    } else if (par->wdr_en == 0) {
        if (par->boot == -1) {
            info->default_boot = 0;
        }
        ar0544_setting_select(sd, 0);
        ar0544_attr_set(sd, wsize);
    } else {
        ISP_ERROR("Can not support this data type!!!");
        return -1;
    }

    return 0;
}

static int ar0544_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
    struct tx_isp_sensor_register_info *info = &sensor->info;
    int ret = ISP_SUCCESS;

    private_gpio_direction_output(info->rst_gpio, 0);
    private_msleep(1);
    private_gpio_direction_output(info->rst_gpio, 1);
    private_msleep(1);

    ret = ar0544_write_array(sd, wsize->regs);
    ret = ar0544_write_array(sd, ar0544_stream_on_mipi);

    return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int ar0544_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
    long ret = 0;
    struct tx_isp_sensor_value *sensor_val = arg;

    if (IS_ERR_OR_NULL(sd)) {
        ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
        return -EINVAL;
    }

    switch (cmd) {
#ifdef SENSOR_EXPO
    case TX_ISP_EVENT_SENSOR_EXPO:
        if (arg)
            ret = ar0544_set_expo(sd, sensor_val->value);
        break;
#else
    case TX_ISP_EVENT_SENSOR_INT_TIME:
        if (arg)
            ret = ar0544_set_integration_time(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_AGAIN:
        if (arg)
            ret = ar0544_set_analog_gain(sd, sensor_val->value);
        break;
#endif /* SENSOR_EXPO */
    case TX_ISP_EVENT_SENSOR_DGAIN:
        if (arg)
            ret = ar0544_set_digital_gain(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
        if (arg)
            ret = ar0544_get_black_pedestal(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_RESIZE:
        if (arg)
            ret = ar0544_set_mode(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
        if (arg)
            ret = ar0544_write_array(sd, ar0544_stream_off_mipi);
        break;
    case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
        if (arg)
            ret = ar0544_write_array(sd, ar0544_stream_on_mipi);
        break;
    case TX_ISP_EVENT_SENSOR_FPS:
        if (arg)
            ret = ar0544_set_fps(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_HVFLIP:
        if(arg)
            ret = ar0544_set_hvflip(sd, (void *)sensor_val->value);
        break;
    case TX_ISP_EVENT_PM_S_STREAM_CTRL:
        if(arg)
            ret =  ar0544_pm_s_stream_ctrl(sd, arg);
        break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
    case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
        if (arg)
            ret = ar0544_set_expo_short(sd, sensor_val->value);
        break;
#else
    case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
        if(arg)
            ret = ar0544_set_integration_time_short(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
        if(arg)
            ret = ar0544_set_analog_gain_short(sd, sensor_val->value);
        break;
#endif /* SENSOR_EXPO */
    case TX_ISP_EVENT_SENSOR_WDR:
        if(arg)
            ret = ar0544_set_wdr(sd, (void *)sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_WDR_STOP:
        if(arg)
            ret = ar0544_set_wdr_stop(sd, (void *)sensor_val->value);
        break;
#endif /* SENSOR_WDR_2_FRAME */
    default:
        break;
    }

    return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops ar0544_core_ops = {
    .g_chip_ident = ar0544_g_chip_ident,
    .reset = ar0544_reset,
    .init = ar0544_init,
    .g_register = ar0544_g_register,
    .s_register = ar0544_s_register,
};

static struct tx_isp_subdev_video_ops ar0544_video_ops = {
    .s_stream = ar0544_s_stream,
};

static struct tx_isp_subdev_sensor_ops ar0544_sensor_ops = {
#ifndef SENSOR_TEST
    .ioctl  = ar0544_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops ar0544_ops = {
    .core = &ar0544_core_ops,
    .video = &ar0544_video_ops,
    .sensor = &ar0544_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
    .name = "ar0544",
    .id = -1,
    .dev = {
        .dma_mask = &tx_isp_module_dma_mask,
        .coherent_dma_mask = 0xffffffff,
        .platform_data = NULL,
    },
    .num_resources = 0,
};

static int ar0544_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
    struct tx_isp_subdev *sd;
    struct tx_isp_video_in *video;
    struct tx_isp_sensor *sensor;

    sensor = (struct tx_isp_sensor *) kzalloc(sizeof(*sensor), GFP_KERNEL);
    if (!sensor) {
        ISP_ERROR("Failed to allocate sensor subdev.\n");
        return -ENOMEM;
    }
    memset(sensor, 0, sizeof(*sensor));
    sd = &sensor->sd;
    video = &sensor->video;

#ifdef SENSOR_MIR_FLIP
    sensor->video.hvflip_mode = TX_ISP_SENSOR_HVFLIP_NOMAL;
#endif /* SENSOR_MIR_FLIP */
    sensor->video.attr = &ar0544_attr;
    sensor->dev = &client->dev;
    tx_isp_subdev_init(&sensor_platform_device, sd, &ar0544_ops);
    tx_isp_set_subdevdata(sd, client);
    tx_isp_set_subdev_hostdata(sd, sensor);
    private_i2c_set_clientdata(client, sd);

    pr_debug("probe ok ------->ar0544\n");

    return 0;
}

static int ar0544_remove(struct i2c_client *client)
{
    struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
    struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;

    if (info->rst_gpio != -1)
        private_gpio_free(info->rst_gpio);
    if (info->pwdn_gpio != -1)
        private_gpio_free(info->pwdn_gpio);

    private_clk_disable_unprepare(sensor->mclk);
    tx_isp_subdev_deinit(sd);

    kfree(sensor);

    return 0;
}

static const struct i2c_device_id ar0544_id[] = {
    {"ar0544", 0},
    {}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, ar0544_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver ar0544_driver = {
    .driver = {
#ifdef CONFIG_ZERATUL
        .owner  = NULL,
#else
        .owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
        .name   = "ar0544",
    },
    .probe          = ar0544_probe,
    .remove         = ar0544_remove,
    .id_table       = ar0544_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_ar0544(void) {
    return private_i2c_add_driver(&ar0544_driver);
}

static __exit void exit_ar0544(void) {
    private_i2c_del_driver(&ar0544_driver);
}

module_init(init_ar0544);
module_exit(exit_ar0544);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_ar0544(void) {
    return private_i2c_add_driver(&ar0544_driver);
}

static void exit_first_ar0544(void) {
    private_i2c_del_driver(&ar0544_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
    .name = "ar0544",
    .i2c_addr = 0x30,
    .width = 1920,
    .height = 1080,
#ifdef SENSOR_WDR_2_FRAME
    .wdr = 1,
#else
    .wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
    .init_sensor = init_first_ar0544,
    .exit_sensor = exit_first_ar0544
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for ar0544 sensor");
MODULE_LICENSE("GPL");
