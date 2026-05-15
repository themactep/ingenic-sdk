/*
 * gc2083s2.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps        dvdd
 *  0           1920*1080       30        mipi_2lane    linear  10        30           null
 *
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
#define SENSOR_VERSION  "H20251003a"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x20)
#define SENSOR_CHIP_ID_L    (0x83)
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

struct tx_isp_sensor_attribute gc2083s2_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	int index;
	unsigned int again_pga;
	unsigned int exp_rate_darkc;
	unsigned int darks_g1;
	unsigned int darks_r1;
	unsigned int darks_b1;
	unsigned int darks_g2;
	unsigned int darkn_g1;
	unsigned int darkn_r1;
	unsigned int darkn_b1;
	unsigned int darkn_g2;
	unsigned int col_gain0;
	unsigned int col_gain1;
	unsigned int again_shift;
	unsigned int gain;
};

struct again_lut gc2083s2_again_lut[] = {
	//0x00d0 0x0155 0x0410 0x0411 0x0412 0x0413 0x0414 0x0415 0x0416 0x0417 0x00b8 0x00b9 0x0dc1
	{0x00, 0x00, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x01, 0x00, 0x00, 0},   // 1.000000
	{0x01, 0x10, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x01, 0x0c, 0x00, 16247},   // 1.187500
	{0x02, 0x01, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x01, 0x1a, 0x00, 32233},   // 1.406250
	{0x03, 0x11, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x01, 0x2b, 0x00, 48592},   // 1.671875
	{0x04, 0x02, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x02, 0x00, 0x00, 65536},   // 2.000000
	{0x05, 0x12, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x02, 0x18, 0x00, 81783},   // 2.375000
	{0x06, 0x03, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x02, 0x33, 0x00, 97243},   // 2.796875
	{0x07, 0x13, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x03, 0x15, 0x00, 113685},   // 3.328125
	{0x08, 0x04, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x04, 0x00, 0x00, 131072},   // 4.000000
	{0x09, 0x14, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x04, 0xe0, 0x00, 147629},   // 4.765625
	{0x0a, 0x05, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x05, 0x26, 0x00, 162779},   // 5.593750
	{0x0b, 0x15, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x06, 0x2b, 0x00, 179442},   // 6.671875
	{0x0c, 0x44, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x08, 0x00, 0x00, 196608},   // 8.000000
	{0x0d, 0x54, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x09, 0x22, 0x00, 213165},   // 9.531250
	{0x0e, 0x45, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x0b, 0x0d, 0x00, 228446},   // 11.203125
	{0x0f, 0x55, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x0d, 0x16, 0x00, 244978},   // 13.343750
	{0x10, 0x04, 0x19, 0x16, 0x16, 0x16, 0x16, 0x6f, 0x6f, 0x6f, 0x6f, 0x10, 0x00, 0x01, 262144},   // 16.000000
	{0x11, 0x14, 0x19, 0x16, 0x16, 0x16, 0x16, 0x6f, 0x6f, 0x6f, 0x6f, 0x13, 0x04, 0x01, 278701},   // 19.062500
	{0x12, 0x24, 0x19, 0x16, 0x16, 0x16, 0x16, 0x6f, 0x6f, 0x6f, 0x6f, 0x16, 0x1a, 0x01, 293982},   // 22.406250
	{0x13, 0x34, 0x19, 0x16, 0x16, 0x16, 0x16, 0x6f, 0x6f, 0x6f, 0x6f, 0x1a, 0x2b, 0x01, 310458},   // 26.671875
	{0x14, 0x44, 0x36, 0x18, 0x18, 0x18, 0x18, 0x6f, 0x6f, 0x6f, 0x6f, 0x20, 0x00, 0x01, 327680},   // 32.000000
	{0x15, 0x54, 0x36, 0x18, 0x18, 0x18, 0x18, 0x6f, 0x6f, 0x6f, 0x6f, 0x26, 0x07, 0x01, 344199},   // 38.109375
	{0x16, 0x64, 0x36, 0x18, 0x18, 0x18, 0x18, 0x6f, 0x6f, 0x6f, 0x6f, 0x2c, 0x33, 0x01, 359485},   // 44.796875
	{0x17, 0x74, 0x36, 0x18, 0x18, 0x18, 0x18, 0x6f, 0x6f, 0x6f, 0x6f, 0x35, 0x17, 0x01, 376022},   // 53.359375
	{0x18, 0x84, 0x64, 0x16, 0x16, 0x16, 0x16, 0x72, 0x72, 0x72, 0x72, 0x35, 0x17, 0x01, 393216},   // 64.000000
	{0x19, 0x94, 0x64, 0x16, 0x16, 0x16, 0x16, 0x72, 0x72, 0x72, 0x72, 0x35, 0x17, 0x01, 409735},   // 76.218750
	{0x1a, 0x85, 0x64, 0x16, 0x16, 0x16, 0x16, 0x72, 0x72, 0x72, 0x72, 0x35, 0x17, 0x01, 425021},   // 89.593750
	{0x1b, 0x95, 0x64, 0x16, 0x16, 0x16, 0x16, 0x72, 0x72, 0x72, 0x72, 0x35, 0x17, 0x01, 441558},   // 106.718750
	{0x1c, 0xa5, 0x64, 0x16, 0x16, 0x16, 0x16, 0x72, 0x72, 0x72, 0x72, 0x35, 0x17, 0x01, 456839},   // 125.437500
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int gc2083s2_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = gc2083s2_again_lut;
	while (lut->gain <= gc2083s2_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].index;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == gc2083s2_attr.again.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
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
unsigned int gc2083s2_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = gc2083s2_again_lut;
	while(lut->gain <= gc2083s2_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == gc2083s2_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int gc2083s2_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int gc2083s2_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int gc2083s2_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus gc2083s2_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 675,
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

struct tx_isp_dvp_bus gc2083s2_dvp = {
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

struct tx_isp_sensor_attribute gc2083s2_attr = {
	.name = "gc2083s2",
	.chip_id = 0x2083,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x37,
	.sensor_ctrl.alloc_again = gc2083s2_alloc_again,
	.sensor_ctrl.alloc_dgain = gc2083s2_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = gc2083s2_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list gc2083s2_init_regs_1920_1080_30fps_mipi[] = {
	{0x03fe,0xf0},
	{0x03fe,0xf0},
	{0x03fe,0xf0},
	{0x03fe,0x00},
	{0x03f2,0x00},
	{0x03f3,0x00},
	{0x03f4,0x36},
	{0x03f5,0xc0},
	{0x03f6,0x24},
	{0x03f7,0x01},
	{0x03f8,0x32},
	{0x03f9,0x43},
	{0x03fc,0x8e},
	{0x0381,0x07},
	{0x00d7,0x29},
	{0x0d6d,0x18},
	{0x00d5,0x03},
	{0x0082,0x01},
	{0x0db3,0xd4},
	{0x0db0,0x0d},
	{0x0db5,0x96},
	{0x0d03,0x02},
	{0x0d04,0x02},
	{0x0d05,0x05},// hts -> 0x540 = 1344
	{0x0d06,0x40},//
	{0x0d07,0x00},
	{0x0d08,0x11},
	{0x0d09,0x00},
	{0x0d0a,0x02},
	{0x000b,0x00},
	{0x000c,0x02},
	{0x0d0d,0x04},
	{0x0d0e,0x40},
	{0x000f,0x07},
	{0x0010,0x90},
	{0x0017,0x0c},
	{0x0d73,0x92},
	{0x0076,0x00},
	{0x0d76,0x00},
	{0x0d41,0x04},//vts -> 0x4d8 = 1240
	{0x0d42,0xd8},//
	{0x0d7a,0x10},
	{0x0d19,0x31},
	{0x0d25,0x0b},
	{0x0d20,0x60},
	{0x0d27,0x03},
	{0x0d29,0x60},
	{0x0d43,0x10},
	{0x0d49,0x10},
	{0x0d55,0x18},
	{0x0dc2,0x44},
	{0x0058,0x3c},
	{0x00d8,0x68},
	{0x00d9,0x14},
	{0x00da,0xc1},
	{0x0050,0x18},
	{0x0db6,0x3d},
	{0x00d2,0xbc},
	{0x0d66,0x42},
	{0x008c,0x07},
	{0x008d,0xff},
	{0x007a,0x50},
	{0x00d0,0x00},
	{0x0dc1,0x00},
	{0x0102,0xa9},
	{0x0158,0x00},
	{0x0107,0xa6},
	{0x0108,0xa9},
	{0x0109,0xa8},
	{0x010a,0xa7},
	{0x010b,0xff},
	{0x010c,0xff},
	{0x0428,0x86},
	{0x0429,0x86},
	{0x042a,0x86},
	{0x042b,0x68},
	{0x042c,0x68},
	{0x042d,0x68},
	{0x042e,0x68},
	{0x042f,0x68},
	{0x0430,0x4f},
	{0x0431,0x68},
	{0x0432,0x67},
	{0x0433,0x66},
	{0x0434,0x66},
	{0x0435,0x66},
	{0x0436,0x66},
	{0x0437,0x66},
	{0x0438,0x62},
	{0x0439,0x62},
	{0x043a,0x62},
	{0x043b,0x62},
	{0x043c,0x62},
	{0x043d,0x62},
	{0x043e,0x62},
	{0x043f,0x62},
	{0x0077,0x01},
	{0x0078,0x65},
	{0x0079,0x04},
	{0x0067,0xa0},
	{0x0054,0xff},
	{0x0055,0x02},
	{0x0056,0x00},
	{0x0057,0x04},
	{0x005a,0xff},
	{0x005b,0x07},
	{0x0026,0x01},
	{0x0152,0x02},
	{0x0153,0x50},
	{0x0155,0x93},
	{0x0410,0x16},
	{0x0411,0x16},
	{0x0412,0x16},
	{0x0413,0x16},
	{0x0414,0x6f},
	{0x0415,0x6f},
	{0x0416,0x6f},
	{0x0417,0x6f},
	{0x04e0,0x18},
	{0x0192,0x04},
	{0x0194,0x04},
	{0x0195,0x04},
	{0x0196,0x38},
	{0x0197,0x07},
	{0x0198,0x80},
	{0x0201,0x27},
	{0x0202,0x53},
	{0x0203,0xce},
	{0x0204,0x40},
	{0x0212,0x07},
	{0x0213,0x80},
	{0x0215,0x10},
	{0x0229,0x05},
	{0x0237,0x03},
	{0x023e,0x99},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list gc2083s2_init_regs_1920_1080_30fps_mipi_slave[] = {
	{0x03fe,0xf0},
	{0x03fe,0xf0},
	{0x03fe,0xf0},
	{0x03fe,0x00},
	{0x03f2,0x00},
	{0x03f3,0x00},
	{0x03f4,0x36},
	{0x03f5,0xc0},
	{0x03f6,0x24},
	{0x03f7,0x01},
	{0x03f8,0x32},
	{0x03f9,0x43},
	{0x03fc,0x8e},
	{0x0381,0x07},
	{0x00d7,0x29},
	{0x0d6d,0x18},
	{0x00d5,0x03},
	{0x0082,0x01},
	{0x0db3,0xd4},
	{0x0db0,0x0d},
	{0x0db5,0x96},
	{0x0d03,0x02},
	{0x0d04,0x02},
	{0x0d05,0x05},// hts -> 0x540 = 1344
	{0x0d06,0x40},//
	{0x0d07,0x00},
	{0x0d08,0x11},
	{0x0d09,0x00},
	{0x0d0a,0x02},
	{0x000b,0x00},
	{0x000c,0x02},
	{0x0d0d,0x04},
	{0x0d0e,0x40},
	{0x000f,0x07},
	{0x0010,0x90},
	{0x0017,0x0c},
	{0x0d73,0x92},
	{0x0076,0x00},
	{0x0d76,0x00},
	{0x0d41,0x04},//vts -> 0x4d8 = 1240
	{0x0d42,0xd8},//
	{0x0d7a,0x10},
	{0x0d19,0x31},
	{0x0d25,0x0b},
	{0x0d20,0x60},
	{0x0d27,0x03},
	{0x0d29,0x60},
	{0x0d43,0x10},
	{0x0d49,0x10},
	{0x0d55,0x18},
	{0x0dc2,0x44},
	{0x0058,0x3c},
	{0x00d8,0x68},
	{0x00d9,0x14},
	{0x00da,0xc1},
	{0x0050,0x18},
	{0x0db6,0x3d},
	{0x00d2,0xbc},
	{0x0d66,0x42},
	{0x008c,0x07},
	{0x008d,0xff},
	{0x007a,0x50},
	{0x00d0,0x00},
	{0x0dc1,0x00},
	{0x0102,0xa9},
	{0x0158,0x00},
	{0x0107,0xa6},
	{0x0108,0xa9},
	{0x0109,0xa8},
	{0x010a,0xa7},
	{0x010b,0xff},
	{0x010c,0xff},
	{0x0428,0x86},
	{0x0429,0x86},
	{0x042a,0x86},
	{0x042b,0x68},
	{0x042c,0x68},
	{0x042d,0x68},
	{0x042e,0x68},
	{0x042f,0x68},
	{0x0430,0x4f},
	{0x0431,0x68},
	{0x0432,0x67},
	{0x0433,0x66},
	{0x0434,0x66},
	{0x0435,0x66},
	{0x0436,0x66},
	{0x0437,0x66},
	{0x0438,0x62},
	{0x0439,0x62},
	{0x043a,0x62},
	{0x043b,0x62},
	{0x043c,0x62},
	{0x043d,0x62},
	{0x043e,0x62},
	{0x043f,0x62},
	{0x0077,0x01},
	{0x0078,0x65},
	{0x0079,0x04},
	{0x0067,0xa0},
	{0x0054,0xff},
	{0x0055,0x02},
	{0x0056,0x00},
	{0x0057,0x04},
	{0x005a,0xff},
	{0x005b,0x07},
	{0x0026,0x01},
	{0x0152,0x02},
	{0x0153,0x50},
	{0x0155,0x93},
	{0x0410,0x16},
	{0x0411,0x16},
	{0x0412,0x16},
	{0x0413,0x16},
	{0x0414,0x6f},
	{0x0415,0x6f},
	{0x0416,0x6f},
	{0x0417,0x6f},
	{0x04e0,0x18},
	{0x0192,0x04},
	{0x0194,0x04},
	{0x0195,0x04},
	{0x0196,0x38},
	{0x0197,0x07},
	{0x0198,0x80},
	{0x0201,0x27},
	{0x0202,0x53},
	{0x0203,0xce},
	{0x0204,0x40},
	{0x0212,0x07},
	{0x0213,0x80},
	{0x0215,0x10},
	{0x0229,0x05},
	{0x0237,0x03},
	{0x023e,0x99},

	{0x0d41, 0x09},//30fps@vts=0x4d8=1240 15fps@vts=0x9b0=2480
	{0x0d42, 0xb0},//
	{0x0068, 0x93},
	{0x0069, 0x00},
	{0x0d67, 0x00},
	{0x0d69, 0x90},
	{0x0d6a, 0x80},
	{0x0d6c, 0x02},
	{0x0d6d, 0x89},
	{0x0d6e, 0x00},
	{0x0d6f, 0x04},
	{0x0d70, 0x00},
	{0x0d71, 0x30},
	{0x0d73, 0x92},
	{0x0d6b, 0x51},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};
/*
 * the order of the gc2083s2_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting gc2083s2_win_sizes[] = {
	{
		.width          = 1920,
		.height         = 1080,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = gc2083s2_init_regs_1920_1080_30fps_mipi,
	},
	{
		.width          = 1920,
		.height         = 1080,
		.fps            = 15 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = gc2083s2_init_regs_1920_1080_30fps_mipi_slave,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &gc2083s2_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list gc2083s2_stream_on_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list gc2083s2_stream_off_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int gc2083s2_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int gc2083s2_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int gc2083s2_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc2083s2_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int gc2083s2_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc2083s2_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int gc2083s2_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int gc2083s2_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int gc2083s2_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc2083s2_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int gc2083s2_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	// unsigned char val = 0;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc2083s2_write(sd, vals->reg_num, vals->value);
			// ret = gc2083s2_read(sd, vals->reg_num, &val);
			// printk("	{0x%x 0x%x}\n", vals->reg_num, val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int gc2083s2_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	/* struct i2c_client *client = tx_isp_get_subdevdata(sd); */
	unsigned long rate;
	int ret = ISP_SUCCESS;

	return 0;
	rate = private_clk_get_rate(sensor->mclk);
	if(((rate / 1000) % (mclk / 1000)) != 0) {
		/* ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK)); */
		/* sclka = private_devm_clk_get(&client->dev, SEN_TCLK); */
		if (IS_ERR(sclka)) {
			pr_err("get sclka failed\n");
		} else {
			/* rate = private_clk_get_rate(sclka); */
			/* if(((rate / 1000) % (mclk / 1000)) != 0) { */
			/* 	switch(mclk) { */
			/* 	case 24000000: */
			/* 		private_clk_set_rate(sclka, 1200000000); */
			/* 		break; */
			/* 	case 27000000: */
			/* 	case 37125000: */
			/* 		private_clk_set_rate(sclka, 1188000000); */
			/* 		break; */
			/* 	default: */
			/* 		ret = -1; */
			/* 		goto error; */
			/* 	} */
			/* } else { */
			/* 	if ((mclk != 24000000) && (mclk != 27000000) && (mclk != 37125000)) { */
			/* 		ret = -1; */
			/* 		goto error; */
			/* 	} */
			/* } */
		}
	}

	private_clk_set_rate(sensor->mclk, mclk);
	private_clk_prepare_enable(sensor->mclk);

