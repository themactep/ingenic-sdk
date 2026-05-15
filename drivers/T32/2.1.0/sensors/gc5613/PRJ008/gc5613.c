/*
 * gc5613.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps     dvdd
 *  0           2880*1620       25        mipi_2lane    linear  10        25        1.2V
 *  1           2880*1620       30        mipi_2lane    linear  10        30        1.2V
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
#define SENSOR_VERSION  "H20260130a"

//#define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT	/**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE	  /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME	/**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP		  /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H	(0x56)
#define SENSOR_CHIP_ID_L	(0x03)
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

struct tx_isp_sensor_attribute gc5613_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int index;
	unsigned char reg0614;
	unsigned char reg0615;
	unsigned char reg0225;
	unsigned char reg1467;
	unsigned char reg1468;
	unsigned char reg00b8;
	unsigned char reg00b9;
	unsigned int gain;
};

struct again_lut gc5613_again_lut[] = {
	//index 0614, 0615, 0225, 1467	1468, 00b8, 00b9 gain
	{0x00, 0x00, 0x00, 0x04, 0x15, 0x15, 0x01, 0x00, 0},			 //1.000000
	{0x01, 0x90, 0x02, 0x04, 0x15, 0x15, 0x01, 0x0a, 13726},			 //1.156250
	{0x02, 0x00, 0x00, 0x00, 0x15, 0x15, 0x01, 0x12, 23431},			 //1.281250
	{0x03, 0x90, 0x02, 0x00, 0x15, 0x15, 0x01, 0x20, 38335},			 //1.500000
	{0x04, 0x01, 0x00, 0x00, 0x15, 0x15, 0x01, 0x30, 52910},			 //1.750000
	{0x05, 0x91, 0x02, 0x00, 0x15, 0x15, 0x02, 0x05, 69158},			 //2.078125
	{0x06, 0x02, 0x00, 0x00, 0x15, 0x15, 0x02, 0x19, 82403},			 //2.390625
	{0x07, 0x92, 0x02, 0x00, 0x16, 0x16, 0x02, 0x3f, 103377},			  //2.984375
	{0x08, 0x03, 0x00, 0x00, 0x16, 0x16, 0x03, 0x20, 118446},			  //3.500000
	{0x09, 0x93, 0x02, 0x00, 0x17, 0x17, 0x04, 0x0a, 134694},			  //4.156250
	{0x0a, 0x00, 0x00, 0x01, 0x18, 0x18, 0x05, 0x02, 152758},			  //5.031250
	{0x0b, 0x90, 0x02, 0x01, 0x19, 0x19, 0x05, 0x39, 167667},			  //5.890625
	{0x0c, 0x01, 0x00, 0x01, 0x19, 0x19, 0x06, 0x3c, 183134},			  //6.937500
	{0x0d, 0x91, 0x02, 0x01, 0x19, 0x19, 0x08, 0x0d, 198977},			  //8.203125
	{0x0e, 0x02, 0x00, 0x01, 0x1a, 0x1a, 0x09, 0x21, 213010},			  //9.515625
	{0x0f, 0x92, 0x02, 0x01, 0x1a, 0x1a, 0x0b, 0x0f, 228710},			  //11.234375
	{0x10, 0x03, 0x00, 0x01, 0x1c, 0x1c, 0x0d, 0x17, 245089},			  //13.359375
	{0x11, 0x93, 0x02, 0x01, 0x1c, 0x1c, 0x0f, 0x33, 260934},			  //15.796875
	{0x12, 0x04, 0x00, 0x01, 0x1d, 0x1d, 0x12, 0x30, 277139},			  //18.750000
	{0x13, 0x94, 0x02, 0x01, 0x1d, 0x1d, 0x16, 0x10, 293321},			  //22.250000
	{0x14, 0x05, 0x00, 0x01, 0x1e, 0x1e, 0x1a, 0x19, 309456},			  //26.390625
	{0x15, 0x95, 0x02, 0x01, 0x1e, 0x1e, 0x1f, 0x13, 325578},			  //31.296875
	{0x16, 0x06, 0x00, 0x01, 0x20, 0x20, 0x25, 0x08, 341724},			  //37.125000
	{0x17, 0x96, 0x02, 0x01, 0x20, 0x20, 0x2c, 0x03, 357889},			  //44.046875
	{0x18, 0xb6, 0x04, 0x01, 0x20, 0x20, 0x34, 0x0f, 374007},			  //52.234375
	{0x19, 0x86, 0x06, 0x01, 0x20, 0x20, 0x3d, 0x3d, 390142},			  //61.953125
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int gc5613_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = gc5613_again_lut;
	while (lut->gain <= gc5613_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].index;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == gc5613_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int gc5613_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = gc5613_again_lut;
	while(lut->gain <= gc5613_attr.max_again_short) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == gc5613_attr.max_again_short) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
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

unsigned int gc5613_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int gc5613_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int gc5613_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus gc5613_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 848,
	.lans = 2,
	.image_twidth = 2880,
	.image_theight = 1620,
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

struct tx_isp_dvp_bus gc5613_dvp = {
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

struct tx_isp_sensor_attribute gc5613_attr = {
	.name = "gc5613",
	.chip_id = 0x5603,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x31,
	.sensor_ctrl.alloc_again = gc5613_alloc_again,
	.sensor_ctrl.alloc_dgain = gc5613_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = gc5613_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list gc5613_init_regs_2880_1620_30fps_mipi[] = {
	//release_liner_v1.4_2lane_2880x1620(window)_30fps_24mhz_GC5613
	//version 1.4
	//mclk 24Mhz
	//mipi 2 lane  848Mbps/lane
	//vts = 1750,row_time=19.04762us
	//window 2880 1620
	//BGGR
	{0x03fe,0xf0},
	{0x03fe,0x00},
	{0x03fe,0x10},
	{0x03fe,0x00},
	{0x0a38,0x02},
	{0x0a38,0x03},
	{0x0a20,0x07},
	{0x061b,0x03},
	{0x061c,0x50},
	{0x061d,0x05},
	{0x061e,0x7e},
	{0x061f,0x03},
	{0x0a21,0x08},
	{0x0a34,0x40},
	{0x0a35,0x11},
	{0x0a36,0x6a},
	{0x0a37,0x03},
	{0x0314,0x50},
	{0x0315,0x32},
	{0x031c,0xce},
	{0x0219,0x47},
	{0x0342,0x04},//hts = 0x4b0 = 1200
	{0x0343,0xb0},//
	{0x0340,0x06},//vts = 0x6d6 = 1750
	{0x0341,0xd6},//
	{0x0345,0x02},
	{0x0347,0x18},
	{0x0348,0x0b},
	{0x0349,0x98},
	{0x034a,0x06},
	{0x034b,0x5c},
	{0x0094,0x0b},
	{0x0095,0x40},
	{0x0096,0x06},
	{0x0097,0x54},
	{0x0099,0x04},
	{0x009b,0x28},
	{0x060c,0x01},
	{0x060e,0xd2},
	{0x060f,0x05},
	{0x070c,0x01},
	{0x070e,0xd2},
	{0x070f,0x05},
	{0x0709,0x40},
	{0x0719,0x40},
	{0x0909,0x07},
	{0x0902,0x04},
	{0x0904,0x0b},
	{0x0907,0x54},
	{0x0908,0x06},
	{0x0903,0x9d},
	{0x072a,0x1c},
	{0x072b,0x1c},
	{0x0724,0x2b},
	{0x0727,0x2b},
	{0x1466,0x18},
	{0x1467,0x15},
	{0x1468,0x15},
	{0x1469,0x70},
	{0x146a,0xe8},
	{0x0707,0x07},
	{0x0737,0x0f},
	{0x0704,0x01},
	{0x0706,0x02},
	{0x0716,0x02},
	{0x0708,0xc8},
	{0x0718,0xc8},
	{0x061a,0x02},
	{0x1430,0x80},
	{0x1407,0x10},
	{0x1408,0x16},
	{0x1409,0x03},
	{0x1438,0x01},
	{0x02ce,0x03},
	{0x0245,0xc9},
	{0x023a,0x08},
	{0x02cd,0x88},
	{0x0612,0x02},
	{0x0613,0xc7},
	{0x0243,0x03},
	{0x0089,0x03},
	{0x0002,0xab},
	{0x0040,0xa3},
	{0x0075,0x64},
	{0x0004,0x0f},
	{0x0053,0x0a},
	{0x0205,0x0c},
	{0x0a67,0x80},
	{0x0a54,0x0e},
	{0x0a65,0x10},
	{0x0a98,0x04},
	{0x05be,0x00},
	{0x05a9,0x01},
	{0x0023,0x00},
	{0x0022,0x00},
	{0x0025,0x00},
	{0x0024,0x00},
	{0x0028,0x0b},
	{0x0029,0x98},
	{0x002a,0x06},
	{0x002b,0x86},
	{0x0a83,0xe0},
	{0x0a72,0x02},
	{0x0a73,0x60},
	{0x0a75,0x41},
	{0x0a70,0x03},
	{0x0a5a,0x80},
	{0x0181,0x30},
	{0x0182,0x05},
	{0x0185,0x01},
	{0x0180,0x46},
	{0x0100,0x08},
	{0x010d,0x10},
	{0x010e,0x0e},
	{0x0113,0x02},
	{0x0114,0x01},
	{0x0115,0x10},
	{SENSOR_REG_DELAY, 0x20},
	{0x05be,0x01},
	{0x0a70,0x00},
	{0x0080,0x02},
	{0x0a67,0x00},
	{0x0052,0x02},
	{0x0076,0x01},
	{0x021a,0x10},
	{0x0049,0x0f},
	{0x004a,0x3c},
	{0x004b,0x00},
	{0x0430,0x25},
	{0x0431,0x25},
	{0x0432,0x25},
	{0x0433,0x25},
	{0x0434,0x59},
	{0x0435,0x59},
	{0x0436,0x59},
	{0x0437,0x59},
	{0x0100,0x09},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list gc5613_init_regs_2880_1620_25fps_mipi[] = {
	//01_GC5613_MIPI2L_24M_2880x1620_25fps_raw10_linear
	//mclk 24Mhz
	//mipi 2 lane  720Mbps/lane
	//vts = 1867  ,row time=21.43us
	//window 2880 1620
	//BGGR
	{0x03fe,0xf0},
	{0x03fe,0x00},
	{0x03fe,0x10},
	{0x03fe,0x00},
	{0x0a38,0x02},
	{0x0a38,0x03},
	{0x0a20,0x07},
	{0x061b,0x09},
	{0x061c,0x50},
	{0x061d,0x05},
	{0x061e,0x70},
	{0x061f,0x03},
	{0x0a21,0x08},
	{0x0a34,0x40},
	{0x0a35,0x11},
	{0x0a36,0x5a},
	{0x0a37,0x03},
	{0x0314,0x50},
	{0x0315,0x32},
	{0x031c,0xce},
	{0x0219,0x47},
	{0x0342,0x04},//hts = 0x4b0 = 1200
	{0x0343,0xb0},//
	{0x0259,0x06},
	{0x025a,0x8a},
	{0x0340,0x07},//vts = 0x74b = 1867
	{0x0341,0x4b},//
	{0x0345,0x02},
	{0x0347,0x02},
	{0x0348,0x0b},
	{0x0349,0x98},
	{0x034a,0x06},
	{0x034b,0x8a},
	{0x0094,0x0b},
	{0x0095,0x40},
	{0x0096,0x06},
	{0x0097,0x54},
	{0x0099,0x1a},
	{0x009b,0x2c},
	{0x060c,0x01},
	{0x060e,0xd2},
	{0x060f,0x05},
	{0x070c,0x01},
	{0x070e,0xd2},
	{0x070f,0x05},
	{0x0709,0x40},
	{0x0719,0x40},
	{0x0909,0x07},
	{0x0902,0x04},
	{0x0904,0x0b},
	{0x0907,0x54},
	{0x0908,0x06},
	{0x0903,0x9d},
	{0x072a,0x1c},
	{0x072b,0x1c},
	{0x0724,0x2b},
	{0x0727,0x2b},
	{0x1466,0x18},
	{0x1467,0x15},
	{0x1468,0x15},
	{0x1469,0x70},
	{0x146a,0xe8},
	{0x0707,0x07},
	{0x0737,0x0f},
	{0x0704,0x01},
	{0x0706,0x02},
	{0x0716,0x02},
	{0x0708,0xc8},
	{0x0718,0xc8},
	{0x061a,0x02},
	{0x1430,0x80},
	{0x1407,0x10},
	{0x1408,0x16},
	{0x1409,0x03},
	{0x1412,0x12},
	{0x1438,0x01},
	{0x02ce,0x03},
	{0x0245,0xc9},
	{0x023a,0x08},
	{0x02cd,0x88},
	{0x0612,0x02},
	{0x0613,0xc7},
	{0x0243,0x03},
	{0x0089,0x03},
	{0x0002,0xab},
	{0x0040,0xa3},
	{0x0075,0x64},
	{0x0004,0x0f},
	{0x0053,0x0a},
	{0x0205,0x0c},
	{0x0052,0x02},
	{0x0076,0x01},
	{0x021a,0x10},
	{0x0049,0x0f}, //darkrow select
	{0x004a,0x3c},
	{0x004b,0x00},
	{0x0430,0x25},
	{0x0431,0x25},
	{0x0432,0x25},
	{0x0433,0x25},
	{0x0434,0x59},
	{0x0435,0x59},
	{0x0436,0x59},
	{0x0437,0x59},
	{0x0181,0x30},
	{0x0182,0x05},
	{0x0185,0x01},
	{0x0180,0x46},
	{0x0100,0x08},
	{0x010d,0x10},
	{0x010e,0x0e},
	{0x0113,0x02},
	{0x0114,0x01},
	{0x0115,0x10},
	{0x0125,0x10},
	{0x0124,0x02},
	{0x0122,0x05},
	{0x0123,0x18},
	{0x0126,0x07},
	{0x0121,0x10},
	{0x0129,0x06},
	{0x012a,0x08},
	{0x012b,0x08},
	{0x031f,0x01},
	{0x031f,0x00},
	{0x0317,0x1c},
	{0x0a67,0x80},
	{0x0a50,0x80},
	{0x0a53,0x00},
	{0x0a51,0x80},
	{0x0a54,0xa0},
	{0x0a63,0x20},
	{0x0a65,0x20},
	{0x0a68,0x21},
	{0x05be,0x00},
	{0x05a9,0x01},
	{0x0024,0x00},
	{0x0025,0x00},
	{0x0028,0x0b},
	{0x0029,0x98},
	{0x002a,0x06},
	{0x002b,0x8a},
	{0x0a82,0x00},
	{0x0a83,0xe0},
	{0x0a72,0x02},
	{0x0a73,0x60},
	{0x0a80,0x01},
	{0x0a70,0x03},
	{0x0a75,0x41},
	{0x0a5a,0x80},
	{SENSOR_REG_DELAY, 0x20},
	{0x05be,0x01},
	{0x0a70,0x00},
	{0x0080,0x02},
	{0x0021,0x40},
	{0x0a67,0x00},
	{0x0100,0x09},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the gc5613_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting gc5613_win_sizes[] = {
	{
                .width                  = 2880,
                .height                 = 1620,
                .fps                    = 25 << 16 | 1,
                .mbus_code              = TISP_VI_FMT_SBGGR10_1X10,
                .colorspace             = TISP_COLORSPACE_SRGB,
                .regs                   = gc5613_init_regs_2880_1620_25fps_mipi,
        },
	{
		.width			= 2880,
		.height		= 1620,
		.fps			= 30 << 16 | 1,
		.mbus_code		= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace		= TISP_COLORSPACE_SRGB,
		.regs			= gc5613_init_regs_2880_1620_30fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &gc5613_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list gc5613_stream_on_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list gc5613_stream_off_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int gc5613_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int gc5613_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int gc5613_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc5613_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int gc5613_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc5613_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif	/* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int gc5613_read(struct tx_isp_subdev *sd, uint16_t reg,unsigned char *value) {
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

int gc5613_write(struct tx_isp_subdev *sd, uint16_t reg,unsigned char value)
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
static int gc5613_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc5613_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int gc5613_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc5613_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif	/* SENSOR_I2C_REG_16BIT */

