// SPDX-License-Identifier: GPL-2.0+
/*
 * sc230ai.c
 * Copyright (C) 2023 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps     interface              mode
 *   0          1920*1080       60        mipi_2lane           linear
 *   1          1920*1080       30        mipi_2lane           hdr
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

#define SENSOR_NAME "sc230ai"
#define SENSOR_CHIP_ID_H (0xcb)
#define SENSOR_CHIP_ID_L (0x34)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_OUTPUT_MAX_FPS 60
#define SENSOR_OUTPUT_MAX_FPS_DOL 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20230909a"
#define MCLK 24000000

static int reset_gpio = GPIO_PA(18);
static int pwdn_gpio = GPIO_PC(18);

static int wdr_bufsize = 5921280;  /* (2*{0x3e24,0x3e23}-15)*fps/SENSOR_OUTPUT_MIN_FPS*1920*2 */

static int shvflip = 1;

static int data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;

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
	{0x82, 1500},
	{0x84, 2886},
	{0x86, 4342},
	{0x88, 5776},
	{0x8a, 7101},
	{0x8c, 8494},
	{0x8e, 9781},
	{0x90, 11136},
	{0x92, 12471},
	{0x94, 13706},
	{0x96, 15005},
	{0x98, 16287},
	{0x9a, 17474},
	{0x9c, 18723},
	{0x9e, 19879},
	{0xa0, 21097},
	{0xa2, 22300},
	{0xa4, 23414},
	{0xa6, 24587},
	{0xa8, 25746},
	{0xaa, 26820},
	{0xac, 27953},
	{0xae, 29002},
	{0xb0, 30109},
	{0xb2, 31203},
	{0xb4, 32217},
	{0xb6, 33287},
	{0xb8, 34345},
	{0xba, 35326},
	{0xbc, 36361},
	{0xbe, 37322},
	{0xc0, 38336},
	{0xc2, 39339},
	{0xc4, 40270},
	{0xc6, 41253},
	{0xc8, 42226},
	{0xca, 43129},
	{0xcc, 44082},
	{0xce, 44968},
	{0xd0, 45904},
	{0xd2, 46830},
	{0xd4, 47690},
	{0xd6, 48599},
	{0xd8, 49500},
	{0xda, 50336},
	{0xdc, 51220},
	{0xde, 52042},
	{0xe0, 52910},
	{0xe2, 53771},
	{0xe4, 54571},
	{0xe6, 55416},
	{0xe8, 56254},
	{0xea, 57033},
	{0xec, 57857},
	{0xee, 58623},
	{0xf0, 59433},
	{0xf2, 60237},
	{0xf4, 60984},
	{0xf6, 61774},
	{0xf8, 62558},
	{0xfa, 63287},
	{0xfc, 64059},
	{0xfe, 64776},
	{0x180, 65536},
	{0x182, 67036},
	{0x184, 68422},
	{0x186, 69878},
	{0x188, 71312},
	{0x18a, 72637},
	{0x18c, 74030},
	{0x18e, 75317},
	{0x190, 76672},
	{0x192, 78007},
	{0x194, 79242},
	{0x196, 80541},
	{0x198, 81823},
	{0x19a, 83010},
	{0x19c, 84259},
	{0x19e, 85415},
	{0x1a0, 86633},
	{0x1a2, 87836},
	{0x1a4, 88950},
	{0x1a6, 90123},
	{0x1a8, 91282},
	{0x1aa, 92356},
	{0x1ac, 93489},
	{0x1ae, 94538},
	{0x1b0, 95645},
	{0x1b2, 96739},
	{0x1b4, 97753},
	{0x1b6, 98823},
	{0x1b8, 99881},
	{0x1ba, 100862},
	{0x1bc, 101897},
	{0x1be, 102858},
	{0x1c0, 103872},
	{0x1c2, 104875},
	{0x1c4, 105806},
	{0x1c6, 106789},
	{0x4080, 106972},
	{0x4082, 108485},
	{0x4084, 109855},
	{0x4086, 111323},
	{0x4088, 112740},
	{0x408a, 114079},
	{0x408c, 115455},
	{0x408e, 116756},
	{0x4090, 118094},
	{0x4092, 119441},
	{0x4094, 120689},
	{0x4096, 121973},
	{0x4098, 123265},
	{0x409a, 124439},
	{0x409c, 125698},
	{0x409e, 126842},
	{0x40a0, 128070},
	{0x40a2, 129282},
	{0x40a4, 130384},
	{0x40a6, 131567},
	{0x40a8, 132712},
	{0x40aa, 133797},
	{0x40ac, 134916},
	{0x40ae, 135977},
	{0x40b0, 137070},
	{0x40b2, 138173},
	{0x40b4, 139198},
	{0x40b6, 140255},
	{0x40b8, 141321},
	{0x40ba, 142292},
	{0x40bc, 143336},
	{0x40be, 144286},
	{0x40c0, 145308},
	{0x40c2, 146319},
	{0x40c4, 147240},
	{0x40c6, 148231},
	{0x40c8, 149192},
	{0x40ca, 150105},
	{0x40cc, 151047},
	{0x40ce, 151942},
	{0x40d0, 152866},
	{0x40d2, 153800},
	{0x40d4, 154670},
	{0x40d6, 155568},
	{0x40d8, 156476},
	{0x40da, 157303},
	{0x40dc, 158195},
	{0x40de, 159007},
	{0x40e0, 159883},
	{0x40e2, 160750},
	{0x40e4, 161541},
	{0x40e6, 162394},
	{0x40e8, 163222},
	{0x40ea, 164009},
	{0x40ec, 164823},
	{0x40ee, 165597},
	{0x40f0, 166398},
	{0x40f2, 167208},
	{0x40f4, 167963},
	{0x40f6, 168743},
	{0x40f8, 169534},
	{0x40fa, 170255},
	{0x40fc, 171032},
	{0x40fe, 171742},
	{0x4880, 172508},
	{0x4882, 174006},
	{0x4884, 175391},
	{0x4886, 176845},
	{0x4888, 178290},
	{0x488a, 179615},
	{0x488c, 181005},
	{0x488e, 182292},
	{0x4890, 183644},
	{0x4892, 184977},
	{0x4894, 186211},
	{0x4896, 187509},
	{0x4898, 188801},
	{0x489a, 189987},
	{0x489c, 191234},
	{0x489e, 192390},
	{0x48a0, 193606},
	{0x48a2, 194806},
	{0x48a4, 195920},
	{0x48a6, 197091},
	{0x48a8, 198259},
	{0x48aa, 199333},
	{0x48ac, 200463},
	{0x48ae, 201513},
	{0x48b0, 202617},
	{0x48b2, 203709},
	{0x48b4, 204723},
	{0x48b6, 205791},
	{0x48b8, 206857},
	{0x48ba, 207838},
	{0x48bc, 208872},
	{0x48be, 209832},
	{0x48c0, 210844},
	{0x48c2, 211845},
	{0x48c4, 212776},
	{0x48c6, 213757},
	{0x48c8, 214738},
	{0x48ca, 215641},
	{0x48cc, 216593},
	{0x48ce, 217478},
	{0x48d0, 218412},
	{0x48d2, 219336},
	{0x48d4, 220197},
	{0x48d6, 221104},
	{0x48d8, 222012},
	{0x48da, 222848},
	{0x48dc, 223731},
	{0x48de, 224552},
	{0x48e0, 225419},
	{0x48e2, 226277},
	{0x48e4, 227077},
	{0x48e6, 227921},
	{0x48e8, 228766},
	{0x48ea, 229545},
	{0x48ec, 230367},
	{0x48ee, 231133},
	{0x48f0, 231942},
	{0x48f2, 232744},
	{0x48f4, 233491},
	{0x48f6, 234279},
	{0x48f8, 235070},
	{0x48fa, 235799},
	{0x48fc, 236568},
	{0x48fe, 237286},
	{0x4980, 238044},
	{0x4982, 239542},
	{0x4984, 240927},
	{0x4986, 242388},
	{0x4988, 243819},
	{0x498a, 245144},
	{0x498c, 246541},
	{0x498e, 247828},
	{0x4990, 249180},
	{0x4992, 250513},
	{0x4994, 251747},
	{0x4996, 253051},
	{0x4998, 254331},
	{0x499a, 255517},
	{0x499c, 256770},
	{0x499e, 257926},
	{0x49a0, 259142},
	{0x49a2, 260342},
	{0x49a4, 261456},
	{0x49a6, 262633},
	{0x49a8, 263790},
	{0x49aa, 264864},
	{0x49ac, 265999},
	{0x49ae, 267049},
	{0x49b0, 268153},
	{0x49b2, 269245},
	{0x49b4, 270259},
	{0x49b6, 271332},
	{0x49b8, 272388},
	{0x49ba, 273369},
	{0x49bc, 274408},
	{0x49be, 275368},
	{0x49c0, 276380},
	{0x49c2, 277381},
	{0x49c4, 278312},
	{0x49c6, 279298},
	{0x49c8, 280269},
	{0x49ca, 281172},
	{0x49cc, 282129},
	{0x49ce, 283014},
	{0x49d0, 283948},
	{0x49d2, 284872},
	{0x49d4, 285733},
	{0x49d6, 286645},
	{0x49d8, 287543},
	{0x49da, 288380},
	{0x49dc, 289267},
	{0x49de, 290088},
	{0x49e0, 290955},
	{0x49e2, 291813},
	{0x49e4, 292613},
	{0x49e6, 293461},
	{0x49e8, 294298},
	{0x49ea, 295077},
	{0x49ec, 295903},
	{0x49ee, 296669},
	{0x49f0, 297478},
	{0x49f2, 298280},
	{0x49f4, 299027},
	{0x49f6, 299819},
	{0x49f8, 300602},
	{0x49fa, 301331},
	{0x49fc, 302104},
	{0x49fe, 302822},
	{0x4b80, 303580},
	{0x4b82, 305081},
	{0x4b84, 306467},
	{0x4b86, 307924},
	{0x4b88, 309355},
	{0x4b8a, 310680},
	{0x4b8c, 312073},
	{0x4b8e, 313361},
	{0x4b90, 314716},
	{0x4b92, 316052},
	{0x4b94, 317287},
	{0x4b96, 318587},
	{0x4b98, 319867},
	{0x4b9a, 321053},
	{0x4b9c, 322303},
	{0x4b9e, 323459},
	{0x4ba0, 324678},
	{0x4ba2, 325881},
	{0x4ba4, 326995},
	{0x4ba6, 328169},
	{0x4ba8, 329326},
	{0x4baa, 330400},
	{0x4bac, 331533},
	{0x4bae, 332582},
	{0x4bb0, 333689},
	{0x4bb2, 334784},
	{0x4bb4, 335798},
	{0x4bb6, 336868},
	{0x4bb8, 337924},
	{0x4bba, 338905},
	{0x4bbc, 339941},
	{0x4bbe, 340902},
	{0x4bc0, 341916},
	{0x4bc2, 342920},
	{0x4bc4, 343851},
	{0x4bc6, 344834},
	{0x4bc8, 345805},
	{0x4bca, 346708},
	{0x4bcc, 347662},
	{0x4bce, 348548},
	{0x4bd0, 349484},
	{0x4bd2, 350411},
	{0x4bd4, 351271},
	{0x4bd6, 352181},
	{0x4bd8, 353079},
	{0x4bda, 353916},
	{0x4bdc, 354800},
	{0x4bde, 355622},
	{0x4be0, 356491},
	{0x4be2, 357352},
	{0x4be4, 358151},
	{0x4be6, 358997},
	{0x4be8, 359834},
	{0x4bea, 360613},
	{0x4bec, 361437},
	{0x4bee, 362203},
	{0x4bf0, 363014},
	{0x4bf2, 363818},
	{0x4bf4, 364565},
	{0x4bf6, 365355},
	{0x4bf8, 366138},
	{0x4bfa, 366867},
	{0x4bfc, 367638},
	{0x4bfe, 368356},
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
	.clk = 810,
	.lans = 2,
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
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_mipi_bus sensor_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 810,
	.lans = 2,
	.index = 0,
	.settle_time_apative_en = 0,
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
	.chip_id = 0xcb34,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL,
	.cbus_device = 0x30,
	.max_again = 368356,
	.max_again_short = 368356,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_short = 1,
	.min_integration_time_native = 1,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_again_short = sensor_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	.sensor_ctrl.alloc_integration_time_short = sensor_alloc_integration_time_short,
};

