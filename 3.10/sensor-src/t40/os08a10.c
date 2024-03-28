// SPDX-License-Identifier: GPL-2.0+
/*
 * os08a10.c
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
#include <txx-funcs.h>

#define SENSOR_NAME "os08a10"
#define SENSOR_CHIP_ID_H (0x53)
#define SENSOR_CHIP_ID_M (0x08)
#define SENSOR_CHIP_ID_L (0x41)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK_8M_FPS_15 (98222880)
#define SENSOR_SUPPORT_SCLK_8M_FPS_30 (71924300)
#define SENSOR_SUPPORT_SCLK_8M_FPS_30_2lane (2314 * 2072 * 30)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_1
#define SENSOR_VERSION "H20211019a"
#define MCLK 24000000

static int reset_gpio = GPIO_PC(27);
static int pwdn_gpio = -1;
static int sensor_max_fps = 15;

static int wdr_bufsize = 2400000;//cache lines corrponding on VPB1
module_param(wdr_bufsize, int, S_IRUGO);
MODULE_PARM_DESC(wdr_bufsize, "Wdr Buf Size");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
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

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sensor_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi_ ={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1440,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
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

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x530841,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x36,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 800,
		.lans = 4,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
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
	},
	.max_again = 262080,
	.max_again_short = 262080,
	.max_dgain = 0,
	.min_integration_time = 8,
	.min_integration_time_native = 8,
	.max_integration_time_native = 0xbea - 8,
	.integration_time_limit = 0xbea - 8,
	.min_integration_time_short = 1,
	.max_integration_time_short = 98,
	.total_width = 0x16b0,
	.total_height = 0xbea,
	.max_integration_time = 0xbea - 8,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_again_short = sensor_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_3840_2160_30fps_mipi[] = {
//@@ 0 2 Res 3840X2160 4lane MIPI0720Mbps Linear10 30fps MCLK24M GAIA26
	{0x0100, 0x00},
	{0x0103, 0x01},
	{0x0303, 0x01},
	{0x0305, 0x2d}, //;32
	{0x0306, 0x00},
	{0x0308, 0x03},
	{0x0309, 0x04},
	{0x0325, 0x2a},
	{0x0328, 0x06},
	{0x0327, 0x06},
	{0x032a, 0x00},
	{0x300f, 0x11},
	{0x3010, 0x01},
	{0x3011, 0x04},
	{0x3012, 0x41},
	{0x3016, 0xf0},
	{0x301e, 0x98},
	{0x3031, 0xa9},
	{0x3103, 0x92},
	{0x3104, 0x01},
	{0x3106, 0x10},
	{0x3400, 0x04},
	{0x3025, 0x03},
	{0x3425, 0x01},
	{0x3428, 0x01},
	{0x3406, 0x08},
	{0x3408, 0x03},
	{0x340c, 0xff},
	{0x340d, 0xff},
	{0x031e, 0x09},
	{0x3501, 0x08},
	{0x3502, 0xe5},
	{0x3505, 0x83},
	{0x3508, 0x00},
	{0x3509, 0x80},
	{0x350a, 0x04},
	{0x350b, 0x00},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3600, 0x00},
	{0x3603, 0x2c},
	{0x3605, 0x50},
	{0x3609, 0xb5},
	{0x3610, 0x39},
	{0x360c, 0x01},
	{0x3628, 0xa4},
	{0x362d, 0x10},
	{0x3660, 0x43},
	{0x3661, 0x06},
	{0x3662, 0x00},
	{0x3663, 0x28},
	{0x3664, 0x0d},
	{0x366a, 0x38},
	{0x366b, 0xa0},
	{0x366d, 0x00},
	{0x366e, 0x00},
	{0x3680, 0x00},
	{0x36c0, 0x00},
	{0x3701, 0x02},
	{0x373b, 0x02},
	{0x373c, 0x02},
	{0x3736, 0x02},
	{0x3737, 0x02},
	{0x3705, 0x00},
	{0x3706, 0x39},
	{0x370a, 0x00},
	{0x370b, 0x98},
	{0x3709, 0x49},
	{0x3714, 0x21},
	{0x371c, 0x00},
	{0x371d, 0x08},
	{0x3740, 0x1b},
	{0x3741, 0x04},
	{0x375e, 0x0b},
	{0x3760, 0x10},
	{0x3776, 0x10},
	{0x3781, 0x02},
	{0x3782, 0x04},
	{0x3783, 0x02},
	{0x3784, 0x08},
	{0x3785, 0x08},
	{0x3788, 0x01},
	{0x3789, 0x01},
	{0x3797, 0x04},
	{0x3762, 0x11},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0e},
	{0x3805, 0xff},
	{0x3806, 0x08},
	{0x3807, 0x6f},
	{0x3808, 0x0f},
	{0x3809, 0x00},
	{0x380a, 0x08},
	{0x380b, 0x70},
	{0x380c, 0x04},
	{0x380d, 0x0c},
	{0x380e, 0x09},
	{0x380f, 0x0a},
	{0x3813, 0x10},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},
	{0x381c, 0x00},
	{0x3820, 0x00},
	{0x3821, 0x04},
	{0x3823, 0x08},
	{0x3826, 0x00},
	{0x3827, 0x08},
	{0x382d, 0x08},
	{0x3832, 0x02},
	{0x3833, 0x00},
	{0x383c, 0x48},
	{0x383d, 0xff},
	{0x3d85, 0x0b},
	{0x3d84, 0x40},
	{0x3d8c, 0x63},
	{0x3d8d, 0xd7},
	{0x4000, 0xf8},
	{0x4001, 0x2b},
	{0x4004, 0x00},
	{0x4005, 0x40},
	{0x400a, 0x01},
	{0x400f, 0xa0},
	{0x4010, 0x12},
	{0x4018, 0x00},
	{0x4008, 0x02},
	{0x4009, 0x0d},
	{0x401a, 0x58},
	{0x4050, 0x00},
	{0x4051, 0x01},
	{0x4028, 0x2f},
	{0x4052, 0x00},
	{0x4053, 0x80},
	{0x4054, 0x00},
	{0x4055, 0x80},
	{0x4056, 0x00},
	{0x4057, 0x80},
	{0x4058, 0x00},
	{0x4059, 0x80},
	{0x430b, 0xff},
	{0x430c, 0xff},
	{0x430d, 0x00},
	{0x430e, 0x00},
	{0x4501, 0x18},
	{0x4502, 0x00},
	{0x4643, 0x00},
	{0x4640, 0x01},
	{0x4641, 0x04},
	{0x4800, 0x64},
	{0x4809, 0x2b},
	{0x4813, 0x90},
	{0x4817, 0x04},
	{0x4819, 0x76},
	{0x482a, 0x05},
	{0x4826, 0x30},
	{0x4831, 0x40},
	{0x4833, 0x18},
	{0x4837, 0x16}, //;14
	{0x483b, 0x00},
	{0x484b, 0x03},
	{0x4850, 0x7c},
	{0x4852, 0x06},
	{0x4856, 0x58},
	{0x4857, 0xaa},
	{0x4862, 0x0a},
	{0x4869, 0x18},
	{0x486a, 0xaa},
	{0x486e, 0x03},
	{0x486f, 0x55},
	{0x4875, 0xf0},
	{0x5000, 0x89},
	{0x5001, 0x42},
	{0x5004, 0x40},
	{0x5005, 0x00},
	{0x5180, 0x00},
	{0x5181, 0x10},
	{0x580b, 0x03},
	{0x4d00, 0x03},
	{0x4d01, 0xc9},
	{0x4d02, 0xbc},
	{0x4d03, 0xc6},
	{0x4d04, 0x4a},
	{0x4d05, 0x25},
	{0x4700, 0x2b},
	{0x4e00, 0x2b},
	{0x380c, 0x04}, //;07
	{0x380d, 0x0c}, //;10
	{0x380e, 0x0a}, //;0a
	{0x380f, 0xd9}, //;5e
	{0x3501, 0x0a},
	{0x3502, 0x4e},
	{0x0100, 0x01},
	{0x0100, 0x01},
	{0x0100, 0x01},
	{0x0100, 0x01},

	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_3840_2160_15fps_mipi[] = {
//@@ 0 2 Res 3840X2160 4lane MIPI640Mbps Linear10 15fps MCLK24M
	{0x0100, 0x00},
	{0x0103, 0x01},
	{0x0303, 0x01},
	{0x0305, 0x28},
	{0x0306, 0x00},
	{0x0308, 0x03},
	{0x0309, 0x04},
	{0x032a, 0x00},
	{0x0325, 0x32},
	{0x300f, 0x11},
	{0x3010, 0x01},
	{0x3011, 0x04},
	{0x3012, 0x41},
	{0x3016, 0xf0},
	{0x301e, 0x98},
	{0x3031, 0xa9},
	{0x3103, 0x92},
	{0x3104, 0x01},
	{0x3106, 0x10},
	{0x3400, 0x04},
	{0x3025, 0x03},
	{0x3425, 0x01},
	{0x3428, 0x01},
	{0x3406, 0x08},
	{0x3408, 0x03},
	{0x340c, 0xff},
	{0x340d, 0xff},
	{0x031e, 0x09},
	{0x3501, 0x08},
	{0x3502, 0xe5},
	{0x3505, 0x83},
	{0x3508, 0x00},
	{0x3509, 0x80},
	{0x350a, 0x04},
	{0x350b, 0x00},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3600, 0x00},
	{0x3603, 0x2c},
	{0x3605, 0x50},
	{0x3609, 0xb5},
	{0x3610, 0x39},
	{0x360c, 0x01},
	{0x3628, 0xa4},
	{0x362d, 0x10},
	{0x3660, 0x43},
	{0x3661, 0x06},
	{0x3662, 0x00},
	{0x3663, 0x28},
	{0x3664, 0x0d},
	{0x366a, 0x38},
	{0x366b, 0xa0},
	{0x366d, 0x00},
	{0x366e, 0x00},
	{0x3680, 0x00},
	{0x36c0, 0x00},
	{0x3701, 0x02},
	{0x373b, 0x02},
	{0x373c, 0x02},
	{0x3736, 0x02},
	{0x3737, 0x02},
	{0x3705, 0x00},
	{0x3706, 0x39},
	{0x370a, 0x00},
	{0x370b, 0x98},
	{0x3709, 0x49},
	{0x3714, 0x21},
	{0x371c, 0x00},
	{0x371d, 0x08},
	{0x3740, 0x1b},
	{0x3741, 0x04},
	{0x375e, 0x0b},
	{0x3760, 0x10},
	{0x3776, 0x10},
	{0x3781, 0x02},
	{0x3782, 0x04},
	{0x3783, 0x02},
	{0x3784, 0x08},
	{0x3785, 0x08},
	{0x3788, 0x01},
	{0x3789, 0x01},
	{0x3797, 0x04},
	{0x3762, 0x11},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0e},
	{0x3805, 0xff},
	{0x3806, 0x08},
	{0x3807, 0x6f},
	{0x3808, 0x0f},
	{0x3809, 0x00},
	{0x380a, 0x08},
	{0x380b, 0x70},
	{0x380c, 0x0a},
	{0x380d, 0x10},
	{0x380e, 0x09},
	{0x380f, 0xee},
	{0x3813, 0x10},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},
	{0x381c, 0x00},
	{0x3820, 0x00},
	{0x3821, 0x04},
	{0x3823, 0x08},
	{0x3826, 0x00},
	{0x3827, 0x08},
	{0x382d, 0x08},
	{0x3832, 0x02},
	{0x3833, 0x00},
	{0x383c, 0x48},
	{0x383d, 0xff},
	{0x3d85, 0x0b},
	{0x3d84, 0x40},
	{0x3d8c, 0x63},
	{0x3d8d, 0xd7},
	{0x4000, 0xf8},
	{0x4001, 0x2b},
	{0x4004, 0x00},
	{0x4005, 0x40},
	{0x400a, 0x01},
	{0x400f, 0xa0},
	{0x4010, 0x12},
	{0x4018, 0x00},
	{0x4008, 0x02},
	{0x4009, 0x0d},
	{0x401a, 0x58},
	{0x4050, 0x00},
	{0x4051, 0x01},
	{0x4028, 0x2f},
	{0x4052, 0x00},
	{0x4053, 0x80},
	{0x4054, 0x00},
	{0x4055, 0x80},
	{0x4056, 0x00},
	{0x4057, 0x80},
	{0x4058, 0x00},
	{0x4059, 0x80},
	{0x430b, 0xff},
	{0x430c, 0xff},
	{0x430d, 0x00},
	{0x430e, 0x00},
	{0x4501, 0x18},
	{0x4502, 0x00},
	{0x4643, 0x00},
	{0x4640, 0x01},
	{0x4641, 0x04},
	{0x4800, 0x64},
	{0x4809, 0x2b},
	{0x4813, 0x90},
	{0x4817, 0x04},
	{0x4821, 0x4a},
	{0x4826, 0x30},
	{0x482e, 0x2c},
	{0x4819, 0x73},
	{0x481b, 0x3c},
	{0x4831, 0x40},
	{0x4833, 0x18},
	{0x4837, 0x19},
	{0x483b, 0x00},
	{0x484b, 0x03},
	{0x4850, 0x7c},
	{0x4852, 0x06},
	{0x4856, 0x58},
	{0x4857, 0xaa},
	{0x4862, 0x0a},
	{0x4869, 0x18},
	{0x486a, 0xaa},
	{0x486e, 0x03},
	{0x486f, 0x55},
	{0x4875, 0xf0},
	{0x5000, 0x89},
	{0x5001, 0x42},
	{0x5004, 0x40},
	{0x5005, 0x00},
	{0x5180, 0x00},
	{0x5181, 0x10},
	{0x580b, 0x03},
	{0x4d00, 0x03},
	{0x4d01, 0xc9},
	{0x4d02, 0xbc},
	{0x4d03, 0xc6},
	{0x4d04, 0x4a},
	{0x4d05, 0x25},
	{0x4700, 0x2b},
	{0x4e00, 0x2b},
	{0x3501, 0x09},
	{0x3502, 0x01},
	{0x0100, 0x01},
	{0x0100, 0x01},
	{0x0100, 0x01},
	{0x0100, 0x01},

	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_3840_2160_15fps_mipi_wdr[] = {
/* @@ 3840x2160 4lane 10bit MIPI HDR Combine15fps Mclk24 */
	{0x0100, 0x00},
	{0x0103, 0x01},
	{0x0303, 0x01},
	{0x0305, 0x5a},
	{0x0306, 0x00},
	{0x0308, 0x03},
	{0x0309, 0x04},
	{0x032a, 0x00},
	{0x300f, 0x11},
	{0x3010, 0x01},
	{0x3011, 0x04},
	{0x3012, 0x41},
	{0x3016, 0xf0},
	{0x301e, 0x98},
	{0x3031, 0xa9},
	{0x3103, 0x92},
	{0x3104, 0x01},
	{0x3106, 0x10},
	{0x340c, 0xff},
	{0x340d, 0xff},
	{0x031e, 0x09},
	{0x3501, 0x08},
	{0x3502, 0xe5},
	{0x3505, 0x83},
	{0x3508, 0x00},
	{0x3509, 0x80},
	{0x350a, 0x04},
	{0x350b, 0x00},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3600, 0x00},
	{0x3603, 0x2c},
	{0x3605, 0x50},
	{0x3609, 0xb5},
	{0x3610, 0x39},
	{0x360c, 0x01},
	{0x3628, 0xa4},
	{0x362d, 0x10},
	{0x3660, 0x42},
	{0x3661, 0x07},
	{0x3662, 0x00},
	{0x3663, 0x28},
	{0x3664, 0x0d},
	{0x366a, 0x38},
	{0x366b, 0xa0},
	{0x366d, 0x00},
	{0x366e, 0x00},
	{0x3680, 0x00},
	{0x36c0, 0x00},
	{0x3701, 0x02},
	{0x373b, 0x02},
	{0x373c, 0x02},
	{0x3736, 0x02},
	{0x3737, 0x02},
	{0x3705, 0x00},
	{0x3706, 0x39},
	{0x370a, 0x00},
	{0x370b, 0x98},
	{0x3709, 0x49},
	{0x3714, 0x21},
	{0x371c, 0x00},
	{0x371d, 0x08},
	{0x3740, 0x1b},
	{0x3741, 0x04},
	{0x375e, 0x0b},
	{0x3760, 0x10},
	{0x3776, 0x10},
	{0x3781, 0x02},
	{0x3782, 0x04},
	{0x3783, 0x02},
	{0x3784, 0x08},
	{0x3785, 0x08},
	{0x3788, 0x01},
	{0x3789, 0x01},
	{0x3797, 0x04},
	{0x3762, 0x11},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0e},
	{0x3805, 0xff},
	{0x3806, 0x08},
	{0x3807, 0x6f},
	{0x3808, 0x0f},
	{0x3809, 0x00},
	{0x380a, 0x08},
	{0x380b, 0x70},
	{0x380c, 0x07},
	{0x380d, 0x62},
	{0x380e, 0x09},
	{0x380f, 0xe4},
	{0x3813, 0x10},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},
	{0x381c, 0x08},
	{0x3820, 0x00},
	{0x3821, 0x24},
	{0x3823, 0x08},
	{0x3826, 0x00},
	{0x3827, 0x08},
	{0x382d, 0x08},
	{0x3832, 0x02},
	{0x3833, 0x01},
	{0x383c, 0x48},
	{0x383d, 0xff},
	{0x3d85, 0x0b},
	{0x3d84, 0x40},
	{0x3d8c, 0x63},
	{0x3d8d, 0xd7},
	{0x4000, 0xf8},
	{0x4001, 0x2b},
	{0x4004, 0x00},
	{0x4005, 0x40},
	{0x400a, 0x01},
	{0x400f, 0xa0},
	{0x4010, 0x12},
	{0x4018, 0x00},
	{0x4008, 0x02},
	{0x4009, 0x0d},
	{0x401a, 0x58},
	{0x4050, 0x00},
	{0x4051, 0x01},
	{0x4028, 0x2f},
	{0x4052, 0x00},
	{0x4053, 0x80},
	{0x4054, 0x00},
	{0x4055, 0x80},
	{0x4056, 0x00},
	{0x4057, 0x80},
	{0x4058, 0x00},
	{0x4059, 0x80},
	{0x430b, 0xff},
	{0x430c, 0xff},
	{0x430d, 0x00},
	{0x430e, 0x00},
	{0x4501, 0x18},
	{0x4502, 0x00},
	{0x4643, 0x00},
	{0x4640, 0x01},
	{0x4641, 0x04},
	{0x4800, 0x64},
	{0x4809, 0x2b},
	{0x4813, 0x98},
	{0x4817, 0x04},
	{0x4833, 0x18},
	{0x4837, 0x0b},
	{0x483b, 0x00},
	{0x484b, 0x03},
	{0x4850, 0x7c},
	{0x4852, 0x06},
	{0x4856, 0x58},
	{0x4857, 0xaa},
	{0x4862, 0x0a},
	{0x4869, 0x18},
	{0x486a, 0xaa},
	{0x486e, 0x07},
	{0x486f, 0x55},
	{0x4875, 0xf0},
	{0x5000, 0x89},
	{0x5001, 0x42},
	{0x5004, 0x40},
	{0x5005, 0x00},
	{0x5180, 0x00},
	{0x5181, 0x10},
	{0x580b, 0x03},
	{0x4d00, 0x03},
	{0x4d01, 0xc9},
	{0x4d02, 0xbc},
	{0x4d03, 0xc6},
	{0x4d04, 0x4a},
	{0x4d05, 0x25},
	{0x4700, 0x2b},
	{0x4e00, 0x2b},
	{0x3501, 0x08},
	{0x3502, 0xe1},
	{0x3511, 0x00},
	{0x3512, 0x20},
	{0x3833, 0x01},
	{0x0100, 0x01},
	{0x0100, 0x01},
	{0x0100, 0x01},
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_3840_2160_15fps_mipi_2lane[] = {
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
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 3840*2160@15fps [0] */
	{
		.width = 3840,
		.height = 2160,
		.fps = 15 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_3840_2160_15fps_mipi,
	},
	/* 3840*2160@25fps [1] */
	{
		.width = 3840,
		.height = 2160,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_3840_2160_30fps_mipi,
	},
	/* 3840*2160@15fps [2] */
	{
		.width = 3840,
		.height = 2160,
		.fps = 15 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_3840_2160_15fps_mipi_wdr,
	},
	/* 3840*2160@30fps_2lane [3] */
	{
		.width = 3840,
		.height = 2160,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_3840_2160_15fps_mipi_2lane,
	}
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sensor_stream_on_mipi[] = {
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,
		 unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = buf,
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

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg,
		  unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 3,
		.buf = buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 0
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
		}
		vals++;
	}

	return 0;
}
#endif

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

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sensor_read(sd, 0x300a, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x300b, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_M)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	ret = sensor_read(sd, 0x300c, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = -1;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret += sensor_write(sd, 0x3501, (unsigned char)((it >> 8) & 0xff));
	ret += sensor_write(sd, 0x3502, (unsigned char)(it & 0xff));

	ret += sensor_write(sd, 0x3508, (unsigned char)((again >> 8) & 0x3f));
	ret += sensor_write(sd, 0x3509, (unsigned char)(again & 0xff));

	return 0;
}