static int gc5613_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned long rate = 0;
	struct clk *pll = NULL;
	char *plls[] = {"mpll", "sclka"};
	int psize = sizeof(plls) / sizeof(char *);
	char *ppll = plls[psize - 1];
	int ret = 0, i = 0;

	if (mclk == 24000000) {
		ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
		goto set_clk;
	}

	pll = clk_get_parent(sclka);
	rate = clk_get_rate(pll);
	if (rate % mclk) {
		for (i = 0; i < psize; i++) {
			pll = clk_get(NULL, plls[i]);
			rate = clk_get_rate(pll);
			if (!(rate % mclk)) {
				ret = clk_set_parent(sclka, pll);
				if (ret) {
					ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
						    __func__, __LINE__, plls[i]);
					continue;
				} else {
					break;
				}
			}
		}
		if (i == psize) {
			if (!ret) {
				pll = clk_get(NULL, ppll);
				rate = clk_get_rate(pll);
				if(mclk == 37125000){
					if (rate >= 891000000) {
						rate = 891000000;
					} else {
						ISP_ERROR("[%s %d] The %s clock setting failed !!!\n",
							  __func__, __LINE__, ppll);
						ret = -1;
						goto error;
					}
				} else if (mclk == 27000000) {
					rate -= rate % mclk;
				} else {
					ret = -1;
					goto error;
				}
				ret = private_clk_set_rate(pll, rate);
				if (ret) {
					ISP_WARNING("[%s %d] Failed to set %s !!!\n",
						    __func__, __LINE__, ppll);
					goto error;
				} else {
					ISP_WARNING("[%s %d] !!!!!!!!!!! The %s frequency has been changed to %ld !!!\n",
						    __func__, __LINE__, ppll, rate);
				}
				ret = clk_set_parent(sclka, pll);
				if (ret) {
					ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
						    __func__, __LINE__, ppll);
					goto error;
				}
			} else {
				goto error;
			}
		}
	}
