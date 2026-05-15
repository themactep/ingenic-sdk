/*
 * os08c10.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          3840*2160       20        mipi_2lane             linear
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

#define TVERSION "V20231127a"
#define SENSOR_VERSION  "H20231212a"

//#define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */

#define SENSOR_CHIP_ID_H    (0x53)
#define SENSOR_CHIP_ID_M    (0x08)
#define SENSOR_CHIP_ID_L    (0x43)
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_MCLK 24000000

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
        uint8_t value;
};

struct tx_isp_sensor_attribute os08c10_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
        unsigned int value;
        unsigned int gain;
};

struct again_lut os08c10_again_lut[] = {
	{0x80, 0},
	{0x88, 5731},
	{0x90, 11135},
	{0x98, 16247},
	{0xa0, 21097},
	{0xa8, 25710},
	{0xb0, 30108},
	{0xb8, 34311},
	{0xc0, 38335},
	{0xc8, 42195},
	{0xd0, 45903},
	{0xd8, 49472},
	{0xe0, 52910},
	{0xe8, 56228},
	{0xf0, 59433},
	{0xf8, 62534},
	{0x100, 65536},
	{0x110, 71267},
	{0x120, 76671},
	{0x130, 81783},
	{0x140, 86633},
	{0x150, 91246},
	{0x160, 95644},
	{0x170, 99847},
	{0x180, 103871},
	{0x190, 107731},
	{0x1a0, 111439},
	{0x1b0, 115008},
	{0x1c0, 118446},
	{0x1d0, 121764},
	{0x1e0, 124969},
	{0x1f0, 128070},
	{0x200, 131072},
	{0x220, 136803},
	{0x240, 142207},
	{0x260, 147319},
	{0x280, 152169},
	{0x2a0, 156782},
	{0x2c0, 161180},
	{0x2e0, 165383},
	{0x300, 169407},
	{0x320, 173267},
	{0x340, 176975},
	{0x360, 180544},
	{0x380, 183982},
	{0x3a0, 187300},
	{0x3c0, 190505},
	{0x3e0, 193606},
	{0x400, 196608},
	{0x440, 202339},
	{0x480, 207743},
	{0x4c0, 212855},
	{0x500, 217705},
	{0x540, 222318},
	{0x580, 226716},
	{0x5c0, 230919},
	{0x600, 234943},
	{0x640, 238803},
	{0x680, 242511},
	{0x6c0, 246080},
	{0x700, 249518},
	{0x740, 252836},
	{0x780, 256041},
	{0x7c0, 259142},
	{0x7ff, 262080},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int os08c10_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
        /* Analog gain table */
        struct again_lut *lut = os08c10_again_lut;
        while (lut->gain <= os08c10_attr.max_again) {
                if (isp_gain == 0) {
                        *sensor_again = 0;
                        return 0;
                } else if (isp_gain < lut->gain) {
                        *sensor_again = (lut - 1)->value;
                        return (lut - 1)->gain;
                } else {
                        if ((lut->gain == os08c10_attr.max_again) && (isp_gain >= lut->gain)) {
                                *sensor_again = lut->value;
                                return lut->gain;
                        }
                }

                lut++;
        }

#else
        /* Non analog gain table */
#endif  /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

        return isp_gain;
}

