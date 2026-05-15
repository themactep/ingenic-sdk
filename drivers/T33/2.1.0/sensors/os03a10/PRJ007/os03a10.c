/*
 * os03a10.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps     dvdd
 *  0           2560*1440       30        mipi_2lane    linear  12        30        1.2V
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

#define TVERSION "V20241023a"
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
#define SENSOR_CHIP_ID_M	(0x03)
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

struct tx_isp_sensor_attribute os03a10_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut os03a10_again_lut[] = {
	{0x0100, 0},
	{0x0110, 5732},
	{0x0120, 11136},
	{0x0130, 16248},
	{0x0140, 21098},
	{0x0150, 25711},
	{0x0160, 30109},
	{0x0170, 34312},
	{0x0180, 38336},
	{0x0190, 42196},
	{0x01a0, 45904},
	{0x01b0, 49472},
	{0x01c0, 52911},
	{0x01d0, 56229},
	{0x01e0, 59434},
	{0x01f0, 62534},
	{0x0200, 65536},
	{0x0220, 71268},
	{0x0240, 76672},
	{0x0260, 81784},
	{0x0280, 86634},
	{0x02a0, 91247},
	{0x02c0, 95645},
	{0x02e0, 99848},
	{0x0300, 103872},
	{0x0320, 107732},
	{0x0340, 111440},
	{0x0360, 115008},
	{0x0380, 118447},
	{0x03a0, 121765},
	{0x03c0, 124970},
	{0x03e0, 128070},
	{0x0400, 131072},
	{0x0440, 136804},
	{0x0480, 142208},
	{0x04c0, 147320},
	{0x0500, 152170},
	{0x0540, 156783},
	{0x0580, 161181},
	{0x05c0, 165384},
	{0x0600, 169408},
	{0x0640, 173268},
	{0x0680, 176976},
	{0x06c0, 180544},
	{0x0700, 183983},
	{0x0740, 187301},
	{0x0780, 190506},
	{0x07c0, 193606},
	{0x0800, 196608},
	{0x0880, 202340},
	{0x0900, 207744},
	{0x0980, 212856},
	{0x0a00, 217706},
	{0x0a80, 222319},
	{0x0b00, 226717},
	{0x0b80, 230920},
	{0x0c00, 234944},
	{0x0c80, 238804},
	{0x0d00, 242512},
	{0x0d80, 246080},
	{0x0e00, 249519},
	{0x0e80, 252837},
	{0x0f00, 256042},
	{0x0f80, 259142},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int os03a10_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = os03a10_again_lut;
	while (lut->gain <= os03a10_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == os03a10_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int os03a10_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = os03a10_again_lut;
	while(lut->gain <= os03a10_attr.max_again_short) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == os03a10_attr.max_again_short) && (isp_gain >= lut->gain)) {
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

unsigned int os03a10_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int os03a10_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int os03a10_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus os03a10_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 864,
	.lans = 2,
	.image_twidth = 2560,
	.image_theight = 1440,
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

struct tx_isp_dvp_bus os03a10_dvp = {
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

struct tx_isp_sensor_attribute os03a10_attr = {
	.name = "os03a10",
	.chip_id = 0x530341,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x36,
	.sensor_ctrl.alloc_again = os03a10_alloc_again,
	.sensor_ctrl.alloc_dgain = os03a10_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = os03a10_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list os03a10_init_regs_2560_1440_30fps_mipi[] = {
	{0x302a,0x00},
	{0x0103,0x01},
	{0x0109,0x01},
	{0x0104,0x02},
	{0x0102,0x00},
	{0x0305,0x48},
	{0x0306,0x00},
	{0x0308,0x05},
	{0x030a,0x01},
	{0x0317,0x0a},
	{0x0322,0x01},
	{0x0323,0x02},
	{0x0324,0x00},
	{0x0325,0x90},
	{0x0327,0x05},
	{0x0329,0x01},
	{0x032c,0x02},
	{0x300f,0x11},
	{0x3012,0x21},
	{0x3026,0x10},
	{0x3027,0x08},
	{0x302d,0x24},
	{0x3106,0x10},
	{0x3400,0x00},
	{0x3408,0x05},
	{0x340c,0x0c},
	{0x340d,0xb0},
	{0x3425,0x51},
	{0x3426,0x50},
	{0x3427,0x15},
	{0x3428,0x50},
	{0x3429,0x10},
	{0x342a,0x10},
	{0x342b,0x04},
	{0x3501,0x02},
	{0x3504,0x08},
	{0x3508,0x01},
	{0x3509,0x00},
	{0x350a,0x01},
	{0x3542,0x10},
	{0x3544,0x08},
	{0x3548,0x01},
	{0x3549,0x00},
	{0x3582,0x02},
	{0x3584,0x08},
	{0x3588,0x01},
	{0x3589,0x00},
	{0x3601,0x70},
	{0x3604,0xe3},
	{0x3603,0xe5},
	{0x3605,0xff},
	{0x3606,0x01},
	{0x3608,0xa8},
	{0x360a,0xd0},
	{0x360b,0x18},
	{0x360e,0xc8},
	{0x360f,0x66},
	{0x3610,0x89},
	{0x3611,0x8a},
	{0x3612,0x4e},
	{0x3613,0xbd},
	{0x3614,0x9b},
	{0x3623,0x08},
	{0x362a,0x0e},
	{0x362b,0x0e},
	{0x362c,0x0e},
	{0x362d,0x09},
	{0x362e,0x07},
	{0x362f,0x0f},
	{0x3630,0x1f},
	{0x3631,0x40},
	{0x3638,0x00},
	{0x3643,0x00},
	{0x3644,0x00},
	{0x3645,0x00},
	{0x3646,0x00},
	{0x3647,0x00},
	{0x3648,0x00},
	{0x3649,0x00},
	{0x364a,0x04},
	{0x364c,0x0e},
	{0x364d,0x0e},
	{0x364e,0x0e},
	{0x364f,0x0e},
	{0x3650,0xff},
	{0x3651,0xff},
	{0x365a,0x00},
	{0x365b,0x00},
	{0x365c,0x00},
	{0x365d,0x00},
	{0x3661,0x07},
	{0x3662,0x00},
	{0x3663,0x20},
	{0x3665,0x12},
	{0x3667,0xd4},
	{0x3668,0x80},
	{0x366c,0x00},
	{0x366d,0x00},
	{0x366e,0x00},
	{0x366f,0x00},
	{0x3671,0x08},
	{0x3673,0x2a},
	{0x3681,0x80},
	{0x3700,0x2d},
	{0x3701,0x22},
	{0x3702,0x25},
	{0x3703,0x28},
	{0x3705,0x00},
	{0x3706,0xa8},
	{0x3707,0x0a},
	{0x3708,0x36},
	{0x3709,0x57},
	{0x370a,0x02},
	{0x370b,0x18},
	{0x3714,0x01},
	{0x371b,0x16},
	{0x371c,0x00},
	{0x371d,0x08},
	{0x373f,0x4c},
	{0x3740,0x4c},
	{0x3741,0x4c},
	{0x3742,0x63},
	{0x3756,0xe6},
	{0x3757,0xe6},
	{0x3762,0x1d},
	{0x376c,0x00},
	{0x3776,0x05},
	{0x3777,0x22},
	{0x3779,0x60},
	{0x377c,0x48},
	{0x3793,0x04},
	{0x3794,0x07},
	{0x379c,0x4d},
	{0x3784,0x06},
	{0x3785,0x0a},
	{0x37c4,0x72},
	{0x37c5,0x72},
	{0x37c6,0x72},
	{0x37d0,0x00},
	{0x37d1,0xa8},
	{0x37d2,0x02},
	{0x37d3,0x18},
	{0x37d4,0x00},
	{0x37d5,0x6c},
	{0x37d6,0x00},
	{0x37d7,0xf7},
	{0x37d8,0x01},
	{0x37da,0x00},
	{0x37db,0x00},
	{0x37dc,0x00},
	{0x37dd,0x00},
	{0x3790,0x10},
	{0x3793,0x04},
	{0x3794,0x07},
	{0x3796,0x00},
	{0x3797,0x02},
	{0x37a1,0x80},
	{0x37bb,0x88},
	{0x37bd,0x01},
	{0x37be,0x48},
	{0x37bf,0x01},
	{0x37c0,0x01},
	{0x37ca,0x21},
	{0x37cc,0x13},
	{0x37cd,0x90},
	{0x37cf,0x02},
	{0x37ec,0x00},
	{0x37ed,0x00},
	{0x3800,0x00},
	{0x3801,0x00},
	{0x3802,0x00},
	{0x3803,0x00},
	{0x3804,0x0a},
	{0x3805,0x0f},
	{0x3806,0x05},
	{0x3807,0xaf},
	{0x3808,0x0a},
	{0x3809,0x00},
	{0x380a,0x05},
	{0x380b,0xa0},
	{0x380c,0x05},
	{0x380d,0xc4},
	{0x380e,0x06},
	{0x380f,0x58},
	{0x3811,0x08},
	{0x3813,0x08},
	{0x3814,0x01},
	{0x3815,0x01},
	{0x3816,0x01},
	{0x3817,0x01},
	{0x381c,0x00},
	{0x3820,0x02},
	{0x3821,0x00},
	{0x3822,0x14},
	{0x3823,0x18},
	{0x3826,0x00},
	{0x3827,0x00},
	{0x3833,0x40},
	{0x384c,0x05},
	{0x384d,0xc4},
	{0x3858,0x3c},
	{0x3865,0x02},
	{0x3866,0x00},
	{0x3867,0x00},
	{0x3868,0x02},
	{0x3900,0x13},
	{0x3940,0x13},
	{0x3980,0x13},
	{0x3c01,0x11},
	{0x3c05,0x00},
	{0x3c0f,0x1c},
	{0x3c12,0x0d},
	{0x3c19,0x00},
	{0x3c21,0x00},
	{0x3c3a,0x50},
	{0x3c3b,0x18},
	{0x3c3d,0xc6},
	{0x3c55,0xcb},
	{0x3c5d,0xcf},
	{0x3c5e,0xcf},
	{0x3ce0,0x00},
	{0x3ce1,0x00},
	{0x3ce2,0x00},
	{0x3ce3,0x00},
	{0x3d8c,0x70},
	{0x3d8d,0x10},
	{0x4001,0x2f},
	{0x4004,0x00},
	{0x4005,0x80},
	{0x4008,0x02},
	{0x4009,0x11},
	{0x400a,0x03},
	{0x400b,0x3f},
	{0x400e,0x40},
	{0x4011,0xbb},
	{0x402e,0x00},
	{0x402f,0x80},
	{0x4030,0x00},
	{0x4031,0x80},
	{0x4032,0x9f},
	{0x4033,0x80},
	{0x4050,0x00},
	{0x4051,0x07},
	{0x405e,0x20},
	{0x410f,0x01},
	{0x4288,0xcf},
	{0x4289,0x00},
	{0x428a,0x46},
	{0x430b,0xff},
	{0x430c,0xff},
	{0x430d,0x00},
	{0x430e,0x00},
	{0x4314,0x04},
	{0x4500,0x18},
	{0x4501,0x18},
	{0x4503,0x10},
	{0x4504,0x00},
	{0x4506,0x32},
	{0x4507,0x02},
	{0x4601,0x30},
	{0x4603,0x00},
	{0x460a,0x50},
	{0x460c,0x60},
	{0x4640,0x62},
	{0x4646,0xaa},
	{0x4647,0x55},
	{0x4648,0x99},
	{0x4649,0x66},
	{0x464d,0x00},
	{0x4654,0x11},
	{0x4655,0x22},
	{0x4800,0x04},
	{0x480e,0x00},
	{0x4810,0xff},
	{0x4811,0xff},
	{0x4813,0x00},
	{0x481f,0x30},
	{0x4837,0x12},
	{0x484b,0x27},
	{0x4d00,0x4d},
	{0x4d01,0x9d},
	{0x4d02,0xb9},
	{0x4d03,0x2e},
	{0x4d04,0x4a},
	{0x4d05,0x3d},
	{0x4d09,0x4f},
	{0x5000,0x1f},
	{0x5001,0x0d},
	{0x5080,0x00},
	{0x50c0,0x00},
	{0x5100,0x00},
	{0x5200,0x00},
	{0x5201,0x00},
	{0x5202,0x03},
	{0x5203,0xff},
	{0x5780,0x53},
	{0x5782,0x60},
	{0x5783,0xf0},
	{0x5786,0x01},
	{0x5788,0x60},
	{0x5789,0xf0},
	{0x5792,0x11},
	{0x5793,0x33},
	{0x5857,0x00},
	{0x5858,0x00},
	{0x5859,0x00},
	{0x58d7,0x00},
	{0x58d8,0x00},
	{0x58d9,0x00},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the os03a10_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting os03a10_win_sizes[] = {
	/* 2560*1440 [0] */
	{
		.width			= 2560,
		.height			= 1440,
		.fps			= 30 << 16 | 1,
		.mbus_code		= TISP_VI_FMT_SBGGR12_1X12,
		.colorspace		= TISP_COLORSPACE_SRGB,
		.regs			= os03a10_init_regs_2560_1440_30fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &os03a10_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list os03a10_stream_on_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list os03a10_stream_off_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int os03a10_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int os03a10_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int os03a10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = os03a10_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int os03a10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = os03a10_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif	/* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int os03a10_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int os03a10_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int os03a10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = os03a10_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int os03a10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = os03a10_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif	/* SENSOR_I2C_REG_16BIT */

static int os03a10_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int os03a10_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int os03a10_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &os03a10_win_sizes[0];
		os03a10_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		os03a10_attr.again.value = 0;
		os03a10_attr.again.max = 259142;
		os03a10_attr.again.min = 0;
		os03a10_attr.again.apply_delay = 2;

		os03a10_attr.integration_time.value = 0x240;
		os03a10_attr.integration_time.max = 1624 - 8;
		os03a10_attr.integration_time.min = 2;
		os03a10_attr.integration_time.apply_delay = 2;

		os03a10_attr.total_width = 1476;
		os03a10_attr.total_height = 1624;

		os03a10_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		os03a10_attr.hcg.base_gain = ;
		os03a10_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		os03a10_attr.again_short.value = ;
		os03a10_attr.again_short.max = ;
		os03a10_attr.again_short.min = ;
		os03a10_attr.again_short.apply_delay = ;

		os03a10_attr.integration_time_short.value = ;
		os03a10_attr.integration_time_short.max = ;
		os03a10_attr.integration_time_short.min = ;
		os03a10_attr.integration_time_short.apply_delay = ;

		os03a10_attr.wdr_cache = wdr_line * os03a10_attr.total_width;

#ifdef SENSOR_HCG
		os03a10_attr.hcg_short.base_gain = ;
		os03a10_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(os03a10_attr.mipi)), (void *)(&os03a10_mipi_linear), sizeof(os03a10_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int os03a10_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	os03a10_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		os03a10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		os03a10_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		os03a10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		os03a10_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		os03a10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = os03a10_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
	os03a10_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int os03a10_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = os03a10_read(sd, 0x300a, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = os03a10_read(sd, 0x300b, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_M)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	ret = os03a10_read(sd, 0x300c, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int os03a10_g_chip_ident(struct tx_isp_subdev *sd,
								 struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	os03a10_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "os03a10_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "os03a10_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = os03a10_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an os03a10 chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("OS03A10 version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", os03a10_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "os03a10", sizeof("os03a10"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int os03a10_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int os03a10_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = os03a10_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int os03a10_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = os03a10_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int os03a10_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	os03a10_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int os03a10_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = os03a10_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = os03a10_write_array(sd, os03a10_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("os03a10 stream on\n");
		}

	} else {
		ret = os03a10_write_array(sd, os03a10_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("os03a10 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int os03a10_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret += os03a10_write(sd, 0x3501, (unsigned char)((it >> 8) & 0xff));
	ret += os03a10_write(sd, 0x3502, (unsigned char)(it & 0xff));

	ret += os03a10_write(sd, 0x3508, (unsigned char)((again >> 8) & 0x3f));
	ret += os03a10_write(sd, 0x3509, (unsigned char)(again & 0xff));

	return ret;
}
#else
static int os03a10_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int os03a10_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int os03a10_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int os03a10_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int os03a10_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = os03a10_attr_set(sd, wsize);
	}

	return ret;
}

static int os03a10_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 1476 * 1624 * 30;  /**< HTS * VTS * FPS */
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
	ret += os03a10_read(sd, 0x380c, &val);
	hts = val << 8;
	val = 0;
	ret += os03a10_read(sd, 0x380d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: os03a10 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	os03a10_write(sd, 0x380f, (unsigned char) (vts & 0xff));
	os03a10_write(sd, 0x380e, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: os03a10_write err\n");
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

static int os03a10_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	ret = os03a10_write(sd, 0x0100, 0x00);
	ret = os03a10_read(sd, 0x3820, &val);
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		val = ((val & 0xF9) | 0x02);
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		val &= 0xF9;
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		val |= 0x06;
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		val = ((val & 0xF9) | 0x04);
		break;
	}
	ret = os03a10_write(sd, 0x3820, val);
	ret = os03a10_write(sd, 0x0100, 0x01);

	sensor->video.hvflip_mode = par->hvflip;
	os03a10_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int os03a10_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int os03a10_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int os03a10_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int os03a10_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = os03a10_write_array(sd, os03a10_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		os03a10_setting_select(sd, 1);
		os03a10_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		os03a10_setting_select(sd, 0);
		os03a10_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int os03a10_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = os03a10_write_array(sd, wsize->regs);
	ret = os03a10_write_array(sd, os03a10_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int os03a10_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = os03a10_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = os03a10_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = os03a10_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = os03a10_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = os03a10_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = os03a10_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = os03a10_write_array(sd, os03a10_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = os03a10_write_array(sd, os03a10_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = os03a10_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = os03a10_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = os03a10_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = os03a10_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = os03a10_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = os03a10_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = os03a10_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops os03a10_core_ops = {
	.g_chip_ident = os03a10_g_chip_ident,
	.reset = os03a10_reset,
	.init = os03a10_init,
	.g_register = os03a10_g_register,
	.s_register = os03a10_s_register,
};

static struct tx_isp_subdev_video_ops os03a10_video_ops = {
	.s_stream = os03a10_s_stream,
};

static struct tx_isp_subdev_sensor_ops os03a10_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl	= os03a10_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops os03a10_ops = {
	.core = &os03a10_core_ops,
	.video = &os03a10_video_ops,
	.sensor = &os03a10_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "os03a10",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int os03a10_probe(struct i2c_client *client,
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
	sensor->video.attr = &os03a10_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &os03a10_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->os03a10\n");

	return 0;
}

static int os03a10_remove(struct i2c_client *client)
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

static const struct i2c_device_id os03a10_id[] = {
	{"os03a10", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, os03a10_id);
#endif	/* CONFIG_ZERATUL */

static struct i2c_driver os03a10_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner	= NULL,
#else
		.owner	= THIS_MODULE,
#endif	/* CONFIG_ZERATUL */
		.name	= "os03a10",
	},
	.probe			= os03a10_probe,
	.remove			= os03a10_remove,
	.id_table		= os03a10_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_os03a10(void) {
	return private_i2c_add_driver(&os03a10_driver);
}

static __exit void exit_os03a10(void) {
	private_i2c_del_driver(&os03a10_driver);
}

module_init(init_os03a10);
module_exit(exit_os03a10);
#endif	/* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_os03a10(void) {
	return private_i2c_add_driver(&os03a10_driver);
}

static void exit_first_os03a10(void) {
	private_i2c_del_driver(&os03a10_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "os03a10",
	.i2c_addr = 0x36,
	.width = 2560,
	.height = 1440,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_os03a10,
	.exit_sensor = exit_first_os03a10
};
#endif	/* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for Sony os03a10 sensor");
MODULE_LICENSE("GPL");
