// SPDX-License-Identifier: GPL-2.0+
/*
 * gc2053.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <tx-isp-common.h>
#include <sensor-common.h>
#include <sensor-info.h>

#define SENSOR_NAME "gc2053"
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x37
#define SENSOR_MAX_WIDTH 1920
#define SENSOR_MAX_HEIGHT 1080
#define SENSOR_CHIP_ID 0x2053
#define SENSOR_CHIP_ID_H (0x20)
#define SENSOR_CHIP_ID_L (0x53)
#define SENSOR_REG_END 0xff
#define SENSOR_REG_DELAY 0x00
#define SENSOR_PAGE_REG 0xfe
#define SENSOR_SUPPORT_WPCLK_FPS_30 (74250000)
#define SENSOR_SUPPORT_WPCLK_FPS_15 (48000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20190708a"

static struct sensor_info sensor_info = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.version = SENSOR_VERSION,
	.min_fps = SENSOR_OUTPUT_MIN_FPS,
	.max_fps = SENSOR_OUTPUT_MAX_FPS,
	.chip_i2c_addr = SENSOR_I2C_ADDRESS,
	.width = SENSOR_MAX_WIDTH,
	.height = SENSOR_MAX_HEIGHT,
};

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_LOW_10BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

static int data_interface = TX_SENSOR_DATA_INTERFACE_DVP;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_25;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

struct tx_isp_sensor_attribute sensor_attr;

struct regval_list {
    uint16_t reg_num;
    unsigned char value;
};

struct again_lut {
    int index;
    unsigned int regb4;
    unsigned int regb3;
    unsigned int regb2;
    unsigned int dpc;
    unsigned int blc;
    unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
/*	old gain lut
	{0x0, 0x00, 0x00, 0x00, 0x01, 0x00, 0},
	{0x01, 0x00, 0x00, 0x14, 0x01, 0x00, 7910},
	{0x02, 0x00, 0x10, 0x00, 0x01, 0x0c, 15820},
	{0x03, 0x00, 0x10, 0x18, 0x01, 0x0c, 24564},
	{0x04, 0x00, 0x20, 0x00, 0x01, 0x1b, 33309},
	{0x05, 0x00, 0x20, 0x14, 0x01, 0x1b, 41219},
	{0x06, 0x00, 0x30, 0x00, 0x01, 0x2c, 49129},
	{0x07, 0x00, 0x30, 0x14, 0x01, 0x2c, 56978},
	{0x08, 0x00, 0x40, 0x00, 0x01, 0x3f, 64826},
	{0x09, 0x00, 0x40, 0x04, 0x01, 0x3f, 66804},
	{0x0a, 0x00, 0x40, 0x0c, 0x01, 0x3f, 68781},
	{0x0b, 0x00, 0x40, 0x10, 0x01, 0x3f, 70759},
	{0x0c, 0x00, 0x40, 0x14, 0x01, 0x3f, 72736},
	{0x0d, 0x00, 0x40, 0x1c, 0x01, 0x3f, 74714},
	{0x0e, 0x00, 0x40, 0x20, 0x01, 0x3f, 76692},
	{0x0f, 0x00, 0x40, 0x28, 0x01, 0x3f, 78669},
	{0x10, 0x00, 0x50, 0x00, 0x02, 0x16, 80647},
	{0x11, 0x00, 0x50, 0x0c, 0x02, 0x16, 85072},
	{0x12, 0x00, 0x50, 0x18, 0x02, 0x16, 89497},
	{0x13, 0x00, 0x50, 0x28, 0x02, 0x16, 93922},
	{0x14, 0x00, 0x60, 0x00, 0x02, 0x35, 98347},
	{0x15, 0x00, 0x60, 0x0c, 0x02, 0x35, 102303},
	{0x16, 0x00, 0x60, 0x14, 0x02, 0x35, 106258},
	{0x17, 0x00, 0x60, 0x20, 0x02, 0x35, 110213},
	{0x18, 0x00, 0x70, 0x00, 0x03, 0x16, 114168},
	{0x19, 0x00, 0x70, 0x0c, 0x03, 0x16, 118572},
	{0x1A, 0x00, 0x70, 0x18, 0x03, 0x16, 122975},
	{0x1B, 0x00, 0x70, 0x24, 0x03, 0x16, 127379},
	{0x1C, 0x00, 0x80, 0x00, 0x04, 0x02, 131783},
	{0x1d, 0x00, 0x80, 0x14, 0x04, 0x02, 139693},
	{0x1e, 0x00, 0x90, 0x00, 0x04, 0x31, 147603},
	{0x1f, 0x00, 0x90, 0x18, 0x04, 0x31, 156760},
	{0x20, 0x00, 0xa0, 0x00, 0x05, 0x32, 165916},
	{0x21, 0x00, 0xa0, 0x14, 0x05, 0x32, 173826},
	{0x22, 0x00, 0xb0, 0x00, 0x06, 0x35, 181736},
	{0x23, 0x00, 0xb0, 0x14, 0x06, 0x35, 189505},
	{0x24, 0x00, 0xc0, 0x00, 0x08, 0x04, 197274},
	{0x25, 0x00, 0xc0, 0x14, 0x08, 0x04, 204498},
	{0x26, 0x00, 0x5a, 0x00, 0x09, 0x19, 211722},
	{0x27, 0x00, 0x5a, 0x18, 0x09, 0x19, 220205},
	{0x28, 0x00, 0x83, 0x00, 0x0b, 0x0f, 228688},
	{0x29, 0x00, 0x83, 0x14, 0x0b, 0x0f, 236598},
	{0x2A, 0x00, 0x93, 0x00, 0x0d, 0x12, 244508},
	{0x2B, 0x00, 0x93, 0x18, 0x0d, 0x12, 253315},
	{0x2C, 0x00, 0x84, 0x00, 0x10, 0x00, 262123},
	{0x2d, 0x00, 0x84, 0x14, 0x10, 0x00, 270033},
	{0x2e, 0x00, 0x94, 0x00, 0x12, 0x3a, 277943},
	{0x2f, 0x00, 0x94, 0x14, 0x12, 0x3a, 285492},
	{0x30, 0x00, 0x94, 0x2c, 0x12, 0x3a, 293040},
	{0x31, 0x00, 0x94, 0x48, 0x12, 0x3a, 300589},
	{0x32, 0x01, 0x2c, 0x00, 0x1a, 0x02, 308137},
	{0x33, 0x01, 0x3c, 0x00, 0x1b, 0x20, 313352},
	{0x34, 0x01, 0x3c, 0x14, 0x1b, 0x20, 320862},
	{0x35, 0x00, 0x8c, 0x00, 0x20, 0x0f, 328372},
	{0x36, 0x00, 0x8c, 0x14, 0x20, 0x0f, 336282},
	{0x37, 0x00, 0x9c, 0x00, 0x26, 0x07, 344192},
	{0x38, 0x00, 0x9c, 0x14, 0x26, 0x07, 352652},
	{0x39, 0x00, 0x9c, 0x2c, 0x26, 0x07, 361112},
	{0x3a, 0x00, 0x9c, 0x48, 0x26, 0x07, 369571},
	{0x3b, 0x02, 0x64, 0x00, 0x36, 0x21, 378031},
	{0x3c, 0x02, 0x74, 0x00, 0x37, 0x3a, 380432},
	{0x3d, 0x00, 0xc6, 0x00, 0x3d, 0x02, 388722},
	{0x3e, 0x00, 0xc6, 0x14, 0x3d, 0x02, 397647},
	{0x3f, 0x00, 0xc6, 0x2c, 0x3d, 0x02, 406572},
	{0x40, 0x00, 0xdc, 0x00, 0x3f, 0x3f, 415498},
	{0x41, 0x02, 0x85, 0x00, 0x3f, 0x3f, 421213},
	{0x42, 0x02, 0x85, 0x14, 0x3f, 0x3f, 430783},
	{0x43, 0x02, 0x36, 0x00, 0x3f, 0x3f, 440353},
	{0x44, 0x00, 0xce, 0x00, 0x3f, 0x3f, 444863},
*/
	{0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0},
	{0x01, 0x00, 0x00, 0x14, 0x01, 0x00, 5776},
	{0x02, 0x00, 0x00, 0x20, 0x01, 0x00, 11553},
	{0x03, 0x00, 0x10, 0x00, 0x01, 0x0c, 17330},
	{0x04, 0x00, 0x10, 0x10, 0x01, 0x0c, 23450},
	{0x05, 0x00, 0x20, 0x00, 0x01, 0x1b, 29570},
	{0x06, 0x00, 0x20, 0x14, 0x01, 0x1b, 36240},
	{0x07, 0x00, 0x20, 0x28, 0x01, 0x1b, 42910},
	{0x08, 0x00, 0x30, 0x00, 0x01, 0x2c, 49580},
	{0x09, 0x00, 0x30, 0x14, 0x01, 0x2c, 57187},
	{0x0A, 0x00, 0x40, 0x00, 0x01, 0x3f, 64793},
	{0x0B, 0x00, 0x40, 0x14, 0x01, 0x3f, 72266},
	{0x0C, 0x00, 0x50, 0x00, 0x02, 0x16, 79739},
	{0x0D, 0x00, 0x50, 0x18, 0x02, 0x16, 88754},
	{0x0E, 0x00, 0x60, 0x00, 0x02, 0x35, 97768},
	{0x0F, 0x00, 0x60, 0x18, 0x02, 0x35, 106442},
	{0x10, 0x00, 0x70, 0x00, 0x03, 0x16, 115115},
	{0x11, 0x00, 0x70, 0x14, 0x03, 0x16, 122907},
	{0x12, 0x00, 0x80, 0x00, 0x04, 0x02, 130699},
	{0x13, 0x00, 0x80, 0x14, 0x04, 0x02, 138382},
	{0x14, 0x00, 0x90, 0x00, 0x04, 0x31, 146065},
	{0x15, 0x00, 0x90, 0x18, 0x04, 0x31, 154815},
	{0x16, 0x00, 0xa0, 0x00, 0x05, 0x32, 163565},
	{0x17, 0x00, 0xa0, 0x14, 0x05, 0x32, 171002},
	{0x18, 0x00, 0xb0, 0x00, 0x06, 0x35, 178439},
	{0x19, 0x00, 0xb0, 0x18, 0x06, 0x35, 186777},
	{0x1A, 0x00, 0xc0, 0x00, 0x08, 0x04, 195116},
	{0x1B, 0x00, 0xc0, 0x14, 0x08, 0x04, 202863},
	{0x1C, 0x00, 0x5a, 0x00, 0x09, 0x19, 210610},
	{0x1D, 0x00, 0x5a, 0x14, 0x09, 0x19, 216774},
	{0x1E, 0x00, 0x5a, 0x20, 0x09, 0x19, 222937},
	{0x1F, 0x00, 0x83, 0x00, 0x0b, 0x0f, 229100},
	{0x20, 0x00, 0x83, 0x14, 0x0b, 0x0f, 236816},
	{0x21, 0x00, 0x93, 0x00, 0x0d, 0x12, 244531},
	{0x22, 0x00, 0x93, 0x18, 0x0d, 0x12, 252603},
	{0x23, 0x00, 0x84, 0x00, 0x10, 0x00, 260674},
	{0x24, 0x00, 0x84, 0x14, 0x10, 0x00, 268190},
	{0x25, 0x00, 0x94, 0x00, 0x12, 0x3a, 275706},
	{0x26, 0x00, 0x94, 0x18, 0x12, 0x3a, 283977},
	{0x27, 0x01, 0x2c, 0x00, 0x1a, 0x02, 292248},
	{0x28, 0x01, 0x2c, 0x18, 0x1a, 0x02, 300872},
	{0x29, 0x01, 0x3c, 0x00, 0x1b, 0x20, 309495},
	{0x2A, 0x01, 0x3c, 0x18, 0x1b, 0x20, 317963},
	{0x2B, 0x00, 0x8c, 0x00, 0x20, 0x0f, 326431},
	{0x2C, 0x00, 0x8c, 0x18, 0x20, 0x0f, 334373},
	{0x2D, 0x00, 0x9c, 0x00, 0x26, 0x07, 342315},
	{0x2E, 0x00, 0x9c, 0x18, 0x26, 0x07, 350616},
	{0x2F, 0x02, 0x64, 0x00, 0x36, 0x21, 358918},
	{0x30, 0x02, 0x64, 0x18, 0x36, 0x21, 366834},
	{0x31, 0x02, 0x74, 0x00, 0x37, 0x3a, 374750},
	{0x32, 0x02, 0x74, 0x18, 0x37, 0x3a, 382687},
	{0x33, 0x00, 0xc6, 0x00, 0x3d, 0x02, 390624},
	{0x34, 0x00, 0xc6, 0x18, 0x3d, 0x02, 399464},
	{0x35, 0x00, 0xdc, 0x00, 0x3f, 0x3f, 408304},
	{0x36, 0x00, 0xdc, 0x18, 0x3f, 0x3f, 416945},
	{0x37, 0x02, 0x85, 0x00, 0x3f, 0x3f, 425587},
	{0x38, 0x02, 0x85, 0x18, 0x3f, 0x3f, 433407},
	{0x39, 0x03, 0x36, 0x00, 0x3f, 0x3f, 441227},
	{0x3A, 0x03, 0x36, 0x16, 0x3f, 0x3f, 447199},
	{0x3B, 0x00, 0xce, 0x00, 0x3f, 0x3f, 453170}
};

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].index;
			return lut[0].gain;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
				return lut->gain;
			}
		}
		lut++;
	}
	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return 0;
}

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
	},
	.max_again = 453170,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 1346,
	.integration_time_limit = 1346,
	.total_width = 2200,
	.total_height = 1350,
	.max_integration_time = 1346,
	.one_line_expr_in_us = 30,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

