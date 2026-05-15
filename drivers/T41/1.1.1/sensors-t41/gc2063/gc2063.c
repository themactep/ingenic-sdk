/*
 * gc2063.c
 *
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          1920*1080       30          dvp                  linear
 *   1          1920*1080       30        mipi_2lane             linear
 *   2          1920*1080       15        mipi_2lane             linear
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

#define GC2063_CHIP_ID_H	(0x20)
#define GC2063_CHIP_ID_L	(0x53)
#define GC2063_REG_END		0xff
#define GC2063_REG_DELAY	0x00
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20220809a"

static int reset_gpio = -1;
static int pwdn_gpio = -1;

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	int index;
	unsigned int regb4;
	unsigned int regb3;
	unsigned int dpc;
	unsigned int blc;
	unsigned int gain;
};

struct again_lut gc2063_again_lut[] = {
	//inx, 0xb4 0xb3 0xb8 0xb9 gain
	{0x0, 0x0, 0x0, 0x1, 0x0, 0},                //  1.000000
	{0x1, 0x0, 0x10, 0x1, 0xc, 13726},           //  1.156250
	{0x2, 0x0, 0x20, 0x1, 0x1b, 31177},          //   1.390625
	{0x3, 0x0, 0x30, 0x1, 0x2c, 44067},          //   1.593750
	{0x4, 0x0, 0x40, 0x1, 0x3f, 64793},          //   1.984375
	{0x5, 0x0, 0x50, 0x2, 0x16, 78621},          //   2.296875
	{0x6, 0x0, 0x60, 0x2, 0x35, 96180},          //   2.765625
	{0x7, 0x0, 0x70, 0x3, 0x16, 109138},         //    3.171875
	{0x8, 0x0, 0x80, 0x4, 0x2, 132537},          //   4.062500
	{0x9, 0x0, 0x90, 0x4, 0x31, 146067},         //    4.687500
	{0xa, 0x0, 0xa0, 0x5, 0x32, 163567},         //    5.640625
	{0xb, 0x0, 0xb0, 0x6, 0x35, 176747},         //    6.484375
	{0xc, 0x0, 0xc0, 0x8, 0x4, 195118},          //   7.875000
	{0xd, 0x0, 0x5a, 0x9, 0x19, 208560},         //    9.078125
	{0xe, 0x0, 0x83, 0xb, 0xf, 229103},          //   11.281250
	{0xf, 0x0, 0x93, 0xd, 0x12, 242511},         //    13.000000
	{0x10, 0x0, 0x84, 0x10, 0x0, 262419},        //     16.046875
	{0x11, 0x0, 0x94, 0x12, 0x3a, 275710},       //      18.468750
	{0x12, 0x1, 0x2c, 0x1a, 0x2, 292252},        //     22.000000
	{0x13, 0x1, 0x3c, 0x1b, 0x20, 305571},       //      25.328125
	{0x14, 0x0, 0x8c, 0x20, 0xf, 324962},        //     31.093750
	{0x15, 0x0, 0x9c, 0x26, 0x7, 338280},        //     35.796875
	{0x16, 0x2, 0x64, 0x36, 0x21, 358923},       //      44.531250
	{0x17, 0x2, 0x74, 0x37, 0x3a, 372267},       //      51.281250
	{0x18, 0x0, 0xc6, 0x3d, 0x2, 392101},        //     63.250000
	{0x19, 0x0, 0xdc, 0x3f, 0x3f, 415415},       //      80.937500
	{0x1a, 0x2, 0x85, 0x3f, 0x3f, 421082},       //      85.937500
	{0x1b, 0x2, 0x95, 0x3f, 0x3f, 440360},       //      105.375000
	{0x1c, 0x0, 0xce, 0x3f, 0x3f, 444864},       //      110.515625
};

struct tx_isp_sensor_attribute gc2063_attr;

unsigned int gc2063_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = gc2063_again_lut;

	while(lut->gain <= gc2063_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].index;
			return lut[0].gain;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == gc2063_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int gc2063_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus gc2063_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 390,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.image_twidth = 1920,
	.image_theight = 1080,
	.mipi_sc.mipi_crop_start0x = 0,
	.mipi_sc.mipi_crop_start0y = 0,
	.mipi_sc.mipi_crop_start1x = 0,
	.mipi_sc.mipi_crop_start1y = 0,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_dvp_bus gc2063_dvp={
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking = {
		.vblanking = 0,
		.hblanking = 0,
	},
        .polar = {
                .hsync_polar = 0,
                .vsync_polar = 0,
                .pclk_polar = 0,
        },
};

struct tx_isp_sensor_attribute gc2063_attr={
	.name = "gc2063",
	.chip_id = 0x2053,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x37,
	.max_again = 261402,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = gc2063_alloc_again,
	.sensor_ctrl.alloc_dgain = gc2063_alloc_dgain,
	.one_line_expr_in_us = 28,
};

static struct regval_list gc2063_init_regs_1920_1080_30fps_dvp[] = {
	/****system****/
	{0xfe, 0x80},
	{0xfe, 0x80},
	{0xfe, 0x80},
	{0xfe, 0x00},
	{0xf2, 0x00},
	{0xf3, 0x0f},
	{0xf4, 0x36},
	{0xf5, 0xc0},
	{0xf6, 0x44},
	{0xf7, 0x01},
	{0xf8, 0x63},
	{0xf9, 0x40},
	{0xfc, 0x8e},
	/****CISCTL & ANALOG****/
	{0xfe, 0x00},
	{0x87, 0x18},
	{0xee, 0x30},
	{0xd0, 0xb7},
	{0x03, 0x04},
	{0x04, 0x60},
	{0x05, 0x04},//hts 0x44c = 1120
	{0x06, 0x4c},//
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
	{0x42, 0x50},/*0x546:25fps 0x450:30*/
	{0x9d, 0x10},
	{0xce, 0x7c},
	{0xd2, 0x41},
	{0xd3, 0xdc},
	{0xe6, 0x50},
	/*gain*/
	{0xb6,0xc0},
	{0xb0,0x70},
	{0xb1,0x01},
	{0xb2,0x00},
	{0xb3,0x00},
	{0xb4,0x00},
	{0xb8,0x01},
	{0xb9,0x00},
	/*blk*/
	{0x26,0x30},
	{0xfe,0x01},
	{0x40,0x23},
	{0x55,0x07},
	{0x60,0x40},
	{0xfe,0x04},
	{0x14,0x78},
	{0x15,0x78},
	{0x16,0x78},
	{0x17,0x78},
	/*window*/
	{0xfe,0x01},
	{0x92,0x00},
	{0x94,0x03},
	{0x95,0x04},
	{0x96,0x38},
	{0x97,0x07},
	{0x98,0x80},
	/*ISP*/
	{0xfe,0x01},
	{0x01,0x05},
	{0x02,0x89},
	{0x04,0x01},
	{0x07,0xa6},
	{0x08,0xa9},
	{0x09,0xa8},
	{0x0a,0xa7},
	{0x0b,0xff},
	{0x0c,0xff},
	{0x0f,0x00},
	{0x50,0x1c},
	{0x89,0x03},
	{0xfe,0x04},
	{0x28,0x86},
	{0x29,0x86},
	{0x2a,0x86},
	{0x2b,0x68},
	{0x2c,0x68},
	{0x2d,0x68},
	{0x2e,0x68},
	{0x2f,0x68},
	{0x30,0x4f},
	{0x31,0x68},
	{0x32,0x67},
	{0x33,0x66},
	{0x34,0x66},
	{0x35,0x66},
	{0x36,0x66},
	{0x37,0x66},
	{0x38,0x62},
	{0x39,0x62},
	{0x3a,0x62},
	{0x3b,0x62},
	{0x3c,0x62},
	{0x3d,0x62},
	{0x3e,0x62},
	{0x3f,0x62},
	/****DVP & MIPI****/
	{0xfe,0x01},
	{0x9a,0x06},
	{0xfe,0x00},
	{0x7b,0x2a},
	{0x23,0x2d},
	{0xfe,0x03},
	{0x01,0x20},
	{0x02,0x56},
	{0x03,0xb2},
	{0x12,0x80},
	{0x13,0x07},
	{0xfe,0x00},
	{0x3e,0x40},
	{GC2063_REG_END, 0x00},/* END MARKER */
};

