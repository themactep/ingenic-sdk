/*
 * ov8856.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * Settings:
 * sboot        resolution      fps     interface             mode
 *   0          1920*1080        30     mipi_2lane           linear
 *   1          640*480          30     mipi_2lane           linear
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

#define OV8856_CHIP_ID_H	(0x88)
#define OV8856_CHIP_ID_L	(0x5a)
#define OV8856_REG_END		0xffff
#define OV8856_REG_DELAY	0xfffe
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20230512a"

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
MODULE_PARM_DESC(sensor_resolution, "Sensor Max Fps set interface");

static int shvflip = 1;
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

struct again_lut ov8856_again_lut[] = {
	{0x80, 0},
	{0x88, 5687},
	{0x90, 11136},
	{0x98, 16208},
	{0xa0, 21097},
	{0xa8, 25674},
	{0xb0, 30109},
	{0xb8, 34279},
	{0xc0, 38336},
	{0xc8, 42165},
	{0xd0, 45904},
	{0xd8, 49444},
	{0xe0, 52910},
	{0xe8, 56202},
	{0xf0, 59433},
	{0xf8, 62509},
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
};

struct tx_isp_sensor_attribute ov8856_attr;

unsigned int ov8856_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = ov8856_again_lut;
	while(lut->gain <= ov8856_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == ov8856_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int ov8856_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus ov8856_mipi_1080P={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 740,
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

struct tx_isp_mipi_bus ov8856_mipi_480P={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 360,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.image_twidth = 640,
	.image_theight = 480,
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

struct tx_isp_sensor_attribute ov8856_attr={
	.name = "ov8856",
	.chip_id = 0x885a,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x36,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 259142,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = ov8856_alloc_again,
	.sensor_ctrl.alloc_dgain = ov8856_alloc_dgain,
};

static struct regval_list ov8856_init_regs_1920_1080_30fps_mipi[] = {
	{0x0100,0x00},
	{0x0302,0x20},
	{0x0303,0x00},
	{0x031e,0x0c},
	{0x3000,0x00},
	{0x300e,0x00},
	{0x3010,0x00},
	{0x3015,0x84},
	{0x3018,0x32},
	{0x3021,0x23},
	{0x3033,0x24},
	{0x3500,0x00},
	{0x3501,0x4c},
	{0x3502,0xa0},
	{0x3503,0x08},
	{0x3505,0x83},
	{0x3508,0x01},
	{0x3509,0x80},
	{0x350c,0x00},
	{0x350d,0x80},
	{0x350e,0x04},
	{0x350f,0x00},
	{0x3510,0x00},
	{0x3511,0x02},
	{0x3512,0x00},
	{0x3600,0x72},
	{0x3601,0x40},
	{0x3602,0x30},
	{0x3610,0xc5},
	{0x3611,0x58},
	{0x3612,0x5c},
	{0x3613,0xca},
	{0x3614,0x20},
	{0x3628,0xff},
	{0x3629,0xff},
	{0x362a,0xff},
	{0x3633,0x10},
	{0x3634,0x10},
	{0x3635,0x10},
	{0x3636,0x10},
	{0x3663,0x08},
	{0x3669,0x34},
	{0x366e,0x10},
	{0x3706,0x86},
	{0x370b,0x7e},
	{0x3714,0x23},
	{0x3730,0x12},
	{0x3733,0x10},
	{0x3764,0x00},
	{0x3765,0x00},
	{0x3769,0x62},
	{0x376a,0x2a},
	{0x376b,0x30},
	{0x3780,0x00},
	{0x3781,0x24},
	{0x3782,0x00},
	{0x3783,0x23},
	{0x3798,0x2f},
	{0x37a1,0x60},
	{0x37a8,0x6a},
	{0x37ab,0x3f},
	{0x37c2,0x04},
	{0x37c3,0xf1},
	{0x37c9,0x80},
	{0x37cb,0x16},
	{0x37cc,0x16},
	{0x37cd,0x16},
	{0x37ce,0x16},
	{0x3800,0x02},
	{0x3801,0xa0},
	{0x3802,0x02},
	{0x3803,0xb8},
	{0x3804,0x0a},
	{0x3805,0x3f},
	{0x3806,0x06},
	{0x3807,0xf7},
	{0x3808,0x07},
	{0x3809,0x80},
	{0x380a,0x04},
	{0x380b,0x38},
	{0x380c,0x07},//hts -> 0x78c = 1932
	{0x380d,0x8c},//
	{0x380e,0x09},//vts -> 0x9c0 = 2496
	{0x380f,0xc0},//
	{0x3810,0x00},
	{0x3811,0x10},
	{0x3812,0x00},
	{0x3813,0x04},
	{0x3814,0x01},
	{0x3815,0x01},
	{0x3816,0x00},
	{0x3817,0x00},
	{0x3818,0x00},
	{0x3819,0x00},
	{0x3820,0x80},
	{0x3821,0x46},
	{0x382a,0x01},
	{0x382b,0x01},
	{0x3830,0x06},
	{0x3836,0x02},
	{0x3862,0x04},
	{0x3863,0x08},
	{0x3cc0,0x33},
	{0x3d85,0x17},
	{0x3d8c,0x73},
	{0x3d8d,0xde},
	{0x4001,0xe0},
	{0x4003,0x40},
	{0x4008,0x00},
	{0x4009,0x0b},
	{0x400a,0x00},
	{0x400b,0x84},
	{0x400f,0x80},
	{0x4010,0xf0},
	{0x4011,0xff},
	{0x4012,0x02},
	{0x4013,0x01},
	{0x4014,0x01},
	{0x4015,0x01},
	{0x4042,0x00},
	{0x4043,0x80},
	{0x4044,0x00},
	{0x4045,0x80},
	{0x4046,0x00},
	{0x4047,0x80},
	{0x4048,0x00},
	{0x4049,0x80},
	{0x4041,0x03},
	{0x404c,0x20},
	{0x404d,0x00},
	{0x404e,0x20},
	{0x4203,0x80},
	{0x4307,0x30},
	{0x4317,0x00},
	{0x4503,0x08},
	{0x4601,0x80},
	{0x4800,0x44},
	{0x4816,0x53},
	{0x481b,0x58},
	{0x481f,0x27},
	{0x4837,0x14},
	{0x483c,0x0f},
	{0x484b,0x05},
	{0x5000,0x77},
	{0x5001,0x0a},
	{0x5004,0x04},
	{0x502e,0x03},
	{0x5030,0x41},
	{0x5795,0x02},
	{0x5796,0x20},
	{0x5797,0x20},
	{0x5798,0xd5},
	{0x5799,0xd5},
	{0x579a,0x00},
	{0x579b,0x00},
	{0x579c,0x00},
	{0x579d,0x00},
	{0x579e,0x07},
	{0x579f,0xa0},
	{0x57a0,0x04},
	{0x57a1,0x40},
	{0x5780,0x14},
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
	{0x5791,0x04},
	{0x5792,0x00},
	{0x5793,0x52},
	{0x5794,0xa3},
	{0x59f8,0x3d},
	{0x5a08,0x02},
	{0x5b00,0x02},
	{0x5b01,0x10},
	{0x5b02,0x03},
	{0x5b03,0xcf},
	{0x5b05,0x6c},
	{0x5e00,0x00},
	{0x0100,0x01},
	{OV8856_REG_DELAY,0x10},
	{OV8856_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8856_init_regs_640_480_30fps_mipi[] = {
	{0x0100,0x0 },
	{0x0302,0x3c},
	{0x0303,0x1 },
	{0x031e,0xc },
	{0x3000,0x0 },
	{0x300e,0x0 },
	{0x3010,0x0 },
	{0x3015,0x84},
	{0x3018,0x32},
	{0x3021,0x23},
	{0x3033,0x24},
	{0x3500,0x0 },
	{0x3501,0x25},
	{0x3502,0xc0},
	{0x3503,0x8 },
	{0x3505,0x83},
	{0x3508,0x1 },
	{0x3509,0x80},
	{0x350c,0x0 },
	{0x350d,0x80},
	{0x350e,0x4 },
	{0x350f,0x0 },
	{0x3510,0x0 },
	{0x3511,0x2 },
	{0x3512,0x0 },
	{0x3600,0x72},
	{0x3601,0x40},
	{0x3602,0x30},
	{0x3610,0xc5},
	{0x3611,0x58},
	{0x3612,0x5c},
	{0x3613,0xca},
	{0x3614,0x20},
	{0x3628,0xff},
	{0x3629,0xff},
	{0x362a,0xff},
	{0x3633,0x10},
	{0x3634,0x10},
	{0x3635,0x10},
	{0x3636,0x10},
	{0x3663,0x8 },
	{0x3669,0x34},
	{0x366e,0x8 },
	{0x3706,0x86},
	{0x370b,0x7e},
	{0x3714,0x29},
	{0x3730,0x12},
	{0x3733,0x10},
	{0x3764,0x0 },
	{0x3765,0x0 },
	{0x3769,0x62},
	{0x376a,0x2a},
	{0x376b,0x30},
	{0x3780,0x0 },
	{0x3781,0x24},
	{0x3782,0x0 },
	{0x3783,0x23},
	{0x3798,0x2f},
	{0x37a1,0x60},
	{0x37a8,0x6a},
	{0x37ab,0x3f},
	{0x37c2,0x34},
	{0x37c3,0xf1},
	{0x37c9,0x80},
	{0x37cb,0x16},
	{0x37cc,0x16},
	{0x37cd,0x16},
	{0x37ce,0x16},
	{0x3800,0x1 },
	{0x3801,0x50},
	{0x3802,0x1 },
	{0x3803,0x10},
	{0x3804,0xb },
	{0x3805,0x8f},
	{0x3806,0x8 },
	{0x3807,0x9f},
	{0x3808,0x2 },
	{0x3809,0x80},
	{0x380a,0x1 },
	{0x380b,0xe0},
	{0x380c,0x7 },// 0x78c = 1932
	{0x380d,0x8c},//
	{0x380f,0xc0},// 0x9c0 = 2496
	{0x3810,0x0 },
	{0x3811,0x7 },
	{0x3812,0x0 },
	{0x3813,0x2 },
	{0x3814,0x3 },
	{0x3815,0x1 },
	{0x3816,0x0 },
	{0x3817,0x0 },
	{0x3818,0x0 },
	{0x3819,0x0 },
	{0x3820,0x90},
	{0x3821,0x67},
	{0x382a,0x7 },
	{0x382b,0x1 },
	{0x3830,0x6 },
	{0x3836,0x2 },
	{0x3862,0x4 },
	{0x3863,0x8 },
	{0x3cc0,0x33},
	{0x3d85,0x17},
	{0x3d8c,0x73},
	{0x3d8d,0xde},
	{0x4001,0xe0},
	{0x4003,0x40},
	{0x4008,0x0 },
	{0x4009,0x5 },
	{0x400a,0x0 },
	{0x400b,0x84},
	{0x400f,0x80},
	{0x4010,0xf0},
	{0x4011,0xff},
	{0x4012,0x2 },
	{0x4013,0x1 },
	{0x4014,0x1 },
	{0x4015,0x1 },
	{0x4042,0x0 },
	{0x4043,0x80},
	{0x4044,0x0 },
	{0x4045,0x80},
	{0x4046,0x0 },
	{0x4047,0x80},
	{0x4048,0x0 },
	{0x4049,0x80},
	{0x4041,0x3 },
	{0x404c,0x20},
	{0x404d,0x0 },
	{0x404e,0x20},
	{0x4203,0x80},
	{0x4307,0x30},
	{0x4317,0x0 },
	{0x4503,0x8 },
	{0x4601,0x40},
	{0x4800,0x44},
	{0x4816,0x53},
	{0x481b,0x58},
	{0x481f,0x27},
	{0x4837,0x16},
	{0x483c,0xf },
	{0x484b,0x5 },
	{0x5000,0x77},
	{0x5001,0xa },
	{0x5004,0x4 },
	{0x502e,0x3 },
	{0x5030,0x41},
	{0x5795,0x0 },
	{0x5796,0x10},
	{0x5797,0x10},
	{0x5798,0x73},
	{0x5799,0x73},
	{0x579a,0x0 },
	{0x579b,0x0 },
	{0x579c,0x0 },
	{0x579d,0x0 },
	{0x579e,0x5 },
	{0x579f,0xa0},
	{0x57a0,0x3 },
	{0x57a1,0x20},
	{0x5780,0x14},
	{0x5781,0xf },
	{0x5782,0x44},
	{0x5783,0x2 },
	{0x5784,0x1 },
	{0x5785,0x1 },
	{0x5786,0x0 },
	{0x5787,0x4 },
	{0x5788,0x2 },
	{0x5789,0xf },
	{0x578a,0xfd},
	{0x578b,0xf5},
	{0x578c,0xf5},
	{0x578d,0x3 },
	{0x578e,0x8 },
	{0x578f,0xc },
	{0x5790,0x8 },
	{0x5791,0x4 },
	{0x5792,0x0 },
	{0x5793,0x52},
	{0x5794,0xa3},
	{0x59f8,0x3d},
	{0x5a08,0x2 },
	{0x5b00,0x2 },
	{0x5b01,0x10},
	{0x5b02,0x3 },
	{0x5b03,0xcf},
	{0x5b05,0x6c},
	{0x5e00,0x0 },
	{0x366d,0x11},
	{0x5003,0xc0},
	{0x5006,0x2 },
	{0x5007,0x90},
	{0x5e10,0x7c},
	{0x0100,0x1 },
	{OV8856_REG_DELAY,0x10},
	{OV8856_REG_END, 0x00},	/* END MARKER */
};
static struct tx_isp_sensor_win_setting ov8856_win_sizes[] = {
	/* [0] 1928*1088 @20fps */
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ov8856_init_regs_1920_1080_30fps_mipi,
	},
	{
		.width		= 640,
		.height		= 480,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ov8856_init_regs_640_480_30fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &ov8856_win_sizes[0];

static struct regval_list ov8856_stream_on_mipi[] = {
	{OV8856_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8856_stream_off_mipi[] = {
	{OV8856_REG_END, 0x00},	/* END MARKER */
};

int ov8856_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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

int ov8856_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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
static int ov8856_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != OV8856_REG_END) {
		if (vals->reg_num == OV8856_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = ov8856_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int ov8856_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != OV8856_REG_END) {
		if (vals->reg_num == OV8856_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = ov8856_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int ov8856_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int ov8856_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = ov8856_read(sd, 0x300b, &v);
    ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OV8856_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = ov8856_read(sd, 0x300c, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OV8856_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int ov8856_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	ret = ov8856_write(sd, 0x3502, (unsigned char)((it & 0x0f) << 4));
	ret += ov8856_write(sd, 0x3501, (unsigned char)((it >> 4) & 0xff));
	ret += ov8856_write(sd, 0x3500, (unsigned char)((it >> 12) & 0xf));

	ret += ov8856_write(sd, 0x3509, (unsigned char)(again & 0xff));
	ret += ov8856_write(sd, 0x3508, (unsigned char)(((again >> 8) & 0xf)));
	return 0;
}

static int ov8856_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = ov8856_write(sd, 0x3502, (unsigned char)((value & 0x0f) << 4));
	ret += ov8856_write(sd, 0x3501, (unsigned char)((value >> 4) & 0xff));
	ret += ov8856_write(sd, 0x3500, (unsigned char)((value >> 12) & 0xf));
	if (ret < 0)
		return ret;
	return ret;
}

static int ov8856_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += ov8856_write(sd, 0x3509, (unsigned char)(value & 0xff));
	ret += ov8856_write(sd, 0x3508, (unsigned char)(((value >> 8) & 0xf)));
	if (ret < 0)
		return ret;

	return ret;
}

static int ov8856_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int ov8856_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int ov8856_init(struct tx_isp_subdev *sd, int enable)
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

	ret = ov8856_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int ov8856_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = ov8856_write_array(sd, ov8856_stream_on_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("ov8856 stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = ov8856_write_array(sd, ov8856_stream_off_mipi);
		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("ov8856 stream off\n");
	}

	return ret;
}

static int ov8856_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned char max_fps = 30;

	switch (sensor_resolution)
	{
		case TX_SENSOR_RES_200:
			sclk = 144668160; /* 1932 * 2496 *30 */
			max_fps = 30;
			break;
		case TX_SENSOR_RES_30:
			sclk = 144668160; /* 1932 * 2496 *30 */
			max_fps = 30;
			break;
		default:
			ISP_ERROR("Now we do not support this resolution ratio!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || fps < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		return -1;
	}

	ret += ov8856_read(sd, 0x380c, &val);
	hts = val<<8;
	val = 0;
	ret += ov8856_read(sd, 0x380d, &val);
	hts |= val;
	if (0 != ret) {
		printk("err: ov8856 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += ov8856_write(sd, 0x380f, vts&0xff);
	ret += ov8856_write(sd, 0x380e, (vts>>8)&0xff);
	if (0 != ret) {
		printk("err: ov8856_write err\n");
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

static int ov8856_set_mode(struct tx_isp_subdev *sd, int value)
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

static int ov8856_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"ov8856_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"ov8856_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = ov8856_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an ov8856 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("ov8856 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "ov8856", sizeof("ov8856"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int ov8856_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    int ret = 0;
    unsigned char val;

    ret = ov8856_read(sd, 0x3820, &val);
    if(enable & 0x02){
        val |= 0x06;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SGBRG10_1X10;
    }else{
        val &= 0xF9;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SBGGR10_1X10;
    }
	sensor->video.mbus_change = 1;
    ret = ov8856_write(sd, 0x3820, val);
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

    return 0;
}

static int ov8856_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
	     	ret = ov8856_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
	if(arg)
			ret = ov8856_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = ov8856_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = ov8856_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = ov8856_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = ov8856_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = ov8856_write_array(sd, ov8856_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = ov8856_write_array(sd, ov8856_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = ov8856_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = ov8856_set_vflip(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int ov8856_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = ov8856_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int ov8856_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	ov8856_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops ov8856_core_ops = {
	.g_chip_ident = ov8856_g_chip_ident,
	.reset = ov8856_reset,
	.init = ov8856_init,
	/*.ioctl = ov8856_ops_ioctl,*/
	.g_register = ov8856_g_register,
	.s_register = ov8856_s_register,
};

static struct tx_isp_subdev_video_ops ov8856_video_ops = {
	.s_stream = ov8856_s_stream,
};

static struct tx_isp_subdev_sensor_ops	ov8856_sensor_ops = {
	.ioctl	= ov8856_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops ov8856_ops = {
	.core = &ov8856_core_ops,
	.video = &ov8856_video_ops,
	.sensor = &ov8856_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "ov8856",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int ov8856_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

	switch (sensor_resolution)
	{
		case TX_SENSOR_RES_200:
			wsize = &ov8856_win_sizes[0];
			ov8856_attr.max_integration_time_native = 2488;/* vts - 8 */
			ov8856_attr.integration_time_limit = 2488;
			ov8856_attr.total_width = 1932;
			ov8856_attr.total_height = 2496;
			ov8856_attr.max_integration_time = 2488;
			memcpy((void*)(&(ov8856_attr.mipi)),(void*)(&ov8856_mipi_1080P),sizeof(ov8856_mipi_1080P));
			break;
		case TX_SENSOR_RES_30:
			wsize = &ov8856_win_sizes[1];
			ov8856_attr.max_integration_time_native = 2488;
			ov8856_attr.integration_time_limit = 2488;
			ov8856_attr.total_width = 1932;
			ov8856_attr.total_height = 2496;
			ov8856_attr.max_integration_time = 2488;
			memcpy((void*)(&(ov8856_attr.mipi)),(void*)(&ov8856_mipi_480P),sizeof(ov8856_mipi_480P));
			break;
		default:
			ISP_ERROR("Now we do not support this resolution ratio!!!\n");
	}

	/*
	  convert sensor-gain into isp-gain,
	*/
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &ov8856_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &ov8856_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->ov8856\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int ov8856_remove(struct i2c_client *client)
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

static const struct i2c_device_id ov8856_id[] = {
	{ "ov8856", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov8856_id);

static struct i2c_driver ov8856_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov8856",
	},
	.probe		= ov8856_probe,
	.remove		= ov8856_remove,
	.id_table	= ov8856_id,
};

static __init int init_ov8856(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init ov8856 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&ov8856_driver);
}

static __exit void exit_ov8856(void)
{
	private_i2c_del_driver(&ov8856_driver);
}

module_init(init_ov8856);
module_exit(exit_ov8856);

MODULE_DESCRIPTION("A low-level driver for SmartSens ov8856 sensors");
MODULE_LICENSE("GPL");
