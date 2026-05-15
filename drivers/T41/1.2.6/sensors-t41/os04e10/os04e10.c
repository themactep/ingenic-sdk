/*
 * os04e10.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          2048*2048       30        mipi_2lane             linear
 *   1          512*512         30        mipi_2lane             linear
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
#define SENSOR_VERSION  "H20240321a"

//#define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */

#define SENSOR_CHIP_ID_H    (0x53)
#define SENSOR_CHIP_ID_M    (0x04)
#define SENSOR_CHIP_ID_L    (0x45)
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

struct tx_isp_sensor_attribute os04e10_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
        unsigned int value;
        unsigned int gain;
};

struct again_lut os04e10_again_lut[] = {
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

unsigned int os04e10_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
        /* Analog gain table */
        struct again_lut *lut = os04e10_again_lut;
        while (lut->gain <= os04e10_attr.max_again) {
                if (isp_gain == 0) {
                        *sensor_again = lut->value;
                        return 0;
                } else if (isp_gain < lut->gain) {
                        *sensor_again = (lut - 1)->value;
                        return (lut - 1)->gain;
                } else {
                        if ((lut->gain == os04e10_attr.max_again) && (isp_gain >= lut->gain)) {
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
unsigned int os04e10_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
        /* Analog gain table */
        struct again_lut *lut = os04e10_again_lut;
        while(lut->gain <= os04e10_attr.max_again_short) {
                if(isp_gain == 0) {
                        *sensor_again = lut->value;
                        return 0;
                }
                else if(isp_gain < lut->gain) {
                        *sensor_again = (lut - 1)->value;
                        return (lut - 1)->gain;
                }
                else{
                        if((lut->gain == os04e10_attr.max_again_short) && (isp_gain >= lut->gain)) {
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

unsigned int os04e10_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
        return 0;
}

struct tx_isp_mipi_bus os04e10_mipi_linear = {
        .mode = SENSOR_MIPI_OTHER_MODE,
        .clk = 900,
        .lans = 2,
        .image_twidth = 2048,
        .image_theight = 2048,
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

struct tx_isp_mipi_bus os04e10_mipi_512_linear = {
        .mode = SENSOR_MIPI_OTHER_MODE,
        .clk = 900,
        .lans = 2,
        .image_twidth = 512,
        .image_theight = 512,
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

struct tx_isp_dvp_bus os04e10_dvp = {
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

struct tx_isp_sensor_attribute os04e10_attr = {
        .name = "os04e10",
        .chip_id = 0x530445,
        .cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
        .cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
        .cbus_device = 0x36,
        .sensor_ctrl.alloc_again = os04e10_alloc_again,
        .sensor_ctrl.alloc_dgain = os04e10_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
        .sensor_ctrl.alloc_again_short = os04e10_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */
};

static struct regval_list os04e10_init_regs_2048_2048_30fps_mipi[] = {
        {0x0103, 0x01},
        {0x0301, 0x44},
        {0x0303, 0x02},
        {0x0304, 0x00},
        {0x0305, 0x4b},
        {0x0306, 0x00},
        {0x0325, 0x3b},
        {0x0327, 0x04},
        {0x0328, 0x05},
        {0x3002, 0x21},
        {0x3016, 0x32},
        {0x301b, 0xf0},
        {0x301e, 0xb4},
        {0x301f, 0xd0},
        {0x3021, 0x03},
        {0x3022, 0x01},
        {0x3107, 0xa1},
        {0x3108, 0x7d},
        {0x3109, 0xfc},
        {0x3500, 0x00},
        {0x3501, 0x08},
        {0x3502, 0x54},
        {0x3503, 0x88},
        {0x3508, 0x01},
        {0x3509, 0x00},
        {0x350a, 0x04},
        {0x350b, 0x00},
        {0x350c, 0x04},
        {0x350d, 0x00},
        {0x350e, 0x04},
        {0x350f, 0x00},
        {0x3510, 0x00},
        {0x3511, 0x00},
        {0x3512, 0x20},
        {0x3600, 0x4c},
        {0x3601, 0x08},
        {0x3610, 0x87},
        {0x3611, 0x24},
        {0x3614, 0x4c},
        {0x3620, 0x0c},
        {0x3621, 0x04},
        {0x3632, 0x80},
        {0x3633, 0x00},
        {0x3660, 0x00},
        {0x3662, 0x10},
        {0x3664, 0x70},
        {0x3665, 0x00},
        {0x3666, 0x00},
        {0x3667, 0x00},
        {0x366a, 0x14},
        {0x3670, 0x0b},
        {0x3671, 0x0b},
        {0x3672, 0x0b},
        {0x3673, 0x0b},
        {0x3674, 0x00},
        {0x3678, 0x2b},
        {0x3679, 0x43},
        {0x3681, 0xff},
        {0x3682, 0x86},
        {0x3683, 0x44},
        {0x3684, 0x24},
        {0x3685, 0x00},
        {0x368a, 0x00},
        {0x368d, 0x2b},
        {0x368e, 0x6b},
        {0x3690, 0x00},
        {0x3691, 0x0b},
        {0x3692, 0x0b},
        {0x3693, 0x0b},
        {0x3694, 0x0b},
        {0x3699, 0x03},
        {0x369d, 0x68},
        {0x369e, 0x34},
        {0x369f, 0x1b},
        {0x36a0, 0x0f},
        {0x36a1, 0x77},
        {0x36a2, 0x00},
        {0x36a3, 0x02},
        {0x36a4, 0x02},
        {0x36b0, 0x30},
        {0x36b1, 0xf0},
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
        {0x36c0, 0x1f},
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
        {0x3704, 0x05},
        {0x3705, 0x00},
        {0x3706, 0x2b},
        {0x3709, 0x49},
        {0x370a, 0x00},
        {0x370b, 0x60},
        {0x370e, 0x0c},
        {0x370f, 0x1c},
        {0x3710, 0x00},
        {0x3713, 0x00},
        {0x3714, 0x24},
        {0x3716, 0x24},
        {0x371a, 0x1e},
        {0x3724, 0x0d},
        {0x3725, 0xb2},
        {0x372b, 0x54},
        {0x3739, 0x10},
        {0x373f, 0xb0},
        {0x3740, 0x2b},
        {0x3741, 0x2b},
        {0x3742, 0x2b},
        {0x3743, 0x2b},
        {0x3744, 0x60},
        {0x3745, 0x60},
        {0x3746, 0x60},
        {0x3747, 0x60},
        {0x3748, 0x00},
        {0x3749, 0x00},
        {0x374a, 0x00},
        {0x374b, 0x00},
        {0x374c, 0x00},
        {0x374d, 0x00},
        {0x374e, 0x00},
        {0x374f, 0x00},
        {0x3756, 0x00},
        {0x3757, 0x0e},
        {0x3760, 0x11},
        {0x3767, 0x08},
        {0x3773, 0x01},
        {0x3774, 0x02},
        {0x3775, 0x12},
        {0x3776, 0x02},
        {0x377b, 0x40},
        {0x377c, 0x00},
        {0x377d, 0x0c},
        {0x3782, 0x02},
        {0x3787, 0x24},
        {0x3795, 0x24},
        {0x3796, 0x01},
        {0x3798, 0x40},
        {0x379c, 0x00},
        {0x379d, 0x00},
        {0x379e, 0x00},
        {0x379f, 0x01},
        {0x37a1, 0x10},
        {0x37a6, 0x00},
        {0x37ac, 0xa0},
        {0x37bb, 0x02},
        {0x37be, 0x0a},
        {0x37bf, 0x0a},
        {0x37c2, 0x04},
        {0x37c4, 0x11},
        {0x37c5, 0x80},
        {0x37c6, 0x14},
        {0x37c7, 0x08},
        {0x37c8, 0x42},
        {0x37cd, 0x17},
        {0x37ce, 0x04},
        {0x37d9, 0x08},
        {0x37dc, 0x01},
        {0x37e0, 0x30},
        {0x37e1, 0x10},
        {0x37e2, 0x14},
        {0x37e4, 0x28},
        {0x37ef, 0x00},
        {0x37f4, 0x00},
        {0x37f5, 0x00},
        {0x37f6, 0x00},
        {0x37f7, 0x00},
        {0x3800, 0x00},
        {0x3801, 0x00},
        {0x3802, 0x00},
        {0x3803, 0x00},
        {0x3804, 0x08},
        {0x3805, 0x0f},
        {0x3806, 0x08},
        {0x3807, 0x0f},
        {0x3808, 0x08},
        {0x3809, 0x00},
        {0x380a, 0x08},
        {0x380b, 0x00},
        {0x380c, 0x06},//hts = 0x650 = 1616
        {0x380d, 0x50},//
        {0x380e, 0x08},//vts = 0x874 = 2164
        {0x380f, 0x74},//
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
        {0x3822, 0x14},
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
        {0x3848, 0x08},
        {0x3849, 0x00},
        {0x384a, 0x08},
        {0x384b, 0x00},
        {0x384c, 0x00},
        {0x384d, 0x08},
        {0x384e, 0x00},
        {0x384f, 0x08},
        {0x3880, 0x16},
        {0x3881, 0x00},
        {0x3882, 0x08},
        {0x388a, 0x00},
        {0x389a, 0x00},
        {0x389b, 0x00},
        {0x389c, 0x00},
        {0x38a2, 0x02},
        {0x38a3, 0x02},
        {0x38a4, 0x02},
        {0x38a5, 0x02},
        {0x38a7, 0x04},
        {0x38ae, 0x1e},
        {0x38b8, 0x02},
        {0x38c3, 0x06},
        {0x3c80, 0x3f},
        {0x3c86, 0x01},
        {0x3c87, 0x02},
        {0x3ca0, 0x01},
        {0x3ca2, 0x0c},
        {0x3d8c, 0x71},
        {0x3d8d, 0xe2},
        {0x3f00, 0xcb},
        {0x3f04, 0x04},
        {0x3f07, 0x04},
        {0x3f09, 0x50},
        {0x3f9e, 0x07},
        {0x3f9f, 0x04},
        {0x4000, 0xf3},
        {0x4002, 0x00},
        {0x4003, 0x40},
        {0x4008, 0x00},
        {0x4009, 0x0f},
        {0x400a, 0x01},
        {0x400b, 0x78},
        {0x400f, 0x89},
        {0x4040, 0x00},
        {0x4041, 0x07},
        {0x4090, 0x14},
        {0x40b0, 0x00},
        {0x40b1, 0x00},
        {0x40b2, 0x00},
        {0x40b3, 0x00},
        {0x40b4, 0x00},
        {0x40b5, 0x00},
        {0x40b7, 0x00},
        {0x40b8, 0x00},
        {0x40b9, 0x00},
        {0x40ba, 0x00},
        {0x4300, 0xff},
        {0x4301, 0x00},
        {0x4302, 0x0f},
        {0x4303, 0x01},
        {0x4304, 0x01},
        {0x4305, 0x83},
        {0x4306, 0x21},
        {0x430d, 0x00},
        {0x4501, 0x00},
        {0x4505, 0xc4},
        {0x4506, 0x00},
        {0x4507, 0x60},
        {0x4508, 0x00},
        {0x4600, 0x00},
        {0x4601, 0x40},
        {0x4603, 0x03},
        {0x4800, 0x64},
        {0x4803, 0x00},
        {0x4809, 0x8e},
        {0x480e, 0x00},
        {0x4813, 0x00},
        {0x4814, 0x2a},
        {0x481b, 0x3c},
        {0x481f, 0x26},
        {0x4825, 0x32},
        {0x4829, 0x64},
        {0x4837, 0x11},
        {0x484b, 0x07},
        {0x4883, 0x36},
        {0x4885, 0x03},
        {0x488b, 0x00},
        {0x4d00, 0x04},
        {0x4d01, 0x99},
        {0x4d02, 0xbd},
        {0x4d03, 0xac},
        {0x4d04, 0xf2},
        {0x4d05, 0x54},
        {0x4e00, 0x2a},
        {0x4e0d, 0x00},
        {0x5000, 0xbb},
        {0x5001, 0x09},
        {0x5004, 0x00},
        {0x5005, 0x0e},
        {0x5036, 0x00},
        {0x5080, 0x04},
        {0x5082, 0x00},
        {0x5180, 0x70},
        {0x5181, 0x10},
        {0x5182, 0x71},
        {0x5183, 0xdf},
        {0x5184, 0x02},
        {0x5185, 0x6c},
        {0x5189, 0x48},
        {0x5324, 0x09},
        {0x5325, 0x11},
        {0x5326, 0x1f},
        {0x5327, 0x3b},
        {0x5328, 0x49},
        {0x5329, 0x61},
        {0x532a, 0x9c},
        {0x532b, 0xc9},
        {0x5335, 0x04},
        {0x5336, 0x00},
        {0x5337, 0x04},
        {0x5338, 0x00},
        {0x5339, 0x0b},
        {0x533a, 0x00},
        {0x53a4, 0x09},
        {0x53a5, 0x11},
        {0x53a6, 0x1f},
        {0x53a7, 0x3b},
        {0x53a8, 0x49},
        {0x53a9, 0x61},
        {0x53aa, 0x9c},
        {0x53ab, 0xc9},
        {0x53b5, 0x04},
        {0x53b6, 0x00},
        {0x53b7, 0x04},
        {0x53b8, 0x00},
        {0x53b9, 0x0b},
        {0x53ba, 0x00},
        {0x580b, 0x03},
        {0x580d, 0x00},
        {0x580f, 0x00},
        {0x5820, 0x00},
        {0x5821, 0x00},
        {0x5888, 0x01},
        {0x0100, 0x01},
        {SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list os04e10_init_regs_512_512_30fps_mipi[] = {
        {0x0103,0x01},
        {0x0301,0x44},
        {0x0303,0x02},
        {0x0304,0x00},
        {0x0305,0x4b},
        {0x0306,0x00},
        {0x0325,0x3b},
        {0x0327,0x04},
        {0x0328,0x05},
        {0x3002,0x21},
        {0x3016,0x32},
        {0x301b,0xf0},
        {0x301e,0xb4},
        {0x301f,0xd0},
        {0x3021,0x03},
        {0x3022,0x01},
        {0x3107,0xa1},
        {0x3108,0x7d},
        {0x3109,0xfc},
        {0x3500,0x00},
        {0x3501,0x02},
        {0x3502,0xb0},
        {0x3503,0x88},
        {0x3508,0x01},
        {0x3509,0x00},
        {0x350a,0x04},
        {0x350b,0x00},
        {0x350c,0x04},
        {0x350d,0x00},
        {0x350e,0x04},
        {0x350f,0x00},
        {0x3510,0x00},
        {0x3511,0x00},
        {0x3512,0x20},
        {0x3600,0x4c},
        {0x3601,0x08},
        {0x3610,0x87},
        {0x3611,0x24},
        {0x3614,0x4c},
        {0x3620,0x0c},
        {0x3621,0x04},
        {0x3632,0x80},
        {0x3633,0x00},
        {0x3660,0x00},
        {0x3662,0x08},
        {0x3664,0x70},
        {0x3665,0x00},
        {0x3666,0x00},
        {0x3667,0x00},
        {0x366a,0x14},
        {0x3670,0x0b},
        {0x3671,0x0b},
        {0x3672,0x0b},
        {0x3673,0x0b},
        {0x3674,0x00},
        {0x3678,0x2b},
        {0x3679,0x43},
        {0x3681,0xff},
        {0x3682,0x86},
        {0x3683,0x44},
        {0x3684,0x24},
        {0x3685,0x00},
        {0x368a,0x00},
        {0x368d,0x2b},
        {0x368e,0x6b},
        {0x3690,0x00},
        {0x3691,0x0b},
        {0x3692,0x0b},
        {0x3693,0x0b},
        {0x3694,0x0b},
        {0x3699,0x03},
        {0x369d,0x68},
        {0x369e,0x34},
        {0x369f,0x1b},
        {0x36a0,0x0f},
        {0x36a1,0x77},
        {0x36a2,0x00},
        {0x36a3,0x02},
        {0x36a4,0x02},
        {0x36b0,0x30},
        {0x36b1,0xf0},
        {0x36b2,0x00},
        {0x36b3,0x00},
        {0x36b4,0x00},
        {0x36b5,0x00},
        {0x36b6,0x00},
        {0x36b7,0x00},
        {0x36b8,0x00},
        {0x36b9,0x00},
        {0x36ba,0x00},
        {0x36bb,0x00},
        {0x36bc,0x00},
        {0x36bd,0x00},
        {0x36be,0x00},
        {0x36bf,0x00},
        {0x36c0,0x1f},
        {0x36c1,0x00},
        {0x36c2,0x00},
        {0x36c3,0x00},
        {0x36c4,0x00},
        {0x36c5,0x00},
        {0x36c6,0x00},
        {0x36c7,0x00},
        {0x36c8,0x00},
        {0x36c9,0x00},
        {0x36ca,0x0e},
        {0x36cb,0x0e},
        {0x36cc,0x0e},
        {0x36cd,0x0e},
        {0x36ce,0x0c},
        {0x36cf,0x0c},
        {0x36d0,0x0c},
        {0x36d1,0x0c},
        {0x36d2,0x00},
        {0x36d3,0x08},
        {0x36d4,0x10},
        {0x36d5,0x10},
        {0x36d6,0x00},
        {0x36d7,0x08},
        {0x36d8,0x10},
        {0x36d9,0x10},
        {0x3704,0x05},
        {0x3705,0x00},
        {0x3706,0x2b},
        {0x3709,0x49},
        {0x370a,0x00},
        {0x370b,0x60},
        {0x370e,0x0c},
        {0x370f,0x1c},
        {0x3710,0x00},
        {0x3713,0x00},
        {0x3714,0x28},
        {0x3716,0x24},
        {0x371a,0x1e},
        {0x3724,0x0d},
        {0x3725,0xb2},
        {0x372b,0x54},
        {0x3739,0x10},
        {0x373f,0xb0},
        {0x3740,0x2b},
        {0x3741,0x2b},
        {0x3742,0x2b},
        {0x3743,0x2b},
        {0x3744,0x60},
        {0x3745,0x60},
        {0x3746,0x60},
        {0x3747,0x60},
        {0x3748,0x00},
        {0x3749,0x00},
        {0x374a,0x00},
        {0x374b,0x00},
        {0x374c,0x00},
        {0x374d,0x00},
        {0x374e,0x00},
        {0x374f,0x00},
        {0x3756,0x00},
        {0x3757,0x0e},
        {0x3760,0x11},
        {0x3767,0x08},
        {0x3773,0x01},
        {0x3774,0x02},
        {0x3775,0x12},
        {0x3776,0x02},
        {0x377b,0x40},
        {0x377c,0x00},
        {0x377d,0x0c},
        {0x3782,0x02},
        {0x3787,0x24},
        {0x3795,0x24},
        {0x3796,0x01},
        {0x3798,0x40},
        {0x379c,0x00},
        {0x379d,0x00},
        {0x379e,0x00},
        {0x379f,0x01},
        {0x37a1,0x10},
        {0x37a6,0x77},
        {0x37ac,0xa0},
        {0x37bb,0x02},
        {0x37be,0x0a},
        {0x37bf,0x0a},
        {0x37c2,0x2c},
        {0x37c4,0x11},
        {0x37c5,0x80},
        {0x37c6,0x14},
        {0x37c7,0x08},
        {0x37c8,0x42},
        {0x37cd,0x17},
        {0x37ce,0x04},
        {0x37d9,0x04},
        {0x37dc,0x01},
        {0x37e0,0x30},
        {0x37e1,0x10},
        {0x37e2,0x14},
        {0x37e4,0x28},
        {0x37ef,0x00},
        {0x37f4,0x00},
        {0x37f5,0x00},
        {0x37f6,0x00},
        {0x37f7,0x00},
        {0x3800,0x00},
        {0x3801,0x00},
        {0x3802,0x00},
        {0x3803,0x00},
        {0x3804,0x08},
        {0x3805,0x0f},
        {0x3806,0x08},
        {0x3807,0x0f},
        {0x3808,0x02},
        {0x3809,0x00},
        {0x380a,0x02},
        {0x380b,0x00},
        {0x380c,0x05},//hts = 0x580 = 1408
        {0x380d,0x80},//
        {0x380e,0x09},//vts = 0x9b6 = 2486
        {0x380f,0xb6},//
        {0x3810,0x00},
        {0x3811,0x02},
        {0x3812,0x00},
        {0x3813,0x02},
        {0x3814,0x07},
        {0x3815,0x01},
        {0x3816,0x07},
        {0x3817,0x01},
        {0x3818,0x00},
        {0x3819,0x00},
        {0x381a,0x00},
        {0x381b,0x01},
        {0x3820,0x8c},
        {0x3821,0x01},
        {0x3822,0x14},
        {0x3823,0x08},
        {0x3824,0x00},
        {0x3825,0x20},
        {0x3826,0x00},
        {0x3827,0x08},
        {0x3829,0x03},
        {0x382a,0x00},
        {0x382b,0x00},
        {0x3832,0x08},
        {0x3838,0x00},
        {0x3839,0x00},
        {0x383a,0x00},
        {0x383b,0x00},
        {0x383d,0x01},
        {0x383e,0x00},
        {0x383f,0x00},
        {0x3843,0x00},
        {0x3848,0x08},
        {0x3849,0x00},
        {0x384a,0x08},
        {0x384b,0x00},
        {0x384c,0x00},
        {0x384d,0x08},
        {0x384e,0x00},
        {0x384f,0x08},
        {0x3880,0x16},
        {0x3881,0x00},
        {0x3882,0x08},
        {0x388a,0x03},
        {0x389a,0x00},
        {0x389b,0x00},
        {0x389c,0x00},
        {0x38a2,0x02},
        {0x38a3,0x02},
        {0x38a4,0x02},
        {0x38a5,0x02},
        {0x38a7,0x04},
        {0x38ae,0x1e},
        {0x38b8,0x02},
        {0x38c3,0x06},
        {0x3c80,0x3f},
        {0x3c86,0x01},
        {0x3c87,0x02},
        {0x3ca0,0x01},
        {0x3ca2,0x0c},
        {0x3d8c,0x71},
        {0x3d8d,0xe2},
        {0x3f00,0xcb},
        {0x3f04,0x04},
        {0x3f07,0x04},
        {0x3f09,0x50},
        {0x3f9e,0x07},
        {0x3f9f,0x04},
        {0x4000,0xf3},
        {0x4002,0x00},
        {0x4003,0x40},
        {0x4008,0x00},
        {0x4009,0x05},
        {0x400a,0x01},
        {0x400b,0x78},
        {0x400f,0x89},
        {0x4040,0x00},
        {0x4041,0x03},
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
        {0x4300,0xff},
        {0x4301,0x00},
        {0x4302,0x0f},
        {0x4303,0x01},
        {0x4304,0x01},
        {0x4305,0x83},
        {0x4306,0x21},
        {0x430d,0x00},
        {0x4501,0x00},
        {0x4505,0xc4},
        {0x4506,0x00},
        {0x4507,0x60},
        {0x4508,0x00},
        {0x4600,0x00},
        {0x4601,0x10},
        {0x4603,0x03},
        {0x4800,0x64},
        {0x4803,0x00},
        {0x4809,0x8e},
        {0x480e,0x00},
        {0x4813,0x00},
        {0x4814,0x2a},
        {0x481b,0x3c},
        {0x481f,0x26},
        {0x4825,0x32},
        {0x4829,0x64},
        {0x4837,0x11},
        {0x484b,0x07},
        {0x4883,0x36},
        {0x4885,0x03},
        {0x488b,0x00},
        {0x4d00,0x04},
        {0x4d01,0x99},
        {0x4d02,0xbd},
        {0x4d03,0xac},
        {0x4d04,0xf2},
        {0x4d05,0x54},
        {0x4e00,0x2a},
        {0x4e0d,0x00},
        {0x5000,0xbb},
        {0x5001,0x09},
        {0x5004,0x00},
        {0x5005,0x0e},
        {0x5036,0x00},
        {0x5080,0x04},
        {0x5082,0x00},
        {0x5180,0x70},
        {0x5181,0x10},
        {0x5182,0x71},
        {0x5183,0xdf},
        {0x5184,0x02},
        {0x5185,0x6c},
        {0x5189,0x48},
        {0x5324,0x09},
        {0x5325,0x11},
        {0x5326,0x1f},
        {0x5327,0x3b},
        {0x5328,0x49},
        {0x5329,0x61},
        {0x532a,0x9c},
        {0x532b,0xc9},
        {0x5335,0x04},
        {0x5336,0x00},
        {0x5337,0x04},
        {0x5338,0x00},
        {0x5339,0x0b},
        {0x533a,0x00},
        {0x53a4,0x09},
        {0x53a5,0x11},
        {0x53a6,0x1f},
        {0x53a7,0x3b},
        {0x53a8,0x49},
        {0x53a9,0x61},
        {0x53aa,0x9c},
        {0x53ab,0xc9},
        {0x53b5,0x04},
        {0x53b6,0x00},
        {0x53b7,0x04},
        {0x53b8,0x00},
        {0x53b9,0x0b},
        {0x53ba,0x00},
        {0x580b,0x03},
        {0x580d,0x00},
        {0x580f,0x00},
        {0x5820,0x00},
        {0x5821,0x00},
        {0x5888,0x01},
        {0x0100,0x01},
        {SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the os04e10_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting os04e10_win_sizes[] = {
        /* 2048*2048 [0] */
        {
                .width          = 2048,
                .height         = 2048,
                .fps            = 30 << 16 | 1,
                .mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
                .colorspace     = TISP_COLORSPACE_SRGB,
                .regs           = os04e10_init_regs_2048_2048_30fps_mipi,
        },
        /* 512*512 [1] */
        {
                .width          = 512,
                .height         = 512,
                .fps            = 30 << 16 | 1,
                .mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
                .colorspace     = TISP_COLORSPACE_SRGB,
                .regs           = os04e10_init_regs_512_512_30fps_mipi,
        },
};

static struct tx_isp_sensor_win_setting *wsize = &os04e10_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list os04e10_stream_on_mipi[] = {
        {0x0100, 0x01},
        { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list os04e10_stream_off_mipi[] = {
        {0x0100, 0x00},
        { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int os04e10_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int os04e10_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int os04e10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        unsigned char val;
        while (vals->reg_num != OS04E10_REG_END) {
                if (vals->reg_num == OS04E10_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = os04e10_read(sd, vals->reg_num, &val);
                        /* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}
#endif

static int os04e10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        while (vals->reg_num != OS04E10_REG_END) {
                if (vals->reg_num == OS04E10_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = os04e10_write(sd, vals->reg_num, vals->value);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int os04e10_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int os04e10_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int os04e10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        unsigned char val;
        while (vals->reg_num != SENSOR_REG_END) {
                if (vals->reg_num == SENSOR_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = os04e10_read(sd, vals->reg_num, &val);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }
        return 0;
}
#endif

static int os04e10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        while (vals->reg_num != SENSOR_REG_END) {
                if (vals->reg_num == SENSOR_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = os04e10_write(sd, vals->reg_num, vals->value);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int os04e10_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int os04e10_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int os04e10_setting_select(struct tx_isp_subdev *sd, int deboot)
{
        int ret = ISP_SUCCESS;

        switch (deboot) {
        case 0:
                wsize = &os04e10_win_sizes[0];
                os04e10_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
                os04e10_attr.max_dgain = 0;
                os04e10_attr.max_again = 259142;
                os04e10_attr.min_integration_time = 2;
                os04e10_attr.max_integration_time = 2164 - 8;
                os04e10_attr.total_width = 1616;
                os04e10_attr.total_height = 2164;
                os04e10_attr.integration_time_apply_delay = 2;
                os04e10_attr.again_apply_delay = 2;
                os04e10_attr.dgain_apply_delay = 0;
                os04e10_attr.integration_time_limit = os04e10_attr.max_integration_time;
                os04e10_attr.max_integration_time_native = os04e10_attr.max_integration_time;
                os04e10_attr.min_integration_time_native = os04e10_attr.min_integration_time;
                os04e10_attr.expo_fs = 1;
#ifdef SENSOR_WDR_2_FRAME
                os04e10_attr.max_again_short = xxxx;
                os04e10_attr.min_integration_time_short = xx;
                os04e10_attr.max_integration_time_short = xx;
                os04e10_attr.wdr_cache = wdr_line * os04e10_attr.total_width;
#endif /* SENSOR_WDR_2_FRAME */
                memcpy((void *)(&(os04e10_attr.mipi)), (void *)(&os04e10_mipi_linear), sizeof(os04e10_attr.mipi));
                break;
        case 1:
                wsize = &os04e10_win_sizes[1];
                os04e10_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
                os04e10_attr.max_dgain = 0;
                os04e10_attr.max_again = 259142;
                os04e10_attr.min_integration_time = 2;
                os04e10_attr.max_integration_time = 2486 - 8;
                os04e10_attr.total_width = 1408;
                os04e10_attr.total_height = 2486;
                os04e10_attr.integration_time_apply_delay = 2;
                os04e10_attr.again_apply_delay = 2;
                os04e10_attr.dgain_apply_delay = 0;
                os04e10_attr.integration_time_limit = os04e10_attr.max_integration_time;
                os04e10_attr.max_integration_time_native = os04e10_attr.max_integration_time;
                os04e10_attr.min_integration_time_native = os04e10_attr.min_integration_time;
                os04e10_attr.expo_fs = 1;
#ifdef SENSOR_WDR_2_FRAME
                os04e10_attr.max_again_short = xxxx;
                os04e10_attr.min_integration_time_short = xx;
                os04e10_attr.max_integration_time_short = xx;
                os04e10_attr.wdr_cache = wdr_line * os04e10_attr.total_width;
#endif /* SENSOR_WDR_2_FRAME */
                memcpy((void *)(&(os04e10_attr.mipi)), (void *)(&os04e10_mipi_512_linear), sizeof(os04e10_attr.mipi));
                break;
        default:
                ISP_ERROR("Have no this Setting Source!!!\n");
        }

        return ret;
}

static int os04e10_attr_check(struct tx_isp_subdev *sd)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        struct tx_isp_sensor_register_info *info = &sensor->info;
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        struct clk *sclka;
        int ret = ISP_SUCCESS;

        os04e10_setting_select(sd, info->default_boot);

        switch (info->video_interface) {
        case TISP_SENSOR_VI_MIPI_CSI0:
        case TISP_SENSOR_VI_MIPI_CSI1:
                os04e10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
                os04e10_attr.mipi.index = 0;
                break;
        case TISP_SENSOR_VI_DVP:
                os04e10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

        ret = os04e10_clk_set(sd, sclka, SENSOR_MCLK);
        if (ret) {
                ISP_ERROR("MCLK configuration failed!!!\n");
        }

        os04e10_attr_set(sd, wsize);
        sensor->priv = wsize;

        return 0;

err_get_mclk:
        return -1;
}

static int os04e10_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
        unsigned char v;
        int ret;

        ret = os04e10_read(sd, 0x300a, &v);
        pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        if (ret < 0)
                return ret;
        if (v != SENSOR_CHIP_ID_H)
                return -ENODEV;
        *ident = v;

        ret = os04e10_read(sd, 0x300b, &v);
        pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        if (ret < 0)
                return ret;
        if (v != SENSOR_CHIP_ID_M)
                return -ENODEV;
        *ident = (*ident << 8) | v;

        ret = os04e10_read(sd, 0x300c, &v);
        pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        if (ret < 0)
                return ret;
        if (v != SENSOR_CHIP_ID_L)
                return -ENODEV;
        *ident = (*ident << 8) | v;

        return 0;
}

static int os04e10_g_chip_ident(struct tx_isp_subdev *sd,
                                 struct tx_isp_chip_ident *chip)
{
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        struct tx_isp_sensor_register_info *info = &sensor->info;
        unsigned int ident = 0;
        int ret = ISP_SUCCESS;

        os04e10_attr_check(sd);
        if (info->rst_gpio != -1) {
                ret = private_gpio_request(info->rst_gpio, "os04e10_reset");
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
                ret = private_gpio_request(info->pwdn_gpio, "os04e10_pwdn");
                if (!ret) {
                        private_gpio_direction_output(info->pwdn_gpio, 0);
                        private_msleep(5);
                        private_gpio_direction_output(info->pwdn_gpio, 1);
                        private_msleep(5);
                } else {
                        ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
                }
        }
        ret = os04e10_detect(sd, &ident);
        if (ret) {
                ISP_ERROR("chip found @ 0x%x (%s) is not an os04e10 chip.\n",
                          client->addr, client->adapter->name);
                return ret;
        }

        ISP_WARNING("===================================================\n");
        ISP_WARNING("Template version is %s\n", TVERSION);
        ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
        ISP_WARNING("Sensor name is %s\n", os04e10_attr.name);
        ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
        ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
        ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
                    info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
        ISP_WARNING("===================================================\n");

        if (chip) {
                memcpy(chip->name, "os04e10", sizeof("os04e10"));
                chip->ident = ident;
                chip->revision = SENSOR_VERSION;
        }

        return 0;
}

static int os04e10_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        return 0;
}

static int os04e10_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = ISP_SUCCESS;

        if (!init->enable) {
                sensor->video.state = TX_ISP_MODULE_DEINIT;
                return ISP_SUCCESS;
        } else {
                ret = os04e10_attr_set(sd, wsize);
                sensor->video.state = TX_ISP_MODULE_DEINIT;
        }

        return ret;
}

static int os04e10_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
        ret = os04e10_read(sd, reg->reg & 0xffff, &val);
        reg->val = val;
        reg->size = 2;

        return ret;
}

static int os04e10_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
        int len = 0;

        len = strlen(sd->chip.name);
        if (len && strncmp(sd->chip.name, reg->name, len)) {
                return -EINVAL;
        }
        if (!private_capable(CAP_SYS_ADMIN))
                return -EPERM;
        os04e10_write(sd, reg->reg & 0xffff, reg->val & 0xff);

        return 0;
}

static int os04e10_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = ISP_SUCCESS;

        if (init->enable) {
                if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
                        ret = os04e10_write_array(sd, wsize->regs);
                        if (ret)
                                return ret;
                        sensor->video.state = TX_ISP_MODULE_INIT;
                }
                if (sensor->video.state == TX_ISP_MODULE_INIT) {
                        ret = os04e10_write_array(sd, os04e10_stream_on_mipi);
                        sensor->video.state = TX_ISP_MODULE_RUNNING;
                        pr_debug("os04e10 stream on\n");
                }

        } else {
                ret = os04e10_write_array(sd, os04e10_stream_off_mipi);
                sensor->video.state = TX_ISP_MODULE_INIT;
                pr_debug("os04e10 stream off\n");
        }

        return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int os04e10_set_expo(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret += os04e10_write(sd, 0x3501, (unsigned char)((it >> 8) & 0xff));
	ret += os04e10_write(sd, 0x3502, (unsigned char)(it & 0xff));

	ret += os04e10_write(sd, 0x3508, (unsigned char)((again >> 8) & 0xff));
	ret += os04e10_write(sd, 0x3509, (unsigned char)(again & 0xff));

        return ret;
}
#else
static int os04e10_set_integration_time(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;

	ret += os04e10_write(sd, 0x3501, (unsigned char)((value >> 8) & 0xff));
	ret += os04e10_write(sd, 0x3502, (unsigned char)(value & 0xff));

        return ret;
}

static int os04e10_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;

	ret += os04e10_write(sd, 0x3508, (unsigned char)((value >> 8) & 0xff));
	ret += os04e10_write(sd, 0x3509, (unsigned char)(value & 0xff));

        return ret;
}
#endif /* SENSOR_EXPO */

static int os04e10_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
        return 0;
}

static int os04e10_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
        return 0;
}

static int os04e10_set_mode(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;

        if (wsize) {
                ret = os04e10_attr_set(sd, wsize);
        }

        return ret;
}

static int os04e10_set_fps(struct tx_isp_subdev *sd, int fps)
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
                sclk = 1616 * 2164 * 30;  /**< HTS * VTS * FPS */
                max_fps = 30;
                break;
        case 1:
                sclk = 1408 * 2486 * 30;  /**< HTS * VTS * FPS */
                max_fps = 30;
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
        ret += os04e10_read(sd, 0x380c, &val);
        hts = val << 8;
        val = 0;
        ret += os04e10_read(sd, 0x380d, &val);
        hts = (hts | val);
        if (0 != ret) {
                ISP_ERROR("err: os04e10 read err\n");
                return ret;
        }

        vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

        os04e10_write(sd, 0x380f, (unsigned char) (vts & 0xff));
        os04e10_write(sd, 0x380e, (unsigned char) (vts >> 8));

        if (0 != ret) {
                ISP_ERROR("err: os04e10_write err\n");
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
static int os04e10_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = ISP_SUCCESS;
        uint8_t val_3716 = 0;
        uint8_t val_3820 = 0;

        os04e10_write(sd, 0x0100, 0x00);
        os04e10_read(sd, 0x3716, &val_3716);
        os04e10_read(sd, 0x3820, &val_3820);
	switch(enable) {
	case 0:
                val_3716 =0x24;
                val_3820 =0x88;
		break;
	case 1:
                val_3716 =0x28;
                val_3820 =0x80;
		break;
	case 2:
                val_3716 =0x04;
                val_3820 =0xb8;
		break;
	case 3:
                val_3716 =0x04;
                val_3820 =0xb0;
		break;
	}
        os04e10_write(sd, 0x3716, val_3716);
        os04e10_write(sd, 0x3820, val_3820);
        os04e10_write(sd, 0x0100, 0x01);

	return ret;
}
#endif /* SENSOR_MIR_FLIP */

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int os04e10_set_expo_short(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;


        return ret;
}
#else
static int os04e10_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;


        return ret;
}

static int os04e10_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;


        return ret;
}
#endif /* SENSOR_EXPO */

static int os04e10_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
        struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
        int ret = ISP_SUCCESS;

        ret = os04e10_write_array(sd, os04e10_stream_off_mipi);
        if (wdr_en == 1) {
                os04e10_setting_select(sd, 1);
                os04e10_attr_set(sd, wsize);
        } else if (wdr_en == 0) {
                os04e10_setting_select(sd, 0);
                os04e10_attr_set(sd, wsize);
        } else {
                ISP_ERROR("Can not support this data type!!!");
                return -1;
        }

        return 0;
}

static int os04e10_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        struct tx_isp_sensor_register_info *info = &sensor->info;
        int ret = ISP_SUCCESS;

        private_gpio_direction_output(info->rst_gpio, 0);
        private_msleep(1);
        private_gpio_direction_output(info->rst_gpio, 1);
        private_msleep(1);

        ret = os04e10_write_array(sd, wsize->regs);
        ret = os04e10_write_array(sd, os04e10_stream_on_mipi);

        return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int os04e10_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
                        ret = os04e10_set_expo(sd, sensor_val->value);
                break;
#else
        case TX_ISP_EVENT_SENSOR_INT_TIME:
                if (arg)
                        ret = os04e10_set_integration_time(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_AGAIN:
                if (arg)
                        ret = os04e10_set_analog_gain(sd, sensor_val->value);
                break;
#endif /* SENSOR_EXPO */
        case TX_ISP_EVENT_SENSOR_DGAIN:
                if (arg)
                        ret = os04e10_set_digital_gain(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
                if (arg)
                        ret = os04e10_get_black_pedestal(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_RESIZE:
                if (arg)
                        ret = os04e10_set_mode(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
                if (arg)
                        ret = os04e10_write_array(sd, os04e10_stream_off_mipi);
                break;
        case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
                if (arg)
                        ret = os04e10_write_array(sd, os04e10_stream_on_mipi);
                break;
        case TX_ISP_EVENT_SENSOR_FPS:
                if (arg)
                        ret = os04e10_set_fps(sd, sensor_val->value);
                break;
#ifdef SENSOR_MIR_FLIP
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = os04e10_set_vflip(sd, sensor_val->value);
		break;
#endif /* SENSOR_MIR_FLIP */
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
        case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
                if (arg)
                        ret = os04e10_set_expo_short(sd, sensor_val->value);
                break;
#else
        case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
                if(arg)
                        ret = os04e10_set_integration_time_short(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
                if(arg)
                        ret = os04e10_set_analog_gain_short(sd, sensor_val->value);
                break;
#endif /* SENSOR_EXPO */
        case TX_ISP_EVENT_SENSOR_WDR:
                if(arg)
                        ret = os04e10_set_wdr(sd, init->enable);
                break;
        case TX_ISP_EVENT_SENSOR_WDR_STOP:
                if(arg)
                        ret = os04e10_set_wdr_stop(sd, init->enable);
                break;
#endif /* SENSOR_WDR_2_FRAME */
        default:
                break;
        }

        return 0;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops os04e10_core_ops = {
        .g_chip_ident = os04e10_g_chip_ident,
        .reset = os04e10_reset,
        .init = os04e10_init,
        .g_register = os04e10_g_register,
        .s_register = os04e10_s_register,
};

static struct tx_isp_subdev_video_ops os04e10_video_ops = {
        .s_stream = os04e10_s_stream,
};

static struct tx_isp_subdev_sensor_ops os04e10_sensor_ops = {
#ifndef SENSOR_TEST
        .ioctl  = os04e10_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops os04e10_ops = {
        .core = &os04e10_core_ops,
        .video = &os04e10_video_ops,
        .sensor = &os04e10_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
        .name = "os04e10",
        .id = -1,
        .dev = {
                .dma_mask = &tx_isp_module_dma_mask,
                .coherent_dma_mask = 0xffffffff,
                .platform_data = NULL,
        },
        .num_resources = 0,
};

static int os04e10_probe(struct i2c_client *client,
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

        sensor->video.attr = &os04e10_attr;
        sensor->dev = &client->dev;
        tx_isp_subdev_init(&sensor_platform_device, sd, &os04e10_ops);
        tx_isp_set_subdevdata(sd, client);
        tx_isp_set_subdev_hostdata(sd, sensor);
        private_i2c_set_clientdata(client, sd);

        pr_debug("probe ok ------->os04e10\n");

        return 0;
}

static int os04e10_remove(struct i2c_client *client)
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

static const struct i2c_device_id os04e10_id[] = {
        {"os04e10", 0},
        {}
};
MODULE_DEVICE_TABLE(i2c, os04e10_id
                   );

static struct i2c_driver os04e10_driver = {
        .driver = {
                .owner  = THIS_MODULE,
                .name   = "os04e10",
        },
        .probe          = os04e10_probe,
        .remove         = os04e10_remove,
        .id_table       = os04e10_id,
};

static __init int init_os04e10(void) {
        return private_i2c_add_driver(&os04e10_driver);
}

static __exit void exit_os04e10(void) {
        private_i2c_del_driver(&os04e10_driver);
}

module_init(init_os04e10);
module_exit(exit_os04e10);

MODULE_DESCRIPTION("A low-level driver for Sony os04e10 sensor");
MODULE_LICENSE("GPL");
