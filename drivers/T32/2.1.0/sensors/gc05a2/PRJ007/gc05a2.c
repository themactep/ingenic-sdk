/*
 * gc05a2.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps        dvdd
 *  0           2560*1440       30        mipi_2lane    linear  10        30           null

 * @I2C addr:0x37,0x3f
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

#define TVERSION "VH20241105a"
#define SENSOR_VERSION  "H20250714a"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x05)
#define SENSOR_CHIP_ID_L    (0xa2)
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

struct tx_isp_sensor_attribute gc05a2_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut gc05a2_again_lut[] = {
	{0x0400, 0},
	{0x0440, 5731},
	{0x0480, 11136},
	{0x04c0, 16248},
	{0x0500, 21097},
	{0x0540, 25710},
	{0x0580, 30109},
	{0x05c0, 34312},
	{0x0600, 38336},
	{0x0640, 42195},
	{0x0680, 45904},
	{0x06c0, 49472},
	{0x0700, 52910},
	{0x0740, 56228},
	{0x0780, 59433},
	{0x07c0, 62534},
	{0x0800, 65536},
	{0x0849, 68847},
	{0x0892, 72046},
	{0x08db, 75141},
	{0x0924, 78138},
	{0x096d, 81042},
	{0x09b6, 83860},
	{0x09ff, 86596},
	{0x0a48, 89256},
	{0x0a91, 91842},
	{0x0ada, 94360},
	{0x0b23, 96813},
	{0x0b6c, 99203},
	{0x0bb5, 101535},
	{0x0bfe, 103810},
	{0x0c47, 106032},
	{0x0c90, 108203},
	{0x0cd9, 110325},
	{0x0d22, 112401},
	{0x0d6b, 114432},
	{0x0db4, 116420},
	{0x0dfd, 118367},
	{0x0e46, 120275},
	{0x0e8f, 122145},
	{0x0ed8, 123979},
	{0x0f21, 125779},
	{0x0f6a, 127544},
	{0x0fb3, 129277},
	{0x0ffc, 130979},
	{0x1045, 132651},
	{0x109a, 134561},
	{0x10ef, 136433},
	{0x1144, 138269},
	{0x1199, 140070},
	{0x11ee, 141838},
	{0x1243, 143573},
	{0x1298, 145276},
	{0x12ed, 146950},
	{0x1342, 148594},
	{0x1397, 150210},
	{0x13ec, 151799},
	{0x1441, 153362},
	{0x1496, 154900},
	{0x14eb, 156412},
	{0x1540, 157901},
	{0x1595, 159367},
	{0x15ea, 160811},
	{0x163f, 162233},
	{0x1694, 163633},
	{0x16e9, 165014},
	{0x173e, 166374},
	{0x1793, 167715},
	{0x17e8, 169038},
	{0x183d, 170342},
	{0x18a3, 171883},
	{0x1909, 173400},
	{0x196f, 174893},
	{0x19d5, 176363},
	{0x1a3b, 177810},
	{0x1aa1, 179235},
	{0x1b07, 180640},
	{0x1b6d, 182023},
	{0x1bd3, 183387},
	{0x1c39, 184731},
	{0x1c9f, 186057},
	{0x1d05, 187364},
	{0x1d6b, 188653},
	{0x1dd1, 189925},
	{0x1e37, 191180},
	{0x1e9d, 192419},
	{0x1f03, 193641},
	{0x1f69, 194848},
	{0x1fcf, 196040},
	{0x2035, 197217},
	{0x20b5, 198674},
	{0x2135, 200108},
	{0x21b5, 201521},
	{0x2235, 202913},
	{0x22b5, 204285},
	{0x2335, 205638},
	{0x23b5, 206971},
	{0x2435, 208286},
	{0x24b5, 209583},
	{0x2535, 210862},
	{0x25b5, 212124},
	{0x2635, 213369},
	{0x26b5, 214599},
	{0x2735, 215812},
	{0x27b5, 217010},
	{0x2835, 218193},
	{0x28df, 219742},
	{0x2989, 221266},
	{0x2a33, 222766},
	{0x2add, 224242},
	{0x2b87, 225696},
	{0x2c31, 227127},
	{0x2cdb, 228537},
	{0x2d85, 229927},
	{0x2e2f, 231296},
	{0x2ed9, 232646},
	{0x2f83, 233977},
	{0x302d, 235289},
	{0x312d, 237232},
	{0x322d, 239135},
	{0x332d, 241001},
	{0x342d, 242831},
	{0x352d, 244626},
	{0x362d, 246387},
	{0x372d, 248116},
	{0x382d, 249815},
	{0x3a2d, 253122},
	{0x3c2d, 256318},
	{0x3e2d, 259409},
	{0x4000, 262144},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int gc05a2_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = gc05a2_again_lut;
	while (lut->gain <= gc05a2_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == gc05a2_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int gc05a2_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = gc05a2_again_lut;
	while(lut->gain <= gc05a2_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == gc05a2_attr.again_short.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
				return lut->gain;
			}
		}
		lut++;
	}
#else
	/* Non analog gain table */
