// SPDX-License-Identifier: GPL-2.0+
/*
 * ov4688.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          2688*1520       30        mipi_4lane           linear
 *   1          1920*1080       30        mipi_4lane           linear
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

#define SENSOR_NAME "ov4688"
#define SENSOR_CHIP_ID_H (0x46)
#define SENSOR_CHIP_ID_L (0x88)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define MCLK 24000000
#define SENSOR_VERSION "H20230928a"

static int reset_gpio = -1;
static int pwdn_gpio = -1;

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
	{0x178, 65536},
	{0x17c, 68422},
	{0x180, 71267},
	{0x184, 73987},
	{0x188, 76672},
	{0x18c, 79242},
	{0x190, 81784},
	{0x194, 84220},
	{0x198, 86633},
	{0x19c, 88950},
	{0x1a0, 91246},
	{0x1a4, 93454},
	{0x1a8, 95645},
	{0x1ac, 97753},
	{0x1b0, 99848},
	{0x1b4, 101865},
	{0x1b8, 103872},
	{0x1bc, 105806},
	{0x1c0, 107731},
	{0x1c4, 109589},
	{0x1c8, 111440},
	{0x1cc, 113226},
	{0x1d0, 115008},
	{0x1d4, 116729},
	{0x1d8, 118446},
	{0x1dc, 120107},
	{0x1e0, 121764},
	{0x1e4, 123368},
	{0x1e8, 124969},
	{0x1ec, 126520},
	{0x1f0, 128070},
	{0x1f4, 129571},
	{0x374, 131072},
	{0x376, 132526},
	{0x378, 133981},
	{0x37a, 135391},
	{0x37c, 136803},
	{0x37e, 138173},
	{0x380, 139544},
	{0x382, 140875},
	{0x384, 142208},
	{0x386, 143501},
	{0x388, 144798},
	{0x38a, 146057},
	{0x38c, 147320},
	{0x38e, 148546},
	{0x390, 149776},
	{0x392, 150971},
	{0x394, 152169},
	{0x396, 153335},
	{0x398, 154504},
	{0x39a, 155641},
	{0x39c, 156782},
	{0x39e, 157892},
	{0x3a0, 159007},
	{0x3a2, 160092},
	{0x3a4, 161181},
	{0x3a6, 162241},
	{0x3a8, 163306},
	{0x3aa, 164342},
	{0x3ac, 165384},
	{0x3ae, 166398},
	{0x3b0, 167417},
	{0x3b2, 168410},
	{0x3b4, 169408},
	{0x3b6, 170380},
	{0x3b8, 171357},
	{0x3ba, 172309},
	{0x3bc, 173267},
	{0x3be, 174201},
	{0x3c0, 175140},
	{0x3c2, 176055},
	{0x3c4, 176976},
	{0x3c6, 177873},
	{0x3c8, 178777},
	{0x3ca, 179657},
	{0x3cc, 180544},
	{0x3ce, 181408},
	{0x3d0, 182279},
	{0x3d2, 183128},
	{0x3d4, 183982},
	{0x3d6, 184816},
	{0x3d8, 185656},
	{0x3da, 186475},
	{0x3dc, 187300},
	{0x3de, 188105},
	{0x3e0, 188916},
	{0x3e2, 189708},
	{0x3e4, 190505},
	{0x3e6, 191284},
	{0x3e8, 192068},
	{0x3ea, 192834},
	{0x3ec, 193606},
	{0x3ee, 194359},
	{0x3f0, 195119},
	{0x3f2, 195860},
	{0x778, 196608},
	{0x779, 197337},
	{0x77a, 198073},
	{0x77b, 198792},
	{0x77c, 199517},
	{0x77d, 200225},
	{0x77e, 200939},
	{0x77f, 201636},
	{0x780, 202339},
	{0x781, 203027},
	{0x782, 203720},
	{0x783, 204397},
	{0x784, 205080},
	{0x785, 205748},
	{0x786, 206421},
	{0x787, 207080},
	{0x788, 207744},
	{0x789, 208393},
	{0x78a, 209048},
	{0x78b, 209688},
	{0x78c, 210334},
	{0x78d, 210966},
	{0x78e, 211603},
	{0x78f, 212227},
	{0x790, 212856},
	{0x791, 213471},
	{0x792, 214092},
	{0x793, 214699},
	{0x794, 215312},
	{0x795, 215911},
	{0x796, 216516},
	{0x797, 217108},
	{0x798, 217705},
	{0x799, 218290},
	{0x79a, 218880},
	{0x79b, 219457},
	{0x79c, 220040},
	{0x79d, 220610},
	{0x79e, 221186},
	{0x79f, 221749},
	{0x7a0, 222318},
	{0x7a1, 222875},
	{0x7a2, 223437},
	{0x7a3, 223987},
	{0x7a4, 224543},
	{0x7a5, 225087},
	{0x7a6, 225636},
	{0x7a7, 226174},
	{0x7a8, 226717},
	{0x7a9, 227248},
	{0x7aa, 227785},
	{0x7ab, 228311},
	{0x7ac, 228842},
	{0x7ad, 229361},
	{0x7ae, 229886},
	{0x7af, 230400},
	{0x7b0, 230920},
	{0x7b1, 231428},
	{0x7b2, 231942},
	{0x7b3, 232445},
	{0x7b4, 232953},
	{0x7b5, 233451},
	{0x7b6, 233954},
	{0x7b7, 234446},
	{0x7b8, 234944},
	{0x7b9, 235431},
	{0x7ba, 235923},
	{0x7bb, 236406},
	{0x7bc, 236893},
	{0x7bd, 237370},
	{0x7be, 237853},
	{0x7bf, 238326},
	{0x7c0, 238803},
	{0x7c1, 239271},
	{0x7c2, 239744},
	{0x7c3, 240207},
	{0x7c4, 240676},
	{0x7c5, 241134},
	{0x7c6, 241598},
	{0x7c7, 242052},
	{0x7c8, 242512},
	{0x7c9, 242961},
	{0x7ca, 243416},
	{0x7cb, 243862},
	{0x7cc, 244313},
	{0x7cd, 244754},
	{0x7ce, 245200},
	{0x7cf, 245638},
	{0x7d0, 246080},
	{0x7d1, 246513},
	{0x7d2, 246951},
	{0x7d3, 247381},
	{0x7d4, 247815},
	{0x7d5, 248240},
	{0x7d6, 248670},
	{0x7d7, 249092},
	{0x7d8, 249518},
	{0x7d9, 249936},
	{0x7da, 250359},
	{0x7db, 250773},
	{0x7dc, 251192},
	{0x7dd, 251602},
	{0x7de, 252018},
	{0x7df, 252424},
	{0x7e0, 252836},
	{0x7e1, 253240},
	{0x7e2, 253648},
	{0x7e3, 254048},
	{0x7e4, 254452},
	{0x7e5, 254849},
	{0x7e6, 255250},
	{0x7e7, 255644},
	{0x7e8, 256041},
	{0x7e9, 256431},
	{0x7ea, 256826},
	{0x7eb, 257213},
	{0x7ec, 257604},
	{0x7ed, 257988},
	{0x7ee, 258376},
	{0x7ef, 258757},
	{0x7f0, 259142},
	{0x7f1, 259519},
	{0x7f2, 259901},
	{0x7f3, 260276},
	{0x7f4, 260655},
	{0x7f5, 261026},
	{0x7f6, 261402},
	{0x7f7, 261770},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;

	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
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

struct tx_isp_mipi_bus sensor_mipi_linear = {
	.clk = 1008,
	.lans = 4,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
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
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus sensor_mipi_1080P_linear = {
	.clk = 1008,
	.lans = 4,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
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
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x4688,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x36,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.max_again = 261770,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_2688_1520_30fps_mipi[] = {
	{0x0103,0x01},
	{0x3638,0x00},
	{0x0300,0x00},
	{0x0302,0x2a},
	{0x0303,0x00},
	{0x0304,0x03},
	{0x030b,0x00},
	{0x030d,0x1e},
	{0x030e,0x04},
	{0x030f,0x01},
	{0x0312,0x01},
	{0x031e,0x00},
	{0x3000,0x20},
	{0x3002,0x00},
	{0x3018,0x72},
	{0x3020,0x93},
	{0x3021,0x03},
	{0x3022,0x01},
	{0x3031,0x0a},
	{0x3305,0xf1},
	{0x3307,0x04},
	{0x3309,0x29},
	{0x3500,0x00},
	{0x3501,0x60},
	{0x3502,0x00},
	{0x3503,0x04},
	{0x3504,0x00},
	{0x3505,0x00},
	{0x3506,0x00},
	{0x3507,0x00},
	{0x3508,0x00},
	{0x3509,0x80},
	{0x350a,0x00},
	{0x350b,0x00},
	{0x350c,0x00},
	{0x350d,0x00},
	{0x350e,0x00},
	{0x350f,0x80},
	{0x3510,0x00},
	{0x3511,0x00},
	{0x3512,0x00},
	{0x3513,0x00},
	{0x3514,0x00},
	{0x3515,0x80},
	{0x3516,0x00},
	{0x3517,0x00},
	{0x3518,0x00},
	{0x3519,0x00},
	{0x351a,0x00},
	{0x351b,0x80},
	{0x351c,0x00},
	{0x351d,0x00},
	{0x351e,0x00},
	{0x351f,0x00},
	{0x3520,0x00},
	{0x3521,0x80},
	{0x3522,0x08},
	{0x3524,0x08},
	{0x3526,0x08},
	{0x3528,0x08},
	{0x352a,0x08},
	{0x3602,0x00},
	{0x3604,0x02},
	{0x3605,0x00},
	{0x3606,0x00},
	{0x3607,0x00},
	{0x3609,0x12},
	{0x360a,0x40},
	{0x360c,0x08},
	{0x360f,0xe5},
	{0x3608,0x8f},
	{0x3611,0x00},
	{0x3613,0xf7},
	{0x3616,0x58},
	{0x3619,0x99},
	{0x361b,0x60},
	{0x361c,0x7a},
	{0x361e,0x79},
	{0x361f,0x02},
	{0x3632,0x00},
	{0x3633,0x10},
	{0x3634,0x10},
	{0x3635,0x10},
	{0x3636,0x15},
	{0x3646,0x86},
	{0x364a,0x0b},
	{0x3700,0x17},
	{0x3701,0x22},
	{0x3703,0x10},
	{0x370a,0x37},
	{0x3705,0x00},
	{0x3706,0x63},
	{0x3709,0x3c},
	{0x370b,0x01},
	{0x370c,0x30},
	{0x3710,0x24},
	{0x3711,0x0c},
	{0x3716,0x00},
	{0x3720,0x28},
	{0x3729,0x7b},
	{0x372a,0x84},
	{0x372b,0xbd},
	{0x372c,0xbc},
	{0x372e,0x52},
	{0x373c,0x0e},
	{0x373e,0x33},
	{0x3743,0x10},
	{0x3744,0x88},
	{0x374a,0x43},
	{0x374c,0x00},
	{0x374e,0x23},
	{0x3751,0x7b},
	{0x3752,0x84},
	{0x3753,0xbd},
	{0x3754,0xbc},
	{0x3756,0x52},
	{0x375c,0x00},
	{0x3760,0x00},
	{0x3761,0x00},
	{0x3762,0x00},
	{0x3763,0x00},
	{0x3764,0x00},
	{0x3767,0x04},
	{0x3768,0x04},
	{0x3769,0x08},
	{0x376a,0x08},
	{0x376b,0x20},
	{0x376c,0x00},
	{0x376d,0x00},
	{0x376e,0x00},
	{0x3773,0x00},
	{0x3774,0x51},
	{0x3776,0xbd},
	{0x3777,0xbd},
	{0x3781,0x18},
	{0x3783,0x25},
	{0x3800,0x00},
	{0x3801,0x08},
	{0x3802,0x00},
	{0x3803,0x04},
	{0x3804,0x0a},
	{0x3805,0x97},
	{0x3806,0x05},
	{0x3807,0xfb},
	{0x3808,0x0a},
	{0x3809,0x80},
	{0x380a,0x05},
	{0x380b,0xf0},
	{0x380c,0x0a}, //hts 0xa18 = 2584
	{0x380d,0x18}, //
	{0x380e,0x06}, //vts 0x612 = 1554
	{0x380f,0x12}, //
	{0x3810,0x00},
	{0x3811,0x08},
	{0x3812,0x00},
	{0x3813,0x04},
	{0x3814,0x01},
	{0x3815,0x01},
	{0x3819,0x01},
	{0x3820,0x00},
	{0x3821,0x06},
	{0x3829,0x00},
	{0x382a,0x01},
	{0x382b,0x01},
	{0x382d,0x7f},
	{0x3830,0x04},
	{0x3836,0x01},
	{0x3841,0x02},
	{0x3846,0x08},
	{0x3847,0x07},
	{0x3d85,0x36},
	{0x3d8c,0x71},
	{0x3d8d,0xcb},
	{0x3f0a,0x00},
	{0x4000,0x71},
	{0x4001,0x40},
	{0x4002,0x04},
	{0x4003,0x14},
	{0x400e,0x00},
	{0x4011,0x00},
	{0x401a,0x00},
	{0x401b,0x00},
	{0x401c,0x00},
	{0x401d,0x00},
	{0x401f,0x00},
	{0x4020,0x00},
	{0x4021,0x10},
	{0x4022,0x07},
	{0x4023,0xcf},
	{0x4024,0x09},
	{0x4025,0x60},
	{0x4026,0x09},
	{0x4027,0x6f},
	{0x4028,0x00},
	{0x4029,0x02},
	{0x402a,0x06},
	{0x402b,0x04},
	{0x402c,0x02},
	{0x402d,0x02},
	{0x402e,0x0e},
	{0x402f,0x04},
	{0x4302,0xff},
	{0x4303,0xff},
	{0x4304,0x00},
	{0x4305,0x00},
	{0x4306,0x00},
	{0x4308,0x02},
	{0x4500,0x6c},
	{0x4501,0xc4},
	{0x4502,0x40},
	{0x4503,0x02},
	{0x4601,0xA7},
	{0x4800,0x04},
	{0x4813,0x08},
	{0x481f,0x40},
	{0x4829,0x78},
	{0x4837,0x10},
	{0x4b00,0x2a},
	{0x4b0d,0x00},
	{0x4d00,0x04},
	{0x4d01,0x42},
	{0x4d02,0xd1},
	{0x4d03,0x93},
	{0x4d04,0xf5},
	{0x4d05,0xc1},
	{0x5000,0xf3},
	{0x5001,0x11},
	{0x5004,0x00},
	{0x500a,0x00},
	{0x500b,0x00},
	{0x5032,0x00},
	{0x5040,0x00},
	{0x5050,0x0c},
	{0x5500,0x00},
	{0x5501,0x10},
	{0x5502,0x01},
	{0x5503,0x0f},
	{0x8000,0x00},
	{0x8001,0x00},
	{0x8002,0x00},
	{0x8003,0x00},
	{0x8004,0x00},
	{0x8005,0x00},
	{0x8006,0x00},
	{0x8007,0x00},
	{0x8008,0x00},
	{0x3638,0x00},
	{0x3105,0x31},
	{0x301a,0xf9},
	{0x3508,0x07},
	{0x484b,0x05},
	{0x4805,0x03},
	{0x3601,0x01},
	{0x0100,0x01},
	{0xfffe,0x01},
	{0x3105,0x11},
	{0x301a,0xf1},
	{0x4805,0x00},
	{0x301a,0xf0},
	{0x3208,0x00},
	{0x302a,0x00},
	{0x302a,0x00},
	{0x302a,0x00},
	{0x302a,0x00},
	{0x302a,0x00},
	{0x3601,0x00},
	{0x3638,0x00},
	{0x3208,0x10},
	{0x3208,0xa0},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_25fps_mipi[] = {
	{0x0103,0x01},
	{0x3638,0x00},
	{0x0300,0x00},
	{0x0302,0x2a},
	{0x0303,0x00},
	{0x0304,0x03},
	{0x030b,0x00},
	{0x030d,0x1e},
	{0x030e,0x04},
	{0x030f,0x01},
	{0x0312,0x01},
	{0x031e,0x00},
	{0x3000,0x20},
	{0x3002,0x00},
	{0x3018,0x72},
	{0x3020,0x93},
	{0x3021,0x03},
	{0x3022,0x01},
	{0x3031,0x0a},
	{0x3305,0xf1},
	{0x3307,0x04},
	{0x3309,0x29},
	{0x3500,0x00},
	{0x3501,0x4c},
	{0x3502,0x00},
	{0x3503,0x04},
	{0x3504,0x00},
	{0x3505,0x00},
	{0x3506,0x00},
	{0x3507,0x00},
	{0x3508,0x00},
	{0x3509,0x80},
	{0x350a,0x00},
	{0x350b,0x00},
	{0x350c,0x00},
	{0x350d,0x00},
	{0x350e,0x00},
	{0x350f,0x80},
	{0x3510,0x00},
	{0x3511,0x00},
	{0x3512,0x00},
	{0x3513,0x00},
	{0x3514,0x00},
	{0x3515,0x80},
	{0x3516,0x00},
	{0x3517,0x00},
	{0x3518,0x00},
	{0x3519,0x00},
	{0x351a,0x00},
	{0x351b,0x80},
	{0x351c,0x00},
	{0x351d,0x00},
	{0x351e,0x00},
	{0x351f,0x00},
	{0x3520,0x00},
	{0x3521,0x80},
	{0x3522,0x08},
	{0x3524,0x08},
	{0x3526,0x08},
	{0x3528,0x08},
	{0x352a,0x08},
	{0x3602,0x00},
	{0x3604,0x02},
	{0x3605,0x00},
	{0x3606,0x00},
	{0x3607,0x00},
	{0x3609,0x12},
	{0x360a,0x40},
	{0x360c,0x08},
	{0x360f,0xe5},
	{0x3608,0x8f},
	{0x3611,0x00},
	{0x3613,0xf7},
	{0x3616,0x58},
	{0x3619,0x99},
	{0x361b,0x60},
	{0x361c,0x7a},
	{0x361e,0x79},
	{0x361f,0x02},
	{0x3632,0x00},
	{0x3633,0x10},
	{0x3634,0x10},
	{0x3635,0x10},
	{0x3636,0x15},
	{0x3646,0x86},
	{0x364a,0x0b},
	{0x3700,0x17},
	{0x3701,0x22},
	{0x3703,0x10},
	{0x370a,0x37},
	{0x3705,0x00},
	{0x3706,0x63},
	{0x3709,0x3c},
	{0x370b,0x01},
	{0x370c,0x30},
	{0x3710,0x24},
	{0x3711,0x0c},
	{0x3716,0x00},
	{0x3720,0x28},
	{0x3729,0x7b},
	{0x372a,0x84},
	{0x372b,0xbd},
	{0x372c,0xbc},
	{0x372e,0x52},
	{0x373c,0x0e},
	{0x373e,0x33},
	{0x3743,0x10},
	{0x3744,0x88},
	{0x374a,0x43},
	{0x374c,0x00},
	{0x374e,0x23},
	{0x3751,0x7b},
	{0x3752,0x84},
	{0x3753,0xbd},
	{0x3754,0xbc},
	{0x3756,0x52},
	{0x375c,0x00},
	{0x3760,0x00},
	{0x3761,0x00},
	{0x3762,0x00},
	{0x3763,0x00},
	{0x3764,0x00},
	{0x3767,0x04},
	{0x3768,0x04},
	{0x3769,0x08},
	{0x376a,0x08},
	{0x376b,0x20},
	{0x376c,0x00},
	{0x376d,0x00},
	{0x376e,0x00},
	{0x3773,0x00},
	{0x3774,0x51},
	{0x3776,0xbd},
	{0x3777,0xbd},
	{0x3781,0x18},
	{0x3783,0x25},
	{0x3800,0x01},
	{0x3801,0x88},
	{0x3802,0x00},
	{0x3803,0xe0},
	{0x3804,0x09},
	{0x3805,0x17},
	{0x3806,0x05},
	{0x3807,0x1f},
	{0x3808,0x07},
	{0x3809,0x80},
	{0x380a,0x04},
	{0x380b,0x38},
	{0x380c,0x0f},//hts 0xf50 = 3920
	{0x380d,0x50},//
	{0x380e,0x04},//vts 0x48a = 1162
	{0x380f,0x8A},//
	{0x3810,0x00},
	{0x3811,0x08},
	{0x3812,0x00},
	{0x3813,0x04},
	{0x3814,0x01},
	{0x3815,0x01},
	{0x3819,0x01},
	{0x3820,0x00},
	{0x3821,0x06},
	{0x3829,0x00},
	{0x382a,0x01},
	{0x382b,0x01},
	{0x382d,0x7f},
	{0x3830,0x04},
	{0x3836,0x01},
	{0x3841,0x02},
	{0x3846,0x08},
	{0x3847,0x07},
	{0x3d85,0x36},
	{0x3d8c,0x71},
	{0x3d8d,0xcb},
	{0x3f0a,0x00},
	{0x4000,0x71},
	{0x4001,0x40},
	{0x4002,0x04},
	{0x4003,0x14},
	{0x400e,0x00},
	{0x4011,0x00},
	{0x401a,0x00},
	{0x401b,0x00},
	{0x401c,0x00},
	{0x401d,0x00},
	{0x401f,0x00},
	{0x4020,0x00},
	{0x4021,0x10},
	{0x4022,0x06},
	{0x4023,0x13},
	{0x4024,0x07},
	{0x4025,0x40},
	{0x4026,0x07},
	{0x4027,0x50},
	{0x4028,0x00},
	{0x4029,0x02},
	{0x402a,0x06},
	{0x402b,0x04},
	{0x402c,0x02},
	{0x402d,0x02},
	{0x402e,0x0e},
	{0x402f,0x04},
	{0x4302,0xff},
	{0x4303,0xff},
	{0x4304,0x00},
	{0x4305,0x00},
	{0x4306,0x00},
	{0x4308,0x02},
	{0x4500,0x6c},
	{0x4501,0xc4},
	{0x4502,0x40},
	{0x4503,0x02},
	{0x4601,0x77},
	{0x4800,0x04},
	{0x4813,0x08},
	{0x481f,0x40},
	{0x4829,0x78},
	{0x4837,0x10},
	{0x4b00,0x2a},
	{0x4b0d,0x00},
	{0x4d00,0x04},
	{0x4d01,0x42},
	{0x4d02,0xd1},
	{0x4d03,0x93},
	{0x4d04,0xf5},
	{0x4d05,0xc1},
	{0x5000,0xf3},
	{0x5001,0x11},
	{0x5004,0x00},
	{0x500a,0x00},
	{0x500b,0x00},
	{0x5032,0x00},
	{0x5040,0x00},
	{0x5050,0x0c},
	{0x5500,0x00},
	{0x5501,0x10},
	{0x5502,0x01},
	{0x5503,0x0f},
	{0x8000,0x00},
	{0x8001,0x00},
	{0x8002,0x00},
	{0x8003,0x00},
	{0x8004,0x00},
	{0x8005,0x00},
	{0x8006,0x00},
	{0x8007,0x00},
	{0x8008,0x00},
	{0x3638,0x00},
	{0x3105,0x31},
	{0x301a,0xf9},
	{0x3508,0x07},
	{0x484b,0x05},
	{0x4805,0x03},
	{0x3601,0x01},
	{0x0100,0x01},
	{0x3105,0x11},
	{0x301a,0xf1},
	{0x4805,0x00},
	{0x301a,0xf0},
	{0x3208,0x00},
	{0x302a,0x00},
	{0x302a,0x00},
	{0x302a,0x00},
	{0x302a,0x00},
	{0x302a,0x00},
	{0x3601,0x00},
	{0x3638,0x00},
	{0x3208,0x10},
	{0x3208,0xa0},
	{SENSOR_REG_END, 0x00},
};
/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 2688*1520_mipi*/
	{
		.width = 2688,
		.height = 1520,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2688_1520_30fps_mipi,
	},
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_25fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

