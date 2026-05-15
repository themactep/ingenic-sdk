/*
 * gc2093.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/* 1920*1080  carrier-server  --st=gc2093  data_interface=1  i2c=0x37  */

#define __WDR__
//#define FAST_AE
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

#define GC2093_CHIP_ID_H	(0x20)
#define GC2093_CHIP_ID_L	(0x93)
#define GC2093_REG_END	0xffff
#define GC2093_REG_DELAY   0x0000
#define GC2093_SUPPORT_30FPS_SCLK (58725000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20220408a"
#ifdef FAST_AE
#define GC2093_FAST_AE 0xfffe
unsigned long long time_reg = 0;
#endif

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_30;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

static int sensor_gpio_func = DVP_PA_12BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
module_param(data_type, int, S_IRUGO);
MODULE_PARM_DESC(data_type, "Sensor Date Type");

static int wdr_bufsize = 1996800;
module_param(wdr_bufsize, int, S_IRUGO);
MODULE_PARM_DESC(wdr_bufsize, "Wdr Buf Size");

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	unsigned int index;
	unsigned char regb0;
	unsigned char regb1;
	unsigned int  regb2;
	unsigned char regb3;
	unsigned char regb4;
	unsigned char regb5;
	unsigned char regb6;
	unsigned char regb7;

	unsigned int gain;
};

struct again_lut gc2093_again_lut[] = {
	{0x0,0x00,0x00,0x01,0x00,0x68,0x07,0x00,0xf8,0},
	{0x1,0x00,0x10,0x01,0x0c,0x68,0x07,0x00,0xf8,14997},
	{0x2,0x00,0x20,0x01,0x1b,0x6c,0x07,0x00,0xf8,32233},
	{0x3,0x00,0x30,0xf01,0x2c,0x6c,0x07,0x00,0xf8,46808},
	{0x4,0x00,0x40,0x01,0x3f,0x7c,0x07,0x00,0xf8,60995},
	{0x5,0x00,0x50,0x02,0x16,0x7c,0x07,0x00,0xf8,75348},
	{0x6,0x00,0x60,0x02,0x35,0x7c,0x08,0x00,0xf9,90681},
	{0x7,0x00,0x70,0x03,0x16,0x7c,0x0b,0x00,0xfc,104361},
	{0x8,0x00,0x80,0x04,0x02,0x7c,0x0d,0x00,0xfe,118021},
	{0x9,0x00,0x90,0x04,0x31,0x7c,0x0f,0x08,0x00,131438},
	{0xA,0x00,0xa0,0x05,0x32,0x7c,0x11,0x08,0x02,145749},
	{0xB,0x00,0xb0,0x06,0x35,0x7c,0x14,0x08,0x05,159553},
	{0xC,0x00,0xc0,0x08,0x04,0x7c,0x16,0x08,0x07,172553},
	{0xD,0x00,0x5a,0x09,0x19,0x7c,0x18,0x08,0x09,183132},
	{0xE,0x00,0x83,0x0b,0x0f,0x7c,0x1b,0x08,0x0c,198614},
	{0xF,0x00,0x93,0x0d,0x12,0x7c,0x1e,0x08,0x0f,212697},
	{0x10,0x00,0x84,0x10,0x00,0x7c,0x22,0x08,0x13,226175},
	{0x11,0x00,0x94,0x12,0x3a,0x7c,0x26,0x08,0x17,240788},
	{0x12,0x00,0x5d,0x1a,0x02,0x7c,0x30,0x08,0x21,271536},
	{0x13,0x00,0x9b,0x1b,0x20,0x7c,0x30,0x08,0x21,272451},
	{0x14,0x00,0x8c,0x20,0x0f,0x7c,0x35,0x08,0x26,287144},
	{0x15,0x00,0x9c,0x26,0x07,0x7c,0x3b,0x08,0x2c,302425},
	{0x16,0x00,0xB6,0x36,0x21,0x7c,0x43,0x08,0x34,334228},
	{0x17,0x00,0xad,0x37,0x3a,0x7c,0x43,0x08,0x34,351574},
	{0x18,0x00,0xbd,0x3d,0x02,0x7c,0x43,0x08,0x34,367506},
};

struct tx_isp_sensor_attribute gc2093_attr;

unsigned int gc2093_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;
#if 0
	if(data_type == TX_SENSOR_DATA_TYPE_WDR_DOL){
		if (expo % 2 == 0)
			expo = expo - 1;
		if(expo < gc2093_attr.min_integration_time)
			expo = 3;
	}
	isp_it = expo << shift;
#endif
	*sensor_it = expo;

	return isp_it;
}

unsigned int gc2093_alloc_integration_time_short(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;
#if 0
	if (expo % 2 == 0)
		expo = expo - 1;
	if(expo < gc2093_attr.min_integration_time_short)
		expo = 3;
	isp_it = expo << shift;
	expo = (expo - 1) / 2;
	if(expo < 0)
		expo = 0;
#endif
	*sensor_it = expo;

	return isp_it;
}

unsigned int gc2093_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = gc2093_again_lut;
	while(lut->gain <= gc2093_attr.max_again){
		if(isp_gain == 0) {
			*sensor_again = 0;
			return lut[0].gain;
		} else if(isp_gain < lut->gain){
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == gc2093_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
				return lut->gain;
			}
		}

		lut++;
	}

	return 0;
}

