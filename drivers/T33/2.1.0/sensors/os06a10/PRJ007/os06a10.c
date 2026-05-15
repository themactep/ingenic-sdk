/*
 * os06a10.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps     dvdd
 *  0           3200*1800       30        mipi_2lane    linear  12        30
 *
 * @I2C addr:0x36, 0x10
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

#define TVERSION "V20241113a"
#define SENSOR_VERSION  "H20250211a"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT	/**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE	  /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME	/**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP			/**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H	(0x53)
#define SENSOR_CHIP_ID_M	(0x06)
#define SENSOR_CHIP_ID_L	(0x41)
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
#define SENSOR_REG_END	  0xff
#define SENSOR_REG_DELAY  0xfe
#endif /* SENSOR_I2C_REG_8BIT */
#ifdef SENSOR_I2C_REG_16BIT
#define SENSOR_REG_END	  0xffff
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

struct tx_isp_sensor_attribute os06a10_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
    unsigned int value;
    unsigned int gain;
};

struct again_lut os06a10_again_lut[] = {
    {0x0080, 0},
    {0x0088, 5732},
    {0x0090, 11136},
    {0x0098, 16248},
    {0x00a0, 21098},
    {0x00a8, 25711},
    {0x00b0, 30109},
    {0x00b8, 34312},
    {0x00c0, 38336},
    {0x00c8, 42196},
    {0x00d0, 45904},
    {0x00d8, 49472},
    {0x00e0, 52911},
    {0x00e8, 56229},
    {0x00f0, 59434},
    {0x00f8, 62534},
    {0x0100, 65536},
    {0x0110, 71268},
    {0x0120, 76672},
    {0x0130, 81784},
    {0x0140, 86634},
    {0x0150, 91247},
    {0x0160, 95645},
    {0x0170, 99848},
    {0x0180, 103872},
    {0x0190, 107732},
    {0x01a0, 111440},
    {0x01b0, 115008},
    {0x01c0, 118447},
    {0x01d0, 121765},
    {0x01e0, 124970},
    {0x01f0, 128070},
    {0x0200, 131072},
    {0x0220, 136804},
    {0x0240, 142208},
    {0x0260, 147320},
    {0x0280, 152170},
    {0x02a0, 156783},
    {0x02c0, 161181},
    {0x02e0, 165384},
    {0x0300, 169408},
    {0x0320, 173268},
    {0x0340, 176976},
    {0x0360, 180544},
    {0x0380, 183983},
    {0x03a0, 187301},
    {0x03c0, 190506},
    {0x03e0, 193606},
    {0x0400, 196608},
    {0x0440, 202340},
    {0x0480, 207744},
    {0x04c0, 212856},
    {0x0500, 217706},
    {0x0540, 222319},
    {0x0580, 226717},
    {0x05c0, 230920},
    {0x0600, 234944},
    {0x0640, 238804},
    {0x0680, 242512},
    {0x06c0, 246080},
    {0x0700, 249519},
    {0x0740, 252837},
    {0x0780, 256042},
    {0x07c0, 259142},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int os06a10_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
    /* Analog gain table */
    struct again_lut *lut = os06a10_again_lut;
    while (lut->gain <= os06a10_attr.again.max) {
        if (isp_gain == 0) {
            *sensor_again = lut[0].value;
            return 0;
        } else if (isp_gain < lut->gain) {
            *sensor_again = (lut - 1)->value;
            return (lut - 1)->gain;
        } else {
            if ((lut->gain == os06a10_attr.again.max) && (isp_gain >= lut->gain)) {
                *sensor_again = lut->value;
                return lut->gain;
            }
        }

        lut++;
    }

#else
    /* Non analog gain table */
    ...;
#endif	/* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

    return isp_gain;
}

