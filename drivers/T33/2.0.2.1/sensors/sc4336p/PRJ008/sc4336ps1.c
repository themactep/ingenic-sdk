/*
* sc4336ps1.c
*
* Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* @Settings:
* sboot        resolution      fps       interface     mode    raw     max_fps		dvdd               fd_stream_off
*  0           2560*1440       30        mipi_2lane    linear  10        30       interior              support
*  1           1280*720        60        mipi_2lane    linear  10        60       interior              none
*  2           1280*720        60        mipi_2lane    linear  10        60       interior  fast_boot   none
*  3           2560*1440       30        mipi_2lane    linear  10        30       interior  fast_boot   none
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

#define TVERSION "V20241023a"
#define SENSOR_VERSION  "H20251217a"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
/* #define SENSOR_HCG */
#define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x9c)
#define SENSOR_CHIP_ID_L    (0x42)
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

struct tx_isp_sensor_attribute sc4336ps1_attr;

/*
* The part of driver maybe modify about different sensor and different board.
*/
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sc4336ps1_again_lut[] = {
	{0x0080, 0},
	{0x0084, 2886},
	{0x0088, 5776},
	{0x008c, 8494},
	{0x0090, 11136},
	{0x0094, 13706},
	{0x0098, 16287},
	{0x009c, 18723},
	{0x00a0, 21097},
	{0x00a4, 23414},
	{0x00a8, 25746},
	{0x00ac, 27953},
	{0x00b0, 30109},
	{0x00b4, 32217},
	{0x00b8, 34345},
	{0x00bc, 36361},
	{0x00c0, 38336},
	{0x00c4, 40270},
	{0x00c8, 42226},
	{0x00cc, 44082},
	{0x00d0, 45904},
	{0x00d4, 47690},
	{0x00d8, 49500},
	{0x00dc, 51220},
	{0x00e0, 52910},
	{0x00e4, 54571},
	{0x00e8, 56254},
	{0x00ec, 57857},
	{0x00f0, 59433},
	{0x00f4, 60984},
	{0x00f8, 62558},
	{0x00fc, 64059},
	{0x0880, 65536},
	{0x0884, 68422},
	{0x0888, 71312},
	{0x088c, 74030},
	{0x0890, 76672},
	{0x0894, 79242},
	{0x0898, 81823},
	{0x089c, 84259},
	{0x08a0, 86633},
	{0x08a4, 88950},
	{0x08a8, 91282},
	{0x08ac, 93489},
	{0x08b0, 95645},
	{0x08b4, 97753},
	{0x08b8, 99881},
	{0x08bc, 101897},
	{0x08c0, 103872},
	{0x08c4, 105806},
	{0x08c8, 107762},
	{0x08cc, 109618},
	{0x08d0, 111440},
	{0x08d4, 113226},
	{0x08d8, 115036},
	{0x08dc, 116756},
	{0x08e0, 118446},
	{0x08e4, 120107},
	{0x08e8, 121790},
	{0x08ec, 123393},
	{0x08f0, 124969},
	{0x08f4, 126520},
	{0x08f8, 128094},
	{0x08fc, 129595},
	{0x0980, 131072},
	{0x0984, 133958},
	{0x0988, 136848},
	{0x098c, 139566},
	{0x0990, 142208},
	{0x0994, 144778},
	{0x0998, 147359},
	{0x099c, 149795},
	{0x09a0, 152169},
	{0x09a4, 154486},
	{0x09a8, 156818},
	{0x09ac, 159025},
	{0x09b0, 161181},
	{0x09b4, 163289},
	{0x09b8, 165417},
	{0x09bc, 167433},
	{0x09c0, 169408},
	{0x09c4, 171342},
	{0x09c8, 173298},
	{0x09cc, 175154},
	{0x09d0, 176976},
	{0x09d4, 178762},
	{0x09d8, 180572},
	{0x09dc, 182292},
	{0x09e0, 183982},
	{0x09e4, 185643},
	{0x09e8, 187326},
	{0x09ec, 188929},
	{0x09f0, 190505},
	{0x09f4, 192056},
	{0x09f8, 193630},
	{0x09fc, 195131},
	{0x0b80, 196608},
	{0x0b84, 199494},
	{0x0b88, 202384},
	{0x0b8c, 205102},
	{0x0b90, 207744},
	{0x0b94, 210314},
	{0x0b98, 212895},
	{0x0b9c, 215331},
	{0x0ba0, 217705},
	{0x0ba4, 220022},
	{0x0ba8, 222354},
	{0x0bac, 224561},
	{0x0bb0, 226717},
	{0x0bb4, 228825},
	{0x0bb8, 230953},
	{0x0bbc, 232969},
	{0x0bc0, 234944},
	{0x0bc4, 236878},
	{0x0bc8, 238834},
	{0x0bcc, 240690},
	{0x0bd0, 242512},
	{0x0bd4, 244298},
	{0x0bd8, 246108},
	{0x0bdc, 247828},
	{0x0be0, 249518},
	{0x0be4, 251179},
	{0x0be8, 252862},
	{0x0bec, 254465},
	{0x0bf0, 256041},
	{0x0bf4, 257592},
	{0x0bf8, 259166},
	{0x0bfc, 260667},
	{0x0f80, 262144},
	{0x0f84, 265030},
	{0x0f88, 267920},
	{0x0f8c, 270638},
	{0x0f90, 273280},
	{0x0f94, 275850},
	{0x0f98, 278431},
	{0x0f9c, 280867},
	{0x0fa0, 283241},
	{0x0fa4, 285558},
	{0x0fa8, 287890},
	{0x0fac, 290097},
	{0x0fb0, 292253},
	{0x0fb4, 294361},
	{0x0fb8, 296489},
	{0x0fbc, 298505},
	{0x0fc0, 300480},
	{0x0fc4, 302414},
	{0x0fc8, 304370},
	{0x0fcc, 306226},
	{0x0fd0, 308048},
	{0x0fd4, 309834},
	{0x0fd8, 311644},
	{0x0fdc, 313364},
	{0x0fe0, 315054},
	{0x0fe4, 316715},
	{0x0fe8, 318398},
	{0x0fec, 320001},
	{0x0ff0, 321577},
	{0x0ff4, 323128},
	{0x0ff8, 324702},
	{0x0ffc, 326203},
	{0x1f80, 327680},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int sc4336ps1_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc4336ps1_again_lut;
	while (lut->gain <= sc4336ps1_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sc4336ps1_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int sc4336ps1_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc4336ps1_again_lut;
	while(lut->gain <= sc4336ps1_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc4336ps1_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int sc4336ps1_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int sc4336ps1_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int sc4336ps1_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus sc4336ps1_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 630,
	.lans = 2,
	.image_twidth = 2560,
	.image_theight = 1440,
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

struct tx_isp_mipi_bus sc4336ps1_mipi_linear_720P = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 315,
	.lans = 2,
	.image_twidth = 1280,
	.image_theight = 720,
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

struct tx_isp_dvp_bus sc4336ps1_dvp = {
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

struct tx_isp_sensor_attribute sc4336ps1_attr = {
	.name = "sc4336ps1",
	.chip_id = 0x9c42,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.sensor_ctrl.alloc_again = sc4336ps1_alloc_again,
	.sensor_ctrl.alloc_dgain = sc4336ps1_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = sc4336ps1_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list sc4336ps1_init_regs_2560_1440_30fps_mipi[] = {
	//Cleaned_0x05_sc4336ps1_MIPI_24Minput_2lane_10bit_630Mbps_2560x1440_30fps_内供_LDO_1.35v
	{0x0103,0x01},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x05},
	{0x3021,0x07},
	{0x5f00,0x40},
	{0x30b8,0x44},
	{0x320c,0x0a},//hts = 0xaf0 = 2800
	{0x320d,0xf0},//
	{0x320e,0x05},//vts = 0x5dc = 1500
	{0x320f,0xdc},//
	{0x3253,0x10},
	{0x3301,0x0a},
	{0x3302,0xff},
	{0x3305,0x00},
	{0x3306,0x90},
	{0x3308,0x08},
	{0x330a,0x01},
	{0x330b,0xb0},
	{0x330d,0xf0},
	{0x3314,0x14},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x335e,0x06},
	{0x335f,0x0a},
	{0x3364,0x5e},
	{0x337d,0x0e},
	{0x338f,0x20},
	{0x3390,0x08},
	{0x3391,0x09},
	{0x3392,0x0f},
	{0x3393,0x18},
	{0x3394,0x60},
	{0x3395,0xff},
	{0x3396,0x08},
	{0x3397,0x09},
	{0x3398,0x0f},
	{0x3399,0x0a},
	{0x339a,0x18},
	{0x339b,0x60},
	{0x339c,0xff},
	{0x33a2,0x04},
	{0x33ad,0x0c},
	{0x33b2,0x40},
	{0x33b3,0x30},
	{0x33f8,0x00},
	{0x33f9,0xb0},
	{0x33fa,0x00},
	{0x33fb,0xf8},
	{0x33fc,0x09},
	{0x33fd,0x1f},
	{0x349f,0x03},
	{0x34a6,0x09},
	{0x34a7,0x1f},
	{0x34a8,0x28},
	{0x34a9,0x28},
	{0x34aa,0x01},
	{0x34ab,0xe0},
	{0x34ac,0x02},
	{0x34ad,0x28},
	{0x34f8,0x1f},
	{0x34f9,0x20},
	{0x3630,0xc0},
	{0x3631,0x84},
	{0x3632,0x54},
	{0x3633,0x44},
	{0x3637,0x49},
	{0x363f,0xc0},
	{0x3641,0x28},
	{0x3670,0x56},
	{0x3674,0xb0},
	{0x3675,0xa0},
	{0x3676,0xa0},
	{0x3677,0x84},
	{0x3678,0x88},
	{0x3679,0x8d},
	{0x367c,0x09},
	{0x367d,0x0b},
	{0x367e,0x08},
	{0x367f,0x0f},
	{0x3696,0x24},
	{0x3697,0x34},
	{0x3698,0x34},
	{0x36a0,0x0f},
	{0x36a1,0x1f},
	{0x36b0,0x81},
	{0x36b1,0x83},
	{0x36b2,0x85},
	{0x36b3,0x8b},
	{0x36b4,0x09},
	{0x36b5,0x0b},
	{0x36b6,0x0f},
	{0x370f,0x01},
	{0x3722,0x09},
	{0x3724,0x21},
	{0x3771,0x09},
	{0x3772,0x05},
	{0x3773,0x05},
	{0x377a,0x0f},
	{0x377b,0x1f},
	{0x37fa,0x0e},
	{0x37fb,0x31},
	{0x37fc,0x11},
	{0x37fd,0x36},
	{0x3905,0x8c},
	{0x391d,0x02},
	{0x391f,0x49},
	{0x3926,0x21},
	{0x3933,0x80},
	{0x3934,0x03},
	{0x3937,0x7b},
	{0x3939,0x00},
	{0x393a,0x00},
	{0x39dc,0x02},
	{0x3e00,0x00},
	{0x3e01,0x5d},
	{0x3e02,0x40},
	{0x440d,0x10},
	{0x440e,0x01},
	{0x4509,0x28},
	{0x450d,0x32},
	{0x5000,0x06},
	{0x5780,0x76},
	{0x5784,0x10},
	{0x5785,0x04},
	{0x5787,0x0a},
	{0x5788,0x0a},
	{0x5789,0x08},
	{0x578a,0x0a},
	{0x578b,0x0a},
	{0x578c,0x08},
	{0x578d,0x40},
	{0x5790,0x08},
	{0x5791,0x04},
	{0x5792,0x04},
	{0x5793,0x08},
	{0x5794,0x04},
	{0x5795,0x04},
	{0x5799,0x46},
	{0x579a,0x77},
	{0x57a1,0x04},
	{0x57a8,0xd2},
	{0x57aa,0x2a},
	{0x57ab,0x7f},
	{0x57ac,0x00},
	{0x57ad,0x00},
	{0x57d9,0x46},
	{0x57da,0x77},
	{0x59e2,0x08},
	{0x59e3,0x03},
	{0x59e4,0x00},
	{0x59e5,0x10},
	{0x59e6,0x06},
	{0x59e7,0x00},
	{0x59e8,0x08},
	{0x59e9,0x02},
	{0x59ea,0x00},
	{0x59eb,0x10},
	{0x59ec,0x04},
	{0x59ed,0x00},
	{0x5ae0,0xfe},
	{0x5ae1,0x40},
	{0x5ae2,0x38},
	{0x5ae3,0x30},
	{0x5ae4,0x28},
	{0x5ae5,0x38},
	{0x5ae6,0x30},
	{0x5ae7,0x28},
	{0x5ae8,0x3f},
	{0x5ae9,0x34},
	{0x5aea,0x2c},
	{0x5aeb,0x3f},
	{0x5aec,0x34},
	{0x5aed,0x2c},
	{0x36e9,0x53},
	{0x37f9,0x20},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sc4336ps1_init_regs_1280_720_30fps_mipi[] = {
	{0x0103,0x01},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x16},
	{0x30b8,0x44},
	{0x3208,0x05},
	{0x3209,0x00},
	{0x320a,0x02},
	{0x320b,0xd0},
	{0x320c,0x0b},//hts = 0xb15 = 2837
	{0x320d,0x15},//
	{0x320e,0x02},//vts = 0x2e4 = 740
	{0x320f,0xe4},//
	{0x3211,0x02},
	{0x3213,0x02},
	{0x3215,0x31},
	{0x3220,0x01},
	{0x3253,0x10},
	{0x3301,0x0a},
	{0x3302,0xff},
	{0x3305,0x00},
	{0x3306,0x90},
	{0x3308,0x08},
	{0x330a,0x01},
	{0x330b,0xb0},
	{0x330d,0xf0},
	{0x3314,0x14},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x335e,0x06},
	{0x335f,0x0a},
	{0x3364,0x5e},
	{0x337d,0x0e},
	{0x338f,0x20},
	{0x3390,0x08},
	{0x3391,0x09},
	{0x3392,0x0f},
	{0x3393,0x18},
	{0x3394,0x60},
	{0x3395,0xff},
	{0x3396,0x08},
	{0x3397,0x09},
	{0x3398,0x0f},
	{0x3399,0x0a},
	{0x339a,0x18},
	{0x339b,0x60},
	{0x339c,0xff},
	{0x33a2,0x04},
	{0x33ad,0x0c},
	{0x33b2,0x40},
	{0x33b3,0x30},
	{0x33f8,0x00},
	{0x33f9,0xb0},
	{0x33fa,0x00},
	{0x33fb,0xf8},
	{0x33fc,0x09},
	{0x33fd,0x1f},
	{0x349f,0x03},
	{0x34a6,0x09},
	{0x34a7,0x1f},
	{0x34a8,0x28},
	{0x34a9,0x28},
	{0x34aa,0x01},
	{0x34ab,0xe0},
	{0x34ac,0x02},
	{0x34ad,0x28},
	{0x34f8,0x1f},
	{0x34f9,0x20},
	{0x3630,0xc0},
	{0x3631,0x84},
	{0x3632,0x54},
	{0x3633,0x44},
	{0x3637,0x49},
	{0x363f,0xc0},
	{0x3641,0x28},
	{0x3670,0x56},
	{0x3674,0xb0},
	{0x3675,0xa0},
	{0x3676,0xa0},
	{0x3677,0x84},
	{0x3678,0x88},
	{0x3679,0x8d},
	{0x367c,0x09},
	{0x367d,0x0b},
	{0x367e,0x08},
	{0x367f,0x0f},
	{0x3696,0x24},
	{0x3697,0x34},
	{0x3698,0x34},
	{0x36a0,0x0f},
	{0x36a1,0x1f},
	{0x36b0,0x81},
	{0x36b1,0x83},
	{0x36b2,0x85},
	{0x36b3,0x8b},
	{0x36b4,0x09},
	{0x36b5,0x0b},
	{0x36b6,0x0f},
	{0x36ec,0x1c},
	{0x36ed,0x2a},
	{0x370f,0x01},
	{0x3722,0x09},
	{0x3724,0x21},
	{0x3771,0x09},
	{0x3772,0x05},
	{0x3773,0x05},
	{0x377a,0x0f},
	{0x377b,0x1f},
	{0x3905,0x8c},
	{0x391d,0x02},
	{0x391f,0x49},
	{0x3926,0x21},
	{0x3933,0x80},
	{0x3934,0x03},
	{0x3937,0x7b},
	{0x3939,0x00},
	{0x393a,0x00},
	{0x39dc,0x02},
	{0x3e00,0x00},
	{0x3e01,0x2d},
	{0x3e02,0xc0},
	{0x440d,0x10},
	{0x440e,0x01},
	{0x4509,0x28},
	{0x450d,0x32},
	{0x4819,0x05},
	{0x481b,0x03},
	{0x481d,0x09},
	{0x481f,0x02},
	{0x4821,0x08},
	{0x4823,0x02},
	{0x4825,0x02},
	{0x4827,0x02},
	{0x4829,0x04},
	{0x5000,0x46},
	{0x5780,0x76},
	{0x5784,0x10},
	{0x5785,0x04},
	{0x5787,0x0a},
	{0x5788,0x0a},
	{0x5789,0x08},
	{0x578a,0x0a},
	{0x578b,0x0a},
	{0x578c,0x08},
	{0x578d,0x40},
	{0x5790,0x08},
	{0x5791,0x04},
	{0x5792,0x04},
	{0x5793,0x08},
	{0x5794,0x04},
	{0x5795,0x04},
	{0x5799,0x46},
	{0x579a,0x77},
	{0x57a1,0x04},
	{0x57a8,0xd2},
	{0x57aa,0x2a},
	{0x57ab,0x7f},
	{0x57ac,0x00},
	{0x57ad,0x00},
	{0x57d9,0x46},
	{0x57da,0x77},
	{0x59e2,0x08},
	{0x59e3,0x03},
	{0x59e4,0x00},
	{0x59e5,0x10},
	{0x59e6,0x06},
	{0x59e7,0x00},
	{0x59e8,0x08},
	{0x59e9,0x02},
	{0x59ea,0x00},
	{0x59eb,0x10},
	{0x59ec,0x04},
	{0x59ed,0x00},
	{0x5900,0xf1},
	{0x5901,0x04},
	{0x5988,0x70},
	{0x5ae0,0xfe},
	{0x5ae1,0x40},
	{0x5ae2,0x38},
	{0x5ae3,0x30},
	{0x5ae4,0x28},
	{0x5ae5,0x38},
	{0x5ae6,0x30},
	{0x5ae7,0x28},
	{0x5ae8,0x3f},
	{0x5ae9,0x34},
	{0x5aea,0x2c},
	{0x5aeb,0x3f},
	{0x5aec,0x34},
	{0x5aed,0x2c},
	{0x36e9,0x53},
	{0x37f9,0x53},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sc4336ps1_init_regs_1280_720_60fps_mipi_fast[] = {
	{0x0103,0x01},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x16},
	{0x30b8,0x44},
	{0x3208,0x05},
	{0x3209,0x00},
	{0x320a,0x02},
	{0x320b,0xd0},
	{0x320c,0x0b},
	{0x320d,0x15},
	{0x320e,0x02},
	{0x320f,0xe4},
	{0x3211,0x02},
	{0x3213,0x02},
	{0x3215,0x31},
	{0x3220,0x01},
	{0x3253,0x10},
	{0x3301,0x0a},
	{0x3302,0xff},
	{0x3305,0x00},
	{0x3306,0x90},
	{0x3308,0x08},
	{0x330a,0x01},
	{0x330b,0xb0},
	{0x330d,0xf0},
	{0x3314,0x14},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x335e,0x06},
	{0x335f,0x0a},
	{0x3364,0x5e},
	{0x337d,0x0e},
	{0x338f,0x20},
	{0x3390,0x08},
	{0x3391,0x09},
	{0x3392,0x0f},
	{0x3393,0x18},
	{0x3394,0x60},
	{0x3395,0xff},
	{0x3396,0x08},
	{0x3397,0x09},
	{0x3398,0x0f},
	{0x3399,0x0a},
	{0x339a,0x18},
	{0x339b,0x60},
	{0x339c,0xff},
	{0x33a2,0x04},
	{0x33ad,0x0c},
	{0x33b2,0x40},
	{0x33b3,0x30},
	{0x33f8,0x00},
	{0x33f9,0xb0},
	{0x33fa,0x00},
	{0x33fb,0xf8},
	{0x33fc,0x09},
	{0x33fd,0x1f},
	{0x349f,0x03},
	{0x34a6,0x09},
	{0x34a7,0x1f},
	{0x34a8,0x28},
	{0x34a9,0x28},
	{0x34aa,0x01},
	{0x34ab,0xe0},
	{0x34ac,0x02},
	{0x34ad,0x28},
	{0x34f8,0x1f},
	{0x34f9,0x20},
	{0x3630,0xc0},
	{0x3631,0x84},
	{0x3632,0x54},
	{0x3633,0x44},
	{0x3637,0x49},
	{0x363f,0xc0},
	{0x3641,0x28},
	{0x3670,0x56},
	{0x3674,0xb0},
	{0x3675,0xa0},
	{0x3676,0xa0},
	{0x3677,0x84},
	{0x3678,0x88},
	{0x3679,0x8d},
	{0x367c,0x09},
	{0x367d,0x0b},
	{0x367e,0x08},
	{0x367f,0x0f},
	{0x3696,0x24},
	{0x3697,0x34},
	{0x3698,0x34},
	{0x36a0,0x0f},
	{0x36a1,0x1f},
	{0x36b0,0x81},
	{0x36b1,0x83},
	{0x36b2,0x85},
	{0x36b3,0x8b},
	{0x36b4,0x09},
	{0x36b5,0x0b},
	{0x36b6,0x0f},
	{0x36ec,0x1c},
	{0x36ed,0x2a},
	{0x370f,0x01},
	{0x3722,0x09},
	{0x3724,0x21},
	{0x3771,0x09},
	{0x3772,0x05},
	{0x3773,0x05},
	{0x377a,0x0f},
	{0x377b,0x1f},
	{0x3905,0x8c},
	{0x391d,0x02},
	{0x391f,0x49},
	{0x3926,0x21},
	{0x3933,0x80},
	{0x3934,0x03},
	{0x3937,0x7b},
	{0x3939,0x00},
	{0x393a,0x00},
	{0x39dc,0x02},
	{0x3e00,0x00},
	{0x3e01,0x2d},
	{0x3e02,0xc0},
	{0x440d,0x10},
	{0x440e,0x01},
	{0x4509,0x28},
	{0x450d,0x32},
	{0x4819,0x05},
	{0x481b,0x03},
	{0x481d,0x09},
	{0x481f,0x02},
	{0x4821,0x08},
	{0x4823,0x02},
	{0x4825,0x02},
	{0x4827,0x02},
	{0x4829,0x04},
	{0x5000,0x46},
	{0x5780,0x76},
	{0x5784,0x10},
	{0x5785,0x04},
	{0x5787,0x0a},
	{0x5788,0x0a},
	{0x5789,0x08},
	{0x578a,0x0a},
	{0x578b,0x0a},
	{0x578c,0x08},
	{0x578d,0x40},
	{0x5790,0x08},
	{0x5791,0x04},
	{0x5792,0x04},
	{0x5793,0x08},
	{0x5794,0x04},
	{0x5795,0x04},
	{0x5799,0x46},
	{0x579a,0x77},
	{0x57a1,0x04},
	{0x57a8,0xd2},
	{0x57aa,0x2a},
	{0x57ab,0x7f},
	{0x57ac,0x00},
	{0x57ad,0x00},
	{0x57d9,0x46},
	{0x57da,0x77},
	{0x59e2,0x08},
	{0x59e3,0x03},
	{0x59e4,0x00},
	{0x59e5,0x10},
	{0x59e6,0x06},
	{0x59e7,0x00},
	{0x59e8,0x08},
	{0x59e9,0x02},
	{0x59ea,0x00},
	{0x59eb,0x10},
	{0x59ec,0x04},
	{0x59ed,0x00},
	{0x5900,0xf1},
	{0x5901,0x04},
	{0x5988,0x70},
	{0x5ae0,0xfe},
	{0x5ae1,0x40},
	{0x5ae2,0x38},
	{0x5ae3,0x30},
	{0x5ae4,0x28},
	{0x5ae5,0x38},
	{0x5ae6,0x30},
	{0x5ae7,0x28},
	{0x5ae8,0x3f},
	{0x5ae9,0x34},
	{0x5aea,0x2c},
	{0x5aeb,0x3f},
	{0x5aec,0x34},
	{0x5aed,0x2c},
	{0x36e9,0x53},
	{0x37f9,0x53},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sc4336ps1_init_regs_2560_1440_30fps_mipi_fast[] = {
	{0x0103,0x01},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x16},
	{0x30b8,0x44},
	{0x3208,0x05},
	{0x3209,0x00},
	{0x320a,0x02},
	{0x320b,0xd0},
	{0x320c,0x0b},
	{0x320d,0x15},
	{0x320e,0x02},
	{0x320f,0xe4},
	{0x3211,0x02},
	{0x3213,0x02},
	{0x3215,0x31},
	{0x3220,0x01},
	{0x3253,0x10},
	{0x3301,0x0a},
	{0x3302,0xff},
	{0x3305,0x00},
	{0x3306,0x90},
	{0x3308,0x08},
	{0x330a,0x01},
	{0x330b,0xb0},
	{0x330d,0xf0},
	{0x3314,0x14},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x335e,0x06},
	{0x335f,0x0a},
	{0x3364,0x5e},
	{0x337d,0x0e},
	{0x338f,0x20},
	{0x3390,0x08},
	{0x3391,0x09},
	{0x3392,0x0f},
	{0x3393,0x18},
	{0x3394,0x60},
	{0x3395,0xff},
	{0x3396,0x08},
	{0x3397,0x09},
	{0x3398,0x0f},
	{0x3399,0x0a},
	{0x339a,0x18},
	{0x339b,0x60},
	{0x339c,0xff},
	{0x33a2,0x04},
	{0x33ad,0x0c},
	{0x33b2,0x40},
	{0x33b3,0x30},
	{0x33f8,0x00},
	{0x33f9,0xb0},
	{0x33fa,0x00},
	{0x33fb,0xf8},
	{0x33fc,0x09},
	{0x33fd,0x1f},
	{0x349f,0x03},
	{0x34a6,0x09},
	{0x34a7,0x1f},
	{0x34a8,0x28},
	{0x34a9,0x28},
	{0x34aa,0x01},
	{0x34ab,0xe0},
	{0x34ac,0x02},
	{0x34ad,0x28},
	{0x34f8,0x1f},
	{0x34f9,0x20},
	{0x3630,0xc0},
	{0x3631,0x84},
	{0x3632,0x54},
	{0x3633,0x44},
	{0x3637,0x49},
	{0x363f,0xc0},
	{0x3641,0x28},
	{0x3670,0x56},
	{0x3674,0xb0},
	{0x3675,0xa0},
	{0x3676,0xa0},
	{0x3677,0x84},
	{0x3678,0x88},
	{0x3679,0x8d},
	{0x367c,0x09},
	{0x367d,0x0b},
	{0x367e,0x08},
	{0x367f,0x0f},
	{0x3696,0x24},
	{0x3697,0x34},
	{0x3698,0x34},
	{0x36a0,0x0f},
	{0x36a1,0x1f},
	{0x36b0,0x81},
	{0x36b1,0x83},
	{0x36b2,0x85},
	{0x36b3,0x8b},
	{0x36b4,0x09},
	{0x36b5,0x0b},
	{0x36b6,0x0f},
	{0x36ec,0x1c},
	{0x36ed,0x2a},
	{0x370f,0x01},
	{0x3722,0x09},
	{0x3724,0x21},
	{0x3771,0x09},
	{0x3772,0x05},
	{0x3773,0x05},
	{0x377a,0x0f},
	{0x377b,0x1f},
	{0x3905,0x8c},
	{0x391d,0x02},
	{0x391f,0x49},
	{0x3926,0x21},
	{0x3933,0x80},
	{0x3934,0x03},
	{0x3937,0x7b},
	{0x3939,0x00},
	{0x393a,0x00},
	{0x39dc,0x02},
	{0x3e00,0x00},
	{0x3e01,0x2d},
	{0x3e02,0xc0},
	{0x440d,0x10},
	{0x440e,0x01},
	{0x4509,0x28},
	{0x450d,0x32},
	{0x4819,0x05},
	{0x481b,0x03},
	{0x481d,0x09},
	{0x481f,0x02},
	{0x4821,0x08},
	{0x4823,0x02},
	{0x4825,0x02},
	{0x4827,0x02},
	{0x4829,0x04},
	{0x5000,0x46},
	{0x5780,0x76},
	{0x5784,0x10},
	{0x5785,0x04},
	{0x5787,0x0a},
	{0x5788,0x0a},
	{0x5789,0x08},
	{0x578a,0x0a},
	{0x578b,0x0a},
	{0x578c,0x08},
	{0x578d,0x40},
	{0x5790,0x08},
	{0x5791,0x04},
	{0x5792,0x04},
	{0x5793,0x08},
	{0x5794,0x04},
	{0x5795,0x04},
	{0x5799,0x46},
	{0x579a,0x77},
	{0x57a1,0x04},
	{0x57a8,0xd2},
	{0x57aa,0x2a},
	{0x57ab,0x7f},
	{0x57ac,0x00},
	{0x57ad,0x00},
	{0x57d9,0x46},
	{0x57da,0x77},
	{0x59e2,0x08},
	{0x59e3,0x03},
	{0x59e4,0x00},
	{0x59e5,0x10},
	{0x59e6,0x06},
	{0x59e7,0x00},
	{0x59e8,0x08},
	{0x59e9,0x02},
	{0x59ea,0x00},
	{0x59eb,0x10},
	{0x59ec,0x04},
	{0x59ed,0x00},
	{0x5900,0xf1},
	{0x5901,0x04},
	{0x5988,0x70},
	{0x5ae0,0xfe},
	{0x5ae1,0x40},
	{0x5ae2,0x38},
	{0x5ae3,0x30},
	{0x5ae4,0x28},
	{0x5ae5,0x38},
	{0x5ae6,0x30},
	{0x5ae7,0x28},
	{0x5ae8,0x3f},
	{0x5ae9,0x34},
	{0x5aea,0x2c},
	{0x5aeb,0x3f},
	{0x5aec,0x34},
	{0x5aed,0x2c},
	{0x36e9,0x53},
	{0x37f9,0x53},
	// {0x0100,0x01},
	{SENSOR_REG_DELAY, 0x05},
	{0x3208,0x0a},
	{0x3209,0x00},
	{0x320a,0x05},
	{0x320b,0xa0},
	{0x320c,0x0a},
	{0x320d,0xb0},
	{0x320e,0x05},
	{0x320f,0xff},
	{0x3211,0x04},
	{0x3213,0x04},
	{0x3215,0x11},
	{0x3220,0x00},
	{0x36ec,0x0c},
	{0x36ed,0xaa},
	{0x5000,0x06},
	{0x5900,0x01},
	{0x5901,0x00},
	{0x5988,0x84},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
* the order of the sc4336ps1_win_sizes is [full_resolution, preview_resolution].
*/
static struct tx_isp_sensor_win_setting sc4336ps1_win_sizes[] = {
	{
		.width          = 2560,
		.height         = 1440,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc4336ps1_init_regs_2560_1440_30fps_mipi,
	},
	{
		.width          = 1280,
		.height         = 720,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc4336ps1_init_regs_1280_720_30fps_mipi,
	},
	{
		.width          = 1280,
		.height         = 720,
		.fps            = 60 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc4336ps1_init_regs_1280_720_60fps_mipi_fast,
	},
	{
		.width          = 2560,
		.height         = 1440,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc4336ps1_init_regs_2560_1440_30fps_mipi_fast,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &sc4336ps1_win_sizes[0];

/*
* the part of driver was fixed.
*/

static struct regval_list sc4336ps1_stream_on_mipi[] = {
	{0x0100, 0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list sc4336ps1_stream_off_mipi[] = {
	{0x0100, 0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int sc4336ps1_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int sc4336ps1_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int sc4336ps1_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc4336ps1_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc4336ps1_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc4336ps1_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int sc4336ps1_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int sc4336ps1_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc4336ps1_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc4336ps1_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int sc4336ps1_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc4336ps1_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int sc4336ps1_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = ISP_SUCCESS;

	rate = private_clk_get_rate(sensor->mclk);
	if(((rate / 1000) % (mclk / 1000)) != 0) {
		switch(mclk) {
		case 24000000:
			ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
			break;
		case 27000000:
		case 37125000:
			ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK1));
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

	private_clk_set_rate(sensor->mclk, mclk);
	private_clk_prepare_enable(sensor->mclk);

error:
	return ret;
}

static int sc4336ps1_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int sc4336ps1_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &sc4336ps1_win_sizes[0];
		sc4336ps1_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc4336ps1_attr.again.value = 65536;
		sc4336ps1_attr.again.max = 327680;
		sc4336ps1_attr.again.min = 0;
		sc4336ps1_attr.again.apply_delay = 2;

		sc4336ps1_attr.integration_time.value = 0x5d4;
		sc4336ps1_attr.integration_time.max = 1500 - 8;
		sc4336ps1_attr.integration_time.min = 2;
		sc4336ps1_attr.integration_time.apply_delay = 2;

		sc4336ps1_attr.total_width = 2800;
		sc4336ps1_attr.total_height = 1500;

		sc4336ps1_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
		sc4336ps1_attr.hcg.base_gain = ;
		sc4336ps1_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc4336ps1_attr.again_short.value = ;
		sc4336ps1_attr.again_short.max = ;
		sc4336ps1_attr.again_short.min = ;
		sc4336ps1_attr.again_short.apply_delay = ;

		sc4336ps1_attr.integration_time_short.value = ;
		sc4336ps1_attr.integration_time_short.max = ;
		sc4336ps1_attr.integration_time_short.min = ;
		sc4336ps1_attr.integration_time_short.apply_delay = ;

		sc4336ps1_attr.wdr_cache = wdr_line * sc4336ps1_attr.total_width;

#ifdef SENSOR_HCG
		sc4336ps1_attr.hcg_short.base_gain = ;
		sc4336ps1_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(sc4336ps1_attr.mipi)), (void *)(&sc4336ps1_mipi_linear), sizeof(sc4336ps1_attr.mipi));
		break;
	case 1:
		wsize = &sc4336ps1_win_sizes[1];
		sc4336ps1_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc4336ps1_attr.again.value = 65536;
		sc4336ps1_attr.again.max = 327680;
		sc4336ps1_attr.again.min = 0;
		sc4336ps1_attr.again.apply_delay = 2;

		sc4336ps1_attr.integration_time.value = 0x2dc;
		sc4336ps1_attr.integration_time.max = 740 - 8;
		sc4336ps1_attr.integration_time.min = 2;
		sc4336ps1_attr.integration_time.apply_delay = 2;

		sc4336ps1_attr.total_width = 2837;
		sc4336ps1_attr.total_height = 740;

		sc4336ps1_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
		sc4336ps1_attr.hcg.base_gain = ;
		sc4336ps1_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc4336ps1_attr.again_short.value = ;
		sc4336ps1_attr.again_short.max = ;
		sc4336ps1_attr.again_short.min = ;
		sc4336ps1_attr.again_short.apply_delay = ;

		sc4336ps1_attr.integration_time_short.value = ;
		sc4336ps1_attr.integration_time_short.max = ;
		sc4336ps1_attr.integration_time_short.min = ;
		sc4336ps1_attr.integration_time_short.apply_delay = ;

		sc4336ps1_attr.wdr_cache = wdr_line * sc4336ps1_attr.total_width;

#ifdef SENSOR_HCG
		sc4336ps1_attr.hcg_short.base_gain = ;
		sc4336ps1_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(sc4336ps1_attr.mipi)), (void *)(&sc4336ps1_mipi_linear), sizeof(sc4336ps1_attr.mipi));
		sc4336ps1_attr.mipi.clk = 315;
		sc4336ps1_attr.mipi.image_twidth = 1280;
		sc4336ps1_attr.mipi.image_theight = 720;
		break;
	case 2:
		wsize = &sc4336ps1_win_sizes[2];
		sc4336ps1_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc4336ps1_attr.again.value = 65536;
		sc4336ps1_attr.again.max = 327680;
		sc4336ps1_attr.again.min = 0;
		sc4336ps1_attr.again.apply_delay = 2;

		sc4336ps1_attr.integration_time.value = 740 - 8;
		sc4336ps1_attr.integration_time.max = 740 - 8;
		sc4336ps1_attr.integration_time.min = 2;
		sc4336ps1_attr.integration_time.apply_delay = 2;

		sc4336ps1_attr.total_width = 2837;
		sc4336ps1_attr.total_height = 740;

		sc4336ps1_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
		sc4336ps1_attr.hcg.base_gain = ;
		sc4336ps1_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc4336ps1_attr.again_short.value = ;
		sc4336ps1_attr.again_short.max = ;
		sc4336ps1_attr.again_short.min = ;
		sc4336ps1_attr.again_short.apply_delay = ;

		sc4336ps1_attr.integration_time_short.value = ;
		sc4336ps1_attr.integration_time_short.max = ;
		sc4336ps1_attr.integration_time_short.min = ;
		sc4336ps1_attr.integration_time_short.apply_delay = ;

		sc4336ps1_attr.wdr_cache = wdr_line * sc4336ps1_attr.total_width;

#ifdef SENSOR_HCG
		sc4336ps1_attr.hcg_short.base_gain = ;
		sc4336ps1_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(sc4336ps1_attr.mipi)), (void *)(&sc4336ps1_mipi_linear), sizeof(sc4336ps1_attr.mipi));
		sc4336ps1_attr.mipi.clk = 315;
		sc4336ps1_attr.mipi.image_twidth = 1280;
		sc4336ps1_attr.mipi.image_theight = 720;
		break;
	case 3:
		wsize = &sc4336ps1_win_sizes[3];
		sc4336ps1_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc4336ps1_attr.again.value = 65536;
		sc4336ps1_attr.again.max = 327680;
		sc4336ps1_attr.again.min = 0;
		sc4336ps1_attr.again.apply_delay = 2;

		sc4336ps1_attr.integration_time.value = 1535 - 8;
		sc4336ps1_attr.integration_time.max = 1535 - 8;
		sc4336ps1_attr.integration_time.min = 2;
		sc4336ps1_attr.integration_time.apply_delay = 2;

		sc4336ps1_attr.total_width = 2736;
		sc4336ps1_attr.total_height = 1535;

		sc4336ps1_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
		sc4336ps1_attr.hcg.base_gain = ;
		sc4336ps1_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc4336ps1_attr.again_short.value = ;
		sc4336ps1_attr.again_short.max = ;
		sc4336ps1_attr.again_short.min = ;
		sc4336ps1_attr.again_short.apply_delay = ;

		sc4336ps1_attr.integration_time_short.value = ;
		sc4336ps1_attr.integration_time_short.max = ;
		sc4336ps1_attr.integration_time_short.min = ;
		sc4336ps1_attr.integration_time_short.apply_delay = ;

		sc4336ps1_attr.wdr_cache = wdr_line * sc4336ps1_attr.total_width;

