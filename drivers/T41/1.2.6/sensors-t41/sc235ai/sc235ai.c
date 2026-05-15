/*
 * sc235ai.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode         DVDD
 *   0          1920*1056       30        mipi_2lane             linear       外供1.2V
 *   1          1920*1080       30        mipi_2lane             linear       外供1.2V
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

#define TVERSION "V20231226a"
#define SENSOR_VERSION	"H20241218a"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT	/**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE	  /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
// #define SENSOR_WDR_2_FRAME	 /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP			/**< 镜像翻转功能开关 */

#define SENSOR_CHIP_ID_H	(0xcb)
#define SENSOR_CHIP_ID_L	(0x6a)
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_MCLK 24000000

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

struct tx_isp_sensor_attribute sc235ai_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sc235ai_again_lut[] = {
	// cnt_gain = 220 cnt_reg = 220
	{0x20, 0},
	{0x21, 2886},
	{0x22, 5776},
	{0x23, 8494},
	{0x24, 11136},
	{0x25, 13706},
	{0x26, 16287},
	{0x27, 18723},
	{0x28, 21097},
	{0x29, 23413},
	{0x2a, 25746},
	{0x2b, 27952},
	{0x2c, 30108},
	{0x2d, 32216},
	{0x2e, 34344},
	{0x2f, 36361},
	{0x30, 38335},
	{0x31, 40269},
	{0x32, 42225},
	{0x33, 44082},
	{0x34, 45903},
	{0x35, 47689},
	{0x36, 49499},
	{0x37, 51220},
	{0x38, 52910},
	{0x39, 54570},
	{0x3a, 56253},
	{0x3b, 57856},
	{0x3c, 59433},
	{0x3d, 60983},
	{0x3e, 62557},
	{0x3f, 64058},
	{0x120, 65535},
	{0x121, 68467},
	{0x122, 71266},
	{0x123, 74029},
	{0x124, 76671},
	{0x125, 79281},
	{0x126, 81782},
	{0x127, 84258},
	{0x128, 86632},
	{0x129, 88985},
	{0x12a, 91245},
	{0x12b, 93487},
	{0x12c, 95643},
	{0x12d, 97785},
	{0x12e, 99846},
	{0x12f, 101896},
	{0x130, 103870},
	{0x131, 105835},
	{0x132, 107730},
	{0x133, 109617},
	{0x134, 111438},
	{0x135, 113253},
	{0x136, 115006},
	{0x137, 116755},
	{0x138, 118445},
	{0x139, 120131},
	{0x13a, 121762},
	{0x13b, 123391},
	{0x8020, 123698},
	{0x8021, 126617},
	{0x8022, 129424},
	{0x8023, 132174},
	{0x8024, 134846},
	{0x8025, 137422},
	{0x8026, 139952},
	{0x8027, 142394},
	{0x8028, 144796},
	{0x8029, 147138},
	{0x802a, 149404},
	{0x802b, 151636},
	{0x802c, 153817},
	{0x802d, 155930},
	{0x802e, 158015},
	{0x802f, 160037},
	{0x8030, 162034},
	{0x8031, 163990},
	{0x8032, 165889},
	{0x8033, 167768},
	{0x8034, 169610},
	{0x8035, 171401},
	{0x8036, 173174},
	{0x8037, 174899},
	{0x8038, 176608},
	{0x8039, 178287},
	{0x803a, 179923},
	{0x803b, 181544},
	{0x803c, 183138},
	{0x803d, 184693},
	{0x803e, 186235},
	{0x803f, 187740},
	{0x8120, 189233},
	{0x8121, 192140},
	{0x8122, 194971},
	{0x8123, 197709},
	{0x8124, 200370},
	{0x8125, 202957},
	{0x8126, 205487},
	{0x8127, 207940},
	{0x8128, 210331},
	{0x8129, 212663},
	{0x812a, 214949},
	{0x812b, 217171},
	{0x812c, 219342},
	{0x812d, 221465},
	{0x812e, 223550},
	{0x812f, 225581},
	{0x8130, 227569},
	{0x8131, 229516},
	{0x8132, 231433},
	{0x8133, 233303},
	{0x8134, 235137},
	{0x8135, 236936},
	{0x8136, 238709},
	{0x8137, 240442},
	{0x8138, 242143},
	{0x8139, 243815},
	{0x813a, 245465},
	{0x813b, 247079},
	{0x813c, 248667},
	{0x813d, 250228},
	{0x813e, 251770},
	{0x813f, 253281},
	{0x8320, 254768},
	{0x8321, 257681},
	{0x8322, 260500},
	{0x8323, 263244},
	{0x8324, 265905},
	{0x8325, 268498},
	{0x8326, 271016},
	{0x8327, 273475},
	{0x8328, 275866},
	{0x8329, 278203},
	{0x832a, 280479},
	{0x832b, 282706},
	{0x832c, 284877},
	{0x832d, 287004},
	{0x832e, 289080},
	{0x832f, 291116},
	{0x8330, 293104},
	{0x8331, 295056},
	{0x8332, 296964},
	{0x8333, 298838},
	{0x8334, 300672},
	{0x8335, 302475},
	{0x8336, 304240},
	{0x8337, 305977},
	{0x8338, 307678},
	{0x8339, 309354},
	{0x833a, 310996},
	{0x833b, 312614},
	{0x833c, 314202},
	{0x833d, 315766},
	{0x833e, 317302},
	{0x833f, 318816},
	{0x8720, 320303},
	{0x8721, 323213},
	{0x8722, 326035},
	{0x8723, 328776},
	{0x8724, 331440},
	{0x8725, 334030},
	{0x8726, 336551},
	{0x8727, 339007},
	{0x8728, 341401},
	{0x8729, 343736},
	{0x872a, 346014},
	{0x872b, 348239},
	{0x872c, 350412},
	{0x872d, 352537},
	{0x872e, 354615},
	{0x872f, 356648},
	{0x8730, 358639},
	{0x8731, 360588},
	{0x8732, 362499},
	{0x8733, 364371},
	{0x8734, 366207},
	{0x8735, 368008},
	{0x8736, 369775},
	{0x8737, 371510},
	{0x8738, 373213},
	{0x8739, 374887},
	{0x873a, 376531},
	{0x873b, 378147},
	{0x873c, 379737},
	{0x873d, 381299},
	{0x873e, 382837},
	{0x873f, 384350},
	{0x8f20, 385838},
	{0x8f21, 388748},
	{0x8f22, 391570},
	{0x8f23, 394311},
	{0x8f24, 396975},
	{0x8f25, 399565},
	{0x8f26, 402086},
	{0x8f27, 404542},
	{0x8f28, 406936},
	{0x8f29, 409271},
	{0x8f2a, 411549},
	{0x8f2b, 413774},
	{0x8f2c, 415947},
	{0x8f2d, 418072},
	{0x8f2e, 420150},
	{0x8f2f, 422183},
	{0x8f30, 424174},
	{0x8f31, 426123},
	{0x8f32, 428034},
	{0x8f33, 429906},
	{0x8f34, 431742},
	{0x8f35, 433543},
	{0x8f36, 435310},
	{0x8f37, 437045},
	{0x8f38, 438748},
	{0x8f39, 440422},
	{0x8f3a, 442066},
	{0x8f3b, 443682},
	{0x8f3c, 445272},
	{0x8f3d, 446834},
	{0x8f3e, 448372},
	{0x8f3f, 449885},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int sc235ai_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc235ai_again_lut;
	while (lut->gain <= sc235ai_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sc235ai_attr.max_again) && (isp_gain >= lut->gain)) {
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
unsigned int sc235ai_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc235ai_again_lut;
	while(lut->gain <= sc235ai_attr.max_again_short) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc235ai_attr.max_again_short) && (isp_gain >= lut->gain)) {
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

unsigned int sc235ai_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sc235ai_mipi_1920_1056_30fps_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 396,
	.lans = 2,
	.image_twidth = 1920,
	.image_theight = 1056,
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

struct tx_isp_mipi_bus sc235ai_mipi_1920_1080_30fps_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 396,
	.lans = 2,
	.image_twidth = 1920,
	.image_theight = 1080,
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

struct tx_isp_dvp_bus sc235ai_dvp = {
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

struct tx_isp_sensor_attribute sc235ai_attr = {
	.name = "sc235ai",
	.chip_id = 0xcb6a,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.sensor_ctrl.alloc_again = sc235ai_alloc_again,
	.sensor_ctrl.alloc_dgain = sc235ai_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = sc235ai_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */
};

static struct regval_list sc235ai_init_regs_1920_1056_30fps_mipi[] = {
	{0x0103,0x01},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x14},
	{0x3058,0x21},
	{0x3059,0x53},
	{0x305a,0x40},
	{0x320e,0x04},
	{0x320f,0xb0},
	{0x3210,0x00},
	{0x3211,0x04},
	{0x3212,0x00},
	{0x3213,0x04},
	{0x3250,0x00},
	{0x3301,0x0a},
	{0x3302,0x20},
	{0x3304,0x90},
	{0x3305,0x00},
	{0x3306,0x68},
	{0x3309,0xd0},
	{0x330b,0xd8},
	{0x330d,0x08},
	{0x331c,0x04},
	{0x331e,0x81},
	{0x331f,0xc1},
	{0x3323,0x06},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x3364,0x5e},
	{0x336c,0x8e},
	{0x337f,0x13},
	{0x338f,0x80},
	{0x3390,0x08},
	{0x3391,0x18},
	{0x3392,0xb8},
	{0x3393,0x0e},
	{0x3394,0x14},
	{0x3395,0x10},
	{0x3396,0x88},
	{0x3397,0x98},
	{0x3398,0xf8},
	{0x3399,0x0a},
	{0x339a,0x0e},
	{0x339b,0x10},
	{0x339c,0x3c},
	{0x33ae,0x80},
	{0x33af,0xc0},
	{0x33b2,0x50},
	{0x33b3,0x14},
	{0x33f8,0x00},
	{0x33f9,0x68},
	{0x33fa,0x00},
	{0x33fb,0x68},
	{0x33fc,0x48},
	{0x33fd,0x78},
	{0x349f,0x03},
	{0x34a6,0x40},
	{0x34a7,0x58},
	{0x34a8,0x10},
	{0x34a9,0x10},
	{0x34f8,0x78},
	{0x34f9,0x10},
	{0x3619,0x20},
	{0x361a,0x90},
	{0x3633,0x44},
	{0x3637,0x5c},
	{0x363c,0xc0},
	{0x363d,0x02},
	{0x3660,0x80},
	{0x3661,0x81},
	{0x3662,0x8f},
	{0x3663,0x81},
	{0x3664,0x81},
	{0x3665,0x82},
	{0x3666,0x8f},
	{0x3667,0x08},
	{0x3668,0x80},
	{0x3669,0x88},
	{0x366a,0x98},
	{0x366b,0xb8},
	{0x366c,0xf8},
	{0x3670,0xb2},
	{0x3671,0xa2},
	{0x3672,0x88},
	{0x3680,0x33},
	{0x3681,0x33},
	{0x3682,0x43},
	{0x36c0,0x80},
	{0x36c1,0x88},
	{0x36c8,0x88},
	{0x36c9,0xb8},
	{0x36ea,0x0b},
	{0x36eb,0x0c},
	{0x36ec,0x5c},
	{0x36ed,0x04},
	{0x3718,0x04},
	{0x3722,0x8b},
	{0x3724,0xd1},
	{0x3741,0x08},
	{0x3770,0x17},
	{0x3771,0x9b},
	{0x3772,0x9b},
	{0x37c0,0x88},
	{0x37c1,0xb8},
	{0x37fa,0x0b},
	{0x37fc,0x10},
	{0x37fd,0x04},
	{0x3902,0xc0},
	{0x3903,0x40},
	{0x3909,0x00},
	{0x391f,0x41},
	{0x3926,0xe0},
	{0x3933,0x80},
	{0x3934,0x02},
	{0x3937,0x6f},
	{0x3e00,0x00},
	{0x3e01,0x95},
	{0x3e02,0x50},
	{0x3e08,0x00},
	{0x4509,0x20},
	{0x450d,0x07},
	{0x4837,0x33},
	{0x5780,0x76},
	{0x5784,0x10},
	{0x5787,0x0a},
	{0x5788,0x0a},
	{0x5789,0x08},
	{0x578a,0x0a},
	{0x578b,0x0a},
	{0x578c,0x08},
	{0x578d,0x40},
	{0x5792,0x04},
	{0x5795,0x04},
	{0x57ac,0x00},
	{0x57ad,0x00},
	{0x3200,0x00},
	{0x3201,0x00},
	{0x3202,0x00},
	{0x3203,0x0c},
	{0x3204,0x07},
	{0x3205,0x87},
	{0x3206,0x04},
	{0x3207,0x33},
	{0x3208,0x07},
	{0x3209,0x80},
	{0x320a,0x04},
	{0x320b,0x20},
	{0x3210,0x00},
	{0x3211,0x04},
	{0x3212,0x00},
	{0x3213,0x04},
	{0x36e9,0x27},
	{0x37f9,0x27},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sc235ai_init_regs_1920_1080_30fps_mipi[] = {
	{0x0103,0x01},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x14},
	{0x3058,0x21},
	{0x3059,0x53},
	{0x305a,0x40},
	{0x320e,0x04},
	{0x320f,0xb0},
	{0x3210,0x00},
	{0x3211,0x04},
	{0x3212,0x00},
	{0x3213,0x04},
	{0x3250,0x00},
	{0x3301,0x0a},
	{0x3302,0x20},
	{0x3304,0x90},
	{0x3305,0x00},
	{0x3306,0x68},
	{0x3309,0xd0},
	{0x330b,0xd8},
	{0x330d,0x08},
	{0x331c,0x04},
	{0x331e,0x81},
	{0x331f,0xc1},
	{0x3323,0x06},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x3364,0x5e},
	{0x336c,0x8e},
	{0x337f,0x13},
	{0x338f,0x80},
	{0x3390,0x08},
	{0x3391,0x18},
	{0x3392,0xb8},
	{0x3393,0x0e},
	{0x3394,0x14},
	{0x3395,0x10},
	{0x3396,0x88},
	{0x3397,0x98},
	{0x3398,0xf8},
	{0x3399,0x0a},
	{0x339a,0x0e},
	{0x339b,0x10},
	{0x339c,0x3c},
	{0x33ae,0x80},
	{0x33af,0xc0},
	{0x33b2,0x50},
	{0x33b3,0x14},
	{0x33f8,0x00},
	{0x33f9,0x68},
	{0x33fa,0x00},
	{0x33fb,0x68},
	{0x33fc,0x48},
	{0x33fd,0x78},
	{0x349f,0x03},
	{0x34a6,0x40},
	{0x34a7,0x58},
	{0x34a8,0x10},
	{0x34a9,0x10},
	{0x34f8,0x78},
	{0x34f9,0x10},
	{0x3619,0x20},
	{0x361a,0x90},
	{0x3633,0x44},
	{0x3637,0x5c},
	{0x363c,0xc0},
	{0x363d,0x02},
	{0x3660,0x80},
	{0x3661,0x81},
	{0x3662,0x8f},
	{0x3663,0x81},
	{0x3664,0x81},
	{0x3665,0x82},
	{0x3666,0x8f},
	{0x3667,0x08},
	{0x3668,0x80},
	{0x3669,0x88},
	{0x366a,0x98},
	{0x366b,0xb8},
	{0x366c,0xf8},
	{0x3670,0xb2},
	{0x3671,0xa2},
	{0x3672,0x88},
	{0x3680,0x33},
	{0x3681,0x33},
	{0x3682,0x43},
	{0x36c0,0x80},
	{0x36c1,0x88},
	{0x36c8,0x88},
	{0x36c9,0xb8},
	{0x36ea,0x0b},
	{0x36eb,0x0c},
	{0x36ec,0x5c},
	{0x36ed,0x04},
	{0x3718,0x04},
	{0x3722,0x8b},
	{0x3724,0xd1},
	{0x3741,0x08},
	{0x3770,0x17},
	{0x3771,0x9b},
	{0x3772,0x9b},
	{0x37c0,0x88},
	{0x37c1,0xb8},
	{0x37fa,0x0b},
	{0x37fc,0x10},
	{0x37fd,0x04},
	{0x3902,0xc0},
	{0x3903,0x40},
	{0x3909,0x00},
	{0x391f,0x41},
	{0x3926,0xe0},
	{0x3933,0x80},
	{0x3934,0x02},
	{0x3937,0x6f},
	{0x3e00,0x00},
	{0x3e01,0x95},
	{0x3e02,0x50},
	{0x3e08,0x00},
	{0x4509,0x20},
	{0x450d,0x07},
	{0x4837,0x33},
	{0x5780,0x76},
	{0x5784,0x10},
	{0x5787,0x0a},
	{0x5788,0x0a},
	{0x5789,0x08},
	{0x578a,0x0a},
	{0x578b,0x0a},
	{0x578c,0x08},
	{0x578d,0x40},
	{0x5792,0x04},
	{0x5795,0x04},
	{0x57ac,0x00},
	{0x57ad,0x00},
	{0x36e9,0x27},
	{0x37f9,0x27},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the sc235ai_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc235ai_win_sizes[] = {
	{
		.width			= 1920,
		.height			= 1056,
		.fps			= 30 << 16 | 1,
		.mbus_code		= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace		= TISP_COLORSPACE_SRGB,
		.regs			= sc235ai_init_regs_1920_1056_30fps_mipi,
	},
	{
		.width			= 1920,
		.height			= 1080,
		.fps			= 30 << 16 | 1,
		.mbus_code		= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace		= TISP_COLORSPACE_SRGB,
		.regs			= sc235ai_init_regs_1920_1080_30fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &sc235ai_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sc235ai_stream_on_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list sc235ai_stream_off_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int sc235ai_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int sc235ai_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int sc235ai_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc235ai_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc235ai_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc235ai_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif	/* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int sc235ai_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int sc235ai_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc235ai_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc235ai_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int sc235ai_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc235ai_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif	/* SENSOR_I2C_REG_16BIT */

static int sc235ai_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int sc235ai_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int sc235ai_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &sc235ai_win_sizes[0];
		sc235ai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sc235ai_attr.max_dgain = 0;
		sc235ai_attr.max_again = 449885;
		sc235ai_attr.min_integration_time = 2;
		sc235ai_attr.max_integration_time = 1200 - 5;
		sc235ai_attr.total_width = 2200;
		sc235ai_attr.total_height = 1200;
		sc235ai_attr.integration_time_apply_delay = 2;
		sc235ai_attr.again_apply_delay = 2;
		sc235ai_attr.dgain_apply_delay = 0;
		sc235ai_attr.integration_time_limit = sc235ai_attr.max_integration_time;
		sc235ai_attr.max_integration_time_native = sc235ai_attr.max_integration_time;
		sc235ai_attr.min_integration_time_native = sc235ai_attr.min_integration_time;
		sc235ai_attr.expo_fs = 1;
#ifdef SENSOR_WDR_2_FRAME
		sc235ai_attr.max_again_short = xxxx;
		sc235ai_attr.min_integration_time_short = xx;
		sc235ai_attr.max_integration_time_short = xx;
		sc235ai_attr.wdr_cache = wdr_line * sc235ai_attr.total_width;
#endif /* SENSOR_WDR_2_FRAME */
		memcpy((void *)(&(sc235ai_attr.mipi)), (void *)(&sc235ai_mipi_1920_1056_30fps_linear), sizeof(sc235ai_attr.mipi));
		break;
	case 1:
		wsize = &sc235ai_win_sizes[1];
		sc235ai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sc235ai_attr.max_dgain = 0;
		sc235ai_attr.max_again = 449885;
		sc235ai_attr.min_integration_time = 2;
		sc235ai_attr.max_integration_time = 1200 - 5;
		sc235ai_attr.total_width = 2200;
		sc235ai_attr.total_height = 1200;
		sc235ai_attr.integration_time_apply_delay = 2;
		sc235ai_attr.again_apply_delay = 2;
		sc235ai_attr.dgain_apply_delay = 0;
		sc235ai_attr.integration_time_limit = sc235ai_attr.max_integration_time;
		sc235ai_attr.max_integration_time_native = sc235ai_attr.max_integration_time;
		sc235ai_attr.min_integration_time_native = sc235ai_attr.min_integration_time;
		sc235ai_attr.expo_fs = 1;
#ifdef SENSOR_WDR_2_FRAME
		sc235ai_attr.max_again_short = xxxx;
		sc235ai_attr.min_integration_time_short = xx;
		sc235ai_attr.max_integration_time_short = xx;
		sc235ai_attr.wdr_cache = wdr_line * sc235ai_attr.total_width;
#endif /* SENSOR_WDR_2_FRAME */
		memcpy((void *)(&(sc235ai_attr.mipi)), (void *)(&sc235ai_mipi_1920_1080_30fps_linear), sizeof(sc235ai_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
	}

	return ret;
}

static int sc235ai_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	sc235ai_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
	case TISP_SENSOR_VI_MIPI_CSI1:
		sc235ai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc235ai_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_DVP:
		sc235ai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = sc235ai_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}

	sc235ai_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int sc235ai_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sc235ai_read(sd, 0x3107, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc235ai_read(sd, 0x3108, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc235ai_g_chip_ident(struct tx_isp_subdev *sd,
								struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sc235ai_attr_check(sd);
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "sc235ai_reset");
		if (!ret) {
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->rst_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
		}
	}
	if (info->pwdn_gpio != -1) {
		ret = private_gpio_request(info->pwdn_gpio, "sc235ai_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = sc235ai_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc235ai chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}

	ISP_WARNING("===================================================\n");
	ISP_WARNING("SC235AI version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", sc235ai_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "sc235ai", sizeof("sc235ai"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc235ai_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sc235ai_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (!init->enable) {
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		return ISP_SUCCESS;
	} else {
		ret = sc235ai_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int sc235ai_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc235ai_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc235ai_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc235ai_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int sc235ai_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
			ret = sc235ai_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sc235ai_write_array(sd, sc235ai_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("sc235ai stream on\n");
		}

	} else {
		ret = sc235ai_write_array(sd, sc235ai_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("sc235ai stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int sc235ai_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	it <<= 1;

	ret = sc235ai_write(sd,  0x3e00, (unsigned char)((it >> 12) & 0x0f));
	ret += sc235ai_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc235ai_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));


	ret += sc235ai_write(sd, 0x3e08, (unsigned char)((again >> 8) & 0xff));
	ret += sc235ai_write(sd, 0x3e09, (unsigned char)(again & 0xff));

	return ret;
}
#else
static int sc235ai_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int sc235ai_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc235ai_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc235ai_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc235ai_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = sc235ai_attr_set(sd, wsize);
	}

	return ret;
}

static int sc235ai_set_fps(struct tx_isp_subdev *sd, int fps)
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
	case 1:
		sclk = 2200 * 1200 * 30;  /**< HTS * VTS * FPS */
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
	ret += sc235ai_read(sd, 0x320c, &val);
	hts = val << 8;
	val = 0;
	ret += sc235ai_read(sd, 0x320d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: sc235ai read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	sc235ai_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	sc235ai_write(sd, 0x320e, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: sc235ai_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 5;
	sensor->video.attr->integration_time_limit = vts - 5;
	sensor->video.attr->max_integration_time_native = vts - 5;
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
static int sc235ai_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = ISP_SUCCESS;
	uint8_t val = 0;

	/* 2'b01:mirror,2'b10:filp */
	val = sc235ai_read(sd, 0x3221, &val);
	switch(enable) {
	case 0:
		val &= 0x99;
		break;
	case 1:
		val = ((val & 0x9F) | 0x06);
		break;
	case 2:
		val = ((val & 0xF9) | 0x60);
		break;
	case 3:
		val |= 0x66;
		break;
	}
	sc235ai_write(sd, 0x3221, val);

	return ret;
}
#endif /* SENSOR_MIR_FLIP */

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int sc235ai_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int sc235ai_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int sc235ai_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc235ai_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = ISP_SUCCESS;

	ret = sc235ai_write_array(sd, sc235ai_stream_off_mipi);
	if (wdr_en == 1) {
		sc235ai_setting_select(sd, 1);
		sc235ai_attr_set(sd, wsize);
	} else if (wdr_en == 0) {
		sc235ai_setting_select(sd, 0);
		sc235ai_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int sc235ai_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = sc235ai_write_array(sd, wsize->regs);
	ret = sc235ai_write_array(sd, sc235ai_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int sc235ai_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	/* struct tx_isp_initarg *init = arg; */

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}

	switch (cmd) {
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = sc235ai_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sc235ai_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sc235ai_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sc235ai_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sc235ai_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sc235ai_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = sc235ai_write_array(sd, sc235ai_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = sc235ai_write_array(sd, sc235ai_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sc235ai_set_fps(sd, sensor_val->value);
		break;
#ifdef SENSOR_MIR_FLIP
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = sc235ai_set_vflip(sd, sensor_val->value);
		break;
#endif /* SENSOR_MIR_FLIP */
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = sc235ai_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = sc235ai_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = sc235ai_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = sc235ai_set_wdr(sd, init->enable);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = sc235ai_set_wdr_stop(sd, init->enable);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return 0;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops sc235ai_core_ops = {
	.g_chip_ident = sc235ai_g_chip_ident,
	.reset = sc235ai_reset,
	.init = sc235ai_init,
	.g_register = sc235ai_g_register,
	.s_register = sc235ai_s_register,
};

static struct tx_isp_subdev_video_ops sc235ai_video_ops = {
	.s_stream = sc235ai_s_stream,
};

static struct tx_isp_subdev_sensor_ops sc235ai_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl	= sc235ai_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops sc235ai_ops = {
	.core = &sc235ai_core_ops,
	.video = &sc235ai_video_ops,
	.sensor = &sc235ai_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
	.name = "sc235ai",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc235ai_probe(struct i2c_client *client,
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

	sensor->video.attr = &sc235ai_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc235ai_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc235ai\n");

	return 0;
}

static int sc235ai_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc235ai_id[] = {
	{"sc235ai", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, sc235ai_id
				   );

static struct i2c_driver sc235ai_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc235ai",
	},
	.probe			= sc235ai_probe,
	.remove			= sc235ai_remove,
	.id_table		= sc235ai_id,
};

static __init int init_sc235ai(void) {
	return private_i2c_add_driver(&sc235ai_driver);
}

static __exit void exit_sc235ai(void) {
	private_i2c_del_driver(&sc235ai_driver);
}

module_init(init_sc235ai);
module_exit(exit_sc235ai);

MODULE_DESCRIPTION("A low-level driver for Sony sc235ai sensor");
MODULE_LICENSE("GPL");