#ifdef SENSOR_WDR_2_FRAME
unsigned int os06a10_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
    /* Analog gain table */
    struct again_lut *lut = os06a10_again_lut;
    while(lut->gain <= os06a10_attr.max_again_short) {
        if(isp_gain == 0) {
            *sensor_again = 0;
            return 0;
        }
        else if(isp_gain < lut->gain) {
            *sensor_again = (lut - 1)->value;
            return (lut - 1)->gain;
        }
        else{
            if((lut->gain == os06a10_attr.max_again_short) && (isp_gain >= lut->gain)) {
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

unsigned int os06a10_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
    return 0;
}

#ifdef SENSOR_HCG
unsigned int os06a10_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

    return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int os06a10_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

    return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus os06a10_mipi_linear = {
    .mode = SENSOR_MIPI_OTHER_MODE,
    .clk = 1000,
    .lans = 2,
    .image_twidth = 3200,
    .image_theight = 1800,
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

struct tx_isp_dvp_bus os06a10_dvp = {
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

struct tx_isp_sensor_attribute os06a10_attr = {
    .name = "os06a10",
    .chip_id = 0x530641,
    .cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
    .cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
    .cbus_device = 0x36,
    .sensor_ctrl.alloc_again = os06a10_alloc_again,
    .sensor_ctrl.alloc_dgain = os06a10_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
    .sensor_ctrl.alloc_again_short = os06a10_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

    .dgain = {0},
    .dgain_short = {0},
};

static struct regval_list os06a10_init_regs_3200_1800_30fps_mipi[] = {
    {0x0103, 0x01},
    {0x0301, 0x44},
    {0x0303, 0x02},
    {0x0305, 0x64},
    {0x0306, 0x00},
    {0x0325, 0x3b},
    {0x3002, 0x21},
    {0x3016, 0x32},
    {0x301e, 0xb4},
    {0x301f, 0xd0},
    {0x3021, 0x03},
    {0x3022, 0x61},
    {0x3107, 0xa1},
    {0x3108, 0x7d},
    {0x3109, 0xfc},
    {0x3500, 0x00},
    {0x3501, 0x07},
    {0x3502, 0xb6},
    {0x3503, 0x88},
    {0x3508, 0x00},
    {0x3509, 0x80},
    {0x350a, 0x04},
    {0x350b, 0x00},
    {0x350c, 0x00},
    {0x350d, 0x80},
    {0x350e, 0x04},
    {0x350f, 0x00},
    {0x3510, 0x00},
    {0x3511, 0x00},
    {0x3512, 0x20},
    {0x3600, 0x65},
    {0x3601, 0x88},
    {0x3610, 0x87},
    {0x3611, 0x24},
    {0x3614, 0x4c},
    {0x3620, 0x0c},
    {0x3632, 0x00},
    {0x3633, 0x02},
    {0x3636, 0xcc},
    {0x3637, 0x27},
    {0x3660, 0x00},
    {0x3662, 0x10},
    {0x3665, 0x00},
    {0x3666, 0x01},
    {0x366a, 0x10},
    {0x3670, 0x03},
    {0x3671, 0x03},
    {0x3672, 0x03},
    {0x3673, 0x03},
    {0x3678, 0x2b},
    {0x367a, 0x11},
    {0x367b, 0x11},
    {0x367c, 0x11},
    {0x367d, 0x11},
    {0x3681, 0xff},
    {0x3682, 0x86},
    {0x3683, 0x44},
    {0x3684, 0x24},
    {0x3685, 0x00},
    {0x368a, 0x00},
    {0x368d, 0x2b},
    {0x368e, 0x2b},
    {0x3690, 0x00},
    {0x3691, 0x0b},
    {0x3692, 0x0b},
    {0x3693, 0x0b},
    {0x3694, 0x0b},
    {0x369d, 0x68},
    {0x369e, 0x34},
    {0x369f, 0x1b},
    {0x36a0, 0x0f},
    {0x36a1, 0x77},
    {0x36b0, 0x30},
    {0x36b2, 0x00},
    {0x36b3, 0x00},
    {0x36b4, 0x00},
    {0x36b5, 0x00},
    {0x36b6, 0x00},
    {0x36b7, 0x00},
    {0x36b8, 0x00},
    {0x36b9, 0x00},
    {0x36ba, 0x00},
    {0x36bb, 0x00},
    {0x36bc, 0x00},
    {0x36bd, 0x00},
    {0x36be, 0x00},
    {0x36bf, 0x00},
    {0x36c0, 0x01},
    {0x36c1, 0x00},
    {0x36c2, 0x00},
    {0x36c3, 0x00},
    {0x36c4, 0x00},
    {0x36c5, 0x00},
    {0x36c6, 0x00},
    {0x36c7, 0x00},
    {0x36c8, 0x00},
    {0x36c9, 0x00},
    {0x36ca, 0x0e},
    {0x36cb, 0x0e},
    {0x36cc, 0x0e},
    {0x36cd, 0x0e},
    {0x36ce, 0x0c},
    {0x36cf, 0x0c},
    {0x36d0, 0x0c},
    {0x36d1, 0x0c},
    {0x36d2, 0x00},
    {0x36d3, 0x08},
    {0x36d4, 0x10},
    {0x36d5, 0x10},
    {0x36d6, 0x00},
    {0x36d7, 0x08},
    {0x36d8, 0x10},
    {0x36d9, 0x10},
    {0x3701, 0x1d},
    {0x3703, 0x2a},
    {0x3704, 0x05},
    {0x3709, 0x57},
    {0x370b, 0x48},
    {0x3706, 0x6f},
    {0x370a, 0x01},
    {0x370b, 0x48},
    {0x370e, 0x0c},
    {0x370f, 0x1c},
    {0x3710, 0x00},
    {0x3713, 0x00},
    {0x3714, 0x24},
    {0x3716, 0x24},
    {0x371a, 0x1e},
    {0x3724, 0x09},
    {0x3725, 0xb2},
    {0x372b, 0x54},
    {0x3730, 0xe1},
    {0x3735, 0x80},
    {0x3739, 0x10},
    {0x373f, 0xb0},
    {0x3740, 0x6f},
    {0x3741, 0x6f},
    {0x3742, 0x6f},
    {0x3743, 0x6f},
    {0x3744, 0x48},
    {0x3745, 0x48},
    {0x3746, 0x48},
    {0x3747, 0x48},
    {0x3748, 0x01},
    {0x3749, 0x01},
    {0x374a, 0x01},
    {0x374b, 0x01},
    {0x3756, 0x00},
    {0x3757, 0x0e},
    {0x375d, 0x84},
    {0x3760, 0x11},
    {0x3767, 0x08},
    {0x376f, 0x42},
    {0x3771, 0x00},
    {0x3773, 0x01},
    {0x3774, 0x02},
    {0x3775, 0x12},
    {0x3776, 0x02},
    {0x377b, 0x40},
    {0x377c, 0x00},
    {0x377d, 0x0c},
    {0x3782, 0x02},
    {0x3787, 0x24},
    {0x378a, 0x01},
    {0x378d, 0x00},
    {0x3790, 0x1f},
    {0x3791, 0x58},
    {0x3795, 0x24},
    {0x3796, 0x01},
    {0x3798, 0x40},
    {0x379c, 0x00},
    {0x379d, 0x00},
    {0x379e, 0x00},
    {0x379f, 0x01},
    {0x37a1, 0x10},
    {0x37a6, 0x00},
    {0x37ab, 0x0e},
    {0x37ac, 0xa0},
    {0x37be, 0x0a},
    {0x37bf, 0x05},
    {0x37bb, 0x02},
    {0x37bf, 0x05},
    {0x37c2, 0x04},
    {0x37c4, 0x11},
    {0x37c5, 0x80},
    {0x37c6, 0x14},
    {0x37c7, 0x08},
    {0x37c8, 0x42},
    {0x37cd, 0x17},
    {0x37ce, 0x01},
    {0x37d8, 0x02},
    {0x37d9, 0x08},
    {0x37dc, 0x01},
    {0x37e0, 0x0c},
    {0x37e1, 0x20},
    {0x37e2, 0x10},
    {0x37e3, 0x04},
    {0x37e4, 0x28},
    {0x37e5, 0x02},
    {0x37ef, 0x00},
    {0x37f4, 0x00},
    {0x37f5, 0x00},
    {0x37f6, 0x00},
    {0x37f7, 0x00},
    {0x3800, 0x00},
    {0x3801, 0x00},
    {0x3802, 0x00},
    {0x3803, 0x48},
    {0x3804, 0x0c},
    {0x3805, 0x8f},
    {0x3806, 0x07},
    {0x3807, 0x5f},
    {0x3808, 0x0c},
    {0x3809, 0x80},
    {0x380a, 0x07},
    {0x380b, 0x08},
    {0x380c, 0x06},
    {0x380d, 0xd0},
    {0x380e, 0x07},
    {0x380f, 0xd6},
    {0x3810, 0x00},
    {0x3811, 0x08},
    {0x3812, 0x00},
    {0x3813, 0x08},
    {0x3814, 0x01},
    {0x3815, 0x01},
    {0x3816, 0x01},
    {0x3817, 0x01},
    {0x3818, 0x00},
    {0x3819, 0x00},
    {0x381a, 0x00},
    {0x381b, 0x01},
    {0x3820, 0x88},
    {0x3821, 0x00},
    {0x3822, 0x12},
    {0x3823, 0x08},
    {0x3824, 0x00},
    {0x3825, 0x20},
    {0x3826, 0x00},
    {0x3827, 0x08},
    {0x3829, 0x03},
    {0x382a, 0x00},
    {0x382b, 0x00},
    {0x3832, 0x08},
    {0x3838, 0x00},
    {0x3839, 0x00},
    {0x383a, 0x00},
    {0x383b, 0x00},
    {0x383d, 0x01},
    {0x383e, 0x00},
    {0x383f, 0x00},
    {0x3843, 0x00},
    {0x3880, 0x16},
    {0x3881, 0x00},
    {0x3882, 0x08},
    {0x389a, 0x00},
    {0x389b, 0x00},
    {0x38a2, 0x02},
    {0x38a3, 0x02},
    {0x38a4, 0x02},
    {0x38a5, 0x02},
    {0x38a7, 0x04},
    {0x38b8, 0x02},
    {0x3c80, 0x3f},
    {0x3c86, 0x03},
    {0x3c87, 0x02},
    {0x389c, 0x00},
    {0x3ca2, 0x0c},
    {0x3d85, 0x1b},
    {0x3d8c, 0x71},
    {0x3d8d, 0xe2},
    {0x3f00, 0xcb},
    {0x3f03, 0x08},
    {0x3f9e, 0x07},
    {0x3f9f, 0x04},
    {0x4000, 0xf3},
    {0x4002, 0x01},
    {0x4003, 0x00},
    {0x4008, 0x02},
    {0x4009, 0x0d},
    {0x400a, 0x02},
    {0x400b, 0x80},
    {0x4040, 0x00},
    {0x4041, 0x07},
    {0x4090, 0x04},
    {0x40b0, 0x01},
    {0x40b1, 0x01},
    {0x40b2, 0x30},
    {0x40b3, 0x04},
    {0x40b4, 0xe8},
    {0x40b5, 0x01},
    {0x40b7, 0x07},
    {0x40b8, 0xff},
    {0x40b9, 0x00},
    {0x40ba, 0x00},
    {0x4300, 0xff},
    {0x4301, 0x00},
    {0x4302, 0x0f},
    {0x4303, 0x20},
    {0x4304, 0x20},
    {0x4305, 0x83},
    {0x4306, 0x21},
    {0x430d, 0x00},
    {0x4505, 0xc4},
    {0x4506, 0x00},
    {0x4507, 0x60},
    {0x4803, 0x00},
    {0x4809, 0x8e},
    {0x480e, 0x00},
    {0x4813, 0x00},
    {0x4814, 0x6c},
    {0x481b, 0x40},
    {0x481f, 0x30},
    {0x4825, 0x34},
    {0x4829, 0x64},
    {0x4837, 0x0d},
    {0x484b, 0x07},
    {0x4883, 0x36},
    {0x4885, 0x03},
    {0x488b, 0x00},
    {0x4d06, 0x01},
    {0x4e00, 0x2a},
    {0x4e0d, 0x00},
    {0x5000, 0xf9},
    {0x5001, 0x09},
    {0x5004, 0x00},
    {0x5005, 0x0e},
    {0x5036, 0x00},
    {0x5080, 0x04},
    {0x5082, 0x00},
    {0x5180, 0x00},
    {0x5181, 0x10},
    {0x5182, 0x01},
    {0x5183, 0xdf},
    {0x5184, 0x02},
    {0x5185, 0x6c},
    {0x5189, 0x48},
    {0x520a, 0x03},
    {0x520b, 0x0f},
    {0x520c, 0x3f},
    {0x580b, 0x03},
    {0x580d, 0x00},
    {0x580f, 0x00},
    {0x5820, 0x00},
    {0x5821, 0x00},
    {0x3222, 0x03},
    {0x3208, 0x06},
    {0x3701, 0x1d},
    {0x37ab, 0x01},
    {0x3790, 0x6f},
    {0x38be, 0x01},
    {0x3791, 0x48},
    {0x37bf, 0x1c},
    {0x3610, 0x37},
    {0x3208, 0x16},
    {0x3208, 0x07},
    {0x3701, 0x1d},
    {0x37ab, 0x0e},
    {0x3790, 0x6f},
    {0x38be, 0x01},
    {0x3791, 0x48},
    {0x37bf, 0x0a},
    {0x3610, 0x87},
    {0x3208, 0x17},
    {0x3208, 0x08},
    {0x3701, 0x1d},
    {0x37ab, 0x0e},
    {0x3790, 0x6f},
    {0x38be, 0x01},
    {0x3791, 0x48},
    {0x37bf, 0x0a},
    {0x3610, 0x87},
    {0x3208, 0x18},
    {0x3208, 0x09},
    {0x3701, 0x1d},
    {0x37ab, 0x0e},
    {0x3790, 0x6f},
    {0x38be, 0x01},
    {0x3791, 0x48},
    {0x37bf, 0x0a},
    {0x3610, 0x87},
    {0x3208, 0x19},
    {0x0100, 0x01},
    {SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the os06a10_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting os06a10_win_sizes[] = {
    {
        .width          = 3200,
        .height         = 1800,
        .fps            = 30 << 16 | 1,
        .mbus_code      = TISP_VI_FMT_SBGGR12_1X12,
        .colorspace     = TISP_COLORSPACE_SRGB,
        .regs           = os06a10_init_regs_3200_1800_30fps_mipi,
    },
};

static struct tx_isp_sensor_win_setting *wsize = &os06a10_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list os06a10_stream_on_mipi[] = {
    {0x0100, 0x01},
    { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list os06a10_stream_off_mipi[] = {
    {0x0100, 0x00},
    { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int os06a10_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int os06a10_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int os06a10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    unsigned char val;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = os06a10_read(sd, vals->reg_num, &val);
            /* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif

static int os06a10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = os06a10_write(sd, vals->reg_num, vals->value);
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif	/* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int os06a10_read(struct tx_isp_subdev *sd, uint16_t reg,
                 unsigned char *value) {
    int ret;
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
    struct i2c_msg msg[2] = {
        [0] = {
            .addr	= client->addr,
            .flags	= 0,
            .len	= 2,
            .buf	= buf,
        },
        [1] = {
            .addr	= client->addr,
            .flags	= I2C_M_RD,
            .len	= 1,
            .buf	= value,
        }
    };

    ret = private_i2c_transfer(client->adapter, msg, 2);
    if (ret > 0)
        ret = 0;

    return ret;
}

int os06a10_write(struct tx_isp_subdev *sd, uint16_t reg,
                  unsigned char value)
{
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
    struct i2c_msg msg = {
        .addr	= client->addr,
        .flags	= 0,
        .len	= 3,
        .buf	= buf,
    };
    int ret;
    ret = private_i2c_transfer(client->adapter, &msg, 1);
    if (ret > 0)
        ret = 0;

    return ret;
}

#if 0
static int os06a10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    unsigned char val;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = os06a10_read(sd, vals->reg_num, &val);
            if (ret < 0)
                return ret;
        }
        vals++;
    }
    return 0;
}
#endif

static int os06a10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
    int ret;
    while (vals->reg_num != SENSOR_REG_END) {
        if (vals->reg_num == SENSOR_REG_DELAY) {
            private_msleep(vals->value);
        } else {
            ret = os06a10_write(sd, vals->reg_num, vals->value);
            if (ret < 0)
                return ret;
        }
        vals++;
    }

    return 0;
}
#endif	/* SENSOR_I2C_REG_16BIT */

static int os06a10_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int os06a10_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int os06a10_setting_select(struct tx_isp_subdev *sd, int deboot)
{
    int ret = ISP_SUCCESS;

    switch (deboot) {
    case 0:
        wsize = &os06a10_win_sizes[0];
        os06a10_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

        os06a10_attr.again.value = 65536;
        os06a10_attr.again.max = 259142;
        os06a10_attr.again.min = 0;
        os06a10_attr.again.apply_delay = 2;

        os06a10_attr.integration_time.value = 0x7b6;
        os06a10_attr.integration_time.max = 2006 - 8;
        os06a10_attr.integration_time.min = 2;
        os06a10_attr.integration_time.apply_delay = 2;

        os06a10_attr.total_width = 1744;
        os06a10_attr.total_height = 2006;

        os06a10_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
        os06a10_attr.hcg.base_gain = ;
        os06a10_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
        os06a10_attr.again_short.value = ;
        os06a10_attr.again_short.max = ;
        os06a10_attr.again_short.min = ;
        os06a10_attr.again_short.apply_delay = ;

        os06a10_attr.integration_time_short.value = ;
        os06a10_attr.integration_time_short.max = ;
        os06a10_attr.integration_time_short.min = ;
        os06a10_attr.integration_time_short.apply_delay = ;

        os06a10_attr.wdr_cache = wdr_line * os06a10_attr.total_width;

#ifdef SENSOR_HCG
        os06a10_attr.hcg_short.base_gain = ;
        os06a10_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

        memcpy((void *)(&(os06a10_attr.mipi)), (void *)(&os06a10_mipi_linear), sizeof(os06a10_attr.mipi));
        break;
    default:
        ISP_ERROR("Have no this Setting Source!!!\n");
        break;
    }

    return ret;
}

static int os06a10_attr_check(struct tx_isp_subdev *sd)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    struct clk *sclka;
    int ret = ISP_SUCCESS;

    os06a10_setting_select(sd, info->default_boot);

    switch (info->video_interface) {
    case TISP_SENSOR_VI_MIPI_CSI0:
        os06a10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
        os06a10_attr.mipi.index = 0;
        break;
    case TISP_SENSOR_VI_MIPI_CSI1:
        os06a10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
        os06a10_attr.mipi.index = 1;
        break;
    case TISP_SENSOR_VI_DVP:
        os06a10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
        break;
    default:
        ISP_ERROR("Have no this interface!!!\n");
    }

#ifndef ZRT_SENSOR_WITHOUT_INIT
    switch (info->mclk) {
    case TISP_SENSOR_MCLK0:
        sclka = private_devm_clk_get(&client->dev, SEN_MCLK);
        sensor->mclk = private_devm_clk_get(sensor->dev, SEN_BCLK);
        set_sensor_mclk_function(0);
	break;
    case TISP_SENSOR_MCLK1:
        sclka = private_devm_clk_get(&client->dev, SEN_MCLK);
        sensor->mclk = private_devm_clk_get(sensor->dev, SEN_BCLK);
        set_sensor_mclk_function(1);
        break;
    default:
        ISP_ERROR("Have no this MCLK Source!!!\n");
    }

    if (IS_ERR(sensor->mclk)) {
        ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
        goto err_get_mclk;
    }

    ret = os06a10_clk_set(sd, sclka, SENSOR_MCLK);
    if (ret) {
        ISP_ERROR("MCLK configuration failed!!!\n");
    }
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
    os06a10_attr_set(sd, wsize);
    sensor->priv = wsize;

    return 0;

err_get_mclk:
    return -1;
}

static int os06a10_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
    unsigned char v;
    int ret;

    ret = os06a10_read(sd, 0x300a, &v);
    pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    if (ret < 0)
        return ret;
    if (v != SENSOR_CHIP_ID_H)
        return -ENODEV;
    *ident = v;

    ret = os06a10_read(sd, 0x300b, &v);
    pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    if (ret < 0)
        return ret;
    if (v != SENSOR_CHIP_ID_M)
        return -ENODEV;
    *ident = (*ident << 8) | v;

    ret = os06a10_read(sd, 0x300c, &v);
    pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
    if (ret < 0)
        return ret;
    if (v != SENSOR_CHIP_ID_L)
        return -ENODEV;
    *ident = (*ident << 8) | v;

    return 0;
}

static int os06a10_g_chip_ident(struct tx_isp_subdev *sd,
                                struct tx_isp_chip_ident *chip)
{
    struct i2c_client *client = tx_isp_get_subdevdata(sd);
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    unsigned int ident = 0;
    int ret = ISP_SUCCESS;

    os06a10_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
    if (info->rst_gpio != -1) {
        ret = private_gpio_request(info->rst_gpio, "os06a10_reset");
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
        ret = private_gpio_request(info->pwdn_gpio, "os06a10_pwdn");
        if (!ret) {
            private_gpio_direction_output(info->pwdn_gpio, 0);
            private_msleep(5);
            private_gpio_direction_output(info->pwdn_gpio, 1);
            private_msleep(5);
        } else {
            ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
        }
    }
    ret = os06a10_detect(sd, &ident);
    if (ret) {
        ISP_ERROR("chip found @ 0x%x (%s) is not an os06a10 chip.\n",
                  client->addr, client->adapter->name);
        return ret;
    }
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    ISP_WARNING("===================================================\n");
    ISP_WARNING("OS03A10 version is %s\n", TVERSION);
    ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
    ISP_WARNING("Sensor name is %s\n", os06a10_attr.name);
    ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
    ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
    ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
                info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
    ISP_WARNING("===================================================\n");

    if (chip) {
        memcpy(chip->name, "os06a10", sizeof("os06a10"));
        chip->ident = ident;
        chip->revision = SENSOR_VERSION;
    }

    return 0;
}

static int os06a10_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
    return 0;
}

static int os06a10_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
        ret = os06a10_attr_set(sd, wsize);
        sensor->video.state = TX_ISP_MODULE_DEINIT;
    }

    return ret;
}

static int os06a10_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
    ret = os06a10_read(sd, reg->reg & 0xffff, &val);
    reg->val = val;
    reg->size = 2;

    return ret;
}

static int os06a10_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
    int len = 0;

    len = strlen(sd->chip.name);
    if (len && strncmp(sd->chip.name, reg->name, len)) {
        return -EINVAL;
    }
    if (!private_capable(CAP_SYS_ADMIN))
        return -EPERM;
    os06a10_write(sd, reg->reg & 0xffff, reg->val & 0xff);

    return 0;
}

static int os06a10_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    int ret = ISP_SUCCESS;

    if (init->enable) {
        if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
            ret = os06a10_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
            if (ret)
                return ret;
            sensor->video.state = TX_ISP_MODULE_INIT;
        }
        if (sensor->video.state == TX_ISP_MODULE_INIT) {
            ret = os06a10_write_array(sd, os06a10_stream_on_mipi);
            sensor->video.state = TX_ISP_MODULE_RUNNING;
            pr_debug("os06a10 stream on\n");
        }

    } else {
        ret = os06a10_write_array(sd, os06a10_stream_off_mipi);
        sensor->video.state = TX_ISP_MODULE_INIT;
        pr_debug("os06a10 stream off\n");
    }

    return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int os06a10_set_expo(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;
    int it = value & 0xffff;
    int again = (value & 0xffff0000) >> 16;

    ret += os06a10_write(sd, 0x3501, (unsigned char)((it >> 8) & 0xff));
    ret += os06a10_write(sd, 0x3502, (unsigned char)(it & 0xff));

    ret += os06a10_write(sd, 0x3508, (unsigned char)((again >> 8) & 0x3f));
    ret += os06a10_write(sd, 0x3509, (unsigned char)(again & 0xff));

    return ret;
}
#else
static int os06a10_set_integration_time(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ret += os06a10_write(sd, 0x3501, (unsigned char)((value >> 8) & 0xff));
    ret += os06a10_write(sd, 0x3502, (unsigned char)(value & 0xff));

    return ret;
}

static int os06a10_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ret += os06a10_write(sd, 0x3508, (unsigned char)((value >> 8) & 0x3f));
    ret += os06a10_write(sd, 0x3509, (unsigned char)(value & 0xff));

    return ret;
}
#endif /* SENSOR_EXPO */

static int os06a10_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
    return 0;
}

