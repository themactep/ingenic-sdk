/*
 * ar0246.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps		dvdd
 *  0           1920*1080       60        mipi_2lane    linear  12        30
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
#define SENSOR_VERSION  "H20251121a"

/* #define SENSOR_TEST */

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
/* #define SENSOR_HCG */
#define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x1F)
#define SENSOR_CHIP_ID_L    (0x56)
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
};

struct tx_isp_sensor_attribute ar0246_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
    unsigned int value;
    unsigned int gain;
};

//todo
struct again_lut ar0246_again_lut[] = {
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
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int ar0246_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
    /* Analog gain table */
    struct again_lut *lut = ar0246_again_lut;
    while (lut->gain <= ar0246_attr.again.max) {
        if (isp_gain == 0) {
            *sensor_again = lut[0].value;
            return 0;
        } else if (isp_gain < lut->gain) {
            *sensor_again = (lut - 1)->value;
            return (lut - 1)->gain;
        } else {
            if ((lut->gain == ar0246_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int ar0246_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
    /* Analog gain table */
    struct again_lut *lut = ar0246_again_lut;
    while(lut->gain <= ar0246_attr.again_short.max) {
        if(isp_gain == 0) {
            *sensor_again = 0;
            return 0;
        }
        else if(isp_gain < lut->gain) {
            *sensor_again = (lut - 1)->value;
            return (lut - 1)->gain;
        }
        else{
            if((lut->gain == ar0246_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int ar0246_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
    return 0;
}

#ifdef SENSOR_HCG
unsigned int ar0246_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

    return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int ar0246_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

    return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus ar0246_mipi_linear = {
    .mode = SENSOR_MIPI_OTHER_MODE,
    .clk = 891,
    .lans = 2,
    .image_twidth = 1920,
    .image_theight = 1080,
    .mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
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
    .mipi_sc.data_type_value = RAW12,
    .mipi_sc.del_start = 0,
    .mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
    .mipi_sc.sensor_fid_mode = 0,
    .mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus ar0246_mipi_ehdr = {
    .mode = SENSOR_MIPI_OTHER_MODE,
    .clk = 891,
    .lans = 2,
    .image_twidth = 1920,
    .image_theight = 1080,
    .mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
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
    .mipi_sc.data_type_value = RAW12,
    .mipi_sc.del_start = 0,
    .mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
    .mipi_sc.sensor_fid_mode = 0,
    .mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_dvp_bus ar0246_dvp = {
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

struct tx_isp_sensor_attribute ar0246_attr = {
    .name = "ar0246",
    .chip_id = 0xcd1c,//todo
    .cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
    .cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
    .cbus_device = 0x30,//todo
    .sensor_ctrl.alloc_again = ar0246_alloc_again,
    .sensor_ctrl.alloc_dgain = ar0246_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
    .sensor_ctrl.alloc_again_short = ar0246_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

    .dgain = {0},
    .dgain_short = {0},
};

static struct regval_list ar0246_init_regs_1920_1080_60fps_mipi[] = {
    {0x301A,0x001D},
    {0x3030,0x04A4},
    {0x302E,0x0010},
    {0x302C,0x0702},
    {0x302A,0x0006},
    {0x3038,0x0004},
    {0x3036,0x0006},
    {0x31DC,0x1FB0},
    {0x31AE,0x0202},
    {0x301A,0x0018},
    {0x31B0,0x0068},
    {0x31B2,0x0047},
    {0x31B4,0x51C7},
    {0x31B4,0x51C7},
    {0x31B4,0x51C7},
    {0x31B6,0x4247},
    {0x31B6,0x4247},
    {0x31B6,0x4247},
    {0x31B8,0x60C9},
    {0x31B8,0x60C9},
    {0x31B8,0x60C9},
    {0x31BA,0x028A},
    {0x31BA,0x028A},
    {0x31BA,0x028A},
    {0x31BC,0x0B88},
    {0x31BC,0x0B88},
    {0x31BC,0x0B88},
    {0x31BC,0x0B88},
    {0x31C8,0x0AC0},
    {0x3004,0x0008},
    {0x3008,0x0787},
    {0x3002,0x0008},
    {0x3006,0x043F},
    {0x30A2,0x0001},
    {0x30A6,0x0001},
    {0x30BA,0x0124},
    {0x3082,0x0001},
    {0x3082,0x0001},
    {0x3082,0x0001},
    {0x33E2,0x0000},
    {0x31D0,0x0000},
    {0x31AC,0x0C0C},
    {0x3342,0x122C},
    {0x3012,0x01E0},
    {0x300C,0x0870},
    {0x300A,0x0479},
    {0x3040,0x0000},
    {0x3064,0x0400},
    {0x3180,0x0000},
    {0x3C70,0x622C},
    {0x3C70,0x622C},
    {0x3C70,0x642C},
    {0x352A,0x6F8F},
    {0x352A,0x6F6F},
    {0x352E,0x6A8A},
    {0x352E,0x668A},
    {0x352E,0x669A},
    {0x352E,0x6698},
    {0x3530,0x2A08},
    {0x3530,0x2508},
    {0x3528,0xFC84},
    {0x3528,0xFB84},
    {0x3528,0xFB84},
    {0x3528,0xFB84},
    {0x3C70,0x642C},
    {0x3428,0x0209},
    {0x3520,0x6804},
    {0x3540,0x3597},
    {0x350A,0x0654},
    {0x3532,0x4C82},
    {0x3534,0x4E60},
    {0x353E,0x981C},
    {0x351A,0x4FFF},
    {0x3092,0x0926},
    {0x3522,0x68BF},
    {0x351E,0x5B56},
    {0x3086,0x1101},
    {0x30BA,0x0124},
    {0x3364,0x00E8},
    {0x3508,0x40BB},
    {0x3782,0x0160},
    {0x3784,0x0160},
    {0x3750,0x0160},
    {0x3752,0x0160},
    {0x3096,0x0160},
    {0x3098,0x0160},
    {0x58E2,0x3EFA},
    {0x58E6,0x09AC},
    {0x3522,0x689F},
    {0x3522,0x6897},
    {0x351E,0x5B5E},
    {0x3534,0x4E64},
    {0x353C,0x221C},
    {0x3516,0xFFF4},
    {0x3514,0x888F},
    {0x350E,0x39DE},
    {0x350E,0x39EE},
    {0x3510,0x9E88},
    {0x3510,0xEE88},
    {0x3516,0xFFF4},
    {0x350A,0x0654},
    {0x350A,0x0054},
    {0x353E,0x981C},
    {0x353E,0x981C},
    {0x3516,0xFFF4},
    {0x350C,0x6448},
    {0x350C,0x6648},
    {0x3516,0xFFF4},
    {0x350C,0x6648},
    {0x3516,0xFFF4},
    {0x350A,0x0054},
    {0x350A,0x0055},
    {0x3514,0x888F},
    {0x3510,0xEE89},
    {0x3510,0xEE99},
    {0x3512,0x8988},
    {0x3512,0x9988},
    {0x3514,0x888F},
    {0x3512,0x9989},
    {0x3512,0x9999},
    {0x3514,0x898F},
    {0x3514,0x998F},
    {0x5002,0x0DC3},
    {0x51CC,0x0149},
    {0x51D8,0x044D},
    {0x51CE,0x0700},
    {0x51D0,0x0008},
    {0x51D2,0x0010},
    {0x51D4,0x0018},
    {0x51D6,0x0020},
    {0x5202,0x0DC3},
    {0x51EA,0x0149},
    {0x51FC,0x044D},
    {0x51EC,0x0700},
    {0x51EE,0x0008},
    {0x51F0,0x0010},
    {0x51F2,0x0018},
    {0x51F4,0x0020},
    {0x5402,0x0DC3},
    {0x5560,0x0149},
    {0x556C,0x044D},
    {0x5562,0x0700},
    {0x5564,0x0008},
    {0x5566,0x0010},
    {0x5568,0x0018},
    {0x556A,0x0020},
    {0x31E0,0x0001},
    {0x5000,0x0001},
    {0x5000,0x0081},
    {0x5000,0x0181},
    {0x5000,0x0181},
    {0x5200,0x0001},
    {0x5200,0x0081},
    {0x5200,0x0181},
    {0x5200,0x0181},
    {0x5400,0x0001},
    {0x5400,0x0081},
    {0x5400,0x0181},
    {0x5400,0x0181},
    {0x5000,0x1181},
    {0x50A2,0x0002},
    {0x50A4,0x000C},
    {0x50A6,0x030F},
    {0x50A6,0x0F0F},
    {0x50A8,0x030F},
    {0x50A8,0x0F0F},
    {0x50AA,0x030F},
    {0x50AA,0x050F},
    {0x50AC,0x0301},
    {0x50AC,0x0101},
    {0x50AE,0x0301},
    {0x50AE,0x0101},
    {0x50B0,0x0301},
    {0x50B0,0x0101},
    {0x50B2,0x03FF},
    {0x50B4,0x030F},
    {0x50B4,0x0F0F},
    {0x50B6,0x030F},
    {0x50B6,0x090F},
    {0x50B8,0x030F},
    {0x50B8,0x050F},
    {0x5914,0x4012},
    {0x5914,0x4001},
    {0x5910,0x6080},
    {0x5910,0x5882},
    {0x5910,0x688D},
    {0x5910,0x7897},
    {0x5910,0x989D},
    {0x5910,0xA89A},
    {0x5910,0xC886},
    {0x5910,0xC8BD},
    {0x5910,0xC90B},
    {0x5910,0xC97A},
    {0x5910,0xCA16},
    {0x5910,0xCAF2},
    {0x5910,0xCC29},
    {0x5910,0xCDE0},
    {0x5910,0x6080},
    {0x5910,0xC8F0},
    {0x5910,0xC953},
    {0x5910,0xC9DF},
    {0x5910,0x0006},
    {0x5910,0x0001},
    {0x5910,0x0002},
    {0x5910,0x0003},
    {0x5910,0x0005},
    {0x5910,0x0004},
    {0x5910,0x0001},
    {0x5910,0x0003},
    {0x5910,0x0004},
    {0x5910,0x0004},
    {0x5910,0x0007},
    {0x5910,0x0006},
    {0x5910,0x0000},
    {0x5910,0x0003},
    {0x5910,0x0002},
    {0x5910,0x0006},
    {0x5910,0x0005},
    {0x5910,0x0001},
    {0x5910,0x5940},
    {0x5910,0x0000},
    {0x5910,0x0001},
    {0x5910,0x0002},
    {0x5910,0x0003},
    {0x5910,0x0004},
    {0x5910,0x0005},
    {0x5910,0x0006},
    {0x5910,0x0007},
    {0x5910,0x9941},
    {0x5910,0x0010},
    {0x5910,0x0011},
    {0x5910,0x0012},
    {0x5910,0x0013},
    {0x5910,0x0014},
    {0x5910,0x0015},
    {0x5910,0x0016},
    {0x5910,0x0017},
    {0x5910,0x5942},
    {0x5910,0x0020},
    {0x5910,0x0021},
    {0x5910,0x0022},
    {0x5910,0x0023},
    {0x5910,0x0024},
    {0x5910,0x0025},
    {0x5910,0x0026},
    {0x5910,0x0027},
    {0x5910,0xD943},
    {0x5910,0x0030},
    {0x5910,0x0031},
    {0x5910,0x0032},
    {0x5910,0x0033},
    {0x5910,0x0034},
    {0x5910,0x0035},
    {0x5910,0x0036},
    {0x5910,0x0037},
    {0x5910,0x5944},
    {0x5910,0x0040},
    {0x5910,0x0041},
    {0x5910,0x0042},
    {0x5910,0x0043},
    {0x5910,0x0044},
    {0x5910,0x0045},
    {0x5910,0x0046},
    {0x5910,0x0047},
    {0x5910,0xD945},
    {0x5910,0x0050},
    {0x5910,0x0051},
    {0x5910,0x0052},
    {0x5910,0x0053},
    {0x5910,0x0054},
    {0x5910,0x0055},
    {0x5910,0x0056},
    {0x5910,0x0057},
    {0x5910,0xD946},
    {0x5910,0x0060},
    {0x5910,0x0061},
    {0x5910,0x0062},
    {0x5910,0x0063},
    {0x5910,0x0064},
    {0x5910,0x0065},
    {0x5910,0x0066},
    {0x5910,0x0067},
    {0x5910,0x9947},
    {0x5910,0x0070},
    {0x5910,0x0071},
    {0x5910,0x0072},
    {0x5910,0x0073},
    {0x5910,0x0074},
    {0x5910,0x0075},
    {0x5910,0x0076},
    {0x5910,0x0077},
    {0x2512,0xA000},
    {0x2510,0x0720},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0x2123},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0x275F},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0xFFFF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x0F8C},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20E0},
    {0x2510,0x8055},
    {0x2510,0xA0E1},
    {0x2510,0x3041},
    {0x2510,0x2000},
    {0x2510,0x3088},
    {0x2510,0x3282},
    {0x2510,0xA681},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FE},
    {0x2510,0x9070},
    {0x2510,0x891D},
    {0x2510,0x867F},
    {0x2510,0x20FF},
    {0x2510,0x20FC},
    {0x2510,0x893F},
    {0x2510,0x0F92},
    {0x2510,0x20E0},
    {0x2510,0x0F8F},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20E0},
    {0x2510,0x9770},
    {0x2510,0x20FC},
    {0x2510,0x8054},
    {0x2510,0x896C},
    {0x2510,0x200A},
    {0x2510,0x9030},
    {0x2510,0x200A},
    {0x2510,0x8040},
    {0x2510,0x8948},
    {0x2510,0x200A},
    {0x2510,0x1597},
    {0x2510,0x8808},
    {0x2510,0x200A},
    {0x2510,0x1F96},
    {0x2510,0x20FF},
    {0x2510,0x20E0},
    {0x2510,0xA0C0},
    {0x2510,0x200A},
    {0x2510,0x3044},
    {0x2510,0x3088},
    {0x2510,0x3282},
    {0x2510,0x2004},
    {0x2510,0x1FAA},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20FF},
    {0x2510,0x20E0},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2510,0x20FF},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2510,0x3108},
    {0x2510,0x2400},
    {0x2510,0x2401},
    {0x2510,0x3244},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2510,0x3108},
    {0x2510,0x2400},
    {0x2510,0x2401},
    {0x2510,0x2702},
    {0x2510,0x3242},
    {0x2510,0x3108},
    {0x2510,0x2420},
    {0x2510,0x2421},
    {0x2510,0x2703},
    {0x2510,0x3242},
    {0x2510,0x3108},
    {0x2510,0x2420},
    {0x2510,0x2421},
    {0x2510,0x2704},
    {0x2510,0x3244},
    {0x2510,0x7FFF},
    {0x2510,0x2000},
    {0x2510,0x8801},
    {0x2510,0x008F},
    {0x2510,0x8855},
    {0x2510,0x3101},
    {0x2510,0x2000},
    {0x2510,0x3041},
    {0x2510,0x1051},
    {0x2510,0x3102},
    {0x2510,0x3041},
    {0x2510,0x3181},
    {0x2510,0x2000},
    {0x2510,0x3041},
    {0x2510,0x0014},
    {0x2510,0x3188},
    {0x2510,0x3041},
    {0x2510,0x003B},
    {0x2510,0x3282},
    {0x2510,0x3104},
    {0x2510,0x30C1},
    {0x2510,0xB0E4},
    {0x2510,0xA992},
    {0x2510,0x1007},
    {0x2510,0xB800},
    {0x2510,0x1025},
    {0x2510,0x0051},
    {0x2510,0x1014},
    {0x2510,0x1006},
    {0x2510,0x0033},
    {0x2510,0xC020},
    {0x2510,0xB1E0},
    {0x2510,0x0030},
    {0x2510,0x0007},
    {0x2510,0xAA5A},
    {0x2510,0x8095},
    {0x2510,0xA228},
    {0x2510,0x100F},
    {0x2510,0x00A8},
    {0x2510,0x1082},
    {0x2510,0x02D6},
    {0x2510,0xA620},
    {0x2510,0x8891},
    {0x2510,0x11BD},
    {0x2510,0x1051},
    {0x2510,0x0023},
    {0x2510,0xCA36},
    {0x2510,0x10A9},
    {0x2510,0x99CD},
    {0x2510,0x2004},
    {0x2510,0x0C4E},
    {0x2510,0x113B},
    {0x2510,0xC000},
    {0x2510,0x0342},
    {0x2510,0x00C1},
    {0x2510,0x10D5},
    {0x2510,0x06C5},
    {0x2510,0x1342},
    {0x2510,0x0295},
    {0x2510,0x9008},
    {0x2510,0x200F},
    {0x2510,0x06CB},
    {0x2510,0x000E},
    {0x2510,0x1022},
    {0x2510,0x1015},
    {0x2510,0xA84B},
    {0x2510,0x002C},
    {0x2510,0x80B9},
    {0x2510,0x104B},
    {0x2510,0x1056},
    {0x2510,0x0024},
    {0x2510,0x9B0F},
    {0x2510,0x0535},
    {0x2510,0x0692},
    {0x2510,0x102C},
    {0x2510,0x009A},
    {0x2510,0x3002},
    {0x2510,0x2007},
    {0x2510,0x0028},
    {0x2510,0x8091},
    {0x2510,0x1024},
    {0x2510,0x0021},
    {0x2510,0x002F},
    {0x2510,0xCD26},
    {0x2510,0x10AD},
    {0x2510,0x9000},
    {0x2510,0x108E},
    {0x2510,0x1A28},
    {0x2510,0x122F},
    {0x2510,0x11B5},
    {0x2510,0x129D},
    {0x2510,0x1C59},
    {0x2510,0x0095},
    {0x2510,0x074B},
    {0x2510,0x1021},
    {0x2510,0x0020},
    {0x2510,0x1095},
    {0x2510,0x1030},
    {0x2510,0x104B},
    {0x2510,0xB106},
    {0x2510,0xC490},
    {0x2510,0xA882},
    {0x2510,0x8255},
    {0x2510,0xB100},
    {0x2510,0xC802},
    {0x2510,0x8801},
    {0x2510,0x00A9},
    {0x2510,0x0022},
    {0x2510,0x00D1},
    {0x2510,0x0201},
    {0x2510,0x98BB},
    {0x2510,0x2006},
    {0x2510,0x0D8A},
    {0x2510,0x0036},
    {0x2510,0x001D},
    {0x2510,0x9B3F},
    {0x2510,0x101B},
    {0x2510,0x0039},
    {0x2510,0x1001},
    {0x2510,0x0040},
    {0x2510,0x101F},
    {0x2510,0x104C},
    {0x2510,0x3081},
    {0x2510,0x100A},
    {0x2510,0x002A},
    {0x2510,0x3044},
    {0x2510,0x2001},
    {0x2510,0x1023},
    {0x2510,0x102A},
    {0x2510,0x2020},
    {0x2510,0x121D},
    {0x2510,0x04B8},
    {0x2510,0x10B8},
    {0x2510,0x1040},
    {0x2510,0xB500},
    {0x2510,0x01A3},
    {0x2510,0x022B},
    {0x2510,0x30D0},
    {0x2510,0x00CC},
    {0x2510,0x3141},
    {0x2510,0x3041},
    {0x2510,0x0044},
    {0x2510,0x3142},
    {0x2510,0x3041},
    {0x2510,0x2000},
    {0x2510,0x3110},
    {0x2510,0x2000},
    {0x2510,0x00B5},
    {0x2510,0x996B},
    {0x2510,0x3041},
    {0x2510,0x1044},
    {0x2510,0x3120},
    {0x2510,0x3041},
    {0x2510,0x1045},
    {0x2510,0x3144},
    {0x2510,0x3041},
    {0x2510,0x987B},
    {0x2510,0x3148},
    {0x2510,0x3041},
    {0x2510,0x2000},
    {0x2510,0x3182},
    {0x2510,0x3041},
    {0x2510,0x2000},
    {0x2510,0x3194},
    {0x2510,0x2001},
    {0x2510,0x3041},
    {0x2510,0x3088},
    {0x2510,0x31A0},
    {0x2510,0x2000},
    {0x2510,0x807D},
    {0x2510,0x2006},
    {0x2510,0x8815},
    {0x2510,0x8877},
    {0x2510,0x0A92},
    {0x2510,0x2201},
    {0x2510,0x2000},
    {0x2510,0x2206},
    {0x2510,0x2002},
    {0x2510,0x8055},
    {0x2510,0x3001},
    {0x2510,0x2005},
    {0x2510,0x8C61},
    {0x2510,0x8801},
    {0x2510,0x1112},
    {0x2510,0x151F},
    {0x2510,0x091F},
    {0x2510,0x0036},
    {0x2510,0x986E},
    {0x2510,0x0023},
    {0x2510,0x10CC},
    {0x2510,0x00B9},
    {0x2510,0x0040},
    {0x2510,0x1035},
    {0x2510,0x1321},
    {0x2510,0x9826},
    {0x2510,0x00C8},
    {0x2510,0xC81B},
    {0x2510,0x2008},
    {0x2510,0x1048},
    {0x2510,0x3281},
    {0x2510,0x00AA},
    {0x2510,0x3044},
    {0x2510,0x11AB},
    {0x2510,0x102A},
    {0x2510,0x08C9},
    {0x2510,0x010C},
    {0x2510,0x1249},
    {0x2510,0x1154},
    {0x2510,0x1140},
    {0x2510,0x1041},
    {0x2510,0x9A25},
    {0x2510,0xBAA0},
    {0x2510,0xB06D},
    {0x2510,0x004D},
    {0x2510,0x1046},
    {0x2510,0x1020},
    {0x2510,0x0047},
    {0x2510,0xB064},
    {0x2510,0x1047},
    {0x2510,0x0025},
    {0x2510,0x99C5},
    {0x2510,0xC810},
    {0x2510,0x1025},
    {0x2510,0x00B5},
    {0x2510,0x7FFF},
    {0x2510,0x3250},
    {0x2510,0x8801},
    {0x2510,0x010F},
    {0x2510,0x8855},
    {0x2510,0x3101},
    {0x2510,0x3041},
    {0x2510,0x1051},
    {0x2510,0x3102},
    {0x2510,0x3041},
    {0x2510,0x2000},
    {0x2510,0x3181},
    {0x2510,0x3041},
    {0x2510,0x0014},
    {0x2510,0x3188},
    {0x2510,0x3041},
    {0x2510,0x003B},
    {0x2510,0x3282},
    {0x2510,0x3104},
    {0x2510,0x30C1},
    {0x2510,0xB0E4},
    {0x2510,0xA992},
    {0x2510,0x1007},
    {0x2510,0xB800},
    {0x2510,0x1025},
    {0x2510,0x0051},
    {0x2510,0x1014},
    {0x2510,0x1006},
    {0x2510,0x0033},
    {0x2510,0xC020},
    {0x2510,0xB1E0},
    {0x2510,0x0030},
    {0x2510,0x0007},
    {0x2510,0xAA5A},
    {0x2510,0xA028},
    {0x2510,0x8295},
    {0x2510,0x100F},
    {0x2510,0x0128},
    {0x2510,0x1002},
    {0x2510,0x02D6},
    {0x2510,0xA620},
    {0x2510,0x8891},
    {0x2510,0x11BD},
    {0x2510,0x1051},
    {0x2510,0x0023},
    {0x2510,0xCA36},
    {0x2510,0x10A9},
    {0x2510,0x99CD},
    {0x2510,0x2004},
    {0x2510,0x0C4E},
    {0x2510,0x113B},
    {0x2510,0xC000},
    {0x2510,0x0342},
    {0x2510,0x00C1},
    {0x2510,0x10D5},
    {0x2510,0x06C5},
    {0x2510,0x13C2},
    {0x2510,0x0295},
    {0x2510,0x9008},
    {0x2510,0x200E},
    {0x2510,0x06CB},
    {0x2510,0x000E},
    {0x2510,0x1022},
    {0x2510,0x1015},
    {0x2510,0xA84B},
    {0x2510,0x002C},
    {0x2510,0x80B9},
    {0x2510,0x104B},
    {0x2510,0x1056},
    {0x2510,0x0024},
    {0x2510,0x9B0F},
    {0x2510,0x05B5},
    {0x2510,0x0612},
    {0x2510,0x10AC},
    {0x2510,0x009A},
    {0x2510,0x3002},
    {0x2510,0x2006},
    {0x2510,0x0028},
    {0x2510,0x8091},
    {0x2510,0x1024},
    {0x2510,0x0021},
    {0x2510,0x002F},
    {0x2510,0xCD26},
    {0x2510,0x112D},
    {0x2510,0x9000},
    {0x2510,0x100E},
    {0x2510,0x1A28},
    {0x2510,0x122F},
    {0x2510,0x11B5},
    {0x2510,0x129D},
    {0x2510,0x1CD9},
    {0x2510,0x0015},
    {0x2510,0x074B},
    {0x2510,0x1021},
    {0x2510,0x0020},
    {0x2510,0x1095},
    {0x2510,0x1030},
    {0x2510,0x104B},
    {0x2510,0xB106},
    {0x2510,0xC490},
    {0x2510,0xA882},
    {0x2510,0x8255},
    {0x2510,0xB100},
    {0x2510,0xC802},
    {0x2510,0x8801},
    {0x2510,0x00A9},
    {0x2510,0x0022},
    {0x2510,0x0151},
    {0x2510,0x0181},
    {0x2510,0x98BB},
    {0x2510,0x2007},
    {0x2510,0x0D0A},
    {0x2510,0x0036},
    {0x2510,0x001D},
    {0x2510,0x9B3F},
    {0x2510,0x101B},
    {0x2510,0x00B9},
    {0x2510,0x0040},
    {0x2510,0x101F},
    {0x2510,0x104C},
    {0x2510,0x1001},
    {0x2510,0x3081},
    {0x2510,0x002A},
    {0x2510,0x108A},
    {0x2510,0x3044},
    {0x2510,0x1023},
    {0x2510,0x102A},
    {0x2510,0x2020},
    {0x2510,0x121D},
    {0x2510,0x04B8},
    {0x2510,0x10B8},
    {0x2510,0x1040},
    {0x2510,0xB500},
    {0x2510,0x01A3},
    {0x2510,0x02AB},
    {0x2510,0x004C},
    {0x2510,0x30D0},
    {0x2510,0x3141},
    {0x2510,0x3041},
    {0x2510,0x0044},
    {0x2510,0x3142},
    {0x2510,0x3041},
    {0x2510,0x2000},
    {0x2510,0x3110},
    {0x2510,0x2000},
    {0x2510,0x00B5},
    {0x2510,0x996B},
    {0x2510,0x3041},
    {0x2510,0x1044},
    {0x2510,0x3120},
    {0x2510,0x3041},
    {0x2510,0x1045},
    {0x2510,0x3144},
    {0x2510,0x3041},
    {0x2510,0x987B},
    {0x2510,0x3148},
    {0x2510,0x3041},
    {0x2510,0x2000},
    {0x2510,0x3182},
    {0x2510,0x3041},
    {0x2510,0x2000},
    {0x2510,0x3184},
    {0x2510,0x2000},
    {0x2510,0x3190},
    {0x2510,0x3041},
    {0x2510,0x2000},
    {0x2510,0x31A0},
    {0x2510,0x3088},
    {0x2510,0x2001},
    {0x2510,0x807D},
    {0x2510,0x2006},
    {0x2510,0x8815},
    {0x2510,0x8877},
    {0x2510,0x0992},
    {0x2510,0x2201},
    {0x2510,0x2000},
    {0x2510,0x2206},
    {0x2510,0x2004},
    {0x2510,0x8055},
    {0x2510,0x3001},
    {0x2510,0x2005},
    {0x2510,0x8C61},
    {0x2510,0x8801},
    {0x2510,0x1012},
    {0x2510,0x151F},
    {0x2510,0x089F},
    {0x2510,0x0057},
    {0x2510,0x0036},
    {0x2510,0x986E},
    {0x2510,0x0023},
    {0x2510,0x10CC},
    {0x2510,0x00B9},
    {0x2510,0x0040},
    {0x2510,0x1035},
    {0x2510,0x1321},
    {0x2510,0x9826},
    {0x2510,0x00C8},
    {0x2510,0xC85B},
    {0x2510,0x2008},
    {0x2510,0x10C8},
    {0x2510,0x002A},
    {0x2510,0x3281},
    {0x2510,0x2000},
    {0x2510,0x102B},
    {0x2510,0x3044},
    {0x2510,0x2001},
    {0x2510,0x102A},
    {0x2510,0x09C9},
    {0x2510,0x000C},
    {0x2510,0x1149},
    {0x2510,0x1157},
    {0x2510,0x10D4},
    {0x2510,0x1140},
    {0x2510,0x1041},
    {0x2510,0x9A25},
    {0x2510,0xBAA0},
    {0x2510,0xB06D},
    {0x2510,0x004D},
    {0x2510,0x1046},
    {0x2510,0x1020},
    {0x2510,0x0047},
    {0x2510,0xB064},
    {0x2510,0x1047},
    {0x2510,0x0025},
    {0x2510,0x99C5},
    {0x2510,0xC810},
    {0x2510,0x1025},
    {0x2510,0x00B5},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2510,0x7FFF},
    {0x2512,0x2000},
    {0x301A,0x001C},
    {SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the ar0246_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting ar0246_win_sizes[] = {
    {
        .width          = 1920,
        .height         = 1080,
        .fps            = 60 << 16 | 1,
        .mbus_code      = TISP_VI_FMT_SBGGR12_1X12,
        .colorspace     = TISP_COLORSPACE_SRGB,
        .regs           = ar0246_init_regs_1920_1080_60fps_mipi,
    },
};

static struct tx_isp_sensor_win_setting *wsize = &ar0246_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list ar0246_stream_on_mipi[] = {
    {0x0100, 0x0001},
    {SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list ar0246_stream_off_mipi[] = {
    {0x0100, 0x0000},
    {SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int ar0246_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int ar0246_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int ar0246_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    unsigned char val;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = ar0246_read(sd, vals->reg_num, &val);
            /* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif

static int ar0246_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = ar0246_write(sd, vals->reg_num, vals->value);
            printk("{0x%04x,0x%04x},\n", vals->reg_num, vals->value);
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int ar0246_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int ar0246_read16(struct tx_isp_subdev *sd, uint16_t reg,
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

int ar0246_write(struct tx_isp_subdev *sd, uint16_t reg,
                  uint16_t value)
{
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    uint8_t buf[4] = {(reg >> 8) & 0xff, reg & 0xff, (value >> 8) & 0xff, value & 0xff};
    struct i2c_msg msg = {
        .addr   = client->addr,
        .flags  = 0,
        .len    = 4,
        .buf    = buf,
    };
    int ret;
    ret = private_i2c_transfer(client->adapter, &msg, 1);
    if (ret > 0)
        ret = 0;

    return ret;
}

#if 0
static int ar0246_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    unsigned char val;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = ar0246_read(sd, vals->reg_num, &val);
            if (ret < 0)
                return ret;
        }
        vals++;
    }
    return 0;
}
#endif

static int ar0246_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = ar0246_write(sd, vals->reg_num, vals->value);
            printk("{%04x,%04x},\n", vals->reg_num, vals->value);
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int ar0246_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int ar0246_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int ar0246_setting_select(struct tx_isp_subdev *sd, int deboot)
{
    int ret = ISP_SUCCESS;

    switch (deboot) {
    case 0:
        wsize = &ar0246_win_sizes[0];
        ar0246_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

        ar0246_attr.again.value = 65536;
        ar0246_attr.again.max = 453089;
        ar0246_attr.again.min = 0;
        ar0246_attr.again.apply_delay = 2;

        ar0246_attr.integration_time.value = 0xb60;
        ar0246_attr.integration_time.max = 0x479 - 4;
        ar0246_attr.integration_time.min = 1;
        ar0246_attr.integration_time.apply_delay = 2;

       ar0246_attr.total_width = 0x870;
        ar0246_attr.total_height = 0x479;

        ar0246_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
        ar0246_attr.hcg.base_gain = ;
        ar0246_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
        ar0246_attr.again_short.value = ;
        ar0246_attr.again_short.max = ;
        ar0246_attr.again_short.min = ;
        ar0246_attr.again_short.apply_delay = ;

        ar0246_attr.integration_time_short.value = ;
        ar0246_attr.integration_time_short.max = ;
        ar0246_attr.integration_time_short.min = ;
        ar0246_attr.integration_time_short.apply_delay = ;

        ar0246_attr.wdr_cache = wdr_line * ar0246_attr.total_width;

#ifdef SENSOR_HCG
        ar0246_attr.hcg_short.base_gain = ;
        ar0246_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

        memcpy((void *)(&(ar0246_attr.mipi)), (void *)(&ar0246_mipi_linear), sizeof(ar0246_attr.mipi));
        break;
    default:
        ISP_ERROR("Have no this Setting Source!!!\n");
        break;
    }

    return ret;
}

static int ar0246_attr_check(struct tx_isp_subdev *sd)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    struct clk *sclka;
    int ret = ISP_SUCCESS;

    ar0246_setting_select(sd, info->default_boot);

    switch (info->video_interface) {
    case TISP_SENSOR_VI_MIPI_CSI0:
        ar0246_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
        ar0246_attr.mipi.index = 0;
        break;
    case TISP_SENSOR_VI_MIPI_CSI1:
        ar0246_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
        ar0246_attr.mipi.index = 1;
        break;
    case TISP_SENSOR_VI_DVP:
        ar0246_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

    ret = ar0246_clk_set(sd, sclka, SENSOR_MCLK);
    if (ret) {
        ISP_ERROR("MCLK configuration failed!!!\n");
    }
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
    ar0246_attr_set(sd, wsize);
    sensor->priv = wsize;

    return 0;

err_get_mclk:
    return -1;
}

static int ar0246_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
    unsigned char v;
    int ret;

    ret = ar0246_read(sd, 0x3000, &v);
    pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    if (ret < 0)
        return ret;
    /* if (v != SENSOR_CHIP_ID_H) */
    /*     return -ENODEV; */
    /* *ident = v; */

    ret = ar0246_read(sd, 0x3001, &v);
    pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    if (ret < 0)
        return ret;
    /* if (v != SENSOR_CHIP_ID_L) */
    /*     return -ENODEV; */
    /* *ident = (*ident << 8) | v; */

    return 0;
}

static int ar0246_g_chip_ident(struct tx_isp_subdev *sd,
                                struct tx_isp_chip_ident *chip)
{
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    unsigned int ident = 0;
    int ret = ISP_SUCCESS;

    ar0246_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
    if (info->rst_gpio != -1) {
        ret = private_gpio_request(info->rst_gpio, "ar0246_reset");
        if (!ret) {
            private_gpio_direction_output(info->rst_gpio, 1);
            private_msleep(5);
            private_gpio_direction_output(info->rst_gpio, 0);
            private_msleep(5);
            private_gpio_direction_output(info->rst_gpio, 1);
            private_msleep(5);
        } else {
            ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
        }
    }
    if (info->pwdn_gpio != -1) {
        ret = private_gpio_request(info->pwdn_gpio, "ar0246_pwdn");
        if (!ret) {
            private_gpio_direction_output(info->pwdn_gpio, 0);
            private_msleep(5);
            private_gpio_direction_output(info->pwdn_gpio, 1);
            private_msleep(5);
        } else {
            ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
        }
    }
    ret = ar0246_detect(sd, &ident);
    if (ret) {
        ISP_ERROR("chip found @ 0x%x (%s) is not an ar0246 chip.\n",
                  client->addr, client->adapter->name);
        return ret;
    }
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    ISP_WARNING("===================================================\n");
    ISP_WARNING("Template version is %s\n", TVERSION);
    ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
    ISP_WARNING("Sensor name is %s\n", ar0246_attr.name);
    ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
    ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
    ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
                info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
    ISP_WARNING("===================================================\n");

    if (chip) {
        memcpy(chip->name, "ar0246", sizeof("ar0246"));
        chip->ident = ident;
        chip->revision = SENSOR_VERSION;
    }

    return 0;
}

static int ar0246_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
    return 0;
}

static int ar0246_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
        ret = ar0246_attr_set(sd, wsize);
        sensor->video.state = TX_ISP_MODULE_DEINIT;
    }

    return ret;
}

static int ar0246_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
    ret = ar0246_read16(sd, reg->reg & 0xffff, &val);
    reg->val = val;
    reg->size = 2;

    return ret;
}

static int ar0246_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
    int len = 0;