unsigned int gc2093_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = gc2093_again_lut;
	while(lut->gain <= gc2093_attr.max_again_short) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->gain;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == gc2093_attr.max_again_short) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int gc2093_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus gc2093_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 392,
	.lans = 2,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
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
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = 0,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus gc2093_mipi_wdr = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 432,
	.lans = 2,
	.settle_time_apative_en = 1,
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
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = 0,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_sensor_attribute gc2093_attr={
	.name = "gc2093",
	.chip_id = 0x2093,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x37,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 392,
		.lans = 2,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
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
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.mipi_sc.data_type_en = 0,
		.mipi_sc.data_type_value = 0,
		.mipi_sc.del_start = 0,
		.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
		.mipi_sc.sensor_fid_mode = 0,
		.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 367506,
	.max_again_short = 367506,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_short = 4,
	.max_integration_time_short = 100 - 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 1350 - 4,
	.integration_time_limit = 1350 - 4,
	.total_width = 2900,
	.total_height = 1350,
	.max_integration_time = 1350 - 4,
	.integration_time_apply_delay = 4,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.one_line_expr_in_us = 30,
	.sensor_ctrl.alloc_again = gc2093_alloc_again,
	.sensor_ctrl.alloc_again_short = gc2093_alloc_again_short,
	.sensor_ctrl.alloc_dgain = gc2093_alloc_dgain,
	.sensor_ctrl.alloc_integration_time = gc2093_alloc_integration_time,
	.sensor_ctrl.alloc_integration_time_short = gc2093_alloc_integration_time_short,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};
#if 0
static struct regval_list gc2093_init_regs_1920_1080_30fps_mipi[]={
	{0x03fe,0xf0},
	{0x03fe,0xf0},
	{0x03fe,0xf0},
	{0x03fe,0x00},
	{0x03f2,0x00},
	{0x03f3,0x00},
	{0x03f4,0x36},
	{0x03f5,0xc0},
	{0x03f6,0x0A},
	{0x03f7,0x01},
	{0x03f8,0x1d},
	{0x03f9,0x10},
	{0x03fc,0x8e},
	/****CISCTL & ANALOG****/
	{0x0087,0x18},
	{0x00ee,0x30},
	{0x00d0,0xbf},
	{0x01a0,0x00},
	{0x01a4,0x40},
	{0x01a5,0x40},
	{0x01a6,0x40},
	{0x01af,0x09},
	{0x0001,0x00},
	{0x0002,0x02},
	{0x0003,0x04},
	{0x0004,0x00},
	{0x0005,0x02},
	{0x0006,0xd5},
	{0x0007,0x00},
//	{0x0008,0x11},
	{0x0008,0xf2},//jz vb=242
	{0x0009,0x00},
	{0x000a,0x02},
	{0x000b,0x00},
	{0x000c,0x04},
	{0x000d,0x04},
	{0x000e,0x40},
	{0x000f,0x07},
	{0x0010,0x8c},
	{0x0013,0x15},
//	{0x0018,0x00},
	{0x0019,0x0c},
	{0x0041,0x05},
	{0x0042,0x46},
	{0x0053,0x60},
	{0x008d,0x92},
	{0x0090,0x00},
	{0x00c7,0xe1},
	{0x001b,0x73},
	{0x0028,0x0d},
	{0x0029,0x24},
	{0x002b,0x04},
	{0x002e,0x23},
	{0x0037,0x03},
	{0x0043,0x04},
	{0x0044,0x38},
	{0x004a,0x01},
	{0x004b,0x28},
	{0x0055,0x38},
	{0x006b,0x44},
	{0x0077,0x00},
	{0x0078,0x20},
	{0x007c,0xa1},
	{0x00ce,0x7c},
	{0x00d3,0xd4},
	{0x00e6,0x50},
	/*gain*/
	{0x00b6,0xc0},
	{0x00b0,0x60},
	{0x00b3,0x00},
	{0x00b8,0x01},
	{0x00b9,0x00},
	{0x00b1,0x01},
	{0x00b2,0x00},
	/*isp*/
	{0x0102,0x89},
	{0x0104,0x01},
	{0x0158,0x00},
	/*dark sun*/
	{0x0123,0x08},
	{0x0123,0x00},
	{0x0120,0x00},
	{0x0121,0x00},
	{0x0122,0x0f},
	{0x0124,0x03},
	{0x0125,0xff},
	{0x0126,0x3c},
	{0x001a,0x8c},
	{0x00c6,0xe0},
	/*blk*/
	{0x0026,0x30},
	{0x0142,0x00},
	{0x0149,0x1e},
	{0x014a,0x07},
	{0x014b,0x80},
	{0x0155,0x07},
	{0x0414,0x7e},
	{0x0415,0x7e},
	{0x0416,0x7e},
	{0x0417,0x7e},
	{0x04e0,0x18},
	/*window*/
	{0x0192,0x02},
	{0x0194,0x03},
	{0x0195,0x04},
	{0x0196,0x38},
	{0x0197,0x07},
	{0x0198,0x80},
	{0x01b0,0x01},
	{0x01b1,0x00},
	{0x01b2,0x20},
	{0x01b3,0x00},
	{0x01b4,0xf0},
	{0x01b5,0x80},
	{0x01b6,0x05},
	{0x01b8,0x01},
	{0x01b9,0xe0},
	{0x01ba,0x01},
	{0x01bb,0x80},
	/****DVP & MIPI****/
	{0x019a ,0x06},
	{0x007b ,0x2a},
	{0x0023 ,0x2d},
	{0x0201 ,0x27},
	{0x0202 ,0x56},
	{0x0203 ,0xb6},
	{0x0212 ,0x80},
	{0x0213 ,0x07},
	{0x0215 ,0x12},
	{0x003e ,0x91},
	{GC2093_REG_END, 0x00}, /* END MARKER */
};
#endif
static struct regval_list gc2093_init_regs_1920_1080_30fps_mipi_lin[] = {
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03f2, 0x00},
	{0x03f3, 0x00},
	{0x03f4, 0x36},
	{0x03f5, 0xc0},
	{0x03f6, 0x0A},
	{0x03f7, 0x01},
	{0x03f8, 0x24},
	{0x03f9, 0x10},
	{0x03fc, 0x8e},
	/****CISCTL & ANALOG****/
	{0x0087, 0x18},
	{0x00ee, 0x30},
	{0x00d0, 0xbf},
	{0x01a0, 0x00},
	{0x01a4, 0x40},
	{0x01a5, 0x40},
	{0x01a6, 0x40},
	{0x01af, 0x09},
	{0x0001, 0x00},
	{0x0002, 0x02},
	{0x0003, 0x04},
	{0x0004, 0x00},
	{0x0005, 0x02},
	{0x0006, 0xd5},
	{0x0007, 0x00},
	{0x0008, 0x8e},//vb=142
	{0x0009, 0x00},
	{0x000a, 0x02},
	{0x000b, 0x00},
	{0x000c, 0x04},
	{0x000d, 0x04},
	{0x000e, 0x40},
	{0x000f, 0x07},
	{0x0010, 0x8c},
	{0x0013, 0x15},
	{0x0019, 0x0c},
