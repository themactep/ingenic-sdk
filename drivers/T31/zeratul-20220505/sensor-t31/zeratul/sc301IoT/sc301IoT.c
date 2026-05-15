/*
 * sc301iot.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
#ifdef FAST_AE
#include <asm/div64.h>
#endif

#include <tx-isp-common.h>
#include <sensor-common.h>
#include <txx-funcs.h>

//#define DEBUG
//#define FAST_AE
//#define SENSOR_WITHOUT_INIT
extern int riscv_is_pass;
#define SC301IOT_CHIP_ID_H	(0xcc)
#define SC301IOT_CHIP_ID_L	(0x40)
#define SC301IOT_REG_END	0xffff
#define SC301IOT_REG_DELAY	0xfffe
#define SC301IOT_FAST_AE	0xfffd
#define SC301IOT_SUPPORT_30FPS_SCLK (54000000)
//#define SC301IOT_SUPPORT_25FPS_SCLK (86375000)
#define SC301IOT_SUPPORT_100FPS_SCLK (54000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20211118a"

#ifdef FAST_AE
#define L_Target  0x2a
unsigned long long  Big_Exp = 0;
#endif
static int reset_gpio = GPIO_PA(18);

static int pwdn_gpio = -1;

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;

static int sensor_max_fps = TX_SENSOR_MAX_FPS_30;
static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
static int sensor_resolution = TX_SENSOR_RES_100;

static int shvflip = 0;

int fast_wdr_mode = 0;
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

struct again_lut sc301iot_again_lut[] = {
    {0x80, 0},
    {0x84, 2886},
    {0x88, 5776},
    {0x8c, 8494},
    {0x90, 11136},
    {0x94, 13706},
    {0x98, 16287},
    {0x9c, 18723},
    {0xa0, 21097},
    {0xa4, 23413},
    {0xa8, 25746},
    {0xac, 27952},
    {0xb0, 30108},
    {0xb4, 32216},
    {0xb8, 34344},
    {0xbc, 36361},
    {0xc0, 38335},
    {0xc4, 40269},
    {0xc8, 42225},
    {0x4080, 42587},
    {0x4084, 45495},
    {0x4088, 48372},
    {0x408c, 51055},
    {0x4090, 53716},
    {0x4094, 56305},
    {0x4098, 58876},
    {0x409c, 61330},
    {0x40a0, 63673},
    {0x40a4, 66006},
    {0x40a8, 68329},
    {0x40ac, 70552},
    {0x40b0, 72680},
    {0x40b4, 74803},
    {0x40b8, 76922},
    {0x40bc, 78954},
    {0x40c0, 80903},
    {0x40c4, 82851},
    {0x40c8, 84799},
    {0x40cc, 86670},
    {0x40d0, 88504},
    {0x40d4, 90268},
    {0x40d8, 92070},
    {0x40dc, 93803},
    {0x40e0, 95506},
    {0x40e4, 97144},
    {0x40e8, 98854},
    {0x40ec, 100436},
    {0x40f0, 102024},
    {0x40f4, 103586},
    {0x40f8, 105153},
    {0x40fc, 106634},
    {0x4880, 108122},
    {0x4884, 111000},
    {0x4888, 113907},
    {0x488c, 116617},
    {0x4890, 119251},
    {0x4894, 121840},
    {0x4898, 124411},
    {0x489c, 126840},
    {0x48a0, 129232},
    {0x48a4, 131541},
    {0x48a8, 133864},
    {0x48ac, 136064},
    {0x48b0, 138236},
    {0x48b4, 140338},
    {0x48b8, 142457},
    {0x48bc, 144489},
    {0x48c0, 146458},
    {0x48c4, 148386},
    {0x48c8, 150353},
    {0x48cc, 152205},
    {0x48d0, 154021},
    {0x48d4, 155821},
    {0x48d8, 157623},
    {0x48dc, 159338},
    {0x48e0, 161024},
    {0x48e4, 162696},
    {0x48e8, 164373},
    {0x48ec, 165971},
    {0x48f0, 167559},
    {0x48f4, 169105},
    {0x48f8, 170673},
    {0x48fc, 172184},
    {0x4980, 173657},
    {0x4984, 176550},
    {0x4988, 179428},
    {0x498c, 182152},
    {0x4990, 184800},
    {0x4994, 187362},
    {0x4998, 189946},
    {0x499c, 192375},
    {0x49a0, 194755},
    {0x49a4, 197076},
    {0x49a8, 199399},
    {0x49ac, 201611},
    {0x49b0, 203771},
    {0x49b4, 205873},
    {0x49b8, 208003},
    {0x49bc, 210014},
    {0x49c0, 211993},
    {0x49c4, 213931},
    {0x49c8, 215879},
    {0x49cc, 217740},
    {0x49d0, 219556},
    {0x49d4, 221347},
    {0x49d8, 223158},
    {0x49dc, 224873},
    {0x49e0, 226567},
    {0x49e4, 228231},
    {0x49e8, 229908},
    {0x49ec, 231514},
    {0x49f0, 233086},
    {0x49f4, 234640},
    {0x49f8, 236216},
    {0x49fc, 237712},
    {0x4b80, 239192},
    {0x4b84, 242078},
    {0x4b88, 244970},
    {0x4b8c, 247687},
    {0x4b90, 250328},
    {0x4b94, 252897},
    {0x4b98, 255481},
    {0x4b9c, 257916},
    {0x4ba0, 260290},
    {0x4ba4, 262605},
    {0x4ba8, 264940},
    {0x4bac, 267146},
    {0x4bb0, 269301},
    {0x4bb4, 271408},
    {0x4bb8, 273538},
    {0x4bbc, 275554},
    {0x4bc0, 277528},
    {0x4bc4, 279461},
    {0x4bc8, 281419},
    {0x4bcc, 283275},
    {0x4bd0, 285095},
    {0x4bd4, 286882},
    {0x4bd8, 288693},
    {0x4bdc, 290413},
    {0x4be0, 292102},
    {0x4be4, 293762},
    {0x4be8, 295447},
    {0x4bec, 297049},
    {0x4bf0, 298625},
    {0x4bf4, 300175},
    {0x4bf8, 301751},
    {0x4bfc, 303251},
    {0x4f80, 304727},
    {0x4f84, 307613},
    {0x4f88, 310505},
    {0x4f8c, 313222},
    {0x4f90, 315863},
    {0x4f94, 318432},
    {0x4f98, 321016},
    {0x4f9c, 323451},
    {0x4fa0, 325825},
    {0x4fa4, 328140},
    {0x4fa8, 330475},
    {0x4fac, 332681},
    {0x4fb0, 334836},
    {0x4fb4, 336943},
    {0x4fb8, 339073},
    {0x4fbc, 341089},
    {0x4fc0, 343063},
    {0x4fc4, 344996},
    {0x4fc8, 346954},
    {0x4fcc, 348810},
    {0x4fd0, 350630},
    {0x4fd4, 352417},
    {0x4fd8, 354228},
    {0x4fdc, 355948},
    {0x4fe0, 357637},
    {0x4fe4, 359297},
    {0x4fe8, 360982},
    {0x4fec, 362584},
    {0x4ff0, 364160},
    {0x4ff4, 365710},
    {0x4ff8, 367286},
    {0x4ffc, 368786},
    {0x5f80, 370262},
    {0x5f84, 373148},
    {0x5f88, 376038},
    {0x5f8c, 378757},
    {0x5f90, 381398},
    {0x5f94, 383967},
    {0x5f98, 386550},
    {0x5f9c, 388986},
    {0x5fa0, 391360},
    {0x5fa4, 393675},
    {0x5fa8, 396008},
    {0x5fac, 398216},
    {0x5fb0, 400371},
    {0x5fb4, 402478},
    {0x5fb8, 404606},
    {0x5fbc, 406624},
    {0x5fc0, 408598},
    {0x5fc4, 410531},
    {0x5fc8, 412487},
    {0x5fcc, 414345},
    {0x5fd0, 416165},
    {0x5fd4, 417952},
    {0x5fd8, 419762},
    {0x5fdc, 421483},
    {0x5fe0, 423172},
    {0x5fe4, 424832},
    {0x5fe8, 426516},
    {0x5fec, 428119},
    {0x5ff0, 429695},
    {0x5ff4, 431245},
    {0x5ff8, 432820},
    {0x5ffc, 434321},
};

struct tx_isp_sensor_attribute sc301iot_attr;

unsigned int sc301iot_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc301iot_again_lut;
	while(lut->gain <= sc301iot_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == sc301iot_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc301iot_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef FAST_AE
static uint8_t sc301iot_leading_one_position(const uint32_t in)
{
	uint8_t pos = 0;
	uint32_t val = in;

	if (val >= 1<<16) { val >>= 16; pos += 16; }
	if (val >= 1<< 8) { val >>=  8; pos +=  8; }
	if (val >= 1<< 4) { val >>=  4; pos +=  4; }
	if (val >= 1<< 2) { val >>=  2; pos +=  2; }
	if (val >= 1<< 1) {             pos +=  1; }

	return pos;
}

uint32_t sc301iot_log2_int_to_fixed(const uint32_t val, const uint8_t out_precision, const uint8_t shift_out)
{
	int i;
	int pos = 0;
	uint32_t a = 0;
	uint32_t b = 0;
	uint32_t in = val;
	uint32_t result = 0;
	const unsigned char precision = out_precision;

	if(0 == val)
	{
		return 0;
	}
	// integral part
	pos = sc301iot_leading_one_position(val);
	// fractional part
	a = (pos <= 15) ? (in << (15 - pos)) : (in>>(pos - 15));
	for(i = 0; i < precision; ++i)
	{
		b = a * a;
		if(b & (1<<31))
		{
			result = (result << 1) + 1;
			a = b >> 16;
		}
		else
		{
			result = (result << 1);
			a = b >> 15;
		}
	}

	return (((pos << precision) + result) << shift_out) | ((a & 0x7fff)>> (15-shift_out));
}
#endif

struct tx_isp_sensor_attribute sc301iot_attr={
	.name = "sc301iot",
	.chip_id = 0xcc40,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
		.dvp_hcomp_en = 0,
	},
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 540,
		.lans = 2,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 2000,
		.image_theight = 1504,
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
	.max_again = 434321,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 0x640 - 4,
	.integration_time_limit = 0x640 - 4,
	.total_width = 0x465,
	.total_height = 0x640,
	.max_integration_time = 0x640 - 4,
	.one_line_expr_in_us = 25,
	.expo_fs = 1,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc301iot_alloc_again,
	.sensor_ctrl.alloc_dgain = sc301iot_alloc_dgain,
};

static struct regval_list sc301iot_init_regs_2000_1504_30fps_mipi[] = {
    {0x0103,0x01},
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x37f9,0x80},
    {0x301c,0x78},
    {0x301f,0x42},
    {0x30b8,0x44},
    {0x3200,0x00},
    {0x3201,0x00},
    {0x3202,0x00},
    {0x3203,0x10},
    {0x3204,0x08},
    {0x3205,0x07},
    {0x3206,0x05},
    {0x3207,0xf7},
    {0x3208,0x07},
    {0x3209,0xd0},
    {0x320a,0x05},
    {0x320b,0xe0},
    {0x320c,0x04},
    {0x320d,0x65},
    {0x320e,0x06},
    {0x320f,0x40},
    {0x3210,0x00},
    {0x3211,0x1c},
    {0x3212,0x00},
    {0x3213,0x04},
    {0x3214,0x11},
    {0x3215,0x11},
    {0x3223,0xc0},
    {0x3253,0x0c},
    {0x3274,0x09},
    {0x3301,0x08},
    {0x3306,0x58},
    {0x3308,0x08},
    {0x330a,0x00},
    {0x330b,0xe0},
    {0x330e,0x10},
    {0x3314,0x14},
    {0x331e,0x55},
    {0x331f,0x7d},
    {0x3333,0x10},
    {0x3334,0x40},
    {0x335e,0x06},
    {0x335f,0x08},
    {0x3364,0x5e},
    {0x337c,0x02},
    {0x337d,0x0a},
    {0x3390,0x01},
    {0x3391,0x03},
    {0x3392,0x07},
    {0x3393,0x08},
    {0x3394,0x08},
    {0x3395,0x08},
    {0x3396,0x08},
    {0x3397,0x09},
    {0x3398,0x1f},
    {0x3399,0x08},
    {0x339a,0x0a},
    {0x339b,0x40},
    {0x339c,0x88},
    {0x33a2,0x04},
    {0x33ad,0x0c},
    {0x33b1,0x80},
    {0x33b3,0x30},
    {0x33f9,0x68},
    {0x33fb,0x80},
    {0x33fc,0x48},
    {0x33fd,0x5f},
    {0x349f,0x03},
    {0x34a6,0x48},
    {0x34a7,0x5f},
    {0x34a8,0x30},
    {0x34a9,0x30},
    {0x34aa,0x00},
    {0x34ab,0xf0},
    {0x34ac,0x01},
    {0x34ad,0x08},
    {0x34f8,0x5f},
    {0x34f9,0x10},
    {0x3630,0xf0},
    {0x3631,0x85},
    {0x3632,0x74},
    {0x3633,0x22},
    {0x3637,0x4d},
    {0x3638,0xcb},
    {0x363a,0x8b},
    {0x363c,0x08},
    {0x3640,0x00},
    {0x3641,0x00},
    {0x3670,0x4e},
    {0x3674,0xc0},
    {0x3675,0xb0},
    {0x3676,0xa0},
    {0x3677,0x83},
    {0x3678,0x87},
    {0x3679,0x8a},
    {0x367c,0x49},
    {0x367d,0x4f},
    {0x367e,0x48},
    {0x367f,0x4b},
    {0x3690,0x33},
    {0x3691,0x33},
    {0x3692,0x44},
    {0x3699,0x8a},
    {0x369a,0xa1},
    {0x369b,0xc2},
    {0x369c,0x48},
    {0x369d,0x4f},
    {0x36a2,0x4b},
    {0x36a3,0x4f},
    {0x36ea,0x09},
    {0x36eb,0x0d},
    {0x36ec,0x1c},
    {0x36ed,0x25},
    {0x370f,0x01},
    {0x3714,0x80},
    {0x3722,0x09},
    {0x3724,0x41},
    {0x3725,0xc1},
    {0x3728,0x00},
    {0x3771,0x09},
    {0x3772,0x05},
    {0x3773,0x05},
    {0x377a,0x48},
    {0x377b,0x49},
    {0x37fa,0x06},
    {0x37fb,0x33},
    {0x37fc,0x11},
    {0x37fd,0x38},
    {0x3905,0x8d},
    {0x391d,0x08},
    {0x3922,0x1a},
    {0x3926,0x21},
    {0x3933,0x80},
    {0x3934,0x0d},
    {0x3937,0x6a},
    {0x3939,0x00},
    {0x393a,0x0e},
    {0x39dc,0x02},
    {0x3e00,0x00},
    {0x3e01,0x63},
    {0x3e02,0x80},
    {0x3e03,0x0b},
    {0x3e1b,0x2a},
    {0x4407,0x34},
    {0x440e,0x02},
    {0x5001,0x40},
    {0x5007,0x80},
	{SC301IOT_FAST_AE, 0x00},	/* END MARKER */
    {0x36e9,0x24},
    {0x37f9,0x24},
    {0x0100,0x01},