static struct regval_list sensor_stream_on_mipi[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,
	unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg>>8)&0xff, reg&0xff};
	int ret;
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

	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int sensor_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg>>8)&0xff, reg&0xff, value};
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
		pr_debug("vals->reg_num:0x%02x, vals->value:0x%02x\n",vals->reg_num, val);
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
	unsigned char v;
	int ret;
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
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{

	int ret = 0;

	ret = sensor_write(sd, 0x3502, (unsigned char)((value & 0xff) << 4));
	ret += sensor_write(sd, 0x3501, (unsigned char)((value >> 4) & 0xff));
	ret += sensor_write(sd, 0x3500, (unsigned char)((value >> 12) & 0xf));
	if (ret < 0)
		return ret;
	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = -1;

	ret = sensor_write(sd, 0x3509, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3508, (unsigned char)((value >> 8) & 0xff));
	if (ret < 0)
		return ret;

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

	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable)
		return ISP_SUCCESS;

	sensor_set_attr(sd, wsize);
	sensor->video.state = TX_ISP_MODULE_DEINIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

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
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int max_fps = 0;
	unsigned int newformat = 0; //the format is 24.8

	switch(info->default_boot) {
		case 0:
			sclk = 2584 * 1554 * 30;
			max_fps = 30;
			break;
		case 1:
			sclk = 3920 * 1162 * 25;
			max_fps = 25;
			break;
		default:
			ISP_ERROR("Have no this setting!!!\n");
	}

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || fps < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}


	ret += sensor_read(sd, 0x380c, &val);
	hts = val;
	ret += sensor_read(sd, 0x380d, &val);
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}
	hts = ((hts << 8) + val);

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += sensor_write(sd, 0x380f, vts & 0xff);
	ret += sensor_write(sd, 0x380e, (vts >> 8) & 0xff);
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts -4;
	sensor->video.attr->integration_time_limit = vts -4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts -4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