//	{0x0041, 0x04},//1250
//	{0x0042, 0xe2},//1250
	{0x0041, 0x05},//1350 jz
	{0x0042, 0x46},//1350 jz
	{0x0053, 0x60},
	{0x008d, 0x92},
	{0x0090, 0x00},
	{0x00c7, 0xe1},
	{0x001b, 0x73},
	{0x0028, 0x0d},
	{0x0029, 0x24},
	{0x002b, 0x04},
	{0x002e, 0x23},
	{0x0037, 0x03},
	{0x0043, 0x04},
	{0x0044, 0x38},
	{0x004a, 0x01},
	{0x004b, 0x28},
	{0x0055, 0x38},
	{0x006b, 0x44},
	{0x0077, 0x00},
	{0x0078, 0x20},
	{0x007c, 0xa1},
	{0x00ce, 0x7c},
	{0x00d3, 0xd4},
	{0x00e6, 0x50},
	/*gain*/
	{0x00b6, 0xc0},
	{0x00b0, 0x60},
	{0x00b3, 0x00},
	{0x00b8, 0x01},
	{0x00b9, 0x00},
	{0x00b1, 0x01},
	{0x00b2, 0x00},
	/*isp*/
	{0x0102, 0x89},
	{0x0104, 0x01},
	{0x010e, 0x00},//jz wdr -> lin
	{0x010f, 0x00},
	{0x0158, 0x00},
	/*dark sun*/
	{0x0123, 0x08},
	{0x0123, 0x00},
	{0x0120, 0x00},
	{0x0121, 0x00},
	{0x0122, 0x0f},
	{0x0124, 0x03},
	{0x0125, 0xff},
	{0x0126, 0x3c},
	{0x001a, 0x8c},
	{0x00c6, 0xe0},
	/*blk*/
	{0x0026, 0x30},
	{0x0142, 0x00},
	{0x0149, 0x1e},
	{0x014a, 0x07},
	{0x014b, 0x80},
	{0x0155, 0x00},
	{0x0414, 0x78},
	{0x0415, 0x78},
	{0x0416, 0x78},
	{0x0417, 0x78},
	{0x04e0, 0x18},
	/*window*/
	{0x0192, 0x02},
	{0x0194, 0x03},
	{0x0195, 0x04},
	{0x0196, 0x38},
	{0x0197, 0x07},
	{0x0198, 0x80},
	{0x01b0, 0x01},
	{0x01b1, 0x00},
	{0x01b2, 0x20},
	{0x01b3, 0x00},
	{0x01b4, 0xf0},
	{0x01b5, 0x80},
	{0x01b6, 0x05},
	{0x01b8, 0x01},
	{0x01b9, 0xe0},
	{0x01ba, 0x01},
	{0x01bb, 0x80},
	/****DVP & MIPI****/
	{0x019a, 0x06},
	{0x007b, 0x2a},
	{0x0023, 0x2d},
	{0x0201, 0x27},
	{0x0202, 0x56},
