// SPDX-License-Identifier: GPL-2.0+
/*
 * gc2023.c
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
#include <soc/gpio.h>
#include <tx-isp-common.h>
#include <sensor-common.h>

#define SENSOR_NAME "gc2023"
#define SENSOR_CHIP_ID_H (0x20)
#define SENSOR_CHIP_ID_L (0x23)
#define SENSOR_REG_END 0xff
#define SENSOR_REG_DELAY 0x00
#define SENSOR_PAGE_REG 0xfe
#define SENSOR_SUPPORT_PCLK_FPS_30 (96*1000*1000)
#define SENSOR_SUPPORT_PCLK_FPS_15 (42*1000*1000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20170911a"

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
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

static int sensor_max_fps = TX_SENSOR_MAX_FPS_25;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

const unsigned int  ANALOG_GAIN_1 = (1<<TX_ISP_GAIN_FIXED_POINT)|(unsigned int)((0.0*(1<<TX_ISP_GAIN_FIXED_POINT)));
const unsigned int  ANALOG_GAIN_2 = (1<<TX_ISP_GAIN_FIXED_POINT)|(unsigned int)((0.42*(1<<TX_ISP_GAIN_FIXED_POINT)));
const unsigned int  ANALOG_GAIN_3 = (1<<TX_ISP_GAIN_FIXED_POINT)|(unsigned int)((0.99*(1<<TX_ISP_GAIN_FIXED_POINT)));
const unsigned int  ANALOG_GAIN_4 = (2<<TX_ISP_GAIN_FIXED_POINT)|(unsigned int)((0.86*(1<<TX_ISP_GAIN_FIXED_POINT)));
const unsigned int  ANALOG_GAIN_5 = (4<<TX_ISP_GAIN_FIXED_POINT)|(unsigned int)((0.05*(1<<TX_ISP_GAIN_FIXED_POINT)));
const unsigned int  ANALOG_GAIN_6 = (5<<TX_ISP_GAIN_FIXED_POINT)|(unsigned int)((0.78*(1<<TX_ISP_GAIN_FIXED_POINT)));
const unsigned int  ANALOG_GAIN_7 = (8<<TX_ISP_GAIN_FIXED_POINT)|(unsigned int)((0.23*(1<<TX_ISP_GAIN_FIXED_POINT)));
const unsigned int  ANALOG_GAIN_8 = (11<<TX_ISP_GAIN_FIXED_POINT)|(unsigned int)((0.72*(1<<TX_ISP_GAIN_FIXED_POINT)));
const unsigned int  ANALOG_GAIN_9 = (16<<TX_ISP_GAIN_FIXED_POINT)|(unsigned int)((0.55*(1<<TX_ISP_GAIN_FIXED_POINT)));
const unsigned int  ANALOG_GAIN_10 = (22<<TX_ISP_GAIN_FIXED_POINT)|(unsigned int)((0.68*(1<<TX_ISP_GAIN_FIXED_POINT)));

struct tx_isp_sensor_attribute sensor_attr;

unsigned int fix_point_mult2(unsigned int a, unsigned int b)
{
	unsigned int x1,x2,x;
	unsigned int a1,a2,b1,b2;
	unsigned int mask = (((unsigned int)0xffffffff)>>(32-TX_ISP_GAIN_FIXED_POINT));
	a1 = a>>TX_ISP_GAIN_FIXED_POINT;
	a2 = a&mask;
	b1 = b>>TX_ISP_GAIN_FIXED_POINT;
	b2 = b&mask;
	x1 = a1*b1;
	x1 += (a1*b2)>>TX_ISP_GAIN_FIXED_POINT;
	x1 += (a2*b1)>>TX_ISP_GAIN_FIXED_POINT;
	x2 = (a1*b2)&mask;
	x2 += (a2*b1)&mask;
	x2 += (a2*b2)>>TX_ISP_GAIN_FIXED_POINT;
	x = (x1<<TX_ISP_GAIN_FIXED_POINT)+x2;
	return x;
}

unsigned int fix_point_mult3(unsigned int a, unsigned int b, unsigned int c)
{
	unsigned int x = 0;
	x = fix_point_mult2(a,b);
	x = fix_point_mult2(x,c);
	return x;
}

#define ANALOG_GAIN_MAX (fix_point_mult2(ANALOG_GAIN_10, (0xf<<TX_ISP_GAIN_FIXED_POINT) + (0x3f<<(TX_ISP_GAIN_FIXED_POINT-6))))

unsigned int sensor_gainone_to_reg(unsigned int gain_one, unsigned int *regs)
{
	unsigned int gain_one1 = 0;
	unsigned int gain_tmp = 0;
	unsigned char regb6 = 0;
	unsigned char regb1 =0x1;
	unsigned char regb2 = 0;
	int i,j;
	unsigned int gain_one_max = fix_point_mult2(ANALOG_GAIN_10, (0xf<<TX_ISP_GAIN_FIXED_POINT) + (0x3f<<(TX_ISP_GAIN_FIXED_POINT-6)));
	if (gain_one < ANALOG_GAIN_1) {
		gain_one1 = ANALOG_GAIN_1;
		regb6 = 0x00;
		regb1 = 0x01;
		regb2 = 0x00;
		goto done;
	} else if (gain_one < (ANALOG_GAIN_2)) {
		gain_one1 = gain_tmp = ANALOG_GAIN_1;
		regb1 = 0;
		regb2 = 0;
		for (i = 1; i <= 0xf; i++ )
			for (j = 0; j <= 0x3f; j++) {
				gain_tmp = fix_point_mult2(ANALOG_GAIN_1, (i<<TX_ISP_GAIN_FIXED_POINT)+(j<<(TX_ISP_GAIN_FIXED_POINT-6)));
				if (gain_one < gain_tmp) {
					goto done;
				}
				gain_one1 = gain_tmp;
				regb6 = 0x00;
				regb1 = i;
				regb2 = j;
			}
	} else if (gain_one < ANALOG_GAIN_3) {
		gain_one1 = gain_tmp = ANALOG_GAIN_2;
		regb1 = 0;
		regb2 = 0;
		for (i = 1; i <= 0xf; i++ )
			for (j = 0; j <= 0x3f; j++) {
				gain_tmp = fix_point_mult2(ANALOG_GAIN_2, (i<<TX_ISP_GAIN_FIXED_POINT)+(j<<(TX_ISP_GAIN_FIXED_POINT-6)));
				if (gain_one < gain_tmp) {
					goto done;
				}
				gain_one1 = gain_tmp;
				regb6 = 0x01;
				regb1 = i;
				regb2 = j;
			}
	} else if (gain_one < ANALOG_GAIN_4) {
		gain_one1 = gain_tmp = ANALOG_GAIN_3;
		regb1 = 0;
		regb2 = 0;
		for (i = 1; i <= 0xf; i++ )
			for (j = 0; j <= 0x3f; j++) {
				gain_tmp = fix_point_mult2(ANALOG_GAIN_3, (i<<TX_ISP_GAIN_FIXED_POINT)+(j<<(TX_ISP_GAIN_FIXED_POINT-6)));
				if (gain_one < gain_tmp) {
					goto done;
				}
				gain_one1 = gain_tmp;
				regb6 = 0x02;
				regb1 = i;
				regb2 = j;
			}
	} else if (gain_one < ANALOG_GAIN_5) {
		gain_one1 = gain_tmp = ANALOG_GAIN_4;
		regb1 = 0;
		regb2 = 0;
		for (i = 1; i <= 0xf; i++ )
			for (j = 0; j <= 0x3f; j++) {
				gain_tmp = fix_point_mult2(ANALOG_GAIN_4, (i<<TX_ISP_GAIN_FIXED_POINT)+(j<<(TX_ISP_GAIN_FIXED_POINT-6)));
				if (gain_one < gain_tmp) {
					goto done;
				}
				gain_one1 = gain_tmp;
				regb6 = 0x03;
				regb1 = i;
				regb2 = j;
			}
	} else if (gain_one < ANALOG_GAIN_6) {
		gain_one1 = gain_tmp = ANALOG_GAIN_5;
		regb1 = 0;
		regb2 = 0;
		for (i = 1; i <= 0xf; i++ )
			for (j = 0; j <= 0x3f; j++) {
				gain_tmp = fix_point_mult2(ANALOG_GAIN_5, (i<<TX_ISP_GAIN_FIXED_POINT)+(j<<(TX_ISP_GAIN_FIXED_POINT-6)));
				if (gain_one < gain_tmp) {
					goto done;
				}
				gain_one1 = gain_tmp;
				regb6 = 0x04;
				regb1 = i;
				regb2 = j;
			}
	} else if (gain_one < ANALOG_GAIN_7) {
		gain_one1 = gain_tmp = ANALOG_GAIN_6;
		regb1 = 0;
		regb2 = 0;
		for (i = 1; i <= 0xf; i++ )
			for (j = 0; j <= 0x3f; j++) {
				gain_tmp = fix_point_mult2(ANALOG_GAIN_6, (i<<TX_ISP_GAIN_FIXED_POINT)+(j<<(TX_ISP_GAIN_FIXED_POINT-6)));
				if (gain_one < gain_tmp) {
					goto done;
				}
				gain_one1 = gain_tmp;
				regb6 = 0x05;
				regb1 = i;
				regb2 = j;
			}
	} else if (gain_one < ANALOG_GAIN_8) {
		gain_one1 = gain_tmp = ANALOG_GAIN_7;
		regb1 = 0;
		regb2 = 0;
		for (i = 1; i <= 0xf; i++ )
			for (j = 0; j <= 0x3f; j++) {
				gain_tmp = fix_point_mult2(ANALOG_GAIN_7, (i<<TX_ISP_GAIN_FIXED_POINT)+(j<<(TX_ISP_GAIN_FIXED_POINT-6)));
				if (gain_one < gain_tmp) {
					goto done;
				}
				gain_one1 = gain_tmp;
				regb6 = 0x06;
				regb1 = i;
				regb2 = j;
			}
	} else if (gain_one < ANALOG_GAIN_9) {
		gain_one1 = gain_tmp = ANALOG_GAIN_8;
		regb1 = 0;
		regb2 = 0;
		for (i = 1; i <= 0xf; i++ )
			for (j = 0; j <= 0x3f; j++) {
				gain_tmp = fix_point_mult2(ANALOG_GAIN_8, (i<<TX_ISP_GAIN_FIXED_POINT)+(j<<(TX_ISP_GAIN_FIXED_POINT-6)));
				if (gain_one < gain_tmp) {
					goto done;
				}
				gain_one1 = gain_tmp;
				regb6 = 0x07;
				regb1 = i;
				regb2 = j;
			}
	} else if (gain_one < ANALOG_GAIN_10) {
		gain_one1 = gain_tmp = ANALOG_GAIN_9;
		regb1 = 0;
		regb2 = 0;
		for (i = 1; i <= 0xf; i++ )
			for (j = 0; j <= 0x3f; j++) {
				gain_tmp = fix_point_mult2(ANALOG_GAIN_9, (i<<TX_ISP_GAIN_FIXED_POINT)+(j<<(TX_ISP_GAIN_FIXED_POINT-6)));
				if (gain_one < gain_tmp) {
					goto done;
				}
				gain_one1 = gain_tmp;
				regb6 = 0x08;
				regb1 = i;
				regb2 = j;
			}
	} else if (gain_one < gain_one_max) {
		gain_one1 = gain_tmp = ANALOG_GAIN_10;
		regb1 = 0;
		regb2 = 0;
		for (i = 1; i <= 0xf; i++ )
			for (j = 0; j <= 0x3f; j++) {
				gain_tmp = fix_point_mult2(ANALOG_GAIN_10, (i<<TX_ISP_GAIN_FIXED_POINT)+(j<<(TX_ISP_GAIN_FIXED_POINT-6)));
				if (gain_one < gain_tmp) {
					goto done;
				}
				gain_one1 = gain_tmp;
				regb6 = 0x09;
				regb1 = i;
				regb2 = j;
			}
	} else {
		gain_one1 = gain_one_max;
		regb6 = 0x09;
		regb1 = 0xf;
		regb2 = 0x3f;
		goto done;
	}
	gain_one1 = ANALOG_GAIN_1;
	done:
	*regs = (regb6<<12)|(regb1<<8)|(regb2);
	return gain_one1;
}

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	unsigned int gain_one = 0;
	unsigned int gain_one1 = 0;
	unsigned int regs = 0;
	unsigned int isp_gain1 = 0;
	gain_one = private_math_exp2(isp_gain, shift, TX_ISP_GAIN_FIXED_POINT);
	gain_one1 = sensor_gainone_to_reg(gain_one, &regs);
	isp_gain1 = private_log2_fixed_to_fixed(gain_one1, TX_ISP_GAIN_FIXED_POINT, shift);
	*sensor_again = regs;
	return isp_gain1;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return isp_gain;
}

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x2023,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x37,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
	},
	.max_again = 321573,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 1348,
	.integration_time_limit = 1348,
	.total_width = 2840,
	.total_height = 1352,
	.max_integration_time = 1348,
	.one_line_expr_in_us = 30,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_1920_1080_25fps_dvp[] = {
	/*SYS*/
	{0xf2, 0x00},
	{0xf6, 0x00},
	{0xfc, 0x06},
	{0xf7, 0x01},
	{0xf8, 0x07},
	{0xf9, 0x06},
	{0xfa, 0x00},
	{0xfc, 0x0e},
	/*ANALOG & CISCTL*/
	{0xfe, 0x00},
	{0x03, 0x03},
	{0x04, 0xf6},
	{0x05, 0x02},
	{0x06, 0xc6},
	{0x86, 0x51},
	{0x8e, 0x0c},/*drop frame while change VB*/
	{0x07, 0x00},
	{0x08, 0xf8},
	{0x09, 0x00},
	{0x0a, 0x00},
	{0x0b, 0x00},
	{0x0c, 0x00},
	{0x0d, 0x04},
	{0x0e, 0x40},
	{0x0f, 0x07},
	{0x10, 0x88},
	{0x17, 0x54},
	{0x18, 0x02},
	{0x19, 0x0d},
	{0x1a, 0x18},

	{0x20, 0x54},
	{0x23, 0xf0},
	{0x24, 0xc1},
	{0x25, 0x18},
	{0x26, 0x64},
	{0x28, 0xe8},
	{0x29, 0x08},
	{0x2a, 0x08},
	{0x2b, 0x48},
	{0x2f, 0x40},
	{0x30, 0x99},
	{0x34, 0x00},
	{0x38, 0x80},
	{0x3b, 0x12},
	{0x3d, 0xb0},
	{0xcc, 0x8a},
	{0xcd, 0x99},
	{0xcf, 0x70},
	{0xd0, 0x9c},
	{0xd2, 0xc1},
	{0xd8, 0x80},
	{0xda, 0x28},
	{0xdc, 0x24},
	{0xe1, 0x14},
	{0xe3, 0xf0},
	{0xe4, 0xfa},
	{0xe6, 0x1f},
	{0xe8, 0x02},
	{0xe9, 0x02},
	{0xea, 0x03},
	{0xeb, 0x03},
	/*ISP*/
	{0xfe, 0x00},
	{0x80, 0x4c},/*disable sensor DD*/
	{0x88, 0x23},
	{0x89, 0x03},
	{0x90, 0x01},
	{0x92, 0x02},
	{0x94, 0x02},
	{0x95, 0x04},
	{0x96, 0x38},
	{0x97, 0x07},
	{0x98, 0x80},
	/*BLK*/
	{0xfe, 0x00},
	{0x40, 0x22},
	{0x43, 0x07},
	{0x4e, 0x3c},
	{0x4f, 0x00},
	{0x60, 0x00},
	{0x61, 0x80},
	/*GAIN*/
	{0xfe, 0x00},
	{0xb0, 0x58},
	{0xb1, 0x01},
	{0xb2, 0x00},
	{0xb6, 0x00},
	{0xfe, 0x01},
	{0x01, 0x00},
	{0x02, 0x01},
	{0x03, 0x02},
	{0x04, 0x03},
	{0x05, 0x04},
	{0x06, 0x05},
	{0x07, 0x06},
	{0x08, 0x0e},
	{0x09, 0x16},
	{0x0a, 0x1e},
	{0x0b, 0x36},
	{0x0c, 0x3e},
	{0x0d, 0x56},
	{0x0e, 0x5e},
	/*DNDD*/
	{0xfe, 0x02},
	{0x81, 0x05},
	/*{0x85, 0x21},*/
	/*{0x87, 0x8a},//bit 7 ,change mode*/
	/*dark sun*/
	{0xfe, 0x01},
	{0x54, 0x77},
	{0x58, 0x00},
	{0x5a, 0x05},
	/*MIPI*/
	{0xfe, 0x03},
	{0x01, 0x00},
	{0x02, 0x00},
	{0x03, 0x00},
	{0x06, 0x00},
	{0x10, 0x00},
	{0x15, 0x00},
	/*pad enable*/
	{0xfe, 0x00},
	{0xf2, 0x0f},

  	{SENSOR_REG_END, 0x00},

};