static struct regval_list gc2063_init_regs_1920_1080_30fps_mipi[] = {
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
	{0x15, 0x12},
	{0x29, 0x05},/*mipi pre*/
	{0x2a, 0x0a},/*mipi zero*/
	{0xfe, 0x00},
	{0x3e, 0x91},
	{GC2063_REG_END, 0x00},/* END MARKER */
};

static struct regval_list gc2063_init_regs_1920_1080_15fps_mipi[] = {
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
	{0x15, 0x12},
        {0x29, 0x03},
	{0xfe, 0x00},
	{0x3e, 0x91},
	{GC2063_REG_END, 0x00},/* END MARKER */
};

static struct tx_isp_sensor_win_setting gc2063_win_sizes[] = {
	/* [0] 1920*1080 @ max 30fps dvp*/
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 30 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SRGGB10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= gc2063_init_regs_1920_1080_30fps_dvp,
	},
	/* [1] 1920*1080 @ max 30fps mipi*/
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 30 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SRGGB10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= gc2063_init_regs_1920_1080_30fps_mipi,
	},
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 15 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SRGGB10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= gc2063_init_regs_1920_1080_15fps_mipi,
	}
};
struct tx_isp_sensor_win_setting *wsize = &gc2063_win_sizes[0];

static struct regval_list gc2063_stream_on_mipi[] = {
	{GC2063_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2063_stream_off_mipi[] = {
	{GC2063_REG_END, 0x00},	/* END MARKER */
};

int gc2063_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int gc2063_write(struct tx_isp_subdev *sd, unsigned char reg,
		 unsigned char value)
{
	int ret;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 2,
		.buf	= buf,
	};
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 0
static int gc2063_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != GC2063_REG_END) {
		if (vals->reg_num == GC2063_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = gc2063_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int gc2063_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != GC2063_REG_END) {
		if (vals->reg_num == GC2063_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = gc2063_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int gc2063_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int gc2063_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = gc2063_read(sd, 0xf0, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != GC2063_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = gc2063_read(sd, 0xf1, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != GC2063_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int gc2063_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;
	struct again_lut *val_lut = gc2063_again_lut;

	/*set sensor reg page*/
	ret = gc2063_write(sd, 0xfe, 0x00);

	/*set integration time*/
	ret += gc2063_write(sd, 0x04, it & 0xff);
	ret += gc2063_write(sd, 0x03, (it & 0x3f00)>>8);

	/*set analog gain*/
	ret += gc2063_write(sd, 0xb4, val_lut[again].regb4);
	ret += gc2063_write(sd, 0xb3, val_lut[again].regb3);
	ret += gc2063_write(sd, 0xb8, val_lut[again].dpc);
	ret += gc2063_write(sd, 0xb9, val_lut[again].blc);
	if (ret < 0) {
		ISP_ERROR("gc2063_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}

#if 0
static int gc2063_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = gc2063_write(sd, 0x04, value&0xff);
	ret += gc2063_write(sd, 0x03, (value&0x3f00)>>8);
	if (ret < 0) {
		ISP_ERROR("gc2063_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int gc2063_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	struct again_lut *val_lut = gc2063_again_lut;

	ret = gc2063_write(sd, 0xfe, 0x00);
	ret += gc2063_write(sd, 0xb4, val_lut[value].regb4);
	ret += gc2063_write(sd, 0xb3, val_lut[value].regb3);
	ret += gc2063_write(sd, 0xb2, val_lut[value].regb2);
	ret += gc2063_write(sd, 0xb8, val_lut[value].dpc);
	ret += gc2063_write(sd, 0xb9, val_lut[value].blc);
	if (ret < 0) {
		ISP_ERROR("gc2063_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}
#endif

static int gc2063_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc2063_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_set_attr(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

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
	return 0;
}

static int gc2063_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!init->enable)
		return ISP_SUCCESS;

	sensor_set_attr(sd, wsize);
	sensor->video.state = TX_ISP_MODULE_INIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int gc2063_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if(sensor->video.state == TX_ISP_MODULE_INIT){
			ret = gc2063_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if(sensor->video.state == TX_ISP_MODULE_RUNNING){

			ret = gc2063_write_array(sd, gc2063_stream_on_mipi);
			ISP_WARNING("gc2063 stream on\n");
		}
	}
	else {
		ret = gc2063_write_array(sd, gc2063_stream_off_mipi);
		ISP_WARNING("gc2063 stream off\n");
	}

	return ret;
}

static int gc2063_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int max_fps;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch(sensor->info.default_boot){
	case 0:
		sclk = 74250000;
		max_fps = TX_SENSOR_MAX_FPS_30;
		break;
	case 1:
		sclk = 78000000;
		max_fps = TX_SENSOR_MAX_FPS_30;
		break;
	case 2:
		sclk = 39000000;
		max_fps = TX_SENSOR_MAX_FPS_15;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps<< 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}

	ret = gc2063_write(sd, 0xfe, 0x0);
	ret += gc2063_read(sd, 0x05, &val);
	hts = val;
	ret += gc2063_read(sd, 0x06, &val);
	if(ret < 0)
		return -1;
	hts = ((hts << 8) + val) << 1;

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = gc2063_write(sd, 0x41, (unsigned char)((vts & 0x3f00) >> 8));
	ret += gc2063_write(sd, 0x42, (unsigned char)(vts & 0xff));
	if(ret < 0)
		return -1;

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 8;
	sensor->video.attr->integration_time_limit = vts - 8;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 8;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int gc2063_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int gc2063_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = -1;
	unsigned char val = 0x0;

	ret = gc2063_write(sd, 0xfe, 0x0);
	ret += gc2063_read(sd, 0x17, &val);

	if(enable & 0x2)
		val |= 0x02;
	else
		val &= 0xfd;

	ret += gc2063_write(sd, 0x17, val);

	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int sensor_gpio_func = DVP_PA_LOW_10BIT;
	unsigned long rate;
	int ret;

	switch(info->default_boot){
	case 0:
		wsize = &gc2063_win_sizes[0];
		memcpy(&(gc2063_attr.dvp), &gc2063_dvp, sizeof(gc2063_dvp));
		gc2063_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		gc2063_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		gc2063_attr.max_integration_time_native = 0X546 -8;
		gc2063_attr.integration_time_limit = 0X546 -8;
		gc2063_attr.total_width = 0X44C * 2;
		gc2063_attr.total_height = 0X546;
		gc2063_attr.max_integration_time = 0X546 -8;
		gc2063_attr.again =0;
		gc2063_attr.integration_time = 0xb201;
		gc2063_attr.one_line_expr_in_us = 29;
		ret = set_sensor_gpio_function(sensor_gpio_func);
		if (ret < 0)
			goto err_set_sensor_gpio;
		gc2063_attr.dvp.gpio = sensor_gpio_func;
		break;
	case 1:
		wsize = &gc2063_win_sizes[1];
		memcpy(&(gc2063_attr.mipi), &gc2063_mipi, sizeof(gc2063_mipi));
		gc2063_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		gc2063_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
		gc2063_attr.max_integration_time_native = 0x49d -8;
		gc2063_attr.integration_time_limit = 0x49d -8;
		gc2063_attr.total_width = 0x44c *2;
		gc2063_attr.total_height = 0x49d;
		gc2063_attr.max_integration_time = 0x49d -8;
		gc2063_attr.again =0;
		gc2063_attr.integration_time = 0x8e01;
		gc2063_attr.one_line_expr_in_us = 28;
		break;
	case 2:
		wsize = &gc2063_win_sizes[2];
		memcpy(&(gc2063_attr.mipi), &gc2063_mipi, sizeof(gc2063_mipi));
		gc2063_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		gc2063_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
		gc2063_attr.mipi.clk = 312;
		gc2063_attr.max_integration_time_native = 0x49d -8;
		gc2063_attr.integration_time_limit = 0x49d -8;
		gc2063_attr.total_width = 0x44c *2;
		gc2063_attr.total_height = 0x49d;
		gc2063_attr.max_integration_time = 0x49d -8;
		gc2063_attr.again =0;
		gc2063_attr.integration_time = 0x8e01;
		gc2063_attr.one_line_expr_in_us = 57;
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
	}

	switch(info->video_interface){
	case TISP_SENSOR_VI_MIPI_CSI0:
		gc2063_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc2063_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_DVP:
		gc2063_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this Interface Source!!!\n");
	}

	switch(info->mclk){
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

	rate = private_clk_get_rate(sensor->mclk);
	switch(info->default_boot){
	case 0:
	case 1:
	case 2:
                if (((rate / 1000) % 24000) != 0) {
                        ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
                        sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
                        if (IS_ERR(sclka)) {
                                pr_err("get sclka failed\n");
                        } else {
                                rate = private_clk_get_rate(sclka);
                                if (((rate / 1000) % 24000) != 0) {
                                        private_clk_set_rate(sclka, 1200000000);
                                }
                        }
                }
                private_clk_set_rate(sensor->mclk, 24000000);
                private_clk_prepare_enable(sensor->mclk);
                break;
	}

	ISP_WARNING("\n====>[default_boot=%d] [resolution=%dx%d] [video_interface=%d] [MCLK=%d] \n", info->default_boot, wsize->width, wsize->height, info->video_interface, info->mclk);
	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;
	return 0;
err_set_sensor_gpio:
	return -1;
}

static int gc2063_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"gc2063_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"gc2063_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = gc2063_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc2063 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("gc2063 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "gc2063", sizeof("gc2063"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int gc2063_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = gc2063_set_expo(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		//if(arg)
		//	ret = gc2063_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		//if(arg)
		//	ret = gc2063_set_analog_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = gc2063_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = gc2063_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = gc2063_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = gc2063_write_array(sd, gc2063_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = gc2063_write_array(sd, gc2063_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = gc2063_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = gc2063_set_vflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int gc2063_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = gc2063_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int gc2063_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc2063_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops gc2063_core_ops = {
	.g_chip_ident = gc2063_g_chip_ident,
	.reset = gc2063_reset,
	.init = gc2063_init,
	.g_register = gc2063_g_register,
	.s_register = gc2063_s_register,
};

static struct tx_isp_subdev_video_ops gc2063_video_ops = {
	.s_stream = gc2063_s_stream,
};

static struct tx_isp_subdev_sensor_ops	gc2063_sensor_ops = {
	.ioctl	= gc2063_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops gc2063_ops = {
	.core = &gc2063_core_ops,
	.video = &gc2063_video_ops,
	.sensor = &gc2063_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "gc2063",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc2063_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->dev = &client->dev;
	gc2063_attr.expo_fs = 1;
	sensor->video.shvflip = 1;
	sensor->video.attr = &gc2063_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc2063_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->gc2063\n");

	return 0;
}

static int gc2063_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	private_devm_clk_put(&client->dev, sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id gc2063_id[] = {
	{ "gc2063", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc2063_id);

static struct i2c_driver gc2063_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "gc2063",
	},
	.probe		= gc2063_probe,
	.remove		= gc2063_remove,
	.id_table	= gc2063_id,
};

static __init int init_gc2063(void)
{
	return private_i2c_add_driver(&gc2063_driver);
}

static __exit void exit_gc2063(void)
{
	private_i2c_del_driver(&gc2063_driver);
}

module_init(init_gc2063);
module_exit(exit_gc2063);

MODULE_DESCRIPTION("A low-level driver for Smartsens gc2063 sensors");
MODULE_LICENSE("GPL");
