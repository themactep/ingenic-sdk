/*
* gc6603.c
*
* Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* @Settings:
* sboot        resolution      fps       interface     mode    raw     max_fps     dvdd
*  0           2688*2048        30       mipi_2lane    linear  10        30        1.2V
*  0           2688*2048        15       mipi_2lane    hdr     10        15        1.2V
*
* @I2C addr:0x31, 0x10
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
#define SENSOR_VERSION  "H20250429a"

//#define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT	/**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE	  /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
#define SENSOR_WDR_2_FRAME	/**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP		  /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H	(0x56)
#define SENSOR_CHIP_ID_L	(0x23)
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_MCLK 24000000

#ifdef CONFIG_ZERATUL
#define ZRT_SENSOR_WITHOUT_INIT
#endif

#ifdef SENSOR_WDR_2_FRAME
static int wdr_line = 693;
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

struct tx_isp_sensor_attribute gc6603_attr;

/*
* The part of driver maybe modify about different sensor and different board.
*/
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int index;
	unsigned char reg0914;
	unsigned char reg0915;
	unsigned char reg0225;
	unsigned char reg0e67;
	unsigned char reg0e68;
	unsigned char reg0242;
	unsigned int gain;
};

struct again_lut_hdr {
	unsigned int index;
	unsigned char reg0914;
	unsigned char reg0915;
	unsigned char reg0916;
	unsigned char reg0917;
	unsigned char reg0225;
	unsigned char reg0e67;
	unsigned char reg0e68;
	unsigned char reg0242;
	unsigned int gain;
};

struct again_lut gc6603_again_lut[] = {
	//index, 0914, 0915, 0225, 0e67, 0e68, 0242 gain   |  CG | 实际倍数|Again dB|
	{ 1, 0x01, 0x00, 0x04, 0x0f, 0x0f, 0x65, 0 },//| LCG |   1.000 |  0.000 |
	{ 2, 0x01, 0x05, 0x04, 0x0f, 0x0f, 0x65, 15328 },//| LCG |   1.176 |  1.408 |
	{ 3, 0x21, 0x09, 0x04, 0x0f, 0x0f, 0x65, 29211 },//| LCG |   1.362 |  2.682 |
	{ 4, 0xb1, 0x0C, 0x04, 0x0f, 0x0f, 0x65, 46600 },//| LCG |   1.637 |  4.279 |
	{ 5, 0x01, 0x00, 0x00, 0x0f, 0x0f, 0x65, 67778 },//| HCG |   2.048 |  6.227 |
	{ 6, 0x01, 0x05, 0x00, 0x0f, 0x0f, 0x65, 85684 },//| HCG |   2.475 |  7.871 |
	{ 7, 0x21, 0x09, 0x00, 0x0f, 0x0f, 0x65, 102091 },//| HCG |   2.944 |  9.379 |
	{ 8, 0xb1, 0x0C, 0x00, 0x0f, 0x0f, 0x65, 118203 },//| HCG |   3.491 | 10.859 |
	{ 9, 0x03, 0x00, 0x00, 0x0f, 0x0f, 0x65, 136358 },//| HCG |   4.230 | 12.526 |
	{ 10, 0x03, 0x05, 0x00, 0x10, 0x10, 0x65, 151848 },//| HCG |   4.983 | 13.950 |
	{ 11, 0x23, 0x09, 0x00, 0x11, 0x11, 0x65, 166966 },//| HCG |   5.847 | 15.338 |
	{ 12, 0xb3, 0x0C, 0x00, 0x12, 0x12, 0x65, 183183 },//| HCG |   6.941 | 16.829 |
	{ 13, 0x03, 0x10, 0x00, 0x13, 0x13, 0x65, 200418 },//| HCG |   8.329 | 18.412 |
	{ 14, 0x05, 0x05, 0x00, 0x13, 0x13, 0x65, 217165 },//| HCG |   9.943 | 19.950 |
	{ 15, 0x25, 0x09, 0x00, 0x13, 0x13, 0x65, 233627 },//| HCG |  11.834 | 21.462 |
	{ 16, 0xb5, 0x0C, 0x00, 0x14, 0x14, 0x65, 249167 },//| HCG |  13.948 | 22.890 |
	{ 17, 0x05, 0x10, 0x00, 0x15, 0x15, 0x65, 265937 },//| HCG |  16.655 | 24.431 |
	{ 18, 0x85, 0x12, 0x00, 0x16, 0x16, 0x65, 282129 },//| HCG |  19.766 | 25.918 |
	{ 19, 0x95, 0x14, 0x00, 0x17, 0x17, 0x65, 297470 },//| HCG |  23.248 | 27.328 |
	{ 20, 0x65, 0x16, 0x00, 0x19, 0x19, 0x65, 313585 },//| HCG |  27.568 | 28.808 |
	{ 21, 0x05, 0x18, 0x00, 0x1a, 0x1a, 0x65, 331709 },//| HCG |  33.393 | 30.473 |
	{ 22, 0x05, 0x05, 0x01, 0x1b, 0x1b, 0x65, 345468 },//| HCG |  38.624 | 31.737 |
	{ 23, 0x25, 0x09, 0x01, 0x1c, 0x1c, 0x65, 361848 },//| HCG |  45.930 | 33.242 |
	{ 24, 0xb5, 0x0C, 0x01, 0x1c, 0x1c, 0x75, 378923 },//| HCG |  55.021 | 34.811 |
	{ 25, 0x05, 0x10, 0x01, 0x1d, 0x1d, 0x75, 395519 },//| HCG |  65.578 | 36.335 |
	{ 26, 0x85, 0x12, 0x01, 0x1e, 0x1e, 0x85, 411850 },//| HCG |  77.942 | 37.835 |
	{ 27, 0x95, 0x14, 0x01, 0x1e, 0x1e, 0x85, 427958 },//| HCG |  92.419 | 39.315 |
	{ 28, 0x65, 0x16, 0x01, 0x20, 0x20, 0x85, 443405 },//| HCG | 108.822 | 40.734 |
	{ 29, 0x05, 0x18, 0x01, 0x22, 0x22, 0x85, 460769 },//| HCG | 130.760 | 42.330 |
};