static struct regval_list sensor_init_regs_1920_1080_15fps_dvp[] = {
	/*SYS*/
	{0xf2, 0x00},
	{0xf3, 0x00},
	{0xf6, 0x00},
	{0xfc, 0x06},
	{0xf7, 0x03},
	{0xf8, 0x06},
	{0xf9, 0x06},
	{0xfa, 0x03},
	{0xfc, 0x0e},
	/*ANALOG & CISCTL*/
	{0xfe, 0x00},
	{0x03, 0x03},
	{0x04, 0xf6},
	{0x05, 0x02},
	{0x06, 0xc6},
	{0x86, 0x51},
	{0x8e, 0x0c},/*drop frame while change VB*/
	{0x07, 0x00},
	{0x08, 0x15},
	{0x09, 0x00},
	{0x0a, 0x00},
	{0x0b, 0x00},
	{0x0c, 0x00},
	{0x0d, 0x04},
	{0x0e, 0x40},
	{0x0f, 0x07},
	{0x10, 0x88},
	{0x17, 0x54},
	{0x18, 0x02},
	{0x19, 0x0d},
	{0x1a, 0x18},

	{0x20, 0x54},
	{0x23, 0xf0},
	{0x24, 0xc1},
	{0x25, 0x18},
	{0x26, 0x64},
	{0x28, 0xe8},
	{0x29, 0x08},
	{0x2a, 0x08},
	{0x2b, 0x48},
	{0x2f, 0x40},
	{0x30, 0x99},
	{0x34, 0x00},
	{0x38, 0x80},
	{0x3b, 0x12},
	{0x3d, 0xb0},
	{0xcc, 0x8a},
	{0xcd, 0x99},
	{0xcf, 0x70},
	{0xd0, 0x9c},
	{0xd2, 0xc1},
	{0xd8, 0x80},
	{0xda, 0x28},
	{0xdc, 0x24},
	{0xe1, 0x14},
	{0xe3, 0xf0},
	{0xe4, 0xfa},
	{0xe6, 0x1f},
	{0xe8, 0x02},
	{0xe9, 0x02},
	{0xea, 0x03},
	{0xeb, 0x03},
	/*ISP*/
	{0xfe, 0x00},
	{0x80, 0x5c},
	{0x88, 0x23},
	{0x89, 0x03},
	{0x90, 0x01},
	{0x92, 0x04},
	{0x94, 0x04},
	{0x95, 0x04},
	{0x96, 0x38},
	{0x97, 0x07},
	{0x98, 0x80},
	/*BLK*/
	{0xfe, 0x00},
	{0x40, 0x22},
	{0x43, 0x07},
	{0x4e, 0x3c},
	{0x4f, 0x00},
	{0x60, 0x00},
	{0x61, 0x80},
	/*GAIN*/
	{0xfe, 0x00},
	{0xb0, 0x58},
	{0xb1, 0x01},
	{0xb2, 0x00},
	{0xb6, 0x00},
	{0xfe, 0x01},
	{0x01, 0x00},
	{0x02, 0x01},
	{0x03, 0x02},
	{0x04, 0x03},
	{0x05, 0x04},
	{0x06, 0x05},
	{0x07, 0x06},
	{0x08, 0x0e},
	{0x09, 0x16},
	{0x0a, 0x1e},
	{0x0b, 0x36},
	{0x0c, 0x3e},
	{0x0d, 0x56},
	{0x0e, 0x5e},
	/*DNDD*/
	{0xfe, 0x02},
	{0x81, 0x05},
	{0x80, 0x4c},/*disable sensor DD*/
	/*dark sun*/
	{0xfe, 0x01},
	{0x54, 0x77},
	{0x58, 0x00},
	{0x5a, 0x05},
	/*MIPI*/
	{0xfe, 0x03},
	{0x01, 0x00},
	{0x02, 0x00},
	{0x03, 0x00},
	{0x06, 0x00},
	{0x10, 0x00},
	{0x15, 0x00},
	{0x36, 0x83},
	{0x8f, 0x64},
	/*pad enable*/
	{0xfe, 0x00},
	{0xf3, 0x01},
	{0xfa, 0x83},
	{0xf2, 0x0f},

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
		.fps = 25<<16|1,
		.mbus_code = V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_25fps_dvp,
	}
};

