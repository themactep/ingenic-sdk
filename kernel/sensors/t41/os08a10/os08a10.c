/*
 * os08a10.c
 *
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          3840*2160       30        mipi_2lane           linear
 *   1          3840*2160       20        mipi_2lane           linear
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

#define OS08A10_CHIP_ID_H	(0x53)
#define OS08A10_CHIP_ID_M	(0x08)
#define OS08A10_CHIP_ID_L	(0x41)
#define OS08A10_REG_END		0xffff
#define OS08A10_REG_DELAY	0xfffe
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20220812a"

static int reset_gpio = -1;
static int pwdn_gpio = -1;

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

struct again_lut os08a10_again_lut[] = {
	{0x80, 0},
	{0x88, 5731},
	{0x90, 11135},
	{0x98, 16247},
	{0xa0, 21097},
	{0xa8, 25710},
	{0xb0, 30108},
	{0xb8, 34311},
	{0xc0, 38335},
	{0xc8, 42195},
	{0xd0, 45903},
	{0xd8, 49472},
	{0xe0, 52910},
	{0xe8, 56228},
	{0xf0, 59433},
	{0xf8, 62534},
	{0x100, 65536},
	{0x110, 71267},
	{0x120, 76671},
	{0x130, 81783},
	{0x140, 86633},
	{0x150, 91246},
	{0x160, 95644},
	{0x170, 99847},
	{0x180, 103871},
	{0x190, 107731},
	{0x1a0, 111439},
	{0x1b0, 115008},
	{0x1c0, 118446},
	{0x1d0, 121764},
	{0x1e0, 124969},
	{0x1f0, 128070},
	{0x200, 131072},
	{0x220, 136803},
	{0x240, 142207},
	{0x260, 147319},
	{0x280, 152169},
	{0x2a0, 156782},
	{0x2c0, 161180},
	{0x2e0, 165383},
	{0x300, 169407},
	{0x320, 173267},
	{0x340, 176975},
	{0x360, 180544},
	{0x380, 183982},
	{0x3a0, 187300},
	{0x3c0, 190505},
	{0x3e0, 193606},
	{0x400, 196608},
	{0x440, 202339},
	{0x480, 207743},
	{0x4c0, 212855},
	{0x500, 217705},
	{0x540, 222318},
	{0x580, 226716},
	{0x5c0, 230919},
	{0x600, 234943},
	{0x640, 238803},
	{0x680, 242511},
	{0x6c0, 246080},
	{0x700, 249518},
	{0x740, 252836},
	{0x780, 256041},
	{0x7c0, 259142},
	{0x7ff, 262080},
};

struct tx_isp_sensor_attribute os08a10_attr;

unsigned int os08a10_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = os08a10_again_lut;
	while(lut->gain <= os08a10_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == os08a10_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int os08a10_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus os08a10_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1439,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.image_twidth = 3840,
	.image_theight = 2160,
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

struct tx_isp_sensor_attribute os08a10_attr={
	.name = "os08a10",
	.chip_id = 0x530841,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x36,
	.max_again = 262080,
	.max_dgain = 0,
	.min_integration_time = 8,
	.min_integration_time_native = 8,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = os08a10_alloc_again,
	.sensor_ctrl.alloc_dgain = os08a10_alloc_dgain,
};

static struct regval_list os08a10_init_regs_3840_2160_30fps_mipi[] = {
	{0x0100,0x00},
	{0x0103,0x01},
	{0x0303,0x01},
	{0x0305,0x5a},
	{0x0306,0x00},
	{0x0308,0x03},
	{0x0309,0x04},
	{0x032a,0x00},
	{0x300f,0x11},
	{0x3010,0x01},
	{0x3011,0x04},
	{0x3012,0x21},
	{0x3016,0xf0},
	{0x301e,0x98},
	{0x3031,0xa9},
	{0x3103,0x92},
	{0x3104,0x01},
	{0x3106,0x10},
	{0x3400,0x04},
	{0x3025,0x03},
	{0x3425,0x01},
	{0x3428,0x01},
	{0x3406,0x08},
	{0x3408,0x03},
	{0x340c,0xff},
	{0x340d,0xff},
	{0x031e,0x09},
	{0x3501,0x08},
	{0x3502,0xe5},
	{0x3505,0x83},
	{0x3508,0x00},
	{0x3509,0x80},
	{0x350a,0x04},
	{0x350b,0x00},
	{0x350c,0x00},
	{0x350d,0x80},
	{0x350e,0x04},
	{0x350f,0x00},
	{0x3600,0x00},
	{0x3603,0x2c},
	{0x3605,0x50},
	{0x3609,0xb5},
	{0x3610,0x39},
	{0x360c,0x01},
	{0x3628,0xa4},
	{0x362d,0x10},
	{0x3660,0x43},
	{0x3661,0x06},
	{0x3662,0x00},
	{0x3663,0x28},
	{0x3664,0x0d},
	{0x366a,0x38},
	{0x366b,0xa0},
	{0x366d,0x00},
	{0x366e,0x00},
	{0x3680,0x00},
	{0x36c0,0x00},
	{0x3701,0x02},
	{0x373b,0x02},
	{0x373c,0x02},
	{0x3736,0x02},
	{0x3737,0x02},
	{0x3705,0x00},
	{0x3706,0x39},
	{0x370a,0x00},
	{0x370b,0x98},
	{0x3709,0x49},
	{0x3714,0x21},
	{0x371c,0x00},
	{0x371d,0x08},
	{0x3740,0x1b},
	{0x3741,0x04},
	{0x375e,0x0b},
	{0x3760,0x10},
	{0x3776,0x10},
	{0x3781,0x02},
	{0x3782,0x04},
	{0x3783,0x02},
	{0x3784,0x08},
	{0x3785,0x08},
	{0x3788,0x01},
	{0x3789,0x01},
	{0x3797,0x04},
	{0x3762,0x11},
	{0x3800,0x00},
	{0x3801,0x00},
	{0x3802,0x00},
	{0x3803,0x0c},
	{0x3804,0x0e},
	{0x3805,0xff},
	{0x3806,0x08},
	{0x3807,0x6f},
	{0x3808,0x0f},
	{0x3809,0x00},
	{0x380a,0x08},
	{0x380b,0x70},
	{0x380c,0x08},//
	{0x380d,0x18},/*hts = 0x818 = 2072*/
	{0x380e,0x09},//
	{0x380f,0x0a},/*vts = 90a = 2314*/
	{0x3813,0x10},
	{0x3814,0x01},
	{0x3815,0x01},
	{0x3816,0x01},
	{0x3817,0x01},
	{0x381c,0x00},
	{0x3820,0x00},
	{0x3821,0x04},
	{0x3823,0x08},
	{0x3826,0x00},
	{0x3827,0x08},
	{0x382d,0x08},
	{0x3832,0x02},
	{0x3833,0x00},
	{0x383c,0x48},
	{0x383d,0xff},
	{0x3d85,0x0b},
	{0x3d84,0x40},
	{0x3d8c,0x63},
	{0x3d8d,0xd7},
	{0x4000,0xf8},
	{0x4001,0x2b},
	{0x4004,0x00},
	{0x4005,0x40},
	{0x400a,0x01},
	{0x400f,0xa0},
	{0x4010,0x12},
	{0x4018,0x00},
	{0x4008,0x02},
	{0x4009,0x0d},
	{0x401a,0x58},
	{0x4050,0x00},
	{0x4051,0x01},
	{0x4028,0x2f},
	{0x4052,0x00},
	{0x4053,0x80},
	{0x4054,0x00},
	{0x4055,0x80},
	{0x4056,0x00},
	{0x4057,0x80},
	{0x4058,0x00},
	{0x4059,0x80},
	{0x430b,0xff},
	{0x430c,0xff},
	{0x430d,0x00},
	{0x430e,0x00},
	{0x4501,0x18},
	{0x4502,0x00},
	{0x4643,0x00},
	{0x4640,0x01},
	{0x4641,0x04},
	{0x4800,0x64},
	{0x4809,0x2b},
	{0x4813,0x90},
	{0x4817,0x04},
	{0x4833,0x18},
	{0x4837,0x0b},
	{0x483b,0x00},
	{0x484b,0x03},
	{0x4850,0x7c},
	{0x4852,0x06},
	{0x4856,0x58},
	{0x4857,0xaa},
	{0x4862,0x0a},
	{0x4869,0x18},
	{0x486a,0xaa},
	{0x486e,0x03},
	{0x486f,0x55},
	{0x4875,0xf0},
	{0x5000,0x89},
	{0x5001,0x42},
	{0x5004,0x40},
	{0x5005,0x00},
	{0x5180,0x00},
	{0x5181,0x10},
	{0x580b,0x03},
	{0x4d00,0x03},
	{0x4d01,0xc9},
	{0x4d02,0xbc},
	{0x4d03,0xc6},
	{0x4d04,0x4a},
	{0x4d05,0x25},
	{0x4028,0x4f},
	{0x4029,0x1f},
	{0x402a,0x7f},
	{0x402b,0x01},
	{0x4700,0x2b},
	{0x4e00,0x2b},
	{0x3501,0x09},
	{0x3502,0x01},
	{OS08A10_REG_END, 0x00},/* END MARKER */
};