//	{0x0203, 0xb6},
	{0x0203, 0x8e},
	{0x0212, 0x80},
	{0x0213, 0x07},
	{0x0215, 0x12},
	/****HDR EN****/
	{0x0027, 0x70},//jz
	{0x0215, 0x12},//jz
	{0x024d, 0x00},//jz
	{0x003e, 0x91},
	{GC2093_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list gc2093_init_regs_1920_1080_15fps_mipi_wdr[] = {
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03f2, 0x00},
	{0x03f3, 0x00},
	{0x03f4, 0x36},
	{0x03f5, 0xc0},
	{0x03f6, 0x0A},
	{0x03f7, 0x01},
	{0x03f8, 0x24},
	{0x03f9, 0x10},
	{0x03fc, 0x8e},
	/****CISCTL & ANALOG****/
	{0x0087, 0x18},
	{0x00ee, 0x30},
	{0x00d0, 0xbf},
	{0x01a0, 0x00},
	{0x01a4, 0x40},
	{0x01a5, 0x40},
	{0x01a6, 0x40},
	{0x01af, 0x09},
	{0x0001, 0x00},
	{0x0002, 0x02},
	{0x0003, 0x04},
	{0x0004, 0x00},
	{0x0005, 0x02},
	{0x0006, 0xd5},
	{0x0007, 0x00},
	{0x0008, 0x8e},//vb=142
	{0x0009, 0x00},
	{0x000a, 0x02},
	{0x000b, 0x00},
	{0x000c, 0x04},
	{0x000d, 0x04},
	{0x000e, 0x40},
	{0x000f, 0x07},
	{0x0010, 0x8c},
	{0x0013, 0x15},
	{0x0019, 0x0c},
//	{0x0041, 0x04},//1250
//	{0x0042, 0xe2},//1250
	{0x0041, 0x05},//1350 jz
	{0x0042, 0x46},//1350 jz
	{0x0053, 0x60},
	{0x008d, 0x92},
	{0x0090, 0x00},
	{0x00c7, 0xe1},
	{0x001b, 0x73},
	{0x0028, 0x0d},
	{0x0029, 0x24},
	{0x002b, 0x04},
	{0x002e, 0x23},
	{0x0037, 0x03},
	{0x0043, 0x04},
	{0x0044, 0x38},
	{0x004a, 0x01},
	{0x004b, 0x28},
	{0x0055, 0x38},
	{0x006b, 0x44},
	{0x0077, 0x00},
	{0x0078, 0x20},
	{0x007c, 0xa1},
	{0x00ce, 0x7c},
	{0x00d3, 0xd4},
	{0x00e6, 0x50},
	/*gain*/
	{0x00b6, 0xc0},
	{0x00b0, 0x60},
	{0x00b3, 0x00},
	{0x00b8, 0x01},
	{0x00b9, 0x00},
	{0x00b1, 0x01},
	{0x00b2, 0x00},
	/*isp*/
	{0x0102, 0x89},
	{0x0104, 0x01},
	{0x010e, 0x01},
	{0x010f, 0x00},
	{0x0158, 0x00},
	/*dark sun*/
	{0x0123, 0x08},
	{0x0123, 0x00},
	{0x0120, 0x00},
	{0x0121, 0x00},
	{0x0122, 0x0f},
	{0x0124, 0x03},
	{0x0125, 0xff},
	{0x0126, 0x3c},
	{0x001a, 0x8c},
	{0x00c6, 0xe0},
	/*blk*/
	{0x0026, 0x30},
	{0x0142, 0x00},
	{0x0149, 0x1e},
	{0x014a, 0x07},
	{0x014b, 0x80},
	{0x0155, 0x00},
	{0x0414, 0x78},
	{0x0415, 0x78},
	{0x0416, 0x78},
	{0x0417, 0x78},
	{0x04e0, 0x18},
	/*window*/
	{0x0192, 0x02},
	{0x0194, 0x03},
	{0x0195, 0x04},
	{0x0196, 0x38},
	{0x0197, 0x07},
	{0x0198, 0x80},
	{0x01b0, 0x01},
	{0x01b1, 0x00},
	{0x01b2, 0x20},
	{0x01b3, 0x00},
	{0x01b4, 0xf0},
	{0x01b5, 0x80},
	{0x01b6, 0x05},
	{0x01b8, 0x01},
	{0x01b9, 0xe0},
	{0x01ba, 0x01},
	{0x01bb, 0x80},
	/****DVP & MIPI****/
	{0x019a, 0x06},
	{0x007b, 0x2a},
	{0x0023, 0x2d},
	{0x0201, 0x27},
	{0x0202, 0x56},
//	{0x0203, 0xb6},
	{0x0203, 0x8e},
	{0x0212, 0x80},
	{0x0213, 0x07},
	{0x0215, 0x12},
	/****HDR EN****/
	{0x0027, 0x71},
	{0x0215, 0x92},
	{0x024d, 0x01},
	{0x003e, 0x91},
	{GC2093_REG_END, 0x00}, /* END MARKER */
};
#ifdef FAST_AE
static struct regval_list gc2093_init_regs_1920_1080_30fps_mipi_ae[] = {
/****system****/
	{0x03fe,0xf0},
	{0x03fe,0xf0},
	{0x03fe,0xf0},
	{0x03fe,0x10},
	{0x03f2,0x00},
	{0x03f3,0x00},
	{0x03f4,0x36},
	{0x03f5,0xc0},
	{0x03f6,0x0c},//0A
	{0x03f7,0x01},
	{0x03f8,0x63},//2C
	{0x03f9,0x80},
	{0x03fc,0x8e},