/*
 * mclk=24m
 * pclk=75m hts=2200
 * vts=1125 row_time=29
 * 629us
 * max 30fps
 */
static struct regval_list sensor_init_regs_1920_1080_25fps_dvp[] = {
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
	{0x41, 0x05},
	{0x42, 0x46},/*vts for 25fps*/
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
	{0x01, 0x20},
	{0x02, 0x56},
	{0x03, 0xb2},
	{0x12, 0x80},
	{0x13, 0x07},
	{0xfe, 0x00},
	{0x3e, 0x40},

	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_15fps_dvp[] = {

	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_25fps_mipi[] = {

	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1920*1080 */
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_25fps_dvp,
	},
};

static enum v4l2_mbus_pixelcode sensor_mbus_code[] = {
	V4L2_MBUS_FMT_SRGGB10_1X10,
	V4L2_MBUS_FMT_SBGGR12_1X12,
};

static struct regval_list sensor_stream_on[] = {
	//{ 0xf2, 0x8f},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	//{ 0xf2, 0x80},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, unsigned char reg, unsigned char *value) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct i2c_msg msg[2] = {
		[0] = {
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		[1] = {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = value,
		}
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int sensor_write(struct tx_isp_subdev *sd, unsigned char reg, unsigned char value) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
		.buf = buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sensor_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
			if (vals->reg_num == SENSOR_PAGE_REG) {
				val &= 0xf8;
				val |= (vals->value & 0x07);
				ret = sensor_write(sd, vals->reg_num, val);
				ret = sensor_read(sd, vals->reg_num, &val);
			}
		}
		vals++;
	}
	return 0;
}

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sensor_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int sensor_reset(struct tx_isp_subdev *sd, int val) {
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident) {
	unsigned char v;
	int ret;
	ret = sensor_read(sd, 0xf0, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;

	ret = sensor_read(sd, 0xf1, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;

	*ident = (*ident << 8) | v;
	return 0;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	ret = sensor_write(sd, 0x04, value & 0xff);
	ret += sensor_write(sd, 0x03, (value & 0x3f00) >> 8);
	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d\n", __LINE__);
		return ret;
	}
	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, unsigned int value) {
	int ret = 0;
	struct again_lut *val_lut = sensor_again_lut;
	ret = sensor_write(sd, 0xfe, 0x00);
	ret += sensor_write(sd, 0xb4, val_lut[value].regb4);
	ret += sensor_write(sd, 0xb3, val_lut[value].regb3);
	ret += sensor_write(sd, 0xb2, val_lut[value].regb2);
	ret += sensor_write(sd, 0xb8, val_lut[value].dpc);
	ret += sensor_write(sd, 0xb9, val_lut[value].blc);
	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d\n", __LINE__);
		return ret;
	}
	return 0;
}

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, int enable) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
	int ret = 0;

	if (!enable)
		return ISP_SUCCESS;

	if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
		switch (sensor_max_fps) {
			case TX_SENSOR_MAX_FPS_25:
				wsize->fps = 25 << 16 | 1;
				wsize->regs = sensor_init_regs_1920_1080_25fps_dvp;
				break;
			case TX_SENSOR_MAX_FPS_15:
				wsize->fps = 15 << 16 | 1;
				wsize->regs = sensor_init_regs_1920_1080_15fps_dvp;
				break;
			default:
				printk("Now we do not support this framerate!!!\n");
		}
	} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
		wsize->fps = 25 << 16 | 1;
		wsize->regs = sensor_init_regs_1920_1080_25fps_mipi;
	} else {
		printk("Don't support this Sensor Data interface\n");
	}

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = sensor_write_array(sd, wsize->regs);
	if (ret)
		return ret;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;
	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, int enable) {
	int ret = 0;

	if (enable) {
		ret = sensor_write_array(sd, sensor_stream_on);
		ISP_WARNING("%s stream on\n", SENSOR_NAME);
	} else {
		ret = sensor_write_array(sd, sensor_stream_off);
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
	}
	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int clk = 0;
	unsigned short vts = 0;
	unsigned short hts = 0;
	unsigned int max_fps = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch (sensor_max_fps) {
		case TX_SENSOR_MAX_FPS_25:
			clk = SENSOR_SUPPORT_WPCLK_FPS_30;
			max_fps = SENSOR_OUTPUT_MAX_FPS;
			break;
		case TX_SENSOR_MAX_FPS_15:
			clk = SENSOR_SUPPORT_WPCLK_FPS_15;
			max_fps = TX_SENSOR_MAX_FPS_15;
			break;
		default:
			ret = -1;
			printk("Now we do not support this framerate!!!\n");
	}

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}

	ret = sensor_write(sd, 0xfe, 0x0);
	ret += sensor_read(sd, 0x05, &val);
	hts = val;
	ret += sensor_read(sd, 0x06, &val);
	if (ret < 0)
		return -1;

	hts = ((hts << 8) + val) << 1;
	vts = clk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = sensor_write(sd, 0x41, (unsigned char) ((vts & 0x3f00) >> 8));
	ret += sensor_write(sd, 0x42, (unsigned char) (vts & 0xff));
	if (ret < 0)
		return -1;

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;

	if (value == TX_ISP_SENSOR_FULL_RES_MAX_FPS) {
		wsize = &sensor_win_sizes[0];
	} else if (value == TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS) {
		wsize = &sensor_win_sizes[0];
	}

	if (wsize) {
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

static int sensor_g_chip_ident(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio, "sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio request fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio request fail %d\n", pwdn_gpio);
		}
	}
	ret = sensor_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an %s chip.\n",
			  client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}

	ISP_WARNING("%s chip found @ 0x%02x (%s)\n",
		    SENSOR_NAME, client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n", SENSOR_VERSION);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg) {
	long ret = 0;
	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd) {
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			if (arg)
				ret = sensor_set_integration_time(sd, *(int *) arg);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			if (arg)
				ret = sensor_set_analog_gain(sd, *(int *) arg);
			break;
		case TX_ISP_EVENT_SENSOR_DGAIN:
			if (arg)
				ret = sensor_set_digital_gain(sd, *(int *) arg);
			break;
		case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
			if (arg)
				ret = sensor_get_black_pedestal(sd, *(int *) arg);
			break;
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if (arg)
				ret = sensor_set_mode(sd, *(int *) arg);
			break;
		case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
			ret = sensor_write_array(sd, sensor_stream_off);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			ret = sensor_write_array(sd, sensor_stream_on);
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if (arg)
				ret = sensor_set_fps(sd, *(int *) arg);
			break;
		default:
			break;
	}

	return ret;
}