static int sensor_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;

	ret += sensor_write(sd, 0x3512, (unsigned char)(expo & 0xff));
	ret += sensor_write(sd, 0x3511, (unsigned char)((expo >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sensor_write(sd, 0x350d, (unsigned char)((value & 0xff)));
	ret += sensor_write(sd, 0x350c, (unsigned char)((value >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;

	ret += sensor_write(sd, 0x3502, (unsigned char)(expo & 0xff));
	ret += sensor_write(sd, 0x3501, (unsigned char)((expo >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sensor_write(sd, 0x3509, (unsigned char)((value & 0xff)));
	ret += sensor_write(sd, 0x3508, (unsigned char)((value >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable) {
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		return ISP_SUCCESS;
	} else {
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = TISP_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		sensor->video.state = TX_ISP_MODULE_DEINIT;

		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
		sensor->priv = wsize;
	}

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
			ret = sensor_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			ISP_WARNING("%s stream on\n", SENSOR_NAME);
		}
	} else {
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int max_fps;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	if (sensor_attr.data_type == TX_SENSOR_DATA_TYPE_WDR_DOL)
		return 0;

	switch(sensor->info.default_boot) {
	case 0:
		sclk = SENSOR_SUPPORT_SCLK_8M_FPS_15;
		max_fps = TX_SENSOR_MAX_FPS_15;
		break;
	case 1:
		sclk = SENSOR_SUPPORT_SCLK_8M_FPS_30;
		max_fps = TX_SENSOR_MAX_FPS_30;
		break;
	case 2:
		sclk = SENSOR_SUPPORT_SCLK_8M_FPS_15;
		max_fps = TX_SENSOR_MAX_FPS_15;
		break;
	case 3:
		sclk = SENSOR_SUPPORT_SCLK_8M_FPS_30_2lane;
		max_fps = TX_SENSOR_MAX_FPS_30;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps<< 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}

	ret += sensor_read(sd, 0x380c, &val);
	hts = val << 8;
	ret += sensor_read(sd, 0x380d, &val);
	hts = (hts | val);

	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = sensor_write(sd, 0x380f, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x380e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
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

static unsigned char evl1,evl2,evlgain1,evlgain2;
static int sensor_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = 0;

	/* struct timeval tv; */

	/* do_gettimeofday(&tv); */
	/* pr_debug("%d:before:time is %d.%d\n", __LINE__,tv.tv_sec,tv.tv_usec); */

	ret = sensor_read(sd, 0x3501, &evl1);
	ret = sensor_read(sd, 0x3502, &evl2);
	ret = sensor_read(sd, 0x3508, &evlgain1);
	ret = sensor_read(sd, 0x3509, &evlgain2);


	if (wdr_en == 1) {
		sensor_max_fps = 15;
		sensor_attr.mipi.clk = 720;
		sensor_attr.mipi.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE;
		sensor_attr.mipi.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE;
		wsize = &sensor_win_sizes[2];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		sensor_attr.wdr_cache = wdr_bufsize;

		sensor_attr.min_integration_time_short = 4;
		sensor_attr.max_integration_time_short = 148;//exposure ratio 16
		sensor_attr.mipi.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE;
		sensor_attr.max_integration_time_native = 2376;
		sensor_attr.integration_time_limit = 2376;
		sensor_attr.total_width = 0x762 * 4;
		sensor_attr.total_height = 0x9e4;
		sensor_attr.max_integration_time = 2376;
		sensor_attr.wdr_cache = wdr_bufsize;

		sensor->video.attr = &sensor_attr;
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = TISP_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else if (wdr_en == 0) {
		sensor_max_fps = 30;
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		wsize = &sensor_win_sizes[1];
		sensor_attr.mipi.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE;
		sensor_attr.mipi.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE;
		sensor_attr.wdr_cache = 0;
		sensor_attr.max_integration_time_native = 0xad9 - 8;
		sensor_attr.integration_time_limit = 0xad9 - 8;
		sensor_attr.total_width = 0x40c * 4; //4144
		sensor_attr.total_height = 0xad9; //2777
		sensor_attr.max_integration_time = 0xad9 - 8;
		sensor_attr.total_width = 0x40c * 4; //4144
		sensor_attr.total_height = 0xad9; //2777

		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = TISP_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		sensor->video.attr = &sensor_attr;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int sensor_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
	int ret = 0;


	ret = sensor_write(sd, 0x0103, 0x01);
	private_msleep(5);

	/* private_gpio_direction_output(reset_gpio, 0); */
	/* private_msleep(5); */
	/* private_gpio_direction_output(reset_gpio, 1); */
	/* private_msleep(5); */

	ret = sensor_write_array(sd, wsize->regs);

	ret = sensor_write(sd, 0x3501, evl1);
	ret = sensor_write(sd, 0x3502, evl2);
	ret = sensor_write(sd, 0x3508, evlgain1);
	ret = sensor_write(sd, 0x3509, evlgain2);

	ret = sensor_write_array(sd, sensor_stream_on_mipi);

	return 0;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = TISP_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned long rate;
	struct clk *sclka;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	int ret;

	pr_debug("boot is %d\n", info->default_boot);

	switch(info->default_boot) {
	case 0:
		wsize = &sensor_win_sizes[0];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.mipi.clk = 640;
		sensor_attr.mipi.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE;
		sensor_attr.mipi.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE;
		sensor_attr.max_integration_time_native = 0x9ee - 8;
		sensor_attr.integration_time_limit = 0x9ee - 8;
		sensor_attr.total_width = 0xa10 * 2; //5152
		sensor_attr.total_height = 0x9ee; //2542
		sensor_attr.max_integration_time = 0x9ee - 8;
	        sensor_attr.again = 0x80;
                sensor_attr.integration_time = 0x901;
		break;
	case 1:
		wsize = &sensor_win_sizes[1];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.mipi.clk = 720;
		sensor_attr.mipi.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE;
		sensor_attr.mipi.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE;
		sensor_attr.max_integration_time_native = 0xad9 - 8;
		sensor_attr.integration_time_limit = 0xad9 - 8;
		sensor_attr.total_width = 0x40c * 4; //4144
		sensor_attr.total_height = 0xad9; //2777
		sensor_attr.max_integration_time = 0xad9 - 8;
	        sensor_attr.again = 0x80;
                sensor_attr.integration_time = 0xa4e;
		break;
	case 2:
		wsize = &sensor_win_sizes[2];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		sensor_attr.mipi.clk = 720;
		sensor_attr.mipi.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE;
		sensor_attr.min_integration_time_short = 4;
		sensor_attr.max_integration_time_short = 148;//exposure ratio 16
		sensor_attr.mipi.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE;
		sensor_attr.max_integration_time_native = 2376;
		sensor_attr.integration_time_limit = 2376;
		sensor_attr.total_width = 0x762 * 4;
		sensor_attr.total_height = 0x9e4;
		sensor_attr.max_integration_time = 2376;
		sensor_attr.wdr_cache = wdr_bufsize;
	        sensor_attr.again = 0x80;
                sensor_attr.integration_time = 0x8e1;
		break;
	case 3:
		wsize = &sensor_win_sizes[3];
		memcpy(&(sensor_attr.mipi), &sensor_mipi_2, sizeof(sensor_mipi_2));
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.max_integration_time_native = 2314 - 8;
		sensor_attr.integration_time_limit = 2314 - 8;
		sensor_attr.total_width = 4144;
		sensor_attr.total_height = 2314;
		sensor_attr.max_integration_time = 2314 - 8;
	        sensor_attr.again = 0x80;
                sensor_attr.integration_time = 0x901;
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	switch(info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor_attr.mipi.index = 0;
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	switch(info->mclk) {
	case TISP_SENSOR_MCLK0:
		sclka = private_devm_clk_get(&client->dev, "mux_cim0");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim0");
		set_sensor_mclk_function(0);
		break;
	case TISP_SENSOR_MCLK1:
		sclka = private_devm_clk_get(&client->dev, "mux_cim1");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim1");
		set_sensor_mclk_function(1);
		break;
	case TISP_SENSOR_MCLK2:
		sclka = private_devm_clk_get(&client->dev, "mux_cim2");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim2");
		set_sensor_mclk_function(2);
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	rate = private_clk_get_rate(sensor->mclk);

	if (MCLK == 27000000) {
		if (((rate / 1000) % 27000) != 0) {
			ret = clk_set_parent(sclka, clk_get(NULL, "epll"));
			sclka = private_devm_clk_get(&client->dev, "epll");
			if (IS_ERR(sclka)) {
				pr_err("get sclka failed\n");
			} else {
				rate = private_clk_get_rate(sclka);
				if (((rate / 1000) % 27000) != 0) {
					private_clk_set_rate(sclka, 891000000);
				}
			}
		}
	}
	private_clk_set_rate(sensor->mclk, MCLK);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	return 0;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
				struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
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
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
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
	struct tx_isp_sensor_value *sensor_val = arg;
	struct tx_isp_initarg *init = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = sensor_set_expo(sd, sensor_val->value);
		break;
		/* case TX_ISP_EVENT_SENSOR_INT_TIME: */
		/* 	if (arg) */
		/* 		ret = sensor_set_integration_time(sd, sensor_val->value); */
		/* 	break; */
		/* case TX_ISP_EVENT_SENSOR_AGAIN: */
		/* 	if (arg) */
		/* 		ret = sensor_set_analog_gain(sd, sensor_val->value); */
		/* 	break; */
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if (arg)
			ret = sensor_set_analog_gain_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if (arg)
			ret = sensor_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sensor_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sensor_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sensor_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sensor_write_array(sd, sensor_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR:
		if (arg)
			ret = sensor_set_wdr(sd, init->enable);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if (arg)
			ret = sensor_set_wdr_stop(sd, init->enable);
		break;
	default:
		break;
	}

	return ret;
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

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->dev = &client->dev;
	sensor_attr.expo_fs = 1;
	sensor->video.attr = &sensor_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
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