#ifdef SENSOR_WDR_2_FRAME
struct again_lut_hdr gc6603_again_lut_hdr[] = {
	//index,0914, 0915, 0916, 0917, 0225, 0e67, 0e68, 0242 gain    |  CG | 实际倍数 |Again dB|
	{ 1,0x01, 0x00, 0x01, 0x00, 0x0c, 0x0f, 0x0f, 0x65, 0 },//     | LCG |   1.000 |   0.000  |
	{ 2,0x01, 0x00, 0x01, 0x00, 0x00, 0x0f, 0x0f, 0x65, 65536 },// | HCG |   2.000 |   6.021  |
	{ 3,0x03, 0x00, 0x03, 0x00, 0x00, 0x0f, 0x0f, 0x65, 131072 },//| HCG |   4.000 |  12.041  |
	{ 4,0x05, 0x00, 0x05, 0x00, 0x00, 0x12, 0x12, 0x65, 196608 },//| HCG |   8.000 |  18.062  |
	{ 5,0x07, 0x00, 0x07, 0x00, 0x00, 0x16, 0x16, 0x65, 262144 },//| HCG |  16.000 |  24.082  |
	{ 6,0x05, 0x00, 0x05, 0x00, 0x03, 0x1b, 0x1b, 0x65, 327680 },//| HCG |  32.000 |  30.103  |
	{ 7,0x06, 0x00, 0x06, 0x00, 0x03, 0x1d, 0x1d, 0x65, 359493 },//| HCG |  44.800 |  33.026  |
	{ 8,0x07, 0x00, 0x07, 0x00, 0x03, 0x1d, 0x1d, 0x75, 393216 },//| HCG |  64.000 |  36.124  |
};
#endif
#endif /* SENSOR_AGAIN_TABLE */

unsigned int gc6603_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = gc6603_again_lut;
	while (lut->gain <= gc6603_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].index;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == gc6603_attr.again.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
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

unsigned int gc6603_alloc_again_hdr(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut_hdr *lut = gc6603_again_lut_hdr;
	while (lut->gain <= gc6603_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].index;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == gc6603_attr.again.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
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
unsigned int gc6603_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut_hdr *lut = gc6603_again_lut_hdr;
	while(lut->gain <= gc6603_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == gc6603_attr.again_short.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
				return lut->gain;
			}
		}

		lut++;
	}
#else
	/* Non analog gain table */

	...;
#endif /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

	return isp_gain;
}
#endif /* SENSOR_WDR_2_FRAME */

unsigned int gc6603_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int gc6603_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int gc6603_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus gc6603_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1032,
	.lans = 2,
	.image_twidth = 2688,
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