/* error: */
	return ret;
}

static int gc2083s2_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int gc2083s2_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &gc2083s2_win_sizes[0];
		gc2083s2_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		gc2083s2_attr.again.value = 65536;
		gc2083s2_attr.again.max = 456839;
		gc2083s2_attr.again.min = 0;
		gc2083s2_attr.again.apply_delay = 2;

		gc2083s2_attr.integration_time.value = 0x5b8;
		gc2083s2_attr.integration_time.max = 1240 - 8;
		gc2083s2_attr.integration_time.min = 2;
		gc2083s2_attr.integration_time.apply_delay = 2;

		gc2083s2_attr.total_width = 1344;
		gc2083s2_attr.total_height = 1240;

		gc2083s2_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
		gc2083s2_attr.hcg.base_gain = ;
		gc2083s2_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		gc2083s2_attr.again_short.value = ;
		gc2083s2_attr.again_short.max = ;
		gc2083s2_attr.again_short.min = ;
		gc2083s2_attr.again_short.apply_delay = ;

		gc2083s2_attr.integration_time_short.value = ;
		gc2083s2_attr.integration_time_short.max = ;
		gc2083s2_attr.integration_time_short.min = ;
		gc2083s2_attr.integration_time_short.apply_delay = ;

		gc2083s2_attr.wdr_cache = wdr_line * gc2083s2_attr.total_width;