    len = strlen(sd->chip.name);
    if (len && strncmp(sd->chip.name, reg->name, len)) {
        return -EINVAL;
    }
    if (!private_capable(CAP_SYS_ADMIN))
        return -EPERM;
    ar0246_write(sd, reg->reg & 0xffff, reg->val & 0xffff);

    return 0;
}

static int ar0246_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    int ret = ISP_SUCCESS;

    if (init->enable) {
        if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
            ret = ar0246_write_array(sd, wsize->regs);
            printk("%s %d ret = %d\n", __func__, __LINE__, ret);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
            if (ret)
                return ret;
            sensor->video.state = TX_ISP_MODULE_INIT;
        }
        if (sensor->video.state == TX_ISP_MODULE_INIT) {
            ret = ar0246_write_array(sd, ar0246_stream_on_mipi);
            sensor->video.state = TX_ISP_MODULE_RUNNING;
            pr_debug("ar0246 stream on\n");
        }

    } else {
        ret = ar0246_write_array(sd, ar0246_stream_off_mipi);
        sensor->video.state = TX_ISP_MODULE_INIT;
        pr_debug("ar0246 stream off\n");
    }

    return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int ar0246_set_expo(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;
    int it = value & 0xffff;
    int again = (value & 0xffff0000) >> 16;
    printk("it =  %d,again = %d \n", it, again);

    ret += ar0246_write(sd, 0x3012, (unsigned char)it);

    ret += ar0246_write(sd, 0x5900, (unsigned char)again);

    return ret;
}
#else
static int ar0246_set_integration_time(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    value = (value << 1) - 1;
    ret += ar0246_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
    ret += ar0246_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
    ret += ar0246_write(sd, 0x3e02, (unsigned char)((value & 0xf) << 4));

    return ret;
}

