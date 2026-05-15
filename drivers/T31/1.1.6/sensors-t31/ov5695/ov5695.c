/*
 * ov5695.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#define OV5695_CHIP_ID_H	(0x56)
#define OV5695_CHIP_ID_L	(0x95)
#define OV5695_REG_END		0xffff
#define OV5695_REG_DELAY	0xfffe
#define SENSOR_OUTPUT_MAX_FPS 60
#define SENSOR_OUTPUT_MIN_FPS 5
#define OV5695_SUPPORT_SCLK (45000000)
#define SENSOR_VERSION	"H20211109a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int sensor_resolution = TX_SENSOR_RES_200;
module_param(sensor_resolution, int, S_IRUGO);
MODULE_PARM_DESC(sensor_resolution, "Sensor Resolution");

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
	unsigned int value;
	unsigned int gain;
};

struct again_lut ov5695_again_lut[] = {
	{0x10, 0},
	{0x11, 5776},
	{0x12, 11136},
	{0x13, 16287},
	{0x14, 21097},
	{0x15, 25746},
	{0x16, 30109},
	{0x17, 34345},
	{0x18, 38336},
	{0x19, 42226},
	{0x1a, 45904},
	{0x1b, 49500},
	{0x1c, 52910},
	{0x1d, 56254},
	{0x1e, 59433},
	{0x1f, 62558},
	{0x20, 65536},
	{0x21, 71267},
	{0x22, 76672},
	{0x23, 81784},
	{0x24, 86633},
	{0x25, 91246},
	{0x26, 95645},
	{0x27, 99848},
	{0x28, 103872},
	{0x29, 107731},
	{0x2a, 111440},
	{0x2b, 115008},
	{0x2c, 118446},
	{0x2d, 121764},
	{0x2e, 124969},
	{0x2f, 128070},
	{0x30, 131072},
	{0x31, 136803},
	{0x32, 142208},
	{0x33, 147320},
	{0x34, 152169},
	{0x35, 156782},
	{0x36, 161181},
	{0x37, 165384},
	{0x38, 169408},
	{0x39, 173267},
	{0x3a, 176976},
	{0x3b, 180544},
	{0x3c, 183982},
	{0x3d, 187300},
	{0x3e, 190505},
	{0x3f, 193606},
	{0x40, 196608},
	{0x41, 202339},
	{0x42, 207744},
	{0x43, 212856},
	{0x44, 217705},
	{0x45, 222318},
	{0x46, 226717},
	{0x47, 230920},
	{0x48, 234944},
	{0x49, 238803},
	{0x4a, 242512},
	{0x4b, 246080},
	{0x4c, 249518},
	{0x4d, 252836},
	{0x4e, 256041},
	{0x4f, 259142},
};

struct tx_isp_sensor_attribute ov5695_attr;

unsigned int ov5695_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = ov5695_again_lut;
	while(lut->gain <= ov5695_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
		}
                else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
                else {
			if((lut->gain == ov5695_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int ov5695_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute ov5695_attr={
	.name = "ov5695",
	.chip_id = 0x5695,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x36,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 450,
		.lans = 2,
		.settle_time_apative_en = 1,
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
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 259142,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 0x45c - 4,
	.integration_time_limit = 0x45c - 4,
	.total_width = 0x2a0,
	.total_height = 0x45c,
	.max_integration_time = 0x45c - 4,
	.one_line_expr_in_us = 14,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = ov5695_alloc_again,
	.sensor_ctrl.alloc_dgain = ov5695_alloc_dgain,
};

static struct regval_list ov5695_init_regs_1920_1080_60fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x0300,0x04},
	{0x0301,0x00},
	{0x0302,0x69},
	{0x0303,0x00},
	{0x0304,0x00},
	{0x0305,0x01},
	{0x0307,0x00},
	{0x030b,0x00},
	{0x030c,0x00},
	{0x030d,0x1e},
	{0x030e,0x04},
	{0x030f,0x03},
	{0x0312,0x01},
	{0x3000,0x00},
	{0x3002,0x21},
	{0x3016,0x32},
	{0x3022,0x51},
	{0x3106,0x15},
	{0x3107,0x01},
	{0x3108,0x05},
	{0x3500,0x00},
	{0x3501,0x45},
	{0x3502,0x00},
	{0x3503,0x08},
	{0x3504,0x03},
	{0x3505,0x8c},
	{0x3507,0x03},
	{0x3508,0x00},
	{0x3509,0x10},
	{0x350c,0x00},
	{0x350d,0x80},
	{0x3510,0x00},
	{0x3511,0x02},
	{0x3512,0x00},
	{0x3601,0x55},
	{0x3602,0x58},
	{0x3611,0x58},
	{0x3614,0x30},
	{0x3615,0x77},
	{0x3621,0x08},
	{0x3624,0x40},
	{0x3633,0x0c},
	{0x3634,0x0c},
	{0x3635,0x0c},
	{0x3636,0x0c},
	{0x3638,0x00},
	{0x3639,0x00},
	{0x363a,0x00},
	{0x363b,0x00},
	{0x363c,0xff},
	{0x363d,0xfa},
	{0x3650,0x44},
	{0x3651,0x44},
	{0x3652,0x44},
	{0x3653,0x44},
	{0x3654,0x44},
	{0x3655,0x44},
	{0x3656,0x44},
	{0x3657,0x44},
	{0x3660,0x00},
	{0x3661,0x00},
	{0x3662,0x00},
	{0x366a,0x00},
	{0x366e,0x18},
	{0x3673,0x04},
	{0x3700,0x14},
	{0x3703,0x0c},
	{0x3706,0x24},
	{0x3714,0x23},
	{0x3715,0x01},
	{0x3716,0x00},
	{0x3717,0x02},
	{0x3733,0x10},
	{0x3734,0x40},
	{0x373f,0xa0},
	{0x3765,0x20},
	{0x37a1,0x1d},
	{0x37a8,0x26},
	{0x37ab,0x14},
	{0x37c2,0x04},
	{0x37c3,0xf0},
	{0x37cb,0x09},
	{0x37cc,0x13},
	{0x37cd,0x1f},
	{0x37ce,0x1f},
	{0x3800,0x01},
	{0x3801,0x50},
	{0x3802,0x01},
	{0x3803,0xb8},
	{0x3804,0x08},
	{0x3805,0xef},
	{0x3806,0x05},
	{0x3807,0xf7},
	{0x3808,0x07},/*1920*/
	{0x3809,0x80},//
	{0x380a,0x04},/*1080*/
	{0x380b,0x38},//
	{0x380c,0x02},/* hts */
	{0x380d,0xa0},//
	{0x380e,0x04},/* vts */
	{0x380f,0x5c},//
	{0x3810,0x00},
	{0x3811,0x10},
	{0x3812,0x00},
	{0x3813,0x04},
	{0x3814,0x01},
	{0x3815,0x01},
	{0x3816,0x01},
	{0x3817,0x01},
	{0x3818,0x00},
	{0x3819,0x00},
	{0x381a,0x00},
	{0x381b,0x01},
	{0x3820,0x88},
	{0x3821,0x00},
	{0x3c80,0x08},
	{0x3c82,0x00},
	{0x3c83,0x00},
	{0x3c88,0x00},
	{0x3d85,0x14},
	{0x3f02,0x08},
	{0x3f03,0x10},
	{0x4008,0x04},
	{0x4009,0x13},
	{0x404e,0x20},
	{0x4501,0x00},
	{0x4502,0x10},
	{0x4800,0x00},
	{0x481f,0x2a},
	{0x4837,0x13},
	{0x5000,0x13},
	{0x5780,0x3e},
	{0x5781,0x0f},
	{0x5782,0x44},
	{0x5783,0x02},
	{0x5784,0x01},
	{0x5785,0x01},
	{0x5786,0x00},
	{0x5787,0x04},
	{0x5788,0x02},
	{0x5789,0x0f},
	{0x578a,0xfd},
	{0x578b,0xf5},
	{0x578c,0xf5},
	{0x578d,0x03},
	{0x578e,0x08},
	{0x578f,0x0c},
	{0x5790,0x08},
	{0x5791,0x06},
	{0x5792,0x00},
	{0x5793,0x52},
	{0x5794,0xa3},
	{0x5b00,0x00},
	{0x5b01,0x1c},
	{0x5b02,0x00},
	{0x5b03,0x7f},
	{0x5b05,0x6c},
	{0x5e10,0xfc},
	{0x4010,0xf1},
	{0x3503,0x08},
	{0x3505,0x8c},
	{0x3507,0x03},
	{0x3508,0x00},
	{0x3509,0xf8},
	{0x0100,0x01},

	{OV5695_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5695_init_regs_1280_720_60fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x0300,0x04},
	{0x0301,0x00},
	{0x0302,0x69},
	{0x0303,0x00},
	{0x0304,0x00},
	{0x0305,0x01},
	{0x0307,0x00},
	{0x030b,0x00},
	{0x030c,0x00},
	{0x030d,0x1e},
	{0x030e,0x04},
	{0x030f,0x03},
	{0x0312,0x01},
	{0x3000,0x00},
	{0x3002,0x21},
	{0x3016,0x32},
	{0x3022,0x51},
	{0x3106,0x15},
	{0x3107,0x01},
	{0x3108,0x05},
	{0x3500,0x00},
	{0x3501,0x45},
	{0x3502,0x00},
	{0x3503,0x08},
	{0x3504,0x03},
	{0x3505,0x8c},
	{0x3507,0x03},
	{0x3508,0x00},
	{0x3509,0x10},
	{0x350c,0x00},
	{0x350d,0x80},
	{0x3510,0x00},
	{0x3511,0x02},
	{0x3512,0x00},
	{0x3601,0x55},
	{0x3602,0x58},
	{0x3611,0x58},
	{0x3614,0x30},
	{0x3615,0x77},
	{0x3621,0x08},
	{0x3624,0x40},
	{0x3633,0x0c},
	{0x3634,0x0c},
	{0x3635,0x0c},
	{0x3636,0x0c},
	{0x3638,0x00},
	{0x3639,0x00},
	{0x363a,0x00},
	{0x363b,0x00},
	{0x363c,0xff},
	{0x363d,0xfa},
	{0x3650,0x44},
	{0x3651,0x44},
	{0x3652,0x44},
	{0x3653,0x44},
	{0x3654,0x44},
	{0x3655,0x44},
	{0x3656,0x44},
	{0x3657,0x44},
	{0x3660,0x00},
	{0x3661,0x00},
	{0x3662,0x00},
	{0x366a,0x00},
	{0x366e,0x18},
	{0x3673,0x04},
	{0x3700,0x14},
	{0x3703,0x0c},
	{0x3706,0x24},
	{0x3714,0x23},
	{0x3715,0x01},
	{0x3716,0x00},
	{0x3717,0x02},
	{0x3733,0x10},
	{0x3734,0x40},
	{0x373f,0xa0},
	{0x3765,0x20},
	{0x37a1,0x1d},
	{0x37a8,0x26},
	{0x37ab,0x14},
	{0x37c2,0x04},
	{0x37c3,0xf0},
	{0x37cb,0x09},
	{0x37cc,0x13},
	{0x37cd,0x1f},
	{0x37ce,0x1f},
	{0x3800,0x01},
	{0x3801,0x50},
	{0x3802,0x01},
	{0x3803,0xb8},
	{0x3804,0x08},
	{0x3805,0xef},
	{0x3806,0x05},
	{0x3807,0xf7},
	{0x3808,0x05},/* 1280 */
	{0x3809,0x00},//
	{0x380a,0x02},/* 720 */
	{0x380b,0xd0},//
	{0x380c,0x02},/* hts */
	{0x380d,0xa0},//
	{0x380e,0x04},/* vts */
	{0x380f,0x5c},//
	{0x3810,0x01},
	{0x3811,0x50},
	{0x3812,0x00},
	{0x3813,0xb8},
	{0x3814,0x01},
	{0x3815,0x01},
	{0x3816,0x01},
	{0x3817,0x01},
	{0x3818,0x00},
	{0x3819,0x00},
	{0x381a,0x00},
	{0x381b,0x01},
	{0x3820,0x88},
	{0x3821,0x00},
	{0x3c80,0x08},
	{0x3c82,0x00},
	{0x3c83,0x00},
	{0x3c88,0x00},
	{0x3d85,0x14},
	{0x3f02,0x08},
	{0x3f03,0x10},
	{0x4008,0x04},
	{0x4009,0x13},
	{0x404e,0x20},
	{0x4501,0x00},
	{0x4502,0x10},
	{0x4800,0x00},
	{0x481f,0x2a},
	{0x4837,0x13},
	{0x5000,0x13},
	{0x5780,0x3e},
	{0x5781,0x0f},
	{0x5782,0x44},
	{0x5783,0x02},
	{0x5784,0x01},
	{0x5785,0x01},
	{0x5786,0x00},
	{0x5787,0x04},
	{0x5788,0x02},
	{0x5789,0x0f},
	{0x578a,0xfd},
	{0x578b,0xf5},
	{0x578c,0xf5},
	{0x578d,0x03},
	{0x578e,0x08},
	{0x578f,0x0c},
	{0x5790,0x08},
	{0x5791,0x06},
	{0x5792,0x00},
	{0x5793,0x52},
	{0x5794,0xa3},
	{0x5b00,0x00},
	{0x5b01,0x1c},
	{0x5b02,0x00},
	{0x5b03,0x7f},
	{0x5b05,0x6c},
	{0x5e10,0xfc},
	{0x4010,0xf1},
	{0x3503,0x08},
	{0x3505,0x8c},
	{0x3507,0x03},
	{0x3508,0x00},
	{0x3509,0xf8},
	{0x0100,0x01},

	{OV5695_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list ov5695_init_regs_640_480_60fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x0300,0x04},
	{0x0301,0x00},
	{0x0302,0x69},
	{0x0303,0x00},
	{0x0304,0x00},
	{0x0305,0x01},
	{0x0307,0x00},
	{0x030b,0x00},
	{0x030c,0x00},
	{0x030d,0x1e},
	{0x030e,0x04},
	{0x030f,0x03},
	{0x0312,0x01},
	{0x3000,0x00},
	{0x3002,0x21},
	{0x3016,0x32},
	{0x3022,0x51},
	{0x3106,0x15},
	{0x3107,0x01},
	{0x3108,0x05},
	{0x3500,0x00},
	{0x3501,0x45},
	{0x3502,0x00},
	{0x3503,0x08},
	{0x3504,0x03},
	{0x3505,0x8c},
	{0x3507,0x03},
	{0x3508,0x00},
	{0x3509,0x10},
	{0x350c,0x00},
	{0x350d,0x80},
	{0x3510,0x00},
	{0x3511,0x02},
	{0x3512,0x00},
	{0x3601,0x55},
	{0x3602,0x58},
	{0x3611,0x58},
	{0x3614,0x30},
	{0x3615,0x77},
	{0x3621,0x08},
	{0x3624,0x40},
	{0x3633,0x0c},
	{0x3634,0x0c},
	{0x3635,0x0c},
	{0x3636,0x0c},
	{0x3638,0x00},
	{0x3639,0x00},
	{0x363a,0x00},
	{0x363b,0x00},
	{0x363c,0xff},
	{0x363d,0xfa},
	{0x3650,0x44},
	{0x3651,0x44},
	{0x3652,0x44},
	{0x3653,0x44},
	{0x3654,0x44},
	{0x3655,0x44},
	{0x3656,0x44},
	{0x3657,0x44},
	{0x3660,0x00},
	{0x3661,0x00},
	{0x3662,0x00},
	{0x366a,0x00},
	{0x366e,0x18},
	{0x3673,0x04},
	{0x3700,0x14},
	{0x3703,0x0c},
	{0x3706,0x24},
	{0x3714,0x23},
	{0x3715,0x01},
	{0x3716,0x00},
	{0x3717,0x02},
	{0x3733,0x10},
	{0x3734,0x40},
	{0x373f,0xa0},
	{0x3765,0x20},
	{0x37a1,0x1d},
	{0x37a8,0x26},
	{0x37ab,0x14},
	{0x37c2,0x04},
	{0x37c3,0xf0},
	{0x37cb,0x09},
	{0x37cc,0x13},
	{0x37cd,0x1f},
	{0x37ce,0x1f},
	{0x3800,0x01},
	{0x3801,0x50},
	{0x3802,0x01},
	{0x3803,0xb8},
	{0x3804,0x08},
	{0x3805,0xef},
	{0x3806,0x05},
	{0x3807,0xf7},
	{0x3808,0x02},/*640*/
	{0x3809,0x80},//
	{0x380a,0x01},/*480*/
	{0x380b,0xe0},//
	{0x380c,0x02},/* hts */
	{0x380d,0xa0},//
	{0x380e,0x04},/* vts */
	{0x380f,0x5c},//
	{0x3810,0x02},
	{0x3811,0x90},
	{0x3812,0x01},
	{0x3813,0x30},
	{0x3814,0x01},
	{0x3815,0x01},
	{0x3816,0x01},
	{0x3817,0x01},
	{0x3818,0x00},
	{0x3819,0x00},
	{0x381a,0x00},
	{0x381b,0x01},
	{0x3820,0x88},
	{0x3821,0x00},
	{0x3c80,0x08},
	{0x3c82,0x00},
	{0x3c83,0x00},
	{0x3c88,0x00},
	{0x3d85,0x14},
	{0x3f02,0x08},
	{0x3f03,0x10},
	{0x4008,0x04},
	{0x4009,0x13},
	{0x404e,0x20},
	{0x4501,0x00},
	{0x4502,0x10},
	{0x4800,0x00},
	{0x481f,0x2a},
	{0x4837,0x13},
	{0x5000,0x13},
	{0x5780,0x3e},
	{0x5781,0x0f},
	{0x5782,0x44},
	{0x5783,0x02},
	{0x5784,0x01},
	{0x5785,0x01},
	{0x5786,0x00},
	{0x5787,0x04},
	{0x5788,0x02},
	{0x5789,0x0f},
	{0x578a,0xfd},
	{0x578b,0xf5},
	{0x578c,0xf5},
	{0x578d,0x03},
	{0x578e,0x08},
	{0x578f,0x0c},
	{0x5790,0x08},
	{0x5791,0x06},
	{0x5792,0x00},
	{0x5793,0x52},
	{0x5794,0xa3},
	{0x5b00,0x00},
	{0x5b01,0x1c},
	{0x5b02,0x00},
	{0x5b03,0x7f},
	{0x5b05,0x6c},
	{0x5e10,0xfc},
	{0x4010,0xf1},
	{0x3503,0x08},
	{0x3505,0x8c},
	{0x3507,0x03},
	{0x3508,0x00},
	{0x3509,0xf8},
	{0x0100,0x01},

	{OV5695_REG_END, 0x00},/* END MARKER */
};