#ifdef SENSOR_WDR_2_FRAME
unsigned int os08c10_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
        /* Analog gain table */
        struct again_lut *lut = os08c10_again_lut;
        while(lut->gain <= os08c10_attr.max_again_short) {
                if(isp_gain == 0) {
                        *sensor_again = 0;
                        return 0;
                }
                else if(isp_gain < lut->gain) {
                        *sensor_again = (lut - 1)->value;
                        return (lut - 1)->gain;
                }
                else{
                        if((lut->gain == os08c10_attr.max_again_short) && (isp_gain >= lut->gain)) {
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

unsigned int os08c10_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
        return 0;
}

struct tx_isp_mipi_bus os08c10_mipi_linear = {
        .mode = SENSOR_MIPI_OTHER_MODE,
        .clk = 1296,
        .lans = 2,
        .image_twidth = 3840,
        .image_theight = 2160,
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

struct tx_isp_dvp_bus os08c10_dvp = {
        .gpio = DVP_PA_LOW_10BIT,
        .mode = SENSOR_DVP_HREF_MODE,
        .blanking = {
                .hblanking = 0,
                .vblanking = 0,
        },
        .polar = {
                .hsync_polar = 0,
                .vsync_polar = 0,
                .pclk_polar = 0, /**< reserved */
        },
        .dvp_hcomp_en = 0,
};

struct tx_isp_sensor_attribute os08c10_attr = {
        .name = "os08c10",
        .chip_id = 0x530843,
        .cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
        .cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
        .cbus_device = 0x36,
        .sensor_ctrl.alloc_again = os08c10_alloc_again,
        .sensor_ctrl.alloc_dgain = os08c10_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
        .sensor_ctrl.alloc_again_short = os08c10_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */
};

static struct regval_list os08c10_init_regs_3840_2160_20fps_mipi[] = {
	{0x0100,0x00},
	{0x0103,0x01},
	{0x4a11,0xeb},
	{0x4a13,0xe6},
	{0x4a15,0xe6},
	{0x4a17,0xeb},
	{0x4a19,0xf0},
	{0x4a1b,0xf0},
	{0x4a1d,0xfa},
	{0x4a1f,0xfd},
	{0x4a21,0x64},
	{0x4a23,0x64},
	{0x4a25,0x64},
	{0x4a27,0x64},
	{0x4a29,0x64},
	{0x4a2b,0x64},
	{0x4a2d,0x64},
	{0x4a2f,0x64},
	{0x4a31,0xeb},
	{0x4a33,0xe4},
	{0x4a35,0xe7},
	{0x4a37,0xeb},
	{0x4a39,0xfa},
	{0x4a3b,0xfa},
	{0x4a3d,0xfa},
	{0x4a3f,0xfa},
	{0x0102,0x01},
	{0x0301,0x43},
	{0x0304,0x01},
	{0x0305,0x44},
	{0x0307,0x00},
	{0x030a,0x00},
	{0x0326,0xc7},
	{0x0360,0x01},
	{0x3012,0x21},
	{0x3015,0x82},
	{0x301b,0xb4},
	{0x3021,0xe1},
	{0x3023,0xf0},
	{0x3216,0x10},
	{0x3218,0x08},
	{0x3501,0x05},//
	{0x3502,0x20},//it:0x520
	{0x3506,0x73},
	{0x3541,0x00},
	{0x3542,0x10},
	{0x3504,0x4c},
	{0x3507,0x00},
	{0x3508,0x08},//
	{0x3509,0x00},//again:0x800
	{0x3544,0x4c},
	{0x3546,0x73},
	{0x3548,0x01},
	{0x3549,0x00},
	{0x3588,0x00},
	{0x3589,0x00},
	{0x360e,0xe7},
	{0x360f,0x20},
	{0x3613,0x86},
	{0x3626,0x8d},
	{0x3616,0x77},
	{0x3617,0x26},
	{0x3620,0x84},
	{0x3621,0xa0},
	{0x3674,0x01},
	{0x3680,0x1c},
	{0x3681,0x20},
	{0x3682,0x01},
	{0x3683,0x20},
	{0x3684,0x74},
	{0x3685,0x44},
	{0x368a,0x01},
	{0x368b,0xb0},
	{0x368c,0x38},
	{0x368d,0xcc},
	{0x36a2,0x41},
	{0x36ab,0x00},
	{0x36ac,0x00},
	{0x36a3,0x00},
	{0x36a4,0x00},
	{0x36a5,0x00},
	{0x3400,0x0c},
	{0x3421,0x24},
	{0x3422,0x02},
	{0x3423,0x00},
	{0x3424,0x45},
	{0x3425,0x41},
	{0x3426,0x11},
	{0x3427,0x05},
	{0x3428,0x01},
	{0x3429,0x00},
	{0x3700,0x38},
	{0x3701,0x18},
	{0x3702,0x59},
	{0x3703,0x12},
	{0x3704,0x07},
	{0x3705,0x00},
	{0x3706,0x26},
	{0x3707,0x08},
	{0x3708,0x71},
	{0x370a,0x00},
	{0x370b,0x3f},
	{0x370c,0x0f},
	{0x3711,0x30},
	{0x3712,0xb9},
	{0x3714,0x28},
	{0x3716,0xb8},
	{0x3717,0xb1},
	{0x371a,0x1c},
	{0x371c,0x02},
	{0x371d,0x07},
	{0x371e,0x12},
	{0x371f,0x07},
	{0x3720,0x04},
	{0x3721,0x02},
	{0x3725,0x22},
	{0x3727,0x22},
	{0x372b,0x10},
	{0x3732,0x04},
	{0x3734,0xff},
	{0x3736,0xff},
	{0x3743,0x00},
	{0x3754,0x01},
	{0x3755,0x0d},
	{0x3760,0x04},
	{0x3761,0x18},
	{0x3762,0x04},
	{0x3763,0x04},
	{0x3764,0x04},
	{0x3765,0x1c},
	{0x3766,0x04},
	{0x3767,0x1c},
	{0x3768,0x20},
	{0x3769,0x24},
	{0x376c,0x00},
	{0x376b,0x20},
	{0x376e,0x02},
	{0x3780,0x01},
	{0x3789,0x80},
	{0x37a5,0x05},
	{0x37f8,0x3c},
	{0x37fc,0xc0},
	{0x37ff,0x08},
	{0x4d00,0x03},
	{0x4d01,0xea},
	{0x4d02,0xb8},
	{0x4d03,0xee},
	{0x4d04,0x64},
	{0x4d05,0x3f},
	{0xc025,0x49},
	{0x3800,0x00},
	{0x3801,0x00},
	{0x3802,0x00},
	{0x3803,0x00},
	{0x3804,0x0f},
	{0x3805,0x1f},
	{0x3806,0x08},
	{0x3807,0x8f},
	{0x3808,0x0f},
	{0x3809,0x00},
	{0x380a,0x08},
	{0x380b,0x70},
	{0x380c,0x0b},//hts:0xba0=2976
	{0x380d,0xa0},//
	{0x380e,0x09},//vts:0x9d8=2520
	{0x380f,0xd8},//
	{0x3810,0x00},
	{0x3811,0x0f},
	{0x3812,0x00},
	{0x3813,0x10},
	{0x3814,0x11},
	{0x3815,0x11},
	{0x381a,0x12},
	{0x381b,0x82},
	{0x381f,0x08},
	{0x3820,0x80},
	{0x3821,0x04},
	{0x3822,0x80},
	{0x3823,0x04},
	{0x3828,0x08},
	{0x3831,0x00},
	{0x3837,0x09},
	{0x383f,0x00},
	{0x3847,0x00},
	{0x3894,0x00},
	{0x389c,0x00},
	{0x38a4,0x00},
	{0x38a5,0x00},
	{0x38a6,0x00},
	{0x38a7,0x00},
	{0x38a8,0x00},
	{0x38a9,0x00},
	{0x38ab,0x00},
	{0x38ac,0x30},
	{0x3d8c,0x73},
	{0x3d8d,0xa8},
	{0x3daa,0x70},
	{0x3dab,0x14},
	{0x3dac,0x00},
	{0x3dad,0x00},
	{0x3dae,0x73},
	{0x3daf,0x97},
	{0x400e,0x1c},
	{0x4010,0xf4},
	{0x4011,0x00},
	{0x4012,0x77},
	{0x401b,0x04},
	{0x4009,0x01},
	{0x401e,0x01},
	{0x401f,0x10},
	{0x4015,0x08},
	{0x4016,0x27},
	{0x4017,0x02},
	{0x4018,0x05},
	{0x4058,0x01},
	{0x4059,0x00},
	{0x405a,0x3f},
	{0x405b,0x4f},
	{0x405c,0x03},
	{0x405d,0x80},
	{0x4288,0xc3},
	{0x429f,0x00},
	{0x42a0,0x00},
	{0x480e,0x04},
	{0x481b,0x40},
	{0x481f,0x30},
	{0x4823,0x40},
	{0x4837,0x0c},
	{0x4850,0x42},
	{0x4880,0x00},
	{0x4881,0x00},
	{0x4884,0x09},
	{0x488b,0x10},
	{0x4500,0x04},
	{0x4501,0x00},
	{0x4502,0x00},
	{0x4503,0x00},
	{0x4504,0x00},
	{0x4505,0x00},
	{0x4508,0x00},
	{0x450a,0x04},
	{0x450c,0x00},
	{0x450e,0x00},
	{0x450f,0x00},
	{0x4544,0x00},
	{0x4564,0x00},
	{0x4a00,0xa1},
	{0x4b00,0x2b},
	{0x4b02,0x80},
	{0x4b03,0x80},
	{0x4b04,0x10},
	{0x4b05,0x10},
	{0x4b0a,0x08},
	{0x4b0d,0x00},
	{0x4b18,0xc8},
	{0x4604,0x44},
	{0x4680,0x01},
	{0x4683,0x2b},
	{0x468f,0x00},
	{0x4800,0x04},
	{0x4815,0x40},
	{0x4816,0x12},
	{0x4090,0xf4},
	{0x4091,0x00},
	{0x4092,0x97},
	{0x5000,0x0f},
	{0x5007,0x01},
	{0x5080,0x00},
	{0x50c0,0x00},
	{0x5398,0x00},
	{0x53a3,0x01},
	{0x5200,0x70},
	{0x5201,0x14},
	{0x5202,0x73},
	{0x5203,0x97},
	{0x5240,0x70},
	{0x5241,0x14},
	{0x5242,0x73},
	{0x5243,0x97},
	{0x5244,0x70},
	{0x5245,0x14},
	{0x5246,0x73},
	{0x5247,0x97},
	{0x5002,0x00},
	{0x5003,0x40},
	{0x5004,0x00},
	{0x5005,0x40},
	{0x5006,0x00},
	{0x5023,0x01},
	{0x5074,0xdf},
	{0x507a,0xff},
	{0x507b,0xff},
	{0x53a0,0x00},
	{0x5420,0x00},
	{0x53a3,0x01},
	{0x5423,0x01},
	{0x5800,0x28},
	{0x5801,0x0c},
	{0x5802,0x08},
	{0x5803,0x08},
	{0x5804,0x09},
	{0x5805,0x0a},
	{0x5806,0x0b},
	{0x5807,0x0c},
	{0x5808,0x0d},
	{0x5809,0x0e},
	{0x580a,0x0f},
	{0x5826,0x01},
	{0x5827,0x00},
	{0x582a,0x00},
	{0x582b,0x80},
	{0x582e,0x00},
	{0x582f,0x80},
	{0x5832,0x01},
	{0x5833,0x00},
	{0x5836,0x01},
	{0x5837,0x00},
	{0x583a,0x02},
	{0x583b,0x00},
	{0x583e,0x02},
	{0x583f,0x00},
	{0x5842,0x04},
	{0x5843,0x00},
	{0x5846,0x03},
	{0x5847,0xff},
	{0x5781,0x58},
	{0x57a4,0x00},
	{0x57a5,0x20},
	{0x57a6,0x07},
	{0x57a7,0xff},
	{0x57a8,0x00},
	{0x57a9,0x20},
	{0x57aa,0x03},
	{0x57ab,0xb8},
	{0x57bc,0x02},
	{0x57bd,0x00},
	{0x57be,0x02},
	{0x57bf,0x76},
	{0x57c0,0x03},
	{0x57c1,0x08},
	{0x57c2,0x00},
	{0x57c3,0x40},
	{0x57c4,0x01},
	{0x57c5,0x00},
	{0x57c6,0x02},
	{0x57c7,0x00},
	{0xc08b,0x07},
	{0xc08f,0x07},
	{0xc09b,0x00},
	{0xc0bd,0x44},
	{0xc0c6,0x04},
	{0xc0d3,0x44},
	{0xc0d7,0x0e},
	{0xc0da,0x05},
	{0xc0e7,0x00},
	{0xc0e8,0x02},
	{0xc0e9,0x02},
	{0xc0eb,0x00},
	{0xc0f2,0x02},
	{0xc0ff,0x07},
	{0xc103,0x07},
	{0xc155,0x03},
	{0xc211,0x0a},
	{0xc30b,0x06},
	{0xc311,0x10},
	{0xc334,0x33},
	{0xc335,0x33},
	{0xc336,0x33},
	{0xc337,0x33},
	{0xc338,0x33},
	{0xc339,0x33},
	{0xc33f,0x77},
	{0xc340,0x77},
	{0xc341,0x77},
	{0xc347,0xff},
	{0xc348,0xff},
	{0xc349,0xff},
	{0xc359,0xfc},
	{0xc39a,0x55},
	{0xc39b,0x50},
	{0xc3a4,0x2c},
	{0xc3a5,0x2c},
	{0xc3a6,0x2c},
	{0xc3a7,0x2c},
	{0xc3a8,0x2c},
	{0xc3a9,0x2c},
	{0xc3b6,0x08},
	{0xc3b7,0x08},
	{0xc3b8,0x08},
	{0xc3b9,0x08},
	{0xc3ba,0x08},
	{0xc3bb,0x08},
	{0xc3c3,0x86},
	{0xc3ca,0x75},
	{0xc3cb,0x75},
	{0xc3cc,0x75},
	{0xc3cd,0x75},
	{0xc3ce,0x75},
	{0xc3cf,0x75},
	{0xc3e8,0x75},
	{0xc3e9,0x75},
	{0xc3ea,0x75},
	{0xc3eb,0x75},
	{0xc3ec,0x75},
	{0xc3ed,0x75},
	{0xc48e,0x1e},
	{0xc48f,0x1e},
	{0xc490,0x1e},
	{0xc491,0x1e},
	{0xc012,0x79},
	{0xc013,0x10},
	{0xc016,0x04},
	{0xc01c,0x07},
	{0xc01d,0xff},
	{0xc02a,0x00},
	{0xc00a,0x05},
	{0xc00c,0x40},
	{0xc00f,0x1f},
	{0xc010,0x86},
	{0xc00b,0x54},
	{0xc31d,0x26},
	{0xc46e,0x33},
	{0xc46f,0x33},
	{0xc48c,0x18},
	{0xc48d,0x1b},
	{0xc48e,0x30},
	{0xc48f,0x30},
	{0xc490,0x30},
	{0xc491,0x30},
	{0xc361,0x0f},
	{0xc362,0x18},
	{0xc363,0x1b},
	{0xc364,0x1b},
	{0xc365,0x1c},
	{0xc366,0x1e},
	{0xc353,0x00},
	{0xc051,0x20},
	{0xc306,0x09},
	{0xc308,0x09},
	{0xc367,0x20},
	{0xc368,0x20},
	{0xc369,0x20},
	{0xc36a,0x20},
	{0xc36b,0x20},
	{0xc36c,0x20},
	{0xc47d,0x0d},
	{0xc47c,0x0d},
	{0xc47b,0x0d},
	{0xc47a,0x1b},
	{0xc479,0x27},
	{0xc492,0xff},
	{0xc3b0,0x4c},
	{0xc3b1,0x4c},
	{0xc3b2,0x4c},
	{0xc3b3,0x4c},
	{0xc3b4,0x4c},
	{0xc3b5,0x4c},
	{0xc3bc,0x3e},
	{0xc3bd,0x3e},
	{0xc3be,0x3e},
	{0xc3bf,0x3e},
	{0xc3c0,0x3e},
	{0xc3c1,0x3e},
	{0xc3d6,0x4c},
	{0xc3d7,0x4c},
	{0xc3d8,0x4c},
	{0xc3d9,0x4c},
	{0xc3da,0x4c},
	{0xc3db,0x4c},
	{0xc3dc,0x48},
	{0xc3dd,0x44},
	{0xc3de,0x44},
	{0xc3df,0x44},
	{0xc3e0,0x44},
	{0xc3e1,0x44},
	{0xc3c2,0x00},
	{0xc3c3,0x6b},
	{0xc01c,0x00},
	{0xc01d,0x01},
	{0xc277,0x00},
	{0xc278,0x00},
	{0xc279,0x00},
	{0xc27a,0x04},
	{0xc27f,0x00},
	{0xc280,0x00},
	{0xc281,0x00},
	{0xc282,0x00},
	{0xc283,0x00},
	{0xc284,0x00},
	{0xc285,0x00},
	{0xc286,0x00},
	{0xc287,0x00},
	{0xc288,0x00},
	{0xc289,0x00},
	{0xc28a,0x00},
	{0xc28b,0x04},
	{0xc28c,0x04},
	{0xc28d,0x04},
	{0xc28e,0x04},
	{0xc3bc,0x46},
	{0xc3bd,0x46},
	{0xc3be,0x46},
	{0xc3bf,0x46},
	{0xc3c0,0x46},
	{0xc3c1,0x46},
	{0xc3e2,0x00},
	{0xc3e3,0x00},
	{0xc3e4,0x00},
	{0xc3e5,0x00},
	{0xc3e6,0x00},
	{0xc3e7,0x00},
	{0xc3e8,0x34},
	{0xc3e9,0x34},
	{0xc3ea,0x34},
	{0xc3eb,0x34},
	{0xc3ec,0x34},
	{0xc3ed,0x34},
	{0xc3bc,0x58},
	{0xc3bd,0x58},
	{0xc3be,0x58},
	{0xc3bf,0x58},
	{0xc3c0,0x58},
	{0xc3c1,0x58},
	{0xc3dc,0x70},
	{0xc3dd,0x70},
	{0xc3de,0x70},
	{0xc3df,0x70},
	{0xc3e0,0x70},
	{0xc3e1,0x70},
	{0xc3a4,0x40},
	{0xc3a5,0x40},
	{0xc3a6,0x40},
	{0xc3a7,0x40},
	{0xc3a8,0x40},
	{0xc3a9,0x40},
	{0xc3c4,0x01},
	{0xc3c5,0x01},
	{0xc3c6,0x01},
	{0xc3c7,0x01},
	{0xc3c8,0x01},
	{0xc3c9,0x01},
	{0xc3ca,0x10},
	{0xc3cb,0x10},
	{0xc3cc,0x10},
	{0xc3cd,0x10},
	{0xc3ce,0x10},
	{0xc3cf,0x10},
	{0xc3e2,0x01},
	{0xc3e3,0x01},
	{0xc3e4,0x01},
	{0xc3e5,0x01},
	{0xc3e6,0x01},
	{0xc3e7,0x01},
	{0xc3e8,0x10},
	{0xc3e9,0x10},
	{0xc3ea,0x10},
	{0xc3eb,0x10},
	{0xc3ec,0x10},
	{0xc3ed,0x10},
	{0xc244,0x58},
	{0xc1dc,0x45},
	{0xc1dd,0x45},
	{0xc1df,0x45},
	{0xc1e0,0x34},
	{0xc1e1,0x26},
	{0xc1e2,0x0e},
	{0xc1e3,0x45},
	{0xc1e4,0x34},
	{0xc1e5,0x26},
	{0xc1e6,0x0e},
	{0xc1e8,0x54},
	{0xc1e9,0x54},
	{0xc1eb,0x54},
	{0xc1ec,0x43},
	{0xc1ed,0x35},
	{0xc1ee,0x1d},
	{0xc1ef,0x54},
	{0xc1f0,0x43},
	{0xc1f1,0x35},
	{0xc1f2,0x1d},
	{0xc225,0x56},
	{0xc226,0x56},
	{0xc228,0x56},
	{0xc229,0x45},
	{0xc22a,0x37},
	{0xc22b,0x1f},
	{0xc22c,0x56},
	{0xc22d,0x45},
	{0xc22e,0x37},
	{0xc22f,0x1f},
	{0xc202,0x58},
	{0xc203,0x58},
	{0xc205,0x58},
	{0xc206,0x47},
	{0xc207,0x39},
	{0xc208,0x21},
	{0xc209,0x58},
	{0xc20a,0x47},
	{0xc20b,0x39},
	{0xc20c,0x21},
	{0x0100,0x01},
	{SENSOR_REG_DELAY,0x10},
        {SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the os08c10_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting os08c10_win_sizes[] = {
        /* 3840*2160 [0] */
        {
                .width          = 3840,
                .height         = 2160,
                .fps            = 20 << 16 | 1,
                .mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
                .colorspace     = TISP_COLORSPACE_SRGB,
                .regs           = os08c10_init_regs_3840_2160_20fps_mipi,
        },
};

static struct tx_isp_sensor_win_setting *wsize = &os08c10_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list os08c10_stream_on_mipi[] = {
        {0x0100, 0x01},
        { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list os08c10_stream_off_mipi[] = {
        {0x0100, 0x00},
        { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int os08c10_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int os08c10_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int os08c10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        unsigned char val;
        while (vals->reg_num != OS08C10_REG_END) {
                if (vals->reg_num == OS08C10_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = os08c10_read(sd, vals->reg_num, &val);
                        /* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}
#endif

static int os08c10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        while (vals->reg_num != OS08C10_REG_END) {
                if (vals->reg_num == OS08C10_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = os08c10_write(sd, vals->reg_num, vals->value);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int os08c10_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int os08c10_write(struct tx_isp_subdev *sd, uint16_t reg,
                   unsigned char value)
{
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
        struct i2c_msg msg = {
                .addr   = client->addr,
                .flags  = 0,
                .len    = 3,
                .buf    = buf,
        };
        int ret;
        ret = private_i2c_transfer(client->adapter, &msg, 1);
        if (ret > 0)
                ret = 0;

        return ret;
}

#if 0
static int os08c10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        unsigned char val;
        while (vals->reg_num != SENSOR_REG_END) {
                if (vals->reg_num == SENSOR_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = os08c10_read(sd, vals->reg_num, &val);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }
        return 0;
}
#endif

static int os08c10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        while (vals->reg_num != SENSOR_REG_END) {
                if (vals->reg_num == SENSOR_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = os08c10_write(sd, vals->reg_num, vals->value);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int os08c10_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        unsigned long rate;
        int ret = ISP_SUCCESS;

        rate = private_clk_get_rate(sensor->mclk);
        if(((rate / 1000) % mclk) != 0) {
                ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
                sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
                if (IS_ERR(sclka)) {
                        pr_err("get sclka failed\n");
                } else {
                        rate = private_clk_get_rate(sclka);
                        if(((rate / 1000) % mclk) != 0) {
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

        private_clk_set_rate(sensor->mclk, SENSOR_MCLK);
        private_clk_prepare_enable(sensor->mclk);

error:
        return ret;
}

static int os08c10_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = ISP_SUCCESS;

        sensor->video.vi_max_width = wsize->width;
        sensor->video.vi_max_height = wsize->height;

        sensor->video.mbus.width = wsize->width;
        sensor->video.mbus.height = wsize->height;
        sensor->video.mbus.code = wsize->mbus_code;
        sensor->video.mbus.field = TISP_FIELD_NONE;
        sensor->video.mbus.colorspace = wsize->colorspace;

        sensor->video.fps = wsize->fps;
        sensor->video.max_fps = wsize->fps;
        sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

#ifdef SENSOR_MIR_FLIP
	sensor->video.shvflip = 1;
#endif

        ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

        return ret;
}

static int os08c10_setting_select(struct tx_isp_subdev *sd, int deboot)
{
        int ret = ISP_SUCCESS;

        switch (deboot) {
        case 0:
                wsize = &os08c10_win_sizes[0];
                os08c10_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
                os08c10_attr.max_dgain = 0;
                os08c10_attr.max_again = 262080;
                os08c10_attr.min_integration_time = 8;
                os08c10_attr.max_integration_time = 2520 - 8;
                os08c10_attr.total_width = 2976 * 2;
                os08c10_attr.total_height = 2520;
                os08c10_attr.integration_time_apply_delay = 2;
                os08c10_attr.again_apply_delay = 2;
                os08c10_attr.dgain_apply_delay = 0;
                os08c10_attr.integration_time_limit = os08c10_attr.max_integration_time;
                os08c10_attr.max_integration_time_native = os08c10_attr.max_integration_time;
                os08c10_attr.min_integration_time_native = os08c10_attr.min_integration_time;
                os08c10_attr.expo_fs = 1;
#ifdef SENSOR_WDR_2_FRAME
                os08c10_attr.max_again_short = xxxx;
                os08c10_attr.min_integration_time_short = xx;
                os08c10_attr.max_integration_time_short = xx;
                os08c10_attr.wdr_cache = wdr_line * os08c10_attr.total_width;
#endif /* SENSOR_WDR_2_FRAME */
                memcpy((void *)(&(os08c10_attr.mipi)), (void *)(&os08c10_mipi_linear), sizeof(os08c10_attr.mipi));
                break;
        default:
                ISP_ERROR("Have no this Setting Source!!!\n");
        }

        return ret;
}

static int os08c10_attr_check(struct tx_isp_subdev *sd)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        struct tx_isp_sensor_register_info *info = &sensor->info;
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        struct clk *sclka;
        int ret = ISP_SUCCESS;

        os08c10_setting_select(sd, info->default_boot);

        switch (info->video_interface) {
        case TISP_SENSOR_VI_MIPI_CSI0:
        case TISP_SENSOR_VI_MIPI_CSI1:
                os08c10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
                os08c10_attr.mipi.index = 0;
                break;
        case TISP_SENSOR_VI_DVP:
                os08c10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
                break;
        default:
                ISP_ERROR("Have no this interface!!!\n");
        }

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

        ret = os08c10_clk_set(sd, sclka, SENSOR_MCLK);
        if (ret) {
                ISP_ERROR("MCLK configuration failed!!!\n");
        }

        os08c10_attr_set(sd, wsize);
        sensor->priv = wsize;

        return 0;

err_get_mclk:
        return -1;
}

static int os08c10_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
        unsigned char v;
        int ret;

        ret = os08c10_read(sd, 0x300a, &v);
        pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        if (ret < 0)
                return ret;
        if (v != SENSOR_CHIP_ID_H)
                return -ENODEV;
        *ident = v;

        ret = os08c10_read(sd, 0x300b, &v);
        pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        if (ret < 0)
                return ret;
        if (v != SENSOR_CHIP_ID_M)
                return -ENODEV;
        *ident = v;

        ret = os08c10_read(sd, 0x300c, &v);
        pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        if (ret < 0)
                return ret;
        if (v != SENSOR_CHIP_ID_L)
                return -ENODEV;
        *ident = (*ident << 8) | v;

        return 0;
}

static int os08c10_g_chip_ident(struct tx_isp_subdev *sd,
                                 struct tx_isp_chip_ident *chip)
{
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        struct tx_isp_sensor_register_info *info = &sensor->info;
        unsigned int ident = 0;
        int ret = ISP_SUCCESS;

        os08c10_attr_check(sd);
        if (info->rst_gpio != -1) {
                ret = private_gpio_request(info->rst_gpio, "os08c10_reset");
                if (!ret) {
                        private_gpio_direction_output(info->rst_gpio, 1);
                        private_msleep(5);
                        private_gpio_direction_output(info->rst_gpio, 0);
                        private_msleep(10);
                        private_gpio_direction_output(info->rst_gpio, 1);
                        private_msleep(10);
                } else {
                        ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
                }
        }
        if (info->pwdn_gpio != -1) {
                ret = private_gpio_request(info->pwdn_gpio, "os08c10_pwdn");
                if (!ret) {
                        private_gpio_direction_output(info->pwdn_gpio, 0);
                        private_msleep(5);
                        private_gpio_direction_output(info->pwdn_gpio, 1);
                        private_msleep(5);
                } else {
                        ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
                }
        }
        ret = os08c10_detect(sd, &ident);
        if (ret) {
                ISP_ERROR("chip found @ 0x%x (%s) is not an os08c10 chip.\n",
                          client->addr, client->adapter->name);
                return ret;
        }

        ISP_WARNING("===================================================\n");
        ISP_WARNING("Template version is %s\n", TVERSION);
        ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
        ISP_WARNING("Sensor name is %s\n", os08c10_attr.name);
        ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
        ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
        ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
                    info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
        ISP_WARNING("===================================================\n");

        if (chip) {
                memcpy(chip->name, "os08c10", sizeof("os08c10"));
                chip->ident = ident;
                chip->revision = SENSOR_VERSION;
        }

        return 0;
}

static int os08c10_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        return 0;
}

static int os08c10_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = ISP_SUCCESS;

        if (!init->enable) {
                sensor->video.state = TX_ISP_MODULE_DEINIT;
                return ISP_SUCCESS;
        } else {
                ret = os08c10_attr_set(sd, wsize);
                sensor->video.state = TX_ISP_MODULE_DEINIT;
        }

        return ret;
}

static int os08c10_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
        unsigned char val = 0;
        int len = 0;
        int ret = ISP_SUCCESS;

        len = strlen(sd->chip.name);
        if (len && strncmp(sd->chip.name, reg->name, len)) {
                return -EINVAL;
        }
        if (!private_capable(CAP_SYS_ADMIN))
                return -EPERM;
        ret = os08c10_read(sd, reg->reg & 0xffff, &val);
        reg->val = val;
        reg->size = 2;

        return ret;
}

static int os08c10_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
        int len = 0;

        len = strlen(sd->chip.name);
        if (len && strncmp(sd->chip.name, reg->name, len)) {
                return -EINVAL;
        }
        if (!private_capable(CAP_SYS_ADMIN))
                return -EPERM;
        os08c10_write(sd, reg->reg & 0xffff, reg->val & 0xff);

        return 0;
}

static int os08c10_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = ISP_SUCCESS;

        if (init->enable) {
                if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
                        ret = os08c10_write_array(sd, wsize->regs);
                        if (ret)
                                return ret;
                        sensor->video.state = TX_ISP_MODULE_INIT;
                }
                if (sensor->video.state == TX_ISP_MODULE_INIT) {
                        ret = os08c10_write_array(sd, os08c10_stream_on_mipi);
                        sensor->video.state = TX_ISP_MODULE_RUNNING;
                        pr_debug("os08c10 stream on\n");
                }

        } else {
                ret = os08c10_write_array(sd, os08c10_stream_off_mipi);
                sensor->video.state = TX_ISP_MODULE_INIT;
                pr_debug("os08c10 stream off\n");
        }

        return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int os08c10_set_expo(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret += os08c10_write(sd, 0x3501, (unsigned char)((it >> 8) & 0xff));
	ret += os08c10_write(sd, 0x3502, (unsigned char)(it & 0xff));

	ret += os08c10_write(sd, 0x3508, (unsigned char)((again >> 8) & 0x3f));
	ret += os08c10_write(sd, 0x3509, (unsigned char)(again & 0xff));

        return ret;
}
#else
static int os08c10_set_integration_time(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;

	ret += os08c10_write(sd, 0x3501, (unsigned char)((value >> 8) & 0xff));
	ret += os08c10_write(sd, 0x3502, (unsigned char)(value & 0xff));

        return ret;
}

static int os08c10_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;

	ret += os08c10_write(sd, 0x3508, (unsigned char)((value >> 8) & 0x3f));
	ret += os08c10_write(sd, 0x3509, (unsigned char)(value & 0xff));

        return ret;
}
#endif /* SENSOR_EXPO */

static int os08c10_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
        return 0;
}