static int ar0246_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ret += ar0246_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
    ret += ar0246_write(sd, 0x3e09, (unsigned char)(value & 0xff));

    return ret;
}
#endif /* SENSOR_EXPO */

static int ar0246_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
    return 0;
}

static int ar0246_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
    return 0;
}

static int ar0246_set_mode(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    if (wsize) {
        ret = ar0246_attr_set(sd, wsize);
    }

    return ret;
}

static int ar0246_set_fps(struct tx_isp_subdev *sd, int fps)
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
        sclk = 0x0870 * 0x0479 * 60;  /**< HTS * VTS * FPS */
        max_fps = 60;
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
    ret += ar0246_read(sd, 0x300c, &val);
    hts = val << 8;
    val = 0;
    ret += ar0246_read(sd, 0x300d, &val);
    hts |= val;
    if (0 != ret) {
        ISP_ERROR("err: ar0246 read err\n");
        return ret;
    }

    vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

    printk("hts=%d,vts=%d,fps=%d\n", hts, vts, ((fps >> 16) & 0xffff)/(fps & 0xffff));
    ar0246_write(sd, 0x300a, (uint16_t)vts);

    if (0 != ret) {
        ISP_ERROR("err: ar0246_write err\n");
        return ret;
    }

    /* sensor->video.fps = fps; */
    sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
    /* Linear mode */
    sensor->video.attr->total_height = vts;
    sensor->video.attr->integration_time.max = vts - 4;
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