	{0x01e1,0x08},//qcifnum
	{0x0183,0x01},
	{0x0187,0x51},//50
	{0x01a0,0x00},//awb_en
	{0x01af,0x09},//aec_en


	/****CISCTL&ANALOG****/
	{0x0087,0x18},
	{0x00ee,0x30},
	{0x00d0,0xbf},
	{0x00d3,0xd4},
	{0x007c,0xa3},

	{0x0005,0x08},//04
	{0x0006,0x98},//4c
	{0x000a,0x02},
	{0x000c,0x04},
	{0x000e,0x40},
	{0x0010,0x8c},
	{0x0019,0x0c},
	{0x0053,0x60},
	{0x008d,0x92},
	{0x0090,0x01},
	{0x009d,0x11},
	{0x00c7,0xe1},

	{0x001b,0x73},
	{0x0028,0x0d},
	{0x002b,0x04},
	{0x002e,0x23},
	{0x0037,0x03},
	{0x0044,0x20},
	{0x0046,0x0b},
	{0x004a,0x01},
	{0x004b,0x20},
	{0x0055,0x30},

	{0x006b,0x44},
	{0x0077,0x00},
	{0x0078,0x20},
	{0x00ce,0x7c},
	{0x00e6,0x50},

	/*gain*/
	{0x00b6,0xc0},
	{0x00b0,0x60},

	/*isp*/
	{0x0102,0x89},
	{0x0158,0x00},

	/*darksun*/
	//{0x0123,0x08},
	//{0x0123,0x00},
	//{0x0120,0x00},
	//{0x0121,0x04},
	//{0x0122,0xe0},
	//{0x0124,0x03},
	//{0x0125,0xff},
	//{0x0126,0x3c},
	//{0x001a,0x8c},
	//{0x00c6,0xe0},

	/*blk*/
	{0x0026,0x20},
	{0x0149,0x1e},
	{0x0140,0x20},
	{0x0155,0x07},//00
	{0x0160,0x40},
	{0x0414,0x7e},//00
	{0x0415,0x7e},//00
	{0x0416,0x7e},//00
	{0x0417,0x7e},//00
	{0x04e0,0x18},

	/*window*/
	{0x0192,0x02},
	{0x0194,0x03},
	{0x01b0,0x00},//aec max exp high
	{0x01b1,0x90}, //aec max exp low
	{0x01b2,0x02}, //aec min exp
	{0x01b3,0x03}, //aec_gain_max[9:8]
	{0x01b4,0xff}, //aec_gain_max[7:0]
	{0x01b5,0x20}, //aec_y_target
	{0x01b6,0x05}, //aec_margin
	{0x01b8,0x01}, //aec_win_x0
	{0x01b9,0xe0}, //aec_win_x1
	{0x01ba,0x01}, //aec_win_y0
	{0x01bb,0x80}, //aec_win_y1

	/****DVP & MIPI****/
	{0x019a,0x06},
	{0x007b,0x2a},
	{0x0023,0x2d},
	{0x0201,0x27},
	{0x0202,0x56},
	{0x0203,0xb6},
	{0x0212,0x80},
	{0x0213,0x07},
	{0x0215,0x10},
	{0x003e,0x91},

	{0x01af,0x0b},//aec,0xen
	{0x01a0,0xc0},//awb,0xen + awb_write_valid
	{0x01e0,0x03},//speed mode
	{0x03fe,0x00},//cisctrl rst

	{GC2093_FAST_AE, 0x00},
	/*isp 控制操作*/
	//{0x0140,0x23},
	//{0x04e0,0x18},
	//{0x01af,0x09}, //aec off
	//{0x01a0,0x00}, //awb,0xoff
	//{0x01a4,0x40},
	//{0x01a5,0x40},
	//{0x01a6,0x40},
	//0x00a9    y_avg读亮度信息。
	//isp根据    y_avg信息，下aec及awb相关参数

	//{0x01e0, 0x07}, flag 置位，sensor出图。
	{GC2093_REG_END, 0x00}, /* END MARKER */
};
#endif
/*
 * the order of the gc2093_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting gc2093_win_sizes[] = {
	/* 1920*1080 */
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= gc2093_init_regs_1920_1080_30fps_mipi_lin,
	},
#ifdef __WDR__
	/* wdr */
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 15 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= gc2093_init_regs_1920_1080_15fps_mipi_wdr,
	},
#endif
#ifdef FAST_AE
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= gc2093_init_regs_1920_1080_30fps_mipi_ae,
	},
#endif
};