#ifdef SENSOR_HCG
		gc2083s2_attr.hcg_short.base_gain = ;
		gc2083s2_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(gc2083s2_attr.mipi)), (void *)(&gc2083s2_mipi_linear), sizeof(gc2083s2_attr.mipi));
		break;
	case 1:
		wsize = &gc2083s2_win_sizes[1];
		gc2083s2_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		gc2083s2_attr.again.value = 65536;
		gc2083s2_attr.again.max = 456839;
		gc2083s2_attr.again.min = 0;
		gc2083s2_attr.again.apply_delay = 2;

		gc2083s2_attr.integration_time.value = 0x5b8;
		gc2083s2_attr.integration_time.max = 1240 - 8;
		gc2083s2_attr.integration_time.min = 2;
		gc2083s2_attr.integration_time.apply_delay = 2;

		gc2083s2_attr.total_width = 1344;
		gc2083s2_attr.total_height = 1240;

		gc2083s2_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
		gc2083s2_attr.hcg.base_gain = ;
		gc2083s2_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		gc2083s2_attr.again_short.value = ;
		gc2083s2_attr.again_short.max = ;
		gc2083s2_attr.again_short.min = ;
		gc2083s2_attr.again_short.apply_delay = ;

		gc2083s2_attr.integration_time_short.value = ;
		gc2083s2_attr.integration_time_short.max = ;
		gc2083s2_attr.integration_time_short.min = ;
		gc2083s2_attr.integration_time_short.apply_delay = ;

		gc2083s2_attr.wdr_cache = wdr_line * gc2083s2_attr.total_width;