#ifdef SENSOR_HCG
		sc4336ps1_attr.hcg_short.base_gain = ;
		sc4336ps1_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(sc4336ps1_attr.mipi)), (void *)(&sc4336ps1_mipi_linear), sizeof(sc4336ps1_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int sc4336ps1_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	sc4336ps1_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		sc4336ps1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc4336ps1_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		sc4336ps1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc4336ps1_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		sc4336ps1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = sc4336ps1_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.apply_delay = 1;
	sc4336ps1_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int sc4336ps1_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sc4336ps1_read(sd, 0x3107, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc4336ps1_read(sd, 0x3108, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc4336ps1_g_chip_ident(struct tx_isp_subdev *sd,
								struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sc4336ps1_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "sc4336ps1_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "sc4336ps1_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = sc4336ps1_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc4336ps1 chip.\n",
				client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", sc4336ps1_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "sc4336ps1", sizeof("sc4336ps1"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc4336ps1_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sc4336ps1_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = sc4336ps1_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int sc4336ps1_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc4336ps1_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc4336ps1_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc4336ps1_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int sc4336ps1_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = sc4336ps1_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sc4336ps1_write_array(sd, sc4336ps1_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("sc4336ps1 stream on\n");
		}

	} else {
		ret = sc4336ps1_write_array(sd, sc4336ps1_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("sc4336ps1 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int sc4336ps1_set_expo(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;
	uint32_t expo_time = 0;
	uint8_t state_reg = 0;

	expo_time = 1000 * it * (sensor->video.fps.value & 0xffff) /((sensor->video.fps.value >> 16) & 0xffff)/sc4336ps1_attr.total_height;

	if (expo_time < 5) {
		ret += sc4336ps1_write(sd, 0x3812, 0x00);
		ret += sc4336ps1_write(sd, 0x338f, 0xa0);
		ret += sc4336ps1_write(sd, 0x3812, 0x30);
	} else if (expo_time < 10) {
		ret += sc4336ps1_read(sd, 0x338f, &state_reg);
		ret += sc4336ps1_write(sd, 0x3812, 0x00);
		ret += sc4336ps1_write(sd, 0x338f, state_reg);
		ret += sc4336ps1_write(sd, 0x3812, 0x30);

	} else {
		ret += sc4336ps1_write(sd, 0x3812, 0x00);
		ret += sc4336ps1_write(sd, 0x338f, 0x20);
		ret += sc4336ps1_write(sd, 0x3812, 0x30);
	}

	ret += sc4336ps1_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc4336ps1_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc4336ps1_write(sd, 0x3e02, (unsigned char)((it & 0xf) << 4));
	ret += sc4336ps1_write(sd, 0x3e09, (unsigned char)((again >> 8) & 0xff));
	ret += sc4336ps1_write(sd, 0x3e07, (unsigned char)(again & 0xff));

	return ret;
}
#else
static int sc4336ps1_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;
	uint32_t expo_time = 0;
	uint8_t state_reg = 0;

	expo_time = 1000 * it * (sensor->video.fps.value & 0xffff) /((sensor->video.fps.value >> 16) & 0xffff)/sc4336ps1_attr.total_height;

	if (expo_time < 5) {
		ret += sc4336ps1_write(sd, 0x3812, 0x00);
		ret += sc4336ps1_write(sd, 0x338f, 0xa0);
		ret += sc4336ps1_write(sd, 0x3812, 0x30);
	} else if (expo_time < 10) {
		ret += sc4336ps1_read(sd, 0x338f, &state_reg);
		ret += sc4336ps1_write(sd, 0x3812, 0x00);
		ret += sc4336ps1_write(sd, 0x338f, state_reg);
		ret += sc4336ps1_write(sd, 0x3812, 0x30);

	} else {
		ret += sc4336ps1_write(sd, 0x3812, 0x00);
		ret += sc4336ps1_write(sd, 0x338f, 0x20);
		ret += sc4336ps1_write(sd, 0x3812, 0x30);
	}

	ret += sc4336ps1_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc4336ps1_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc4336ps1_write(sd, 0x3e02, (unsigned char)((value & 0xf) << 4));

	return ret;
}

static int sc4336ps1_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	ret += sc4336ps1_write(sd, 0x3e09, (unsigned char)((value >> 8) & 0xff));
	ret += sc4336ps1_write(sd, 0x3e07, (unsigned char)(value & 0xff));

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc4336ps1_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc4336ps1_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc4336ps1_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = sc4336ps1_attr_set(sd, wsize);
	}

	return ret;
}

static int sc4336ps1_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 2800 * 1500 * 30;  /**< HTS * VTS * FPS */
		max_fps = 30;
		break;
	case 1:
		sclk = 2837 * 740 * 60;  /**< HTS * VTS * FPS */
		max_fps = 60;
		break;
	case 2:
		sclk = 2837 * 740 * 60;  /**< HTS * VTS * FPS */
		max_fps = 60;
		break;
	case 3:
		sclk = 2736 * 1535 * 30;  /**< HTS * VTS * FPS */
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
	ret += sc4336ps1_read(sd, 0x320c, &val);
	hts = val << 8;
	val = 0;
	ret += sc4336ps1_read(sd, 0x320d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: sc4336ps1 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	sc4336ps1_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	sc4336ps1_write(sd, 0x320e, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: sc4336ps1_write err\n");
		return ret;
	}

	/* sensor->video.fps = fps; */
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

static int sc4336ps1_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	ret = sc4336ps1_read(sd, 0x3221, &val);
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		val &= 0x99;
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		val = ((val & 0x99) | 0x06);
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		val = ((val & 0x99) | 0x60);
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		val |= 0x66;
		break;
	}
	ret += sc4336ps1_write(sd, 0x3221, val);

	sensor->video.hvflip_mode = par->hvflip;
	sc4336ps1_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int sc4336ps1_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int sc4336ps1_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int sc4336ps1_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc4336ps1_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = sc4336ps1_write_array(sd, sc4336ps1_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		sc4336ps1_setting_select(sd, 1);
		sc4336ps1_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		sc4336ps1_setting_select(sd, 0);
		sc4336ps1_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int sc4336ps1_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = sc4336ps1_write_array(sd, wsize->regs);
	ret = sc4336ps1_write_array(sd, sc4336ps1_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int sc4336ps1_pm_s_stream_ctrl(struct tx_isp_subdev *sd, void *arg)
{
	int ret = ISP_SUCCESS;
	IMPISPUserCtl *data = (IMPISPUserCtl *)arg;
	if(data->cmd == TX_SENSOR_PM_RESUME){
		printk("%s TX_SENSOR_PM_RESUME\n",sc4336ps1_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_SUSPEND) {
		ret = sc4336ps1_write_array(sd, sc4336ps1_stream_off_mipi);
		if (ret < 0) {
			printk("\n TX_SENSOR_PM_SUSPEND error !!!\n");
			return ret;
		}
		printk("%s TX_SENSOR_PM_SUSPEND.\n",sc4336ps1_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_PREPARE) {
		printk("%s TX_SENSOR_PM_PREPARE.\n",sc4336ps1_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_COMPLETE) {
		printk("%s TX_SENSOR_PM_COMPLETE.\n",sc4336ps1_attr.name);

	}else if(data->cmd == TX_SENSOR_PM_STREAMON) {
		ret = sc4336ps1_write_array(sd, sc4336ps1_stream_on_mipi);
		if (ret < 0) {
			printk("\n TX_SENSOR_PM_STREAMON error !!!\n");
			return ret;
		}
		printk("%s TX_SENSOR_PM_STREAMON.\n",sc4336ps1_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_STREAMOFF) {
		ret = sc4336ps1_write_array(sd, sc4336ps1_stream_off_mipi);
		if (ret < 0) {
			printk("\n [%s %d] TX_SENSOR_PM_STREAMOFF error !!!\n", __func__, __LINE__);
			return ret;
		}
		printk("%s TX_SENSOR_PM_STREAMOFF.\n",sc4336ps1_attr.name);
	}else {
		printk("[%s]Don't Support this function.\n",__func__);
		return -EINVAL;
	}

	return ret;
}

static int sc4336ps1_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = sc4336ps1_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sc4336ps1_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sc4336ps1_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sc4336ps1_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sc4336ps1_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sc4336ps1_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = sc4336ps1_write_array(sd, sc4336ps1_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = sc4336ps1_write_array(sd, sc4336ps1_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sc4336ps1_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = sc4336ps1_set_hvflip(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_PM_S_STREAM_CTRL:
		if(arg)
			ret = sc4336ps1_pm_s_stream_ctrl(sd, arg);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = sc4336ps1_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = sc4336ps1_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = sc4336ps1_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = sc4336ps1_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = sc4336ps1_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops sc4336ps1_core_ops = {
	.g_chip_ident = sc4336ps1_g_chip_ident,
	.reset = sc4336ps1_reset,
	.init = sc4336ps1_init,
	.g_register = sc4336ps1_g_register,
	.s_register = sc4336ps1_s_register,
};

static struct tx_isp_subdev_video_ops sc4336ps1_video_ops = {
	.s_stream = sc4336ps1_s_stream,
};

static struct tx_isp_subdev_sensor_ops sc4336ps1_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = sc4336ps1_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops sc4336ps1_ops = {
	.core = &sc4336ps1_core_ops,
	.video = &sc4336ps1_video_ops,
	.sensor = &sc4336ps1_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "sc4336ps1",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc4336ps1_probe(struct i2c_client *client,
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
	sensor->video.attr = &sc4336ps1_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc4336ps1_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc4336ps1\n");

	return 0;
}

static int sc4336ps1_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc4336ps1_id[] = {
	{"sc4336ps1", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, sc4336ps1_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver sc4336ps1_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "sc4336ps1",
	},
	.probe          = sc4336ps1_probe,
	.remove         = sc4336ps1_remove,
	.id_table       = sc4336ps1_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_sc4336ps1(void) {
	return private_i2c_add_driver(&sc4336ps1_driver);
}

static __exit void exit_sc4336ps1(void) {
	private_i2c_del_driver(&sc4336ps1_driver);
}

module_init(init_sc4336ps1);
module_exit(exit_sc4336ps1);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_sc4336ps1(void) {
	return private_i2c_add_driver(&sc4336ps1_driver);
}

static void exit_first_sc4336ps1(void) {
	private_i2c_del_driver(&sc4336ps1_driver);
}

struct tx_isp_sensor_fast_attr sensor1 = {
	.name = "sc4336ps1",
	.i2c_addr = 0x30,
	.width = 2560,
	.height = 1440,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_sc4336ps1,
	.exit_sensor = exit_first_sc4336ps1
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for Sony sc4336ps1 sensor");
MODULE_LICENSE("GPL");
