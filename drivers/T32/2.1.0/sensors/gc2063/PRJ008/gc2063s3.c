/*
 * gc2063s3.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps		dvdd
 *  0           1920*1080       30        mipi_2lane    linear  10        30
 *  1           1920*1080       15        mipi_2lane    linear  10        15
 *
 * @I2C addr:0x37
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

#define SENSOR_VERSION  "H20260130a"

//#define SENSOR_TEST
#define SENSOR_SLAVE
//#define SENSOR_PM

#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
//#define SENSOR_HCG
//#define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x20)
#define SENSOR_CHIP_ID_L    (0x53)
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
#define SENSOR_REG_END    0x8f
#define SENSOR_REG_DELAY  0x8e
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

struct tx_isp_sensor_attribute gc2063s3_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	int index;
	unsigned int regb4;
	unsigned int regb3;
	unsigned int regb8;
	unsigned int regb9;
	unsigned int gain;
};

struct again_lut gc2063s3_again_lut[] = {
	//inx, 0xb4 0xb3 0xb8 0xb9 gain
	{0x00, 0x0, 0x00, 0x01, 0x00, 0},       //1.000000
	{0x01, 0x0, 0x10, 0x01, 0x0c, 13726},   //1.156250
	{0x02, 0x0, 0x20, 0x01, 0x1b, 31177},   //1.390625
	{0x03, 0x0, 0x30, 0x01, 0x2c, 44067},   //1.593750
	{0x04, 0x0, 0x40, 0x01, 0x3f, 64793},   //1.984375
	{0x05, 0x0, 0x50, 0x02, 0x16, 78621},   //2.296875
	{0x06, 0x0, 0x60, 0x02, 0x35, 96180},   //2.765625
	{0x07, 0x0, 0x70, 0x03, 0x16, 109138},  //3.171875
	{0x08, 0x0, 0x80, 0x04, 0x02, 132537},  //4.062500
	{0x09, 0x0, 0x90, 0x04, 0x31, 146067},  //4.687500
	{0x0a, 0x0, 0xa0, 0x05, 0x32, 163567},  //5.640625
	{0x0b, 0x0, 0xb0, 0x06, 0x35, 176747},  //6.484375
	{0x0c, 0x0, 0xc0, 0x08, 0x04, 195118},  //7.875000
	{0x0d, 0x0, 0x5a, 0x09, 0x19, 208560},  //9.078125
	{0x0e, 0x0, 0x83, 0x0b, 0x0f, 229103},  //11.281250
	{0x0f, 0x0, 0x93, 0x0d, 0x12, 242511},  //13.000000
	{0x10, 0x0, 0x84, 0x10, 0x00, 262419},  //16.046875
	{0x11, 0x0, 0x94, 0x12, 0x3a, 275710},  //18.468750
	{0x12, 0x1, 0x2c, 0x1a, 0x02, 292252},  //22.000000
	{0x13, 0x1, 0x3c, 0x1b, 0x20, 305571},  //25.328125
	{0x14, 0x0, 0x8c, 0x20, 0x0f, 324962},  //31.093750
	{0x15, 0x0, 0x9c, 0x26, 0x07, 338280},  //35.796875
	{0x16, 0x2, 0x64, 0x36, 0x21, 358923},  //44.531250
	{0x17, 0x2, 0x74, 0x37, 0x3a, 372267},  //51.281250
	{0x18, 0x0, 0xc6, 0x3d, 0x02, 392101},  //63.250000
	{0x19, 0x0, 0xdc, 0x3f, 0x3f, 415415},  //80.937500
	{0x1a, 0x2, 0x85, 0x3f, 0x3f, 421082},  //85.937500
	{0x1b, 0x2, 0x95, 0x3f, 0x3f, 440360},  //105.375000
	{0x1c, 0x0, 0xce, 0x3f, 0x3f, 444864},  //110.515625
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int gc2063s3_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = gc2063s3_again_lut;
	while (lut->gain <= gc2063s3_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].index;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == gc2063s3_attr.again.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
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
unsigned int gc2063s3_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = gc2063s3_again_lut;
	while(lut->gain <= gc2063s3_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == gc2063s3_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int gc2063s3_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int gc2063s3_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int gc2063s3_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus gc2063s3_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 780,
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

struct tx_isp_dvp_bus gc2063s3_dvp = {
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

struct tx_isp_sensor_attribute gc2063s3_attr = {
	.name = "gc2063s3",
	.chip_id = 0x2053,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x3f,
	.sensor_ctrl.alloc_again = gc2063s3_alloc_again,
	.sensor_ctrl.alloc_dgain = gc2063s3_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = gc2063s3_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list gc2063s3_init_regs_1920_1080_30fps_mipi[] = {
	/* mclk=24mhz,mipi data rate=390mbps/lane, row_time=28.2us frame length=1418,25fps*/
	/*system*/
	{0xfe, 0x80},
	{0xfe, 0x80},
	{0xfe, 0x80},
	{0xfe, 0x00},
	{0xf2, 0x00},
	{0xf3, 0x00},
	{0xf4, 0x36},
	{0xf5, 0xc0},
	{0xf6, 0x44},
	{0xf7, 0x01},
	{0xf8, 0x68},
	{0xf9, 0x40},
	{0xfc, 0x8e},
	/*CISCTL & ANALOG*/
	{0xfe, 0x00},
	{0x87, 0x18},
	{0xee, 0x30},
	{0xd0, 0xb7},
	{0x03, 0x04},
	{0x04, 0x60},
	{0x05, 0x04},//
	{0x06, 0x4c},//0x44c = 1100
	{0x07, 0x00},
	{0x08, 0x11},
	{0x09, 0x00},
	{0x0a, 0x02},
	{0x0b, 0x00},
	{0x0c, 0x02},
	{0x0d, 0x04},
	{0x0e, 0x40},
	{0x12, 0xe2},
	{0x13, 0x16},
	{0x19, 0x0a},
	{0x21, 0x1c},
	{0x28, 0x0a},
	{0x29, 0x24},
	{0x2b, 0x04},
	{0x32, 0xf8},
	{0x37, 0x03},
	{0x39, 0x15},
	{0x43, 0x07},
	{0x44, 0x40},
	{0x46, 0x0b},
	{0x4b, 0x20},
	{0x4e, 0x08},
	{0x55, 0x20},
	{0x66, 0x05},
	{0x67, 0x05},
	{0x77, 0x01},
	{0x78, 0x00},
	{0x7c, 0x93},
	{0x8c, 0x12},
	{0x8d, 0x92},
	{0x90, 0x00},/*use frame length to change fps*/
	{0x41, 0x04},
	{0x42, 0x9d},/*0x58a:25fps  0x49d:30fps*/
	{0x9d, 0x10},
	{0xce, 0x7c},
	{0xd2, 0x41},
	{0xd3, 0xdc},
	{0xe6, 0x50},
	/*gain*/
	{0xb6, 0xc0},
	{0xb0, 0x70},
	{0xb1, 0x01},
	{0xb2, 0x00},
	{0xb3, 0x00},
	{0xb4, 0x00},
	{0xb8, 0x01},
	{0xb9, 0x00},
	/*blk*/
	{0x26, 0x30},
	{0xfe, 0x01},
	{0x40, 0x23},
	{0x55, 0x07},
	{0x60, 0x40},
	{0xfe, 0x04},
	{0x14, 0x78},
	{0x15, 0x78},
	{0x16, 0x78},
	{0x17, 0x78},
	/*window*/
	{0xfe, 0x01},
	{0x92, 0x00},
	{0x94, 0x03},
	{0x95, 0x04},
	{0x96, 0x38},
	{0x97, 0x07},
	{0x98, 0x80},
	/*ISP*/
	{0xfe, 0x01},
	{0x01, 0x05},
	{0x02, 0x89},
	{0x04, 0x01},
	{0x07, 0xa6},
	{0x08, 0xa9},
	{0x09, 0xa8},
	{0x0a, 0xa7},
	{0x0b, 0xff},
	{0x0c, 0xff},
	{0x0f, 0x00},
	{0x50, 0x1c},
	{0x89, 0x03},
	{0xfe, 0x04},
	{0x28, 0x86},
	{0x29, 0x86},
	{0x2a, 0x86},
	{0x2b, 0x68},
	{0x2c, 0x68},
	{0x2d, 0x68},
	{0x2e, 0x68},
	{0x2f, 0x68},
	{0x30, 0x4f},
	{0x31, 0x68},
	{0x32, 0x67},
	{0x33, 0x66},
	{0x34, 0x66},
	{0x35, 0x66},
	{0x36, 0x66},
	{0x37, 0x66},
	{0x38, 0x62},
	{0x39, 0x62},
	{0x3a, 0x62},
	{0x3b, 0x62},
	{0x3c, 0x62},
	{0x3d, 0x62},
	{0x3e, 0x62},
	{0x3f, 0x62},
	/****DVP & MIPI****/
	{0xfe, 0x01},
	{0x9a, 0x06},
	{0xfe, 0x00},
	{0x7b, 0x2a},
	{0x23, 0x2d},
	{0xfe, 0x03},
	{0x01, 0x27},
	{0x02, 0x5b},/*mipi drv cap*/
	{0x03, 0x8e},/*0xb6*/
	{0x12, 0x80},
	{0x13, 0x07},
	{0x15, 0x10},
	{0x29, 0x05},/*mipi pre*/
	{0x2a, 0x0a},/*mipi zero*/
	{0xfe, 0x00},
	{0x3e, 0x91},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list gc2063s3_init_regs_1920_1080_15fps_mipi[] = {
	/*system*/
	{0xfe, 0x80},
	{0xfe, 0x80},
	{0xfe, 0x80},
	{0xfe, 0x00},
	{0xf2, 0x00},
	{0xf3, 0x00},
	{0xf4, 0x36},
	{0xf5, 0xc0},
	{0xf6, 0x44},
	{0xf7, 0x03},
	{0xf8, 0x68},
	{0xf9, 0x40},
	{0xfc, 0x8e},
	/*CISCTL & ANALOG*/
	{0xfe, 0x00},
	{0x87, 0x18},
	{0xee, 0x30},
	{0xd0, 0xb7},
	{0x03, 0x04},
	{0x04, 0x60},
	{0x05, 0x04},
	{0x06, 0x4c},
	{0x07, 0x00},
	{0x08, 0x11},
	{0x09, 0x00},
	{0x0a, 0x02},
	{0x0b, 0x00},
	{0x0c, 0x02},
	{0x0d, 0x04},
	{0x0e, 0x40},
	{0x12, 0xe2},
	{0x13, 0x16},
	{0x19, 0x0a},
	{0x21, 0x1c},
	{0x28, 0x0a},
	{0x29, 0x24},
	{0x2b, 0x04},
	{0x32, 0xf8},
	{0x37, 0x03},
	{0x39, 0x15},
	{0x43, 0x07},
	{0x44, 0x40},
	{0x46, 0x0b},
	{0x4b, 0x20},
	{0x4e, 0x08},
	{0x55, 0x20},
	{0x66, 0x05},
	{0x67, 0x05},
	{0x77, 0x01},
	{0x78, 0x00},
	{0x7c, 0x93},
	{0x8c, 0x12},
	{0x8d, 0x92},
	{0x90, 0x00},/*use frame length to change fps*/
	{0x41, 0x04},
	{0x42, 0x9d},/*vts for max 15fps mipi*/
	{0x9d, 0x10},
	{0xce, 0x7c},
	{0xd2, 0x41},
	{0xd3, 0xdc},
	{0xe6, 0x50},
	/*gain*/
	{0xb6, 0xc0},
	{0xb0, 0x70},
	{0xb1, 0x01},
	{0xb2, 0x00},
	{0xb3, 0x00},
	{0xb4, 0x00},
	{0xb8, 0x01},
	{0xb9, 0x00},
	/*blk*/
	{0x26, 0x30},
	{0xfe, 0x01},
	{0x40, 0x23},
	{0x55, 0x07},
	{0x60, 0x40},
	{0xfe, 0x04},
	{0x14, 0x78},
	{0x15, 0x78},
	{0x16, 0x78},
	{0x17, 0x78},
	/*window*/
	{0xfe, 0x01},
	{0x92, 0x00},
	{0x94, 0x03},
	{0x95, 0x04},
	{0x96, 0x38},
	{0x97, 0x07},
	{0x98, 0x80},
	/*ISP*/
	{0xfe, 0x01},
	{0x01, 0x05},
	{0x02, 0x89},
	{0x04, 0x01},
	{0x07, 0xa6},
	{0x08, 0xa9},
	{0x09, 0xa8},
	{0x0a, 0xa7},
	{0x0b, 0xff},
	{0x0c, 0xff},
	{0x0f, 0x00},
	{0x50, 0x1c},
	{0x89, 0x03},
	{0xfe, 0x04},
	{0x28, 0x86},
	{0x29, 0x86},
	{0x2a, 0x86},
	{0x2b, 0x68},
	{0x2c, 0x68},
	{0x2d, 0x68},
	{0x2e, 0x68},
	{0x2f, 0x68},
	{0x30, 0x4f},
	{0x31, 0x68},
	{0x32, 0x67},
	{0x33, 0x66},
	{0x34, 0x66},
	{0x35, 0x66},
	{0x36, 0x66},
	{0x37, 0x66},
	{0x38, 0x62},
	{0x39, 0x62},
	{0x3a, 0x62},
	{0x3b, 0x62},
	{0x3c, 0x62},
	{0x3d, 0x62},
	{0x3e, 0x62},
	{0x3f, 0x62},
	/****DVP & MIPI****/
	{0xfe, 0x01},
	{0x9a, 0x06},
	{0xfe, 0x00},
	{0x7b, 0x2a},
	{0x23, 0x2d},
	{0xfe, 0x03},
	{0x01, 0x27},
	{0x02, 0x56},
	{0x03, 0x8e},/*0xb6*/
	{0x12, 0x80},
	{0x13, 0x07},
	{0x15, 0x10},
	{0x29, 0x03},
	{0xfe, 0x00},
	{0x3e, 0x91},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};