struct tx_isp_sensor_win_setting *wsize = &gc2093_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list gc2093_stream_on[] = {
	{GC2093_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2093_stream_off[] = {
	{GC2093_REG_END, 0x00},	/* END MARKER */
};

int gc2093_read(struct tx_isp_subdev *sd,  uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg>>8)&0xff, reg&0xff};
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
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int gc2093_write(struct tx_isp_subdev *sd, uint16_t reg,
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

static int gc2093_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != GC2093_REG_END) {
		if (vals->reg_num == GC2093_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc2093_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}
	return 0;
}

static int gc2093_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != GC2093_REG_END) {
		if (vals->reg_num == GC2093_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc2093_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int gc2093_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int gc2093_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = gc2093_read(sd, 0x03f0, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != GC2093_CHIP_ID_H)
		return -ENODEV;
	ret = gc2093_read(sd, 0x03f1, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != GC2093_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int gc2093_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = gc2093_write(sd, 0x04, (unsigned char)(value & 0xff));
	ret += gc2093_write(sd, 0x03, (unsigned char)(value>>8)&0x3f);
	if (ret < 0) {
		ISP_ERROR("gc2093_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int gc2093_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	struct again_lut *val_lut = gc2093_again_lut;


	ret += gc2093_write(sd,0xb4,val_lut[value].regb0);
	ret += gc2093_write(sd,0xb3,val_lut[value].regb1);
	ret += gc2093_write(sd,0xb8,val_lut[value].regb2);
	ret += gc2093_write(sd,0xb9,val_lut[value].regb3);
	ret += gc2093_write(sd,0xce,val_lut[value].regb4);
	ret += gc2093_write(sd,0xc2,val_lut[value].regb5);
	ret += gc2093_write(sd,0xcf,val_lut[value].regb6);
	ret += gc2093_write(sd,0xd9,val_lut[value].regb7);

	if (ret < 0) {
		ISP_ERROR("gc2093_write error  %d" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int gc2093_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc2093_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

#ifdef FAST_AE
int ration_num = 0;
static int gc2093_get_analog_gain_ratio(struct tx_isp_subdev *sd, unsigned long long value)
{
	int loop = 0,ratio_i = 0;
	unsigned long long loop_d;
	unsigned long long ratio_dgain[]={1000,1182,1400,1659,2000,2370,2800,3318,4000,4740,5600,6636,8000,9480,11200,13272,16000,18960,22400,26544,32000,37920,44800,53088,64000};
	loop_d = value * 1000 / 64;
	for(loop = 0 ;loop < 24 ;loop++)
	{
		if(loop_d == ratio_dgain[loop])
		{
			ratio_i = loop;
			break;
		}
		else if((loop_d > ratio_dgain[loop]) && (loop_d < ratio_dgain[loop + 1]))
		{
			ratio_i = (loop_d - ratio_dgain[loop]) > (ratio_dgain[loop + 1] - loop_d) ? loop + 1 : loop;
			break;
		}
		else
			ratio_i = 24;
	}
	ration_num = ratio_i;

	return 0;
}

static int gc2093_set_analog_gain_ae(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret = 0;
	unsigned char dgain = 0;
	struct again_lut *val_lut = gc2093_again_lut;
	unsigned long long again;
	ret = gc2093_read(sd, 0x00b1, &dgain);
	again = dgain;
	ret += gc2093_read(sd, 0x00b2, &dgain);
	again = ((again & 0x0f) << 6) | (dgain >> 2);
	ret += gc2093_get_analog_gain_ratio(sd,again);
	ret += gc2093_write(sd,0xb3,val_lut[ration_num].regb1);
	ret += gc2093_write(sd, 0x00b4, 0x00);
	if (ret < 0)
		pr_debug("set again failed is ae\n");

	return 0;
}

static int gc2093_read_integration_time(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char expt = 0;
	unsigned long long sGain;
	ret = gc2093_read(sd,0x0003,&expt);
	sGain = expt;
	ret += gc2093_read(sd,0x0004,&expt);
	sGain = (sGain << 8) | expt;
	time_reg = sGain;
	if(ret < 0)
		pr_debug("get exposuer failed\n");

	return 0;
}

static int fast_ae_set_reg(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	ret = gc2093_write(sd,0x0140,0x23);
	ret += gc2093_read_integration_time(sd,NULL);
	ret += gc2093_set_integration_time(sd,time_reg);
	ret += gc2093_write(sd,0x01af,0x09);
	ret += gc2093_set_analog_gain_ae(sd,NULL);

	ret += gc2093_write(sd,0x01a0,0x00);
	ret += gc2093_write(sd,0x01a4,0x40);
	ret += gc2093_write(sd,0x01a5,0x40);
	ret += gc2093_write(sd,0x01a6,0x40);
	if(ret < 0)
		pr_debug("set time and analgo failed\n");

	return ret;
}

static int fast_ae_set_reg_default(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret = 0;
	ret = gc2093_write(sd,0x0140,0x23);
	ret += gc2093_write(sd,0x04,0x10);
	ret += gc2093_write(sd,0x03,0x00);
	ret += gc2093_write(sd,0x01af,0x09);
	ret += gc2093_write(sd,0xb3,0x00);
	ret += gc2093_write(sd,0xb4,0x00);

	ret += gc2093_write(sd,0x01a0,0x00);
	ret += gc2093_write(sd,0x01a4,0x40);
	ret += gc2093_write(sd,0x01a5,0x40);
	ret += gc2093_write(sd,0x01a6,0x40);
	if(ret < 0)
		pr_debug("set time and analgo failed\n");

	return ret;
}

static int setting_fast_ae(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char y_avg = 0,y_target = 0;
	int ret = 0,loop = 0;
	gc2093_read(sd,0x00a9,&y_avg);
	gc2093_read(sd,0x01b5,&y_target);
	while((y_avg >= y_target ? y_avg - y_target : y_target - y_avg) > 0x8)
	{
		private_msleep(2);
		if(loop > 30) {
			pr_debug("y_avg != y_target,set failed!!!\n");
			break;
		}
		gc2093_read(sd,0x01b5,&y_target);
		gc2093_read(sd,0x00a9,&y_avg);
		loop++;
	}
	if(loop <= 30) {
		ret = fast_ae_set_reg(sd,NULL);
		pr_debug("set time and analgo of right value\n");
	} else {
		ret = fast_ae_set_reg_default(sd,NULL);
		pr_debug("set time and analgo of default value\n");
	}
	gc2093_write(sd,0x01e0,0x07);

	return ret;
}
#endif
static int gc2093_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = gc2093_write_array(sd, wsize->regs);
#ifdef FAST_AE
	ret += setting_fast_ae(sd,NULL);
	ration_num = 0;
#endif
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int gc2093_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = gc2093_write_array(sd, gc2093_stream_on);
		pr_debug("gc2093 stream on\n");
	} else {
		ret = gc2093_write_array(sd, gc2093_stream_off);
		pr_debug("gc2093 stream off\n");
	}

	return ret;
}

static int gc2093_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int wpclk = 0;
	unsigned short vts = 0;
	unsigned short hts=0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int vb;
	int ret = 0;

	wpclk = GC2093_SUPPORT_30FPS_SCLK;

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (sensor_max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)){
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}
	ret = gc2093_write(sd, 0xfe, 0x0);
	ret += gc2093_read(sd, 0x05, &tmp);
	hts = tmp & 0x0f;
	ret += gc2093_read(sd, 0x06, &tmp);
	if(ret < 0)
		return -1;
	hts = ((hts << 8) + tmp) << 1; //1450

	if(data_type == TX_SENSOR_DATA_TYPE_WDR_DOL)
		hts <<= 1; //2900

	//vts = vb + 20 +win_width;win_width = 1108
	vts = wpclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = gc2093_write(sd, 0x41, (unsigned char)((vts & 0x3f00) >> 8));
	ret += gc2093_write(sd, 0x42, (unsigned char)(vts & 0xff));
	if(ret < 0)
		return -1;
	vb = vts - 20 - 1088;
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts;
	if(data_type == TX_SENSOR_DATA_TYPE_WDR_DOL)
	{
		sensor->video.attr->max_integration_time = vb < 200 ? vts - vb - 4: vts - 200 - 4;
		sensor->video.attr->max_integration_time_short = vb < 200 ? vb - 4 : 200 - 4;
	}
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;
}

#ifdef __WDR__
static int gc2093_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = gc2093_write(sd, 0x02, value & 0xff);
	ret += gc2093_write(sd, 0x01, (value>>8)&0x3f);
	if (ret < 0) {
		ISP_ERROR("gc2093_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}
static int gc2093_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
	int ret = 0;
	ret = gc2093_write(sd, 0x03fe, 0xf0);
	ret = gc2093_write(sd, 0x03fe, 0xf0),
	ret = gc2093_write(sd, 0x03fe, 0xf0),
	ret = gc2093_write(sd, 0x03fe, 0x00),
	private_msleep(5);

	ret = gc2093_write_array(sd, wsize->regs);
	ret = gc2093_write_array(sd, gc2093_stream_on);
	return 0;
}

static int gc2093_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = 0;

	if(wdr_en == 1){
		sensor_max_fps = TX_SENSOR_MAX_FPS_15;
		wsize = &gc2093_win_sizes[1];
		data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		memcpy(&gc2093_attr.mipi, &gc2093_mipi_wdr, sizeof(gc2093_mipi_wdr));
		gc2093_attr.data_type = data_type;
		gc2093_attr.wdr_cache = wdr_bufsize;
		gc2093_attr.one_line_expr_in_us = 27;
		gc2093_attr.max_integration_time = 1150 - 4;
		gc2093_attr.max_integration_time_short = 200 - 4;
		gc2093_attr.max_integration_time_native = 1150 - 4;
		gc2093_attr.integration_time_limit = 1150 - 4;
		gc2093_attr.total_width = 1450;
		gc2093_attr.total_height = 1350;
	}
	else if (wdr_en == 0){
		sensor_max_fps = TX_SENSOR_MAX_FPS_30;
		wsize = &gc2093_win_sizes[0];
		data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		memcpy(&gc2093_attr.mipi, &gc2093_mipi_linear, sizeof(gc2093_mipi_linear));
		gc2093_attr.data_type = data_type;
		gc2093_attr.one_line_expr_in_us = 30;
		gc2093_attr.max_integration_time = 1350 - 4;
		gc2093_attr.integration_time_limit = 1350 - 4;
		gc2093_attr.max_integration_time_native = 1350 - 4;
		gc2093_attr.total_width = 1450;
		gc2093_attr.total_height = 1350;
	}
	else
	{
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	sensor->video.attr = &gc2093_attr;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}
#endif

static int gc2093_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}
	return ret;
}


static int gc2093_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"gc2093_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"gc2093_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = gc2093_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc2093 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("gc2093 chip found @ 0x%02x (%s)\n sensor drv version %s", client->addr, client->adapter->name, SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "gc2093", sizeof("gc2093"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int gc2093_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			if(arg)
				ret = gc2093_set_integration_time(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			if(arg)
				ret = gc2093_set_analog_gain(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_DGAIN:
			if(arg)
				ret = gc2093_set_digital_gain(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
			if(arg)
				ret = gc2093_get_black_pedestal(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if(arg)
				ret = gc2093_set_mode(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
			ret = gc2093_write_array(sd, gc2093_stream_off);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			ret = gc2093_write_array(sd, gc2093_stream_on);
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if(arg)
				ret = gc2093_set_fps(sd, *(int*)arg);
			break;
#ifdef __WDR__
		case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
			if(arg)
				ret = gc2093_set_integration_time_short(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_WDR:
			if(arg)
				ret = gc2093_set_wdr(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_WDR_STOP:
			if(arg)
				ret = gc2093_set_wdr_stop(sd, *(int*)arg);
			break;
#endif
		default:
			break;
	}

	return ret;
}

static int gc2093_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = gc2093_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int gc2093_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc2093_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops gc2093_core_ops = {
	.g_chip_ident = gc2093_g_chip_ident,
	.reset = gc2093_reset,
	.init = gc2093_init,
	/*.ioctl = gc2093_ops_ioctl,*/
	.g_register = gc2093_g_register,
	.s_register = gc2093_s_register,
};

static struct tx_isp_subdev_video_ops gc2093_video_ops = {
	.s_stream = gc2093_s_stream,
};

static struct tx_isp_subdev_sensor_ops	gc2093_sensor_ops = {
	.ioctl	= gc2093_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops gc2093_ops = {
	.core = &gc2093_core_ops,
	.video = &gc2093_video_ops,
	.sensor = &gc2093_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "gc2093",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc2093_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	unsigned long rate = 0;
	int ret;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));

	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	rate = clk_get_rate(clk_get_parent(sensor->mclk));
	if (((rate / 1000) % 27000) != 0) {
		struct clk *vpll;
		vpll = clk_get(NULL,"vpll");
		if (IS_ERR(vpll)) {
			pr_err("get vpll failed\n");
		} else {
			rate = clk_get_rate(vpll);
			if (((rate / 1000) % 27000) != 0) {
				clk_set_rate(vpll,1080000027);
			}
			ret = clk_set_parent(sensor->mclk, vpll);
			if (ret < 0)
				pr_err("set mclk parent as epll err\n");
		}
	}

	private_clk_set_rate(sensor->mclk, 27000000);
	private_clk_enable(sensor->mclk);

	/*
	   convert sensor-gain into isp-gain,
	 */

#ifdef __WDR__
	if(data_type == TX_SENSOR_DATA_TYPE_WDR_DOL)
	{
		wsize = &gc2093_win_sizes[1];
		memcpy(&gc2093_attr.mipi, &gc2093_mipi_wdr, sizeof(gc2093_mipi_wdr));
		gc2093_attr.data_type = data_type;
		gc2093_attr.wdr_cache = wdr_bufsize;
		gc2093_attr.one_line_expr_in_us = 27;
		gc2093_attr.max_integration_time_native = 1150 - 4;
		gc2093_attr.integration_time_limit = 1150 - 4;
		gc2093_attr.max_integration_time = 1150 - 4;
		gc2093_attr.max_integration_time_short = 200 - 4;
		gc2093_attr.total_width = 1450;
		gc2093_attr.total_height = 1350;
	}
	else
	{
#ifdef FAST_AE
			wsize = &gc2093_win_sizes[2];
			memcpy(&gc2093_attr.mipi, &gc2093_mipi_linear, sizeof(gc2093_mipi_linear));
			gc2093_attr.one_line_expr_in_us = 30;
			gc2093_attr.max_integration_time_native = 1350 - 4;
			gc2093_attr.integration_time_limit = 1350 - 4;
			gc2093_attr.max_integration_time = 1350 - 4;
			gc2093_attr.total_width = 1450;
			gc2093_attr.total_height = 1350;
			pr_debug("probe in fast ae------->gc2093\n");
#else
			wsize = &gc2093_win_sizes[0];
			memcpy(&gc2093_attr.mipi, &gc2093_mipi_linear, sizeof(gc2093_mipi_linear));
			gc2093_attr.one_line_expr_in_us = 30;
			gc2093_attr.max_integration_time_native = 1350 - 4;
			gc2093_attr.integration_time_limit = 1350 - 4;
			gc2093_attr.max_integration_time = 1350 - 4;
			gc2093_attr.total_width = 1450;
			gc2093_attr.total_height = 1350;
			pr_debug("probe in default------->gc2093\n");
#endif
	}
#endif

	gc2093_attr.dbus_type = data_interface;
	gc2093_attr.data_type = data_type;
	gc2093_attr.expo_fs = 1;

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &gc2093_attr;
	sensor->video.shvflip = shvflip;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc2093_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->gc2093\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int gc2093_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);
	return 0;
}

static const struct i2c_device_id gc2093_id[] = {
	{ "gc2093", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc2093_id);

static struct i2c_driver gc2093_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "gc2093",
	},
	.probe		= gc2093_probe,
	.remove		= gc2093_remove,
	.id_table	= gc2093_id,
};

static __init int init_gc2093(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init gc2093 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&gc2093_driver);
}

static __exit void exit_gc2093(void)
{
	private_i2c_del_driver(&gc2093_driver);
}

module_init(init_gc2093);
module_exit(exit_gc2093);

MODULE_DESCRIPTION("A low-level driver for gc2093 sensors");
MODULE_LICENSE("GPL");