#ifdef SENSOR_HCG
		gc2083s2_attr.hcg_short.base_gain = ;
		gc2083s2_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(gc2083s2_attr.mipi)), (void *)(&gc2083s2_mipi_linear), sizeof(gc2083s2_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int gc2083s2_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	gc2083s2_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		gc2083s2_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc2083s2_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		gc2083s2_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc2083s2_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		gc2083s2_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = gc2083s2_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
	gc2083s2_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int gc2083s2_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = gc2083s2_read(sd, 0x03f0, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = gc2083s2_read(sd, 0x03f1, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int gc2083s2_g_chip_ident(struct tx_isp_subdev *sd,
								 struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	gc2083s2_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "gc2083s2_reset");
		if (!ret) {
			private_gpio_direction_output(info->rst_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
		}
	}
	if (info->pwdn_gpio != -1) {
		ret = private_gpio_request(info->pwdn_gpio, "gc2083s2_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = gc2083s2_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc2083s2 chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", gc2083s2_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "gc2083s2", sizeof("gc2083s2"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int gc2083s2_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int gc2083s2_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = gc2083s2_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int gc2083s2_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = gc2083s2_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int gc2083s2_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc2083s2_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int gc2083s2_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = gc2083s2_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = gc2083s2_write_array(sd, gc2083s2_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("gc2083s2 stream on\n");
		}

	} else {
		ret = gc2083s2_write_array(sd, gc2083s2_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("gc2083s2 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int gc2083s2_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;
	struct again_lut *val_lut = gc2083s2_again_lut;

	/*set integration time*/
	ret = gc2083s2_write(sd, 0x0d04, it & 0xff);
	ret += gc2083s2_write(sd, 0x0d03, it >> 8);

	/*set analog gain*/
	ret += gc2083s2_write(sd, 0xd0, val_lut[again].again_pga);
	ret += gc2083s2_write(sd, 0x31d, 0x2e);
	ret += gc2083s2_write(sd, 0xdc1, val_lut[again].again_shift);
	ret += gc2083s2_write(sd, 0x31d, 0x28);
	ret += gc2083s2_write(sd, 0xb8, val_lut[again].col_gain0);
	ret += gc2083s2_write(sd, 0xb9, val_lut[again].col_gain1);
	/*set blc*/
	ret += gc2083s2_write(sd, 0x0155, val_lut[again].exp_rate_darkc);
	ret += gc2083s2_write(sd, 0x0410, val_lut[again].darks_g1);
	ret += gc2083s2_write(sd, 0x0411, val_lut[again].darks_r1);
	ret += gc2083s2_write(sd, 0x0412, val_lut[again].darks_b1);
	ret += gc2083s2_write(sd, 0x0413, val_lut[again].darks_g2);
	ret += gc2083s2_write(sd, 0x0414, val_lut[again].darkn_g1);
	ret += gc2083s2_write(sd, 0x0415, val_lut[again].darkn_r1);
	ret += gc2083s2_write(sd, 0x0416, val_lut[again].darkn_b1);
	ret += gc2083s2_write(sd, 0x0417, val_lut[again].darkn_g2);

	return ret;
}
#else
static int gc2083s2_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	/*set integration time*/
	ret = gc5603_write(sd, 0x0d04, value & 0xff);
	ret += gc5603_write(sd, 0x0d03, value >> 8);

	return ret;
}

static int gc2083s2_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	struct again_lut *val_lut = gc2083s2_again_lut;

	/*set analog gain*/
	ret += gc2083s2_write(sd, 0xd0, val_lut[again].again_pga);
	ret += gc2083s2_write(sd, 0x31d, 0x2e);
	ret += gc2083s2_write(sd, 0xdc1, val_lut[again].again_shift);
	ret += gc2083s2_write(sd, 0x31d, 0x28);
	ret += gc2083s2_write(sd, 0xb8, val_lut[again].col_gain0);
	ret += gc2083s2_write(sd, 0xb9, val_lut[again].col_gain1);
	/*set blc*/
	ret += gc2083s2_write(sd, 0x0155, val_lut[again].exp_rate_darkc);
	ret += gc2083s2_write(sd, 0x0410, val_lut[again].darks_g1);
	ret += gc2083s2_write(sd, 0x0411, val_lut[again].darks_r1);
	ret += gc2083s2_write(sd, 0x0412, val_lut[again].darks_b1);
	ret += gc2083s2_write(sd, 0x0413, val_lut[again].darks_g2);
	ret += gc2083s2_write(sd, 0x0414, val_lut[again].darkn_g1);
	ret += gc2083s2_write(sd, 0x0415, val_lut[again].darkn_r1);
	ret += gc2083s2_write(sd, 0x0416, val_lut[again].darkn_b1);
	ret += gc2083s2_write(sd, 0x0417, val_lut[again].darkn_g2);

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc2083s2_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc2083s2_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc2083s2_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = gc2083s2_attr_set(sd, wsize);
	}

	return ret;
}

static int gc2083s2_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	/* unsigned char val = 0; */
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0;
	int ret = ISP_SUCCESS;

	switch (info->default_boot) {
	case 0:
		sclk = 1344 * 1240 * 30;  /**< HTS * VTS * FPS */
		hts = 1344;
		max_fps = 30;
		break;
	case 1:
		sclk = 1344 * 1240 * 30;  /**< HTS * VTS * FPS */
		hts = 1344;
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

	/* val = 0; */
	/* ret += gc2083s2_write(sd, 0xd05, 0x5); */
	/* ret += gc2083s2_read(sd, 0x0d05, &val); */
	/* hts = val << 8; */
	/* val = 0; */
	/* ret += gc2083s2_read(sd, 0x0d06, &val); */
	/* hts  = (hts | val); */
	/* if (0 != ret) { */
	/* 	ISP_ERROR("err: gc2083s2 read err\n"); */
	/* 	return ret; */
	/* } */

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	gc2083s2_write(sd, 0x0d41, (unsigned char) ((vts >> 8) & 0xff));
	gc2083s2_write(sd, 0x0d42, (unsigned char) (vts & 0xff));

	if (0 != ret) {
		ISP_ERROR("err: gc2083s2_write err\n");
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

static int gc2083s2_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	ret = gc2083s2_read(sd, 0x0d15, &val);
	printk("\n==> [%s %d] par->hvflip %d\n", __func__, __LINE__, par->hvflip);
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		val &= 0xFC;
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		val = ((val & 0xFD) | 0x01);
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		val = ((val & 0xFE) | 0x02);
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		val |= 0x03;
		break;
	}
	ret += gc2083s2_write(sd, 0x0d15, val);
	ret += gc2083s2_write(sd, 0x0015, val);

	sensor->video.hvflip_mode = par->hvflip;
	gc2083s2_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int gc2083s2_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int gc2083s2_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int gc2083s2_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc2083s2_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = gc2083s2_write_array(sd, gc2083s2_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		gc2083s2_setting_select(sd, 1);
		gc2083s2_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		gc2083s2_setting_select(sd, 0);
		gc2083s2_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int gc2083s2_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = gc2083s2_write_array(sd, wsize->regs);
	ret = gc2083s2_write_array(sd, gc2083s2_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int gc2083s2_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = gc2083s2_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = gc2083s2_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = gc2083s2_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = gc2083s2_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = gc2083s2_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = gc2083s2_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = gc2083s2_write_array(sd, gc2083s2_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = gc2083s2_write_array(sd, gc2083s2_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = gc2083s2_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = gc2083s2_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = gc2083s2_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = gc2083s2_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = gc2083s2_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = gc2083s2_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = gc2083s2_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops gc2083s2_core_ops = {
	.g_chip_ident = gc2083s2_g_chip_ident,
	.reset = gc2083s2_reset,
	.init = gc2083s2_init,
	.g_register = gc2083s2_g_register,
	.s_register = gc2083s2_s_register,
};

static struct tx_isp_subdev_video_ops gc2083s2_video_ops = {
	.s_stream = gc2083s2_s_stream,
};

static struct tx_isp_subdev_sensor_ops gc2083s2_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = gc2083s2_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops gc2083s2_ops = {
	.core = &gc2083s2_core_ops,
	.video = &gc2083s2_video_ops,
	.sensor = &gc2083s2_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "gc2083s2",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc2083s2_probe(struct i2c_client *client,
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
	sensor->video.attr = &gc2083s2_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc2083s2_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->gc2083s2\n");

	return 0;
}

static int gc2083s2_remove(struct i2c_client *client)
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

static const struct i2c_device_id gc2083s2_id[] = {
	{"gc2083s2", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, gc2083s2_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver gc2083s2_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "gc2083s2",
	},
	.probe          = gc2083s2_probe,
	.remove         = gc2083s2_remove,
	.id_table       = gc2083s2_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_gc2083s2(void) {
	return private_i2c_add_driver(&gc2083s2_driver);
}

static __exit void exit_gc2083s2(void) {
	private_i2c_del_driver(&gc2083s2_driver);
}

module_init(init_gc2083s2);
module_exit(exit_gc2083s2);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_gc2083s2(void) {
	return private_i2c_add_driver(&gc2083s2_driver);
}

static void exit_first_gc2083s2(void) {
	private_i2c_del_driver(&gc2083s2_driver);
}

struct tx_isp_sensor_fast_attr sensor2 = {
	.name = "gc2083s2",
	.i2c_addr = 0x37,
	.width = 1920,
	.height = 1080,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_gc2083s2,
	.exit_sensor = exit_first_gc2083s2
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for gc2083s2 sensor");
MODULE_LICENSE("GPL");