#if 0
    {0x0103,0x01},
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x37f9,0x80},
    {0x301c,0x78},
    {0x301f,0x08},
    {0x30b8,0x44},
    {0x3200,0x00},
    {0x3201,0x00},
    {0x3202,0x00},
    {0x3203,0x10},
    {0x3204,0x08},
    {0x3205,0x07},
    {0x3206,0x05},
    {0x3207,0xf7},
    {0x3208,0x07},
    {0x3209,0xd0},
    {0x320a,0x05},
    {0x320b,0xe0},
    {0x320c,0x04},
    {0x320d,0x65},
    {0x320e,0x06},
    {0x320f,0x40},
    {0x3210,0x00},
    {0x3211,0x1c},
    {0x3212,0x00},
    {0x3213,0x04},
    {0x3214,0x11},
    {0x3215,0x11},
    {0x3223,0xc0},
    {0x3253,0x0c},
    {0x3274,0x09},
    {0x3301,0x08},
    {0x3306,0x58},
    {0x3308,0x08},
    {0x330a,0x00},
    {0x330b,0xe0},
    {0x330e,0x10},
    {0x331e,0x55},
    {0x331f,0x7d},
    {0x3333,0x10},
    {0x3334,0x40},
    {0x335e,0x06},
    {0x335f,0x08},
    {0x3364,0x5e},
    {0x337c,0x02},
    {0x337d,0x0a},
    {0x3390,0x01},
    {0x3391,0x03},
    {0x3392,0x07},
    {0x3393,0x08},
    {0x3394,0x08},
    {0x3395,0x08},
    {0x3396,0x08},
    {0x3397,0x09},
    {0x3398,0x1f},
    {0x3399,0x08},
    {0x339a,0x20},
    {0x339b,0x40},
    {0x339c,0x78},
    {0x33a2,0x04},
    {0x33ad,0x0c},
    {0x33b1,0x80},
    {0x33b3,0x30},
    {0x33f9,0x68},
    {0x33fb,0x88},
    {0x33fc,0x48},
    {0x33fd,0x5f},
    {0x349f,0x03},
    {0x34a6,0x48},
    {0x34a7,0x5f},
    {0x34a8,0x30},
    {0x34a9,0x30},
    {0x34aa,0x00},
    {0x34ab,0xf0},
    {0x34ac,0x01},
    {0x34ad,0x12},
    {0x34f8,0x5f},
    {0x34f9,0x10},
    {0x3630,0xf0},
    {0x3631,0x85},
    {0x3632,0x74},
    {0x3633,0x22},
    {0x3637,0x4d},
    {0x3638,0xcb},
    {0x363a,0x8b},
    {0x3641,0x00},
    {0x3670,0x4e},
    {0x3674,0xf0},
    {0x3675,0xc0},
    {0x3676,0xc0},
    {0x3677,0x85},
    {0x3678,0x8a},
    {0x3679,0x8d},
    {0x367c,0x48},
    {0x367d,0x49},
    {0x367e,0x49},
    {0x367f,0x5f},
    {0x3690,0x22},
    {0x3691,0x33},
    {0x3692,0x44},
    {0x3699,0x88},
    {0x369a,0x98},
    {0x369b,0xc4},
    {0x369c,0x48},
    {0x369d,0x5f},
    {0x36a2,0x49},
    {0x36a3,0x4f},
    {0x36ea,0x09},
    {0x36eb,0x0d},
    {0x36ec,0x1c},
    {0x36ed,0x25},
    {0x370f,0x01},
    {0x3714,0x80},
    {0x3722,0x09},
    {0x3724,0x41},
    {0x3725,0xc1},
    {0x3728,0x00},
    {0x3771,0x09},
    {0x3772,0x05},
    {0x3773,0x05},
    {0x377a,0x48},
    {0x377b,0x49},
    {0x37fa,0x06},
    {0x37fb,0x33},
    {0x37fc,0x11},
    {0x37fd,0x38},
    {0x3905,0x8d},
    {0x391d,0x08},
    {0x3922,0x1a},
    {0x3926,0x21},
    {0x3933,0x80},
    {0x3934,0x02},
    {0x3937,0x72},
    {0x3939,0x00},
    {0x393a,0x03},
    {0x39dc,0x02},
    {0x3e00,0x00},
    {0x3e01,0x63},
    {0x3e02,0xc0},
    {0x3e03,0x0b},
    {0x3e1b,0x2a},
    {0x4407,0x34},
    {0x440e,0x02},
    {0x5001,0x40},
    {0x5007,0x80},
	{SC301IOT_FAST_AE, 0x00},	/* END MARKER */
    {0x36e9,0x24},
    {0x37f9,0x24},
    {0x0100,0x01},
