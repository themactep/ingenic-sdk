// SPDX-License-Identifier: GPL-2.0+
/*
 * os04c10.c
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps     interface              mode
 *   0          2688*1520       20        mipi_2lane           hdr
 *   1          2688*1520       30        mipi_2lane           hdr
 *   2          2688*1520       30        mipi_2lane           linear
 */

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

#define SENSOR_NAME "os04c10"
#define SENSOR_CHIP_ID_H (0x53)
#define SENSOR_CHIP_ID_L (0x04)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0x0000
#define SENSOR_SUPPORT_20FPS_SCLK_HDR (1070 * 20 * 2523)
#define SENSOR_SUPPORT_30FPS_SCLK_HDR (1070 * 30 * 1682)
#define SENSOR_SUPPORT_30FPS_SCLK_LINEAR (1070 * 30 * 3151)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MAX_FPS_DOL 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20220105a"
#define MCLK 24000000

static int reset_gpio = GPIO_PC(27);
static int pwdn_gpio = -1;
static int wdr_bufsize = 1400 * 1000;
static int shvflip = 0;
static unsigned char switch_wdr =0;

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
        unsigned int value;
        unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
    {0x80, 0},
    {0x88, 5731},
    {0x90, 11136},
    {0x98, 16248},
    {0xa0, 21097},
    {0xa8, 25710},
    {0xb0, 30109},
    {0xb8, 34312},
    {0xc0, 38336},
    {0xc8, 42195},
    {0xd0, 45904},
    {0xd8, 49472},
    {0xe0, 52910},
    {0xe8, 56228},
    {0xf0, 59433},
    {0xf8, 62534},
    {0x100, 65536},/*2x*/
    {0x110, 71267},
    {0x120, 76672},
    {0x130, 81784},
    {0x140, 86633},
    {0x150, 91246},
    {0x160, 95645},
    {0x170, 99848},
    {0x180, 103872},
    {0x190, 107731},
    {0x1a0, 111440},
    {0x1b0, 115008},
    {0x1c0, 118446},
    {0x1d0, 121764},
    {0x1e0, 124969},
    {0x1f0, 128070},
    {0x200, 131072},/*4x*/
    {0x220, 136803},
    {0x240, 142208},
    {0x260, 147320},
    {0x280, 152169},
    {0x2a0, 156782},
    {0x2c0, 161181},
    {0x2e0, 165384},
    {0x300, 169408},
    {0x320, 173267},
    {0x340, 176976},
    {0x360, 180544},
    {0x380, 183982},
    {0x3a0, 187300},
    {0x3c0, 190505},
    {0x3e0, 193606},
    {0x400, 196608},
    {0x440, 202339},
    {0x480, 207744},
    {0x4c0, 212856},
    {0x500, 217705},
    {0x540, 222318},
    {0x580, 226717},
    {0x5c0, 230920},
    {0x600, 234944},
    {0x640, 238803},
    {0x680, 242512},
    {0x6c0, 246080},
    {0x700, 249518},
    {0x740, 252836},
    {0x780, 256041},
    {0x7c0, 259142}, /*15.5x*/
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;
	*sensor_it = expo;

	return isp_it;
}
unsigned int sensor_alloc_integration_time_short(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;
	*sensor_it = expo;

	return isp_it;
}

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
        struct again_lut *lut = sensor_again_lut;

        while (lut->gain <= sensor_attr.max_again) {
                if (isp_gain == 0) {
                        *sensor_again = lut->value;
                        return 0;
                } else if (isp_gain < lut->gain) {
                        *sensor_again = (lut - 1)->value;
                        return (lut - 1)->gain;
                } else {
                        if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
                                *sensor_again = lut->value;
                                return lut->gain;
                        }
                }

                lut++;
        }

        return isp_gain;
}
unsigned int sensor_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
        struct again_lut *lut = sensor_again_lut;

        while (lut->gain <= sensor_attr.max_again) {
                if (isp_gain == 0) {
                        *sensor_again = lut->value;
                        return 0;
                } else if (isp_gain < lut->gain) {
                        *sensor_again = (lut - 1)->value;
                        return (lut - 1)->gain;
                } else {
                        if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
                                *sensor_again = lut->value;
                                return lut->gain;
                        }
                }

                lut++;
        }

        return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi_linear={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1456,
	.lans = 2,
	.index = 0,
	.settle_time_apative_en = 0,
	.image_twidth = 2688,
	.image_theight = 1520,
	.mipi_sc.mipi_crop_start0x = 0,
	.mipi_sc.mipi_crop_start0y = 0,
	.mipi_sc.mipi_crop_start1x = 0,
	.mipi_sc.mipi_crop_start1y = 0,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus sensor_mipi_dol = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 720,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 2688,
	.image_theight = 1520,
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
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x5304,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL,
	.cbus_device = 0x36,
	.max_again = 259142,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_short = 4,
	.min_integration_time_native = 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_again_short = sensor_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	.sensor_ctrl.alloc_integration_time_short = sensor_alloc_integration_time_short,
};

