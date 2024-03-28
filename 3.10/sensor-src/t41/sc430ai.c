// SPDX-License-Identifier: GPL-2.0+
/*
 * sc430ai.c
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps     interface              mode
 *   0          2688*1520       20        mipi_2lane           hdr
 *   1          2688*1520       30        mipi_2lane           linear
 *   2          2688*1520       25        mipi_2lane           hdr
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
#include <sensor-info.h>

#define SENSOR_NAME "sc430ai"
#define SENSOR_CHIP_ID_H (0xce)
#define SENSOR_CHIP_ID_L (0x39)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_30FPS_SCLK_HDR (3200 * 3300  * 20)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MAX_FPS_DOL 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20220105a"
#define MCLK 24000000

static int reset_gpio = GPIO_PC(27);
static int pwdn_gpio = -1;
static int wdr_bufsize = 1400 * 1000;
static int shvflip = 0;
static unsigned char switch_wdr = 0;

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x0080, 0},
	{0x0084, 2886},
	{0x0088, 5776},
	{0x008c, 8494},
	{0x0090, 11136},
	{0x0094, 13706},
	{0x0098, 16287},
	{0x009c, 18723},
	{0x00a0, 21097},
	{0x00a4, 23414},
	{0x00a8, 25746},
	{0x00ac, 27953},
	{0x00b0, 30109},
	{0x00b4, 32217},
	{0x00b8, 34345},
	{0x00bc, 36361},
	{0x00c0, 38336},
	{0x00c4, 40270},
	{0x00c8, 42226},
	{0x00cc, 44082},
	{0x00d0, 45904},
	{0x00d4, 47690},
	{0x00d8, 49500},
	{0x00dc, 51220},
	{0x00e0, 52910},
	{0x00e4, 54571},
	{0x00e8, 56254},
	{0x00ec, 57857},
	{0x00f0, 59433},
	{0x00f4, 60984},
	{0x00f8, 62558},
	{0x00fc, 64059},
	{0x0180, 65536},
	{0x0184, 68422},
	{0x0188, 71312},
	{0x018c, 74030},
	{0x0190, 76672},
	{0x0194, 79242},
	{0x0198, 81823},
	{0x4080, 82379},
	{0x4084, 85262},
	{0x4088, 88171},
	{0x408c, 90886},
	{0x4090, 93524},
	{0x4094, 96091},
	{0x4098, 98656},
	{0x409c, 101089},
	{0x40a0, 103493},
	{0x40a4, 105806},
	{0x40a8, 108124},
	{0x40ac, 110328},
	{0x40b0, 112481},
	{0x40b4, 114587},
	{0x40b8, 116729},
	{0x40bc, 118743},
	{0x40c0, 120715},
	{0x40c4, 122647},
	{0x40c8, 124616},
	{0x40cc, 126470},
	{0x40d0, 128289},
	{0x40d4, 130073},
	{0x40d8, 131872},
	{0x40dc, 133590},
	{0x40e0, 135301},
	{0x40e4, 136959},
	{0x40e8, 138632},
	{0x40ec, 140233},
	{0x40f0, 141808},
	{0x40f4, 143356},
	{0x40f8, 144941},
	{0x40fc, 146440},
	{0x4880, 147915},
	{0x4884, 150798},
	{0x4888, 153689},
	{0x488c, 156403},
	{0x4890, 159060},
	{0x4894, 161627},
	{0x4898, 164209},
	{0x489c, 166641},
	{0x48a0, 169013},
	{0x48a4, 171326},
	{0x48a8, 173660},
	{0x48ac, 175864},
	{0x48b0, 178031},
	{0x48b4, 180137},
	{0x48b8, 182265},
	{0x48bc, 184279},
	{0x48c0, 186251},
	{0x48c4, 188183},
	{0x48c8, 190139},
	{0x48cc, 191994},
	{0x48d0, 193825},
	{0x48d4, 195609},
	{0x48d8, 197419},
	{0x48dc, 199138},
	{0x48e0, 200826},
	{0x48e4, 202484},
	{0x48e8, 204168},
	{0x48ec, 205769},
	{0x48f0, 207354},
	{0x48f4, 208903},
	{0x48f8, 210477},
	{0x48fc, 211976},
	{0x4980, 213451},
	{0x4984, 216334},
	{0x4988, 219225},
	{0x498c, 221949},
	{0x4990, 224587},
	{0x4994, 227154},
	{0x4998, 229737},
	{0x499c, 232177},
	{0x49a0, 234549},
	{0x49a4, 236862},
	{0x49a8, 239196},
	{0x49ac, 241407},
	{0x49b0, 243560},
	{0x49b4, 245666},
	{0x49b8, 247794},
	{0x49bc, 249815},
	{0x49c0, 251787},
	{0x49c4, 253719},
	{0x49c8, 255675},
	{0x49cc, 257536},
	{0x49d0, 259355},
	{0x49d4, 261140},
	{0x49d8, 262950},
	{0x49dc, 264674},
	{0x49e0, 266362},
	{0x49e4, 268020},
	{0x49e8, 269704},
	{0x49ec, 271311},
	{0x49f0, 272885},
	{0x49f4, 274434},
	{0x49f8, 276008},
	{0x49fc, 277512},
	{0x4b80, 278987},
	{0x4b84, 281875},
	{0x4b88, 284765},
	{0x4b8c, 287480},
	{0x4b90, 290123},
	{0x4b94, 292694},
	{0x4b98, 295277},
	{0x4b9c, 297709},
	{0x4ba0, 300085},
	{0x4ba4, 302402},
	{0x4ba8, 304736},
	{0x4bac, 306939},
	{0x4bb0, 309096},
	{0x4bb4, 311205},
	{0x4bb8, 313334},
	{0x4bbc, 315348},
	{0x4bc0, 317323},
	{0x4bc4, 319258},
	{0x4bc8, 321214},
	{0x4bcc, 323069},
	{0x4bd0, 324891},
	{0x4bd4, 326679},
	{0x4bd8, 328489},
	{0x4bdc, 330207},
	{0x4be0, 331898},
	{0x4be4, 333559},
	{0x4be8, 335243},
	{0x4bec, 336844},
	{0x4bf0, 338421},
	{0x4bf4, 339972},
	{0x4bf8, 341547},
	{0x4bfc, 343045},
	{0x4f80, 344523},
	{0x4f84, 347408},
	{0x4f88, 350299},
	{0x4f8c, 353018},
	{0x4f90, 355659},
	{0x4f94, 358228},
	{0x4f98, 360811},
	{0x4f9c, 363247},
	{0x4fa0, 365621},
	{0x4fa4, 367936},
	{0x4fa8, 370270},
	{0x4fac, 372477},
	{0x4fb0, 374632},
	{0x4fb4, 376739},
	{0x4fb8, 378868},
	{0x4fbc, 380885},
	{0x4fc0, 382859},
	{0x4fc4, 384792},
	{0x4fc8, 386749},
	{0x4fcc, 388607},
	{0x4fd0, 390427},
	{0x4fd4, 392213},
	{0x4fd8, 394023},
	{0x4fdc, 395745},
	{0x4fe0, 397434},
	{0x4fe4, 399093},
	{0x4fe8, 400778},
	{0x4fec, 402381},
	{0x4ff0, 403957},
	{0x4ff4, 405507},
	{0x4ff8, 407081},
	{0x4ffc, 408583},
	{0x5f80, 410059},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it) {
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;
	*sensor_it = expo;

	return isp_it;
}

unsigned int sensor_alloc_integration_time_short(unsigned int it, unsigned char shift, unsigned int *sensor_it) {
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;
	*sensor_it = expo;

	return isp_it;
}

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	struct again_lut *lut = sensor_again_lut;

	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sensor_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	struct again_lut *lut = sensor_again_lut;

	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
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

struct tx_isp_mipi_bus sensor_mipi_dol = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 720,
	.lans = 2,
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
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_mipi_bus sensor_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 792,
	.lans = 2,
	.index = 0,
	.settle_time_apative_en = 0,
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
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = 0xce39,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL,
	.cbus_device = 0x30,
	.max_again = 410059,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_short = 4,
	.min_integration_time_native = 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_again_short = sensor_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	.sensor_ctrl.alloc_integration_time_short = sensor_alloc_integration_time_short,
};

static struct regval_list sensor_init_regs_2688_1520_20fps_mipi_dol[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x3018, 0x32},
	{0x3019, 0x0c},
	{0x301f, 0x92},
	{0x3203, 0x32},
	{0x3204, 0x0a},
	{0x3205, 0xff},
	{0x3206, 0x06},
	{0x3207, 0x29},
	{0x3208, 0x0a},
	{0x3209, 0x80},
	{0x320a, 0x05},
	{0x320b, 0xf0},
	{0x320e, 0x0c},/*vts = 0xce4 = 3300*/
	{0x320f, 0xe4},//
	{0x3211, 0x1c},
	{0x3213, 0x04},
	{0x3250, 0xff},
	{0x3251, 0x98},
	{0x3253, 0x0c},
	{0x325f, 0x20},
	{0x3281, 0x81},
	{0x3301, 0x08},
	{0x3304, 0x58},
	{0x3306, 0xa0},
	{0x3308, 0x14},
	{0x3309, 0x50},
	{0x330a, 0x01},
	{0x330b, 0x10},
	{0x330d, 0x10},
	{0x331e, 0x49},
	{0x331f, 0x41},
	{0x3333, 0x10},
	{0x335d, 0x60},
	{0x335e, 0x06},
	{0x335f, 0x08},
	{0x3364, 0x56},
	{0x3366, 0x01},
	{0x337c, 0x02},
	{0x337d, 0x0a},
	{0x3390, 0x01},
	{0x3391, 0x03},
	{0x3392, 0x07},
	{0x3393, 0x08},
	{0x3394, 0x08},
	{0x3395, 0x08},
	{0x3396, 0x48},
	{0x3397, 0x4b},
	{0x3398, 0x4f},
	{0x3399, 0x0a},
	{0x339a, 0x0a},
	{0x339b, 0x10},
	{0x339c, 0x22},
	{0x33a2, 0x04},
	{0x33ad, 0x24},
	{0x33ae, 0x38},
	{0x33af, 0x38},
	{0x33b1, 0x80},
	{0x33b2, 0x48},
	{0x33b3, 0x20},
	{0x349f, 0x02},
	{0x34a6, 0x48},
	{0x34a7, 0x4b},
	{0x34a8, 0x20},
	{0x34a9, 0x18},
	{0x34f8, 0x5f},
	{0x34f9, 0x04},
	{0x3632, 0x48},
	{0x3633, 0x32},
	{0x3637, 0x29},
	{0x3638, 0xc1},
	{0x363b, 0x20},
	{0x363d, 0x02},
	{0x3670, 0x09},
	{0x3674, 0x88},
	{0x3675, 0x88},
	{0x3676, 0x88},
	{0x367c, 0x40},
	{0x367d, 0x48},
	{0x3690, 0x33},
	{0x3691, 0x34},
	{0x3692, 0x55},
	{0x3693, 0x4b},
	{0x3694, 0x4f},
	{0x3698, 0x85},
	{0x3699, 0x8f},
	{0x369a, 0xa0},
	{0x369b, 0xc3},
	{0x36a2, 0x49},
	{0x36a3, 0x4b},
	{0x36a4, 0x4f},
	{0x36d0, 0x01},
	{0x36ea, 0x0d},
	{0x36eb, 0x04},
	{0x36ec, 0x03},
	{0x36ed, 0x34},
	{0x370f, 0x01},
	{0x3722, 0x00},
	{0x3728, 0x10},
	{0x37b0, 0x03},
	{0x37b1, 0x03},
	{0x37b2, 0x83},
	{0x37b3, 0x48},
	{0x37b4, 0x4f},
	{0x37fa, 0x0b},
	{0x37fb, 0x04},
	{0x37fc, 0x00},
	{0x37fd, 0x14},
	{0x3901, 0x00},
	{0x3902, 0xc5},
	{0x3904, 0x08},
	{0x3905, 0x8d},
	{0x3909, 0x00},
	{0x391d, 0x04},
	{0x3926, 0x21},
	{0x3929, 0x18},
	{0x3933, 0x82},
	{0x3934, 0x08},
	{0x3937, 0x5b},
	{0x3939, 0x00},
	{0x393a, 0x01},
	{0x39dc, 0x02},
	{0x3e00, 0x01},/*long_expo = 384*/
	{0x3e01, 0x80},//
	{0x3e02, 0x00},//
	{0x3e04, 0x18},/*short_expo = 0x180 = 384*/
	{0x3e05, 0x00},//
	{0x3e23, 0x00},/*max_short_expo = 2 *199-14*/
	{0x3e24, 0xc7},//
	{0x440e, 0x02},
	{0x4509, 0x20},
	{0x4814, 0x2a},
	{0x4837, 0x11},
	{0x4851, 0x6b},
	{0x4853, 0xfd},
	{0x5010, 0x10},
	{0x5799, 0x06},
	{0x57ad, 0x00},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x30},
	{0x5ae3, 0x2a},
	{0x5ae4, 0x24},
	{0x5ae5, 0x30},
	{0x5ae6, 0x2a},
	{0x5ae7, 0x24},
	{0x5ae8, 0x3c},
	{0x5ae9, 0x30},
	{0x5aea, 0x28},
	{0x5aeb, 0x3c},
	{0x5aec, 0x30},
	{0x5aed, 0x28},
	{0x5aee, 0xfe},
	{0x5aef, 0x40},
	{0x5af4, 0x30},
	{0x5af5, 0x2a},
	{0x5af6, 0x24},
	{0x5af7, 0x30},
	{0x5af8, 0x2a},
	{0x5af9, 0x24},
	{0x5afa, 0x3c},
	{0x5afb, 0x30},
	{0x5afc, 0x28},
	{0x5afd, 0x3c},
	{0x5afe, 0x30},
	{0x5aff, 0x28},
	{0x36e9, 0x20},
	{0x37f9, 0x20},
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sensor_init_regs_2688_1520_20fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x3018, 0x32},
	{0x3019, 0x0c},
	{0x301f, 0x93},
	{0x3203, 0x32},
	{0x3204, 0x0a},
	{0x3205, 0xff},
	{0x3206, 0x06},
	{0x3207, 0x29},
	{0x3208, 0x0a},
	{0x3209, 0x80},
	{0x320a, 0x05},
	{0x320b, 0xf0},
	{0x3211, 0x1c},
	{0x3250, 0x40},
	{0x3251, 0x98},
	{0x3253, 0x0c},
	{0x325f, 0x20},
	{0x3301, 0x08},
	{0x3304, 0x50},
	{0x3306, 0x88},
	{0x3308, 0x14},
	{0x3309, 0x70},
	{0x330a, 0x00},
	{0x330b, 0xf8},
	{0x330d, 0x10},
	{0x331e, 0x41},
	{0x331f, 0x61},
	{0x3333, 0x10},
	{0x335d, 0x60},
	{0x335e, 0x06},
	{0x335f, 0x08},
	{0x3364, 0x56},
	{0x3366, 0x01},
	{0x337c, 0x02},
	{0x337d, 0x0a},
	{0x3390, 0x01},
	{0x3391, 0x03},
	{0x3392, 0x07},
	{0x3393, 0x08},
	{0x3394, 0x08},
	{0x3395, 0x08},
	{0x3396, 0x40},
	{0x3397, 0x48},
	{0x3398, 0x4b},
	{0x3399, 0x08},
	{0x339a, 0x08},
	{0x339b, 0x08},
	{0x339c, 0x1d},
	{0x33a2, 0x04},
	{0x33ae, 0x30},
	{0x33af, 0x50},
	{0x33b1, 0x80},
	{0x33b2, 0x48},
	{0x33b3, 0x30},
	{0x349f, 0x02},
	{0x34a6, 0x48},
	{0x34a7, 0x4b},
	{0x34a8, 0x30},
	{0x34a9, 0x18},
	{0x34f8, 0x5f},
	{0x34f9, 0x08},
	{0x3632, 0x48},
	{0x3633, 0x32},
	{0x3637, 0x29},
	{0x3638, 0xc1},
	{0x363b, 0x20},
	{0x363d, 0x02},
	{0x3670, 0x09},
	{0x3674, 0x8b},
	{0x3675, 0xc6},
	{0x3676, 0x8b},
	{0x367c, 0x40},
	{0x367d, 0x48},
	{0x3690, 0x32},
	{0x3691, 0x43},
	{0x3692, 0x33},
	{0x3693, 0x40},
	{0x3694, 0x4b},
	{0x3698, 0x85},
	{0x3699, 0x8f},
	{0x369a, 0xa0},
	{0x369b, 0xc3},
	{0x36a2, 0x49},
	{0x36a3, 0x4b},
	{0x36a4, 0x4f},
	{0x36d0, 0x01},
	{0x36ea, 0x0b},
	{0x36eb, 0x04},
	{0x36ec, 0x03},
	{0x36ed, 0x14},
	{0x370f, 0x01},
	{0x3722, 0x00},
	{0x3728, 0x10},
	{0x37b0, 0x03},
	{0x37b1, 0x03},
	{0x37b2, 0x83},
	{0x37b3, 0x48},
	{0x37b4, 0x49},
	{0x37fa, 0x0b},
	{0x37fb, 0x24},
	{0x37fc, 0x01},
	{0x37fd, 0x14},
	{0x3901, 0x00},
	{0x3902, 0xc5},
	{0x3904, 0x08},
	{0x3905, 0x8c},
	{0x3909, 0x00},
	{0x391d, 0x04},
	{0x391f, 0x44},
	{0x3926, 0x21},
	{0x3929, 0x18},
	{0x3933, 0x82},
	{0x3934, 0x0a},
	{0x3937, 0x5f},
	{0x3939, 0x00},
	{0x393a, 0x00},
	{0x39dc, 0x02},
	{0x3e01, 0xcd},
	{0x3e02, 0xa0},
	{0x440e, 0x02},
	{0x4509, 0x20},
	{0x4837, 0x14},
	{0x5010, 0x10},
	{0x5799, 0x06},
	{0x57ad, 0x00},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x30},
	{0x5ae3, 0x2a},
	{0x5ae4, 0x24},
	{0x5ae5, 0x30},
	{0x5ae6, 0x2a},
	{0x5ae7, 0x24},
	{0x5ae8, 0x3c},
	{0x5ae9, 0x30},
	{0x5aea, 0x28},
	{0x5aeb, 0x3c},
	{0x5aec, 0x30},
	{0x5aed, 0x28},
	{0x5aee, 0xfe},
	{0x5aef, 0x40},
	{0x5af4, 0x30},
	{0x5af5, 0x2a},
	{0x5af6, 0x24},
	{0x5af7, 0x30},
	{0x5af8, 0x2a},
	{0x5af9, 0x24},
	{0x5afa, 0x3c},
	{0x5afb, 0x30},
	{0x5afc, 0x28},
	{0x5afd, 0x3c},
	{0x5afe, 0x30},
	{0x5aff, 0x28},
	{0x36e9, 0x53},
	{0x37f9, 0x53},
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_2688_1520_25fps_mipi_dol[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x3018, 0x32},
	{0x3019, 0x0c},
	{0x301f, 0x94},
	{0x3203, 0x32},
	{0x3204, 0x0a},
	{0x3205, 0xff},
	{0x3206, 0x06},
	{0x3207, 0x29},
	{0x3208, 0x0a},
	{0x3209, 0x80},
	{0x320a, 0x05},
	{0x320b, 0xf0},
	{0x320c, 0x07},//
	{0x320d, 0x80},/*hts = 0x780 = 1920*/
	{0x320e, 0x0c},
	{0x320f, 0xe4},/*vts = 0xce4 = 3300*/
	{0x3211, 0x1c},
	{0x3250, 0xff},
	{0x3251, 0x98},
	{0x3253, 0x0c},
	{0x325f, 0x20},
	{0x3281, 0x81},
	{0x3301, 0x08},
	{0x3304, 0x58},
	{0x3306, 0xa0},
	{0x3308, 0x14},
	{0x3309, 0x50},
	{0x330a, 0x01},
	{0x330b, 0x10},
	{0x330d, 0x10},
	{0x331e, 0x49},
	{0x331f, 0x41},
	{0x3333, 0x10},
	{0x335d, 0x60},
	{0x335e, 0x06},
	{0x335f, 0x08},
	{0x3364, 0x56},
	{0x3366, 0x01},
	{0x337c, 0x02},
	{0x337d, 0x0a},
	{0x3390, 0x01},
	{0x3391, 0x03},
	{0x3392, 0x07},
	{0x3393, 0x08},
	{0x3394, 0x08},
	{0x3395, 0x08},
	{0x3396, 0x48},
	{0x3397, 0x4b},
	{0x3398, 0x4f},
	{0x3399, 0x0a},
	{0x339a, 0x0a},
	{0x339b, 0x10},
	{0x339c, 0x22},
	{0x33a2, 0x04},
	{0x33ad, 0x24},
	{0x33ae, 0x38},
	{0x33af, 0x38},
	{0x33b1, 0x80},
	{0x33b2, 0x48},
	{0x33b3, 0x20},
	{0x349f, 0x02},
	{0x34a6, 0x48},
	{0x34a7, 0x4b},
	{0x34a8, 0x20},
	{0x34a9, 0x18},
	{0x34f8, 0x5f},
	{0x34f9, 0x04},
	{0x3632, 0x48},
	{0x3633, 0x32},
	{0x3637, 0x29},
	{0x3638, 0xcd},
	{0x3639, 0xf8},
	{0x363b, 0x20},
	{0x363d, 0x02},
	{0x3670, 0x09},
	{0x3674, 0x88},
	{0x3675, 0x88},
	{0x3676, 0x88},
	{0x367c, 0x40},
	{0x367d, 0x48},
	{0x3690, 0x33},
	{0x3691, 0x34},
	{0x3692, 0x55},
	{0x3693, 0x4b},
	{0x3694, 0x4f},
	{0x3698, 0x85},
	{0x3699, 0x8f},
	{0x369a, 0xa0},
	{0x369b, 0xd8},
	{0x36a2, 0x49},
	{0x36a3, 0x4b},
	{0x36a4, 0x4f},
	{0x36d0, 0x01},
	{0x36ea, 0x0d},
	{0x36eb, 0x04},
	{0x36ec, 0x03},
	{0x36ed, 0x34},
	{0x370f, 0x01},
	{0x3722, 0x00},
	{0x3728, 0x10},
	{0x37b0, 0x03},
	{0x37b1, 0x03},
	{0x37b2, 0x83},
	{0x37b3, 0x48},
	{0x37b4, 0x4f},
	{0x37fa, 0x0b},
	{0x37fb, 0x04},
	{0x37fc, 0x00},
	{0x37fd, 0x34},
	{0x3901, 0x00},
	{0x3902, 0xc5},
	{0x3904, 0x08},
	{0x3905, 0x8d},
	{0x3909, 0x00},
	{0x391d, 0x04},
	{0x3926, 0x21},
	{0x3929, 0x18},
	{0x3933, 0x82},
	{0x3934, 0x08},
	{0x3937, 0x5b},
	{0x3939, 0x00},
	{0x393a, 0x01},
	{0x39dc, 0x02},
	{0x3e00, 0x01},
	{0x3e01, 0x80},
	{0x3e02, 0x00},
	{0x3e04, 0x18},//
	{0x3e05, 0x00},//
	{0x3e23, 0x00},//
	{0x3e24, 0xc7},//
	{0x440e, 0x02},
	{0x4509, 0x20},
	{0x4814, 0x2a},
	{0x4837, 0x0d},
	{0x4851, 0x6b},
	{0x4853, 0xfd},
	{0x5010, 0x10},
	{0x5799, 0x06},
	{0x57ad, 0x00},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x30},
	{0x5ae3, 0x2a},
	{0x5ae4, 0x24},
	{0x5ae5, 0x30},
	{0x5ae6, 0x2a},
	{0x5ae7, 0x24},
	{0x5ae8, 0x3c},
	{0x5ae9, 0x30},
	{0x5aea, 0x28},
	{0x5aeb, 0x3c},
	{0x5aec, 0x30},
	{0x5aed, 0x28},
	{0x5aee, 0xfe},
	{0x5aef, 0x40},
	{0x5af4, 0x30},
	{0x5af5, 0x2a},
	{0x5af6, 0x24},
	{0x5af7, 0x30},
	{0x5af8, 0x2a},
	{0x5af9, 0x24},
	{0x5afa, 0x3c},
	{0x5afb, 0x30},
	{0x5afc, 0x28},
	{0x5afd, 0x3c},
	{0x5afe, 0x30},
	{0x5aff, 0x28},
	{0x36e9, 0x53},
	{0x37f9, 0x20},
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the jxf23_win_sizes is [full_resolution, preview_resolution]. */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 2688,
		.height = 1520,
		.fps = 20 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2688_1520_20fps_mipi_dol,
	},
	{
		.width = 2688,
		.height = 1520,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2688_1520_20fps_mipi,
	},
	{
		.width = 2688,
		.height = 1520,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2688_1520_25fps_mipi_dol,
	}
};

struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

static struct regval_list sensor_stream_on[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value) {
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
		 unsigned char value) {
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
			private_msleep(vals->value);
		} else {
			ret = sensor_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}
	return 0;
}
#endif

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sensor_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	return 0;
}

#if 1

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident) {
	unsigned char v;
	int ret;

	ret = sensor_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

#endif

static int sensor_set_expo(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	ret = sensor_write(sd, 0x3e00, (unsigned char) ((it >> 12) & 0x0f));
	ret += sensor_write(sd, 0x3e01, (unsigned char) ((it >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char) ((it & 0x0f) << 4));


	ret += sensor_write(sd, 0x3e09, (unsigned char) ((again >> 8) & 0xff));
	ret += sensor_write(sd, 0x3e07, (unsigned char) (again & 0xff));

	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = 0;

	if (info->default_boot == 1) {
		if (value > 1505) value = 1504;
	}

	ret = sensor_write(sd, 0x0203, value & 0xff);
	ret += sensor_write(sd, 0x0202, value >> 8);
	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}
#endif

static int sensor_set_integration_time_short(struct tx_isp_subdev *sd, int value) {
	int ret = 0;

//	printk("\n==============> short_time = 0x%x\n", value);
	ret = sensor_write(sd, 0x3e04, (unsigned char) ((value >> 4) & 0xff));
	ret = sensor_write(sd, 0x3e05, (unsigned char) (value & 0x0f) << 4);

	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	struct again_lut *val_lut = sensor_again_lut;


	ret = sensor_write(sd, 0x02b3, val_lut[value].reg2b3);
	ret = sensor_write(sd, 0x02b4, val_lut[value].reg2b4);
	ret = sensor_write(sd, 0x02b8, val_lut[value].reg2b8);
	ret = sensor_write(sd, 0x02b9, val_lut[value].reg2b9);
	ret = sensor_write(sd, 0x0515, val_lut[value].reg515);
	ret = sensor_write(sd, 0x0519, val_lut[value].reg519);
	ret = sensor_write(sd, 0x02d9, val_lut[value].reg2d9);

	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d" ,__LINE__ );
		return ret;
	}

	return 0;
}
#endif

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	sensor->video.state = TX_ISP_MODULE_DEINIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
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
			ret = sensor_write_array(sd, sensor_stream_on);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("%s stream on\n", SENSOR_NAME);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
	} else {
		ret = sensor_write_array(sd, sensor_stream_off);
		pr_debug("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned short vts = 0;
	unsigned short hts = 0;
	unsigned int sensor_max_fps;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;
	unsigned int short_time;
	unsigned char val;

	switch (sensor->info.default_boot) {
		case 0:
			sclk = SENSOR_SUPPORT_30FPS_SCLK_HDR;
			sensor_max_fps = TX_SENSOR_MAX_FPS_20;
			break;
		case 1:
			sclk = 3200 * 1650 * 30;
			sensor_max_fps = TX_SENSOR_MAX_FPS_30;
			break;
		case 2:
			sclk = 3300 * 3840 * 25;
			sensor_max_fps = TX_SENSOR_MAX_FPS_25;
			break;
		default:
			ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	/* the format of fps is 16/16. for example 30 << 16 | 2, the value is 30/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (sensor_max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}

	val = 0;
	ret += sensor_read(sd, 0x320c, &val);
	hts = val << 8;
	val = 0;
	ret += sensor_read(sd, 0x320d, &val);
	hts |= val;
	hts *= 2;

	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	sensor_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	sensor_write(sd, 0x320e, (unsigned char) (vts >> 8));

	if (sensor->info.default_boot != 1) {
		sensor_read(sd, 0x3e23, &val);
		short_time = val << 8;
		sensor_read(sd, 0x3e24, &val);
		short_time |= val;
		short_time *= 2;
	}

	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = (sensor->info.default_boot == 1) ? (2 * vts - 10) : (2 * vts -
													       short_time -
													       18);
	sensor->video.attr->integration_time_limit = (sensor->info.default_boot == 1) ? (2 * vts - 10) : (2 * vts -
													  short_time -
													  18);
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = (sensor->info.default_boot == 1) ? (2 * vts - 10) : (2 * vts -
													short_time -
													18);

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	if (ret) {
		ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
	}

	return ret;
}


static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable) {
	int ret = 0;
	return ret;
}


static int sensor_set_mode(struct tx_isp_subdev *sd, int value) {
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

struct clk *sclka;

static int sensor_attr_check(struct tx_isp_subdev *sd) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = 0;

	switch (info->default_boot) {
		case 0:
			sensor_attr.wdr_cache = wdr_bufsize;
			wsize = &sensor_win_sizes[0];
			memcpy(&sensor_attr.mipi, &sensor_mipi_dol, sizeof(sensor_mipi_dol));
			sensor_attr.mipi.clk = 936;
			sensor_attr.min_integration_time = 5;
			sensor_attr.min_integration_time_short = 5;
			sensor_attr.total_width = 1600 * 2;
			sensor_attr.total_height = 3300;
			sensor_attr.max_integration_time_native = 6198; /* 2*3300 - 384 -18*/
			sensor_attr.integration_time_limit = 6198;
			sensor_attr.max_integration_time = 6198;
			sensor_attr.max_integration_time_short = 384; /*2*199 -14*/
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
			printk("=================> 20fps_hdr is ok");
			break;
		case 1:
			wsize = &sensor_win_sizes[1];
			memcpy(&sensor_attr.mipi, &sensor_mipi_linear, sizeof(sensor_mipi_linear));
			sensor_attr.min_integration_time = 3;
			sensor_attr.total_width = 1600 * 2;
			sensor_attr.total_height = 1650;
			sensor_attr.max_integration_time_native = 3300 - 10;
			sensor_attr.integration_time_limit = 3300 - 10;
			sensor_attr.max_integration_time = 3300 - 10;
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			printk("=================> linear is ok");
			break;
		case 2:
			sensor_attr.wdr_cache = wdr_bufsize;
			wsize = &sensor_win_sizes[2];
			memcpy(&sensor_attr.mipi, &sensor_mipi_dol, sizeof(sensor_mipi_dol));
			sensor_attr.mipi.clk = 1188,
				sensor_attr.min_integration_time = 5;
			sensor_attr.min_integration_time_short = 5;
			sensor_attr.total_width = 1920 * 2;
			sensor_attr.total_height = 3300;
			sensor_attr.max_integration_time_native = 6198; /* 2*3300 - 384 -18*/
			sensor_attr.integration_time_limit = 6198;
			sensor_attr.max_integration_time = 6198;
			sensor_attr.max_integration_time_short = 384;
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
			printk("=================> 25fps_hdr is ok");
			break;
		default:
			ISP_ERROR("Have no this setting!!!\n");
	}

	switch (info->video_interface) {
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

	switch (info->mclk) {
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

	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	rate = private_clk_get_rate(sensor->mclk);

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

	private_clk_set_rate(sensor->mclk, MCLK);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;
	return 0;

err_get_mclk:
	return -1;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio, "sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", reset_gpio);
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
			ISP_ERROR("gpio requrest fail %d\n", pwdn_gpio);
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

#if 1

static int sensor_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en) {
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = 0;

	ret = sensor_write(sd, 0x0103, 0x1);

	if (wdr_en == 1) {
		if (switch_wdr == 0) {
			info->default_boot = 0;
			memcpy(&sensor_attr.mipi, &sensor_mipi_dol, sizeof(sensor_mipi_dol));
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
			sensor_attr.wdr_cache = wdr_bufsize;
			wsize = &sensor_win_sizes[0];
			sensor_attr.min_integration_time = 5;
			sensor_attr.min_integration_time_short = 5;
			sensor_attr.total_width = 1600 * 2;
			sensor_attr.total_height = 3300;
			sensor_attr.max_integration_time_native = 6144;
			sensor_attr.integration_time_limit = 6144;
			sensor_attr.max_integration_time = 6144;
			sensor_attr.max_integration_time_short = 384;
			printk("\n-------------------------switch wdr@20fps ok ----------------------\n");
		} else {
			info->default_boot = 2;
			memcpy(&sensor_attr.mipi, &sensor_mipi_dol, sizeof(sensor_mipi_dol));
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
			sensor_attr.wdr_cache = wdr_bufsize;
			wsize = &sensor_win_sizes[2];
			sensor_attr.mipi.clk = 1188,
				sensor_attr.min_integration_time = 5;
			sensor_attr.min_integration_time_short = 5;
			sensor_attr.total_width = 1920 * 2;
			sensor_attr.total_height = 3300;
			sensor_attr.max_integration_time_native = 6144;
			sensor_attr.integration_time_limit = 6144;
			sensor_attr.max_integration_time = 6144;
			sensor_attr.max_integration_time_short = 384;
			printk("\n-------------------------switch wdr@25fps ok ----------------------\n");
		}
	} else if (wdr_en == 0) {
		switch_wdr = info->default_boot;
		info->default_boot = 1;
		memcpy(&sensor_attr.mipi, &sensor_mipi_linear, sizeof(sensor_mipi_linear));
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		wsize = &sensor_win_sizes[1];
		sensor_attr.min_integration_time = 3;
		sensor_attr.total_width = 1600 * 2;
		sensor_attr.total_height = 1650;
		sensor_attr.max_integration_time_native = 3300 - 10;
		sensor_attr.integration_time_limit = 3300 - 10;
		sensor_attr.max_integration_time = 3300 - 10;
		printk("\n-------------------------switch linear ok ----------------------\n");
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int sensor_set_wdr(struct tx_isp_subdev *sd, int wdr_en) {
	int ret = 0;
//	printk("\n==========> set_wdr\n");
	private_gpio_direction_output(reset_gpio, 1);
	private_msleep(1);
	private_gpio_direction_output(reset_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(reset_gpio, 1);
	private_msleep(1);

	ret = sensor_write_array(sd, wsize->regs);
	ret = sensor_write_array(sd, sensor_stream_on);

	return 0;
}

#endif

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg) {
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	struct tx_isp_initarg *init = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd) {
		case TX_ISP_EVENT_SENSOR_EXPO:
			if (arg)
				ret = sensor_set_expo(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			//	if (arg)
			//		ret = sensor_set_integration_time(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			//	if (arg)
			//		ret = sensor_set_analog_gain(sd, sensor_val->value);
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
			ret = sensor_write_array(sd, sensor_stream_off);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			ret = sensor_write_array(sd, sensor_stream_on);
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if (arg)
				ret = sensor_set_fps(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
			if (arg)
				ret = sensor_set_integration_time_short(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_WDR:
			if (arg)
				ret = sensor_set_wdr(sd, init->enable);
			break;
		case TX_ISP_EVENT_SENSOR_WDR_STOP:
			if (arg)
				ret = sensor_set_wdr_stop(sd, init->enable);
			break;
		case TX_ISP_EVENT_SENSOR_VFLIP:
			if (arg)
				ret = sensor_set_vflip(sd, sensor_val->value);
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
	/*.ioctl = sensor_ops_ioctl,*/
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

	sensor = (struct tx_isp_sensor *) kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));
	sensor->dev = &client->dev;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor_attr.expo_fs = 1;
	sensor->video.shvflip = shvflip;
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

static int sensor_remove(struct i2c_client *client) {
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
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