set_clk:
	private_clk_set_rate(sensor->mclk, mclk);
	private_clk_prepare_enable(sensor->mclk);

	rate = clk_get_rate(sensor->mclk);
	if (rate % mclk) {
		ret = -1;
		goto error;
	}

	return ret;

error:
	ISP_ERROR("[%s %d] Unable to allocate the required MCLK %ld !!!\n",
		  __func__, __LINE__, mclk);
	return ret;
}

static int gc5613_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int gc5613_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
                wsize = &gc5613_win_sizes[0];
                gc5613_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

                gc5613_attr.again.value = 65536;
                gc5613_attr.again.max = 390142;
                gc5613_attr.again.min = 0;
                gc5613_attr.again.apply_delay = 2;

                gc5613_attr.integration_time.value = 0x127;
                gc5613_attr.integration_time.max = 1867 - 8;
                gc5613_attr.integration_time.min = 2;
                gc5613_attr.integration_time.apply_delay = 2;

                gc5613_attr.total_width =1200;
                gc5613_attr.total_height = 1867;

                gc5613_attr.expo_fs = 1;

#ifdef SENSOR_HCG
                gc5613_attr.hcg.base_gain = ;
                gc5613_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
                gc5613_attr.again_short.value = ;
                gc5613_attr.again_short.max = ;
                gc5613_attr.again_short.min = ;
                gc5613_attr.again_short.apply_delay = ;

                gc5613_attr.integration_time_short.value = ;
                gc5613_attr.integration_time_short.max = ;
                gc5613_attr.integration_time_short.min = ;
                gc5613_attr.integration_time_short.apply_delay = ;

                gc5613_attr.wdr_cache = wdr_line * gc5613_attr.total_width;