static struct regval_list sensor_init_regs_2688_1520_20fps_mipi_dol[] = {
	{0x0103,0x01},
	{0x0301,0x84},
	{0x0303,0x01},
	{0x0305,0x61},
	{0x0306,0x00},
	{0x0307,0x17},
	{0x0323,0x04},
	{0x0324,0x01},
	{0x0325,0x7a},
	{0x3012,0x06},
	{0x3013,0x02},
	{0x3016,0x32},
	{0x3021,0x03},
	{0x3106,0x25},
	{0x3107,0xa1},
	{0x3500,0x00},
	{0x3501,0x03},/**/
	{0x3502,0x08},/*Exp_L=0x308 = 776*/
	{0x3503,0x88},
	{0x3508,0x00},
	{0x3509,0x80},
	{0x350a,0x04},
	{0x350b,0x00},
	{0x350c,0x00},
	{0x350d,0x80},
	{0x350e,0x04},
	{0x350f,0x00},
	{0x3510,0x00},
	{0x3511,0x01},/**/
	{0x3512,0x08},/*Exp_S=0x108 = 264*/
	{0x3624,0x02},
	{0x3625,0x4c},
	{0x3660,0x04},
	{0x3666,0xa5},
	{0x3667,0xa5},
	{0x366a,0x54},
	{0x3673,0x0d},
	{0x3672,0x0d},
	{0x3671,0x0d},
	{0x3670,0x0d},
	{0x3685,0x00},
	{0x3694,0x0d},
	{0x3693,0x0d},
	{0x3692,0x0d},
	{0x3691,0x0d},
	{0x3696,0x4c},
	{0x3697,0x4c},
	{0x3698,0x40},
	{0x3699,0x80},
	{0x369a,0x18},
	{0x369b,0x1f},
	{0x369c,0x14},
	{0x369d,0x80},
	{0x369e,0x40},
	{0x369f,0x21},
	{0x36a0,0x12},
	{0x36a1,0x5d},
	{0x36a2,0x66},
	{0x370a,0x00},
	{0x370e,0x0c},
	{0x3710,0x00},
	{0x3713,0x00},
	{0x3725,0x02},
	{0x372a,0x03},
	{0x3738,0xce},
	{0x3748,0x00},
	{0x374a,0x00},
	{0x374c,0x00},
	{0x374e,0x00},
	{0x3756,0x00},
	{0x3757,0x00},
	{0x3767,0x00},
	{0x3771,0x00},
	{0x377b,0x28},
	{0x377c,0x00},
	{0x377d,0x0c},
	{0x3781,0x03},
	{0x3782,0x00},
	{0x3789,0x14},
	{0x3795,0x02},
	{0x379c,0x00},
	{0x379d,0x00},
	{0x37b8,0x04},
	{0x37ba,0x03},
	{0x37bb,0x00},
	{0x37bc,0x04},
	{0x37be,0x08},
	{0x37c4,0x11},
	{0x37c5,0x80},
	{0x37c6,0x14},
	{0x37c7,0x08},
	{0x37da,0x11},
	{0x381f,0x08},
	{0x3829,0x03},
	{0x3881,0x00},
	{0x3888,0x04},
	{0x388b,0x00},
	{0x3c80,0x10},
	{0x3c86,0x00},
	{0x3c8c,0x20},
	{0x3c9f,0x01},
	{0x3d85,0x1b},
	{0x3d8c,0x71},
	{0x3d8d,0xe2},
	{0x3f00,0x0b},
	{0x3f06,0x04},
	{0x400a,0x01},
	{0x400b,0x50},
	{0x400e,0x08},
	{0x4043,0x7e},
	{0x4045,0x7e},
	{0x4047,0x7e},
	{0x4049,0x7e},
	{0x4090,0x14},
	{0x40b0,0x00},
	{0x40b1,0x00},
	{0x40b2,0x00},
	{0x40b3,0x00},
	{0x40b4,0x00},
	{0x40b5,0x00},
	{0x40b7,0x00},
	{0x40b8,0x00},
	{0x40b9,0x00},
	{0x40ba,0x01},
	{0x4301,0x00},
	{0x4303,0x00},
	{0x4502,0x04},
	{0x4503,0x00},
	{0x4504,0x06},
	{0x4506,0x00},
	{0x4507,0x47},
	{0x4803,0x10},
	{0x480c,0x32},
	{0x480e,0x04},
	{0x4813,0xe4},
	{0x4819,0x70},
	{0x481f,0x30},
	{0x4823,0x3c},
	{0x4825,0x32},
	{0x4833,0x10},
	{0x484b,0x27},
	{0x488b,0x00},
	{0x4d00,0x04},
	{0x4d01,0xad},
	{0x4d02,0xbc},
	{0x4d03,0xa1},
	{0x4d04,0x1f},
	{0x4d05,0x4c},
	{0x4d0b,0x01},
	{0x4e00,0x2a},
	{0x4e0d,0x00},
	{0x5001,0x09},
	{0x5004,0x00},
	{0x5080,0x04},
	{0x5036,0x80},
	{0x5180,0x70},
	{0x5181,0x10},
	{0x520a,0x03},
	{0x520b,0x06},
	{0x520c,0x0c},
	{0x580b,0x0f},
	{0x580d,0x00},
	{0x580f,0x00},
	{0x5820,0x00},
	{0x5821,0x00},
	{0x301c,0xf0},
	{0x301e,0xb4},
	{0x301f,0xf0},
	{0x3022,0x01},
	{0x3109,0xe7},
	{0x3600,0x00},
	{0x3610,0x75},
	{0x3611,0x85},
	{0x3613,0x3a},
	{0x3615,0x60},
	{0x3621,0x90},
	{0x3620,0x0c},
	{0x3629,0x00},
	{0x3661,0x04},
	{0x3664,0x70},
	{0x3665,0x00},
	{0x3681,0xa6},
	{0x3682,0x53},
	{0x3683,0x2a},
	{0x3684,0x15},
	{0x3700,0x2a},
	{0x3701,0x12},
	{0x3703,0x28},
	{0x3704,0x0e},
	{0x3706,0x4a},
	{0x3709,0x4a},
	{0x370b,0xa2},
	{0x370c,0x01},
	{0x370f,0x04},
	{0x3714,0x24},
	{0x3716,0x24},
	{0x3719,0x11},
	{0x371a,0x1e},
	{0x3720,0x00},
	{0x3724,0x13},
	{0x373f,0xb0},
	{0x3741,0x4a},
	{0x3743,0x4a},
	{0x3745,0x4a},
	{0x3747,0x4a},
	{0x3749,0xa2},
	{0x374b,0xa2},
	{0x374d,0xa2},
	{0x374f,0xa2},
	{0x3755,0x10},
	{0x376c,0x00},
	{0x378d,0x30},
	{0x3790,0x4a},
	{0x3791,0xa2},
	{0x3798,0x40},
	{0x379e,0x00},
	{0x379f,0x04},
	{0x37a1,0x10},
	{0x37a2,0x1e},
	{0x37a8,0x10},
	{0x37a9,0x1e},
	{0x37ac,0xa0},
	{0x37b9,0x01},
	{0x37bd,0x01},
	{0x37bf,0x26},
	{0x37c0,0x11},
	{0x37c2,0x04},
	{0x37cd,0x19},
	{0x37e0,0x08},
	{0x37e6,0x04},
	{0x37e5,0x02},
	{0x37e1,0x0c},
	{0x3737,0x04},
	{0x37d8,0x02},
	{0x37e2,0x10},
	{0x3739,0x10},
	{0x3662,0x10},
	{0x37e4,0x20},
	{0x37e3,0x08},
	{0x37d9,0x08},
	{0x4040,0x00},
	{0x4041,0x07},
	{0x4008,0x02},
	{0x4009,0x0d},
	{0x3800,0x00},
	{0x3801,0x00},
	{0x3802,0x00},
	{0x3803,0x00},
	{0x3804,0x0a},
	{0x3805,0x8f},
	{0x3806,0x05},
	{0x3807,0xff},
	{0x3808,0x0a},
	{0x3809,0x80},
	{0x380a,0x05},
	{0x380b,0xf0},
	{0x380c,0x04},//
	{0x380d,0x2e},/*HTS =0x42e = 1070*/
	{0x380e,0x09},//
	{0x380f,0xdb},/*VTS =0x9db = 2523*/
	{0x3811,0x08},
	{0x3813,0x08},
	{0x3814,0x01},
	{0x3815,0x01},
	{0x3816,0x01},
	{0x3817,0x01},
	{0x3820,0x88},
	{0x3821,0x04},
	{0x3880,0x25},
	{0x3882,0x20},
	{0x3c91,0x0b},
	{0x3c94,0x45},
	{0x4000,0xf3},
	{0x4001,0x60},
	{0x4003,0x40},
	{0x4300,0xff},
	{0x4302,0x0f},
	{0x4305,0x83},
	{0x4505,0x84},
	{0x4809,0x0e},
	{0x480a,0x04},
	{0x4837,0x0a},
	{0x4c00,0x08},
	{0x4c01,0x00},
	{0x4c04,0x00},
	{0x4c05,0x00},
	{0x5000,0xf9},
	{0x3624,0x00},

#if 0 //HCG
	{0x320d,0x00},
	{0x3208,0x00},
	{0x3698,0x42},
	{0x3699,0x18},
	{0x369a,0x18},
	{0x369b,0x14},
	{0x369c,0x14},
	{0x369d,0xa6},
	{0x369e,0x53},
	{0x369f,0x2a},
	{0x36a0,0x15},
	{0x36a1,0x53},
	{0x370e,0x0c},
	{0x3713,0x00},
	{0x379c,0x0c},
	{0x379d,0x04},
	{0x37be,0x08},
	{0x37c7,0x08},
	{0x3881,0x25},
	{0x3681,0xa6},
	{0x3682,0x53},
	{0x3683,0x2a},
	{0x3684,0x15},
	{0x370f,0x04},
	{0x379f,0x00},
	{0x37ac,0x00},
	{0x37bf,0x08},
	{0x3880,0x25},
	{0x3208,0x10},
	{0x320d,0x00},
	{0x3208,0xa0},
#endif
	{0x0100,0x01},

	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_2688_1520_30fps_mipi_dol[] = {
	{0x0103,0x01},
	{0x0301,0x84},
	{0x0303,0x01},
	{0x0305,0x61},
	{0x0306,0x00},
	{0x0307,0x17},
	{0x0323,0x04},
	{0x0324,0x01},
	{0x0325,0x7a},
	{0x3012,0x06},
	{0x3013,0x02},
	{0x3016,0x32},
	{0x3021,0x03},
	{0x3106,0x25},
	{0x3107,0xa1},
	{0x3500,0x00},
	{0x3501,0x03},//
	{0x3502,0x08},/* Exp_L = 0x308 = 776*/
	{0x3503,0x88},
	{0x3508,0x00},
	{0x3509,0x80},
	{0x350a,0x04},
	{0x350b,0x00},
	{0x350c,0x00},
	{0x350d,0x80},
	{0x350e,0x04},
	{0x350f,0x00},
	{0x3510,0x00},
	{0x3511,0x01},//
	{0x3512,0x08},/*Exp_S = 0x108 = 264*/
	{0x3624,0x02},
	{0x3625,0x4c},
	{0x3660,0x04},
	{0x3666,0xa5},
	{0x3667,0xa5},
	{0x366a,0x54},
	{0x3673,0x0d},
	{0x3672,0x0d},
	{0x3671,0x0d},
	{0x3670,0x0d},
	{0x3685,0x00},
	{0x3694,0x0d},
	{0x3693,0x0d},
	{0x3692,0x0d},
	{0x3691,0x0d},
	{0x3696,0x4c},
	{0x3697,0x4c},
	{0x3698,0x40},
	{0x3699,0x80},
	{0x369a,0x18},
	{0x369b,0x1f},
	{0x369c,0x14},
	{0x369d,0x80},
	{0x369e,0x40},
	{0x369f,0x21},
	{0x36a0,0x12},
	{0x36a1,0x5d},
	{0x36a2,0x66},
	{0x370a,0x00},
	{0x370e,0x0c},
	{0x3710,0x00},
	{0x3713,0x00},
	{0x3725,0x02},
	{0x372a,0x03},
	{0x3738,0xce},
	{0x3748,0x00},
	{0x374a,0x00},
	{0x374c,0x00},
	{0x374e,0x00},
	{0x3756,0x00},
	{0x3757,0x00},
	{0x3767,0x00},
	{0x3771,0x00},
	{0x377b,0x28},
	{0x377c,0x00},
	{0x377d,0x0c},
	{0x3781,0x03},
	{0x3782,0x00},
	{0x3789,0x14},
	{0x3795,0x02},
	{0x379c,0x00},
	{0x379d,0x00},
	{0x37b8,0x04},
	{0x37ba,0x03},
	{0x37bb,0x00},
	{0x37bc,0x04},
	{0x37be,0x08},
	{0x37c4,0x11},
	{0x37c5,0x80},
	{0x37c6,0x14},
	{0x37c7,0x08},
	{0x37da,0x11},
	{0x381f,0x08},
	{0x3829,0x03},
	{0x3881,0x00},
	{0x3888,0x04},
	{0x388b,0x00},
	{0x3c80,0x10},
	{0x3c86,0x00},
	{0x3c8c,0x20},
	{0x3c9f,0x01},
	{0x3d85,0x1b},
	{0x3d8c,0x71},
	{0x3d8d,0xe2},
	{0x3f00,0x0b},
	{0x3f06,0x04},
	{0x400a,0x01},
	{0x400b,0x50},
	{0x400e,0x08},
	{0x4043,0x7e},
	{0x4045,0x7e},
	{0x4047,0x7e},
	{0x4049,0x7e},
	{0x4090,0x14},
	{0x40b0,0x00},
	{0x40b1,0x00},
	{0x40b2,0x00},
	{0x40b3,0x00},
	{0x40b4,0x00},
	{0x40b5,0x00},
	{0x40b7,0x00},
	{0x40b8,0x00},
	{0x40b9,0x00},
	{0x40ba,0x01},
	{0x4301,0x00},
	{0x4303,0x00},
	{0x4502,0x04},
	{0x4503,0x00},
	{0x4504,0x06},
	{0x4506,0x00},
	{0x4507,0x47},
	{0x4803,0x10},
	{0x480c,0x32},
	{0x480e,0x04},
	{0x4813,0xe4},
	{0x4819,0x70},
	{0x481f,0x30},
	{0x4823,0x3c},
	{0x4825,0x32},
	{0x4833,0x10},
	{0x484b,0x27},
	{0x488b,0x00},
	{0x4d00,0x04},
	{0x4d01,0xad},
	{0x4d02,0xbc},
	{0x4d03,0xa1},
	{0x4d04,0x1f},
	{0x4d05,0x4c},
	{0x4d0b,0x01},
	{0x4e00,0x2a},
	{0x4e0d,0x00},
	{0x5001,0x09},
	{0x5004,0x00},
	{0x5080,0x04},
	{0x5036,0x80},
	{0x5180,0x70},
	{0x5181,0x10},
	{0x520a,0x03},
	{0x520b,0x06},
	{0x520c,0x0c},
	{0x580b,0x0f},
	{0x580d,0x00},
	{0x580f,0x00},
	{0x5820,0x00},
	{0x5821,0x00},
	{0x301c,0xf0},
	{0x301e,0xb4},
	{0x301f,0xf0},
	{0x3022,0x01},
	{0x3109,0xe7},
	{0x3600,0x00},
	{0x3610,0x75},
	{0x3611,0x85},
	{0x3613,0x3a},
	{0x3615,0x60},
	{0x3621,0x90},
	{0x3620,0x0c},
	{0x3629,0x00},
	{0x3661,0x04},
	{0x3664,0x70},
	{0x3665,0x00},
	{0x3681,0xa6},
	{0x3682,0x53},
	{0x3683,0x2a},
	{0x3684,0x15},
	{0x3700,0x2a},
	{0x3701,0x12},
	{0x3703,0x28},
	{0x3704,0x0e},
	{0x3706,0x4a},
	{0x3709,0x4a},
	{0x370b,0xa2},
	{0x370c,0x01},
	{0x370f,0x04},
	{0x3714,0x24},
	{0x3716,0x24},
	{0x3719,0x11},
	{0x371a,0x1e},
	{0x3720,0x00},
	{0x3724,0x13},
	{0x373f,0xb0},
	{0x3741,0x4a},
	{0x3743,0x4a},
	{0x3745,0x4a},
	{0x3747,0x4a},
	{0x3749,0xa2},
	{0x374b,0xa2},
	{0x374d,0xa2},
	{0x374f,0xa2},
	{0x3755,0x10},
	{0x376c,0x00},
	{0x378d,0x30},
	{0x3790,0x4a},
	{0x3791,0xa2},
	{0x3798,0x40},
	{0x379e,0x00},
	{0x379f,0x04},
	{0x37a1,0x10},
	{0x37a2,0x1e},
	{0x37a8,0x10},
	{0x37a9,0x1e},
	{0x37ac,0xa0},
	{0x37b9,0x01},
	{0x37bd,0x01},
	{0x37bf,0x26},
	{0x37c0,0x11},
	{0x37c2,0x04},
	{0x37cd,0x19},
	{0x37e0,0x08},
	{0x37e6,0x04},
	{0x37e5,0x02},
	{0x37e1,0x0c},
	{0x3737,0x04},
	{0x37d8,0x02},
	{0x37e2,0x10},
	{0x3739,0x10},
	{0x3662,0x10},
	{0x37e4,0x20},
	{0x37e3,0x08},
	{0x37d9,0x08},
	{0x4040,0x00},
	{0x4041,0x07},
	{0x4008,0x02},
	{0x4009,0x0d},
	{0x3800,0x00},
	{0x3801,0x00},
	{0x3802,0x00},
	{0x3803,0x00},
	{0x3804,0x0a},
	{0x3805,0x8f},
	{0x3806,0x05},
	{0x3807,0xff},
	{0x3808,0x0a},
	{0x3809,0x80},
	{0x380a,0x05},
	{0x380b,0xf0},
	{0x380c,0x04},//
	{0x380d,0x2e},/*HTS = 0x42e = 1070*/
	{0x380e,0x06},//
	{0x380f,0x92},/*VTS = 0x692 = 1682*/
	{0x3811,0x08},
	{0x3813,0x08},
	{0x3814,0x01},
	{0x3815,0x01},
	{0x3816,0x01},
	{0x3817,0x01},
	{0x3820,0x88},
	{0x3821,0x04},
	{0x3880,0x25},
	{0x3882,0x20},
	{0x3c91,0x0b},
	{0x3c94,0x45},
	{0x4000,0xf3},
	{0x4001,0x60},
	{0x4003,0x40},
	{0x4300,0xff},
	{0x4302,0x0f},
	{0x4305,0x83},
	{0x4505,0x84},
	{0x4809,0x0e},
	{0x480a,0x04},
	{0x4837,0x0a},
	{0x4c00,0x08},
	{0x4c01,0x00},
	{0x4c04,0x00},
	{0x4c05,0x00},
	{0x5000,0xf9},
	{0x3624,0x00},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_2688_1520_30fps_mipi_linear[] = {
	{0x0100,0x00},
	{0x0103,0x01},
	{0x0301,0x84},
	{0x0303,0x01},
	{0x0305,0x5b},
	{0x0306,0x00},
	{0x0307,0x17},
	{0x0323,0x04},
	{0x0324,0x01},
	{0x0325,0x62},
	{0x3012,0x06},
	{0x3013,0x02},
	{0x3016,0x32},
	{0x3021,0x03},
	{0x3106,0x25},
	{0x3107,0xa1},
	{0x3500,0x00},
	{0x3501,0x04},
	{0x3502,0x40},
	{0x3503,0x88},
	{0x3508,0x00},
	{0x3509,0x80},
	{0x350a,0x04},
	{0x350b,0x00},
	{0x350c,0x00},
	{0x350d,0x80},
	{0x350e,0x04},
	{0x350f,0x00},
	{0x3510,0x00},
	{0x3511,0x01},
	{0x3512,0x20},
	{0x3624,0x02},
	{0x3625,0x4c},
	{0x3660,0x00},
	{0x3666,0xa5},
	{0x3667,0xa5},
	{0x366a,0x64},
	{0x3673,0x0d},
	{0x3672,0x0d},
	{0x3671,0x0d},
	{0x3670,0x0d},
	{0x3685,0x00},
	{0x3694,0x0d},
	{0x3693,0x0d},
	{0x3692,0x0d},
	{0x3691,0x0d},
	{0x3696,0x4c},
	{0x3697,0x4c},
	{0x3698,0x40},
	{0x3699,0x80},
	{0x369a,0x18},
	{0x369b,0x1f},
	{0x369c,0x14},
	{0x369d,0x80},
	{0x369e,0x40},
	{0x369f,0x21},
	{0x36a0,0x12},
	{0x36a1,0x5d},
	{0x36a2,0x66},
	{0x370a,0x00},
	{0x370e,0x0c},
	{0x3710,0x00},
	{0x3713,0x00},
	{0x3725,0x02},
	{0x372a,0x03},
	{0x3738,0xce},
	{0x3748,0x00},
	{0x374a,0x00},
	{0x374c,0x00},
	{0x374e,0x00},
	{0x3756,0x00},
	{0x3757,0x0e},
	{0x3767,0x00},
	{0x3771,0x00},
	{0x377b,0x20},
	{0x377c,0x00},
	{0x377d,0x0c},
	{0x3781,0x03},
	{0x3782,0x00},
	{0x3789,0x14},
	{0x3795,0x02},
	{0x379c,0x00},
	{0x379d,0x00},
	{0x37b8,0x04},
	{0x37ba,0x03},
	{0x37bb,0x00},
	{0x37bc,0x04},
	{0x37be,0x08},
	{0x37c4,0x11},
	{0x37c5,0x80},
	{0x37c6,0x14},
	{0x37c7,0x08},
	{0x37da,0x11},
	{0x381f,0x08},
	{0x3829,0x03},
	{0x3881,0x00},
	{0x3888,0x04},
	{0x388b,0x00},
	{0x3c80,0x10},
	{0x3c86,0x00},
	{0x3c8c,0x20},
	{0x3c9f,0x01},
	{0x3d85,0x1b},
	{0x3d8c,0x71},
	{0x3d8d,0xe2},
	{0x3f00,0x0b},
	{0x3f06,0x04},
	{0x400a,0x01},
	{0x400b,0x50},
	{0x400e,0x08},
	{0x4043,0x7e},
	{0x4045,0x7e},
	{0x4047,0x7e},
	{0x4049,0x7e},
	{0x4090,0x14},
	{0x40b0,0x00},
	{0x40b1,0x00},
	{0x40b2,0x00},
	{0x40b3,0x00},
	{0x40b4,0x00},
	{0x40b5,0x00},
	{0x40b7,0x00},
	{0x40b8,0x00},
	{0x40b9,0x00},
	{0x40ba,0x00},
	{0x4301,0x00},
	{0x4303,0x00},
	{0x4502,0x04},
	{0x4503,0x00},
	{0x4504,0x06},
	{0x4506,0x00},
	{0x4507,0x64},
	{0x4803,0x10},
	{0x480c,0x32},
	{0x480e,0x00},
	{0x4813,0x00},
	{0x4819,0x70},
	{0x481f,0x30},
	{0x4823,0x3c},
	{0x4825,0x32},
	{0x4833,0x10},
	{0x484b,0x07},
	{0x488b,0x00},
	{0x4d00,0x04},
	{0x4d01,0xad},
	{0x4d02,0xbc},
	{0x4d03,0xa1},
	{0x4d04,0x1f},
	{0x4d05,0x4c},
	{0x4d0b,0x01},
	{0x4e00,0x2a},
	{0x4e0d,0x00},
	{0x5001,0x09},
	{0x5004,0x00},
	{0x5080,0x04},
	{0x5036,0x00},
	{0x5180,0x70},
	{0x5181,0x10},
	{0x520a,0x03},
	{0x520b,0x06},
	{0x520c,0x0c},
	{0x580b,0x0f},
	{0x580d,0x00},
	{0x580f,0x00},
	{0x5820,0x00},
	{0x5821,0x00},
	{0x301c,0xf0},
	{0x301e,0xb4},
	{0x301f,0xd0},
	{0x3022,0x01},
	{0x3109,0xe7},
	{0x3600,0x00},
	{0x3610,0x65},
	{0x3611,0x85},
	{0x3613,0x3a},
	{0x3615,0x60},
	{0x3621,0x90},
	{0x3620,0x0c},
	{0x3629,0x00},
	{0x3661,0x04},
	{0x3664,0x70},
	{0x3665,0x00},
	{0x3681,0xa6},
	{0x3682,0x53},
	{0x3683,0x2a},
	{0x3684,0x15},
	{0x3700,0x2a},
	{0x3701,0x12},
	{0x3703,0x28},
	{0x3704,0x0e},
	{0x3706,0x4a},
	{0x3709,0x4a},
	{0x370b,0xa2},
	{0x370c,0x01},
	{0x370f,0x04},
	{0x3714,0x24},
	{0x3716,0x24},
	{0x3719,0x11},
	{0x371a,0x1e},
	{0x3720,0x00},
	{0x3724,0x13},
	{0x373f,0xb0},
	{0x3741,0x4a},
	{0x3743,0x4a},
	{0x3745,0x4a},
	{0x3747,0x4a},
	{0x3749,0xa2},
	{0x374b,0xa2},
	{0x374d,0xa2},
	{0x374f,0xa2},
	{0x3755,0x10},
	{0x0370,0x00},
	{0x378d,0x30},
	{0x3790,0x4a},
	{0x3791,0xa2},
	{0x3798,0x40},
	{0x379e,0x00},
	{0x379f,0x04},
	{0x37a1,0x10},
	{0x37a2,0x1e},
	{0x37a8,0x10},
	{0x37a9,0x1e},
	{0x37ac,0xa0},
	{0x37b9,0x01},
	{0x37bd,0x01},
	{0x37bf,0x26},
	{0x37c0,0x11},
	{0x37c2,0x04},
	{0x37cd,0x19},
	{0x37e0,0x08},
	{0x37e6,0x04},
	{0x37e5,0x02},
	{0x37e1,0x0c},
	{0x3737,0x04},
	{0x37d8,0x02},
	{0x37e2,0x10},
	{0x3739,0x10},
	{0x3662,0x10},
	{0x37e4,0x20},
	{0x37e3,0x08},
	{0x37d9,0x08},
	{0x4040,0x00},
	{0x4041,0x07},
	{0x4008,0x02},
	{0x4009,0x0d},
	{0x3800,0x00},
	{0x3801,0x00},
	{0x3802,0x00},
	{0x3803,0x00},
	{0x3804,0x0a},
	{0x3805,0x8f},
	{0x3806,0x05},
	{0x3807,0xff},
	{0x3808,0x0a},
	{0x3809,0x80},
	{0x380a,0x05},
	{0x380b,0xf0},
	{0x380c,0x04},//
	{0x380d,0x2e},/*hts = 1070*/
	{0x380e,0x0c},//
	{0x380f,0x4f},/*vts = 3151*/
	{0x3811,0x08},
	{0x3813,0x08},
	{0x3814,0x01},
	{0x3815,0x01},
	{0x3816,0x01},
	{0x3817,0x01},
	{0x3820,0x88},
	{0x3821,0x00},
	{0x3880,0x25},
	{0x3882,0x20},
	{0x3c91,0x0b},
	{0x3c94,0x45},
	{0x4000,0xf3},
	{0x4001,0x60},
	{0x4003,0x40},
	{0x4300,0xff},
	{0x4302,0x0f},
	{0x4305,0x83},
	{0x4505,0x84},
	{0x4809,0x1e},
	{0x480a,0x04},
	{0x4837,0x0a},
	{0x4c00,0x08},
	{0x4c01,0x00},
	{0x4c04,0x00},
	{0x4c05,0x00},
	{0x5000,0xf9},
	{0x3624,0x00},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},
};
/*
 * the order of the jxf23_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 2688,
		.height = 1520,
		.fps = 20 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2688_1520_20fps_mipi_dol,
	},
	{
		.width = 2688,
		.height = 1520,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2688_1520_30fps_mipi_dol,
	},
	{
		.width = 2688,
		.height = 1520,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2688_1520_30fps_mipi_linear,
	}
};

struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sensor_stream_on[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd,  uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg>>8)&0xff, reg&0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = buf,
		},
		[1] = {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = value,
		}
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg,
		 unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 3,
		.buf = buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 0
static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sensor_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}
	return 0;
}
#endif

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sensor_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

#if 1
static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = sensor_read(sd, 0x300a, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	ret = sensor_read(sd, 0x300b, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}
#endif

static int sensor_set_expo(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = 0;
	int expo = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	if (info->default_boot == 1) {
		if (expo > 1405) expo = 1405;
	} else if (info->default_boot == 0) {
		if (expo > 2247) expo = 2247;
	}

	ret += sensor_write(sd, 0x3502, (unsigned char)(expo & 0xff));
	ret += sensor_write(sd, 0x3501, (unsigned char)((expo >> 8) & 0xff));

	ret += sensor_write(sd, 0x3509, (unsigned char)((again & 0xff)));
	ret += sensor_write(sd, 0x3508, (unsigned char)((again >> 8) & 0xff));

	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = 0;

	if (info->default_boot == 1) {
		if (value > 1505) value = 1504;
	}

	ret = sensor_write(sd, 0x0203, value & 0xff);
	ret += sensor_write(sd, 0x0202, value >> 8);
	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}
#endif

static int sensor_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
        int ret = 0;
        unsigned int expo = value;

        if (expo > 264) expo = 264;
        ret += sensor_write(sd, 0x3512, (unsigned char)(expo & 0xff));
        ret += sensor_write(sd, 0x3511, (unsigned char)((expo >> 8) & 0xff));
        if (ret < 0)
                return ret;

        return 0;
}

#if 0
static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	struct again_lut *val_lut = sensor_again_lut;


	ret = sensor_write(sd, 0x02b3, val_lut[value].reg2b3);
	ret = sensor_write(sd, 0x02b4, val_lut[value].reg2b4);
	ret = sensor_write(sd, 0x02b8, val_lut[value].reg2b8);
	ret = sensor_write(sd, 0x02b9, val_lut[value].reg2b9);
	ret = sensor_write(sd, 0x0515, val_lut[value].reg515);
	ret = sensor_write(sd, 0x0519, val_lut[value].reg519);
	ret = sensor_write(sd, 0x02d9, val_lut[value].reg2d9);

	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d" ,__LINE__ );
		return ret;
	}

	return 0;
}
#endif

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	sensor->video.state = TX_ISP_MODULE_DEINIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
			ret = sensor_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sensor_write_array(sd, sensor_stream_on);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("%s stream on\n", SENSOR_NAME);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
	} else {
		ret = sensor_write_array(sd, sensor_stream_off);
		pr_debug("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int wpclk = 0;
	unsigned short vts = 0;
	unsigned short hts=0;
	unsigned int sensor_max_fps;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;
	unsigned char val;
	unsigned int short_time;

	switch(sensor->info.default_boot) {
	case 0:
		wpclk = SENSOR_SUPPORT_20FPS_SCLK_HDR;
		sensor_max_fps = TX_SENSOR_MAX_FPS_20;
		break;
	case 1:
		wpclk = SENSOR_SUPPORT_30FPS_SCLK_HDR;
		sensor_max_fps = TX_SENSOR_MAX_FPS_30;
		break;
	case 2:
		wpclk = SENSOR_SUPPORT_30FPS_SCLK_LINEAR;
		sensor_max_fps = TX_SENSOR_MAX_FPS_30;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	/* the format of fps is 16/16. for example 30 << 16 | 2, the value is 30/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (sensor_max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}

	ret += sensor_read(sd, 0x380c, &val);
	hts = val<<8;
	val = 0;
	ret += sensor_read(sd, 0x380d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}

	vts = wpclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += sensor_write(sd, 0x380f, vts & 0xff);
	ret += sensor_write(sd, 0x380e, (vts >> 8) & 0xff);
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}

	if ((sensor->info.default_boot == 0) || (sensor->info.default_boot == 1)) {
		ret = sensor_read(sd, 0x3511, &val);
		short_time = val << 8;
		ret += sensor_read(sd, 0x3512, &val);
		short_time |= val;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = (sensor->info.default_boot == 2) ? (vts -12) : (vts -short_time-12);
	sensor->video.attr->integration_time_limit = (sensor->info.default_boot == 2) ? (vts -12) : (vts -short_time-12);
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = (sensor->info.default_boot == 2) ? (vts -12) : (vts -short_time-12);

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;
}


static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	return ret;
}


static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = TISP_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}
	return ret;
}

struct clk *sclka;
static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = 0;

	switch(info->default_boot) {
	case 0:
		sensor_attr.wdr_cache = wdr_bufsize;
		wsize = &sensor_win_sizes[0];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		memcpy(&sensor_attr.mipi, &sensor_mipi_dol, sizeof(sensor_mipi_dol));
		sensor_attr.min_integration_time = 5;
		sensor_attr.min_integration_time_short = 5;
		sensor_attr.total_width = 1070;
		sensor_attr.total_height = 2523; /*(Exp_L + Exp_S) < VTS–12*/
		sensor_attr.max_integration_time_native = 2247;
		sensor_attr.integration_time_limit = 2247;
		sensor_attr.max_integration_time = 2247;
		sensor_attr.max_integration_time_short = 264;
		sensor_attr.again = 0x80;
		sensor_attr.integration_time =0x308;
		printk("=================> 4M@20fps_HDR\n");
		break;
	case 1:
		sensor_attr.wdr_cache = wdr_bufsize;
		wsize = &sensor_win_sizes[1];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		memcpy(&sensor_attr.mipi, &sensor_mipi_dol, sizeof(sensor_mipi_dol));
		sensor_attr.min_integration_time = 5;
		sensor_attr.min_integration_time_short = 5;
		sensor_attr.total_width = 1070;
		sensor_attr.total_height = 1682;
		sensor_attr.max_integration_time_native = 1405;
		sensor_attr.integration_time_limit = 1405;
		sensor_attr.max_integration_time = 1405; //1670
		sensor_attr.max_integration_time_short = 264;
		sensor_attr.again = 0x80;
		sensor_attr.integration_time =0x308;
		printk("=================> 4M@30fps_HDR\n");
		break;
	case 2:
		wsize = &sensor_win_sizes[2];
		memcpy(&sensor_attr.mipi, &sensor_mipi_linear, sizeof(sensor_mipi_linear));
		sensor_attr.min_integration_time = 5;
		sensor_attr.total_width = 1070;
		sensor_attr.total_height = 3151;
		sensor_attr.max_integration_time_native = 3151 -12;
		sensor_attr.integration_time_limit = 3151 -12;
		sensor_attr.max_integration_time = 3151 -12;
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor_attr.again = 0x80;
		sensor_attr.integration_time =0x440;
		printk("=================> 4M@30fps_linear\n");
		break;
	default:
		ISP_ERROR("Have no this setting!!!\n");
	}

	switch(info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this interface!!!\n");
	}

	switch(info->mclk) {
	case TISP_SENSOR_MCLK0:
		sclka = private_devm_clk_get(&client->dev, "mux_cim0");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim0");
		set_sensor_mclk_function(0);
		break;
	case TISP_SENSOR_MCLK1:
		sclka = private_devm_clk_get(&client->dev, "mux_cim1");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim1");
		set_sensor_mclk_function(1);
		break;
	case TISP_SENSOR_MCLK2:
		sclka = private_devm_clk_get(&client->dev, "mux_cim2");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim2");
		set_sensor_mclk_function(2);
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	rate = private_clk_get_rate(sensor->mclk);

	if (MCLK == 27000000) {
		if (((rate / 1000) % 27000) != 0) {
			ret = clk_set_parent(sclka, clk_get(NULL, "epll"));
			sclka = private_devm_clk_get(&client->dev, "epll");
			if (IS_ERR(sclka)) {
				pr_err("get sclka failed\n");
			} else {
				rate = private_clk_get_rate(sclka);
				if (((rate / 1000) % 27000) != 0) {
					private_clk_set_rate(sclka, 891000000);
				}
			}
		}
	}

	private_clk_set_rate(sensor->mclk, MCLK);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	return 0;

err_get_mclk:
	return -1;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio request fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio request fail %d\n",pwdn_gpio);
		}
	}
	ret = sensor_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an %s chip.\n",
			  client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}
	ISP_WARNING("%s chip found @ 0x%02x (%s)\n",
		    SENSOR_NAME, client->addr, client->adapter->name);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