static int ar0246_set_hvflip(struct tx_isp_subdev *sd, void *arg)
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
    ret = ar0246_read(sd, 0x3040, &val);
    mode = val << 8;
    ret = ar0246_read(sd, 0x3041, &val);
    mode += val;
    switch(par->hvflip) {
    case TX_ISP_SENSOR_HVFLIP_NOMAL:
        mode &= 0x3fff;
        break;
    case TX_ISP_SENSOR_HVFLIP_HFLIP:
        mode &= 0x3fff;
        mode |= 0x4000;
        break;
    case TX_ISP_SENSOR_HVFLIP_VFLIP:
        mode &= 0x3fff;
        mode |= 0x8000;
        break;
    case TX_ISP_SENSOR_HVFLIP_HVFLIP:
        mode |= 0xc000;
        break;
    }
    ret += ar0246_write(sd, 0x3040, mode);

    sensor->video.hvflip_mode = par->hvflip;
    ar0246_attr_set(sd, wsize);

    return ret;
#else
    return -1;
#endif /* SENSOR_MIR_FLIP */
}

static int ar0246_pm_s_stream_ctrl(struct tx_isp_subdev *sd, void *arg)
{
    int ret = ISP_SUCCESS;
    IMPISPUserCtl *data = (IMPISPUserCtl *)arg;

    if(data->cmd == TX_SENSOR_PM_RESUME){

        // ret = ar0246_write_array(sd, wsize->regs);
        // if (ret < 0){
        //     printk("ar0246 resume setting failed\n");
        //     return ret;
        // }
        // ret = ar0246_resume(sd);
        // if(ret != 0){
        //     printk("ar0246 resume attr failed\n");
        //     return ret;
        // }

        ret = ar0246_write_array(sd, ar0246_stream_on_mipi);
        if(ret != 0){
            printk("ar0246 streamon failed\n");
            return ret;
        }

        printk("ar0246 TX_SENSOR_PM_RESUME\n");

    }else if(data->cmd == TX_SENSOR_PM_SUSPEND) {
        ret = ar0246_write_array(sd, ar0246_stream_off_mipi);
        if (ret < 0) {
            printk("\n TX_SENSOR_PM_SUSPEND error !!!\n");
            return ret;
        }
        printk("ar0246 TX_SENSOR_PM_SUSPEND !!!\n");
    }else if(data->cmd == TX_SENSOR_PM_PREPARE) {
        printk("\n ar0246 TX_SENSOR_PM_PREPARE !!!\n");
    }else if(data->cmd == TX_SENSOR_PM_COMPLETE) {
        printk("\n ar0246 TX_SENSOR_PM_COMPLETE !!!\n");
    }else if(data->cmd == TX_SENSOR_PM_STREAMON) {
        ret = ar0246_write_array(sd, ar0246_stream_on_mipi);
        if (ret < 0) {
            printk("\n TX_SENSOR_PM_STREAMON error !!!\n");
            return ret;
        }
        printk("\n ar0246 TX_SENSOR_PM_STREAMON !!!\n");
    }else if(data->cmd == TX_SENSOR_PM_STREAMOFF) {
        ret = ar0246_write_array(sd, ar0246_stream_off_mipi);
        if (ret < 0) {
            printk("\n [%s %d] TX_SENSOR_PM_STREAMOFF error !!!\n", __func__, __LINE__);
            return ret;
        }
        printk("\n ar0246 TX_SENSOR_PM_STREAMOFF !!!\n");
    }else {
        printk("\n==> Don't Support this function !!! \n");
        return -EINVAL;
    }

    return ret;
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int ar0246_set_expo_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}
#else
static int ar0246_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}