static struct regval_list ov5695_init_regs_2592_1944_30fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x0300,0x04},
	{0x0301,0x00},
	{0x0302,0x69},
	{0x0303,0x00},
	{0x0304,0x00},
	{0x0305,0x01},
	{0x0307,0x00},
	{0x030b,0x00},
	{0x030c,0x00},
	{0x030d,0x1e},
	{0x030e,0x04},
	{0x030f,0x03},
	{0x0312,0x01},
	{0x3000,0x00},
	{0x3002,0x21},
	{0x3016,0x32},
	{0x3022,0x51},
	{0x3106,0x15},
	{0x3107,0x01},
	{0x3108,0x05},
	{0x3500,0x00},
	{0x3501,0x7e},
	{0x3502,0x00},
	{0x3503,0x08},
	{0x3504,0x03},
	{0x3505,0x8c},
	{0x3507,0x03},
	{0x3508,0x00},
	{0x3509,0x10},
	{0x350c,0x00},
	{0x350d,0x80},
	{0x3510,0x00},
	{0x3511,0x02},
	{0x3512,0x00},
	{0x3601,0x55},
	{0x3602,0x58},
	{0x3611,0x58},
	{0x3614,0x30},
	{0x3615,0x77},
	{0x3621,0x08},
	{0x3624,0x40},
	{0x3633,0x0c},
	{0x3634,0x0c},
	{0x3635,0x0c},
	{0x3636,0x0c},
	{0x3638,0x00},
	{0x3639,0x00},
	{0x363a,0x00},
	{0x363b,0x00},
	{0x363c,0xff},
	{0x363d,0xfa},
	{0x3650,0x44},
	{0x3651,0x44},
	{0x3652,0x44},
	{0x3653,0x44},
	{0x3654,0x44},
	{0x3655,0x44},
	{0x3656,0x44},
	{0x3657,0x44},
	{0x3660,0x00},
	{0x3661,0x00},
	{0x3662,0x00},
	{0x366a,0x00},
	{0x366e,0x18},
	{0x3673,0x04},
	{0x3700,0x14},
	{0x3703,0x0c},
	{0x3706,0x24},
	{0x3714,0x23},
	{0x3715,0x01},
	{0x3716,0x00},
	{0x3717,0x02},
	{0x3733,0x10},
	{0x3734,0x40},
	{0x373f,0xa0},
	{0x3765,0x20},
	{0x37a1,0x1d},
	{0x37a8,0x26},
	{0x37ab,0x14},
	{0x37c2,0x04},
	{0x37c3,0xf0},
	{0x37cb,0x09},
	{0x37cc,0x13},
	{0x37cd,0x1f},
	{0x37ce,0x1f},
	{0x3800,0x00},
	{0x3801,0x00},
	{0x3802,0x00},
	{0x3803,0x04},
	{0x3804,0x0a},
	{0x3805,0x3f},
	{0x3806,0x07},
	{0x3807,0xab},
	{0x3808,0x0a},/*2592*/
	{0x3809,0x20},//
	{0x380a,0x07},/*1944*/
	{0x380b,0x98},//
	{0x380c,0x02},/*hts*/
	{0x380d,0xe4},//
	{0x380e,0x07},/*vts*/
	{0x380f,0xe8},//
	{0x3810,0x00},
	{0x3811,0x10},
	{0x3812,0x00},
	{0x3813,0x08},
	{0x3814,0x01},
	{0x3815,0x01},
	{0x3816,0x01},
	{0x3817,0x01},
	{0x3818,0x00},
	{0x3819,0x00},
	{0x381a,0x00},
	{0x381b,0x01},
	{0x3820,0x88},
	{0x3821,0x00},
	{0x3c80,0x08},
	{0x3c82,0x00},
	{0x3c83,0x00},
	{0x3c88,0x00},
	{0x3d85,0x14},
	{0x3f02,0x08},
	{0x3f03,0x10},
	{0x4008,0x04},
	{0x4009,0x13},
	{0x404e,0x20},
	{0x4501,0x00},
	{0x4502,0x10},
	{0x4800,0x00},
	{0x481f,0x2a},
	{0x4837,0x13},
	{0x5000,0x13},
	{0x5780,0x3e},
	{0x5781,0x0f},
	{0x5782,0x44},
	{0x5783,0x02},
	{0x5784,0x01},
	{0x5785,0x01},
	{0x5786,0x00},
	{0x5787,0x04},
	{0x5788,0x02},
	{0x5789,0x0f},
	{0x578a,0xfd},
	{0x578b,0xf5},
	{0x578c,0xf5},
	{0x578d,0x03},
	{0x578e,0x08},
	{0x578f,0x0c},
	{0x5790,0x08},
	{0x5791,0x06},
	{0x5792,0x00},
	{0x5793,0x52},
	{0x5794,0xa3},
	{0x5b00,0x00},
	{0x5b01,0x1c},
	{0x5b02,0x00},
	{0x5b03,0x7f},
	{0x5b05,0x6c},
	{0x5e10,0xfc},
	{0x4010,0xf1},
	{0x3503,0x08},
	{0x3505,0x8c},
	{0x3507,0x03},
	{0x3508,0x00},
	{0x3509,0xf8},
	{0x0100,0x01},

	{OV5695_REG_END, 0x00},/* END MARKER */
};