static int os06a10_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
    return 0;
}

static int os06a10_set_mode(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    if (wsize) {
        ret = os06a10_attr_set(sd, wsize);
    }

    return ret;
}

static int os06a10_set_fps(struct tx_isp_subdev *sd, int fps)
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
        sclk = 1744 * 2006 * 30;  /**< HTS * VTS * FPS */
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
    ret += os06a10_read(sd, 0x380c, &val);
    hts = val << 8;
    val = 0;
    ret += os06a10_read(sd, 0x380d, &val);
    hts |= val;
    if (0 != ret) {
        ISP_ERROR("err: os06a10 read err\n");
        return ret;
    }

    vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
    /* ISP_WARNING("hts = %d, vts = %d, fps_num = %d, fps_den = %d\n", hts, vts, ((fps >> 16) & 0xffff), (fps & 0xffff)); */

    os06a10_write(sd, 0x380f, (unsigned char) (vts & 0xff));
    os06a10_write(sd, 0x380e, (unsigned char) (vts >> 8));

    if (0 != ret) {
        ISP_ERROR("err: os06a10_write err\n");
        return ret;
    }

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
        sensor->video.attr->integration_time.max = vts - xx;
        break;
    case 1:
        sensor->video.attr->total_height = vts;
        sensor->video.attr->integration_time.max = vts - xx;
        sensor->video.attr->integration_time_short.max = vts - xx;
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