static enum v4l2_mbus_pixelcode sensor_mbus_code[] = {
	V4L2_MBUS_FMT_SRGGB10_1X10,
	V4L2_MBUS_FMT_SBGGR12_1X12,
};

/*
 * the part of driver was fixed.
 */

static struct regval_list sensor_stream_on[] = {
	{ 0xf2, 0x8f},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	{ 0xf2, 0x80},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, unsigned char reg,
		unsigned char *value)
{
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

static int sensor_write(struct tx_isp_subdev *sd, unsigned char reg,
			unsigned char value)
{
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

static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
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
static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
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

static int sensor_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = sensor_read(sd, 0xf0, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	ret = sensor_read(sd, 0xf1, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;
	return 0;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret = sensor_write(sd, 0x4, value&0xff);
	if (ret < 0) {
		printk("sensor_write error  %d\n" ,__LINE__);
		return ret;
	}
	ret = sensor_write(sd, 0x3, (value&0x1f00)>>8);
	if (ret < 0) {
		printk("sensor_write error  %d\n" ,__LINE__ );
		return ret;
	}
	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret = sensor_write(sd, 0xb6, (value >> 12) & 0xf);
	if (ret < 0) {
		printk("sensor_write error  %d" ,__LINE__ );
		return ret;
	}
	ret = sensor_write(sd, 0xb1, (value >> 8) & 0xf);
	if (ret < 0) {
		printk("sensor_write error  %d" ,__LINE__ );
		return ret;
	}
	ret = sensor_write(sd, 0xb2, (value << 2) & 0xff);
	if (ret < 0) {
		printk("sensor_write error  %d" ,__LINE__ );
		return ret;
	}
	return 0;
}

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
	int ret = 0;
	if (!enable)
		return ISP_SUCCESS;

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

static int sensor_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = sensor_write_array(sd, sensor_stream_on);
		pr_debug("%s stream on\n", SENSOR_NAME);
	}
	else {
		ret = sensor_write_array(sd, sensor_stream_off);
		pr_debug("%s stream off\n", SENSOR_NAME);
	}
	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int pclk = 0;
	unsigned short win_high=0;
	unsigned short vts = 0;
	unsigned short hb=0;
	unsigned short vb = 0;
	unsigned short hts=0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0; //the format is 24.8
	int ret = 0;

	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		pclk = SENSOR_SUPPORT_PCLK_FPS_30;
		max_fps = SENSOR_OUTPUT_MAX_FPS;
		break;
	case TX_SENSOR_MAX_FPS_15:
		pclk = SENSOR_SUPPORT_PCLK_FPS_15;
		max_fps = TX_SENSOR_MAX_FPS_15;
		break;
	default:
		printk("Now we do not support this framerate!!!\n");
	}

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		printk("warn: fps(%d) not in range\n", fps);
		return -1;
	}
	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		ret = sensor_read(sd, 0x05, &tmp);
		hb = tmp;
		ret += sensor_read(sd, 0x06, &tmp);
		if (ret < 0)
			return -1;
		hb = (hb << 8) + tmp;
		hts = hb*4;
		break;
	case TX_SENSOR_MAX_FPS_15:
		hts = sensor_attr.total_width;
		break;
	default:
		printk("Now we do not support this framerate!!!\n");
	}

	ret = sensor_read(sd, 0x0d, &tmp);
	win_high = tmp;
	ret += sensor_read(sd, 0x0e, &tmp);
	if (ret < 0)
		return -1;
	win_high = (win_high << 8) + tmp;
	vts = pclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	vb = vts - win_high - 16;
	ret = sensor_write(sd, 0x08, (unsigned char)(vb & 0xff));
	ret += sensor_write(sd, 0x07, (unsigned char)(vb >> 8));
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

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
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
		if (sensor->priv != wsize) {
			ret = sensor_write_array(sd, wsize->regs);
			if (!ret) {
				sensor->priv = wsize;
			}
		}
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}
	return ret;
}

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val = 0;

	ret = sensor_write(sd, 0xfe, 0x00);
	ret += sensor_read(sd, 0x17, &val);
	if (enable) {
		val = val | 0x02;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SGBRG10_1X10;
	} else {
		val = val & 0xfd;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SRGGB10_1X10;
	}
	sensor->video.mbus_change = 1;
	ret += sensor_write(sd, 0x17, val);

	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
		struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		} else {
			printk("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(150);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		} else {
			printk("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sensor_detect(sd, &ident);
	if (ret) {
		printk("chip found @ 0x%x (%s) is not an %s chip.\n",
		       client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}
	printk("%s chip found @ 0x%02x (%s)\n",
	       SENSOR_NAME, client->addr, client->adapter->name);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}
static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if (IS_ERR_OR_NULL(sd)) {
		printk("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			if (arg)
				ret = sensor_set_integration_time(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			if (arg)
				ret = sensor_set_analog_gain(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_DGAIN:
			if (arg)
				ret = sensor_set_digital_gain(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
			if (arg)
				ret = sensor_get_black_pedestal(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if (arg)
				ret = sensor_set_mode(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
			ret = sensor_write_array(sd, sensor_stream_off);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			ret = sensor_write_array(sd, sensor_stream_on);
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if (arg)
				ret = sensor_set_fps(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_VFLIP:
			if (arg)
				ret = sensor_set_vflip(sd, *(int*)arg);
			break;
		default:
			break;
	}
	return 0;
}

static int sensor_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
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

static int sensor_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
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
static u64 tx_isp_module_dma_mask = ~(u64)0;
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


static int sensor_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
	enum v4l2_mbus_pixelcode mbus;
	int i = 0;
	int ret;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		printk("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		printk("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	clk_set_rate(sensor->mclk, 24000000);
	clk_enable(sensor->mclk);

	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;

	sensor_attr.dvp.gpio = sensor_gpio_func;

	switch(sensor_gpio_func) {
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

	for(i = 0; i < ARRAY_SIZE(sensor_win_sizes); i++)
		sensor_win_sizes[i].mbus_code = mbus;

	 /*
		convert sensor-gain into isp-gain,
	 */
	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		break;
	case TX_SENSOR_MAX_FPS_15:
		sensor_attr.max_integration_time_native = 1121;
		sensor_attr.integration_time_limit = 1121;
		sensor_attr.total_width = 2485;
		sensor_attr.total_height = 1125;
		sensor_attr.max_integration_time = 1121;
		break;
	default:
		printk("Now we do not support this framerate!!!\n");
	}
	sensor_attr.max_again = 321573;/*log2_fixed_to_fixed(sensor_attr.max_again, TX_ISP_GAIN_FIXED_POINT, LOG2_GAIN_SHIFT);*/
	sensor_attr.max_dgain = sensor_attr.max_dgain;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sensor_attr;
	sensor->video.mbus_change = 1;
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

static int sensor_remove(struct i2c_client *client)
{
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
	{ SENSOR_NAME, 0 },
	{ }
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

static __init int init_sensor(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if (ret) {
		printk("Failed to init %s driver.\n", SENSOR_NAME);
		return -1;
	}
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