static int ar0246_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}
#endif /* SENSOR_EXPO */

static int ar0246_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
    struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
    int ret = ISP_SUCCESS;

    ret = ar0246_write_array(sd, ar0246_stream_off_mipi);
    if (par->wdr_en == 1) {
        if (par->boot == -1) {
            info->default_boot = 1;
        }
        ar0246_setting_select(sd, 1);
        ar0246_attr_set(sd, wsize);
    } else if (par->wdr_en == 0) {
        if (par->boot == -1) {
            info->default_boot = 0;
        }
        ar0246_setting_select(sd, 0);
        ar0246_attr_set(sd, wsize);
    } else {
        ISP_ERROR("Can not support this data type!!!");
        return -1;
    }

    return 0;
}

static int ar0246_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
    struct tx_isp_sensor_register_info *info = &sensor->info;
    int ret = ISP_SUCCESS;

    private_gpio_direction_output(info->rst_gpio, 0);
    private_msleep(1);
    private_gpio_direction_output(info->rst_gpio, 1);
    private_msleep(1);

    ret = ar0246_write_array(sd, wsize->regs);
    ret = ar0246_write_array(sd, ar0246_stream_on_mipi);

    return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int ar0246_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
            ret = ar0246_set_expo(sd, sensor_val->value);
        break;
