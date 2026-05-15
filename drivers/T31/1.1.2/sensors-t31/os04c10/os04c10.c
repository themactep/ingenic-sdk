/*
 * os04c10.c
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

#define OS04C10_CHIP_ID_H	(0x53)
#define OS04C10_CHIP_ID_M	(0x04)
#define OS04C10_CHIP_ID_L	(0x43)
#define OS04C10_REG_END		0xffff
#define OS04C10_REG_DELAY	0xfffe

#define OS04C10_SUPPORT_SCLK (101046360)
#define OS04C10_SUPPORT_SCLK_WDR (54000000)
#define SENSOR_OUTPUT_MAX_FPS 60
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20200822a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_resolution = TX_SENSOR_RES_400;
module_param(sensor_resolution, int, S_IRUGO);
MODULE_PARM_DESC(sensor_resolution, "Sensor Resolution set interface");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_30;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
module_param(data_type, int, S_IRUGO);
MODULE_PARM_DESC(data_type, "Sensor Date Type");

static int wdr_bufsize = 10077696;//cache lines corrponding on VPB1
module_param(wdr_bufsize, int, S_IRUGO);
MODULE_PARM_DESC(wdr_bufsize, "Wdr Buf Size");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

static unsigned char evl0 = 0;
static unsigned char evl1 = 0;
static unsigned char evl2 = 0;
static unsigned char evs0 = 0;
static unsigned char evs1 = 0;
static unsigned char evs2 = 0;

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	unsigned int value;
	unsigned int gain;
};
struct again_lut os04c10_again_lut[] = {
	{0x80, 0},
	{0x88, 5731},
	{0x90, 11136},
	{0x98, 16248},
	{0xa0, 21097},
	{0xa8, 25710},
	{0xb0, 30109},
	{0xb8, 34312},
	{0xc0, 38336},
	{0xc8, 42195},
	{0xd0, 45904},
	{0xd8, 49472},
	{0xe0, 52910},
	{0xe8, 56228},
	{0xf0, 59433},
	{0xf8, 62534},
	{0x100, 65536},
	{0x110, 71267},
	{0x120, 76672},
	{0x130, 81784},
	{0x140, 86633},
	{0x150, 91246},
	{0x160, 95645},
	{0x170, 99848},
	{0x180, 103872},
	{0x190, 107731},
	{0x1a0, 111440},
	{0x1b0, 115008},
	{0x1c0, 118446},
	{0x1d0, 121764},
	{0x1e0, 124969},
	{0x1f0, 128070},
	{0x200, 131072},
	{0x220, 136803},
	{0x240, 142208},
	{0x260, 147320},
	{0x280, 152169},
	{0x2a0, 156782},
	{0x2c0, 161181},
	{0x2e0, 165384},
	{0x300, 169408},
	{0x320, 173267},
	{0x340, 176976},
	{0x360, 180544},
	{0x380, 183982},
	{0x3a0, 187300},
	{0x3c0, 190505},
	{0x3e0, 193606},
	{0x400, 196608},
	{0x440, 202339},
	{0x480, 207744},
	{0x4c0, 212856},
	{0x500, 217705},
	{0x540, 222318},
	{0x580, 226717},
	{0x5c0, 230920},
	{0x600, 234944},
	{0x640, 238803},
	{0x680, 242512},
	{0x6c0, 246080},
	{0x700, 249518},
	{0x740, 252836},
	{0x780, 256041},
	{0x7c0, 259142},
	/*{0x7ff, 262144},*/
};

struct tx_isp_sensor_attribute os04c10_attr;

unsigned int os04c10_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;

	*sensor_it = expo;

	return it;
}

unsigned int os04c10_alloc_integration_time_short(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;

	*sensor_it = expo;

	return it;
}