#endif
	{SC301IOT_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc301iot_init_regs_248_184_100fps_mipi[] = {
    {0x0103,0x01},
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x37f9,0x80},
    {0x301c,0x78},
    {0x301f,0x44},
    {0x30b8,0x44},
    {0x320c,0x04},
    {0x320d,0x65},
    {0x320e,0x01},
    {0x320f,0xe0},
    {0x3223,0xc0},
    {0x3253,0x0c},
    {0x3274,0x09},
    {0x3301,0x08},
    {0x3306,0x58},
    {0x3308,0x08},
    {0x330a,0x00},
    {0x330b,0xe0},
    {0x330e,0x10},
    {0x3314,0x14},
    {0x331e,0x55},
    {0x331f,0x7d},
    {0x3333,0x10},
    {0x3334,0x40},
    {0x335e,0x06},
    {0x335f,0x08},
    {0x3364,0x5e},
    {0x337c,0x02},
    {0x337d,0x0a},
    {0x3390,0x01},
    {0x3391,0x03},
    {0x3392,0x07},
    {0x3393,0x08},
    {0x3394,0x08},
    {0x3395,0x08},
    {0x3396,0x08},
    {0x3397,0x09},
    {0x3398,0x1f},
    {0x3399,0x08},
    {0x339a,0x0a},
    {0x339b,0x40},
    {0x339c,0x88},
    {0x33a2,0x04},
    {0x33ad,0x0c},
    {0x33b1,0x80},
    {0x33b3,0x30},
    {0x33f9,0x68},
    {0x33fb,0x80},
    {0x33fc,0x48},
    {0x33fd,0x5f},
    {0x349f,0x03},
    {0x34a6,0x48},
    {0x34a7,0x5f},
    {0x34a8,0x30},
    {0x34a9,0x30},
    {0x34aa,0x00},
    {0x34ab,0xf0},
    {0x34ac,0x01},
    {0x34ad,0x08},
    {0x34f8,0x5f},
    {0x34f9,0x10},
    {0x3630,0xf0},
    {0x3631,0x85},
    {0x3632,0x74},
    {0x3633,0x22},
    {0x3637,0x4d},
    {0x3638,0xcb},
    {0x363a,0x8b},
    {0x363c,0x08},
    {0x3640,0x00},
    {0x3641,0x00},
    {0x3670,0x4e},
    {0x3674,0xc0},
    {0x3675,0xb0},
    {0x3676,0xa0},
    {0x3677,0x83},
    {0x3678,0x87},
    {0x3679,0x8a},
    {0x367c,0x49},
    {0x367d,0x4f},
    {0x367e,0x48},
    {0x367f,0x4b},
    {0x3690,0x33},
    {0x3691,0x33},
    {0x3692,0x44},
    {0x3699,0x8a},
    {0x369a,0xa1},
    {0x369b,0xc2},
    {0x369c,0x48},
    {0x369d,0x4f},
    {0x36a2,0x4b},
    {0x36a3,0x4f},
    {0x36ea,0x09},
    {0x36eb,0x0d},
    {0x36ec,0x1c},
    {0x36ed,0x25},
    {0x370f,0x01},
    {0x3714,0x80},
    {0x3722,0x09},
    {0x3724,0x41},
    {0x3725,0xc1},
    {0x3728,0x00},
    {0x3771,0x09},
    {0x3772,0x05},
    {0x3773,0x05},
    {0x377a,0x48},
    {0x377b,0x49},
    {0x37fa,0x06},
    {0x37fb,0x33},
    {0x37fc,0x11},
    {0x37fd,0x38},
    {0x3905,0x8d},
    {0x391d,0x08},
    {0x3922,0x1a},
    {0x3926,0x21},
    {0x3933,0x80},
    {0x3934,0x0d},
    {0x3937,0x6a},
    {0x3939,0x00},
    {0x393a,0x0e},
    {0x39dc,0x02},
    {0x3e00,0x00},
    {0x3e01,0x1d},
    {0x3e02,0x80},
    {0x3e03,0x0b},
    {0x3e1b,0x2a},
    {0x4407,0x34},
    {0x440e,0x02},
    {0x5001,0x40},
    {0x5007,0x80},
    {0x36e9,0x24},
    {0x37f9,0x24},
    {0x0100,0x01},

    {SC301IOT_REG_END, 0x00},	/* END MARKER */

};
    