static struct regval_list os08a10_init_regs_3840_2160_20fps_mipi[] = {
	{0x0100,0x00},
	{0x0103,0x01},
	{0x0303,0x01},
	{0x0305,0x5a},
	{0x0306,0x00},
	{0x0308,0x03},
	{0x0309,0x04},
	{0x032a,0x00},
	{0x300f,0x11},
	{0x3010,0x01},
	{0x3011,0x04},
	{0x3012,0x21},
	{0x3016,0xf0},
	{0x301e,0x98},
	{0x3031,0xa9},
	{0x3103,0x92},
	{0x3104,0x01},
	{0x3106,0x10},
	{0x3400,0x04},
	{0x3025,0x03},
	{0x3425,0x01},
	{0x3428,0x01},
	{0x3406,0x08},
	{0x3408,0x03},
	{0x340c,0xff},
	{0x340d,0xff},
	{0x031e,0x09},
	{0x3501,0x08},
	{0x3502,0xe5},
	{0x3505,0x83},
	{0x3508,0x00},
	{0x3509,0x80},
	{0x350a,0x04},
	{0x350b,0x00},
	{0x350c,0x00},
	{0x350d,0x80},
	{0x350e,0x04},
	{0x350f,0x00},
	{0x3600,0x00},
	{0x3603,0x2c},
	{0x3605,0x50},
	{0x3609,0xb5},
	{0x3610,0x39},
	{0x360c,0x01},
	{0x3628,0xa4},
	{0x362d,0x10},
	{0x3660,0x43},
	{0x3661,0x06},
	{0x3662,0x00},
	{0x3663,0x28},
	{0x3664,0x0d},
	{0x366a,0x38},
	{0x366b,0xa0},
	{0x366d,0x00},
	{0x366e,0x00},
	{0x3680,0x00},
	{0x36c0,0x00},
	{0x3701,0x02},
	{0x373b,0x02},
	{0x373c,0x02},
	{0x3736,0x02},
	{0x3737,0x02},
	{0x3705,0x00},
	{0x3706,0x39},
	{0x370a,0x00},
	{0x370b,0x98},
	{0x3709,0x49},
	{0x3714,0x21},
	{0x371c,0x00},
	{0x371d,0x08},
	{0x3740,0x1b},
	{0x3741,0x04},
	{0x375e,0x0b},
	{0x3760,0x10},
	{0x3776,0x10},
	{0x3781,0x02},
	{0x3782,0x04},
	{0x3783,0x02},
	{0x3784,0x08},
	{0x3785,0x08},
	{0x3788,0x01},
	{0x3789,0x01},
	{0x3797,0x04},
	{0x3762,0x11},
	{0x3800,0x00},
	{0x3801,0x00},
	{0x3802,0x00},
	{0x3803,0x0c},
	{0x3804,0x0e},
	{0x3805,0xff},
	{0x3806,0x08},
	{0x3807,0x6f},
	{0x3808,0x0f},
	{0x3809,0x00},
	{0x380a,0x08},
	{0x380b,0x70},
	{0x380c,0x08},//
	{0x380d,0x18},//hts = 0x818=2072
	{0x380e,0x0d},//
	{0x380f,0x8f},//vts = 0xd8f=3471
	{0x3813,0x10},
	{0x3814,0x01},
	{0x3815,0x01},
	{0x3816,0x01},
	{0x3817,0x01},
	{0x381c,0x00},
	{0x3820,0x00},//flip
	{0x3821,0x04},//mirror
	{0x3823,0x08},
	{0x3826,0x00},
	{0x3827,0x08},
	{0x382d,0x08},
	{0x3832,0x02},
	{0x3833,0x00},
	{0x383c,0x48},
	{0x383d,0xff},
	{0x3d85,0x0b},
	{0x3d84,0x40},
	{0x3d8c,0x63},
	{0x3d8d,0xd7},
	{0x4000,0xf8},
	{0x4001,0x2b},
	{0x4004,0x00},
	{0x4005,0x40},
	{0x400a,0x01},
	{0x400f,0xa0},
	{0x4010,0x12},
	{0x4018,0x00},
	{0x4008,0x02},
	{0x4009,0x0d},
	{0x401a,0x58},
	{0x4050,0x00},
	{0x4051,0x01},
	{0x4028,0x2f},
	{0x4052,0x00},
	{0x4053,0x80},
	{0x4054,0x00},
	{0x4055,0x80},
	{0x4056,0x00},
	{0x4057,0x80},
	{0x4058,0x00},
	{0x4059,0x80},
	{0x430b,0xff},
	{0x430c,0xff},
	{0x430d,0x00},
	{0x430e,0x00},
	{0x4501,0x18},
	{0x4502,0x00},
	{0x4643,0x00},
	{0x4640,0x01},
	{0x4641,0x04},
	{0x4800,0x64},
	{0x4809,0x2b},
	{0x4813,0x90},
	{0x4817,0x04},
	{0x4833,0x18},
	{0x4837,0x0a},
	{0x483b,0x00},
	{0x484b,0x03},
	{0x4850,0x7c},
	{0x4852,0x06},
	{0x4856,0x58},
	{0x4857,0xaa},
	{0x4862,0x0a},
	{0x4869,0x18},
	{0x486a,0xaa},
	{0x486e,0x03},
	{0x486f,0x55},
	{0x4875,0xf0},
	{0x5000,0x89},
	{0x5001,0x42},
	{0x5004,0x40},
	{0x5005,0x00},
	{0x5180,0x00},
	{0x5181,0x10},
	{0x580b,0x03},
	{0x4d00,0x03},
	{0x4d01,0xc9},
	{0x4d02,0xbc},
	{0x4d03,0xc6},
	{0x4d04,0x4a},
	{0x4d05,0x25},
	{0x4700,0x2b},
	{0x4e00,0x2b},
	{0x3501,0x09},
	{0x3502,0x01},
	{0x4028,0x4f},
	{0x4029,0x1f},
	{0x402a,0x7f},
	{0x402b,0x01},
	{0x0100,0x01},
	{OS08A10_REG_END, 0x00},/* END MARKER */
};

