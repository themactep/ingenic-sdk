/*
 * os04a10.c
 *
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          2688*1520       25        mipi_2lane           linear
 *   1          2560*1440       25        mipi_2lane           linear
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

#define OS04A10_CHIP_ID_H	(0x53)
#define OS04A10_CHIP_ID_M	(0x04)
#define OS04A10_CHIP_ID_L	(0x41)
#define OS04A10_REG_END		0xffff
#define OS04A10_REG_DELAY	0xfffe
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

struct again_lut os04a10_again_lut[] = {
	{0x100, 0},
	{0x110, 5687},
	{0x120, 11136},
	{0x130, 16208},
	{0x140, 21097},
	{0x150, 25674},
	{0x160, 30109},
	{0x170, 34279},
	{0x180, 38336},
	{0x190, 42165},
	{0x1a0, 45904},
	{0x1b0, 49444},
	{0x1c0, 52910},
	{0x1d0, 56202},
	{0x1e0, 59433},
	{0x1f0, 62509},
	{0x200, 65536},
	{0x220, 71267},
	{0x240, 76672},
	{0x260, 81784},
	{0x280, 86633},
	{0x2a0, 91246},
	{0x2c0, 95645},
	{0x2e0, 99848},
	{0x300, 103872},
	{0x320, 107731},
	{0x340, 111440},
	{0x360, 115008},
	{0x380, 118446},
	{0x3a0, 121764},
	{0x3c0, 124969},
	{0x3e0, 128070},
	{0x400, 131072},
	{0x440, 136803},
	{0x480, 142208},
	{0x4c0, 147320},
	{0x500, 152169},
	{0x540, 156782},
	{0x580, 161181},
	{0x5c0, 165384},
	{0x600, 169408},
	{0x640, 173267},
	{0x680, 176976},
	{0x6c0, 180544},
	{0x700, 183982},
	{0x740, 187300},
	{0x780, 190505},
	{0x7c0, 193606},
	{0x800, 196608},
	{0x880, 202339},
	{0x900, 207744},
	{0x980, 212856},
	{0xa00, 217705},
	{0xa80, 222318},
	{0xb00, 226717},
	{0xb80, 230920},
	{0xc00, 234944},
	{0xc80, 238803},
	{0xd00, 242512},
	{0xd80, 246080},
	{0xe00, 249518},
	{0xe80, 252836},
	{0xf00, 256041},
	{0xf80, 259142},
};

struct tx_isp_sensor_attribute os04a10_attr;

unsigned int os04a10_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = os04a10_again_lut;
	while(lut->gain <= os04a10_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == os04a10_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int os04a10_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus os04a10_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1104,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.image_twidth = 2688,
	.image_theight = 1520,
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

struct tx_isp_mipi_bus os04a10_2560_1440_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1104,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
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
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute os04a10_attr={
	.name = "os04a10",
	.chip_id = 0x530441,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x36,
	.max_again = 259142,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = os04a10_alloc_again,
	.sensor_ctrl.alloc_dgain = os04a10_alloc_dgain,
};

static struct regval_list os04a10_init_regs_2688_1520_25fps_mipi[] = {
	{0x0103,0x01},
	{0x0109,0x01},
	{0x0104,0x02},
	{0x0102,0x00},
	{0x0305,0x3c},
	{0x0306,0x00},
	{0x0307,0x00},
	{0x0308,0x04},
	{0x030a,0x01},
	{0x0317,0x09},
	{0x0322,0x01},
	{0x0323,0x02},
	{0x0324,0x00},
	{0x0325,0x90},
	{0x0327,0x05},
	{0x0329,0x02},
	{0x032c,0x02},
	{0x032d,0x02},
	{0x032e,0x02},
	{0x300f,0x11},
	{0x3012,0x21},
	{0x3026,0x10},
	{0x3027,0x08},
	{0x302d,0x24},
	{0x3104,0x01},
	{0x3106,0x11},
	{0x3400,0x00},
	{0x3408,0x05},
	{0x340c,0x0c},
	{0x340d,0xb0},
	{0x3425,0x51},
	{0x3426,0x10},
	{0x3427,0x14},
	{0x3428,0x10},
	{0x3429,0x10},
	{0x342a,0x10},
	{0x342b,0x04},
	{0x3501,0x02},
	{0x3504,0x08},
	{0x3508,0x01},
	{0x3509,0x00},
	{0x350a,0x01},
	{0x3544,0x08},
	{0x3548,0x01},
	{0x3549,0x00},
	{0x3584,0x08},
	{0x3588,0x01},
	{0x3589,0x00},
	{0x3601,0x70},
	{0x3604,0xe3},
	{0x3605,0x7f},
	{0x3606,0x80},
	{0x3608,0xa8},
	{0x360a,0xd0},
	{0x360b,0x08},
	{0x360e,0xc8},
	{0x360f,0x66},
	{0x3610,0x89},
	{0x3611,0x8a},
	{0x3612,0x4e},
	{0x3613,0xbd},
	{0x3614,0x9b},
	{0x362a,0x0e},
	{0x362b,0x0e},
	{0x362c,0x0e},
	{0x362d,0x0e},
	{0x362e,0x1a},
	{0x362f,0x34},
	{0x3630,0x67},
	{0x3631,0x7f},
	{0x3638,0x00},
	{0x3643,0x00},
	{0x3644,0x00},
	{0x3645,0x00},
	{0x3646,0x00},
	{0x3647,0x00},
	{0x3648,0x00},
	{0x3649,0x00},
	{0x364a,0x04},
	{0x364c,0x0e},
	{0x364d,0x0e},
	{0x364e,0x0e},
	{0x364f,0x0e},
	{0x3650,0xff},
	{0x3651,0xff},
	{0x365a,0x00},
	{0x365b,0x00},
	{0x365c,0x00},
	{0x365d,0x00},
	{0x3661,0x07},
	{0x3662,0x02},
	{0x3663,0x20},
	{0x3665,0x12},
	{0x3667,0xd4},
	{0x3668,0x80},
	{0x366c,0x00},
	{0x366d,0x00},
	{0x366e,0x00},
	{0x366f,0x00},
	{0x3671,0x08},
	{0x3673,0x2a},
	{0x3681,0x80},
	{0x3700,0x2d},
	{0x3701,0x22},
	{0x3702,0x25},
	{0x3703,0x20},
	{0x3705,0x00},
	{0x3706,0x72},
	{0x3707,0x0a},
	{0x3708,0x36},
	{0x3709,0x57},
	{0x370a,0x01},
	{0x370b,0x14},
	{0x3714,0x01},
	{0x3719,0x1f},
	{0x371b,0x16},
	{0x371c,0x00},
	{0x371d,0x08},
	{0x373f,0x63},
	{0x3740,0x63},
	{0x3741,0x63},
	{0x3742,0x63},
	{0x3743,0x01},
	{0x3756,0x9d},
	{0x3757,0x9d},
	{0x3762,0x1c},
	{0x376c,0x04},
	{0x3776,0x05},
	{0x3777,0x22},
	{0x3779,0x60},
	{0x377c,0x48},
	{0x3784,0x06},
	{0x3785,0x0a},
	{0x3790,0x10},
	{0x3793,0x04},
	{0x3794,0x07},
	{0x3796,0x00},
	{0x3797,0x02},
	{0x379c,0x4d},
	{0x37a1,0x80},
	{0x37bb,0x88},
	{0x37be,0x48},
	{0x37bf,0x01},
	{0x37c0,0x01},
	{0x37c4,0x72},
	{0x37c5,0x72},
	{0x37c6,0x72},
	{0x37ca,0x21},
	{0x37cc,0x13},
	{0x37cd,0x90},
	{0x37cf,0x02},
	{0x37d0,0x00},
	{0x37d1,0x72},
	{0x37d2,0x01},
	{0x37d3,0x14},
	{0x37d4,0x00},
	{0x37d5,0x6c},
	{0x37d6,0x00},
	{0x37d7,0xf7},
	{0x37d8,0x01},
	{0x37dc,0x00},
	{0x37dd,0x00},
	{0x37da,0x00},
	{0x37db,0x00},
	{0x3800,0x00},
	{0x3801,0x00},
	{0x3802,0x00},
	{0x3803,0x00},
	{0x3804,0x0a},
	{0x3805,0x8f},
	{0x3806,0x05},
	{0x3807,0xff},
	{0x3808,0x0a},
	{0x3809,0x80},
	{0x380a,0x05},
	{0x380b,0xf0},
	{0x380c,0x06},//0x6dc -> 1756
	{0x380d,0xdc},//
	{0x380e,0x06},//0x658 -> 1624
	{0x380f,0x58},//
	{0x3811,0x08},
	{0x3813,0x08},
	{0x3814,0x01},
	{0x3815,0x01},
	{0x3816,0x01},
	{0x3817,0x01},
	{0x381c,0x00},
	{0x3820,0x02},
	{0x3821,0x00},
	{0x3822,0x14},
	{0x3823,0x18},
	{0x3826,0x00},
	{0x3827,0x00},
	{0x3833,0x40},
	{0x384c,0x02},
	{0x384d,0xdc},
	{0x3858,0x3c},
	{0x3865,0x02},
	{0x3866,0x00},
	{0x3867,0x00},
	{0x3868,0x02},
	{0x3900,0x13},
	{0x3940,0x13},
	{0x3980,0x13},
	{0x3c01,0x11},
	{0x3c05,0x00},
	{0x3c0f,0x1c},
	{0x3c12,0x0d},
	{0x3c19,0x00},
	{0x3c21,0x00},
	{0x3c3a,0x10},
	{0x3c3b,0x18},
	{0x3c3d,0xc6},
	{0x3c55,0xcb},
	{0x3c5a,0x55},
	{0x3c5d,0xcf},
	{0x3c5e,0xcf},
	{0x3d8c,0x70},
	{0x3d8d,0x10},
	{0x4000,0xf9},
	{0x4001,0x2f},
	{0x4004,0x00},
	{0x4005,0x40},
	{0x4008,0x02},
	{0x4009,0x11},
	{0x400a,0x06},
	{0x400b,0x40},
	{0x400e,0x40},
	{0x402e,0x00},
	{0x402f,0x40},
	{0x4030,0x00},
	{0x4031,0x40},
	{0x4032,0x0f},
	{0x4033,0x80},
	{0x4050,0x00},
	{0x4051,0x07},
	{0x4011,0xbb},
	{0x410f,0x01},
	{0x4288,0xcf},
	{0x4289,0x00},
	{0x428a,0x46},
	{0x430b,0x0f},
	{0x430c,0xfc},
	{0x430d,0x00},
	{0x430e,0x00},
	{0x4314,0x04},
	{0x4500,0x18},
	{0x4501,0x18},
	{0x4503,0x10},
	{0x4504,0x00},
	{0x4506,0x32},
	{0x4507,0x02},
	{0x4601,0x30},
	{0x4603,0x00},
	{0x460a,0x50},
	{0x460c,0x60},
	{0x4640,0x62},
	{0x4646,0xaa},
	{0x4647,0x55},
	{0x4648,0x99},
	{0x4649,0x66},
	{0x464d,0x00},
	{0x4654,0x11},
	{0x4655,0x22},
	{0x4800,0x44},
	{0x480e,0x00},
	{0x4810,0xff},
	{0x4811,0xff},
	{0x4813,0x00},
	{0x481f,0x30},
	{0x4837,0x0e},
	{0x484b,0x27},
	{0x4d00,0x4d},
	{0x4d01,0x9d},
	{0x4d02,0xb9},
	{0x4d03,0x2e},
	{0x4d04,0x4a},
	{0x4d05,0x3d},
	{0x4d09,0x4f},
	{0x5000,0x1f},
	{0x5001,0x0d},
	{0x5080,0x00},
	{0x50c0,0x00},
	{0x5100,0x00},
	{0x5200,0x00},
	{0x5201,0x00},
	{0x5202,0x03},
	{0x5203,0xff},
	{0x5393,0x30},
	{0x5395,0x18},
	{0x5397,0x00},
	{0x539a,0x01},
	{0x539b,0x01},
	{0x539c,0x01},
	{0x539d,0x01},
	{0x539e,0x01},
	{0x539f,0x01},
	{0x5413,0x18},
	{0x5415,0x0c},
	{0x5417,0x00},
	{0x541a,0x01},
	{0x541b,0x01},
	{0x541c,0x01},
	{0x541d,0x01},
	{0x541e,0x01},
	{0x541f,0x01},
	{0x5493,0x06},
	{0x5495,0x03},
	{0x5497,0x00},
	{0x549a,0x01},
	{0x549b,0x01},
	{0x549c,0x01},
	{0x549d,0x01},
	{0x549e,0x01},
	{0x549f,0x01},
	{0x5393,0x30},
	{0x5395,0x18},
	{0x5397,0x00},
	{0x539a,0x01},
	{0x539b,0x01},
	{0x539c,0x01},
	{0x539d,0x01},
	{0x539e,0x01},
	{0x539f,0x01},
	{0x5413,0x18},
	{0x5415,0x0c},
	{0x5417,0x00},
	{0x541a,0x01},
	{0x541b,0x01},
	{0x541c,0x01},
	{0x541d,0x01},
	{0x541e,0x01},
	{0x541f,0x01},
	{0x5493,0x06},
	{0x5495,0x03},
	{0x5497,0x00},
	{0x549a,0x01},
	{0x549b,0x01},
	{0x549c,0x01},
	{0x549d,0x01},
	{0x549e,0x01},
	{0x549f,0x01},
	{0x5780,0x53},
	{0x5782,0x18},
	{0x5783,0x3c},
	{0x5786,0x01},
	{0x5788,0x18},
	{0x5789,0x3c},
	{0x5792,0x11},
	{0x5793,0x33},
	{0x5857,0xff},
	{0x5858,0xff},
	{0x5859,0xff},
	{0x58d7,0xff},
	{0x58d8,0xff},
	{0x58d9,0xff},
	{0x0100,0x01},
	{0x0100,0x01},
	{OS04A10_REG_END, 0x00},/* END MARKER */
};