static struct tx_isp_sensor_win_setting ov5695_win_sizes[] = {
	/* [0] 1920*1080 @60fps */
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 60 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ov5695_init_regs_1920_1080_60fps_mipi,
	},
	/* [1] 1280*720 @60fps */
	{
		.width      = 1280,
        .height     = 720,
        .fps        = 60 << 16 | 1,
        .mbus_code  = V4L2_MBUS_FMT_SBGGR10_1X10,
        .colorspace = V4L2_COLORSPACE_SRGB,
        .regs       = ov5695_init_regs_1280_720_60fps_mipi,
	},
	/* [2] 640*480 @60fps */
    {
        .width      = 640,
        .height     = 480,
        .fps        = 60 << 16 | 1,
        .mbus_code  = V4L2_MBUS_FMT_SBGGR10_1X10,
        .colorspace = V4L2_COLORSPACE_SRGB,
        .regs       = ov5695_init_regs_640_480_60fps_mipi,
    },
	/* [3] 2592*1944 @30fps */
	{
        .width      = 2592,
        .height     = 1944,
        .fps        = 30 << 16 | 1,
        .mbus_code  = V4L2_MBUS_FMT_SBGGR10_1X10,
        .colorspace = V4L2_COLORSPACE_SRGB,
        .regs       = ov5695_init_regs_2592_1944_30fps_mipi,
	}
};
struct tx_isp_sensor_win_setting *wsize = &ov5695_win_sizes[0];