static int os06a10_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
    int ret = ISP_SUCCESS;
    unsigned char val_3820 = 0;
    unsigned char val_3716 = 0;

    par->drop_frame = 0;
    par->reset = 0;

    /* 2'b01:mirror,2'b10:filp */
    switch(par->hvflip) {
    case TX_ISP_SENSOR_HVFLIP_NOMAL:
        val_3820 = 0x88;
        val_3716 = 0x24;
        break;
    case TX_ISP_SENSOR_HVFLIP_HFLIP:
        val_3820 = 0x80;
        val_3716 = 0x24;
        break;
    case TX_ISP_SENSOR_HVFLIP_VFLIP:
        val_3820 = 0xB8;
        val_3716 = 0x04;
        break;
    case TX_ISP_SENSOR_HVFLIP_HVFLIP:
        val_3820 = 0xB0;
        val_3716 = 0x04;
        break;
    }
    ret = os06a10_write(sd, 0x3820, val_3820);
    ret = os06a10_write(sd, 0x3716, val_3716);

    sensor->video.hvflip_mode = par->hvflip;
    os06a10_attr_set(sd, wsize);

    return ret;
#else
    return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int os06a10_set_expo_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}
#else
static int os06a10_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}

static int os06a10_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
    int ret = ISP_SUCCESS;

    ...;

    return ret;
}
#endif /* SENSOR_EXPO */