struct clk *sclka;
static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);

	switch(info->default_boot) {
	case 0:
		wsize = &sensor_win_sizes[0];
		memcpy(&sensor_attr.mipi, &sensor_mipi_linear, sizeof(sensor_mipi_linear));
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
		sensor_attr.max_integration_time_native = 1550,
		sensor_attr.integration_time_limit = 1550,
		sensor_attr.total_width = 2584,
		sensor_attr.total_height = 1554,
		sensor_attr.max_integration_time = 1550,
		sensor_attr.one_line_expr_in_us = 30;
	    sensor_attr.again = 0x80;
        sensor_attr.integration_time = 0x400;
		break;
	case 1:
		wsize = &sensor_win_sizes[1];
		memcpy(&sensor_attr.mipi, &sensor_mipi_1080P_linear, sizeof(sensor_mipi_1080P_linear));
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
		sensor_attr.max_integration_time_native = 1158,
		sensor_attr.integration_time_limit = 1158,
		sensor_attr.total_width = 3920,
		sensor_attr.total_height = 1162,
		sensor_attr.max_integration_time = 1158,
		sensor_attr.one_line_expr_in_us = 30;
	    sensor_attr.again = 0x80;
        sensor_attr.integration_time = 0x400;
		break;
	default:
		ISP_ERROR("Have no this setting!!!\n");
	}

	switch(info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this interface!!!\n");
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

	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

    private_clk_set_rate(sensor->mclk, MCLK);
    private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
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
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(20);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(150);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
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

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sensor_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sensor_set_analog_gain(sd, sensor_val->value);
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
//	.fsync = sensor_frame_sync,
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
	sensor->dev = &client->dev;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sensor_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	ISP_WARNING("probe ok ------->%s\n", SENSOR_NAME);

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
	private_devm_clk_put(&client->dev, sensor->mclk);
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