static struct regval_list os04a10_init_regs_2560_1440_25fps_mipi[] = {
	{0x0103,0x01},
	{0x0109,0x01},
	{0x0104,0x02},
	{0x0102,0x00},
	{0x0305,0x3c},
	{0x0306,0x00},
	{0x0307,0x00},
	{0x0308,0x04},
	{0x030a,0x01},
	{0x0317,0x09},
	{0x0322,0x01},
	{0x0323,0x02},
	{0x0324,0x00},
	{0x0325,0x90},
	{0x0327,0x05},
	{0x0329,0x02},
	{0x032c,0x02},
	{0x032d,0x02},
	{0x032e,0x02},
	{0x300f,0x11},
	{0x3012,0x21},
	{0x3026,0x10},
	{0x3027,0x08},
	{0x302d,0x24},
	{0x3104,0x01},
	{0x3106,0x11},
	{0x3400,0x00},
	{0x3408,0x05},
	{0x340c,0x0c},
	{0x340d,0xb0},
	{0x3425,0x51},
	{0x3426,0x10},
	{0x3427,0x14},
	{0x3428,0x10},
	{0x3429,0x10},
	{0x342a,0x10},
	{0x342b,0x04},
	{0x3501,0x02},
	{0x3504,0x08},
	{0x3508,0x01},
	{0x3509,0x00},
	{0x350a,0x01},
	{0x3544,0x08},
	{0x3548,0x01},
	{0x3549,0x00},
	{0x3584,0x08},
	{0x3588,0x01},
	{0x3589,0x00},
	{0x3601,0x70},
	{0x3604,0xe3},
	{0x3605,0x7f},
	{0x3606,0x80},
	{0x3608,0xa8},
	{0x360a,0xd0},
	{0x360b,0x08},
	{0x360e,0xc8},
	{0x360f,0x66},
	{0x3610,0x89},
	{0x3611,0x8a},
	{0x3612,0x4e},
	{0x3613,0xbd},
	{0x3614,0x9b},
	{0x362a,0x0e},
	{0x362b,0x0e},
	{0x362c,0x0e},
	{0x362d,0x0e},
	{0x362e,0x1a},
	{0x362f,0x34},
	{0x3630,0x67},
	{0x3631,0x7f},
	{0x3638,0x00},
	{0x3643,0x00},
	{0x3644,0x00},
	{0x3645,0x00},
	{0x3646,0x00},
	{0x3647,0x00},
	{0x3648,0x00},
	{0x3649,0x00},
	{0x364a,0x04},
	{0x364c,0x0e},
	{0x364d,0x0e},
	{0x364e,0x0e},
	{0x364f,0x0e},
	{0x3650,0xff},
	{0x3651,0xff},
	{0x365a,0x00},
	{0x365b,0x00},
	{0x365c,0x00},
	{0x365d,0x00},
	{0x3661,0x07},
	{0x3662,0x02},
	{0x3663,0x20},
	{0x3665,0x12},
	{0x3667,0xd4},
	{0x3668,0x80},
	{0x366c,0x00},
	{0x366d,0x00},
	{0x366e,0x00},
	{0x366f,0x00},
	{0x3671,0x08},
	{0x3673,0x2a},
	{0x3681,0x80},
	{0x3700,0x2d},
	{0x3701,0x22},
	{0x3702,0x25},
	{0x3703,0x20},
	{0x3705,0x00},
	{0x3706,0x72},
	{0x3707,0x0a},
	{0x3708,0x36},
	{0x3709,0x57},
	{0x370a,0x01},
	{0x370b,0x14},
	{0x3714,0x01},
	{0x3719,0x1f},
	{0x371b,0x16},
	{0x371c,0x00},
	{0x371d,0x08},
	{0x373f,0x63},
	{0x3740,0x63},
	{0x3741,0x63},
	{0x3742,0x63},
	{0x3743,0x01},
	{0x3756,0x9d},
	{0x3757,0x9d},
	{0x3762,0x1c},
	{0x376c,0x04},
	{0x3776,0x05},
	{0x3777,0x22},
	{0x3779,0x60},
	{0x377c,0x48},
	{0x3784,0x06},
	{0x3785,0x0a},
	{0x3790,0x10},
	{0x3793,0x04},
	{0x3794,0x07},
	{0x3796,0x00},
	{0x3797,0x02},
	{0x379c,0x4d},
	{0x37a1,0x80},
	{0x37bb,0x88},
	{0x37be,0x48},
	{0x37bf,0x01},
	{0x37c0,0x01},
	{0x37c4,0x72},
	{0x37c5,0x72},
	{0x37c6,0x72},
	{0x37ca,0x21},
	{0x37cc,0x13},
	{0x37cd,0x90},
	{0x37cf,0x02},
	{0x37d0,0x00},
	{0x37d1,0x72},
	{0x37d2,0x01},
	{0x37d3,0x14},
	{0x37d4,0x00},
	{0x37d5,0x6c},
	{0x37d6,0x00},
	{0x37d7,0xf7},
	{0x37d8,0x01},
	{0x37dc,0x00},
	{0x37dd,0x00},
	{0x37da,0x00},
	{0x37db,0x00},
	{0x3800,0x00},
	{0x3801,0x00},
	{0x3802,0x00},
	{0x3803,0x00},
	{0x3804,0x0a},
	{0x3805,0x8f},
	{0x3806,0x05},
	{0x3807,0xff},
	{0x3808,0x0a},
	{0x3809,0x00},
	{0x380a,0x05},
	{0x380b,0xa0},
	{0x380c,0x06},//hts 0x6dc -> 1756
	{0x380d,0xdc},//
	{0x380e,0x06},//vts 0x658 -> 1624
	{0x380f,0x58},//
	{0x3811,0x08},
	{0x3813,0x08},
	{0x3814,0x01},
	{0x3815,0x01},
	{0x3816,0x01},
	{0x3817,0x01},
	{0x381c,0x00},
	{0x3820,0x02},
	{0x3821,0x00},
	{0x3822,0x14},
	{0x3823,0x18},
	{0x3826,0x00},
	{0x3827,0x00},
	{0x3833,0x40},
	{0x384c,0x02},
	{0x384d,0xdc},
	{0x3858,0x3c},
	{0x3865,0x02},
	{0x3866,0x00},
	{0x3867,0x00},
	{0x3868,0x02},
	{0x3900,0x13},
	{0x3940,0x13},
	{0x3980,0x13},
	{0x3c01,0x11},
	{0x3c05,0x00},
	{0x3c0f,0x1c},
	{0x3c12,0x0d},
	{0x3c19,0x00},
	{0x3c21,0x00},
	{0x3c3a,0x10},
	{0x3c3b,0x18},
	{0x3c3d,0xc6},
	{0x3c55,0xcb},
	{0x3c5a,0x55},
	{0x3c5d,0xcf},
	{0x3c5e,0xcf},
	{0x3d8c,0x70},
	{0x3d8d,0x10},
	{0x4000,0xf9},
	{0x4001,0x2f},
	{0x4004,0x00},
	{0x4005,0x40},
	{0x4008,0x02},
	{0x4009,0x11},
	{0x400a,0x06},
	{0x400b,0x40},
	{0x400e,0x40},
	{0x402e,0x00},
	{0x402f,0x40},
	{0x4030,0x00},
	{0x4031,0x40},
	{0x4032,0x0f},
	{0x4033,0x80},
	{0x4050,0x00},
	{0x4051,0x07},
	{0x4011,0xbb},
	{0x410f,0x01},
	{0x4288,0xcf},
	{0x4289,0x00},
	{0x428a,0x46},
	{0x430b,0x0f},
	{0x430c,0xfc},
	{0x430d,0x00},
	{0x430e,0x00},
	{0x4314,0x04},
	{0x4500,0x18},
	{0x4501,0x18},
	{0x4503,0x10},
	{0x4504,0x00},
	{0x4506,0x32},
	{0x4507,0x02},
	{0x4601,0x30},
	{0x4603,0x00},
	{0x460a,0x50},
	{0x460c,0x60},
	{0x4640,0x62},
	{0x4646,0xaa},
	{0x4647,0x55},
	{0x4648,0x99},
	{0x4649,0x66},
	{0x464d,0x00},
	{0x4654,0x11},
	{0x4655,0x22},
	{0x4800,0x44},
	{0x480e,0x00},
	{0x4810,0xff},
	{0x4811,0xff},
	{0x4813,0x00},
	{0x481f,0x30},
	{0x4837,0x0e},
	{0x484b,0x27},
	{0x4d00,0x4d},
	{0x4d01,0x9d},
	{0x4d02,0xb9},
	{0x4d03,0x2e},
	{0x4d04,0x4a},
	{0x4d05,0x3d},
	{0x4d09,0x4f},
	{0x5000,0x1f},
	{0x5001,0x0d},
	{0x5080,0x00},
	{0x50c0,0x00},
	{0x5100,0x00},
	{0x5200,0x00},
	{0x5201,0x00},
	{0x5202,0x03},
	{0x5203,0xff},
	{0x5393,0x30},
	{0x5395,0x18},
	{0x5397,0x00},
	{0x539a,0x01},
	{0x539b,0x01},
	{0x539c,0x01},
	{0x539d,0x01},
	{0x539e,0x01},
	{0x539f,0x01},
	{0x5413,0x18},
	{0x5415,0x0c},
	{0x5417,0x00},
	{0x541a,0x01},
	{0x541b,0x01},
	{0x541c,0x01},
	{0x541d,0x01},
	{0x541e,0x01},
	{0x541f,0x01},
	{0x5493,0x06},
	{0x5495,0x03},
	{0x5497,0x00},
	{0x549a,0x01},
	{0x549b,0x01},
	{0x549c,0x01},
	{0x549d,0x01},
	{0x549e,0x01},
	{0x549f,0x01},
	{0x5393,0x30},
	{0x5395,0x18},
	{0x5397,0x00},
	{0x539a,0x01},
	{0x539b,0x01},
	{0x539c,0x01},
	{0x539d,0x01},
	{0x539e,0x01},
	{0x539f,0x01},
	{0x5413,0x18},
	{0x5415,0x0c},
	{0x5417,0x00},
	{0x541a,0x01},
	{0x541b,0x01},
	{0x541c,0x01},
	{0x541d,0x01},
	{0x541e,0x01},
	{0x541f,0x01},
	{0x5493,0x06},
	{0x5495,0x03},
	{0x5497,0x00},
	{0x549a,0x01},
	{0x549b,0x01},
	{0x549c,0x01},
	{0x549d,0x01},
	{0x549e,0x01},
	{0x549f,0x01},
	{0x5780,0x53},
	{0x5782,0x18},
	{0x5783,0x3c},
	{0x5786,0x01},
	{0x5788,0x18},
	{0x5789,0x3c},
	{0x5792,0x11},
	{0x5793,0x33},
	{0x5857,0xff},
	{0x5858,0xff},
	{0x5859,0xff},
	{0x58d7,0xff},
	{0x58d8,0xff},
	{0x58d9,0xff},
	{0x0100,0x01},
	{0x0100,0x01},
	{OS04A10_REG_END, 0x00},/* END MARKER */
};

