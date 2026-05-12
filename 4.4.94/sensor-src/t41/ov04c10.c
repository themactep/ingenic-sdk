/*
 * ov04c10.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          2688*1520       30        mipi_2lane             linear
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
#define SENSOR_VERSION  "H20240222a"

//#define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */

#define SENSOR_CHIP_ID_H    (0x53)
#define SENSOR_CHIP_ID_M    (0x04)
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

struct tx_isp_sensor_attribute ov04c10_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
        unsigned int value;
        unsigned int gain;
};

struct again_lut ov04c10_again_lut[] = {
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

unsigned int ov04c10_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
        /* Analog gain table */
        struct again_lut *lut = ov04c10_again_lut;
        while (lut->gain <= ov04c10_attr.max_again) {
                if (isp_gain == 0) {
                        *sensor_again = lut->value;
                        return 0;
                } else if (isp_gain < lut->gain) {
                        *sensor_again = (lut - 1)->value;
                        return (lut - 1)->gain;
                } else {
                        if ((lut->gain == ov04c10_attr.max_again) && (isp_gain >= lut->gain)) {
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
unsigned int ov04c10_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
        /* Analog gain table */
        struct again_lut *lut = ov04c10_again_lut;
        while(lut->gain <= ov04c10_attr.max_again_short) {
                if(isp_gain == 0) {
                        *sensor_again = lut->value;
                        return 0;
                }
                else if(isp_gain < lut->gain) {
                        *sensor_again = (lut - 1)->value;
                        return (lut - 1)->gain;
                }
                else{
                        if((lut->gain == ov04c10_attr.max_again_short) && (isp_gain >= lut->gain)) {
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

unsigned int ov04c10_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
        return 0;
}

struct tx_isp_mipi_bus ov04c10_mipi_linear = {
        .mode = SENSOR_MIPI_OTHER_MODE,
        .clk = 880,
        .lans = 2,
        .image_twidth = 2688,
        .image_theight = 1520,
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

struct tx_isp_dvp_bus ov04c10_dvp = {
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

struct tx_isp_sensor_attribute ov04c10_attr = {
        .name = "ov04c10",
        .chip_id = 0x530443,
        .cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
        .cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
        .cbus_device = 0x36,
        .sensor_ctrl.alloc_again = ov04c10_alloc_again,
        .sensor_ctrl.alloc_dgain = ov04c10_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
        .sensor_ctrl.alloc_again_short = ov04c10_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */
};

static struct regval_list ov04c10_init_regs_2688_1520_30fps_mipi[] = {
        {0x0103,0x01},
        {0x0301,0xe4},
        {0x0303,0x01},
        {0x0305,0x6e},
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
        {0x3501,0x00},
        {0x3502,0x40},
        {0x3503,0x88},
        {0x3508,0x07},
        {0x3509,0xc0},
        {0x350a,0x04},
        {0x350b,0x00},
        {0x350c,0x07},
        {0x350d,0xc0},
        {0x350e,0x04},
        {0x350f,0x00},
        {0x3510,0x00},
        {0x3511,0x00},
        {0x3512,0x20},
        {0x3624,0x00},
        {0x3625,0x4c},
        {0x3660,0x00},
        {0x3666,0xa5},
        {0x3667,0xa5},
        {0x366a,0x60},
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
        {0x370a,0x02},
        {0x370e,0x0c},
        {0x3710,0x00},
        {0x3713,0x00},
        {0x3725,0x02},
        {0x372a,0x03},
        {0x3738,0xce},
        {0x3748,0x02},
        {0x374a,0x02},
        {0x374c,0x02},
        {0x374e,0x02},
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
        {0x4090,0x04},
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
        {0x3022,0x61},
        {0x3109,0xe7},
        {0x3600,0x00},
        {0x3610,0x95},
        {0x3611,0x85},
        {0x3613,0x3a},
        {0x3615,0x60},
        {0x3621,0xb0},
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
        {0x3706,0x9d},
        {0x3709,0x4a},
        {0x370b,0x48},
        {0x370c,0x01},
        {0x370f,0x04},
        {0x3714,0x24},
        {0x3716,0x24},
        {0x3719,0x11},
        {0x371a,0x1e},
        {0x3720,0x00},
        {0x3724,0x13},
        {0x373f,0xb0},
        {0x3741,0x9d},
        {0x3743,0x9d},
        {0x3745,0x9d},
        {0x3747,0x9d},
        {0x3749,0x48},
        {0x374b,0x48},
        {0x374d,0x48},
        {0x374f,0x48},
        {0x3755,0x10},
        {0x376c,0x00},
        {0x378d,0x3c},
        {0x3790,0x01},
        {0x3791,0x01},
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
        {0x380c,0x08},//hts = 0x85c = 2140
        {0x380d,0x5c},//
        {0x380e,0x06},//vts = 0x626 = 1574
        {0x380f,0x26},//
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
        {0x3cad,0x00},
        {0x3cae,0x00},
        {0x4000,0xf3},
        {0x4001,0x60},
        {0x4003,0x80},
        {0x4300,0xff},
        {0x4302,0x0f},
        {0x4305,0x83},
        {0x4505,0x84},
        {0x4809,0x1e},
        {0x480a,0x04},
        {0x4837,0x12},
        {0x4c00,0x08},
        {0x4c01,0x00},
        {0x4c04,0x00},
        {0x4c05,0x00},
        {0x5000,0xf9},
        {0x3c8c,0x10},
        {0x3822,0x14},
        {0x0100,0x01},
        {SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the ov04c10_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting ov04c10_win_sizes[] = {
        /* 2560*1440 [0] */
        {
                .width          = 2688,
                .height         = 1520,
                .fps            = 30 << 16 | 1,
                .mbus_code      = TISP_VI_FMT_SBGGR12_1X12,
                .colorspace     = TISP_COLORSPACE_SRGB,
                .regs           = ov04c10_init_regs_2688_1520_30fps_mipi,
        },
};

static struct tx_isp_sensor_win_setting *wsize = &ov04c10_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list ov04c10_stream_on_mipi[] = {
        {0x0100, 0x01},
        { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list ov04c10_stream_off_mipi[] = {
        {0x0100, 0x00},
        { SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int ov04c10_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int ov04c10_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int ov04c10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        unsigned char val;
        while (vals->reg_num != OV04C10_REG_END) {
                if (vals->reg_num == OV04C10_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = ov04c10_read(sd, vals->reg_num, &val);
                        /* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}
#endif

static int ov04c10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        while (vals->reg_num != OV04C10_REG_END) {
                if (vals->reg_num == OV04C10_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = ov04c10_write(sd, vals->reg_num, vals->value);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int ov04c10_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int ov04c10_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int ov04c10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        unsigned char val;
        while (vals->reg_num != SENSOR_REG_END) {
                if (vals->reg_num == SENSOR_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = ov04c10_read(sd, vals->reg_num, &val);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }
        return 0;
}
#endif

static int ov04c10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
        int ret;
        while (vals->reg_num != SENSOR_REG_END) {
                if (vals->reg_num == SENSOR_REG_DELAY) {
                        private_msleep(vals->value);
                } else {
                        ret = ov04c10_write(sd, vals->reg_num, vals->value);
                        if (ret < 0)
                                return ret;
                }
                vals++;
        }

        return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int ov04c10_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int ov04c10_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int ov04c10_setting_select(struct tx_isp_subdev *sd, int deboot)
{
        int ret = ISP_SUCCESS;

        switch (deboot) {
        case 0:
                wsize = &ov04c10_win_sizes[0];
                ov04c10_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
                ov04c10_attr.max_dgain = 0;
                ov04c10_attr.max_again = 259142;
                ov04c10_attr.min_integration_time = 2;
                ov04c10_attr.max_integration_time = 1574 - 8;
                ov04c10_attr.total_width = 4280; /* 2140 * 2 */
                ov04c10_attr.total_height = 1574;
                ov04c10_attr.integration_time_apply_delay = 2;
                ov04c10_attr.again_apply_delay = 2;
                ov04c10_attr.dgain_apply_delay = 0;
                ov04c10_attr.integration_time_limit = ov04c10_attr.max_integration_time;
                ov04c10_attr.max_integration_time_native = ov04c10_attr.max_integration_time;
                ov04c10_attr.min_integration_time_native = ov04c10_attr.min_integration_time;
                ov04c10_attr.expo_fs = 1;
#ifdef SENSOR_WDR_2_FRAME
                ov04c10_attr.max_again_short = xxxx;
                ov04c10_attr.min_integration_time_short = xx;
                ov04c10_attr.max_integration_time_short = xx;
                ov04c10_attr.wdr_cache = wdr_line * ov04c10_attr.total_width;
#endif /* SENSOR_WDR_2_FRAME */
                memcpy((void *)(&(ov04c10_attr.mipi)), (void *)(&ov04c10_mipi_linear), sizeof(ov04c10_attr.mipi));
                break;
        default:
                ISP_ERROR("Have no this Setting Source!!!\n");
        }

        return ret;
}

static int ov04c10_attr_check(struct tx_isp_subdev *sd)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        struct tx_isp_sensor_register_info *info = &sensor->info;
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        struct clk *sclka;
        int ret = ISP_SUCCESS;

        ov04c10_setting_select(sd, info->default_boot);

        switch (info->video_interface) {
        case TISP_SENSOR_VI_MIPI_CSI0:
        case TISP_SENSOR_VI_MIPI_CSI1:
                ov04c10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
                ov04c10_attr.mipi.index = 0;
                break;
        case TISP_SENSOR_VI_DVP:
                ov04c10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

        ret = ov04c10_clk_set(sd, sclka, SENSOR_MCLK);
        if (ret) {
                ISP_ERROR("MCLK configuration failed!!!\n");
        }

        ov04c10_attr_set(sd, wsize);
        sensor->priv = wsize;

        return 0;

err_get_mclk:
        return -1;
}

static int ov04c10_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
        unsigned char v;
        int ret;

        ret = ov04c10_read(sd, 0x300a, &v);
        pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        if (ret < 0)
                return ret;
        if (v != SENSOR_CHIP_ID_H)
                return -ENODEV;
        *ident = v;

        ret = ov04c10_read(sd, 0x300b, &v);
        pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        if (ret < 0)
                return ret;
        if (v != SENSOR_CHIP_ID_M)
                return -ENODEV;
        *ident = v;

        ret = ov04c10_read(sd, 0x300c, &v);
        pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
        if (ret < 0)
                return ret;
        if (v != SENSOR_CHIP_ID_L)
                return -ENODEV;
        *ident = (*ident << 8) | v;

        return 0;
}

static int ov04c10_g_chip_ident(struct tx_isp_subdev *sd,
                                 struct tx_isp_chip_ident *chip)
{
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        struct tx_isp_sensor_register_info *info = &sensor->info;
        unsigned int ident = 0;
        int ret = ISP_SUCCESS;

        ov04c10_attr_check(sd);
        if (info->rst_gpio != -1) {
                ret = private_gpio_request(info->rst_gpio, "ov04c10_reset");
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
                ret = private_gpio_request(info->pwdn_gpio, "ov04c10_pwdn");
                if (!ret) {
                        private_gpio_direction_output(info->pwdn_gpio, 0);
                        private_msleep(5);
                        private_gpio_direction_output(info->pwdn_gpio, 1);
                        private_msleep(5);
                } else {
                        ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
                }
        }
        ret = ov04c10_detect(sd, &ident);
        if (ret) {
                ISP_ERROR("chip found @ 0x%x (%s) is not an ov04c10 chip.\n",
                          client->addr, client->adapter->name);
                return ret;
        }

        ISP_WARNING("===================================================\n");
        ISP_WARNING("Template version is %s\n", TVERSION);
        ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
        ISP_WARNING("Sensor name is %s\n", ov04c10_attr.name);
        ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
        ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
        ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
                    info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
        ISP_WARNING("===================================================\n");

        if (chip) {
                memcpy(chip->name, "ov04c10", sizeof("ov04c10"));
                chip->ident = ident;
                chip->revision = SENSOR_VERSION;
        }

        return 0;
}

static int ov04c10_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        return 0;
}

static int ov04c10_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = ISP_SUCCESS;

        if (!init->enable) {
                sensor->video.state = TX_ISP_MODULE_DEINIT;
                return ISP_SUCCESS;
        } else {
                ret = ov04c10_attr_set(sd, wsize);
                sensor->video.state = TX_ISP_MODULE_DEINIT;
        }

        return ret;
}

static int ov04c10_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
        ret = ov04c10_read(sd, reg->reg & 0xffff, &val);
        reg->val = val;
        reg->size = 2;

        return ret;
}

static int ov04c10_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
        int len = 0;

        len = strlen(sd->chip.name);
        if (len && strncmp(sd->chip.name, reg->name, len)) {
                return -EINVAL;
        }
        if (!private_capable(CAP_SYS_ADMIN))
                return -EPERM;
        ov04c10_write(sd, reg->reg & 0xffff, reg->val & 0xff);

        return 0;
}

static int ov04c10_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        int ret = ISP_SUCCESS;

        if (init->enable) {
                if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
                        ret = ov04c10_write_array(sd, wsize->regs);
                        if (ret)
                                return ret;
                        sensor->video.state = TX_ISP_MODULE_INIT;
                }
                if (sensor->video.state == TX_ISP_MODULE_INIT) {
                        ret = ov04c10_write_array(sd, ov04c10_stream_on_mipi);
                        sensor->video.state = TX_ISP_MODULE_RUNNING;
                        pr_debug("ov04c10 stream on\n");
                }

        } else {
                ret = ov04c10_write_array(sd, ov04c10_stream_off_mipi);
                sensor->video.state = TX_ISP_MODULE_INIT;
                pr_debug("ov04c10 stream off\n");
        }

        return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int ov04c10_set_expo(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret += ov04c10_write(sd, 0x3501, (unsigned char)((it >> 8) & 0xff));
	ret += ov04c10_write(sd, 0x3502, (unsigned char)(it & 0xff));

	ret += ov04c10_write(sd, 0x3508, (unsigned char)((again >> 8) & 0xff));
	ret += ov04c10_write(sd, 0x3509, (unsigned char)(again & 0xff));

        return ret;
}
#else
static int ov04c10_set_integration_time(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;

	ret += ov04c10_write(sd, 0x3501, (unsigned char)((value >> 8) & 0xff));
	ret += ov04c10_write(sd, 0x3502, (unsigned char)(value & 0xff));

        return ret;
}

static int ov04c10_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;

	ret += ov04c10_write(sd, 0x3508, (unsigned char)((value >> 8) & 0xff));
	ret += ov04c10_write(sd, 0x3509, (unsigned char)(value & 0xff));

        return ret;
}
#endif /* SENSOR_EXPO */

static int ov04c10_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
        return 0;
}

static int ov04c10_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
        return 0;
}

static int ov04c10_set_mode(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;

        if (wsize) {
                ret = ov04c10_attr_set(sd, wsize);
        }

        return ret;
}

static int ov04c10_set_fps(struct tx_isp_subdev *sd, int fps)
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
                sclk = 2140 * 1574 * 30 * 2;  /**< HTS * VTS * FPS */
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
        ret += ov04c10_read(sd, 0x380c, &val);
        hts = val;
        val = 0;
        ret += ov04c10_read(sd, 0x380d, &val);
        hts = (((hts << 8) | val) << 1);
        if (0 != ret) {
                ISP_ERROR("err: ov04c10 read err\n");
                return ret;
        }

        vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

        ov04c10_write(sd, 0x380f, (unsigned char) (vts & 0xff));
        ov04c10_write(sd, 0x380e, (unsigned char) (vts >> 8));

        if (0 != ret) {
                ISP_ERROR("err: ov04c10_write err\n");
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
static int ov04c10_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = ISP_SUCCESS;
        uint8_t val_foramt = 0;
        uint8_t val_reg16 = 0;

        printk("[%s,%d] -> flip = %d\n", __func__, __LINE__, enable);
        /* ov04c10_read(sd, 0x3820, &val_foramt); */
        /* ov04c10_read(sd, 0x3716, &val_reg16); */
	switch(enable) {
	case 0:
                val_foramt = 0x88;
                val_reg16 = 0x24;
		break;
	case 1:
                val_foramt = 0x80;
                val_reg16 = 0x24;
		break;
	case 2:
                val_foramt = 0xB0;
                val_reg16 = 0x04;
		break;
	case 3:
                val_foramt = 0xB8;
                val_reg16 = 0x04;
		break;
	}
        ov04c10_write(sd, 0x3820, val_foramt);
        ov04c10_write(sd, 0x3716, val_reg16);

	return ret;
}
#endif /* SENSOR_MIR_FLIP */

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int ov04c10_set_expo_short(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;


        return ret;
}
#else
static int ov04c10_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;


        return ret;
}

static int ov04c10_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
        int ret = ISP_SUCCESS;


        return ret;
}
#endif /* SENSOR_EXPO */

static int ov04c10_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
        struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
        int ret = ISP_SUCCESS;

        ret = ov04c10_write_array(sd, ov04c10_stream_off_mipi);
        if (wdr_en == 1) {
                ov04c10_setting_select(sd, 1);
                ov04c10_attr_set(sd, wsize);
        } else if (wdr_en == 0) {
                ov04c10_setting_select(sd, 0);
                ov04c10_attr_set(sd, wsize);
        } else {
                ISP_ERROR("Can not support this data type!!!");
                return -1;
        }

        return 0;
}

static int ov04c10_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
        struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
        struct tx_isp_sensor_register_info *info = &sensor->info;
        int ret = ISP_SUCCESS;

        private_gpio_direction_output(info->rst_gpio, 0);
        private_msleep(1);
        private_gpio_direction_output(info->rst_gpio, 1);
        private_msleep(1);

        ret = ov04c10_write_array(sd, wsize->regs);
        ret = ov04c10_write_array(sd, ov04c10_stream_on_mipi);

        return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int ov04c10_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
                        ret = ov04c10_set_expo(sd, sensor_val->value);
                break;
#else
        case TX_ISP_EVENT_SENSOR_INT_TIME:
                if (arg)
                        ret = ov04c10_set_integration_time(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_AGAIN:
                if (arg)
                        ret = ov04c10_set_analog_gain(sd, sensor_val->value);
                break;
#endif /* SENSOR_EXPO */
        case TX_ISP_EVENT_SENSOR_DGAIN:
                if (arg)
                        ret = ov04c10_set_digital_gain(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
                if (arg)
                        ret = ov04c10_get_black_pedestal(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_RESIZE:
                if (arg)
                        ret = ov04c10_set_mode(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
                if (arg)
                        ret = ov04c10_write_array(sd, ov04c10_stream_off_mipi);
                break;
        case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
                if (arg)
                        ret = ov04c10_write_array(sd, ov04c10_stream_on_mipi);
                break;
        case TX_ISP_EVENT_SENSOR_FPS:
                if (arg)
                        ret = ov04c10_set_fps(sd, sensor_val->value);
                break;
#ifdef SENSOR_MIR_FLIP
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = ov04c10_set_vflip(sd, sensor_val->value);
		break;
#endif /* SENSOR_MIR_FLIP */
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
        case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
                if (arg)
                        ret = ov04c10_set_expo_short(sd, sensor_val->value);
                break;
#else
        case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
                if(arg)
                        ret = ov04c10_set_integration_time_short(sd, sensor_val->value);
                break;
        case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
                if(arg)
                        ret = ov04c10_set_analog_gain_short(sd, sensor_val->value);
                break;
#endif /* SENSOR_EXPO */
        case TX_ISP_EVENT_SENSOR_WDR:
                if(arg)
                        ret = ov04c10_set_wdr(sd, init->enable);
                break;
        case TX_ISP_EVENT_SENSOR_WDR_STOP:
                if(arg)
                        ret = ov04c10_set_wdr_stop(sd, init->enable);
                break;
#endif /* SENSOR_WDR_2_FRAME */
        default:
                break;
        }

        return 0;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops ov04c10_core_ops = {
        .g_chip_ident = ov04c10_g_chip_ident,
        .reset = ov04c10_reset,
        .init = ov04c10_init,
        .g_register = ov04c10_g_register,
        .s_register = ov04c10_s_register,
};

static struct tx_isp_subdev_video_ops ov04c10_video_ops = {
        .s_stream = ov04c10_s_stream,
};

static struct tx_isp_subdev_sensor_ops ov04c10_sensor_ops = {
#ifndef SENSOR_TEST
        .ioctl  = ov04c10_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops ov04c10_ops = {
        .core = &ov04c10_core_ops,
        .video = &ov04c10_video_ops,
        .sensor = &ov04c10_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
        .name = "ov04c10",
        .id = -1,
        .dev = {
                .dma_mask = &tx_isp_module_dma_mask,
                .coherent_dma_mask = 0xffffffff,
                .platform_data = NULL,
        },
        .num_resources = 0,
};

static int ov04c10_probe(struct i2c_client *client,
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

        sensor->video.attr = &ov04c10_attr;
        sensor->dev = &client->dev;
        tx_isp_subdev_init(&sensor_platform_device, sd, &ov04c10_ops);
        tx_isp_set_subdevdata(sd, client);
        tx_isp_set_subdev_hostdata(sd, sensor);
        private_i2c_set_clientdata(client, sd);

        pr_debug("probe ok ------->ov04c10\n");

        return 0;
}

static int ov04c10_remove(struct i2c_client *client)
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

static const struct i2c_device_id ov04c10_id[] = {
        {"ov04c10", 0},
        {}
};
MODULE_DEVICE_TABLE(i2c, ov04c10_id
                   );

static struct i2c_driver ov04c10_driver = {
        .driver = {
                .owner  = THIS_MODULE,
                .name   = "ov04c10",
        },
        .probe          = ov04c10_probe,
        .remove         = ov04c10_remove,
        .id_table       = ov04c10_id,
};

static __init int init_ov04c10(void) {
        return private_i2c_add_driver(&ov04c10_driver);
}

static __exit void exit_ov04c10(void) {
        private_i2c_del_driver(&ov04c10_driver);
}

module_init(init_ov04c10);
module_exit(exit_ov04c10);

MODULE_DESCRIPTION("A low-level driver for Sony ov04c10 sensor");
MODULE_LICENSE("GPL");