#ifdef SENSOR_HCG
                gc5613_attr.hcg_short.base_gain = ;
                gc5613_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

                memcpy((void *)(&(gc5613_attr.mipi)), (void *)(&gc5613_mipi_linear), sizeof(gc5613_attr.mipi));
                gc5613_attr.mipi.clk = 720;
                break;
	case 1:
		wsize = &gc5613_win_sizes[1];
		gc5613_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		gc5613_attr.again.value = 65536;
		gc5613_attr.again.max = 390142;
		gc5613_attr.again.min = 0;
		gc5613_attr.again.apply_delay = 2;

		gc5613_attr.integration_time.value = 0x127;
		gc5613_attr.integration_time.max = 1750 - 8;
		gc5613_attr.integration_time.min = 2;
		gc5613_attr.integration_time.apply_delay = 2;

		gc5613_attr.total_width =1200;
		gc5613_attr.total_height = 1750;

		gc5613_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		gc5613_attr.hcg.base_gain = ;
		gc5613_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		gc5613_attr.again_short.value = ;
		gc5613_attr.again_short.max = ;
		gc5613_attr.again_short.min = ;
		gc5613_attr.again_short.apply_delay = ;

		gc5613_attr.integration_time_short.value = ;
		gc5613_attr.integration_time_short.max = ;
		gc5613_attr.integration_time_short.min = ;
		gc5613_attr.integration_time_short.apply_delay = ;

		gc5613_attr.wdr_cache = wdr_line * gc5613_attr.total_width;