static struct regval_list ov5695_stream_on_mipi[] = {
	{OV5695_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5695_stream_off_mipi[] = {
	{OV5695_REG_END, 0x00},	/* END MARKER */
};

int ov5695_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
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

int ov5695_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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


static int ov5695_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != OV5695_REG_END) {
		if (vals->reg_num == OV5695_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = ov5695_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int ov5695_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != OV5695_REG_END) {
		if (vals->reg_num == OV5695_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = ov5695_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int ov5695_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int ov5695_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = ov5695_read(sd, 0x300b, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OV5695_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = ov5695_read(sd, 0x300c, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OV5695_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int ov5695_set_expo(struct tx_isp_subdev *sd, int value)
{
    int ret = 0;
    int it = (value & 0xffff) << 4;
    int again = (value & 0xffff0000) >> 16;

    /*set integration time*/
	ret = ov5695_write(sd, 0x3502, (unsigned char)(it & 0xff));
	ret += ov5695_write(sd, 0x3501, (unsigned char)((it >> 8) & 0xff));
	ret += ov5695_write(sd, 0x3500, (unsigned char)((it >> 16) & 0xf));
    /*set analog gain*/
    ret += ov5695_write(sd, 0x3509, (unsigned char)(again & 0xff));
    ret += ov5695_write(sd, 0x3508, (unsigned char)(((again >> 8) & 0xff)));

	if (ret < 0)
	    return ret;

	return 0;
}

static int ov5695_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int it = value << 4;

	ret = ov5695_write(sd, 0x3502, (unsigned char)(it & 0xff));
	ret += ov5695_write(sd, 0x3501, (unsigned char)((it >> 8) & 0xff));
	ret += ov5695_write(sd, 0x3500, (unsigned char)((it >> 16) & 0xf));
	if (ret < 0)
		return ret;
	return 0;
}

static int ov5695_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += ov5695_write(sd, 0x3509, (unsigned char)(value & 0xff));
	ret += ov5695_write(sd, 0x3508, (unsigned char)(((value >> 8) & 0xff)));
	if (ret < 0)
		return ret;

	return 0;
}

static int ov5695_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int ov5695_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int ov5695_init(struct tx_isp_subdev *sd, int enable)
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

	ret = ov5695_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int ov5695_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = ov5695_write_array(sd, ov5695_stream_on_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("ov5695 stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = ov5695_write_array(sd, ov5695_stream_off_mipi);
		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("ov5695 stream off\n");
	}

	return ret;
}

static int ov5695_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;
	unsigned int newformat = 0; //the format is 24.8

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || fps < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		return -1;
	}

	sclk = OV5695_SUPPORT_SCLK;

	ret += ov5695_read(sd, 0x380c, &tmp);
	hts = tmp;
	ret += ov5695_read(sd, 0x380d, &tmp);
	if (0 != ret) {
		printk("err: ov5695 read err\n");
		return ret;
	}

	hts = ((hts << 8) + tmp);

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += ov5695_write(sd, 0x380f, vts & 0xff);
	ret += ov5695_write(sd, 0x380e, (vts >> 8) & 0xff);
	if (0 != ret) {
		printk("err: ov5695_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int ov5695_set_mode(struct tx_isp_subdev *sd, int value)
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

static int ov5695_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"ov5695_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"ov5695_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = ov5695_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an ov5695 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("ov5695 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "ov5695", sizeof("ov5695"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int ov5695_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	return 0;
}

static int ov5695_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
//	return 0;
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
//		if(arg)
//	     	ret = ov5695_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = ov5695_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = ov5695_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = ov5695_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = ov5695_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = ov5695_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = ov5695_write_array(sd, ov5695_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = ov5695_write_array(sd, ov5695_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = ov5695_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = ov5695_set_vflip(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int ov5695_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = ov5695_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int ov5695_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	ov5695_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops ov5695_core_ops = {
	.g_chip_ident = ov5695_g_chip_ident,
	.reset = ov5695_reset,
	.init = ov5695_init,
	/*.ioctl = ov5695_ops_ioctl,*/
	.g_register = ov5695_g_register,
	.s_register = ov5695_s_register,
};

static struct tx_isp_subdev_video_ops ov5695_video_ops = {
	.s_stream = ov5695_s_stream,
};

static struct tx_isp_subdev_sensor_ops	ov5695_sensor_ops = {
	.ioctl	= ov5695_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops ov5695_ops = {
	.core = &ov5695_core_ops,
	.video = &ov5695_video_ops,
	.sensor = &ov5695_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "ov5695",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int ov5695_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	switch(sensor_resolution){
	case TX_SENSOR_RES_200:
		wsize = &ov5695_win_sizes[0];
		printk("-----> 1920*1080 <------\n");
		break;
	case TX_SENSOR_RES_100:
		wsize = &ov5695_win_sizes[1];
		ov5695_attr.mipi.image_twidth = 1280;
		ov5695_attr.mipi.image_theight = 720;
		printk("-------> 1280*720 <------\n");
		break;
	case TX_SENSOR_RES_30:
		wsize = &ov5695_win_sizes[2];
		ov5695_attr.mipi.image_twidth = 640;
		ov5695_attr.mipi.image_theight = 480;
		printk("-------> 640*480 <------\n");
		break;
	case TX_SENSOR_RES_500:
		wsize = &ov5695_win_sizes[3];
		ov5695_attr.mipi.image_twidth = 2592;
		ov5695_attr.mipi.image_theight = 1944;
		ov5695_attr.max_integration_time_native = 0x7e8 - 4;
		ov5695_attr.integration_time_limit = 0x7e8 - 4;
		ov5695_attr.total_width = 0x2e4;
		ov5695_attr.total_height = 0x7e8;
		ov5695_attr.max_integration_time = 0x7e8 - 4;
		ov5695_attr.one_line_expr_in_us = 30;
		printk("-------> 2592*1944 <-------\n");
		break;
	default:
	    ISP_ERROR("Now we do not support this framerate!!!\n");

	}

	/*
	  convert sensor-gain into isp-gain,
	*/
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	ov5695_attr.expo_fs = 1;
	sensor->video.attr = &ov5695_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &ov5695_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	ISP_WARNING("probe ok ------->ov5695\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int ov5695_remove(struct i2c_client *client)
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

static const struct i2c_device_id ov5695_id[] = {
	{ "ov5695", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov5695_id);

static struct i2c_driver ov5695_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov5695",
	},
	.probe		= ov5695_probe,
	.remove		= ov5695_remove,
	.id_table	= ov5695_id,
};

static __init int init_ov5695(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init ov5695 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&ov5695_driver);
}

static __exit void exit_ov5695(void)
{
	private_i2c_del_driver(&ov5695_driver);
}

module_init(init_ov5695);
module_exit(exit_ov5695);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov5695 sensors");
MODULE_LICENSE("GPL");