static struct regval_list sensor_init_regs_1920_1080_60fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x301f, 0x23},
	{0x3208, 0x07},
	{0x3209, 0x80},
	{0x320a, 0x04},
	{0x320b, 0x38},
	{0x320c, 0x09},//hts 0x960 = 2400
	{0x320d, 0x60},//
	{0x320e, 0x04},//vts 0x465 = 1125
	{0x320f, 0x64},//
	{0x3211, 0x04},
	{0x3213, 0x04},
	{0x3227, 0x03},
	{0x3250, 0x00},
	{0x3301, 0x09},
	{0x3304, 0x50},
	{0x3306, 0x48},
	{0x3308, 0x18},
	{0x3309, 0x68},
	{0x330a, 0x00},
	{0x330b, 0xc0},
	{0x331e, 0x41},
	{0x331f, 0x59},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x335d, 0x60},
	{0x335e, 0x06},
	{0x335f, 0x08},
	{0x3364, 0x5e},
	{0x337c, 0x02},
	{0x337d, 0x0a},
	{0x3390, 0x01},
	{0x3391, 0x0b},
	{0x3392, 0x0f},
	{0x3393, 0x0c},
	{0x3394, 0x0d},
	{0x3395, 0x60},
	{0x3396, 0x48},
	{0x3397, 0x49},
	{0x3398, 0x4f},
	{0x3399, 0x0a},
	{0x339a, 0x0f},
	{0x339b, 0x14},
	{0x339c, 0x60},
	{0x33a2, 0x04},
	{0x33af, 0x40},
	{0x33b1, 0x80},
	{0x33b3, 0x40},
	{0x33b9, 0x0a},
	{0x33f9, 0x70},
	{0x33fb, 0x90},
	{0x33fc, 0x4b},
	{0x33fd, 0x5f},
	{0x349f, 0x03},
	{0x34a6, 0x4b},
	{0x34a7, 0x4f},
	{0x34a8, 0x30},
	{0x34a9, 0x20},
	{0x34aa, 0x00},
	{0x34ab, 0xe0},
	{0x34ac, 0x01},
	{0x34ad, 0x00},
	{0x34f8, 0x5f},
	{0x34f9, 0x10},
	{0x3630, 0xc0},
	{0x3633, 0x44},
	{0x3637, 0x29},
	{0x363b, 0x20},
	{0x3670, 0x09},
	{0x3674, 0xb0},
	{0x3675, 0x80},
	{0x3676, 0x88},
	{0x367c, 0x40},
	{0x367d, 0x49},
	{0x3690, 0x54},
	{0x3691, 0x44},
	{0x3692, 0x55},
	{0x369c, 0x49},
	{0x369d, 0x4f},
	{0x36ae, 0x4b},
	{0x36af, 0x4f},
	{0x36b0, 0x87},
	{0x36b1, 0x9b},
	{0x36b2, 0xb7},
	{0x36d0, 0x01},
	{0x36ea, 0x09},
	{0x36eb, 0x04},
	{0x36ec, 0x0c},
	{0x36ed, 0x24},
	{0x370f, 0x01},
	{0x3722, 0x17},
	{0x3728, 0x90},
	{0x37b0, 0x17},
	{0x37b1, 0x17},
	{0x37b2, 0x97},
	{0x37b3, 0x4b},
	{0x37b4, 0x4f},
	{0x37fa, 0x09},
	{0x37fb, 0x04},
	{0x37fc, 0x00},
	{0x37fd, 0x22},
	{0x3901, 0x02},
	{0x3902, 0xc5},
	{0x3904, 0x04},
	{0x3907, 0x00},
	{0x3908, 0x41},
	{0x3909, 0x00},
	{0x390a, 0x00},
	{0x391f, 0x04},
	{0x3928, 0xc1},
	{0x3933, 0x84},
	{0x3934, 0x02},
	{0x3940, 0x62},
	{0x3941, 0x00},
	{0x3942, 0x04},
	{0x3943, 0x03},
	{0x3e00, 0x00},
	{0x3e01, 0x8c},
	{0x3e02, 0x10},
	{0x440e, 0x02},
	{0x450d, 0x11},
	{0x4819, 0x0a},
	{0x481b, 0x06},
	{0x481d, 0x16},
	{0x481f, 0x05},
	{0x4821, 0x0b},
	{0x4823, 0x05},
	{0x4825, 0x05},
	{0x4827, 0x05},
	{0x4829, 0x09},
	{0x5010, 0x01},
	{0x5787, 0x08},
	{0x5788, 0x03},
	{0x5789, 0x00},
	{0x578a, 0x10},
	{0x578b, 0x08},
	{0x578c, 0x00},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x00},
	{0x5793, 0x10},
	{0x5794, 0x08},
	{0x5795, 0x00},
	{0x5799, 0x06},
	{0x57ad, 0x00},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x3f},
	{0x5ae3, 0x38},
	{0x5ae4, 0x28},
	{0x5ae5, 0x3f},
	{0x5ae6, 0x38},
	{0x5ae7, 0x28},
	{0x5ae8, 0x3f},
	{0x5ae9, 0x3c},
	{0x5aea, 0x2c},
	{0x5aeb, 0x3f},
	{0x5aec, 0x3c},
	{0x5aed, 0x2c},
	{0x5af4, 0x3f},
	{0x5af5, 0x38},
	{0x5af6, 0x28},
	{0x5af7, 0x3f},
	{0x5af8, 0x38},
	{0x5af9, 0x28},
	{0x5afa, 0x3f},
	{0x5afb, 0x3c},
	{0x5afc, 0x2c},
	{0x5afd, 0x3f},
	{0x5afe, 0x3c},
	{0x5aff, 0x2c},
	{0x36e9, 0x53},
	{0x37f9, 0x53},
	{SENSOR_REG_DELAY, 0x10},
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_30fps_mipi_dol[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x301f, 0x29},
	{0x320c, 0x09},//hts -> 0x960 = 2400
	{0x320d, 0x60},//
	{0x320e, 0x08},//vts -> 0x8ca = 2250
	{0x320f, 0xca},//
	{0x3250, 0xff},
	{0x3281, 0x01},
	{0x3301, 0x09},
	{0x3304, 0x50},
	{0x3306, 0x48},
	{0x3308, 0x18},
	{0x3309, 0x68},
	{0x330a, 0x00},
	{0x330b, 0xc0},
	{0x331e, 0x41},
	{0x331f, 0x59},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x335d, 0x60},
	{0x335e, 0x06},
	{0x335f, 0x08},
	{0x3364, 0x5e},
	{0x337c, 0x02},
	{0x337d, 0x0a},
	{0x3390, 0x01},
	{0x3391, 0x0b},
	{0x3392, 0x0f},
	{0x3393, 0x0c},
	{0x3394, 0x0d},
	{0x3395, 0x60},
	{0x3396, 0x48},
	{0x3397, 0x49},
	{0x3398, 0x4f},
	{0x3399, 0x0a},
	{0x339a, 0x0f},
	{0x339b, 0x14},
	{0x339c, 0x60},
	{0x33a2, 0x04},
	{0x33af, 0x40},
	{0x33b1, 0x80},
	{0x33b3, 0x40},
	{0x33b9, 0x0a},
	{0x33f9, 0x70},
	{0x33fb, 0x90},
	{0x33fc, 0x4b},
	{0x33fd, 0x5f},
	{0x349f, 0x03},
	{0x34a6, 0x4b},
	{0x34a7, 0x4f},
	{0x34a8, 0x30},
	{0x34a9, 0x20},
	{0x34aa, 0x00},
	{0x34ab, 0xe0},
	{0x34ac, 0x01},
	{0x34ad, 0x00},
	{0x34f8, 0x5f},
	{0x34f9, 0x10},
	{0x3630, 0xc0},
	{0x3633, 0x44},
	{0x3637, 0x29},
	{0x363b, 0x20},
	{0x3670, 0x09},
	{0x3674, 0xb0},
	{0x3675, 0x80},
	{0x3676, 0x88},
	{0x367c, 0x40},
	{0x367d, 0x49},
	{0x3690, 0x54},
	{0x3691, 0x44},
	{0x3692, 0x55},
	{0x369c, 0x49},
	{0x369d, 0x4f},
	{0x36ae, 0x4b},
	{0x36af, 0x4f},
	{0x36b0, 0x87},
	{0x36b1, 0x9b},
	{0x36b2, 0xb7},
	{0x36d0, 0x01},
	{0x36ea, 0x09},
	{0x36eb, 0x04},
	{0x36ec, 0x0c},
	{0x36ed, 0x24},
	{0x370f, 0x01},
	{0x3722, 0x17},
	{0x3728, 0x90},
	{0x37b0, 0x17},
	{0x37b1, 0x17},
	{0x37b2, 0x97},
	{0x37b3, 0x4b},
	{0x37b4, 0x4f},
	{0x37fa, 0x09},
	{0x37fb, 0x04},
	{0x37fc, 0x00},
	{0x37fd, 0x22},
	{0x3901, 0x02},
	{0x3902, 0xc5},
	{0x3904, 0x04},
	{0x3907, 0x00},
	{0x3908, 0x41},
	{0x3909, 0x00},
	{0x390a, 0x00},
	{0x391f, 0x04},
	{0x3933, 0x84},
	{0x3934, 0x02},
	{0x3940, 0x62},
	{0x3941, 0x00},
	{0x3942, 0x04},
	{0x3943, 0x03},
	{0x3e00, 0x01},
	{0x3e01, 0x06},
	{0x3e02, 0x00},
	{0x3e04, 0x10},
	{0x3e05, 0x60},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3e10, 0x00},
	{0x3e11, 0x80},
	{0x3e12, 0x03},
	{0x3e13, 0x40},
	{0x3e23, 0x00},//
	{0x3e24, 0x88},//SEF 0x88 = 136
	{0x440e, 0x02},
	{0x450d, 0x11},
	{0x4816, 0x71},
	{0x4819, 0x0a},
	{0x481b, 0x06},
	{0x481d, 0x16},
	{0x481f, 0x05},
	{0x4821, 0x0b},
	{0x4823, 0x05},
	{0x4825, 0x05},
	{0x4827, 0x05},
	{0x4829, 0x09},
	{0x5010, 0x00},
	{0x5787, 0x08},
	{0x5788, 0x03},
	{0x5789, 0x00},
	{0x578a, 0x10},
	{0x578b, 0x08},
	{0x578c, 0x00},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x00},
	{0x5793, 0x10},
	{0x5794, 0x08},
	{0x5795, 0x00},
	{0x5799, 0x06},
	{0x57ad, 0x00},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x3f},
	{0x5ae3, 0x38},
	{0x5ae4, 0x28},
	{0x5ae5, 0x3f},
	{0x5ae6, 0x38},
	{0x5ae7, 0x28},
	{0x5ae8, 0x3f},
	{0x5ae9, 0x3c},
	{0x5aea, 0x2c},
	{0x5aeb, 0x3f},
	{0x5aec, 0x3c},
	{0x5aed, 0x2c},
	{0x5af4, 0x3f},
	{0x5af5, 0x38},
	{0x5af6, 0x28},
	{0x5af7, 0x3f},
	{0x5af8, 0x38},
	{0x5af9, 0x28},
	{0x5afa, 0x3f},
	{0x5afb, 0x3c},
	{0x5afc, 0x2c},
	{0x5afd, 0x3f},
	{0x5afe, 0x3c},
	{0x5aff, 0x2c},
	{0x36e9, 0x53},
	{0x37f9, 0x53},
	{SENSOR_REG_DELAY, 0x10},
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the jxf23_win_sizes is [full_resolution, preview_resolution]. */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 1920,
		.height = 1080,
		.fps = 60 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_60fps_mipi,
	},
	{
		.width = 1920,
		.height = 1080,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_mipi_dol,
	},
};

struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

static struct regval_list sensor_stream_on[] = {
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	{0x0100, 0x00},
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
	if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) it = (it << 2) - 2;
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
	int ret = 0;

	if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) value = (value << 2) + 1;
	ret = sensor_write(sd,  0x3e00, (unsigned char)((value >> 12) & 0x0f));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sensor_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
	ret += sensor_write(sd, 0x3e09, (unsigned char)(value & 0xff));

	return 0;
}
#endif

static int sensor_set_integration_time_short(struct tx_isp_subdev *sd, int value) {
	int ret = 0;

	value = (value << 2) - 2;
	ret = sensor_write(sd, 0x3e04, (unsigned char) ((value >> 4) & 0xff));
	ret = sensor_write(sd, 0x3e05, (unsigned char) (value & 0x0f) << 4);

	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain_short(struct tx_isp_subdev *sd, int value) {
	int ret = 0;

	ret += sensor_write(sd, 0x3e13, (unsigned char) ((value >> 8) & 0xff));
	ret += sensor_write(sd, 0x3e11, (unsigned char) (value & 0xff));

	return 0;
}

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
	unsigned int aclk = 0;
	unsigned short vts = 0;
	unsigned short hts = 0;
	unsigned int short_time = 0;
	unsigned int sensor_max_fps = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;
	unsigned char val = 0;

	switch (sensor->info.default_boot) {
		case 0:
			sclk = 2400 * 1125 * 60;
			sensor_max_fps = TX_SENSOR_MAX_FPS_60;
			break;
		case 1:
			sclk = 2400 * 2250 * 30;
			aclk = 136 * 30;
			sensor_max_fps = TX_SENSOR_MAX_FPS_30;
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

	ret += sensor_read(sd, 0x320c, &val);
	hts = val;
	val = 0;
	ret += sensor_read(sd, 0x320d, &val);
	hts = ((hts << 8) | val);

	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	sensor_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	sensor_write(sd, 0x320e, (unsigned char) (vts >> 8));

	if (sensor->info.default_boot == 1) {
		short_time = aclk * (fps & 0xffff) / ((fps & 0xffff0000) >> 16);
		sensor_write(sd, 0x3e24, (unsigned char) (short_time & 0xff));
		sensor_write(sd, 0x3e23, (unsigned char) (short_time >> 8));
	}

	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = (sensor->info.default_boot == 0) ? (2 * vts - 9) : (vts / 2 -
													      short_time /
													      2 - 4);
	sensor->video.attr->integration_time_limit = (sensor->info.default_boot == 0) ? (2 * vts - 9) : (vts / 2 -
													 short_time /
													 2 - 4);
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = (sensor->info.default_boot == 0) ? (2 * vts - 9) : (vts / 2 -
												       short_time / 2 -
												       4);
	if (sensor->info.default_boot == 1) sensor->video.attr->max_integration_time_short = short_time / 2 - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	if (ret) {
		ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
	}

	return ret;
}


static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable) {
	int ret = 0;
	uint8_t val;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	val = sensor_read(sd, 0x3221, &val);
	switch (enable) {
		case 0:
			val &= 0x99;
			break;
		case 1:
			val = ((val & 0x9F) | 0x06);
			break;
		case 2:
			val = ((val & 0xF9) | 0x60);
			break;
		case 3:
			val |= 0x66;
			break;
	}
	sensor_write(sd, 0x3221, val);
	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
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
			wsize = &sensor_win_sizes[0];
			memcpy(&sensor_attr.mipi, &sensor_mipi_linear, sizeof(sensor_mipi_linear));
			sensor_attr.mipi.clk = 810,
				sensor_attr.min_integration_time = 2;
			sensor_attr.min_integration_time_native = 2,
				sensor_attr.total_width = 2400;
			sensor_attr.total_height = 1125;
			sensor_attr.max_integration_time_native = 2241;   /* 1125*2-9 */
			sensor_attr.integration_time_limit = 2241;
			sensor_attr.max_integration_time = 2241;
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			printk("=================> linear is ok");
			break;
		case 1:
			sensor_attr.wdr_cache = wdr_bufsize;
			wsize = &sensor_win_sizes[1];
			memcpy(&sensor_attr.mipi, &sensor_mipi_dol, sizeof(sensor_mipi_dol));
			sensor_attr.mipi.clk = 810,
				sensor_attr.min_integration_time = 1;
			sensor_attr.min_integration_time_short = 1;
			sensor_attr.min_integration_time_native = 1,
				sensor_attr.total_width = 2400;
			sensor_attr.total_height = 2250;
			sensor_attr.max_integration_time_native = 1053; /* vts/2 -rhs/2-5 */
			sensor_attr.integration_time_limit = 1053;
			sensor_attr.max_integration_time = 1053;
			sensor_attr.max_integration_time_short = 64; /* rhs/2-4 */
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
			printk("=================> 25fps_hdr is ok");
			break;
		default:
			ISP_ERROR("Have no this setting!!!\n");
	}

	data_type = sensor_attr.data_type;

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
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

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

	ret = sensor_write(sd, 0x0103, 0x01);
	ret = sensor_write(sd, 0x0100, 0x00);
	if (wdr_en == 1) {
		info->default_boot = 1;
		memcpy(&sensor_attr.mipi, &sensor_mipi_dol, sizeof(sensor_mipi_dol));
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		sensor_attr.wdr_cache = wdr_bufsize;
		wsize = &sensor_win_sizes[1];
		sensor_attr.mipi.clk = 810,
			sensor_attr.min_integration_time = 1;
		sensor_attr.min_integration_time_short = 1;
		sensor_attr.min_integration_time_native = 1,
			sensor_attr.total_width = 2400;
		sensor_attr.total_height = 2500;
		sensor_attr.max_integration_time_native = 1053;
		sensor_attr.integration_time_limit = 1053;
		sensor_attr.max_integration_time = 1053;
		sensor_attr.max_integration_time_short = 64;
		printk("\n-------------------------switch wdr ok ----------------------\n");
	} else if (wdr_en == 0) {
		info->default_boot = 0;
		memcpy(&sensor_attr.mipi, &sensor_mipi_linear, sizeof(sensor_mipi_linear));
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		wsize = &sensor_win_sizes[0];
		sensor_attr.mipi.clk = 810,
			sensor_attr.min_integration_time = 2;
		sensor_attr.min_integration_time_native = 2,
			sensor_attr.total_width = 2400;
		sensor_attr.total_height = 1125;
		sensor_attr.max_integration_time_native = 2241;
		sensor_attr.integration_time_limit = 2241;
		sensor_attr.max_integration_time = 2241;
		printk("\n-------------------------switch linear ok ----------------------\n");
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	data_type = sensor_attr.data_type;

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
		case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
			if (arg)
				ret = sensor_set_analog_gain_short(sd, sensor_val->value);
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