static struct regval_list sc301iot_init_regs_1024_768_100fps_mipi[] = {
#if 1
    {0x0103,0x01},
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x37f9,0x80},
    {0x301c,0x78},
    {0x301f,0x0f},
    {0x30b8,0x44},
    {0x3208,0x04},
    {0x3209,0x00},
    {0x320a,0x03},
    {0x320b,0x00},
    {0x320c,0x05},
    {0x320d,0x46},
    {0x320e,0x03},
    {0x320f,0x20},
    {0x3211,0x02},
    {0x3213,0x02},
    {0x3214,0x11},
    {0x3215,0x31},
    {0x3220,0x01},
    {0x3223,0xc0},
    {0x3253,0x0c},
    {0x3274,0x09},
    {0x3301,0x08},
    {0x3304,0x80},
    {0x3306,0x58},
    {0x3308,0x08},
    {0x3309,0xa0},
    {0x330a,0x00},
    {0x330b,0xe0},
    {0x330e,0x10},
    {0x331e,0x71},
    {0x331f,0x91},
    {0x3333,0x10},
    {0x3334,0x40},
    {0x335e,0x06},
    {0x335f,0x08},
    {0x3364,0x5e},
    {0x337c,0x02},
    {0x337d,0x0a},
    {0x3390,0x01},
    {0x3391,0x03},
    {0x3392,0x07},
    {0x3393,0x08},
    {0x3394,0x08},
    {0x3395,0x08},
    {0x3396,0x08},
    {0x3397,0x09},
    {0x3398,0x1f},
    {0x3399,0x08},
    {0x339a,0x1c},
    {0x339b,0x28},
    {0x339c,0x78},
    {0x33a2,0x04},
    {0x33ad,0x0c},
    {0x33b1,0x80},
    {0x33b3,0x38},
    {0x33f9,0x58},
    {0x33fb,0x80},
    {0x33fc,0x48},
    {0x33fd,0x4f},
    {0x349f,0x03},
    {0x34a6,0x48},
    {0x34a7,0x4f},
    {0x34a8,0x38},
    {0x34a9,0x28},
    {0x34aa,0x00},
    {0x34ab,0xe0},
    {0x34ac,0x01},
    {0x34ad,0x08},
    {0x34f8,0x5f},
    {0x34f9,0x18},
    {0x3630,0xf0},
    {0x3631,0x85},
    {0x3632,0x74},
    {0x3633,0x22},
    {0x3637,0x4d},
    {0x3638,0xcb},
    {0x363a,0x8b},
    {0x3641,0x38},
    {0x3670,0x4e},
    {0x3674,0xf0},
    {0x3675,0xc0},
    {0x3676,0xc0},
    {0x3677,0x85},
    {0x3678,0x88},
    {0x3679,0x8a},
    {0x367c,0x48},
    {0x367d,0x49},
    {0x367e,0x49},
    {0x367f,0x5f},
    {0x3690,0x22},
    {0x3691,0x33},
    {0x3692,0x55},
    {0x3699,0x88},
    {0x369a,0x98},
    {0x369b,0xc4},
    {0x369c,0x48},
    {0x369d,0x4f},
    {0x36a2,0x49},
    {0x36a3,0x4f},
    {0x36ea,0x0f},
    {0x36eb,0x0c},
    {0x36ec,0x1c},
    {0x36ed,0x15},
    {0x370f,0x01},
    {0x3714,0x80},
    {0x3722,0x01},
    {0x3724,0x41},
    {0x3725,0xc1},
    {0x3728,0x00},
    {0x3771,0x09},
    {0x3772,0x09},
    {0x3773,0x05},
    {0x377a,0x48},
    {0x377b,0x4f},
    {0x37fa,0x09},
    {0x37fb,0x31},
    {0x37fc,0x10},
    {0x37fd,0x18},
    {0x3905,0x8d},
    {0x391d,0x08},
    {0x3922,0x1a},
    {0x3926,0x21},
    {0x3933,0x80},
    {0x3934,0x02},
    {0x3937,0x72},
    {0x3939,0x00},
    {0x393a,0x03},
    {0x39dc,0x02},
    {0x3e00,0x00},
    {0x3e01,0x31},
    {0x3e02,0xc0},
    {0x3e03,0x0b},
    {0x3e1b,0x2a},
    {0x4407,0x34},
    {0x440e,0x02},
    {0x4509,0x10},
    {0x4819,0x07},
    {0x481b,0x04},
    {0x481d,0x0f},
    {0x481f,0x03},
    {0x4821,0x09},
    {0x4823,0x04},
    {0x4825,0x03},
    {0x4827,0x04},
    {0x4829,0x06},
    {0x5000,0x46},
    {0x5001,0x40},
    {0x5007,0x80},
    {0x5900,0x01},
    {0x5901,0x04},
	{SC301IOT_FAST_AE, 0x00},	/* END MARKER */
    {0x36e9,0x53},
    {0x37f9,0x24},
    {0x0100,0x01},