static int os06a10_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
    struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;
    struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
    int ret = ISP_SUCCESS;

    ret = os06a10_write_array(sd, os06a10_stream_off_mipi);
    if (par->wdr_en == 1) {
        if (par->boot == -1) {
            info->default_boot = 1;
        }
        os06a10_setting_select(sd, 1);
        os06a10_attr_set(sd, wsize);
    } else if (par->wdr_en == 0) {
        if (par->boot == -1) {
            info->default_boot = 0;
        }
        os06a10_setting_select(sd, 0);
        os06a10_attr_set(sd, wsize);
    } else {
        ISP_ERROR("Can not support this data type!!!");
        return -1;
    }

    return 0;
}

static int os06a10_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
    struct tx_isp_sensor_register_info *info = &sensor->info;
    int ret = ISP_SUCCESS;

    private_gpio_direction_output(info->rst_gpio, 0);
    private_msleep(1);
    private_gpio_direction_output(info->rst_gpio, 1);
    private_msleep(1);

    ret = os06a10_write_array(sd, wsize->regs);
    ret = os06a10_write_array(sd, os06a10_stream_on_mipi);

    return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int os06a10_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
            ret = os06a10_set_expo(sd, sensor_val->value);
        break;
#else
    case TX_ISP_EVENT_SENSOR_INT_TIME:
        if (arg)
            ret = os06a10_set_integration_time(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_AGAIN:
        if (arg)
            ret = os06a10_set_analog_gain(sd, sensor_val->value);
        break;
#endif /* SENSOR_EXPO */
    case TX_ISP_EVENT_SENSOR_DGAIN:
        if (arg)
            ret = os06a10_set_digital_gain(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
        if (arg)
            ret = os06a10_get_black_pedestal(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_RESIZE:
        if (arg)
            ret = os06a10_set_mode(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
        if (arg)
            ret = os06a10_write_array(sd, os06a10_stream_off_mipi);
        break;
    case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
        if (arg)
            ret = os06a10_write_array(sd, os06a10_stream_on_mipi);
        break;
    case TX_ISP_EVENT_SENSOR_FPS:
        if (arg)
            ret = os06a10_set_fps(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_HVFLIP:
        if(arg)
            ret = os06a10_set_hvflip(sd, (void *)sensor_val->value);
        break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
    case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
        if (arg)
            ret = os06a10_set_expo_short(sd, sensor_val->value);
        break;
#else
    case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
        if(arg)
            ret = os06a10_set_integration_time_short(sd, sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
        if(arg)
            ret = os06a10_set_analog_gain_short(sd, sensor_val->value);
        break;
#endif /* SENSOR_EXPO */
    case TX_ISP_EVENT_SENSOR_WDR:
        if(arg)
            ret = os06a10_set_wdr(sd, (void *)sensor_val->value);
        break;
    case TX_ISP_EVENT_SENSOR_WDR_STOP:
        if(arg)
            ret = os06a10_set_wdr_stop(sd, (void *)sensor_val->value);
        break;
#endif /* SENSOR_WDR_2_FRAME */
    default:
        break;
    }

    return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops os06a10_core_ops = {
    .g_chip_ident = os06a10_g_chip_ident,
    .reset = os06a10_reset,
    .init = os06a10_init,
    .g_register = os06a10_g_register,
    .s_register = os06a10_s_register,
};

static struct tx_isp_subdev_video_ops os06a10_video_ops = {
    .s_stream = os06a10_s_stream,
};

static struct tx_isp_subdev_sensor_ops os06a10_sensor_ops = {
#ifndef SENSOR_TEST
    .ioctl	= os06a10_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops os06a10_ops = {
    .core = &os06a10_core_ops,
    .video = &os06a10_video_ops,
    .sensor = &os06a10_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
    .name = "os06a10",
    .id = -1,
    .dev = {
        .dma_mask = &tx_isp_module_dma_mask,
        .coherent_dma_mask = 0xffffffff,
        .platform_data = NULL,
    },
    .num_resources = 0,
};

static int os06a10_probe(struct i2c_client *client,
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
    sensor->video.attr = &os06a10_attr;
    sensor->dev = &client->dev;
    tx_isp_subdev_init(&sensor_platform_device, sd, &os06a10_ops);
    tx_isp_set_subdevdata(sd, client);
    tx_isp_set_subdev_hostdata(sd, sensor);
    private_i2c_set_clientdata(client, sd);

    pr_debug("probe ok ------->os06a10\n");

    return 0;
}

static int os06a10_remove(struct i2c_client *client)
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

static const struct i2c_device_id os06a10_id[] = {
    {"os06a10", 0},
    {}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, os06a10_id);
#endif	/* CONFIG_ZERATUL */

static struct i2c_driver os06a10_driver = {
    .driver = {
#ifdef CONFIG_ZERATUL
        .owner	= NULL,
#else
        .owner	= THIS_MODULE,
#endif	/* CONFIG_ZERATUL */
        .name	= "os06a10",
    },
    .probe			= os06a10_probe,
    .remove			= os06a10_remove,
    .id_table		= os06a10_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_os06a10(void) {
    return private_i2c_add_driver(&os06a10_driver);
}

static __exit void exit_os06a10(void) {
    private_i2c_del_driver(&os06a10_driver);
}

module_init(init_os06a10);
module_exit(exit_os06a10);
#endif	/* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_os06a10(void) {
    return private_i2c_add_driver(&os06a10_driver);
}

static void exit_first_os06a10(void) {
    private_i2c_del_driver(&os06a10_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
    .name = "os06a10",
    .i2c_addr = 0x36,
    .width = 3200,
    .height = 1800,
#ifdef SENSOR_WDR_2_FRAME
    .wdr = 1,
#else
    .wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
    .init_sensor = init_first_os06a10,
    .exit_sensor = exit_first_os06a10
};
#endif	/* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for Sony os06a10 sensor");
MODULE_LICENSE("GPL");