static int sensor_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg) {
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}

	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	ret = sensor_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int sensor_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg) {
	int len = 0;
	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}

	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	sensor_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}

static struct tx_isp_subdev_core_ops sensor_core_ops = {
	.g_chip_ident = sensor_g_chip_ident,
	.reset = sensor_reset,
	.init = sensor_init,
	.g_register = sensor_g_register,
	.s_register = sensor_s_register,
};

static struct tx_isp_subdev_video_ops sensor_video_ops = {
	.s_stream = sensor_s_stream,
};

static struct tx_isp_subdev_sensor_ops sensor_sensor_ops = {
	.ioctl = sensor_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.sensor = &sensor_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
	.name = SENSOR_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id) {
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
	enum v4l2_mbus_pixelcode mbus;
	int i = 0;
	int ret;

	sensor = (struct tx_isp_sensor *) kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	clk_set_rate(sensor->mclk, 24000000);
	clk_enable(sensor->mclk);

	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;

	sensor_attr.dvp.gpio = sensor_gpio_func;
	switch (sensor_gpio_func) {
		case DVP_PA_LOW_10BIT:
		case DVP_PA_HIGH_10BIT:
			mbus = sensor_mbus_code[0];
			break;
		case DVP_PA_12BIT:
			mbus = sensor_mbus_code[1];
			break;
		default:
			goto err_set_sensor_gpio;
	}

	for (i = 0; i < ARRAY_SIZE(sensor_win_sizes); i++)
		sensor_win_sizes[i].mbus_code = mbus;

	/*
	  convert sensor-gain into isp-gain,
	*/
	switch (sensor_max_fps) {
		case TX_SENSOR_MAX_FPS_25:
			break;
		case TX_SENSOR_MAX_FPS_15:
			sensor_attr.max_integration_time_native = 1124;
			sensor_attr.integration_time_limit = 1124;
			sensor_attr.total_width = 2840;
			sensor_attr.total_height = 1128;
			sensor_attr.max_integration_time = 1124;
			break;
		default:
			printk("Now we do not support this framerate!!!\n");
	}

	sensor_attr.max_again = 453170;
	sensor_attr.max_dgain = sensor_attr.max_dgain;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sensor_attr;
	sensor->video.mbus_change = 0;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);
	pr_debug("probe ok ------->%s\n", SENSOR_NAME);
	return 0;

err_set_sensor_gpio:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);
	return -1;
}

static int sensor_remove(struct i2c_client *client) {
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);
	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{SENSOR_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
	},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

static __init int init_sensor(void) {
	int ret = 0;
	sensor_common_init(&sensor_info);
	ret = private_driver_get_interface();
	if (ret) {
		ISP_ERROR("Failed to init %s driver.\n", SENSOR_NAME);
		return -1;
	}
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	private_i2c_del_driver(&sensor_driver);
	sensor_common_exit();
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