#endif
    {SC301IOT_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc301iot_init_regs_2000_1504_15fps_mipi[] = {
    {0x0103,0x01},
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x37f9,0x80},
    {0x301c,0x78},
    {0x301f,0x43},
    {0x30b8,0x44},
    {0x3200,0x00},
    {0x3201,0x00},
    {0x3202,0x00},
    {0x3203,0x10},
    {0x3204,0x08},
    {0x3205,0x07},
    {0x3206,0x05},
    {0x3207,0xf7},
    {0x3208,0x07},
    {0x3209,0xd0},
    {0x320a,0x05},
    {0x320b,0xe0},
    {0x320c,0x04},
    {0x320d,0x65},
    {0x320e,0x0c},
    {0x320f,0x80},
    {0x3210,0x00},
    {0x3211,0x1c},
    {0x3212,0x00},
    {0x3213,0x04},
    {0x3214,0x11},
    {0x3215,0x11},
    {0x3223,0xc0},
    {0x3253,0x0c},
    {0x3274,0x09},
    {0x3301,0x08},
    {0x3306,0x58},
    {0x3308,0x08},
    {0x330a,0x00},
    {0x330b,0xe0},
    {0x330e,0x10},
    {0x3314,0x14},
    {0x331e,0x55},
    {0x331f,0x7d},
    {0x3333,0x10},
    {0x3334,0x40},
    {0x335e,0x06},
    {0x335f,0x08},
    {0x3364,0x5e},
    {0x337c,0x02},
    {0x337d,0x0a},
    {0x3390,0x01},
    {0x3391,0x03},
    {0x3392,0x07},
    {0x3393,0x08},
    {0x3394,0x08},
    {0x3395,0x08},
    {0x3396,0x08},
    {0x3397,0x09},
    {0x3398,0x1f},
    {0x3399,0x08},
    {0x339a,0x0a},
    {0x339b,0x40},
    {0x339c,0x88},
    {0x33a2,0x04},
    {0x33ad,0x0c},
    {0x33b1,0x80},
    {0x33b3,0x30},
    {0x33f9,0x68},
    {0x33fb,0x80},
    {0x33fc,0x48},
    {0x33fd,0x5f},
    {0x349f,0x03},
    {0x34a6,0x48},
    {0x34a7,0x5f},
    {0x34a8,0x30},
    {0x34a9,0x30},
    {0x34aa,0x00},
    {0x34ab,0xf0},
    {0x34ac,0x01},
    {0x34ad,0x08},
    {0x34f8,0x5f},
    {0x34f9,0x10},
    {0x3630,0xf0},
    {0x3631,0x85},
    {0x3632,0x74},
    {0x3633,0x22},
    {0x3637,0x4d},
    {0x3638,0xcb},
    {0x363a,0x8b},
    {0x363c,0x08},
    {0x3640,0x00},
    {0x3641,0x00},
    {0x3670,0x4e},
    {0x3674,0xc0},
    {0x3675,0xb0},
    {0x3676,0xa0},
    {0x3677,0x83},
    {0x3678,0x87},
    {0x3679,0x8a},
    {0x367c,0x49},
    {0x367d,0x4f},
    {0x367e,0x48},
    {0x367f,0x4b},
    {0x3690,0x33},
    {0x3691,0x33},
    {0x3692,0x44},
    {0x3699,0x8a},
    {0x369a,0xa1},
    {0x369b,0xc2},
    {0x369c,0x48},
    {0x369d,0x4f},
    {0x36a2,0x4b},
    {0x36a3,0x4f},
    {0x36ea,0x09},
    {0x36eb,0x0d},
    {0x36ec,0x1c},
    {0x36ed,0x25},
    {0x370f,0x01},
    {0x3714,0x80},
    {0x3722,0x09},
    {0x3724,0x41},
    {0x3725,0xc1},
    {0x3728,0x00},
    {0x3771,0x09},
    {0x3772,0x05},
    {0x3773,0x05},
    {0x377a,0x48},
    {0x377b,0x49},
    {0x37fa,0x06},
    {0x37fb,0x33},
    {0x37fc,0x11},
    {0x37fd,0x38},
    {0x3905,0x8d},
    {0x391d,0x08},
    {0x3922,0x1a},
    {0x3926,0x21},
    {0x3933,0x80},
    {0x3934,0x0d},
    {0x3937,0x6a},
    {0x3939,0x00},
    {0x393a,0x0e},
    {0x39dc,0x02},
    {0x3e00,0x00},
    {0x3e01,0xc7},
    {0x3e02,0x80},
    {0x3e03,0x0b},
    {0x3e1b,0x2a},
    {0x4407,0x34},
    {0x440e,0x02},
    {0x5001,0x40},
    {0x5007,0x80},
    {0x36e9,0x24},
    {0x37f9,0x24},
    {0x0100,0x01},
#if 0
    {0x0103,0x01},
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x37f9,0x80},
    {0x301c,0x78},
    {0x301f,0x09},
    {0x30b8,0x44},
    {0x3200,0x00},
    {0x3201,0x00},
    {0x3202,0x00},
    {0x3203,0x10},
    {0x3204,0x08},
    {0x3205,0x07},
    {0x3206,0x05},
    {0x3207,0xf7},
    {0x3208,0x07},
    {0x3209,0xd0},
    {0x320a,0x05},
    {0x320b,0xe0},
    {0x320c,0x04},
    {0x320d,0x65},
    {0x320e,0x0c},
    {0x320f,0x80},
    {0x3210,0x00},
    {0x3211,0x1c},
    {0x3212,0x00},
    {0x3213,0x04},
    {0x3214,0x11},
    {0x3215,0x11},
    {0x3223,0xc0},
    {0x3253,0x0c},
    {0x3274,0x09},
    {0x3301,0x08},
    {0x3306,0x58},
    {0x3308,0x08},
    {0x330a,0x00},
    {0x330b,0xe0},
    {0x330e,0x10},
    {0x331e,0x55},
    {0x331f,0x7d},
    {0x3333,0x10},
    {0x3334,0x40},
    {0x335e,0x06},
    {0x335f,0x08},
    {0x3364,0x5e},
    {0x337c,0x02},
    {0x337d,0x0a},
    {0x3390,0x01},
    {0x3391,0x03},
    {0x3392,0x07},
    {0x3393,0x08},
    {0x3394,0x08},
    {0x3395,0x08},
    {0x3396,0x08},
    {0x3397,0x09},
    {0x3398,0x1f},
    {0x3399,0x08},
    {0x339a,0x20},
    {0x339b,0x40},
    {0x339c,0x78},
    {0x33a2,0x04},
    {0x33ad,0x0c},
    {0x33b1,0x80},
    {0x33b3,0x30},
    {0x33f9,0x68},
    {0x33fb,0x88},
    {0x33fc,0x48},
    {0x33fd,0x5f},
    {0x349f,0x03},
    {0x34a6,0x48},
    {0x34a7,0x5f},
    {0x34a8,0x30},
    {0x34a9,0x30},
    {0x34aa,0x00},
    {0x34ab,0xf0},
    {0x34ac,0x01},
    {0x34ad,0x12},
    {0x34f8,0x5f},
    {0x34f9,0x10},
    {0x3630,0xf0},
    {0x3631,0x85},
    {0x3632,0x74},
    {0x3633,0x22},
    {0x3637,0x4d},
    {0x3638,0xcb},
    {0x363a,0x8b},
    {0x3641,0x00},
    {0x3670,0x4e},
    {0x3674,0xf0},
    {0x3675,0xc0},
    {0x3676,0xc0},
    {0x3677,0x85},
    {0x3678,0x8a},
    {0x3679,0x8d},
    {0x367c,0x48},
    {0x367d,0x49},
    {0x367e,0x49},
    {0x367f,0x5f},
    {0x3690,0x22},
    {0x3691,0x33},
    {0x3692,0x44},
    {0x3699,0x88},
    {0x369a,0x98},
    {0x369b,0xc4},
    {0x369c,0x48},
    {0x369d,0x5f},
    {0x36a2,0x49},
    {0x36a3,0x4f},
    {0x36ea,0x09},
    {0x36eb,0x0d},
    {0x36ec,0x1c},
    {0x36ed,0x25},
    {0x370f,0x01},
    {0x3714,0x80},
    {0x3722,0x09},
    {0x3724,0x41},
    {0x3725,0xc1},
    {0x3728,0x00},
    {0x3771,0x09},
    {0x3772,0x05},
    {0x3773,0x05},
    {0x377a,0x48},
    {0x377b,0x49},
    {0x37fa,0x06},
    {0x37fb,0x33},
    {0x37fc,0x11},
    {0x37fd,0x38},
    {0x3905,0x8d},
    {0x391d,0x08},
    {0x3922,0x1a},
    {0x3926,0x21},
    {0x3933,0x80},
    {0x3934,0x02},
    {0x3937,0x72},
    {0x3939,0x00},
    {0x393a,0x03},
    {0x39dc,0x02},
    {0x3e00,0x00},
    {0x3e01,0xc7},
    {0x3e02,0xc0},
    {0x3e03,0x0b},
    {0x3e1b,0x2a},
    {0x4407,0x34},
    {0x440e,0x02},
    {0x5001,0x40},
    {0x5007,0x80},
	{SC301IOT_FAST_AE, 0x00},	/* END MARKER */
    {0x36e9,0x24},
    {0x37f9,0x24},
    {0x0100,0x01},