#ifdef SENSOR_HCG
		gc5613_attr.hcg_short.base_gain = ;
		gc5613_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(gc5613_attr.mipi)), (void *)(&gc5613_mipi_linear), sizeof(gc5613_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int gc5613_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	gc5613_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		gc5613_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc5613_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		gc5613_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc5613_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		gc5613_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = gc5613_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.apply_delay = 1;
	gc5613_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int gc5613_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = gc5613_read(sd, 0x03f0, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = gc5613_read(sd, 0x03f1, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int gc5613_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	gc5613_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "gc5613_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "gc5613_pwdn");
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
	ret = gc5613_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc5613 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("GC5613 version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", gc5613_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
		    info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "gc5613", sizeof("gc5613"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int gc5613_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int gc5613_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = gc5613_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int gc5613_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = gc5613_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int gc5613_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc5613_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int gc5613_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = gc5613_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = gc5613_write_array(sd, gc5613_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("gc5613 stream on\n");
		}

	} else {
		ret = gc5613_write_array(sd, gc5613_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("gc5613 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int gc5613_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;
	struct again_lut *val_lut = gc5613_again_lut;

	/*set integration time*/
	ret = gc5613_write(sd, 0x0203, it & 0xff);
	ret += gc5613_write(sd, 0x0202, it >> 8);

	/*set sensor analog gain*/
	ret += gc5613_write(sd, 0x031d ,0x2d);
	ret += gc5613_write(sd, 0x0614, val_lut[again].reg0614);
	ret += gc5613_write(sd, 0x0615, val_lut[again].reg0615);
	ret += gc5613_write(sd, 0x0225, val_lut[again].reg0225);
	ret += gc5613_write(sd, 0x031d, 0x28);
	ret += gc5613_write(sd, 0x1467, val_lut[again].reg1467);
	ret += gc5613_write(sd, 0x1468, val_lut[again].reg1468);
	ret += gc5613_write(sd, 0x00b8, val_lut[again].reg00b8);
	ret += gc5613_write(sd, 0x00b9, val_lut[again].reg00b9);

	return ret;
}
#else
static int gc5613_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	/*set integration time*/
	ret = gc5613_write(sd, 0x0203, value & 0xff);
	ret += gc5613_write(sd, 0x0202, value >> 8);

	return ret;
}

static int gc5613_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	struct again_lut *val_lut = gc5613_again_lut;

	/*set sensor analog gain*/
	ret += gc5613_write(sd, 0x031d ,0x2d);
	ret += gc5613_write(sd, 0x0614, val_lut[again].reg0614);
	ret += gc5613_write(sd, 0x0615, val_lut[again].reg0615);
	ret += gc5613_write(sd, 0x0225, val_lut[again].reg0225);
	ret += gc5613_write(sd, 0x031d, 0x28);
	ret += gc5613_write(sd, 0x1467, val_lut[again].reg1467);
	ret += gc5613_write(sd, 0x1468, val_lut[again].reg1468);
	ret += gc5613_write(sd, 0x00b8, val_lut[again].reg00b8);
	ret += gc5613_write(sd, 0x00b9, val_lut[again].reg00b9);

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc5613_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc5613_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc5613_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = gc5613_attr_set(sd, wsize);
	}

	return ret;
}

static int gc5613_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 1200 * 1867 * 25;  /**< HTS * VTS * FPS */
		max_fps = 25;
		break;
	case 1:
		sclk = 1200 * 1750 * 30;  /**< HTS * VTS * FPS */
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

	val = 0;
	ret += gc5613_read(sd, 0x0342, &val);
	hts = (val & 0x0f) << 8;
	val = 0;
	ret += gc5613_read(sd, 0x0343, &val);
	hts  = (hts | val);
	if (0 != ret) {
		ISP_ERROR("err: gc5613 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	gc5613_write(sd, 0x0340, (unsigned char) ((vts >> 8) & 0x3f));
	gc5613_write(sd, 0x0341, (unsigned char) (vts & 0xff));

	gc5613_write(sd, 0x0259, (unsigned char)(((vts - 32) >> 8 ) & 0x3f));
	gc5613_write(sd, 0x025a, (unsigned char)((vts - 32) & 0xff));

	if (0 != ret) {
		ISP_ERROR("err: gc5613_write err\n");
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

static int gc5613_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	int ret = ISP_SUCCESS;
	unsigned char val = 0;
	unsigned char val1 = 0;
	unsigned char val2 = 0;
	unsigned char reg_0x0348 =0;
	unsigned char reg_0x0349 =0;
	unsigned char reg_0x034a =0;
	unsigned char reg_0x034b =0;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	ret = gc5613_read(sd, 0x022c, &val);
	ret = gc5613_read(sd, 0x0063, &val1);
	ret = gc5613_read(sd, 0x0a73, &val2);
	ret = gc5613_read(sd, 0x0348, &reg_0x0348);
	ret = gc5613_read(sd, 0x0349, &reg_0x0349);
	ret = gc5613_read(sd, 0x034a, &reg_0x034a);
	ret = gc5613_read(sd, 0x034b, &reg_0x034b);


	val  &=  0;
	val1 &=  0;
	val2 &=  0;

	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		ret = gc5613_write(sd, 0x022c, val	| 0x00); /*mirror*/
		ret = gc5613_write(sd, 0x0063, val1 | 0x00);
		ret = gc5613_write(sd, 0x0a73, val2 | 0x60);
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		ret = gc5613_write(sd, 0x022c, val	| 0x01); /*mirror*/
		ret = gc5613_write(sd, 0x0063, val1 | 0x01);
		ret = gc5613_write(sd, 0x0a73, val2 | 0x61);
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		ret = gc5613_write(sd, 0x022c, val	| 0x02); /*filp*/
		ret = gc5613_write(sd, 0x0063, val1 | 0x02);
		ret = gc5613_write(sd, 0x0a73, val2 | 0x62);
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		ret = gc5613_write(sd, 0x022c, val	| 0x03); /*mirror & filp*/
		ret = gc5613_write(sd, 0x0063, val1 | 0x03);
		ret = gc5613_write(sd, 0x0a73, val2 | 0x63);
		break;
	}

	ret = gc5613_write(sd, 0x031f,0x01);
	ret = gc5613_write(sd, 0x031f,0x00);
	ret = gc5613_write(sd, 0x0317,0x1c);
	ret = gc5613_write(sd, 0x0a67,0x80);
	ret = gc5613_write(sd, 0x0a50,0x80);
	ret = gc5613_write(sd, 0x0a53,0x00);
	ret = gc5613_write(sd, 0x0a51,0x80);
	ret = gc5613_write(sd, 0x0a54,0xa0);
	ret = gc5613_write(sd, 0x0a63,0x20);
	ret = gc5613_write(sd, 0x0a65,0x20);
	ret = gc5613_write(sd, 0x0a68,0x21);
	ret = gc5613_write(sd, 0x05be,0x00);
	ret = gc5613_write(sd, 0x05a9,0x01);
	ret = gc5613_write(sd, 0x0024,0x00);
	ret = gc5613_write(sd, 0x0025,0x00);
	ret = gc5613_write(sd, 0x0028,reg_0x0348);
	ret = gc5613_write(sd, 0x0029,reg_0x0349);
	ret = gc5613_write(sd, 0x002a,reg_0x034a);
	ret = gc5613_write(sd, 0x002b,reg_0x034b);
	ret = gc5613_write(sd, 0x0a82,0x00);
	ret = gc5613_write(sd, 0x0a83,0xe0);
	ret = gc5613_write(sd, 0x0a72,0x02);
	ret = gc5613_write(sd, 0x0a73,val2);
	ret = gc5613_write(sd, 0x0a80,0x01);
	ret = gc5613_write(sd, 0x0a70,0x03);
	ret = gc5613_write(sd, 0x0a75,0x41);
	ret = gc5613_write(sd, 0x0a5a,0x80);

	private_msleep(0x14);
	ret = gc5613_write(sd, 0x05be,0x01);
	ret = gc5613_write(sd, 0x0a70,0x00);
	ret = gc5613_write(sd, 0x0080,0x02);
	ret = gc5613_write(sd, 0x0021,0x40);
	ret = gc5613_write(sd, 0x0a67,0x00);
	ret = gc5613_write(sd, 0x0100,0x09);

	sensor->video.hvflip_mode = par->hvflip;
	gc5613_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int gc5613_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int gc5613_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int gc5613_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc5613_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = gc5613_write_array(sd, gc5613_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		gc5613_setting_select(sd, 1);
		gc5613_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		gc5613_setting_select(sd, 0);
		gc5613_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int gc5613_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = gc5613_write_array(sd, wsize->regs);
	ret = gc5613_write_array(sd, gc5613_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int gc5613_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = gc5613_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = gc5613_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = gc5613_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = gc5613_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = gc5613_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = gc5613_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = gc5613_write_array(sd, gc5613_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = gc5613_write_array(sd, gc5613_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = gc5613_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = gc5613_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = gc5613_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = gc5613_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = gc5613_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = gc5613_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = gc5613_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops gc5613_core_ops = {
	.g_chip_ident = gc5613_g_chip_ident,
	.reset = gc5613_reset,
	.init = gc5613_init,
	.g_register = gc5613_g_register,
	.s_register = gc5613_s_register,
};

static struct tx_isp_subdev_video_ops gc5613_video_ops = {
	.s_stream = gc5613_s_stream,
};

static struct tx_isp_subdev_sensor_ops gc5613_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl	= gc5613_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops gc5613_ops = {
	.core = &gc5613_core_ops,
	.video = &gc5613_video_ops,
	.sensor = &gc5613_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "gc5613",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc5613_probe(struct i2c_client *client,
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
	sensor->video.attr = &gc5613_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc5613_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->gc5613\n");

	return 0;
}

static int gc5613_remove(struct i2c_client *client)
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

static const struct i2c_device_id gc5613_id[] = {
	{"gc5613", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, gc5613_id);
#endif	/* CONFIG_ZERATUL */

static struct i2c_driver gc5613_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner	= NULL,
#else
		.owner	= THIS_MODULE,
#endif	/* CONFIG_ZERATUL */
		.name	= "gc5613",
	},
	.probe			= gc5613_probe,
	.remove			= gc5613_remove,
	.id_table		= gc5613_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_gc5613(void) {
	return private_i2c_add_driver(&gc5613_driver);
}

static __exit void exit_gc5613(void) {
	private_i2c_del_driver(&gc5613_driver);
}

module_init(init_gc5613);
module_exit(exit_gc5613);
#endif	/* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_gc5613(void) {
	return private_i2c_add_driver(&gc5613_driver);
}

static void exit_first_gc5613(void) {
	private_i2c_del_driver(&gc5613_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "gc5613",
	.i2c_addr = 0x31,
	.width = 2880,
	.height = 1620,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_gc5613,
	.exit_sensor = exit_first_gc5613
};
#endif	/* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for gc5613 sensor");
MODULE_LICENSE("GPL");