/*
 * the order of the gc2063s3_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting gc2063s3_win_sizes[] = {
	{
		.width          = 1920,
		.height         = 1080,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = gc2063s3_init_regs_1920_1080_30fps_mipi,
	},
	{
		.width          = 1920,
		.height         = 1080,
		.fps            = 15 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = gc2063s3_init_regs_1920_1080_15fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &gc2063s3_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list gc2063s3_stream_on_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list gc2063s3_stream_off_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int gc2063s3_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int gc2063s3_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int gc2063s3_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc2063s3_read(sd, vals->reg_num, &val);
			/* ISP_WARNING("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int gc2063s3_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc2063s3_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int gc2063s3_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int gc2063s3_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int gc2063s3_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc2063s3_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int gc2063s3_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc2063s3_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int gc2063s3_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int gc2063s3_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int gc2063s3_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &gc2063s3_win_sizes[0];
		gc2063s3_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		gc2063s3_attr.again.value = 65536;
		gc2063s3_attr.again.max = 444864;
		gc2063s3_attr.again.min = 0;
		gc2063s3_attr.again.apply_delay = 2;

		gc2063s3_attr.integration_time.value = 0x7b8;
		gc2063s3_attr.integration_time.max = 1181 - 8;
		gc2063s3_attr.integration_time.min = 1;
		gc2063s3_attr.integration_time.apply_delay = 2;

		gc2063s3_attr.total_width = 1100;
		gc2063s3_attr.total_height = 1181;

		gc2063s3_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
		gc2063s3_attr.hcg.base_gain = ;
		gc2063s3_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		gc2063s3_attr.again_short.value = ;
		gc2063s3_attr.again_short.max = ;
		gc2063s3_attr.again_short.min = ;
		gc2063s3_attr.again_short.apply_delay = ;

		gc2063s3_attr.integration_time_short.value = ;
		gc2063s3_attr.integration_time_short.max = ;
		gc2063s3_attr.integration_time_short.min = ;
		gc2063s3_attr.integration_time_short.apply_delay = ;

		gc2063s3_attr.wdr_cache = wdr_line * gc2063s3_attr.total_width;

#ifdef SENSOR_HCG
		gc2063s3_attr.hcg_short.base_gain = ;
		gc2063s3_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(gc2063s3_attr.mipi)), (void *)(&gc2063s3_mipi_linear), sizeof(gc2063s3_attr.mipi));
		break;
	case 1:
		wsize = &gc2063s3_win_sizes[1];
		gc2063s3_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		gc2063s3_attr.again.value = 65536;
		gc2063s3_attr.again.max = 444864;
		gc2063s3_attr.again.min = 0;
		gc2063s3_attr.again.apply_delay = 2;

		gc2063s3_attr.integration_time.value = 0x7b8;
		gc2063s3_attr.integration_time.max = 1181 - 8;
		gc2063s3_attr.integration_time.min = 1;
		gc2063s3_attr.integration_time.apply_delay = 2;

		gc2063s3_attr.total_width = 1100;
		gc2063s3_attr.total_height = 1181;

		gc2063s3_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
		gc2063s3_attr.hcg.base_gain = ;
		gc2063s3_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		gc2063s3_attr.again_short.value = ;
		gc2063s3_attr.again_short.max = ;
		gc2063s3_attr.again_short.min = ;
		gc2063s3_attr.again_short.apply_delay = ;

		gc2063s3_attr.integration_time_short.value = ;
		gc2063s3_attr.integration_time_short.max = ;
		gc2063s3_attr.integration_time_short.min = ;
		gc2063s3_attr.integration_time_short.apply_delay = ;

		gc2063s3_attr.wdr_cache = wdr_line * gc2063s3_attr.total_width;

#ifdef SENSOR_HCG
		gc2063s3_attr.hcg_short.base_gain = ;
		gc2063s3_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(gc2063s3_attr.mipi)), (void *)(&gc2063s3_mipi_linear), sizeof(gc2063s3_attr.mipi));
		gc2063s3_attr.mipi.clk = 312;
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int gc2063s3_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	gc2063s3_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		gc2063s3_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc2063s3_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		gc2063s3_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc2063s3_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		gc2063s3_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = gc2063s3_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	sensor->video.attr->fsync_attr.call_times = 1;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.apply_delay = 1;
	gc2063s3_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int gc2063s3_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = gc2063s3_read(sd, 0xf0, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = gc2063s3_read(sd, 0xf1, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int gc2063s3_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	gc2063s3_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "gc2063s3_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "gc2063s3_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = gc2063s3_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc2063s3 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
#ifdef AUTHOR
	ISP_WARNING("Template version is %s from %s\n", TVERSION, AUTHOR);
#else
	ISP_WARNING("Template version is %s\n", TVERSION);
#endif /* AUTHOR */
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", gc2063s3_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
		    info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "gc2063s3", sizeof("gc2063s3"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int gc2063s3_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int gc2063s3_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = gc2063s3_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int gc2063s3_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = gc2063s3_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int gc2063s3_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc2063s3_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int gc2063s3_fsync(struct tx_isp_subdev *sd, struct tx_isp_frame_sync *sync)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

#ifndef SENSOR_SLAVE
	/* master */
	uint8_t val;
	uint16_t ret_val;

	gc2063s3_read(sd, 0x41, &val);
	ret_val = val << 8;
	gc2063s3_read(sd, 0x42, &val);
	ret_val |= val;
	ret_val = ret_val * 2;
	gc2063s3_write(sd, 0x41, ret_val >> 8);
	gc2063s3_write(sd, 0x42, (ret_val & 0xff) + 4);
	gc2063s3_write(sd, 0xfe, 0x00);
	gc2063s3_write(sd, 0x7f, 0x09);
	gc2063s3_write(sd, 0x82, 0x01);
	gc2063s3_write(sd, 0x83, 0x7f);
	gc2063s3_write(sd, 0x84, 0x04);
#else
	/* slave */
	uint8_t val;
	uint16_t ret_val;

	gc2063s3_read(sd, 0x41, &val);
	ret_val = val << 8;
	gc2063s3_read(sd, 0x42, &val);
	ret_val |= val;
	ret_val = ret_val * 2;
	gc2063s3_write(sd, 0x41, ret_val >> 8);
	gc2063s3_write(sd, 0x42, ret_val & 0xff);
	gc2063s3_write(sd, 0xfe, 0x00);
	gc2063s3_write(sd, 0x7f, 0x09);
	gc2063s3_write(sd, 0x82, 0x0a);
	gc2063s3_write(sd, 0x83, 0x0b);
	gc2063s3_write(sd, 0x84, 0x80);
	gc2063s3_write(sd, 0x85, 0x51);
#endif /* SENSOR_SLAVE */
	sensor->video.fps.value /= 2;

	return 0;
}