#endif /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

	return isp_gain;
}
#endif /* SENSOR_WDR_2_FRAME */

unsigned int gc05a2_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int gc05a2_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int gc05a2_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus gc05a2_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 896,
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

struct tx_isp_dvp_bus gc05a2_dvp = {
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

struct tx_isp_sensor_attribute gc05a2_attr = {
	.name = "gc05a2",
	.chip_id = 0x08a8,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x29,
	.sensor_ctrl.alloc_again = gc05a2_alloc_again,
	.sensor_ctrl.alloc_dgain = gc05a2_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = gc05a2_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list gc05a2_init_regs_2560_1440_30fps_mipi[] = {
	/*system*/
	{0x0315,0xd4},
	{0x0d06,0x01},
	{0x0a70,0x80},
	{0x031a,0x00},
	{0x0314,0x00},
	{0x0130,0x08},
	{0x0132,0x01},
	{0x0135,0x01},
	{0x0136,0x38},
	{0x0137,0x03},
	{0x0134,0x5b},
	{0x031c,0xe0},
	{0x0d82,0x14},
	{0x0dd1,0x56},
	/*gate_mode*/
	{0x0af4,0x01},
	{0x0002,0x10},
	{0x00c3,0x34},
	/*pre_setting*/
	{0x0084,0x21},
	{0x0d05,0xcc},
	{0x0218,0x00},
	{0x005e,0x48},
	{0x0d06,0x01},
	{0x0007,0x16},
	{0x0101,0x00},
	/*analog*/
	{0x0342,0x07},
	{0x0343,0x28},
	{0x0220,0x07},
	{0x0221,0xd0},
	{0x0202,0x07},
	{0x0203,0x32},
	{0x0340,0x07},
	{0x0341,0xf0},
	{0x0219,0x00},
	{0x0346,0x01},
	{0x0347,0x00},
	{0x0d14,0x00},
	{0x0d13,0x05},
	{0x0d16,0x05},
	{0x0d15,0x1d},
	{0x00c0,0x0a},
	{0x00c1,0x30},
	{0x034a,0x05},
	{0x034b,0xb0},
	{0x0e0a,0x00},
	{0x0e0b,0x00},
	{0x0e0e,0x03},
	{0x0e0f,0x00},
	{0x0e06,0x0a},
	{0x0e23,0x15},
	{0x0e24,0x15},
	{0x0e2a,0x10},
	{0x0e2b,0x10},
	{0x0e17,0x49},
	{0x0e1b,0x1c},
	{0x0e3a,0x36},
	{0x0d11,0x84},
	{0x0e52,0x14},
	{0x000b,0x10},
	{0x0008,0x08},
	{0x0223,0x17},
	{0x0d27,0x39},
	{0x0d22,0x00},
	{0x03f6,0x0d},
	{0x0d04,0x07},
	{0x03f3,0x72},
	{0x03f4,0xb8},
	{0x03f5,0xbc},
	{0x0d02,0x73},
	/*autoloadstart*/
	{0x00c4,0x00},
	{0x00c5,0x01},
	{0x0af6,0x00},
	{0x0ba0,0x17},
	{0x0ba1,0x00},
	{0x0ba2,0x00},
	{0x0ba3,0x00},
	{0x0ba4,0x03},
	{0x0ba5,0x00},
	{0x0ba6,0x00},
	{0x0ba7,0x00},
	{0x0ba8,0x40},
	{0x0ba9,0x00},
	{0x0baa,0x00},
	{0x0bab,0x00},
	{0x0bac,0x40},
	{0x0bad,0x00},
	{0x0bae,0x00},
	{0x0baf,0x00},
	{0x0bb0,0x02},
	{0x0bb1,0x00},
	{0x0bb2,0x00},
	{0x0bb3,0x00},
	{0x0bb8,0x02},
	{0x0bb9,0x00},
	{0x0bba,0x00},
	{0x0bbb,0x00},
	{0x0a70,0x80},
	{0x0a71,0x00},
	{0x0a72,0x00},
	{0x0a66,0x00},
	{0x0a67,0x80},
	{0x0a4d,0x4e},
	{0x0a50,0x00},
	{0x0a4f,0x0c},
	{0x0a66,0x00},
	{0x00ca,0x00},
	{0x00cb,0xfc},
	{0x00cc,0x00},
	{0x00cd,0x00},
	{0x0aa1,0x00},
	{0x0aa2,0xe0},
	{0x0aa3,0x00},
	{0x0aa4,0x40},
	{0x0a90,0x03},
	{0x0a91,0x0e},
	{0x0a94,0x80},
	/*standby*/
	{0x0af6,0x20},
	{0x0b00,0x91},
	{0x0b01,0x17},
	{0x0b02,0x01},
	{0x0b03,0x00},
	{0x0b04,0x01},
	{0x0b05,0x17},
	{0x0b06,0x01},
	{0x0b07,0x00},
	{0x0ae9,0x01},
	{0x0aea,0x02},
	{0x0ae8,0x53},
	{0x0ae8,0x43},
	/*gain_partition*/
	{0x0af6,0x30},
	{0x0b00,0x08},
	{0x0b01,0x0f},
	{0x0b02,0x00},
	{0x0b04,0x1c},
	{0x0b05,0x24},
	{0x0b06,0x00},
	{0x0b08,0x30},
	{0x0b09,0x40},
	{0x0b0a,0x00},
	{0x0b0c,0x0e},
	{0x0b0d,0x2a},
	{0x0b0e,0x00},
	{0x0b10,0x0e},
	{0x0b11,0x2b},
	{0x0b12,0x00},
	{0x0b14,0x0e},
	{0x0b15,0x23},
	{0x0b16,0x00},
	{0x0b18,0x0e},
	{0x0b19,0x24},
	{0x0b1a,0x00},
	{0x0b1c,0x0c},
	{0x0b1d,0x0c},
	{0x0b1e,0x00},
	{0x0b20,0x03},
	{0x0b21,0x03},
	{0x0b22,0x00},
	{0x0b24,0x0e},
	{0x0b25,0x0e},
	{0x0b26,0x00},
	{0x0b28,0x03},
	{0x0b29,0x03},
	{0x0b2a,0x00},
	{0x0b2c,0x12},
	{0x0b2d,0x12},
	{0x0b2e,0x00},
	{0x0b30,0x08},
	{0x0b31,0x08},
	{0x0b32,0x00},
	{0x0b34,0x14},
	{0x0b35,0x14},
	{0x0b36,0x00},
	{0x0b38,0x10},
	{0x0b39,0x10},
	{0x0b3a,0x00},
	{0x0b3c,0x16},
	{0x0b3d,0x16},
	{0x0b3e,0x00},
	{0x0b40,0x10},
	{0x0b41,0x10},
	{0x0b42,0x00},
	{0x0b44,0x19},
	{0x0b45,0x19},
	{0x0b46,0x00},
	{0x0b48,0x16},
	{0x0b49,0x16},
	{0x0b4a,0x00},
	{0x0b4c,0x19},
	{0x0b4d,0x19},
	{0x0b4e,0x00},
	{0x0b50,0x16},
	{0x0b51,0x16},
	{0x0b52,0x00},
	{0x0b80,0x01},
	{0x0b81,0x00},
	{0x0b82,0x00},
	{0x0b84,0x00},
	{0x0b85,0x00},
	{0x0b86,0x00},
	{0x0b88,0x01},
	{0x0b89,0x6a},
	{0x0b8a,0x00},
	{0x0b8c,0x00},
	{0x0b8d,0x01},
	{0x0b8e,0x00},
	{0x0b90,0x01},
	{0x0b91,0xf6},
	{0x0b92,0x00},
	{0x0b94,0x00},
	{0x0b95,0x02},
	{0x0b96,0x00},
	{0x0b98,0x02},
	{0x0b99,0xc4},
	{0x0b9a,0x00},
	{0x0b9c,0x00},
	{0x0b9d,0x03},
	{0x0b9e,0x00},
	{0x0ba0,0x03},
	{0x0ba1,0xd8},
	{0x0ba2,0x00},
	{0x0ba4,0x00},
	{0x0ba5,0x04},
	{0x0ba6,0x00},
	{0x0ba8,0x05},
	{0x0ba9,0x4d},
	{0x0baa,0x00},
	{0x0bac,0x00},
	{0x0bad,0x05},
	{0x0bae,0x00},
	{0x0bb0,0x07},
	{0x0bb1,0x3e},
	{0x0bb2,0x00},
	{0x0bb4,0x00},
	{0x0bb5,0x06},
	{0x0bb6,0x00},
	{0x0bb8,0x0a},
	{0x0bb9,0x1a},
	{0x0bba,0x00},
	{0x0bbc,0x09},
	{0x0bbd,0x36},
	{0x0bbe,0x00},
	{0x0bc0,0x0e},
	{0x0bc1,0x66},
	{0x0bc2,0x00},
	{0x0bc4,0x10},
	{0x0bc5,0x06},
	{0x0bc6,0x00},
	{0x02c1,0xe0},
	{0x0207,0x04},
	{0x02c2,0x10},
	{0x02c3,0x74},
	{0x02c5,0x09},
	{0x02c1,0xe0},
	{0x0207,0x04},
	{0x02c2,0x10},
	{0x02c5,0x09},
	{0x02c1,0xe0},
	{0x0207,0x04},
	{0x02c2,0x10},
	{0x02c5,0x09},
	/*autoloadCH_GAIN*/
	{0x0aa1,0x15},
	{0x0aa2,0x50},
	{0x0aa3,0x00},
	{0x0aa4,0x09},
	{0x0a90,0x25},
	{0x0a91,0x0e},
	{0x0a94,0x80},
	/*ISP*/
	{0x0050,0x00},
	{0x0089,0x83},
	{0x005a,0x40},
	{0x00c3,0x35},
	{0x00c4,0x80},
	{0x0080,0x10},
	{0x0040,0x12},
	{0x0053,0x0a},
	{0x0054,0x44},
	{0x0055,0x32},
	{0x0058,0x89},
	{0x004a,0x03},
	{0x0048,0xf0},
	{0x0049,0x0f},
	{0x0041,0x20},
	{0x0043,0x0a},
	{0x009d,0x08},
	{0x0236,0x40},
	/*gain*/
	{0x0204,0x04},
	{0x0205,0x00},
	{0x02b3,0x00},
	{0x02b4,0x00},
	{0x009e,0x01},
	{0x009f,0x94},
	/*window2560x1440*/
	{0x0350,0x01},
	{0x0353,0x00},
	{0x0354,0x18},
	{0x034c,0x0a},
	{0x034d,0x00},
	{0x021f,0x14},
	/*autoloadREG*/
	{0x0aa1,0x10},
	{0x0aa2,0xf8},
	{0x0aa3,0x00},
	{0x0aa4,0x1f},
	{0x0a90,0x11},
	{0x0a91,0x0e},
	{0x0a94,0x80},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x0a90,0x00},
	{0x0a70,0x00},
	{0x0a67,0x00},
	{0x0af4,0x29},
	/*DPHY*/
	{0x0d80,0x07},
	{0x0dd3,0x18},
	/*MIPI*/
	{0x0107,0x05},
	{0x0117,0x01},
	{0x0d81,0x00},
	{0x0d84,0x0c},
	{0x0d85,0x80},
	{0x0d86,0x06},
	{0x0d87,0x41},
	{0x0db3,0x06},
	{0x0db4,0x08},
	{0x0db5,0x1e},
	{0x0db6,0x02},
	{0x0db8,0x12},
	{0x0db9,0x0a},
	{0x0d93,0x06},
	{0x0d94,0x09},
	{0x0d95,0x0d},
	{0x0d99,0x0b},
	{0x0084,0x01},
	/*CISCTl_Reset*/
	{0x031c,0x80},
	{0x03fe,0x30},
	{0x0d17,0x06},
	{0x03fe,0x00},
	{0x0d17,0x00},
	{0x031c,0x93},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x031c,0x80},
	{0x03fe,0x30},
	{0x0d17,0x06},
	{0x03fe,0x00},
	{0x0d17,0x00},
	{0x031c,0x93},
	/*OUT*/
	{0x0110,0x01},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the gc05a2_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting gc05a2_win_sizes[] = {
	{
		.width          = 2560,
		.height         = 1440,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SGRBG10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = gc05a2_init_regs_2560_1440_30fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &gc05a2_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list gc05a2_stream_on_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list gc05a2_stream_off_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int gc05a2_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int gc05a2_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int gc05a2_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc05a2_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int gc05a2_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc05a2_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int gc05a2_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int gc05a2_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int gc05a2_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc05a2_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int gc05a2_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	// unsigned char val = 0;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc05a2_write(sd, vals->reg_num, vals->value);
			// ret = gc05a2_read(sd, vals->reg_num, &val);
			// printk("	{0x%x 0x%x}\n", vals->reg_num, val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int gc05a2_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int gc05a2_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int gc05a2_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &gc05a2_win_sizes[0];
		gc05a2_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		gc05a2_attr.again.value = 65536;
		gc05a2_attr.again.max = 262144;
		gc05a2_attr.again.min = 0;
		gc05a2_attr.again.apply_delay = 2;

		gc05a2_attr.integration_time.value = 0x5b8;
		gc05a2_attr.integration_time.max = 0x07f0 - 16;
		gc05a2_attr.integration_time.min = 2;
		gc05a2_attr.integration_time.apply_delay = 2;

		gc05a2_attr.total_width = 0x0728;
		gc05a2_attr.total_height = 0x07f0;

		gc05a2_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
		gc05a2_attr.hcg.base_gain = ;
		gc05a2_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		gc05a2_attr.again_short.value = ;
		gc05a2_attr.again_short.max = ;
		gc05a2_attr.again_short.min = ;
		gc05a2_attr.again_short.apply_delay = ;

		gc05a2_attr.integration_time_short.value = ;
		gc05a2_attr.integration_time_short.max = ;
		gc05a2_attr.integration_time_short.min = ;
		gc05a2_attr.integration_time_short.apply_delay = ;

		gc05a2_attr.wdr_cache = wdr_line * gc05a2_attr.total_width;

#ifdef SENSOR_HCG
		gc05a2_attr.hcg_short.base_gain = ;
		gc05a2_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(gc05a2_attr.mipi)), (void *)(&gc05a2_mipi_linear), sizeof(gc05a2_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int gc05a2_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	gc05a2_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		gc05a2_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc05a2_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		gc05a2_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc05a2_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		gc05a2_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = gc05a2_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
	gc05a2_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int gc05a2_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = gc05a2_read(sd, 0x03f0, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = gc05a2_read(sd, 0x03f1, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int gc05a2_g_chip_ident(struct tx_isp_subdev *sd,
								 struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	gc05a2_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "gc05a2_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "gc05a2_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = gc05a2_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc05a2 chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", gc05a2_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "gc05a2", sizeof("gc05a2"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int gc05a2_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int gc05a2_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = gc05a2_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int gc05a2_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = gc05a2_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int gc05a2_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc05a2_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int gc05a2_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = gc05a2_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = gc05a2_write_array(sd, gc05a2_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("gc05a2 stream on\n");
		}

	} else {
		ret = gc05a2_write_array(sd, gc05a2_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("gc05a2 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int gc05a2_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret += gc05a2_write(sd, 0x0202, (unsigned char)((it >> 8) & 0xff));
	ret += gc05a2_write(sd, 0x0203, (unsigned char)(it & 0xff));

	ret += gc05a2_write(sd, 0x0204, (unsigned char)((again >> 8) & 0xff));
	ret += gc05a2_write(sd, 0x0205, (unsigned char)(again & 0xff));

	return ret;
}
#else
static int gc05a2_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	/*set integration time*/
	ret = gc5603_write(sd, 0x0d04, value & 0xff);
	ret += gc5603_write(sd, 0x0d03, value >> 8);

	return ret;
}

static int gc05a2_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	struct again_lut *val_lut = gc05a2_again_lut;

	/*set analog gain*/
	ret += gc05a2_write(sd, 0xd0, val_lut[again].again_pga);
	ret += gc05a2_write(sd, 0x31d, 0x2e);
	ret += gc05a2_write(sd, 0xdc1, val_lut[again].again_shift);
	ret += gc05a2_write(sd, 0x31d, 0x28);
	ret += gc05a2_write(sd, 0xb8, val_lut[again].col_gain0);
	ret += gc05a2_write(sd, 0xb9, val_lut[again].col_gain1);
	/*set blc*/
	ret += gc05a2_write(sd, 0x0155, val_lut[again].exp_rate_darkc);
	ret += gc05a2_write(sd, 0x0410, val_lut[again].darks_g1);
	ret += gc05a2_write(sd, 0x0411, val_lut[again].darks_r1);
	ret += gc05a2_write(sd, 0x0412, val_lut[again].darks_b1);
	ret += gc05a2_write(sd, 0x0413, val_lut[again].darks_g2);
	ret += gc05a2_write(sd, 0x0414, val_lut[again].darkn_g1);
	ret += gc05a2_write(sd, 0x0415, val_lut[again].darkn_r1);
	ret += gc05a2_write(sd, 0x0416, val_lut[again].darkn_b1);
	ret += gc05a2_write(sd, 0x0417, val_lut[again].darkn_g2);

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc05a2_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc05a2_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc05a2_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = gc05a2_attr_set(sd, wsize);
	}

	return ret;
}

static int gc05a2_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0;
	unsigned char tmp;
	int ret = ISP_SUCCESS;

	switch (info->default_boot) {
	case 0:
		sclk = 0x728 * 0x7f0 * 30;  /**< HTS * VTS * FPS */
		max_fps = 30;
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

	ret += gc05a2_read(sd, 0x0342, &tmp);
	hts = tmp & 0x0f;
	ret += gc05a2_read(sd, 0x0343, &tmp);
	if (ret < 0)
		return -1;
	hts = (hts << 8) | tmp;
	if (0 != ret) {
		ISP_ERROR("err: gc05a2 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	gc05a2_write(sd, 0x0340, (unsigned char) ((vts >> 8) & 0xff));
	gc05a2_write(sd, 0x0341, (unsigned char) (vts & 0xff));

	if (0 != ret) {
		ISP_ERROR("err: gc05a2_write err\n");
		return ret;
	}

	sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->integration_time.max = vts - 16;
#else
	/* WDR mode */
	switch (info->default_boot) {
	case 0:
		sensor->video.attr->total_height = vts;
		sensor->video.attr->integration_time.max = vts - 8;
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

static int gc05a2_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	ret = gc05a2_read(sd, 0x0101, &val);
	// printk("\n==> [%s %d] par->hvflip %d\n", __func__, __LINE__, par->hvflip);
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		val &= 0xFC;
		// sensor->video.mbus.code = TISP_VI_FMT_SGRBG10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		val = ((val & 0xFC) | 0x01);
		// sensor->video.mbus.code = TISP_VI_FMT_SRGGB10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		val = ((val & 0xFC) | 0x02);
		// sensor->video.mbus.code = TISP_VI_FMT_SBGGR10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		val |= 0x03;
		// sensor->video.mbus.code = TISP_VI_FMT_SGBRG10_1X10;
		break;
	}
	ret += gc05a2_write(sd, 0x0101, val);

	sensor->video.mbus_change = 1;
	sensor->video.hvflip_mode = par->hvflip;
	gc05a2_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int gc05a2_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int gc05a2_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int gc05a2_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc05a2_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = gc05a2_write_array(sd, gc05a2_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		gc05a2_setting_select(sd, 1);
		gc05a2_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		gc05a2_setting_select(sd, 0);
		gc05a2_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int gc05a2_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = gc05a2_write_array(sd, wsize->regs);
	ret = gc05a2_write_array(sd, gc05a2_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int gc05a2_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = gc05a2_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = gc05a2_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = gc05a2_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = gc05a2_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = gc05a2_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = gc05a2_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = gc05a2_write_array(sd, gc05a2_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = gc05a2_write_array(sd, gc05a2_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = gc05a2_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = gc05a2_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = gc05a2_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = gc05a2_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = gc05a2_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = gc05a2_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = gc05a2_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops gc05a2_core_ops = {
	.g_chip_ident = gc05a2_g_chip_ident,
	.reset = gc05a2_reset,
	.init = gc05a2_init,
	.g_register = gc05a2_g_register,
	.s_register = gc05a2_s_register,
};

static struct tx_isp_subdev_video_ops gc05a2_video_ops = {
	.s_stream = gc05a2_s_stream,
};

static struct tx_isp_subdev_sensor_ops gc05a2_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = gc05a2_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops gc05a2_ops = {
	.core = &gc05a2_core_ops,
	.video = &gc05a2_video_ops,
	.sensor = &gc05a2_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "gc05a2",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc05a2_probe(struct i2c_client *client,
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
	sensor->video.attr = &gc05a2_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc05a2_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->gc05a2\n");

	return 0;
}

static int gc05a2_remove(struct i2c_client *client)
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

static const struct i2c_device_id gc05a2_id[] = {
	{"gc05a2", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, gc05a2_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver gc05a2_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "gc05a2",
	},
	.probe          = gc05a2_probe,
	.remove         = gc05a2_remove,
	.id_table       = gc05a2_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_gc05a2(void) {
	return private_i2c_add_driver(&gc05a2_driver);
}

static __exit void exit_gc05a2(void) {
	private_i2c_del_driver(&gc05a2_driver);
}

module_init(init_gc05a2);
module_exit(exit_gc05a2);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_gc05a2(void) {
	return private_i2c_add_driver(&gc05a2_driver);
}

static void exit_first_gc05a2(void) {
	private_i2c_del_driver(&gc05a2_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "gc05a2",
	.i2c_addr = 0x31,
	.width = 3264,
	.height = 2448,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_gc05a2,
	.exit_sensor = exit_first_gc05a2
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for gc05a2 sensor");
MODULE_LICENSE("GPL");