#endif
	{SC301IOT_REG_END, 0x00},	/* END MARKER */
};
/*
 * the order of the sc301iot_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc301iot_win_sizes[] = {
	/* resolution 2000*1504 @30fps max*/
	{
		.width		= 2000,
		.height		= 1504,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc301iot_init_regs_2000_1504_30fps_mipi,
	},
	/* resolution 2000*1504 @15fps max*/
	{
		.width		= 2000,
		.height		= 1504,
		.fps		= 15 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc301iot_init_regs_2000_1504_15fps_mipi,
	},
	/* resolution 1024*768 @100fps max*/
	{
		.width		= 1024,
		.height		= 768,
		.fps		= 100 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc301iot_init_regs_1024_768_100fps_mipi,
	},
    /* resolution 248*184 @100fps max*/
	{
		.width		= 248,
		.height		= 184,
		.fps		= 100 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc301iot_init_regs_248_184_100fps_mipi,
	}
};
struct tx_isp_sensor_win_setting *wsize = &sc301iot_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sc301iot_stream_on_mipi[] = {
	{SC301IOT_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc301iot_stream_off_mipi[] = {
	{SC301IOT_REG_END, 0x00},	/* END MARKER */
};

int sc301iot_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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

int sc301iot_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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

static int sc301iot_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC301IOT_REG_END) {
		if (vals->reg_num == SC301IOT_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc301iot_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

#ifdef FAST_AE
static int fast_ae_set_reg(struct tx_isp_subdev *sd, unsigned int *ident);
#endif
static int sc301iot_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC301IOT_REG_END) {
		if (vals->reg_num == SC301IOT_REG_DELAY) {
			private_msleep(vals->value);
        }
        else {
            if(vals->reg_num == SC301IOT_FAST_AE){
#ifdef FAST_AE
                ret = fast_ae_set_reg(sd,NULL);
                if(ret < 0){
                    pr_debug("Fast ae set reg failed !!!\n");
                }
#endif
			} else {
				ret = sc301iot_write(sd, vals->reg_num, vals->value);
				if (ret < 0)
					return ret;
			}
		}
		vals++;
	}

	return 0;
}

static int sc301iot_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

#ifdef FAST_AE
static int fast_ae_smd_set(struct tx_isp_subdev *sd, unsigned int *ident)
{
    int ret;
    int i = 0;
    int num = 0;
	unsigned char flag;
    unsigned char again = 0;
    unsigned char expt = 0;
    unsigned long long bRowTime = 1000000;
    unsigned long long sGain,sExp;
	
    while(!i)
    {
	    ret = sc301iot_read(sd, 0x5e32, &flag);
        i = (flag >> 0) & 0x01;
        
		private_msleep(2);
        if(num > 20){
            pr_debug("fast ae wait time out ...\n");
            break;
        }
        num++;

    }
    
    //sGain
    ret = sc301iot_read(sd, 0x3e08, &again);
    sGain = again;
    ret += sc301iot_read(sd, 0x3e09, &again);
    sGain = (sGain << 8) | again;
    if(ret)
        pr_debug("read again failed !!!\n");
    //sExp
    ret = sc301iot_read(sd, 0x3e00, &expt);
    sExp = expt;
	ret += sc301iot_read(sd, 0x3e01, &expt);
    sExp = (sExp << 8 ) | expt;
	ret += sc301iot_read(sd, 0x3e02, &expt);
    sExp = (sExp << 4 ) | (expt >> 4);
    if(ret)
        pr_debug("read exp failed !!!\n");
   
    do_div(bRowTime , (sensor_max_fps * sc301iot_attr.total_height));
    Big_Exp = L_Target * sExp * bRowTime * sGain * 13 ;
    printk("fast ae num=%d again = %llu,exp = %llu Big_Exp=%llu\n", num, sGain, sExp, Big_Exp);
    do_div(Big_Exp , (0x2a * 0x10 * bRowTime * 9));
    printk("fast ae div Big_Exp=%llu\n", Big_Exp);
    return ret;
}
#endif


static int sc301iot_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sc301iot_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC301IOT_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc301iot_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC301IOT_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc301iot_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret = sc301iot_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc301iot_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc301iot_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));

	ret += sc301iot_write(sd, 0x3e07, (unsigned char)(again & 0xff));
	ret += sc301iot_write(sd, 0x3e09, (unsigned char)(((again >> 8) & 0xff)));
	if (ret < 0)
		return ret;


	return 0;
}