static int gc2063s3_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = gc2063s3_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = gc2063s3_write_array(sd, gc2063s3_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("gc2063s3 stream on\n");
		}

	} else {
		ret = gc2063s3_write_array(sd, gc2063s3_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("gc2063s3 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int gc2063s3_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;
	struct again_lut *val_lut = gc2063s3_again_lut;

	/*set sensor reg page*/
	ret = gc2063s3_write(sd, 0xfe, 0x00);

	/*set integration time*/
	ret += gc2063s3_write(sd, 0x04, it & 0xff);
	ret += gc2063s3_write(sd, 0x03, (it & 0x3f00)>>8);

	/*set analog gain*/
	ret += gc2063s3_write(sd, 0xb4, val_lut[again].regb4);
	ret += gc2063s3_write(sd, 0xb3, val_lut[again].regb3);
	ret += gc2063s3_write(sd, 0xb8, val_lut[again].regb8);
	ret += gc2063s3_write(sd, 0xb9, val_lut[again].regb9);

	return ret;
}
#else
static int gc2063s3_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	/*set sensor reg page*/
	ret = gc2063s3_write(sd, 0xfe, 0x00);

	/*set integration time*/
	ret += gc2063s3_write(sd, 0x04, it & 0xff);
	ret += gc2063s3_write(sd, 0x03, (it & 0x3f00)>>8);

	return ret;
}

static int gc2063s3_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	/*set sensor reg page*/
	ret = gc2063s3_write(sd, 0xfe, 0x00);

	/*set analog gain*/
	ret += gc2063s3_write(sd, 0xb4, val_lut[again].regb4);
	ret += gc2063s3_write(sd, 0xb3, val_lut[again].regb3);
	ret += gc2063s3_write(sd, 0xb8, val_lut[again].regb8);
	ret += gc2063s3_write(sd, 0xb9, val_lut[again].regb9);

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc2063s3_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc2063s3_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc2063s3_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = gc2063s3_attr_set(sd, wsize);
	}

	return ret;
}