unsigned int os04c10_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = os04c10_again_lut;

	while(lut->gain <= os04c10_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == os04c10_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int os04c10_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = os04c10_again_lut;

	while(lut->gain <= os04c10_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else{
			if((lut->gain == os04c10_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int os04c10_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus os04c10_mipi_dol={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1200,
	.lans = 2,
	.settle_time_apative_en = 1,
	.image_twidth = 2560,
	.image_theight = 1440,
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
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_mipi_bus os04c10_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 600,
	.lans = 2,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 2560,
	.image_theight = 1440,
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

struct tx_isp_sensor_attribute os04c10_attr={
	.name = "os04c10",
	.chip_id = 0x530541,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x36,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 700,
		.lans = 2,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.image_twidth = 2560,
		.image_theight = 1440,
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
	},
	.max_again = 259142,
	.max_again_short = 259142,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.min_integration_time_short = 2,
	.max_integration_time_short = 405,
	.max_integration_time_native = 0x626 - 8,
	.integration_time_limit = 0x626 - 8,
	.total_width = 0x85c,
	.total_height = 0x626,
	.max_integration_time = 0x626 - 8,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again_short = os04c10_alloc_again_short,
	.sensor_ctrl.alloc_again = os04c10_alloc_again,
//	.sensor_ctrl.alloc_integration_time = os04c10_alloc_integration_time,
	.sensor_ctrl.alloc_dgain = os04c10_alloc_dgain,
//	.sensor_ctrl.alloc_integration_time_short = os04c10_alloc_integration_time_short,
};

static struct regval_list os04c10_init_regs_2560_1440_30fps[] = {
	{0x0103, 0x01},
	{0x0301, 0x84},
	{0x0303, 0x01},
	{0x0305, 0x3c},
	{0x0306, 0x00},
	{0x0307, 0x17},
	{0x0323, 0x04},
	{0x0324, 0x01},
	{0x0325, 0x62},
	{0x3012, 0x06},
	{0x3013, 0x02},
	{0x3016, 0x32},
	{0x3021, 0x03},
	{0x3106, 0x25},
	{0x3107, 0xa1},
	{0x3500, 0x00},
	{0x3501, 0x02},
	{0x3502, 0x40},/*init integration time*/
	{0x3503, 0x88},
	{0x3508, 0x00},
	{0x3509, 0x80},
	{0x350a, 0x04},
	{0x350b, 0x00},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3510, 0x00},
	{0x3511, 0x00},
	{0x3512, 0x20},
	{0x3624, 0x02},
	{0x3625, 0x4c},
	{0x3660, 0x00},
	{0x3666, 0xa5},
	{0x3667, 0xa5},
	{0x366a, 0x64},
	{0x3673, 0x0d},
	{0x3672, 0x0d},
	{0x3671, 0x0d},
	{0x3670, 0x0d},
	{0x3685, 0x00},
	{0x3694, 0x0d},
	{0x3693, 0x0d},
	{0x3692, 0x0d},
	{0x3691, 0x0d},
	{0x3696, 0x4c},
	{0x3697, 0x4c},
	{0x3698, 0x40},
	{0x3699, 0x80},
	{0x369a, 0x18},
	{0x369b, 0x1f},
	{0x369c, 0x14},
	{0x369d, 0x80},
	{0x369e, 0x40},
	{0x369f, 0x21},
	{0x36a0, 0x12},
	{0x36a1, 0x5d},
	{0x36a2, 0x66},
	{0x370a, 0x00},
	{0x370e, 0x0c},
	{0x3710, 0x00},
	{0x3713, 0x00},
	{0x3725, 0x02},
	{0x372a, 0x03},
	{0x3738, 0xce},
	{0x3739, 0x10},
	{0x3748, 0x00},
	{0x374a, 0x00},
	{0x374c, 0x00},
	{0x374e, 0x00},
	{0x3756, 0x00},
	{0x3757, 0x0e},
	{0x3767, 0x00},
	{0x3771, 0x00},
	{0x377b, 0x20},
	{0x377c, 0x00},
	{0x377d, 0x0c},
	{0x3781, 0x03},
	{0x3782, 0x00},
	{0x3789, 0x14},
	{0x3795, 0x02},
	{0x379c, 0x00},
	{0x379d, 0x00},
	{0x37b8, 0x04},
	{0x37ba, 0x03},
	{0x37bb, 0x00},
	{0x37bc, 0x04},
	{0x37be, 0x08},
	{0x37c4, 0x11},
	{0x37c5, 0x80},
	{0x37c6, 0x14},
	{0x37c7, 0x08},
	{0x37da, 0x11},
	{0x381f, 0x08},
	{0x3829, 0x03},
	{0x3832, 0x00},
	{0x3881, 0x00},
	{0x3888, 0x04},
	{0x388b, 0x00},
	{0x3c80, 0x10},
	{0x3c86, 0x00},
	{0x3c9f, 0x01},
	{0x3d85, 0x1b},
	{0x3d8c, 0x71},
	{0x3d8d, 0xe2},
	{0x3f00, 0x0b},
	{0x3f06, 0x04},
	{0x400a, 0x01},
	{0x400b, 0x50},
	{0x400e, 0x08},
	{0x4040, 0x00},
	{0x4041, 0x07},
	{0x4043, 0x7e},
	{0x4045, 0x7e},
	{0x4047, 0x7e},
	{0x4049, 0x7e},
	{0x4090, 0x14},
	{0x40b0, 0x00},
	{0x40b1, 0x00},
	{0x40b2, 0x00},
	{0x40b3, 0x00},
	{0x40b4, 0x00},
	{0x40b5, 0x00},
	{0x40b7, 0x00},
	{0x40b8, 0x00},
	{0x40b9, 0x00},
	{0x40ba, 0x00},
	{0x4301, 0x00},
	{0x4303, 0x00},
	{0x4502, 0x04},
	{0x4503, 0x00},
	{0x4504, 0x06},
	{0x4506, 0x00},
	{0x4507, 0x64},
	{0x4803, 0x10},
	{0x480c, 0x32},
	{0x480e, 0x00},
	{0x4813, 0x00},
	{0x4819, 0x70},
	{0x481f, 0x30},
	{0x4823, 0x3c},
	{0x4825, 0x32},
	{0x4833, 0x10},
	{0x484b, 0x07},
	{0x488b, 0x00},
	{0x4d00, 0x04},
	{0x4d01, 0xad},
	{0x4d02, 0xbc},
	{0x4d03, 0xa1},
	{0x4d04, 0x1f},
	{0x4d05, 0x4c},
	{0x4d0b, 0x01},
	{0x4e00, 0x2a},
	{0x4e0d, 0x00},
	{0x5001, 0x09},
	{0x5004, 0x00},
	{0x5080, 0x04},
	{0x5036, 0x00},
	{0x5180, 0x70},
	{0x5181, 0x10},
	{0x520a, 0x03},
	{0x520b, 0x06},
	{0x520c, 0x0c},
	{0x580b, 0x0f},
	{0x580d, 0x00},
	{0x580f, 0x00},
	{0x5820, 0x00},
	{0x5821, 0x00},
	{0x301c, 0xf0},
	{0x301e, 0xb4},
	{0x301f, 0xd0},
	{0x3022, 0x01},
	{0x3109, 0xe7},
	{0x3600, 0x00},
	{0x3610, 0x65},
	{0x3611, 0x85},
	{0x3613, 0x3a},
	{0x3615, 0x60},
	{0x3621, 0x90},
	{0x3620, 0x0c},
	{0x3629, 0x00},
	{0x3661, 0x04},
	{0x3662, 0x10},
	{0x3664, 0x70},
	{0x3665, 0x00},
	{0x3681, 0xa6},
	{0x3682, 0x53},
	{0x3683, 0x2a},
	{0x3684, 0x15},
	{0x3700, 0x2a},
	{0x3701, 0x12},
	{0x3703, 0x28},
	{0x3704, 0x0e},
	{0x3706, 0x4a},
	{0x3709, 0x4a},
	{0x370b, 0xa2},
	{0x370c, 0x01},
	{0x370f, 0x04},
	{0x3714, 0x24},
	{0x3716, 0x24},
	{0x3719, 0x11},
	{0x371a, 0x1e},
	{0x3720, 0x00},
	{0x3724, 0x13},
	{0x373f, 0xb0},
	{0x3741, 0x4a},
	{0x3743, 0x4a},
	{0x3745, 0x4a},
	{0x3747, 0x4a},
	{0x3749, 0xa2},
	{0x374b, 0xa2},
	{0x374d, 0xa2},
	{0x374f, 0xa2},
	{0x3755, 0x10},
	{0x376c, 0x00},
	{0x378d, 0x30},
	{0x3790, 0x4a},
	{0x3791, 0xa2},
	{0x3798, 0x40},
	{0x379e, 0x00},
	{0x379f, 0x04},
	{0x37a1, 0x10},
	{0x37a2, 0x1e},
	{0x37a8, 0x10},
	{0x37a9, 0x1e},
	{0x37ac, 0xa0},
	{0x37b9, 0x01},
	{0x37bd, 0x01},
	{0x37bf, 0x26},
	{0x37c0, 0x11},
	{0x37c2, 0x04},
	{0x37cd, 0x19},
	{0x37d8, 0x02},
	{0x37d9, 0x08},
	{0x37e5, 0x02},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x0a},
	{0x3805, 0x8f},
	{0x3806, 0x05},
	{0x3807, 0xff},
	{0x3808, 0x0a},
	{0x3809, 0x00},
	{0x380a, 0x05},
	{0x380b, 0xa0},/*4M 2560*1440*/
	{0x380c, 0x08},
	{0x380d, 0x5c},
	{0x380e, 0x06},
	{0x380f, 0x26},
	{0x3811, 0x08},
	{0x3813, 0x08},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},
	{0x3820, 0x88},
	{0x3821, 0x00},
	{0x3880, 0x25},
	{0x3882, 0x20},
	{0x3c91, 0x0b},
	{0x3c94, 0x45},
	{0x4000, 0xf3},
	{0x4001, 0x60},
	{0x4003, 0x40},
	{0x4008, 0x02},
	{0x4009, 0x0d},
	{0x4300, 0xff},
	{0x4302, 0x0f},
	{0x4305, 0x83},
	{0x4505, 0x84},
	{0x4809, 0x1e},
	{0x480a, 0x04},
	{0x4837, 0x08},
	{0x4c00, 0x08},
	{0x4c01, 0x00},
	{0x4c04, 0x00},
	{0x4c05, 0x00},
	{0x5000, 0xe9},
	{0x0100, 0x00},

	{OS04C10_REG_END, 0x00},/* END MARKER */
};

static struct regval_list os04c10_init_regs_1920_1080_60fps[] = {
	{0x0103, 0x01},
	{0x0301, 0x84},
	{0x0303, 0x01},
	{0x0305, 0x3c},
	{0x0306, 0x00},
	{0x0307, 0x17},
	{0x0323, 0x04},
	{0x0324, 0x01},
	{0x0325, 0x62},
	{0x3012, 0x06},
	{0x3013, 0x02},
	{0x3016, 0x32},
	{0x3021, 0x03},
	{0x3106, 0x25},
	{0x3107, 0xa1},
	{0x3500, 0x00},
	{0x3501, 0x03},
	{0x3502, 0x40},
	{0x3503, 0x88},
	{0x3508, 0x00},
	{0x3509, 0x80},
	{0x350a, 0x04},
	{0x350b, 0x00},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3510, 0x00},
	{0x3511, 0x00},
	{0x3512, 0x20},
	{0x3624, 0x02},
	{0x3625, 0x4c},
	{0x3660, 0x00},
	{0x3666, 0xa5},
	{0x3667, 0xa5},
	{0x366a, 0x64},
	{0x3673, 0x0d},
	{0x3672, 0x0d},
	{0x3671, 0x0d},
	{0x3670, 0x0d},
	{0x3685, 0x00},
	{0x3694, 0x0d},
	{0x3693, 0x0d},
	{0x3692, 0x0d},
	{0x3691, 0x0d},
	{0x3696, 0x4c},
	{0x3697, 0x4c},
	{0x3698, 0x40},
	{0x3699, 0x80},
	{0x369a, 0x18},
	{0x369b, 0x1f},
	{0x369c, 0x14},
	{0x369d, 0x80},
	{0x369e, 0x40},
	{0x369f, 0x21},
	{0x36a0, 0x12},
	{0x36a1, 0x5d},
	{0x36a2, 0x66},
	{0x370a, 0x00},
	{0x370e, 0x0c},
	{0x3710, 0x00},
	{0x3713, 0x00},
	{0x3725, 0x02},
	{0x372a, 0x03},
	{0x3738, 0xce},
	{0x3739, 0x10},
	{0x3748, 0x00},
	{0x374a, 0x00},
	{0x374c, 0x00},
	{0x374e, 0x00},
	{0x3756, 0x00},
	{0x3757, 0x0e},
	{0x3767, 0x00},
	{0x3771, 0x00},
	{0x377b, 0x20},
	{0x377c, 0x00},
	{0x377d, 0x0c},
	{0x3781, 0x03},
	{0x3782, 0x00},
	{0x3789, 0x14},
	{0x3795, 0x02},
	{0x379c, 0x00},
	{0x379d, 0x00},
	{0x37b8, 0x04},
	{0x37ba, 0x03},
	{0x37bb, 0x00},
	{0x37bc, 0x04},
	{0x37be, 0x08},
	{0x37c4, 0x11},
	{0x37c5, 0x80},
	{0x37c6, 0x14},
	{0x37c7, 0x08},
	{0x37da, 0x11},
	{0x381f, 0x08},
	{0x3829, 0x03},
	{0x3832, 0x00},
	{0x3881, 0x00},
	{0x3888, 0x04},
	{0x388b, 0x00},
	{0x3c80, 0x10},
	{0x3c86, 0x00},
	{0x3c9f, 0x01},
	{0x3d85, 0x1b},
	{0x3d8c, 0x71},
	{0x3d8d, 0xe2},
	{0x3f00, 0x0b},
	{0x3f06, 0x04},
	{0x400a, 0x01},
	{0x400b, 0x50},
	{0x400e, 0x08},
	{0x4040, 0x00},
	{0x4041, 0x07},
	{0x4043, 0x7e},
	{0x4045, 0x7e},
	{0x4047, 0x7e},
	{0x4049, 0x7e},
	{0x4090, 0x14},
	{0x40b0, 0x00},
	{0x40b1, 0x00},
	{0x40b2, 0x00},
	{0x40b3, 0x00},
	{0x40b4, 0x00},
	{0x40b5, 0x00},
	{0x40b7, 0x00},
	{0x40b8, 0x00},
	{0x40b9, 0x00},
	{0x40ba, 0x00},
	{0x4301, 0x00},
	{0x4303, 0x00},
	{0x4502, 0x04},
	{0x4503, 0x00},
	{0x4504, 0x06},
	{0x4506, 0x00},
	{0x4507, 0x64},
	{0x4803, 0x10},
	{0x480c, 0x32},
	{0x480e, 0x00},
	{0x4813, 0x00},
	{0x4819, 0x70},
	{0x481f, 0x30},
	{0x4823, 0x3c},
	{0x4825, 0x32},
	{0x4833, 0x10},
	{0x484b, 0x07},
	{0x488b, 0x00},
	{0x4d00, 0x04},
	{0x4d01, 0xad},
	{0x4d02, 0xbc},
	{0x4d03, 0xa1},
	{0x4d04, 0x1f},
	{0x4d05, 0x4c},
	{0x4d0b, 0x01},
	{0x4e00, 0x2a},
	{0x4e0d, 0x00},
	{0x5001, 0x09},
	{0x5004, 0x00},
	{0x5080, 0x04},
	{0x5036, 0x00},
	{0x5180, 0x70},
	{0x5181, 0x10},
	{0x520a, 0x03},
	{0x520b, 0x06},
	{0x520c, 0x0c},
	{0x580b, 0x0f},
	{0x580d, 0x00},
	{0x580f, 0x00},
	{0x5820, 0x00},
	{0x5821, 0x00},
	{0x301c, 0xf0},
	{0x301e, 0xb4},
	{0x301f, 0xd0},
	{0x3022, 0x01},
	{0x3109, 0xe7},
	{0x3600, 0x00},
	{0x3610, 0x65},
	{0x3611, 0x85},
	{0x3613, 0x3a},
	{0x3615, 0x60},
	{0x3621, 0x90},
	{0x3620, 0x0c},
	{0x3629, 0x00},
	{0x3661, 0x04},
	{0x3662, 0x10},
	{0x3664, 0x70},
	{0x3665, 0x00},
	{0x3681, 0xa6},
	{0x3682, 0x53},
	{0x3683, 0x2a},
	{0x3684, 0x15},
	{0x3700, 0x2a},
	{0x3701, 0x12},
	{0x3703, 0x28},
	{0x3704, 0x0e},
	{0x3706, 0x4a},
	{0x3709, 0x4a},
	{0x370b, 0xa2},
	{0x370c, 0x01},
	{0x370f, 0x04},
	{0x3714, 0x24},
	{0x3716, 0x24},
	{0x3719, 0x11},
	{0x371a, 0x1e},
	{0x3720, 0x00},
	{0x3724, 0x13},
	{0x373f, 0xb0},
	{0x3741, 0x4a},
	{0x3743, 0x4a},
	{0x3745, 0x4a},
	{0x3747, 0x4a},
	{0x3749, 0xa2},
	{0x374b, 0xa2},
	{0x374d, 0xa2},
	{0x374f, 0xa2},
	{0x3755, 0x10},
	{0x376c, 0x00},
	{0x378d, 0x30},
	{0x3790, 0x4a},
	{0x3791, 0xa2},
	{0x3798, 0x40},
	{0x379e, 0x00},
	{0x379f, 0x04},
	{0x37a1, 0x10},
	{0x37a2, 0x1e},
	{0x37a8, 0x10},
	{0x37a9, 0x1e},
	{0x37ac, 0xa0},
	{0x37b9, 0x01},
	{0x37bd, 0x01},
	{0x37bf, 0x26},
	{0x37c0, 0x11},
	{0x37c2, 0x04},
	{0x37cd, 0x19},
	{0x37d8, 0x02},
	{0x37d9, 0x08},
	{0x37e5, 0x02},
	{0x3800, 0x01},
	{0x3801, 0x80},
	{0x3802, 0x00},
	{0x3803, 0xdc},
	{0x3804, 0x09},
	{0x3805, 0x0f},
	{0x3806, 0x05},
	{0x3807, 0x23},
	{0x3808, 0x07},
	{0x3809, 0x80},
	{0x380a, 0x04},
	{0x380b, 0x38},
	{0x380c, 0x05},
	{0x380d, 0x92},
	{0x380e, 0x04},
	{0x380f, 0x9d},
	{0x3811, 0x08},
	{0x3813, 0x08},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},
	{0x3820, 0x88},
	{0x3821, 0x00},
	{0x3880, 0x25},
	{0x3882, 0x20},
	{0x3c91, 0x0b},
	{0x3c94, 0x45},
	{0x4000, 0xf3},
	{0x4001, 0x60},
	{0x4003, 0x40},
	{0x4008, 0x02},
	{0x4009, 0x0d},
	{0x4300, 0xff},
	{0x4302, 0x0f},
	{0x4305, 0x83},
	{0x4505, 0x84},
	{0x4809, 0x1e},
	{0x480a, 0x04},
	{0x4837, 0x08},
	{0x4c00, 0x08},
	{0x4c01, 0x00},
	{0x4c04, 0x00},
	{0x4c05, 0x00},
	{0x5000, 0xe9},
	{0x0100, 0x00},

	{OS04C10_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list os04c10_init_regs_1920_1080_30fps[] = {
	{0x0103, 0x01},
	{0x0301, 0x84},
	{0x0303, 0x01},
	{0x0305, 0x3c},
	{0x0306, 0x00},
	{0x0307, 0x17},
	{0x0323, 0x04},
	{0x0324, 0x01},
	{0x0325, 0x62},
	{0x3012, 0x06},
	{0x3013, 0x02},
	{0x3016, 0x32},
	{0x3021, 0x03},
	{0x3106, 0x25},
	{0x3107, 0xa1},
	{0x3500, 0x00},
	{0x3501, 0x03},
	{0x3502, 0x40},
	{0x3503, 0x88},
	{0x3508, 0x00},
	{0x3509, 0x80},
	{0x350a, 0x04},
	{0x350b, 0x00},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3510, 0x00},
	{0x3511, 0x00},
	{0x3512, 0x20},
	{0x3624, 0x02},
	{0x3625, 0x4c},
	{0x3660, 0x00},
	{0x3666, 0xa5},
	{0x3667, 0xa5},
	{0x366a, 0x64},
	{0x3673, 0x0d},
	{0x3672, 0x0d},
	{0x3671, 0x0d},
	{0x3670, 0x0d},
	{0x3685, 0x00},
	{0x3694, 0x0d},
	{0x3693, 0x0d},
	{0x3692, 0x0d},
	{0x3691, 0x0d},
	{0x3696, 0x4c},
	{0x3697, 0x4c},
	{0x3698, 0x40},
	{0x3699, 0x80},
	{0x369a, 0x18},
	{0x369b, 0x1f},
	{0x369c, 0x14},
	{0x369d, 0x80},
	{0x369e, 0x40},
	{0x369f, 0x21},
	{0x36a0, 0x12},
	{0x36a1, 0x5d},
	{0x36a2, 0x66},
	{0x370a, 0x00},
	{0x370e, 0x0c},
	{0x3710, 0x00},
	{0x3713, 0x00},
	{0x3725, 0x02},
	{0x372a, 0x03},
	{0x3738, 0xce},
	{0x3739, 0x10},
	{0x3748, 0x00},
	{0x374a, 0x00},
	{0x374c, 0x00},
	{0x374e, 0x00},
	{0x3756, 0x00},
	{0x3757, 0x0e},
	{0x3767, 0x00},
	{0x3771, 0x00},
	{0x377b, 0x20},
	{0x377c, 0x00},
	{0x377d, 0x0c},
	{0x3781, 0x03},
	{0x3782, 0x00},
	{0x3789, 0x14},
	{0x3795, 0x02},
	{0x379c, 0x00},
	{0x379d, 0x00},
	{0x37b8, 0x04},
	{0x37ba, 0x03},
	{0x37bb, 0x00},
	{0x37bc, 0x04},
	{0x37be, 0x08},
	{0x37c4, 0x11},
	{0x37c5, 0x80},
	{0x37c6, 0x14},
	{0x37c7, 0x08},
	{0x37da, 0x11},
	{0x381f, 0x08},
	{0x3829, 0x03},
	{0x3832, 0x00},
	{0x3881, 0x00},
	{0x3888, 0x04},
	{0x388b, 0x00},
	{0x3c80, 0x10},
	{0x3c86, 0x00},
	{0x3c9f, 0x01},
	{0x3d85, 0x1b},
	{0x3d8c, 0x71},
	{0x3d8d, 0xe2},
	{0x3f00, 0x0b},
	{0x3f06, 0x04},
	{0x400a, 0x01},
	{0x400b, 0x50},
	{0x400e, 0x08},
	{0x4040, 0x00},
	{0x4041, 0x07},
	{0x4043, 0x7e},
	{0x4045, 0x7e},
	{0x4047, 0x7e},
	{0x4049, 0x7e},
	{0x4090, 0x14},
	{0x40b0, 0x00},
	{0x40b1, 0x00},
	{0x40b2, 0x00},
	{0x40b3, 0x00},
	{0x40b4, 0x00},
	{0x40b5, 0x00},
	{0x40b7, 0x00},
	{0x40b8, 0x00},
	{0x40b9, 0x00},
	{0x40ba, 0x00},
	{0x4301, 0x00},
	{0x4303, 0x00},
	{0x4502, 0x04},
	{0x4503, 0x00},
	{0x4504, 0x06},
	{0x4506, 0x00},
	{0x4507, 0x64},
	{0x4803, 0x10},
	{0x480c, 0x32},
	{0x480e, 0x00},
	{0x4813, 0x00},
	{0x4819, 0x70},
	{0x481f, 0x30},
	{0x4823, 0x3c},
	{0x4825, 0x32},
	{0x4833, 0x10},
	{0x484b, 0x07},
	{0x488b, 0x00},
	{0x4d00, 0x04},
	{0x4d01, 0xad},
	{0x4d02, 0xbc},
	{0x4d03, 0xa1},
	{0x4d04, 0x1f},
	{0x4d05, 0x4c},
	{0x4d0b, 0x01},
	{0x4e00, 0x2a},
	{0x4e0d, 0x00},
	{0x5001, 0x09},
	{0x5004, 0x00},
	{0x5080, 0x04},
	{0x5036, 0x00},
	{0x5180, 0x70},
	{0x5181, 0x10},
	{0x520a, 0x03},
	{0x520b, 0x06},
	{0x520c, 0x0c},
	{0x580b, 0x0f},
	{0x580d, 0x00},
	{0x580f, 0x00},
	{0x5820, 0x00},
	{0x5821, 0x00},
	{0x301c, 0xf0},
	{0x301e, 0xb4},
	{0x301f, 0xd0},
	{0x3022, 0x01},
	{0x3109, 0xe7},
	{0x3600, 0x00},
	{0x3610, 0x65},
	{0x3611, 0x85},
	{0x3613, 0x3a},
	{0x3615, 0x60},
	{0x3621, 0x90},
	{0x3620, 0x0c},
	{0x3629, 0x00},
	{0x3661, 0x04},
	{0x3662, 0x10},
	{0x3664, 0x70},
	{0x3665, 0x00},
	{0x3681, 0xa6},
	{0x3682, 0x53},
	{0x3683, 0x2a},
	{0x3684, 0x15},
	{0x3700, 0x2a},
	{0x3701, 0x12},
	{0x3703, 0x28},
	{0x3704, 0x0e},
	{0x3706, 0x4a},
	{0x3709, 0x4a},
	{0x370b, 0xa2},
	{0x370c, 0x01},
	{0x370f, 0x04},
	{0x3714, 0x24},
	{0x3716, 0x24},
	{0x3719, 0x11},
	{0x371a, 0x1e},
	{0x3720, 0x00},
	{0x3724, 0x13},
	{0x373f, 0xb0},
	{0x3741, 0x4a},
	{0x3743, 0x4a},
	{0x3745, 0x4a},
	{0x3747, 0x4a},
	{0x3749, 0xa2},
	{0x374b, 0xa2},
	{0x374d, 0xa2},
	{0x374f, 0xa2},
	{0x3755, 0x10},
	{0x376c, 0x00},
	{0x378d, 0x30},
	{0x3790, 0x4a},
	{0x3791, 0xa2},
	{0x3798, 0x40},
	{0x379e, 0x00},
	{0x379f, 0x04},
	{0x37a1, 0x10},
	{0x37a2, 0x1e},
	{0x37a8, 0x10},
	{0x37a9, 0x1e},
	{0x37ac, 0xa0},
	{0x37b9, 0x01},
	{0x37bd, 0x01},
	{0x37bf, 0x26},
	{0x37c0, 0x11},
	{0x37c2, 0x04},
	{0x37cd, 0x19},
	{0x37d8, 0x02},
	{0x37d9, 0x08},
	{0x37e5, 0x02},
	{0x3800, 0x01},
	{0x3801, 0x80},
	{0x3802, 0x00},
	{0x3803, 0xdc},
	{0x3804, 0x09},
	{0x3805, 0x0f},
	{0x3806, 0x05},
	{0x3807, 0x23},
	{0x3808, 0x07},
	{0x3809, 0x80},
	{0x380a, 0x04},
	{0x380b, 0x38},
	{0x380c, 0x05},// ;04
	{0x380d, 0x92},// ;2e
	{0x380e, 0x09},// ;05 ;04
	{0x380f, 0x3a},// ;89 ;9d
	{0x3811, 0x08},
	{0x3813, 0x08},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},
	{0x3820, 0x88},
	{0x3821, 0x00},
	{0x3880, 0x25},
	{0x3882, 0x20},
	{0x3c91, 0x0b},
	{0x3c94, 0x45},
	{0x4000, 0xf3},
	{0x4001, 0x60},
	{0x4003, 0x40},
	{0x4008, 0x02},
	{0x4009, 0x0d},
	{0x4300, 0xff},
	{0x4302, 0x0f},
	{0x4305, 0x83},
	{0x4505, 0x84},
	{0x4809, 0x1e},
	{0x480a, 0x04},
	{0x4837, 0x08},// ;0a
	{0x4c00, 0x08},
	{0x4c01, 0x00},
	{0x4c04, 0x00},
	{0x4c05, 0x00},
	{0x5000, 0xe9},
	{0x0100, 0x00},

	{OS04C10_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list os04c10_init_regs_2560_1440_15fps_dol[] = {
	{OS04C10_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the os04c10_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting os04c10_win_sizes[] = {
	/* [0] 5M @30fps*/
	{
		.width		= 2560,
		.height		= 1440,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= os04c10_init_regs_2560_1440_30fps,
	},
	/* [1] 2M @60fps*/
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 60 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= os04c10_init_regs_1920_1080_60fps,
	},
	/* [2] 5M @15fps*/
	{
		.width		= 2560,
		.height		= 1440,
		.fps		= 15 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= os04c10_init_regs_2560_1440_15fps_dol,
	},
	/* [3] 2M @30fps*/
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= os04c10_init_regs_1920_1080_30fps,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &os04c10_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list os04c10_stream_on[] = {
	{0x0100, 0x01},
	{OS04C10_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list os04c10_stream_off[] = {
	{0x0100, 0x00},
	{OS04C10_REG_END, 0x00},	/* END MARKER */
};

int os04c10_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int os04c10_write(struct tx_isp_subdev *sd, uint16_t reg,
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

static int os04c10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != OS04C10_REG_END) {
		if (vals->reg_num == OS04C10_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = os04c10_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%02x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}
	return 0;
}
static int os04c10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != OS04C10_REG_END) {
		if (vals->reg_num == OS04C10_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = os04c10_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int os04c10_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int os04c10_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = os04c10_read(sd, 0x300a, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS04C10_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = os04c10_read(sd, 0x300b, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS04C10_CHIP_ID_M)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	ret = os04c10_read(sd, 0x300c, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS04C10_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 16) | v;
	return 0;
}

static int os04c10_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int expo = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	ret = os04c10_write(sd, 0x3208, 0x00);
	/*expo*/
	ret += os04c10_write(sd, 0x3502, (unsigned char)(expo & 0xff));
	ret += os04c10_write(sd, 0x3501, (unsigned char)((expo >> 8) & 0xff));
	ret += os04c10_write(sd, 0x3500, (unsigned char)((expo >> 16) & 0xff));
	/*gain*/
	ret += os04c10_write(sd, 0x3509, (unsigned char)((again & 0xff)));
	ret += os04c10_write(sd, 0x3508, (unsigned char)((again >> 8) & 0xff));
	ret += os04c10_write(sd, 0x3208, 0x10);
	ret += os04c10_write(sd, 0x320d, 0x00);//manual laucch on
	ret += os04c10_write(sd, 0x3208, 0xe0);//quick launch
	if (ret < 0)
		return ret;

	return 0;
}
static int os04c10_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;

	ret += os04c10_write(sd, 0x3502, (unsigned char)(expo & 0xff));
	ret += os04c10_write(sd, 0x3501, (unsigned char)((expo >> 8) & 0xff));
	ret += os04c10_write(sd, 0x3500, (unsigned char)((expo >> 16) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int os04c10_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;

	ret += os04c10_write(sd, 0x3512, (unsigned char)(expo & 0xff));
	ret += os04c10_write(sd, 0x3511, (unsigned char)((expo >> 8) & 0xff));
	ret += os04c10_write(sd, 0x3510, (unsigned char)((expo >> 16) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int os04c10_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += os04c10_write(sd, 0x3509, (unsigned char)((value & 0xff)));
	ret += os04c10_write(sd, 0x3508, (unsigned char)((value >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int os04c10_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int os04c10_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += os04c10_write(sd, 0x350d, (unsigned char)((value & 0xff)));
	ret += os04c10_write(sd, 0x350c, (unsigned char)((value >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int os04c10_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int os04c10_init(struct tx_isp_subdev *sd, int enable)
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
	ret = os04c10_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int os04c10_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = os04c10_write_array(sd, os04c10_stream_on);
		pr_debug("os04c10 stream on\n");
	}
	else {
		ret = os04c10_write_array(sd, os04c10_stream_off);
		pr_debug("os04c10 stream off\n");
	}
	return ret;
}

static int os04c10_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0;

	if(data_type == TX_SENSOR_DATA_TYPE_LINEAR){
		if(sensor_resolution == TX_SENSOR_RES_400){
			switch (sensor_max_fps) {
			case TX_SENSOR_MAX_FPS_30:
				sclk = OS04C10_SUPPORT_SCLK;
				max_fps = TX_SENSOR_MAX_FPS_30;
				break;
			default:
				ISP_ERROR("Now we do not support this framerate @4mp!!!\n");
			}
		} else if(sensor_resolution == TX_SENSOR_RES_200){
			switch (sensor_max_fps) {
			case TX_SENSOR_MAX_FPS_30:
				sclk = OS04C10_SUPPORT_SCLK;
				max_fps = TX_SENSOR_MAX_FPS_30;
				break;
			case TX_SENSOR_MAX_FPS_60:
				sclk = OS04C10_SUPPORT_SCLK;
				max_fps = TX_SENSOR_MAX_FPS_60;
				break;
			default:
				ISP_ERROR("Now we do not support this framerate @2mp!!!\n");
			}
		}
	} else if(data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) {
		sclk = OS04C10_SUPPORT_SCLK_WDR;
		max_fps = TX_SENSOR_MAX_FPS_15;
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}

	ret += os04c10_read(sd, 0x380c, &val);
	hts = val<<8;
	val = 0;
	ret += os04c10_read(sd, 0x380d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: os04c10 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = os04c10_write(sd, 0x3208, 0x02);
	ret += os04c10_write(sd, 0x380f, vts & 0xff);
	ret += os04c10_write(sd, 0x380e, (vts >> 8) & 0xff);
	ret += os04c10_write(sd, 0x3208, 0x12);
	ret += os04c10_write(sd, 0x320d, 0x00);
	ret += os04c10_write(sd, 0x3208, 0xe2);
	if (0 != ret) {
		ISP_ERROR("err: os04c10_write err\n");
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

static int os04c10_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
	int ret = 0;

	return 0;
	ISP_WARNING("%s:%d,wdr_en=%d\n",__func__,__LINE__,wdr_en);
	ret = os04c10_write(sd, 0x0103, 0x01);
	private_msleep(5);

	ret = os04c10_write_array(sd, wsize->regs);
	ret = os04c10_write(sd, 0x3500, evl0);
	ret = os04c10_write(sd, 0x3501, evl1);
	ret = os04c10_write(sd, 0x3502, evl2);
	ret = os04c10_write(sd, 0x3510, evs0);
	ret = os04c10_write(sd, 0x3511, evs1);
	ret = os04c10_write(sd, 0x3512, evs2);
	ret = os04c10_write_array(sd, os04c10_stream_on);

	return 0;
}

static int os04c10_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = 0;

	return 0;
	ret = os04c10_read(sd, 0x3500, &evl0);
	ret = os04c10_read(sd, 0x3501, &evl1);
	ret = os04c10_read(sd, 0x3502, &evl2);

	ret = os04c10_read(sd, 0x3510, &evs0);
	ret = os04c10_read(sd, 0x3511, &evs1);
	ret = os04c10_read(sd, 0x3512, &evs2);

//	ret = os04c10_write(sd, 0x12, 0x40);
	if(wdr_en == 1){
		wsize = &os04c10_win_sizes[2];
		sensor->video.vi_max_width = wsize->width;
		sensor->video.vi_max_height = wsize->height;
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		memcpy((void*)(&(os04c10_attr.mipi)),(void*)(&os04c10_mipi_dol),sizeof(os04c10_mipi_dol));
		data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		os04c10_attr.data_type = data_type;
		os04c10_attr.one_line_expr_in_us = 27;

		os04c10_attr.max_again = 259142;
		os04c10_attr.max_again_short = 259142;
		os04c10_attr.max_dgain = 0;
		os04c10_attr.min_integration_time = 2;
		os04c10_attr.min_integration_time_short = 4;
		os04c10_attr.max_integration_time_short = 147;//exposure ratio 16
		os04c10_attr.max_integration_time_native = 2345;
		os04c10_attr.integration_time_limit = 2345;
		os04c10_attr.total_width = 0x5a0 * 2;
		os04c10_attr.total_height = 0x9c4;
		os04c10_attr.max_integration_time = 2345;
		os04c10_attr.wdr_cache = wdr_bufsize;
		os04c10_attr.mipi.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10;

		sensor->video.attr = &os04c10_attr;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else if (wdr_en == 0){
		wsize = &os04c10_win_sizes[0];
		sensor->video.vi_max_width = wsize->width;
		sensor->video.vi_max_height = wsize->height;
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		sensor_max_fps = TX_SENSOR_MAX_FPS_25;
		memcpy((void*)(&(os04c10_attr.mipi)),(void*)(&os04c10_mipi),sizeof(os04c10_mipi));
		data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		os04c10_attr.data_type = data_type;
		os04c10_attr.one_line_expr_in_us = 19;
		os04c10_attr.data_type = data_type;
		os04c10_attr.max_integration_time_native = 0x626 - 8;
		os04c10_attr.integration_time_limit = 0x626 - 8;
		os04c10_attr.total_width = 0x85c * 2;
		os04c10_attr.total_height = 0x626;
		os04c10_attr.max_integration_time = 0x626 - 8;
		os04c10_attr.mipi.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10;

		sensor->video.attr = &os04c10_attr;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int os04c10_set_mode(struct tx_isp_subdev *sd, int value)
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

static int os04c10_g_chip_ident(struct tx_isp_subdev *sd,
				struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"os04c10_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(15);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"os04c10_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = os04c10_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an os04c10 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("os04c10 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "os04c10", sizeof("os04c10"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int os04c10_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = os04c10_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		//if(arg)
			//ret = os04c10_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = os04c10_set_integration_time_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		//if(arg)
			//ret = os04c10_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = os04c10_set_analog_gain_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = os04c10_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = os04c10_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = os04c10_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if(arg)
			ret = os04c10_write_array(sd, os04c10_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if(arg)
			ret = os04c10_write_array(sd, os04c10_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = os04c10_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = os04c10_set_wdr(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = os04c10_set_wdr_stop(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int os04c10_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = os04c10_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int os04c10_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	os04c10_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}

static struct tx_isp_subdev_core_ops os04c10_core_ops = {
	.g_chip_ident = os04c10_g_chip_ident,
	.reset = os04c10_reset,
	.init = os04c10_init,
	/*.ioctl = os04c10_ops_ioctl,*/
	.g_register = os04c10_g_register,
	.s_register = os04c10_s_register,
};

static struct tx_isp_subdev_video_ops os04c10_video_ops = {
	.s_stream = os04c10_s_stream,
};

static struct tx_isp_subdev_sensor_ops	os04c10_sensor_ops = {
	.ioctl	= os04c10_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops os04c10_ops = {
	.core = &os04c10_core_ops,
	.video = &os04c10_video_ops,
	.sensor = &os04c10_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "os04c10",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int os04c10_probe(struct i2c_client *client,
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
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	if(data_type == TX_SENSOR_DATA_TYPE_LINEAR){
		if(sensor_resolution == TX_SENSOR_RES_400){
			switch (sensor_max_fps) {
			case TX_SENSOR_MAX_FPS_30:
				wsize = &os04c10_win_sizes[0];
				os04c10_attr.max_integration_time_native = 0x626 - 8;
				os04c10_attr.integration_time_limit = 0x626 - 8;
				os04c10_attr.total_width = 0x85c * 2;
				os04c10_attr.total_height = 0x626;
				os04c10_attr.max_integration_time = 0x626 - 8;
				os04c10_attr.mipi.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10;
				break;
			default:
				ISP_ERROR("Now we do not support this framerate @ 4MP!!!\n");
			}
		} else if (sensor_resolution == TX_SENSOR_RES_200){
			switch (sensor_max_fps) {
			case TX_SENSOR_MAX_FPS_60:
				wsize = &os04c10_win_sizes[1];
				os04c10_attr.mipi.image_twidth = 1920;
				os04c10_attr.mipi.image_theight = 1080;
				os04c10_attr.max_integration_time_native = 0x49d - 8;
				os04c10_attr.integration_time_limit = 0x49d - 8;
				os04c10_attr.total_width = 0x592 * 2;
				os04c10_attr.total_height = 0x49d;
				os04c10_attr.max_integration_time = 0x49d - 8;
				os04c10_attr.mipi.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10;
				break;
			case TX_SENSOR_MAX_FPS_30:
				wsize = &os04c10_win_sizes[3];
				os04c10_attr.mipi.image_twidth = 1920;
				os04c10_attr.mipi.image_theight = 1080;
				os04c10_attr.max_integration_time_native = 0x93a - 8;
				os04c10_attr.integration_time_limit = 0x93a - 8;
				os04c10_attr.total_width = 0x592 * 2;
				os04c10_attr.total_height = 0x93a;
				os04c10_attr.max_integration_time = 0x93a - 8;
				os04c10_attr.mipi.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10;
				break;
			default:
				ISP_ERROR("Now we do not support this framerate!!!\n");
			}
		}
	} else if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL){
		wsize = &os04c10_win_sizes[2];
		memcpy((void*)(&(os04c10_attr.mipi)),(void*)(&os04c10_mipi_dol),sizeof(os04c10_mipi_dol));
		os04c10_attr.max_again = 259142;
		os04c10_attr.max_again_short = 259142;
		os04c10_attr.max_dgain = 0;
		os04c10_attr.min_integration_time = 2;
		os04c10_attr.min_integration_time_short = 4;
		os04c10_attr.max_integration_time_short = 147;//exposure ratio 16
		os04c10_attr.max_integration_time_native = 2345;
		os04c10_attr.integration_time_limit = 2345;
		os04c10_attr.total_width = 0x5a0 * 2;
		os04c10_attr.total_height = 0x9c4;
		os04c10_attr.max_integration_time = 2345;
		os04c10_attr.wdr_cache = wdr_bufsize;
		os04c10_attr.mipi.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10;
	}

	os04c10_attr.data_type = data_type;
	os04c10_attr.max_again = 259142;
	os04c10_attr.max_dgain = 0;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &os04c10_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &os04c10_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->os04c10\n");
	return 0;
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int os04c10_remove(struct i2c_client *client)
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

static const struct i2c_device_id os04c10_id[] = {
	{ "os04c10", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, os04c10_id);

static struct i2c_driver os04c10_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "os04c10",
	},
	.probe		= os04c10_probe,
	.remove		= os04c10_remove,
	.id_table	= os04c10_id,
};

static __init int init_os04c10(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init os04c10 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&os04c10_driver);
}

static __exit void exit_os04c10(void)
{
	private_i2c_del_driver(&os04c10_driver);
}

module_init(init_os04c10);
module_exit(exit_os04c10);

MODULE_DESCRIPTION("A low-level driver for OmniVision os04c10 sensors");
MODULE_LICENSE("GPL");