#ifdef FAST_AE
static int sc301iot_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sc301iot_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc301iot_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc301iot_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));
	if (ret < 0)
		return ret;

	return 0;
}

static int sc301iot_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sc301iot_write(sd, 0x3e07, (unsigned char)(value & 0xff));
	ret += sc301iot_write(sd, 0x3e09, (unsigned char)(((value >> 8) & 0xff)));
	if (ret < 0)
		return ret;

	return 0;
}

uint32_t sc301iot_log2_fixed_to_fixed(const uint32_t val, const int in_fix_point, const uint8_t out_fix_point)
{
	return sc301iot_log2_int_to_fixed(val, out_fix_point, 0) - (in_fix_point << out_fix_point);
}

static int fast_ae_set_reg(struct tx_isp_subdev *sd, unsigned int *ident)
{
    int ret;
    unsigned int bGain = 0;
    unsigned int sensor_gain = 0;
    unsigned long long Max_itime = sc301iot_attr.max_integration_time;

    if(Big_Exp == 0){
        pr_debug("Big_Exp = 0\n");
        return -1;
    }
    if(Max_itime > Big_Exp){
        ret = sc301iot_set_integration_time(sd, Big_Exp);
        sc301iot_set_analog_gain(sd, 0x80);

    } else {
        ret = sc301iot_set_integration_time(sd, Max_itime);

        do_div(/*(unsigned long long)*/Big_Exp, Max_itime);
        bGain = sc301iot_log2_fixed_to_fixed(Big_Exp,0,0) * 65535;
        
        sc301iot_alloc_again(bGain , 0 , &sensor_gain);
        ret += sc301iot_set_analog_gain(sd, sensor_gain);
    }
	printk("fast_ae_set_reg\n");

    return ret;
}
#endif

static int sc301iot_set_logic(struct tx_isp_subdev *sd, int value)
{

	return 0;
}

static int sc301iot_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc301iot_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc301iot_init(struct tx_isp_subdev *sd, int enable)
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

#ifndef SENSOR_WITHOUT_INIT
	ret = sc301iot_write_array(sd, wsize->regs);
	if (ret)
		return ret;
#endif
	if(!riscv_is_pass){
		ret = sc301iot_write_array(sd, wsize->regs);
		if (ret)
			return ret;
	}

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc301iot_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = sc301iot_write_array(sd, sc301iot_stream_on_mipi);
		ISP_WARNING("sc301iot stream on\n");

	} else {
		ret = sc301iot_write_array(sd, sc301iot_stream_off_mipi);
		ISP_WARNING("sc301iot stream off\n");
	}

	return ret;
}