static int os08c10_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
        return 0;
}

static int os08c10_set_mode(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;

        if (wsize) {
                ret = os08c10_attr_set(sd, wsize);
        }

        return ret;
}

static int os08c10_set_fps(struct tx_isp_subdev *sd, int fps)
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
                sclk = 2976 * 2520 * 20 * 2;  /**< HTS * VTS * FPS */
                max_fps = 20;
                break;
        default:
                ret = -1;
                ISP_ERROR("Now we do not support this framerate!!!\n");
        }

        max_fps = wsize->fps;
        newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
        if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
                ISP_ERROR("warn: fps(%d/%d) no in range\n", fps >> 16, fps & 0xffff);
                return -1;
        }

        val = 0;
        ret += os08c10_read(sd, 0x380c, &val);
        hts = val << 8;
        val = 0;
        ret += os08c10_read(sd, 0x380d, &val);
        hts = (hts | val) << 1;
        if (0 != ret) {
                ISP_ERROR("err: os08c10 read err\n");
                return ret;
        }

        vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

        os08c10_write(sd, 0x380f, (unsigned char) (vts & 0xff));
        os08c10_write(sd, 0x380e, (unsigned char) (vts >> 8));

        if (0 != ret) {
                ISP_ERROR("err: os08c10_write err\n");
                return ret;
        }

        sensor->video.fps = fps;