static struct tx_isp_sensor_win_setting os04a10_win_sizes[] = {
	{
		.width		= 2688,
		.height		= 1520,
		.fps		= 25 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= os04a10_init_regs_2688_1520_25fps_mipi,
	},
	{
		.width		= 2560,
		.height		= 1440,
		.fps		= 25 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= os04a10_init_regs_2560_1440_25fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &os04a10_win_sizes[0];

static struct regval_list os04a10_stream_on_mipi[] = {
	{0x0100, 0x01},
	{OS04A10_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list os04a10_stream_off_mipi[] = {
	{0x0100, 0x00},
	{OS04A10_REG_END, 0x00},	/* END MARKER */
};

int os04a10_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int os04a10_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int os04a10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != OS04A10_REG_END) {
		if (vals->reg_num == OS04A10_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = os04a10_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int os04a10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != OS04A10_REG_END) {
		if (vals->reg_num == OS04A10_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = os04a10_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int os04a10_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int os04a10_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = os04a10_read(sd, 0x300a, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS04A10_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = os04a10_read(sd, 0x300b, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS04A10_CHIP_ID_M)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	ret = os04a10_read(sd, 0x300c, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS04A10_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int os04a10_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = -1;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret += os04a10_write(sd, 0x3501, (unsigned char)((it >> 8) & 0xff));
	ret += os04a10_write(sd, 0x3502, (unsigned char)(it & 0xff));

	ret += os04a10_write(sd, 0x3508, (unsigned char)((again >> 8) & 0xff));
	ret += os04a10_write(sd, 0x3509, (unsigned char)(again & 0xff));

	return 0;
}

#if 0
static int os04a10_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;

	ret += os04a10_write(sd, 0x3502, (unsigned char)(expo & 0xff));
	ret += os04a10_write(sd, 0x3501, (unsigned char)((expo >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int os04a10_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += os04a10_write(sd, 0x3509, (unsigned char)((value & 0xff)));
	ret += os04a10_write(sd, 0x3508, (unsigned char)((value >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int os04a10_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int os04a10_get_black_pedestal(struct tx_isp_subdev *sd, int value)
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

static int os04a10_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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

static int os04a10_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if(sensor->video.state == TX_ISP_MODULE_INIT){
			ret = os04a10_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if(sensor->video.state == TX_ISP_MODULE_RUNNING){

			ret = os04a10_write_array(sd, os04a10_stream_on_mipi);
			ISP_WARNING("os04a10 stream on\n");
		}
	}
	else {
		ret = os04a10_write_array(sd, os04a10_stream_off_mipi);
		ISP_WARNING("os04a10 stream off\n");
	}

	return ret;
}

static int os04a10_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 1756 * 1624 * 25;
		max_fps = TX_SENSOR_MAX_FPS_25;
		break;
	case 1:
		sclk = 1756 * 1624 * 25;
		max_fps = TX_SENSOR_MAX_FPS_25;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps<< 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}

	ret += os04a10_read(sd, 0x380c, &val);
	hts = val << 8;
	ret += os04a10_read(sd, 0x380d, &val);
	hts = (hts | val);

	if (0 != ret) {
		ISP_ERROR("err: os04a10 read err\n");
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = os04a10_write(sd, 0x380f, (unsigned char)(vts & 0xff));
	ret += os04a10_write(sd, 0x380e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: os04a10_write err\n");
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

static int os04a10_set_mode(struct tx_isp_subdev *sd, int value)
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
static int os04a10_set_hvflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t value;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	printk("\n Enter04a10_set_hvflip enable = 0x%x \n",enable);
	/* 2'b01:mirror,2'b10:filp */
	os04a10_read(sd, 0x3820, &value);
	switch(enable) {
	case 0:
		value &= 0xf9;
		break;
	case 1:
		value &= 0xfb;
		value |= 0x02;
		break;
	case 2:
		value &= 0xfd;
		value |= 0x04;
		break;
	case 3:
		value |= 0x06;
		break;
	}
	os04a10_write(sd, 0x3820, value);

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
		wsize = &os04a10_win_sizes[0];
		memcpy(&(os04a10_attr.mipi), &os04a10_mipi, sizeof(os04a10_mipi));
		os04a10_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		os04a10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		os04a10_attr.max_integration_time_native = 1624 - 8;
		os04a10_attr.integration_time_limit = 1624 - 8;
		os04a10_attr.total_width = 1756;
		os04a10_attr.total_height = 1624;
		os04a10_attr.max_integration_time = 1624 - 8;
		os04a10_attr.again = 0x80;
		os04a10_attr.integration_time = 0x640;
		break;
	case 1:
		wsize = &os04a10_win_sizes[1];
		memcpy(&(os04a10_attr.mipi), &os04a10_2560_1440_mipi, sizeof(os04a10_2560_1440_mipi));
		os04a10_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		os04a10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		os04a10_attr.max_integration_time_native = 1624 - 8;
		os04a10_attr.integration_time_limit = 1624 - 8;
		os04a10_attr.total_width = 1756;
		os04a10_attr.total_height = 1624;
		os04a10_attr.max_integration_time = 1624 - 8;
		os04a10_attr.again = 0x80;
		os04a10_attr.integration_time = 0x640;
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
	}

	switch(info->video_interface){
	case TISP_SENSOR_VI_MIPI_CSI0:
		os04a10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		os04a10_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_DVP:
		os04a10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

static int os04a10_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"os04a10_reset");
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
		ret = private_gpio_request(pwdn_gpio,"os04a10_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = os04a10_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an os04a10 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("os04a10 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "os04a10", sizeof("os04a10"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int os04a10_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = os04a10_set_expo(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		//if(arg)
		//	ret = os04a10_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		//if(arg)
		//	ret = os04a10_set_analog_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = os04a10_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = os04a10_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = os04a10_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = os04a10_write_array(sd, os04a10_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = os04a10_write_array(sd, os04a10_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = os04a10_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = os04a10_set_hvflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int os04a10_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = os04a10_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int os04a10_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	os04a10_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops os04a10_core_ops = {
	.g_chip_ident = os04a10_g_chip_ident,
	.reset = os04a10_reset,
	.init = os04a10_init,
	.g_register = os04a10_g_register,
	.s_register = os04a10_s_register,
};

static struct tx_isp_subdev_video_ops os04a10_video_ops = {
	.s_stream = os04a10_s_stream,
};

static struct tx_isp_subdev_sensor_ops	os04a10_sensor_ops = {
	.ioctl	= os04a10_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops os04a10_ops = {
	.core = &os04a10_core_ops,
	.video = &os04a10_video_ops,
	.sensor = &os04a10_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "os04a10",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int os04a10_probe(struct i2c_client *client,
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
	os04a10_attr.expo_fs = 1;
	sensor->video.shvflip = 1;
	sensor->video.attr = &os04a10_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &os04a10_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->os04a10\n");

	return 0;
}

static int os04a10_remove(struct i2c_client *client)
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

static const struct i2c_device_id os04a10_id[] = {
	{ "os04a10", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, os04a10_id);

static struct i2c_driver os04a10_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "os04a10",
	},
	.probe		= os04a10_probe,
	.remove		= os04a10_remove,
	.id_table	= os04a10_id,
};

static __init int init_os04a10(void)
{
	return private_i2c_add_driver(&os04a10_driver);
}

static __exit void exit_os04a10(void)
{
	private_i2c_del_driver(&os04a10_driver);
}

module_init(init_os04a10);
module_exit(exit_os04a10);

MODULE_DESCRIPTION("A low-level driver for os04a10 sensors");
MODULE_LICENSE("GPL");