static int gc2063s3_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 0x49d * 0x44c * 30;
		max_fps = 30;
		break;
	case 1:
		sclk = 0x49d * 0x44c * 15;
		max_fps = 15;
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
	ret += gc2063s3_write(sd, 0xfe, 0x0);
	ret += gc2063s3_read(sd, 0x05, &val);
	hts = val << 8;
	val = 0;
	ret += gc2063s3_read(sd, 0x06, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: gc2063s3 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	gc2063s3_write(sd, 0x41, (unsigned char) (vts >> 8));
	gc2063s3_write(sd, 0x42, (unsigned char) (vts & 0xff));

	if (0 != ret) {
		ISP_ERROR("err: gc2063s3_write err\n");
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

static int gc2063s3_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

	par->drop_frame = 0;
	par->reset = 0;

	ret = gc2063s3_write(sd, 0xfe, 0x00);
	ret = gc2063s3_read(sd, 0x17, &val);

	/* 2'b01:mirror,2'b10:filp */
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		val &= 0xFC;
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		val = ((val & 0xFC) | 0x01);
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		val = ((val & 0xFC) | 0x02);
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		val |= 0x03;
		break;
	}
	ret += gc2063s3_write(sd, 0x17, val);

	sensor->video.hvflip_mode = par->hvflip;
	gc2063s3_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_PM
static int gc2063s3_pm(struct tx_isp_subdev *sd, void *arg)
{
	int ret = ISP_SUCCESS;
	IMPISPUserCtl *data = (IMPISPUserCtl *)arg;

	switch (data->cmd) {
	case TX_SENSOR_PM_RESUME:
		...
			if (ret < 0) {
				ISP_WARNING("TX_SENSOR_PM_RESUME error !!!\n", __func__, __LINE__);
			}
		/* ISP_WARNING("[%s %d] TX_SENSOR_PM_RESUME\n", __func__, __LINE__); */
		break;
	case TX_SENSOR_PM_SUSPEND:
		...
			if (ret < 0) {
				ISP_WARNING("TX_SENSOR_PM_SUSPEND error !!!\n", __func__, __LINE__);
			}
		/* ISP_WARNING("[%s %d] TX_SENSOR_PM_SUSPEND !!!\n", __func__, __LINE__); */
		break;
	case TX_SENSOR_PM_PREPARE:
		...
			if (ret < 0) {
				ISP_WARNING("TX_SENSOR_PM_PREPARE error !!!\n", __func__, __LINE__);
			}
		/* ISP_WARNING("[%s %d] TX_SENSOR_PM_PREPARE !!!\n", __func__, __LINE__); */
		break;
	case TX_SENSOR_PM_COMPLETE:
		...
			if (ret < 0) {
				ISP_WARNING("TX_SENSOR_PM_SUSPEND error !!!\n", __func__, __LINE__);
			}
		/* ISP_WARNING("[%s %d] TX_SENSOR_PM_COMPLETE !!!\n", __func__, __LINE__); */
		break;
	case TX_SENSOR_PM_STREAMON:
		...
			if (ret < 0) {
				ISP_WARNING("TX_SENSOR_PM_STREAMON error !!!\n", __func__, __LINE__);
			}
		/* ISP_WARNING("[%s %d] TX_SENSOR_PM_STREAMON !!!\n", __func__, __LINE__); */
		break;
	case TX_SENSOR_PM_STREAMOFF:
		...
			if (ret < 0) {
				ISP_WARNING("[%s %d] TX_SENSOR_PM_STREAMOFF error !!!\n", __func__, __LINE__);
			}
		/* ISP_WARNING("[%s %d] TX_SENSOR_PM_STREAMOFF !!!\n", __func__, __LINE__); */
		break;
	default:
		ISP_WARNING("[%s %d] Don't Support this function !!! \n", __func__, __LINE__);
		return -EINVAL;
		break;
	}

	return ret;
}
#endif /* SENSOR_PM */

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int gc2063s3_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int gc2063s3_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int gc2063s3_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc2063s3_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = gc2063s3_write_array(sd, gc2063s3_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		gc2063s3_setting_select(sd, 1);
		gc2063s3_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		gc2063s3_setting_select(sd, 0);
		gc2063s3_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int gc2063s3_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = gc2063s3_write_array(sd, wsize->regs);
	ret = gc2063s3_write_array(sd, gc2063s3_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int gc2063s3_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = gc2063s3_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = gc2063s3_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = gc2063s3_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = gc2063s3_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = gc2063s3_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = gc2063s3_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = gc2063s3_write_array(sd, gc2063s3_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = gc2063s3_write_array(sd, gc2063s3_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = gc2063s3_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = gc2063s3_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_PM
	case TX_ISP_EVENT_PM_S_STREAM_CTRL:
		if(arg)
			ret =  gc2063s3_pm(sd, arg);
		break;
#endif /* SENSOR_PM */
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = gc2063s3_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = gc2063s3_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = gc2063s3_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = gc2063s3_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = gc2063s3_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops gc2063s3_core_ops = {
	.g_chip_ident = gc2063s3_g_chip_ident,
	.reset = gc2063s3_reset,
	.init = gc2063s3_init,
	.g_register = gc2063s3_g_register,
	.s_register = gc2063s3_s_register,
	.fsync = gc2063s3_fsync,
};

static struct tx_isp_subdev_video_ops gc2063s3_video_ops = {
	.s_stream = gc2063s3_s_stream,
};

static struct tx_isp_subdev_sensor_ops gc2063s3_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = gc2063s3_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops gc2063s3_ops = {
	.core = &gc2063s3_core_ops,
	.video = &gc2063s3_video_ops,
	.sensor = &gc2063s3_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "gc2063s3",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc2063s3_probe(struct i2c_client *client,
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
	sensor->video.attr = &gc2063s3_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc2063s3_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->gc2063s3\n");

	return 0;
}

static int gc2063s3_remove(struct i2c_client *client)
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

static const struct i2c_device_id gc2063s3_id[] = {
	{"gc2063s3", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, gc2063s3_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver gc2063s3_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "gc2063s3",
	},
	.probe          = gc2063s3_probe,
	.remove         = gc2063s3_remove,
	.id_table       = gc2063s3_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_gc2063s3(void) {
	return private_i2c_add_driver(&gc2063s3_driver);
}

static __exit void exit_gc2063s3(void) {
	private_i2c_del_driver(&gc2063s3_driver);
}

module_init(init_gc2063s3);
module_exit(exit_gc2063s3);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_gc2063s3(void) {
	return private_i2c_add_driver(&gc2063s3_driver);
}

static void exit_first_gc2063s3(void) {
	private_i2c_del_driver(&gc2063s3_driver);
}

struct tx_isp_sensor_fast_attr sensor3 = {
	.name = "gc2063s3",
	.i2c_addr = 0x3f,
	.width = 1920,
	.height = 1080,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_gc2063s3,
	.exit_sensor = exit_first_gc2063s3
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A gc2063s3 low-level CIS driver");
MODULE_LICENSE("GPL");