#ifndef SENSOR_WDR_2_FRAME
        /* Linear mode */
        sensor->video.attr->total_height = vts;
        sensor->video.attr->max_integration_time = vts - 8;
        sensor->video.attr->integration_time_limit = vts - 8;
        sensor->video.attr->max_integration_time_native = vts - 8;
#else
        /* WDR mode */
        sensor->video.attr->total_height = vts;
        sensor->video.attr->max_integration_time = vts - xx;
        sensor->video.attr->integration_time_limit = vts - xx;
        sensor->video.attr->max_integration_time_native = vts - xx;
        sensor->video.attr->max_integration_time_short = vts - xx;
#endif /* SENSOR_WDR_2_FRAME */
        ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
        if (ret) {
                ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
        }

        return ret;
}

#ifdef SENSOR_MIR_FLIP
static int os08c10_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = ISP_SUCCESS;

        os08c10_write(sd, 0x0100, 0x00);
	switch(enable) {
	case 0:
                os08c10_write(sd, 0x3218, 0x08);
                os08c10_write(sd, 0x368b, 0xb0);
                os08c10_write(sd, 0x3811, 0x0f);
                os08c10_write(sd, 0x3813, 0x10);
                os08c10_write(sd, 0x3820, 0x80);
                os08c10_write(sd, 0x3821, 0x04);
		break;
	case 1:
                os08c10_write(sd, 0x3218, 0x00);
                os08c10_write(sd, 0x368b, 0x30);
                os08c10_write(sd, 0x3811, 0x10);
                os08c10_write(sd, 0x3813, 0x10);
                os08c10_write(sd, 0x3820, 0x80);
                os08c10_write(sd, 0x3821, 0x00);
		break;
	case 2:
                os08c10_write(sd, 0x3218, 0x08);
                os08c10_write(sd, 0x368b, 0xb0);
                os08c10_write(sd, 0x3811, 0x0f);
                os08c10_write(sd, 0x3813, 0x0f);
                os08c10_write(sd, 0x3820, 0x84);
                os08c10_write(sd, 0x3821, 0x04);
		break;
	case 3:
                os08c10_write(sd, 0x3218, 0x00);
                os08c10_write(sd, 0x368b, 0x30);
                os08c10_write(sd, 0x3811, 0x10);
                os08c10_write(sd, 0x3813, 0x0f);
                os08c10_write(sd, 0x3820, 0x84);
                os08c10_write(sd, 0x3821, 0x00);
		break;
	}
        os08c10_write(sd, 0x0100, 0x01);

	return ret;
}
#endif /* SENSOR_MIR_FLIP */

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int os08c10_set_expo_short(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;


        return ret;
}
#else
static int os08c10_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;


        return ret;
}