#else
    case TX_ISP_EVENT_SENSOR_INT_TIME:
        if (arg)
            ret = ar0246_set_integration_time(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_AGAIN:
        if (arg)
            ret = ar0246_set_analog_gain(sd, sensor_val->value);
        break;
#endif /* SENSOR_EXPO */
    case TX_ISP_EVENT_SENSOR_DGAIN:
        if (arg)
            ret = ar0246_set_digital_gain(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
        if (arg)
            ret = ar0246_get_black_pedestal(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_RESIZE:
        if (arg)
            ret = ar0246_set_mode(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
        if (arg)
            ret = ar0246_write_array(sd, ar0246_stream_off_mipi);
        break;
    case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
        if (arg)
            ret = ar0246_write_array(sd, ar0246_stream_on_mipi);
        break;
    case TX_ISP_EVENT_SENSOR_FPS:
        if (arg)
            ret = ar0246_set_fps(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_HVFLIP:
        if(arg)
            ret = ar0246_set_hvflip(sd, (void *)sensor_val->value);
        break;
    case TX_ISP_EVENT_PM_S_STREAM_CTRL:
        if(arg)
            ret =  ar0246_pm_s_stream_ctrl(sd, arg);
        break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
    case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
        if (arg)
            ret = ar0246_set_expo_short(sd, sensor_val->value);
        break;
#else
    case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
        if(arg)
            ret = ar0246_set_integration_time_short(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
        if(arg)
            ret = ar0246_set_analog_gain_short(sd, sensor_val->value);
        break;
#endif /* SENSOR_EXPO */
    case TX_ISP_EVENT_SENSOR_WDR:
        if(arg)
            ret = ar0246_set_wdr(sd, (void *)sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_WDR_STOP:
        if(arg)
            ret = ar0246_set_wdr_stop(sd, (void *)sensor_val->value);
        break;
#endif /* SENSOR_WDR_2_FRAME */
    default:
        break;
    }

    return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops ar0246_core_ops = {
    .g_chip_ident = ar0246_g_chip_ident,
    .reset = ar0246_reset,
    .init = ar0246_init,
    .g_register = ar0246_g_register,
    .s_register = ar0246_s_register,
};

static struct tx_isp_subdev_video_ops ar0246_video_ops = {
    .s_stream = ar0246_s_stream,
};

static struct tx_isp_subdev_sensor_ops ar0246_sensor_ops = {
#ifndef SENSOR_TEST
    .ioctl  = ar0246_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops ar0246_ops = {
    .core = &ar0246_core_ops,
    .video = &ar0246_video_ops,
    .sensor = &ar0246_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
    .name = "ar0246",
    .id = -1,
    .dev = {
        .dma_mask = &tx_isp_module_dma_mask,
        .coherent_dma_mask = 0xffffffff,
        .platform_data = NULL,
    },
    .num_resources = 0,
};

static int ar0246_probe(struct i2c_client *client,
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
    sensor->video.attr = &ar0246_attr;
    sensor->dev = &client->dev;
    tx_isp_subdev_init(&sensor_platform_device, sd, &ar0246_ops);
    tx_isp_set_subdevdata(sd, client);
    tx_isp_set_subdev_hostdata(sd, sensor);
    private_i2c_set_clientdata(client, sd);

    pr_debug("probe ok ------->ar0246\n");

    return 0;
}

static int ar0246_remove(struct i2c_client *client)
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

static const struct i2c_device_id ar0246_id[] = {
    {"ar0246", 0},
    {}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, ar0246_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver ar0246_driver = {
    .driver = {
#ifdef CONFIG_ZERATUL
        .owner  = NULL,
#else
        .owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
        .name   = "ar0246",
    },
    .probe          = ar0246_probe,
    .remove         = ar0246_remove,
    .id_table       = ar0246_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_ar0246(void) {
    return private_i2c_add_driver(&ar0246_driver);
}

static __exit void exit_ar0246(void) {
    private_i2c_del_driver(&ar0246_driver);
}

module_init(init_ar0246);
module_exit(exit_ar0246);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_ar0246(void) {
    return private_i2c_add_driver(&ar0246_driver);
}

static void exit_first_ar0246(void) {
    private_i2c_del_driver(&ar0246_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
    .name = "ar0246",
    .i2c_addr = 0x30,
    .width = 1920,
    .height = 1080,
#ifdef SENSOR_WDR_2_FRAME
    .wdr = 1,
#else
    .wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
    .init_sensor = init_first_ar0246,
    .exit_sensor = exit_first_ar0246
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for ar0246 sensor");
MODULE_LICENSE("GPL");