struct tx_isp_mipi_bus gc6603_mipi_hdr = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1032,
	.lans = 2,
	.image_twidth = 2688,
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
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_dvp_bus gc6603_dvp = {
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

struct tx_isp_sensor_attribute gc6603_attr = {
	.name = "gc6603",
	.chip_id = 0x5623,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x31,
	.sensor_ctrl.alloc_again = gc6603_alloc_again,
	.sensor_ctrl.alloc_dgain = gc6603_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = gc6603_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list gc6603_init_regs_2688_2048_30fps_mipi[] = {
//version:v2.2.1
//AEC:release_AEC_v2.1.0_00&01&06_liner_GC6603.txt
//<MODE_1 type="00_GC6603_MIPI_2688x2048_30fps_raw10_2lane_linear_1">
//mclk  24 Mhz
//mipi 1032 Mbps/lane
//vts = 2150
//window 2688×2048
//row time=15.504us
//bayer order  rggb
	{0x03fe,0xf0},
	{0x03fe,0x00},
	{0x03fe,0x10},
	{0x0938,0x01},
	{0x0360,0xfd},
	{0x091b,0x1a},
	{0x091c,0x18},
	{0x091e,0x00},
	{0x091d,0x06},
	{0x091f,0x81},
	{0x0920,0xa1},
	{0x0922,0x3a},
	{0x0923,0x10},
	{0x0928,0x00},
	{0x0934,0xb7},
	{0x0935,0x06},
	{0x0936,0x00},
	{0x0937,0x81},
	{0x031b,0x00},
	{0x031c,0x4f},
	{0x031e,0x00},
	{0x03e0,0x00},
	{0x0314,0x10},
	{0x0219,0x47},
	{0x022b,0x10},
	{0x0259,0x08},
	{0x025a,0x44},
	{0x025b,0x10},
	{0x0340,0x08},
	{0x0341,0x66},
	{0x0342,0x03},
	{0x0343,0xe8},
	{0x0346,0x00},
	{0x0347,0x40},
	{0x0348,0x0a},
	{0x0349,0x90},
	{0x034a,0x08},
	{0x034b,0x20},
	{0x034e,0x0a},
	{0x034f,0xc0},
	{0x070c,0x03},
	{0x070d,0x00},
	{0x070e,0x98},
	{0x070f,0x0a},
	{0x0053,0x05},
	{0x0099,0x10},
	{0x009b,0x08},
	{0x0094,0x0a},
	{0x0095,0x80},
	{0x0096,0x08},
	{0x0097,0x00},
	{0x0e4c,0x3c},
	{0x0902,0x0b},
	{0x0903,0x15},
	{0x0904,0x14},
	{0x0907,0x14},
	{0x0908,0x15},
	{0x090e,0x26},
	{0x090f,0x15},
	{0x0244,0x75},
	{0x0724,0x0c},
	{0x0727,0x0c},
	{0x072a,0x18},
	{0x072b,0x19},
	{0x0709,0x40},
	{0x0719,0x40},
	{0x0912,0x01},
	{0x0913,0x00},
	{0x0e66,0x10},
	{0x0e69,0x80},
	{0x0e6a,0xc0},
	{0x0e6b,0x02},
	{0x0223,0x00},
	{0x0e81,0x02},
	{0x0e30,0x00},
	{0x0e33,0x80},
	{0x0242,0x65},
	{0x0361,0xbc},
	{0x0362,0x0f},
	{0x0e34,0x04},
	{0x0e47,0x55},
	{0x0e61,0x0d},
	{0x0e62,0x0d},
	{0x023a,0x05},
	{0x0e64,0x0c},
	{0x0e20,0x0c},
	{0x0e6e,0x50},
	{0x0e6f,0x58},
	{0x0e70,0x24},
	{0x0e71,0x28},
	{0x0e28,0x38},
	{0x0e4d,0x80},
	{0x0245,0x08},
	{0x0240,0x06},
	{0x0e63,0x06},
	{0x0236,0x02},
	{0x0261,0x60},
	{0x0262,0x28},
	{0x0072,0x00},
	{0x0074,0x01},
	{0x0087,0x53},
	{0x0704,0x07},
	{0x0705,0x28},
	{0x0706,0x02},
	{0x0715,0x28},
	{0x0716,0x02},
	{0x0708,0xc0},
	{0x0718,0xc0},
	{0x0076,0x01},
	{0x021a,0x10},
	{0x0052,0x02},
	{0x0448,0x06},
	{0x0449,0x04},
	{0x044a,0x04},
	{0x044b,0x06},
	{0x044c,0x78},
	{0x044d,0x7a},
	{0x044e,0x7a},
	{0x044f,0x78},
	{0x0046,0x30},
	{0x0002,0xa9},
	{0x0005,0x83},
	{0x0006,0x83},
	{0x001a,0x83},
	{0x0075,0x65},
	{0x0202,0x08},
	{0x0203,0x46},
	{0x0914,0x01},
	{0x0915,0x00},
	{0x0225,0x00},
	{0x0e67,0x0f},
	{0x0e68,0x0f},
	{0x0089,0x03},
	{0x0144,0x00},
	{0x0122,0x08},
	{0x0123,0x27},
	{0x0126,0x0a},
	{0x0129,0x08},
	{0x012a,0x0d},
	{0x012b,0x0a},
	{0x0180,0x46},
	{0x0181,0x30},
	{0x0185,0x01},
	{0x0106,0x38},
	{0x010d,0x0d},
	{0x010e,0x20},
	{0x0111,0x2b},
	{0x0112,0x0a},
	{0x0113,0x0a},
	{0x0114,0x01},
	{0x0221,0x05},
	{0x023b,0x13},
	{0x0352,0x70},
	{0x0357,0x00},
	{0x0b00,0x40},
	{0x08ef,0x01},
	{0x03fe,0x00},
	{0x031f,0x01},
	{0x031f,0x00},
	{0x0318,0x0e},
	{0x0a67,0x80},
	{0x0a50,0x41},
	{0x0a51,0x41},
	{0x0a52,0x41},
	{0x0a54,0x26},
	{0x0a55,0x26},
	{0x0a4e,0x0c},
	{0x0a4f,0x0c},
	{0x0a65,0x17},
	{0x0a53,0x00},
	{0x0a98,0x04},
	{0x05be,0x00},
	{0x05a9,0x01},
	{0x0a67,0x80},
	{0x0023,0x00},
	{0x0025,0x00},
	{0x0028,0x0a},
	{0x0029,0x90},
	{0x002a,0x08},
	{0x002b,0x20},
	{0x0a8b,0x0a},
	{0x0a8a,0x90},
	{0x0a89,0x08},
	{0x0a88,0x20},
	{0x0a70,0x07},
	{0x0a73,0xe0},
	{0x0a80,0x7b},
	{0x0a82,0x00},
	{0x0a83,0x80},
	{0x0a5a,0x80},
	{SENSOR_REG_DELAY, 20},
	{0x05be,0x01},
	{0x0a70,0x00},
	{0x0080,0x02},
	{0x0021,0x40},
	{0x0a67,0x00},
	{0x0100,0x09},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list gc6603_init_regs_2688_2048_15fps_mipi_hdr[] = {
//version:v2.2.1
//AEC：release_AEC_v2.1.0_hdr_GC6603
//<MODE_1 type="0A_GC6603_MIPI2L_24M_2688x2048_15fps_raw10_HDR">
//mclk  24 Mhz
//mipi 1032 Mbps/lane
//vts = 2150
//window 2688×2048
//row time=15.504us
//bayer order  rggb
	{0x03fe,0xf0},
	{0x03fe,0x00},
	{0x03fe,0x10},
	{0x0938,0x01},
	{0x0360,0xfd},
	{0x091b,0x1a},
	{0x091c,0x18},
	{0x091e,0x00},
	{0x091d,0x06},
	{0x091f,0x81},
	{0x0920,0xa1},
	{0x0922,0x3a},
	{0x0923,0x10},
	{0x0928,0x00},
	{0x0934,0xb7},
	{0x0935,0x06},
	{0x0936,0x00},
	{0x0937,0x81},
	{0x031b,0x00},
	{0x031c,0x4f},
	{0x031e,0x00},
	{0x03e0,0x00},
	{0x0314,0x10},
	{0x0219,0x47},
	{0x022b,0x10},
	{0x0259,0x08},
	{0x025a,0x44},
	{0x025b,0x10},
	{0x0340,0x08},
	{0x0341,0x66},
	{0x0342,0x03},
	{0x0343,0xe8},
	{0x0346,0x00},
	{0x0347,0x40},
	{0x0348,0x0a},
	{0x0349,0x90},
	{0x034a,0x08},
	{0x034b,0x20},
	{0x034e,0x0a},
	{0x034f,0xc0},
	{0x070c,0x03},
	{0x070d,0x00},
	{0x070e,0x98},
	{0x070f,0x0a},
	{0x0053,0x05},
	{0x0099,0x10},
	{0x009b,0x08},
	{0x0094,0x0a},
	{0x0095,0x80},
	{0x0096,0x08},
	{0x0097,0x00},
	{0x0e4c,0x3c},
	{0x0902,0x0b},
	{0x0903,0x15},
	{0x0904,0x14},
	{0x0907,0x14},
	{0x0908,0x15},
	{0x090e,0x26},
	{0x090f,0x15},
	{0x0244,0x75},
	{0x0724,0x0c},
	{0x0727,0x0c},
	{0x072a,0x18},
	{0x072b,0x19},
	{0x0709,0x40},
	{0x0719,0x40},
	{0x0912,0x01},
	{0x0913,0x00},
	{0x0e66,0x10},
	{0x0e69,0x80},
	{0x0e6a,0xc0},
	{0x0e6b,0x02},
	{0x0223,0x00},
	{0x0e81,0x02},
	{0x0e30,0x00},
	{0x0e33,0x80},
	{0x0242,0x65},
	{0x0361,0xbc},
	{0x0362,0x0f},
	{0x0e34,0x04},
	{0x0e47,0x55},
	{0x0e61,0x0d},
	{0x0e62,0x0d},
	{0x023a,0x05},
	{0x0e64,0x0c},
	{0x0e20,0x0c},
	{0x0e6e,0x50},
	{0x0e6f,0x58},
	{0x0e70,0x24},
	{0x0e71,0x28},
	{0x0e28,0x38},
	{0x0e4d,0x80},
	{0x0245,0x08},
	{0x0240,0x06},
	{0x0e63,0x06},
	{0x0236,0x02},
	{0x0261,0x60},
	{0x0262,0x28},
	{0x0072,0x00},
	{0x0074,0x01},
	{0x0087,0x53},
	{0x0704,0x07},
	{0x0705,0x28},
	{0x0706,0x02},
	{0x0715,0x28},
	{0x0716,0x02},
	{0x0708,0xc0},
	{0x0718,0xc0},
	{0x0076,0x01},
	{0x021a,0x10},
	{0x0052,0x02},
	{0x0448,0x06},
	{0x0449,0x04},
	{0x044a,0x04},
	{0x044b,0x06},
	{0x044c,0x78},
	{0x044d,0x7a},
	{0x044e,0x7a},
	{0x044f,0x78},
	{0x0046,0x30},
	{0x0002,0xa9},
	{0x0005,0x83},
	{0x0006,0x83},
	{0x001a,0x83},
	{0x0075,0x65},
	{0x0202,0x08},
	{0x0203,0x46},
	{0x0914,0x01},
	{0x0915,0x00},
	{0x0916,0x01},
	{0x0917,0x00},
	{0x0225,0x00},
	{0x0e67,0x0f},
	{0x0e68,0x0f},
	{0x0089,0x03},
	{0x0144,0x00},
	{0x0122,0x08},
	{0x0123,0x27},
	{0x0126,0x0a},
	{0x0129,0x08},
	{0x012a,0x0d},
	{0x012b,0x0a},
	{0x0180,0x46},
	{0x0181,0x30},
	{0x0185,0x01},
	{0x0106,0x38},
	{0x010d,0x0d},
	{0x010e,0x20},
	{0x0111,0x2b},
	{0x0112,0x0a},
	{0x0113,0x0a},
	{0x0114,0x01},
	{0x0100,0x09},
	{0x0221,0x05},
	{0x023b,0x13},
	{0x0352,0x70},
	{0x0357,0x00},
	{0x0b00,0x40},
	{0x0222,0x41},
	{0x0107,0x89},
	{0x0919,0x02},
	{0x023b,0x02},
	{0x0450,0x06},
	{0x0451,0x04},
	{0x0452,0x04},
	{0x0453,0x06},
	{0x0454,0x78},
	{0x0455,0x7a},
	{0x0456,0x7a},
	{0x0457,0x78},
	{0x08ef,0x01},
	{0x03fe,0x00},
	{0x031f,0x01},
	{0x031f,0x00},
	{0x0318,0x0e},
	{0x0a67,0x80},
	{0x0a50,0x41},
	{0x0a51,0x41},
	{0x0a52,0x41},
	{0x0a54,0x26},
	{0x0a55,0x26},
	{0x0a4e,0x0c},
	{0x0a4f,0x0c},
	{0x0a65,0x17},
	{0x0a53,0x00},
	{0x0a98,0x04},
	{0x05be,0x00},
	{0x05a9,0x01},
	{0x0a67,0x80},
	{0x0023,0x00},
	{0x0025,0x00},
	{0x0028,0x0a},
	{0x0029,0x90},
	{0x002a,0x08},
	{0x002b,0x20},
	{0x0a8b,0x0a},
	{0x0a8a,0x90},
	{0x0a89,0x08},
	{0x0a88,0x20},
	{0x0a70,0x07},
	{0x0a73,0xe0},
	{0x0a80,0x7b},
	{0x0a82,0x00},
	{0x0a83,0x80},
	{0x0a5a,0x80},
	{SENSOR_REG_DELAY, 20},
	{0x05be,0x01},
	{0x0a70,0x00},
	{0x0080,0x02},
	{0x0021,0x40},
	{0x0a67,0x00},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
* the order of the gc6603_win_sizes is [full_resolution, preview_resolution].
*/
static struct tx_isp_sensor_win_setting gc6603_win_sizes[] = {
	{
		.width			= 2688,
		.height			= 2048,
		.fps			= 30 << 16 | 1,
		.mbus_code		= TISP_VI_FMT_SRGGB10_1X10,
		.colorspace		= TISP_COLORSPACE_SRGB,
		.regs			= gc6603_init_regs_2688_2048_30fps_mipi,
	},
	{
		.width			= 2688,
		.height			= 2048,
		.fps			= 15 << 16 | 1,
		.mbus_code		= TISP_VI_FMT_SRGGB10_1X10,
		.colorspace		= TISP_COLORSPACE_SRGB,
		.regs			= gc6603_init_regs_2688_2048_15fps_mipi_hdr,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &gc6603_win_sizes[0];

/*
* the part of driver was fixed.
*/

static struct regval_list gc6603_stream_on_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list gc6603_stream_off_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int gc6603_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int gc6603_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int gc6603_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc6603_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int gc6603_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc6603_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif	/* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int gc6603_read(struct tx_isp_subdev *sd, uint16_t reg,unsigned char *value) {
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

int gc6603_write(struct tx_isp_subdev *sd, uint16_t reg,unsigned char value)
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
static int gc6603_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc6603_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int gc6603_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc6603_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif	/* SENSOR_I2C_REG_16BIT */

static int gc6603_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int gc6603_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int gc6603_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &gc6603_win_sizes[0];
		gc6603_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		gc6603_attr.again.value = 65536;
		gc6603_attr.again.max = 460769;
		gc6603_attr.again.min = 0;
		gc6603_attr.again.apply_delay = 2;

		gc6603_attr.integration_time.value = 0x127;
		gc6603_attr.integration_time.max = 0x866 - 8;
		gc6603_attr.integration_time.min = 2;
		gc6603_attr.integration_time.apply_delay = 2;

		gc6603_attr.total_width =0x3e8;
		gc6603_attr.total_height = 0x866;

		gc6603_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		gc6603_attr.hcg.base_gain = ;
		gc6603_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
/* 		gc6603_attr.again_short.value = ;
		gc6603_attr.again_short.max = ;
		gc6603_attr.again_short.min = ;
		gc6603_attr.again_short.apply_delay = ;

		gc6603_attr.integration_time_short.value = ;
		gc6603_attr.integration_time_short.max = ;
		gc6603_attr.integration_time_short.min = ;
		gc6603_attr.integration_time_short.apply_delay = ;

		gc6603_attr.wdr_cache = wdr_line * gc6603_attr.total_width; */

#ifdef SENSOR_HCG
		gc6603_attr.hcg_short.base_gain = ;
		gc6603_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(gc6603_attr.mipi)), (void *)(&gc6603_mipi_linear), sizeof(gc6603_attr.mipi));
		break;
	case 1:
		wsize = &gc6603_win_sizes[1];
		gc6603_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;

		gc6603_attr.again.value = 65536;
		gc6603_attr.again.max = 393216;
		gc6603_attr.again.min = 0;
		gc6603_attr.again.apply_delay = 2;

		gc6603_attr.integration_time.value = 0x127;
		gc6603_attr.integration_time.max = (0x866 - 8) / 17 * 16;
		gc6603_attr.integration_time.min = 8;
		gc6603_attr.integration_time.apply_delay = 2;

		gc6603_attr.total_width =0x3e8;
		gc6603_attr.total_height = 0x866;

		gc6603_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		gc6603_attr.hcg.base_gain = ;
		gc6603_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		gc6603_attr.again_short.value = 65536;
		gc6603_attr.again_short.max = 393216;
		gc6603_attr.again_short.min = 0;
		gc6603_attr.again_short.apply_delay = 2;

		gc6603_attr.integration_time_short.value = 0x20;
		gc6603_attr.integration_time_short.max = (0x866 - 8) / 17;
		gc6603_attr.integration_time_short.min = 8;
		gc6603_attr.integration_time_short.apply_delay = 2;

		gc6603_attr.wdr_cache = wdr_line * gc6603_attr.total_width * 2;

#ifdef SENSOR_HCG
		gc6603_attr.hcg_short.base_gain = ;
		gc6603_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(gc6603_attr.mipi)), (void *)(&gc6603_mipi_hdr), sizeof(gc6603_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int gc6603_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	gc6603_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		gc6603_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc6603_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		gc6603_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc6603_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		gc6603_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = gc6603_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.apply_delay = 1;
	gc6603_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int gc6603_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = gc6603_read(sd, 0x03f2, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = gc6603_read(sd, 0x03f3, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int gc6603_g_chip_ident(struct tx_isp_subdev *sd,
							struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	gc6603_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "gc6603_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "gc6603_pwdn");
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
	ret = gc6603_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc6603 chip.\n",
				client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", gc6603_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "gc6603", sizeof("gc6603"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int gc6603_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int gc6603_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = gc6603_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int gc6603_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = gc6603_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int gc6603_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc6603_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int gc6603_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = gc6603_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = gc6603_write_array(sd, gc6603_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("gc6603 stream on\n");
		}

	} else {
		ret = gc6603_write_array(sd, gc6603_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("gc6603 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int gc6603_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct again_lut *val_lut = gc6603_again_lut;
	struct again_lut_hdr *val_lut_hdr = gc6603_again_lut_hdr;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	/*set integration time*/
	ret = gc6603_write(sd, 0x0203, it & 0xff);
	ret += gc6603_write(sd, 0x0202, it >> 8);

	/*set sensor analog gain*/
	if(info->default_boot == 0){
		ret += gc6603_write(sd, 0x0914, val_lut[again].reg0914);
		ret += gc6603_write(sd, 0x0915, val_lut[again].reg0915);
		ret += gc6603_write(sd, 0x0225, val_lut[again].reg0225);
		ret += gc6603_write(sd, 0x0e67, val_lut[again].reg0e67);
		ret += gc6603_write(sd, 0x0e68, val_lut[again].reg0e68);
		ret += gc6603_write(sd, 0x0242, val_lut[again].reg0242);
	}else{
		ret += gc6603_write(sd, 0x0914, val_lut_hdr[again].reg0914);
		ret += gc6603_write(sd, 0x0915, val_lut_hdr[again].reg0915);
		ret += gc6603_write(sd, 0x0916, val_lut_hdr[again].reg0916);
		ret += gc6603_write(sd, 0x0917, val_lut_hdr[again].reg0917);
		ret += gc6603_write(sd, 0x0225, val_lut_hdr[again].reg0225);
		ret += gc6603_write(sd, 0x0e67, val_lut_hdr[again].reg0e67);
		ret += gc6603_write(sd, 0x0e68, val_lut_hdr[again].reg0e68);
		ret += gc6603_write(sd, 0x0242, val_lut_hdr[again].reg0242);
	}



	return ret;
}
#else
static int gc6603_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	/*set integration time*/
	ret = gc6603_write(sd, 0x0203, value & 0xff);
	ret += gc6603_write(sd, 0x0202, value >> 8);

	return ret;
}

static int gc6603_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	struct again_lut *val_lut = gc6603_again_lut;

	/*set sensor analog gain*/
	ret += gc6603_write(sd, 0x031d ,0x2d);
	ret += gc6603_write(sd, 0x0614, val_lut[again].reg0614);
	ret += gc6603_write(sd, 0x0615, val_lut[again].reg0615);
	ret += gc6603_write(sd, 0x0225, val_lut[again].reg0225);
	ret += gc6603_write(sd, 0x031d, 0x28);
	ret += gc6603_write(sd, 0x1467, val_lut[again].reg1467);
	ret += gc6603_write(sd, 0x1468, val_lut[again].reg1468);
	ret += gc6603_write(sd, 0x00b8, val_lut[again].reg00b8);
	ret += gc6603_write(sd, 0x00b9, val_lut[again].reg00b9);

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc6603_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc6603_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc6603_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = gc6603_attr_set(sd, wsize);
	}

	return ret;
}

static int gc6603_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 0x3e8 * 0x866 * 30 ;  /**< HTS * VTS * FPS */
		max_fps = wsize->fps;
		break;
	case 1:
		sclk = 0x3e8 * 0x866  * 15;  /**< HTS * VTS * FPS */
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
	ret += gc6603_read(sd, 0x0342, &val);
	hts = (val & 0x0f) << 8;
	val = 0;
	ret += gc6603_read(sd, 0x0343, &val);
	hts  = (hts | val);
	if (0 != ret) {
		ISP_ERROR("err: gc6603 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	gc6603_write(sd, 0x0340, (unsigned char) ((vts >> 8) & 0x3f));
	gc6603_write(sd, 0x0341, (unsigned char) (vts & 0xff));

	// gc6603_write(sd, 0x0259, (unsigned char)(((vts - 32) >> 8 ) & 0x3f));
	// gc6603_write(sd, 0x025a, (unsigned char)((vts - 32) & 0xff));

	if (0 != ret) {
		ISP_ERROR("err: gc6603_write err\n");
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
		sensor->video.attr->integration_time.max = vts - 8;
		break;
	case 1:
		sensor->video.attr->total_height = vts;
		sensor->video.attr->integration_time.max = (vts - 8) / 17 * 16;
		sensor->video.attr->integration_time_short.max = (vts - 8) / 17;
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

static int gc6603_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	int ret = ISP_SUCCESS;
	int val = 0;
	int val1 = 0;
	int val2 = 0;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		ret = gc6603_write(sd, 0x0063, val  | 0x00);
		ret = gc6603_write(sd, 0x022c, val1	| 0x00);
		ret = gc6603_write(sd, 0x0722, val2 | 0x60);
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		ret = gc6603_write(sd, 0x0063, val  | 0x01);
		ret = gc6603_write(sd, 0x022c, val1 | 0x01);
		ret = gc6603_write(sd, 0x0722, val2 | 0x02);
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		ret = gc6603_write(sd, 0x0063, val  | 0x02);
		ret = gc6603_write(sd, 0x022c, val1	| 0x02);
		ret = gc6603_write(sd, 0x0722, val2 | 0x00);
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		ret = gc6603_write(sd, 0x0063, val  | 0x03);
		ret = gc6603_write(sd, 0x022c, val1	| 0x03);
		ret = gc6603_write(sd, 0x0722, val2 | 0x02);
		break;
	}

	sensor->video.hvflip_mode = par->hvflip;
	gc6603_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#if 0
static int gc6603_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int gc6603_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	if(value <= 1) value = 1;
	ret = gc6603_write(sd, 0x0201, value & 0xff);
	ret += gc6603_write(sd, 0x0200, value >> 8);
	if (ret < 0)
		ISP_ERROR("gc8613_write error  %d\n" ,__LINE__ );

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc6603_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = gc6603_write_array(sd, gc6603_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		gc6603_setting_select(sd, 1);
		gc6603_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		gc6603_setting_select(sd, 0);
		gc6603_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int gc6603_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	// struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = gc6603_write_array(sd, wsize->regs);
	ret = gc6603_write_array(sd, gc6603_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int gc6603_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	// return 0;
	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}

	switch (cmd) {
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = gc6603_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = gc6603_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = gc6603_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = gc6603_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = gc6603_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = gc6603_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = gc6603_write_array(sd, gc6603_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = gc6603_write_array(sd, gc6603_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = gc6603_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = gc6603_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#if 0
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = gc6603_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = gc6603_set_integration_time_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = gc6603_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = gc6603_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops gc6603_core_ops = {
	.g_chip_ident = gc6603_g_chip_ident,
	.reset = gc6603_reset,
	.init = gc6603_init,
	.g_register = gc6603_g_register,
	.s_register = gc6603_s_register,
};

static struct tx_isp_subdev_video_ops gc6603_video_ops = {
	.s_stream = gc6603_s_stream,
};

static struct tx_isp_subdev_sensor_ops gc6603_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl	= gc6603_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops gc6603_ops = {
	.core = &gc6603_core_ops,
	.video = &gc6603_video_ops,
	.sensor = &gc6603_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "gc6603",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc6603_probe(struct i2c_client *client,
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
	sensor->video.attr = &gc6603_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc6603_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->gc6603\n");

	return 0;
}

static int gc6603_remove(struct i2c_client *client)
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

static const struct i2c_device_id gc6603_id[] = {
	{"gc6603", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, gc6603_id);
#endif	/* CONFIG_ZERATUL */

static struct i2c_driver gc6603_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner	= NULL,
#else
		.owner	= THIS_MODULE,
#endif	/* CONFIG_ZERATUL */
		.name	= "gc6603",
	},
	.probe			= gc6603_probe,
	.remove			= gc6603_remove,
	.id_table		= gc6603_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_gc6603(void) {
	return private_i2c_add_driver(&gc6603_driver);
}

static __exit void exit_gc6603(void) {
	private_i2c_del_driver(&gc6603_driver);
}

module_init(init_gc6603);
module_exit(exit_gc6603);
#endif	/* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_gc6603(void) {
	return private_i2c_add_driver(&gc6603_driver);
}

static void exit_first_gc6603(void) {
	private_i2c_del_driver(&gc6603_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "gc6603",
	.i2c_addr = 0x31,
	.width = 2688,
	.height = 2048,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_gc6603,
	.exit_sensor = exit_first_gc6603
};
#endif	/* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for gc6603 sensor");
MODULE_LICENSE("GPL");
