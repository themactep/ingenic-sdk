/*
 * sc2356.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps     dvdd
 *   0          1600x1200       15        mipi_1lane    linear  10        15         无
 *   1          1600x1200       30        mipi_1lane    linear  10        30         无
 * @I2C addr:0x36,0x10
 *
 * @FSync:
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

#define TVERSION "V20241115a"
#define SENSOR_VERSION  "H20250211a"

//#define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
//#define SENSOR_HCG
//#define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0xeb)
#define SENSOR_CHIP_ID_L    (0x52)
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
	uint8_t value;
};

struct tx_isp_sensor_attribute sc2356_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sc2356_again_lut[] = {
	//cnt_gain = 160 cnt_reg = 160
	{0x80, 0},
	{0x84, 2886},
	{0x88, 5776},
	{0x8c, 8494},
	{0x90, 11136},
	{0x94, 13706},
	{0x98, 16287},
	{0x9c, 18723},
	{0xa0, 21097},
	{0xa4, 23413},
	{0xa8, 25746},
	{0xac, 27952},
	{0xb0, 30108},
	{0xb4, 32216},
	{0xb8, 34344},
	{0xbc, 36361},
	{0xc0, 38335},
	{0xc4, 40269},
	{0xc8, 42225},
	{0xcc, 44082},
	{0xd0, 45903},
	{0xd4, 47689},
	{0xd8, 49499},
	{0xdc, 51220},
	{0xe0, 52910},
	{0xe4, 54570},
	{0xe8, 56253},
	{0xec, 57856},
	{0xf0, 59433},
	{0xf4, 60983},
	{0xf8, 62557},
	{0xfc, 64058},
	{0x180, 65535},
	{0x184, 68421},
	{0x188, 71311},
	{0x18c, 74029},
	{0x190, 76671},
	{0x194, 79241},
	{0x198, 81822},
	{0x19c, 84258},
	{0x1a0, 86632},
	{0x1a4, 88948},
	{0x1a8, 91281},
	{0x1ac, 93487},
	{0x1b0, 95643},
	{0x1b4, 97751},
	{0x1b8, 99879},
	{0x1bc, 101896},
	{0x1c0, 103870},
	{0x1c4, 105804},
	{0x1c8, 107760},
	{0x1cc, 109617},
	{0x1d0, 111438},
	{0x1d4, 113224},
	{0x1d8, 115034},
	{0x1dc, 116755},
	{0x1e0, 118445},
	{0x1e4, 120105},
	{0x1e8, 121788},
	{0x1ec, 123391},
	{0x1f0, 124968},
	{0x1f4, 126518},
	{0x1f8, 128092},
	{0x1fc, 129593},
	{0x380, 131070},
	{0x384, 133956},
	{0x388, 136846},
	{0x38c, 139564},
	{0x390, 142206},
	{0x394, 144776},
	{0x398, 147357},
	{0x39c, 149793},
	{0x3a0, 152167},
	{0x3a4, 154483},
	{0x3a8, 156816},
	{0x3ac, 159022},
	{0x3b0, 161178},
	{0x3b4, 163286},
	{0x3b8, 165414},
	{0x3bc, 167431},
	{0x3c0, 169405},
	{0x3c4, 171339},
	{0x3c8, 173295},
	{0x3cc, 175152},
	{0x3d0, 176973},
	{0x3d4, 178759},
	{0x3d8, 180569},
	{0x3dc, 182290},
	{0x3e0, 183980},
	{0x3e4, 185640},
	{0x3e8, 187323},
	{0x3ec, 188926},
	{0x3f0, 190503},
	{0x3f4, 192053},
	{0x3f8, 193627},
	{0x3fc, 195128},
	{0x780, 196605},
	{0x784, 199491},
	{0x788, 202381},
	{0x78c, 205099},
	{0x790, 207741},
	{0x794, 210311},
	{0x798, 212892},
	{0x79c, 215328},
	{0x7a0, 217702},
	{0x7a4, 220018},
	{0x7a8, 222351},
	{0x7ac, 224557},
	{0x7b0, 226713},
	{0x7b4, 228821},
	{0x7b8, 230949},
	{0x7bc, 232966},
	{0x7c0, 234940},
	{0x7c4, 236874},
	{0x7c8, 238830},
	{0x7cc, 240687},
	{0x7d0, 242508},
	{0x7d4, 244294},
	{0x7d8, 246104},
	{0x7dc, 247825},
	{0x7e0, 249515},
	{0x7e4, 251175},
	{0x7e8, 252858},
	{0x7ec, 254461},
	{0x7f0, 256038},
	{0x7f4, 257588},
	{0x7f8, 259162},
	{0x7fc, 260663},
	{0xf80, 262140},
	{0xf84, 265026},
	{0xf88, 267916},
	{0xf8c, 270634},
	{0xf90, 273276},
	{0xf94, 275846},
	{0xf98, 278427},
	{0xf9c, 280863},
	{0xfa0, 283237},
	{0xfa4, 285553},
	{0xfa8, 287886},
	{0xfac, 290092},
	{0xfb0, 292248},
	{0xfb4, 294356},
	{0xfb8, 296484},
	{0xfbc, 298501},
	{0xfc0, 300475},
	{0xfc4, 302409},
	{0xfc8, 304365},
	{0xfcc, 306222},
	{0xfd0, 308043},
	{0xfd4, 309829},
	{0xfd8, 311639},
	{0xfdc, 313360},
	{0xfe0, 315050},
	{0xfe4, 316710},
	{0xfe8, 318393},
	{0xfec, 319996},
	{0xff0, 321573},
	{0xff4, 323123},
	{0xff8, 324697},
	{0xffc, 326198},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int sc2356_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc2356_again_lut;
	while (lut->gain <= sc2356_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sc2356_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int sc2356_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc2356_again_lut;
	while(lut->gain <= sc2356_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc2356_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int sc2356_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int sc2356_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int sc2356_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus sc2356_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 720,
	.lans = 1,
	.image_twidth = 1600,
	.image_theight = 1200,
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

struct tx_isp_dvp_bus sc2356_dvp = {
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

struct tx_isp_sensor_attribute sc2356_attr = {
	.name = "sc2356",
	.chip_id = 0xeb52,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x10,
	.sensor_ctrl.alloc_again = sc2356_alloc_again,
	.sensor_ctrl.alloc_dgain = sc2356_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = sc2356_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list sc2356_init_regs_1600_1200_15fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x320e,0x09},
	{0x320f,0xc4},
	{0x36e9,0x80},
	{0x36e9,0x24},
	{0x301f,0x01},
	{0x3301,0xff},
	{0x3304,0x68},
	{0x3306,0x40},
	{0x3308,0x08},
	{0x3309,0xa8},
	{0x330b,0xb0},
	{0x330c,0x18},
	{0x330d,0xff},
	{0x330e,0x20},
	{0x331e,0x59},
	{0x331f,0x99},
	{0x3333,0x10},
	{0x335e,0x06},
	{0x335f,0x08},
	{0x3364,0x1f},
	{0x337c,0x02},
	{0x337d,0x0a},
	{0x338f,0xa0},
	{0x3390,0x01},
	{0x3391,0x03},
	{0x3392,0x1f},
	{0x3393,0xff},
	{0x3394,0xff},
	{0x3395,0xff},
	{0x33a2,0x04},
	{0x33ad,0x0c},
	{0x33b1,0x20},
	{0x33b3,0x38},
	{0x33f9,0x40},
	{0x33fb,0x48},
	{0x33fc,0x0f},
	{0x33fd,0x1f},
	{0x349f,0x03},
	{0x34a6,0x03},
	{0x34a7,0x1f},
	{0x34a8,0x38},
	{0x34a9,0x30},
	{0x34ab,0xb0},
	{0x34ad,0xb0},
	{0x34f8,0x1f},
	{0x34f9,0x20},
	{0x3630,0xa0},
	{0x3631,0x92},
	{0x3632,0x64},
	{0x3633,0x43},
	{0x3637,0x49},
	{0x363a,0x85},
	{0x363c,0x0f},
	{0x3650,0x31},
	{0x3670,0x0d},
	{0x3674,0xc0},
	{0x3675,0xa0},
	{0x3676,0xa0},
	{0x3677,0x92},
	{0x3678,0x96},
	{0x3679,0x9a},
	{0x367c,0x03},
	{0x367d,0x0f},
	{0x367e,0x01},
	{0x367f,0x0f},
	{0x3698,0x83},
	{0x3699,0x86},
	{0x369a,0x8c},
	{0x369b,0x94},
	{0x36a2,0x01},
	{0x36a3,0x03},
	{0x36a4,0x07},
	{0x36ae,0x0f},
	{0x36af,0x1f},
	{0x36bd,0x22},
	{0x36be,0x22},
	{0x36bf,0x22},
	{0x36d0,0x01},
	{0x370f,0x02},
	{0x3721,0x6c},
	{0x3722,0x8d},
	{0x3725,0xc5},
	{0x3727,0x14},
	{0x3728,0x04},
	{0x37b7,0x04},
	{0x37b8,0x04},
	{0x37b9,0x06},
	{0x37bd,0x07},
	{0x37be,0x0f},
	{0x3901,0x02},
	{0x3903,0x40},
	{0x3905,0x8d},
	{0x3907,0x00},
	{0x3908,0x41},
	{0x391f,0x41},
	{0x3933,0x80},
	{0x3934,0x02},
	{0x3937,0x6f},
	{0x393a,0x01},
	{0x393d,0x01},
	{0x393e,0xc0},
	{0x39dd,0x41},
	{0x3e00,0x00},
	{0x3e01,0x4d},
	{0x3e02,0xc0},
	{0x3e09,0x00},
	{0x4509,0x28},
	{0x450d,0x61},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sc2356_init_regs_1600_1200_30fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x36e9,0x24},
	{0x301f,0x01},
	{0x3301,0xff},
	{0x3304,0x68},
	{0x3306,0x40},
	{0x3308,0x08},
	{0x3309,0xa8},
	{0x330b,0xb0},
	{0x330c,0x18},
	{0x330d,0xff},
	{0x330e,0x20},
	{0x331e,0x59},
	{0x331f,0x99},
	{0x3333,0x10},
	{0x335e,0x06},
	{0x335f,0x08},
	{0x3364,0x1f},
	{0x337c,0x02},
	{0x337d,0x0a},
	{0x338f,0xa0},
	{0x3390,0x01},
	{0x3391,0x03},
	{0x3392,0x1f},
	{0x3393,0xff},
	{0x3394,0xff},
	{0x3395,0xff},
	{0x33a2,0x04},
	{0x33ad,0x0c},
	{0x33b1,0x20},
	{0x33b3,0x38},
	{0x33f9,0x40},
	{0x33fb,0x48},
	{0x33fc,0x0f},
	{0x33fd,0x1f},
	{0x349f,0x03},
	{0x34a6,0x03},
	{0x34a7,0x1f},
	{0x34a8,0x38},
	{0x34a9,0x30},
	{0x34ab,0xb0},
	{0x34ad,0xb0},
	{0x34f8,0x1f},
	{0x34f9,0x20},
	{0x3630,0xa0},
	{0x3631,0x92},
	{0x3632,0x64},
	{0x3633,0x43},
	{0x3637,0x49},
	{0x363a,0x85},
	{0x363c,0x0f},
	{0x3650,0x31},
	{0x3670,0x0d},
	{0x3674,0xc0},
	{0x3675,0xa0},
	{0x3676,0xa0},
	{0x3677,0x92},
	{0x3678,0x96},
	{0x3679,0x9a},
	{0x367c,0x03},
	{0x367d,0x0f},
	{0x367e,0x01},
	{0x367f,0x0f},
	{0x3698,0x83},
	{0x3699,0x86},
	{0x369a,0x8c},
	{0x369b,0x94},
	{0x36a2,0x01},
	{0x36a3,0x03},
	{0x36a4,0x07},
	{0x36ae,0x0f},
	{0x36af,0x1f},
	{0x36bd,0x22},
	{0x36be,0x22},
	{0x36bf,0x22},
	{0x36d0,0x01},
	{0x370f,0x02},
	{0x3721,0x6c},
	{0x3722,0x8d},
	{0x3725,0xc5},
	{0x3727,0x14},
	{0x3728,0x04},
	{0x37b7,0x04},
	{0x37b8,0x04},
	{0x37b9,0x06},
	{0x37bd,0x07},
	{0x37be,0x0f},
	{0x3901,0x02},
	{0x3903,0x40},
	{0x3905,0x8d},
	{0x3907,0x00},
	{0x3908,0x41},
	{0x391f,0x41},
	{0x3933,0x80},
	{0x3934,0x02},
	{0x3937,0x6f},
	{0x393a,0x01},
	{0x393d,0x01},
	{0x393e,0xc0},
	{0x39dd,0x41},
	{0x3e00,0x00},
	{0x3e01,0x4d},
	{0x3e02,0xc0},
	{0x3e09,0x00},
	{0x4509,0x28},
	{0x450d,0x61},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the sc2356_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc2356_win_sizes[] = {
	{
		.width          = 1600,
		.height         = 1200,
		.fps            = 15 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc2356_init_regs_1600_1200_15fps_mipi,
	},
	{
		.width          = 1600,
		.height         = 1200,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc2356_init_regs_1600_1200_30fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &sc2356_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sc2356_stream_on_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list sc2356_stream_off_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int sc2356_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int sc2356_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int sc2356_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2356_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc2356_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2356_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int sc2356_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int sc2356_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc2356_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2356_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int sc2356_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2356_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int sc2356_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int sc2356_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int sc2356_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &sc2356_win_sizes[0];
		sc2356_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc2356_attr.again.value = 128;
		sc2356_attr.again.max = 326198;
		sc2356_attr.again.min = 0;
		sc2356_attr.again.apply_delay = 2;

		sc2356_attr.integration_time.value = 1244;
		sc2356_attr.integration_time.max = 2500 - 6;
		sc2356_attr.integration_time.min = 1;
		sc2356_attr.integration_time.apply_delay = 2;

		sc2356_attr.total_width = 1920;
		sc2356_attr.total_height = 2500;

		sc2356_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		sc2356_attr.hcg.base_gain = ;
		sc2356_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc2356_attr.again_short.value = ;
		sc2356_attr.again_short.max = ;
		sc2356_attr.again_short.min = ;
		sc2356_attr.again_short.apply_delay = ;

		sc2356_attr.integration_time_short.value = ;
		sc2356_attr.integration_time_short.max = ;
		sc2356_attr.integration_time_short.min = ;
		sc2356_attr.integration_time_short.apply_delay = ;

		sc2356_attr.wdr_cache = wdr_line * sc2356_attr.total_width;

#ifdef SENSOR_HCG
		sc2356_attr.hcg_short.base_gain = ;
		sc2356_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(sc2356_attr.mipi)), (void *)(&sc2356_mipi_linear), sizeof(sc2356_attr.mipi));
		break;
	case 1:
		wsize = &sc2356_win_sizes[1];
		sc2356_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc2356_attr.again.value = 128;
		sc2356_attr.again.max = 326198;
		sc2356_attr.again.min = 0;
		sc2356_attr.again.apply_delay = 2;

		sc2356_attr.integration_time.value = 1244;
		sc2356_attr.integration_time.max = 1250 - 6;
		sc2356_attr.integration_time.min = 1;
		sc2356_attr.integration_time.apply_delay = 2;

		sc2356_attr.total_width = 1920;
		sc2356_attr.total_height = 1250;

		sc2356_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		sc2356_attr.hcg.base_gain = ;
		sc2356_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc2356_attr.again_short.value = ;
		sc2356_attr.again_short.max = ;
		sc2356_attr.again_short.min = ;
		sc2356_attr.again_short.apply_delay = ;

		sc2356_attr.integration_time_short.value = ;
		sc2356_attr.integration_time_short.max = ;
		sc2356_attr.integration_time_short.min = ;
		sc2356_attr.integration_time_short.apply_delay = ;

		sc2356_attr.wdr_cache = wdr_line * sc2356_attr.total_width;

#ifdef SENSOR_HCG
		sc2356_attr.hcg_short.base_gain = ;
		sc2356_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(sc2356_attr.mipi)), (void *)(&sc2356_mipi_linear), sizeof(sc2356_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int sc2356_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	sc2356_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		sc2356_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc2356_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		sc2356_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc2356_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		sc2356_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = sc2356_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.apply_delay = 1;
	sc2356_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int sc2356_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sc2356_read(sd, 0x3107, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc2356_read(sd, 0x3108, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc2356_g_chip_ident(struct tx_isp_subdev *sd,
							   struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sc2356_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "sc2356_reset");
		if (!ret) {
			private_gpio_direction_output(info->rst_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
		}
	}
	if (info->pwdn_gpio != -1) {
		ret = private_gpio_request(info->pwdn_gpio, "sc2356_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = sc2356_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc2356 chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("SC2356 version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", sc2356_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "sc2356", sizeof("sc2356"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc2356_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sc2356_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = sc2356_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int sc2356_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc2356_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc2356_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc2356_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int sc2356_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = sc2356_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sc2356_write_array(sd, sc2356_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("sc2356 stream on\n");
		}

	} else {
		ret = sc2356_write_array(sd, sc2356_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("sc2356 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int sc2356_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret += sc2356_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc2356_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc2356_write(sd, 0x3e02, (unsigned char)((it & 0xf) << 4));

	ret += sc2356_write(sd, 0x3e09, (unsigned char)((again >> 8) & 0xff));
	ret += sc2356_write(sd, 0x3e07, (unsigned char)(again & 0xff));

	return ret;
}
#else
static int sc2356_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int sc2356_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc2356_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc2356_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc2356_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = sc2356_attr_set(sd, wsize);
	}

	return ret;
}

static int sc2356_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 1920 * 2500 * 15;  /**< HTS * VTS * FPS */
		max_fps = wsize->fps;
		break;
	case 1:
		sclk = 1920 * 1250 * 30;  /**< HTS * VTS * FPS */
		max_fps = wsize->fps;
		break;
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d/%d) no in range\n", fps >> 16, fps & 0xffff);
		return -1;
	}

	val = 0;
	ret += sc2356_read(sd, 0x320c, &val);
	hts = val << 8;
	val = 0;
	ret += sc2356_read(sd, 0x320d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: sc2356 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	sc2356_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	sc2356_write(sd, 0x320e, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: sc2356_write err\n");
		return ret;
	}

	sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->integration_time.max = vts - 6;
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

static int sc2356_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	ret = sc2356_read(sd, 0x3221, &val);
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		/* sensor->video.mbus.code = wsize->mbus_code; */
		val &= 0x99;
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		/* sensor->video.mbus.code = wsize->mbus_code; */
		val = ((val & 0x99) | 0x06);
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		/* sensor->video.mbus.code = wsize->mbus_code; */
		val = ((val & 0x99) | 0x60);
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		/* sensor->video.mbus.code = wsize->mbus_code; */
		val |= 0x66;
		break;
	}

	ret += sc2356_write(sd, 0x3221, val);

	sensor->video.hvflip_mode = par->hvflip;
	sc2356_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int sc2356_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int sc2356_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int sc2356_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc2356_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = sc2356_write_array(sd, sc2356_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		sc2356_setting_select(sd, 1);
		sc2356_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		sc2356_setting_select(sd, 0);
		sc2356_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int sc2356_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = sc2356_write_array(sd, wsize->regs);
	ret = sc2356_write_array(sd, sc2356_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int sc2356_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	/* return 0; */
	switch (cmd) {
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = sc2356_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sc2356_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sc2356_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sc2356_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sc2356_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sc2356_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = sc2356_write_array(sd, sc2356_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = sc2356_write_array(sd, sc2356_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sc2356_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = sc2356_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = sc2356_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = sc2356_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = sc2356_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = sc2356_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = sc2356_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops sc2356_core_ops = {
	.g_chip_ident = sc2356_g_chip_ident,
	.reset = sc2356_reset,
	.init = sc2356_init,
	.g_register = sc2356_g_register,
	.s_register = sc2356_s_register,
};

static struct tx_isp_subdev_video_ops sc2356_video_ops = {
	.s_stream = sc2356_s_stream,
};

static struct tx_isp_subdev_sensor_ops sc2356_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = sc2356_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops sc2356_ops = {
	.core = &sc2356_core_ops,
	.video = &sc2356_video_ops,
	.sensor = &sc2356_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
	.name = "sc2356",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc2356_probe(struct i2c_client *client,
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
	sensor->video.attr = &sc2356_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc2356_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc2356\n");

	return 0;
}

static int sc2356_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc2356_id[] = {
	{"sc2356", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, sc2356_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver sc2356_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "sc2356",
	},
	.probe          = sc2356_probe,
	.remove         = sc2356_remove,
	.id_table       = sc2356_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_sc2356(void) {
	return private_i2c_add_driver(&sc2356_driver);
}

static __exit void exit_sc2356(void) {
	private_i2c_del_driver(&sc2356_driver);
}

module_init(init_sc2356);
module_exit(exit_sc2356);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_sc2356(void) {
	return private_i2c_add_driver(&sc2356_driver);
}

static void exit_first_sc2356(void) {
	private_i2c_del_driver(&sc2356_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "sc2356",
	.i2c_addr = 0x10,
	.width = 1600,
	.height = 1200,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_sc2356,
	.exit_sensor = exit_first_sc2356
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for sc2356 sensor");
MODULE_LICENSE("GPL");