static int sc301iot_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;
	unsigned int max_fps = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch(sensor_max_fps){
	case TX_SENSOR_MAX_FPS_15:
		sclk = SC301IOT_SUPPORT_30FPS_SCLK;
		max_fps = TX_SENSOR_MAX_FPS_15;
		break;
	case TX_SENSOR_MAX_FPS_30:
		sclk = SC301IOT_SUPPORT_30FPS_SCLK;
		max_fps = TX_SENSOR_MAX_FPS_30;
		break;
	case TX_SENSOR_MAX_FPS_100:
		sclk = SC301IOT_SUPPORT_100FPS_SCLK;
		max_fps = TX_SENSOR_MAX_FPS_100;
		break;
	default:
		ISP_ERROR("do not support max framerate %d in mipi mode\n",sensor_max_fps);
	}
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(0x%x) no in range\n", fps);
		return -1;
	}

	ret = sc301iot_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sc301iot_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("err: sc301iot read err\n");
		return ret;
	}
	hts = (hts << 8) + tmp;
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += sc301iot_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc301iot_write(sd, 0x320e, (unsigned char)(vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: sc301iot_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	sensor->video.attr->again = -1;
	sensor->video.attr->again_short = -1;
	sensor->video.attr->integration_time = -1;
	sensor->video.attr->integration_time_short = -1;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sc301iot_set_mode(struct tx_isp_subdev *sd, int value)
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
static int sc301iot_g_chip_ident(struct tx_isp_subdev *sd,
				  struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

#ifndef SENSOR_WITHOUT_INIT
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc301iot_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(50);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(35);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(35);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"sc301iot_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(150);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
#ifdef FAST_AE	
    ret = fast_ae_smd_set(sd, &ident);
    if(ret < 0)
        ISP_WARNING("FAST AE  set fail!\n");
#endif	
    ret = sc301iot_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc301iot chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}

#else
	ident = 0xcc40;
#endif

	ISP_WARNING("sc301iot chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc301iot", sizeof("sc301iot"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int sc301iot_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = -1;
	unsigned char val = 0x0;

	ret = sc301iot_read(sd, 0x3221, &val);
	if(enable & 0x2)
		val |= 0x60;
	else
		val &= 0x9f;
	ret += sc301iot_write(sd, 0x3221, val);
	if(0 != ret)
		return ret;

	return 0;
}

static int sc301iot_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = sc301iot_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
//		if(arg)
//			ret = sc301iot_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
//		if(arg)
//			ret = sc301iot_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc301iot_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc301iot_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc301iot_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = sc301iot_write_array(sd, sc301iot_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sc301iot_write_array(sd, sc301iot_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc301iot_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = sc301iot_set_vflip(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if(arg)
			ret = sc301iot_set_logic(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int sc301iot_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc301iot_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc301iot_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc301iot_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc301iot_core_ops = {
	.g_chip_ident = sc301iot_g_chip_ident,
	.reset = sc301iot_reset,
	.init = sc301iot_init,
	/*.ioctl = sc301iot_ops_ioctl,*/
	.g_register = sc301iot_g_register,
	.s_register = sc301iot_s_register,
};

static struct tx_isp_subdev_video_ops sc301iot_video_ops = {
	.s_stream = sc301iot_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc301iot_sensor_ops = {
	.ioctl	= sc301iot_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc301iot_ops = {
	.core = &sc301iot_core_ops,
	.video = &sc301iot_video_ops,
	.sensor = &sc301iot_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc301iot",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc301iot_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

#ifndef SENSOR_WITHOUT_INIT
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);
#endif
	switch(sensor_max_fps){
	case TX_SENSOR_MAX_FPS_30:
		wsize = &sc301iot_win_sizes[0];
		sc301iot_attr.mipi.clk = 540;
		break;
	case TX_SENSOR_MAX_FPS_15:
		wsize = &sc301iot_win_sizes[1];
		sc301iot_attr.max_integration_time_native = 0xc80 - 4;
		sc301iot_attr.integration_time_limit = 0xc80 - 4;
		sc301iot_attr.total_width = 0x465;
		sc301iot_attr.total_height = 0xc80;
		sc301iot_attr.max_integration_time = 0xc80 - 4;
		break;
	case TX_SENSOR_MAX_FPS_100:
        if(sensor_resolution == TX_SENSOR_RES_100){
			wsize = &sc301iot_win_sizes[2];
			sc301iot_attr.mipi.image_twidth = 1024;
			sc301iot_attr.mipi.image_theight = 768;
			sc301iot_attr.mipi.clk = 540;
			sc301iot_attr.mipi.settle_time_apative_en = 0;
			sc301iot_attr.max_integration_time_native = 0x320 - 4;
			sc301iot_attr.integration_time_limit = 0x320 - 4;
			sc301iot_attr.total_width = 0x546;
			sc301iot_attr.total_height = 0x320;
			sc301iot_attr.max_integration_time = 0x320 - 4;
		}else if(sensor_resolution == TX_SENSOR_RES_30){
					wsize = &sc301iot_win_sizes[3];
			sc301iot_attr.mipi.image_twidth = 248;
			sc301iot_attr.mipi.image_theight = 184;
			sc301iot_attr.mipi.clk = 540;
			sc301iot_attr.mipi.settle_time_apative_en = 0;
			sc301iot_attr.max_integration_time_native = 0x1e0 - 4;
			sc301iot_attr.integration_time_limit = 0x1e0 - 4;
			sc301iot_attr.total_width = 0x465;
			sc301iot_attr.total_height = 0x1e0;
			sc301iot_attr.max_integration_time = 0x1e0 - 4;
		}
		break;
	default:
		ISP_ERROR("do not support max framerate %d in mipi mode\n",sensor_max_fps);
	}

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sc301iot_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc301iot_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	printk("probe ok ------->sc301iot fps=%d\n", sensor_max_fps);

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int sc301iot_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc301iot_id[] = {
	{ "sc301iot", 0 },
	{ }
};
//MODULE_DEVICE_TABLE(i2c, sc301iot_id);

static struct i2c_driver sc301iot_driver = {
	.driver = {
		.owner	= NULL,
		.name	= "sc301iot",
	},
	.probe		= sc301iot_probe,
	.remove		= sc301iot_remove,
	.id_table	= sc301iot_id,
};

char * get_sensor_name(void)
{
	return "sc301iot";
}

int get_sensor_i2c_addr(void)
{
	return 0x30;
}

int get_sensor_width()
{
	return wsize->width;
}

int get_sensor_height()
{
	return wsize->height;
}

int init_sensor(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		printk("Failed to init sc301iot dirver.\n");
		return -1;
	}
    
	if(data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) {
		fast_wdr_mode = 1;
	} else {
		fast_wdr_mode = 0;
	}

	return private_i2c_add_driver(&sc301iot_driver);
}

void exit_sensor(void)
{
	private_i2c_del_driver(&sc301iot_driver);
}