static struct tx_isp_sensor_win_setting os08a10_win_sizes[] = {
	{
		.width		= 3840,
		.height		= 2160,
		.fps		= 30 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= os08a10_init_regs_3840_2160_30fps_mipi,
	},
	{
		.width		= 3840,
		.height		= 2160,
		.fps		= 20 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= os08a10_init_regs_3840_2160_20fps_mipi,
	}
};
struct tx_isp_sensor_win_setting *wsize = &os08a10_win_sizes[0];

static struct regval_list os08a10_stream_on_mipi[] = {
	{0x0100, 0x01},
	{OS08A10_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list os08a10_stream_off_mipi[] = {
	{0x0100, 0x00},
	{OS08A10_REG_END, 0x00},	/* END MARKER */
};

int os08a10_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value)
{
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
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int os08a10_write(struct tx_isp_subdev *sd, uint16_t reg,
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

#if 0
static int os08a10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != OS08A10_REG_END) {
		if (vals->reg_num == OS08A10_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = os08a10_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int os08a10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != OS08A10_REG_END) {
		if (vals->reg_num == OS08A10_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = os08a10_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int os08a10_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int os08a10_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = os08a10_read(sd, 0x300a, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS08A10_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = os08a10_read(sd, 0x300b, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS08A10_CHIP_ID_M)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	ret = os08a10_read(sd, 0x300c, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS08A10_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int os08a10_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = -1;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret += os08a10_write(sd, 0x3501, (unsigned char)((it >> 8) & 0xff));
	ret += os08a10_write(sd, 0x3502, (unsigned char)(it & 0xff));

	ret += os08a10_write(sd, 0x3508, (unsigned char)((again >> 8) & 0x3f));
	ret += os08a10_write(sd, 0x3509, (unsigned char)(again & 0xff));

	return 0;
}

#if 0
static int os08a10_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;

	ret += os08a10_write(sd, 0x3502, (unsigned char)(expo & 0xff));
	ret += os08a10_write(sd, 0x3501, (unsigned char)((expo >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int os08a10_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += os08a10_write(sd, 0x3509, (unsigned char)((value & 0xff)));
	ret += os08a10_write(sd, 0x3508, (unsigned char)((value >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int os08a10_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int os08a10_get_black_pedestal(struct tx_isp_subdev *sd, int value)
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

static int os08a10_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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

static int os08a10_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if(sensor->video.state == TX_ISP_MODULE_INIT){
			ret = os08a10_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if(sensor->video.state == TX_ISP_MODULE_RUNNING){

			ret = os08a10_write_array(sd, os08a10_stream_on_mipi);
			ISP_WARNING("os08a10 stream on\n");
		}
	}
	else {
		ret = os08a10_write_array(sd, os08a10_stream_off_mipi);
		ISP_WARNING("os08a10 stream off\n");
	}

	return ret;
}

static int os08a10_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 2314 * 2072 * 30;
		max_fps = TX_SENSOR_MAX_FPS_30;
		break;
	case 1:
		sclk =  2072 * 3471 * 20;
		max_fps = TX_SENSOR_MAX_FPS_20;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps<< 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}

	ret += os08a10_read(sd, 0x380c, &val);
	hts = val << 8;
	ret += os08a10_read(sd, 0x380d, &val);
	hts = (hts | val);

	if (0 != ret) {
		ISP_ERROR("err: os08a10 read err\n");
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = os08a10_write(sd, 0x380f, (unsigned char)(vts & 0xff));
	ret += os08a10_write(sd, 0x380e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: os08a10_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 8;
	sensor->video.attr->integration_time_limit = vts - 8;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 8;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int os08a10_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

#if 1
static int os08a10_set_hvflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val_m;
	uint8_t val_f;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	os08a10_read(sd, 0x3821, &val_m);
	os08a10_read(sd, 0x3820, &val_f);

	switch(enable) {
	case 0:
		os08a10_write(sd, 0x3221, val_m | 0x0);
		os08a10_write(sd, 0x3220, val_f | 0x0);
		break;
	case 1:
		os08a10_write(sd, 0x3221, val_m | 0x4);
		break;
	case 2:
		os08a10_write(sd, 0x3220, val_f | 0x4);
		break;
	case 3:
		os08a10_write(sd, 0x3221, val_m | 0x4);
		os08a10_write(sd, 0x3220, val_f | 0x4);
		break;
	}

	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}
#endif

static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	unsigned long rate;
	int ret;

	switch(info->default_boot){
	case 0:
		wsize = &os08a10_win_sizes[0];
		memcpy(&(os08a10_attr.mipi), &os08a10_mipi, sizeof(os08a10_mipi));
		os08a10_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		os08a10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		os08a10_attr.max_integration_time_native = 2314 - 8;
		os08a10_attr.integration_time_limit = 2314 - 8;
		os08a10_attr.total_width = 4144;
		os08a10_attr.total_height = 2314;
		os08a10_attr.max_integration_time = 2314 - 8;
		os08a10_attr.again = 0x80;
		os08a10_attr.integration_time = 0x901;
		break;
	case 1:
		wsize = &os08a10_win_sizes[1];
		memcpy(&(os08a10_attr.mipi), &os08a10_mipi, sizeof(os08a10_mipi));
		os08a10_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		os08a10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		os08a10_attr.max_integration_time_native = 3471 - 8;
		os08a10_attr.integration_time_limit = 3471 - 8;
		os08a10_attr.total_width = 4144;
		os08a10_attr.total_height = 3471;
		os08a10_attr.max_integration_time = 3471 - 8;
		os08a10_attr.again = 0x80;
		os08a10_attr.integration_time =0x901;
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
	}

	switch(info->video_interface){
	case TISP_SENSOR_VI_MIPI_CSI0:
		os08a10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		os08a10_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_DVP:
		os08a10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

}

static int os08a10_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"os08a10_reset");
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
		ret = private_gpio_request(pwdn_gpio,"os08a10_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = os08a10_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an os08a10 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("os08a10 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "os08a10", sizeof("os08a10"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int os08a10_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = os08a10_set_expo(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		//if(arg)
		//	ret = os08a10_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		//if(arg)
		//	ret = os08a10_set_analog_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = os08a10_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = os08a10_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = os08a10_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = os08a10_write_array(sd, os08a10_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = os08a10_write_array(sd, os08a10_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = os08a10_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = os08a10_set_hvflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int os08a10_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = os08a10_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int os08a10_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	os08a10_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops os08a10_core_ops = {
	.g_chip_ident = os08a10_g_chip_ident,
	.reset = os08a10_reset,
	.init = os08a10_init,
	.g_register = os08a10_g_register,
	.s_register = os08a10_s_register,
};

static struct tx_isp_subdev_video_ops os08a10_video_ops = {
	.s_stream = os08a10_s_stream,
};

static struct tx_isp_subdev_sensor_ops	os08a10_sensor_ops = {
	.ioctl	= os08a10_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops os08a10_ops = {
	.core = &os08a10_core_ops,
	.video = &os08a10_video_ops,
	.sensor = &os08a10_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "os08a10",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int os08a10_probe(struct i2c_client *client,
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
	os08a10_attr.expo_fs = 1;
	sensor->video.shvflip = 1;
	sensor->video.attr = &os08a10_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &os08a10_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->os08a10\n");

	return 0;
}

static int os08a10_remove(struct i2c_client *client)
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

static const struct i2c_device_id os08a10_id[] = {
	{ "os08a10", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, os08a10_id);

static struct i2c_driver os08a10_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "os08a10",
	},
	.probe		= os08a10_probe,
	.remove		= os08a10_remove,
	.id_table	= os08a10_id,
};

static __init int init_os08a10(void)
{
	return private_i2c_add_driver(&os08a10_driver);
}

static __exit void exit_os08a10(void)
{
	private_i2c_del_driver(&os08a10_driver);
}

module_init(init_os08a10);
module_exit(exit_os08a10);

MODULE_DESCRIPTION("A low-level driver for os08a10 sensors");
MODULE_LICENSE("GPL");