static int os08c10_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;


        return ret;
}
#endif /* SENSOR_EXPO */

static int os08c10_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
        struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
        int ret = ISP_SUCCESS;

        ret = os08c10_write_array(sd, os08c10_stream_off_mipi);
        if (wdr_en == 1) {
                os08c10_setting_select(sd, 1);
                os08c10_attr_set(sd, wsize);
        } else if (wdr_en == 0) {
                os08c10_setting_select(sd, 0);
                os08c10_attr_set(sd, wsize);
        } else {
                ISP_ERROR("Can not support this data type!!!");
                return -1;
        }

        return 0;
}

static int os08c10_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        struct tx_isp_sensor_register_info *info = &sensor->info;
        int ret = ISP_SUCCESS;

        private_gpio_direction_output(info->rst_gpio, 0);
        private_msleep(1);
        private_gpio_direction_output(info->rst_gpio, 1);
        private_msleep(1);

        ret = os08c10_write_array(sd, wsize->regs);
        ret = os08c10_write_array(sd, os08c10_stream_on_mipi);

        return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int os08c10_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
        long ret = 0;
        struct tx_isp_sensor_value *sensor_val = arg;
#ifdef SENSOR_WDR_2_FRAME
        struct tx_isp_initarg *init = arg;
#endif /* SENSOR_WDR_2_FRAME */

        if (IS_ERR_OR_NULL(sd)) {
                ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
                return -EINVAL;
        }

        switch (cmd) {
#ifdef SENSOR_EXPO
        case TX_ISP_EVENT_SENSOR_EXPO:
                if (arg)
                        ret = os08c10_set_expo(sd, sensor_val->value);
                break;
#else
        case TX_ISP_EVENT_SENSOR_INT_TIME:
                if (arg)
                        ret = os08c10_set_integration_time(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_AGAIN:
                if (arg)
                        ret = os08c10_set_analog_gain(sd, sensor_val->value);
                break;
#endif /* SENSOR_EXPO */
        case TX_ISP_EVENT_SENSOR_DGAIN:
                if (arg)
                        ret = os08c10_set_digital_gain(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
                if (arg)
                        ret = os08c10_get_black_pedestal(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_RESIZE:
                if (arg)
                        ret = os08c10_set_mode(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
                if (arg)
                        ret = os08c10_write_array(sd, os08c10_stream_off_mipi);
                break;
        case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
                if (arg)
                        ret = os08c10_write_array(sd, os08c10_stream_on_mipi);
                break;
        case TX_ISP_EVENT_SENSOR_FPS:
                if (arg)
                        ret = os08c10_set_fps(sd, sensor_val->value);
                break;
#ifdef SENSOR_MIR_FLIP
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = os08c10_set_vflip(sd, sensor_val->value);
		break;
#endif /* SENSOR_MIR_FLIP */
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
        case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
                if (arg)
                        ret = os08c10_set_expo_short(sd, sensor_val->value);
                break;
#else
        case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
                if(arg)
                        ret = os08c10_set_integration_time_short(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
                if(arg)
                        ret = os08c10_set_analog_gain_short(sd, sensor_val->value);
                break;
#endif /* SENSOR_EXPO */
        case TX_ISP_EVENT_SENSOR_WDR:
                if(arg)
                        ret = os08c10_set_wdr(sd, init->enable);
                break;
        case TX_ISP_EVENT_SENSOR_WDR_STOP:
                if(arg)
                        ret = os08c10_set_wdr_stop(sd, init->enable);
                break;
#endif /* SENSOR_WDR_2_FRAME */
        default:
                break;
        }

        return 0;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops os08c10_core_ops = {
        .g_chip_ident = os08c10_g_chip_ident,
        .reset = os08c10_reset,
        .init = os08c10_init,
        .g_register = os08c10_g_register,
        .s_register = os08c10_s_register,
};

static struct tx_isp_subdev_video_ops os08c10_video_ops = {
        .s_stream = os08c10_s_stream,
};

static struct tx_isp_subdev_sensor_ops os08c10_sensor_ops = {
#ifndef SENSOR_TEST
        .ioctl  = os08c10_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops os08c10_ops = {
        .core = &os08c10_core_ops,
        .video = &os08c10_video_ops,
        .sensor = &os08c10_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
        .name = "os08c10",
        .id = -1,
        .dev = {
                .dma_mask = &tx_isp_module_dma_mask,
                .coherent_dma_mask = 0xffffffff,
                .platform_data = NULL,
        },
        .num_resources = 0,
};

static int os08c10_probe(struct i2c_client *client,
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

        sensor->video.attr = &os08c10_attr;
        sensor->dev = &client->dev;
        tx_isp_subdev_init(&sensor_platform_device, sd, &os08c10_ops);
        tx_isp_set_subdevdata(sd, client);
        tx_isp_set_subdev_hostdata(sd, sensor);
        private_i2c_set_clientdata(client, sd);

        pr_debug("probe ok ------->os08c10\n");

        return 0;
}

static int os08c10_remove(struct i2c_client *client)
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

static const struct i2c_device_id os08c10_id[] = {
        {"os08c10", 0},
        {}
};
MODULE_DEVICE_TABLE(i2c, os08c10_id
                   );

static struct i2c_driver os08c10_driver = {
        .driver = {
                .owner  = THIS_MODULE,
                .name   = "os08c10",
        },
        .probe          = os08c10_probe,
        .remove         = os08c10_remove,
        .id_table       = os08c10_id,
};

static __init int init_os08c10(void) {
        return private_i2c_add_driver(&os08c10_driver);
}

static __exit void exit_os08c10(void) {
        private_i2c_del_driver(&os08c10_driver);
}

module_init(init_os08c10);
module_exit(exit_os08c10);

MODULE_DESCRIPTION("A low-level driver for Sony os08c10 sensor");
MODULE_LICENSE("GPL");