#if 1
static int sensor_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = 0;

	ret = sensor_write(sd, 0x0103, 0x1);

	if (wdr_en == 1) {
		if (switch_wdr == 0) {
			info->default_boot = 0;
			memcpy(&sensor_attr.mipi, &sensor_mipi_dol, sizeof(sensor_mipi_dol));
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
			sensor_attr.wdr_cache = wdr_bufsize;
			wsize = &sensor_win_sizes[0];
			sensor_attr.min_integration_time = 5;
			sensor_attr.min_integration_time_short = 5;
			sensor_attr.total_width = 1070;
			sensor_attr.total_height = 2523; /*(Exp_L + Exp_S) < VTS–12*/
			sensor_attr.max_integration_time_native = 2247;
			sensor_attr.integration_time_limit = 2247;
			sensor_attr.max_integration_time = 2247;
			sensor_attr.max_integration_time_short = 264;
			printk("\n-------------------------switch wdr@20fps ok ----------------------\n");
		} else {
			info->default_boot=1;
			memcpy(&sensor_attr.mipi, &sensor_mipi_dol, sizeof(sensor_mipi_dol));
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
			sensor_attr.wdr_cache = wdr_bufsize;
			wsize = &sensor_win_sizes[1];
			sensor_attr.min_integration_time = 5;
			sensor_attr.min_integration_time_short = 5;
			sensor_attr.total_width = 1070;
			sensor_attr.total_height = 1682;
			sensor_attr.max_integration_time_native = 1405;
			sensor_attr.integration_time_limit = 1405;
			sensor_attr.max_integration_time = 1405; //1670
			sensor_attr.max_integration_time_short = 264;
			printk("\n-------------------------switch wdr@30fps ok ----------------------\n");
		}
	} else if (wdr_en == 0) {
		switch_wdr = info->default_boot;
		info->default_boot = 2;
		memcpy(&sensor_attr.mipi, &sensor_mipi_linear, sizeof(sensor_mipi_linear));
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		wsize = &sensor_win_sizes[2];
		sensor_attr.min_integration_time = 5;
		sensor_attr.total_width = 1070;
		sensor_attr.total_height = 3151;
		sensor_attr.max_integration_time_native = 3151 -12;
		sensor_attr.integration_time_limit = 3151 -12;
		sensor_attr.max_integration_time = 3151 -12;
		printk("\n-------------------------switch linear ok ----------------------\n");
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int sensor_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
	int ret = 0;

	private_gpio_direction_output(reset_gpio, 1);
	private_msleep(1);
	private_gpio_direction_output(reset_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(reset_gpio, 1);
	private_msleep(1);

	ret = sensor_write_array(sd, wsize->regs);
	ret = sensor_write_array(sd, sensor_stream_on);

	return 0;
}
#endif

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	struct tx_isp_initarg *init = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = sensor_set_expo(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
	//	if (arg)
	//		ret = sensor_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
	//	if (arg)
	//		ret = sensor_set_analog_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sensor_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sensor_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sensor_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = sensor_write_array(sd, sensor_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sensor_write_array(sd, sensor_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if (arg)
			ret = sensor_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR:
		if (arg)
			ret = sensor_set_wdr(sd, init->enable);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if (arg)
			ret = sensor_set_wdr_stop(sd, init->enable);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if (arg)
			ret = sensor_set_vflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int sensor_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = sensor_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sensor_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sensor_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sensor_core_ops = {
	.g_chip_ident = sensor_g_chip_ident,
	.reset = sensor_reset,
	.init = sensor_init,
	/*.ioctl = sensor_ops_ioctl,*/
	.g_register = sensor_g_register,
	.s_register = sensor_s_register,
};

static struct tx_isp_subdev_video_ops sensor_video_ops = {
	.s_stream = sensor_s_stream,
};

static struct tx_isp_subdev_sensor_ops sensor_sensor_ops = {
	.ioctl = sensor_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.sensor = &sensor_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = SENSOR_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));
	sensor->dev = &client->dev;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor_attr.expo_fs = 1;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sensor_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
	},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

static __init int init_sensor(void)
{
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
