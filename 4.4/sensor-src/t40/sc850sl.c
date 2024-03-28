// SPDX-License-Identifier: GPL-2.0+
/*
 * sc850sl.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          1920*1080       25        mipi_2lane            linear
 *
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

#define SENSOR_NAME "sc850sl"
#define SENSOR_CHIP_ID_H (0x9d)
#define SENSOR_CHIP_ID_L (0x1e)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK_25FPS (37125000) /* 1100*1350*25 */
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_1
#define SENSOR_VERSION "H20230404a"

static int reset_gpio = GPIO_PC(27);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x340, 0},
	{0x341, 1500},
	{0x342, 2886},
	{0x343, 4342},
	{0x344, 5776},
	{0x345, 7101},
	{0x346, 8494},
	{0x347, 9781},
	{0x348, 11136},
	{0x349, 12471},
	{0x34a, 13706},
	{0x34b, 15005},
	{0x34c, 16287},
	{0x34d, 17474},
	{0x34e, 18723},
	{0x34f, 19879},
	{0x350, 21097},
	{0x351, 22300},
	{0x352, 23414},
	{0x353, 24587},
	{0x354, 25746},
	{0x355, 26820},
	{0x356, 27953},
	{0x357, 29002},
	{0x358, 30109},
	{0x359, 31203},
	{0x35a, 32217},
	{0x35b, 33287},
	{0x35c, 34345},
	{0x35d, 35326},
	{0x35e, 36361},
	{0x35f, 37322},
	{0x360, 38336},
	{0x361, 39339},
	{0x362, 40270},
	{0x363, 41253},
	{0x364, 42226},
	{0x365, 43129},
	{0x366, 44082},
	{0x367, 44968},
	{0x368, 45904},
	{0x369, 46830},
	{0x36a, 47690},
	{0x36b, 48599},
	{0x36c, 49500},
	{0x36d, 50336},
	{0x36e, 51220},
	{0x36f, 52042},
	{0x370, 52910},
	{0x371, 53771},
	{0x372, 54571},
	{0x373, 55416},
	{0x374, 56254},
	{0x375, 57033},
	{0x376, 57857},
	{0x377, 58623},
	{0x378, 59433},
	{0x379, 60237},
	{0x37a, 60984},
	{0x37b, 61774},
	{0x37c, 62558},
	{0x37d, 63287},
	{0x37e, 64059},
	{0x37f, 64776},
	{0x740, 65536},
	{0x741, 66990},
	{0x742, 68468},
	{0x743, 69878},
	{0x744, 71267},
	{0x745, 72637},
	{0x746, 74030},
	{0x747, 75360},
	{0x748, 76672},
	{0x749, 77965},
	{0x74a, 79283},
	{0x74b, 80541},
	{0x74c, 81784},
	{0x74d, 83010},
	{0x74e, 84259},
	{0x74f, 85454},
	{0x750, 86633},
	{0x751, 87799},
	{0x752, 88986},
	{0x753, 90123},
	{0x754, 91246},
	{0x755, 92356},
	{0x756, 93489},
	{0x757, 94573},
	{0x758, 95645},
	{0x759, 96705},
	{0x75a, 97786},
	{0x75b, 98823},
	{0x75c, 99848},
	{0x75d, 100862},
	{0x75e, 101897},
	{0x75f, 102890},
	{0x760, 103872},
	{0x761, 104844},
	{0x762, 105837},
	{0x763, 106789},
	{0x2340, 107731},
	{0x2341, 109202},
	{0x2342, 110651},
	{0x2343, 112048},
	{0x2344, 113454},
	{0x2345, 114840},
	{0x2346, 116205},
	{0x2347, 117551},
	{0x2348, 118878},
	{0x2349, 120160},
	{0x234a, 121451},
	{0x234b, 122724},
	{0x234c, 123981},
	{0x234d, 125221},
	{0x234e, 126445},
	{0x234f, 127629},
	{0x2350, 128823},
	{0x2351, 130002},
	{0x2352, 131166},
	{0x2353, 132316},
	{0x2354, 133452},
	{0x2355, 134552},
	{0x2356, 135662},
	{0x2357, 136759},
	{0x2358, 137843},
	{0x2359, 138915},
	{0x235a, 139975},
	{0x235b, 141002},
	{0x235c, 142039},
	{0x235d, 143065},
	{0x235e, 144080},
	{0x235f, 145084},
	{0x2360, 146077},
	{0x2361, 147041},
	{0x2362, 148014},
	{0x2363, 148977},
	{0x2364, 149931},
	{0x2365, 150875},
	{0x2366, 151790},
	{0x2367, 152716},
	{0x2368, 153633},
	{0x2369, 154541},
	{0x236a, 155440},
	{0x236b, 156331},
	{0x236c, 157196},
	{0x236d, 158070},
	{0x236e, 158937},
	{0x236f, 159795},
	{0x2370, 160646},
	{0x2371, 161490},
	{0x2372, 162309},
	{0x2373, 163137},
	{0x2374, 163959},
	{0x2375, 164773},
	{0x2376, 165581},
	{0x2377, 166381},
	{0x2378, 167159},
	{0x2379, 167947},
	{0x237a, 168728},
	{0x237b, 169502},
	{0x237c, 170270},
	{0x237d, 171032},
	{0x237e, 171773},
	{0x237f, 172523},
	{0x2740, 173267},
	{0x2741, 174738},
	{0x2742, 176172},
	{0x2743, 177599},
	{0x2744, 179005},
	{0x2745, 180376},
	{0x2746, 181741},
	{0x2747, 183087},
	{0x2748, 184400},
	{0x2749, 185709},
	{0x274a, 187000},
	{0x274b, 188260},
	{0x274c, 189517},
	{0x274d, 190757},
	{0x274e, 191969},
	{0x274f, 193178},
	{0x2750, 194371},
	{0x2751, 195538},
	{0x2752, 196702},
	{0x2753, 197840},
	{0x2754, 198977},
	{0x2755, 200100},
	{0x2756, 201198},
	{0x2757, 202295},
	{0x2758, 203379},
	{0x2759, 204441},
	{0x275a, 205501},
	{0x275b, 206549},
	{0x275c, 207575},
	{0x275d, 208601},
	{0x275e, 209616},
	{0x275f, 210610},
	{0x2760, 211603},
	{0x2761, 212587},
	{0x2762, 213550},
	{0x2763, 214513},
	{0x2764, 215467},
	{0x2765, 216401},
	{0x2766, 217336},
	{0x2767, 218262},
	{0x2768, 219169},
	{0x2769, 220077},
	{0x276a, 220976},
	{0x276b, 221858},
	{0x276c, 222741},
	{0x276d, 223615},
	{0x276e, 224473},
	{0x276f, 225331},
	{0x2770, 226182},
	{0x2771, 227017},
	{0x2772, 227853},
	{0x2773, 228673},
	{0x2774, 229495},
	{0x2775, 230309},
	{0x2776, 231109},
	{0x2777, 231909},
	{0x2778, 232703},
	{0x2779, 233483},
	{0x277a, 234264},
	{0x277b, 235038},
	{0x277c, 235799},
	{0x277d, 236561},
	{0x277e, 237317},
	{0x277f, 238059},
	{0x2f40, 238803},
	{0x2f41, 240267},
	{0x2f42, 241715},
	{0x2f43, 243135},
	{0x2f44, 244533},
	{0x2f45, 245919},
	{0x2f46, 247277},
	{0x2f47, 248616},
	{0x2f48, 249943},
	{0x2f49, 251245},
	{0x2f4a, 252529},
	{0x2f4b, 253796},
	{0x2f4c, 255053},
	{0x2f4d, 256287},
	{0x2f4e, 257505},
	{0x2f4f, 258714},
	{0x2f50, 259901},
	{0x2f51, 261074},
	{0x2f52, 262238},
	{0x2f53, 263382},
	{0x2f54, 264513},
	{0x2f55, 265636},
	{0x2f56, 266740},
	{0x2f57, 267831},
	{0x2f58, 268915},
	{0x2f59, 269982},
	{0x2f5a, 271037},
	{0x2f5b, 272080},
	{0x2f5c, 273117},
	{0x2f5d, 274137},
	{0x2f5f, 276151},
	{0x2f5e, 275147},
	{0x2f5f, 276151},
	{0x2f60, 277139},
	{0x2f61, 278118},
	{0x2f62, 279091},
	{0x2f63, 280049},
	{0x2f64, 280998},
	{0x2f65, 281942},
	{0x2f66, 282872},
	{0x2f67, 283793},
	{0x2f68, 284710},
	{0x2f69, 285613},
	{0x2f6a, 286508},
	{0x2f6b, 287394},
	{0x2f6c, 288277},
	{0x2f6d, 289147},
	{0x2f6e, 290009},
	{0x2f6f, 290867},
	{0x2f70, 291714},
	{0x2f71, 292553},
	{0x2f72, 293389},
	{0x2f73, 294214},
	{0x2f74, 295031},
	{0x2f75, 295845},
	{0x2f76, 296649},
	{0x2f77, 297445},
	{0x2f78, 298239},
	{0x2f79, 299023},
	{0x2f7a, 299800},
	{0x2f7b, 300570},
	{0x2f7c, 301338},
	{0x2f7d, 302097},
	{0x2f7e, 302849},
	{0x2f7f, 303599},
	{0x3f40, 304339},
	{0x3f41, 305807},
	{0x3f42, 307248},
	{0x3f43, 308671},
	{0x3f44, 310073},
	{0x3f45, 311451},
	{0x3f46, 312813},
	{0x3f47, 314152},
	{0x3f48, 315475},
	{0x3f49, 316781},
	{0x3f4a, 318065},
	{0x3f4b, 319336},
	{0x3f4c, 320589},
	{0x3f4d, 321823},
	{0x3f4e, 323044},
	{0x3f4f, 324247},
	{0x3f50, 325437},
	{0x3f51, 326613},
	{0x3f52, 327771},
	{0x3f53, 328918},
	{0x3f54, 330052},
	{0x3f55, 331169},
	{0x3f56, 332276},
	{0x3f57, 333367},
	{0x3f58, 334449},
	{0x3f59, 335518},
	{0x3f5a, 336573},
	{0x3f5b, 337618},
	{0x3f5c, 338653},
	{0x3f5d, 339673},
	{0x3f5e, 340685},
	{0x3f5f, 341684},
	{0x3f60, 342675},
	{0x3f61, 343656},
	{0x3f62, 344624},
	{0x3f63, 345585},
	{0x3f64, 346536},
	{0x3f65, 347476},
	{0x3f66, 348408},
	{0x3f67, 349329},
	{0x3f68, 350243},
	{0x3f69, 351149},
	{0x3f6a, 352044},
	{0x3f6b, 352932},
	{0x3f6c, 353813},
	{0x3f6d, 354683},
	{0x3f6e, 355547},
	{0x3f6f, 356401},
	{0x3f70, 357250},
	{0x3f71, 358091},
	{0x3f72, 358923},
	{0x3f73, 359750},
	{0x3f74, 360569},
	{0x3f75, 361379},
	{0x3f76, 362185},
	{0x3f77, 362981},
	{0x3f78, 363773},
	{0x3f79, 364559},
	{0x3f7a, 365336},
	{0x3f7b, 366108},
	{0x3f7c, 366874},
	{0x3f7d, 367633},
	{0x3f7e, 368387},
	{0x3f7f, 369133},
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

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x9d1e,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 720,
		.lans = 2,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
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
	.max_again = 369133,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1346, /* vts-4 */
	.integration_time_limit = 1346,
	.total_width = 1100,
	.total_height = 1350,
	.max_integration_time = 1346,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_1920_1080_25fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x36f9,0x80},
	{0x36ea,0x08},
	{0x36eb,0x0c},
	{0x36ec,0x4a},
	{0x36ed,0x24},
	{0x36fa,0x0b},
	{0x36fb,0x17},
	{0x36fc,0x10},
	{0x36fd,0x37},
	{0x36e9,0x44},
	{0x36f9,0x40},
	{0x3018,0x3a},
	{0x3019,0xfc},
	{0x301a,0x30},
	{0x301e,0x3c},
	{0x301f,0x59},
	{0x302a,0x00},
	{0x3031,0x0a},
	{0x3032,0x20},
	{0x3033,0x22},
	{0x3037,0x00},
	{0x303e,0xb4},
	{0x3208,0x07},
	{0x3209,0x80},
	{0x320a,0x04},
	{0x320b,0x38},
	{0x320c,0x04},//hts 0x44c -> 1100
	{0x320d,0x4c},//
	{0x320e,0x05},//vts 0x546 -> 1350
	{0x320f,0x46},//
	{0x3211,0x08},
	{0x3213,0x04},
	{0x3215,0x31},
	{0x3220,0x01},
	{0x3226,0x00},
	{0x3227,0x03},
	{0x3230,0x11},
	{0x3231,0x93},
	{0x3250,0x40},
	{0x3253,0x08},
	{0x327e,0x00},
	{0x3280,0x00},
	{0x3281,0x00},
	{0x3301,0x3c},
	{0x3304,0x30},
	{0x3306,0xe8},
	{0x3308,0x10},
	{0x3309,0x70},
	{0x330a,0x01},
	{0x330b,0xe0},
	{0x330d,0x10},
	{0x3314,0x92},
	{0x331e,0x29},
	{0x331f,0x69},
	{0x3333,0x10},
	{0x3347,0x05},
	{0x3348,0xd0},
	{0x3352,0x01},
	{0x3356,0x38},
	{0x335d,0x60},
	{0x3362,0x70},
	{0x338f,0x80},
	{0x33af,0x48},
	{0x33fe,0x00},
	{0x3400,0x12},
	{0x3406,0x04},
	{0x3410,0x12},
	{0x3416,0x06},
	{0x3433,0x01},
	{0x3440,0x12},
	{0x3446,0x08},
	{0x3450,0x55},
	{0x3451,0xaa},
	{0x3452,0x55},
	{0x3453,0xaa},
	{0x3478,0x01},
	{0x3479,0x01},
	{0x347a,0x02},
	{0x347b,0x01},
	{0x347c,0x04},
	{0x347d,0x01},
	{0x3616,0x0c},
	{0x3620,0x92},
	{0x3622,0x74},
	{0x3629,0x74},
	{0x362a,0xf0},
	{0x362b,0x0f},
	{0x362d,0x00},
	{0x3630,0x68},
	{0x3633,0x22},
	{0x3634,0x22},
	{0x3635,0x20},
	{0x3637,0x06},
	{0x3638,0x26},
	{0x363b,0x06},
	{0x363c,0x07},
	{0x363d,0x05},
	{0x363e,0x8f},
	{0x3648,0xe0},
	{0x3649,0x0a},
	{0x364a,0x06},
	{0x364c,0x6a},
	{0x3650,0x3d},
	{0x3654,0x40},
	{0x3656,0x68},
	{0x3657,0x0f},
	{0x3658,0x3d},
	{0x365c,0x40},
	{0x365e,0x68},
	{0x3901,0x04},
	{0x3904,0x20},
	{0x3905,0x91},
	{0x391e,0x83},
	{0x3928,0x04},
	{0x3933,0xa0},
	{0x3934,0x0a},
	{0x3935,0x68},
	{0x3936,0x00},
	{0x3937,0x20},
	{0x3938,0x0a},
	{0x3946,0x20},
	{0x3961,0x40},
	{0x3962,0x40},
	{0x3963,0xc8},
	{0x3964,0xc8},
	{0x3965,0x40},
	{0x3966,0x40},
	{0x3967,0x00},
	{0x39cd,0xc8},
	{0x39ce,0xc8},
	{0x3e00,0x00},
	{0x3e01,0x54},
	{0x3e02,0x00},
	{0x3e0e,0x02},
	{0x3e0f,0x00},
	{0x3e1c,0x0f},
	{0x3e23,0x00},
	{0x3e24,0x00},
	{0x3e53,0x00},
	{0x3e54,0x00},
	{0x3e68,0x00},
	{0x3e69,0x80},
	{0x3e73,0x00},
	{0x3e74,0x00},
	{0x3e86,0x03},
	{0x3e87,0x40},
	{0x3f02,0x24},
	{0x4424,0x02},
	{0x4501,0xc4},
	{0x4509,0x20},
	{0x4561,0x12},
	{0x4800,0x24},
	{0x4837,0x0b},
	{0x4900,0x24},
	{0x4937,0x0b},
	{0x5000,0x4e},
	{0x500f,0x35},
	{0x5020,0x00},
	{0x5787,0x10},
	{0x5788,0x06},
	{0x5789,0x00},
	{0x578a,0x18},
	{0x578b,0x0c},
	{0x578c,0x00},
	{0x5790,0x10},
	{0x5791,0x06},
	{0x5792,0x01},
	{0x5793,0x18},
	{0x5794,0x0c},
	{0x5795,0x01},
	{0x5799,0x06},
	{0x57a2,0x60},
	{0x5900,0xf1},
	{0x5901,0x04},
	{0x59e0,0xfe},
	{0x59e1,0x40},
	{0x59e2,0x38},
	{0x59e3,0x30},
	{0x59e4,0x20},
	{0x59e5,0x38},
	{0x59e6,0x30},
	{0x59e7,0x20},
	{0x59e8,0x3f},
	{0x59e9,0x38},
	{0x59ea,0x30},
	{0x59eb,0x3f},
	{0x59ec,0x38},
	{0x59ed,0x30},
	{0x59ee,0xfe},
	{0x59ef,0x40},
	{0x59f4,0x38},
	{0x59f5,0x30},
	{0x59f6,0x20},
	{0x59f7,0x38},
	{0x59f8,0x30},
	{0x59f9,0x20},
	{0x59fa,0x3f},
	{0x59fb,0x38},
	{0x59fc,0x30},
	{0x59fd,0x3f},
	{0x59fe,0x38},
	{0x59ff,0x30},
	{0x0100,0x01},
	{SENSOR_REG_DELAY,0x10},
	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
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

	ret = sensor_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x3108, &v);
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
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	ret += sensor_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
	ret += sensor_write(sd, 0x3e09, (unsigned char)(again & 0xff));
	ret += sensor_write(sd, 0x3e08, (unsigned char)(((again >> 8) & 0xff)));
	if (again < 0x740) ret += sensor_write(sd,0x363c,0x05);
	else ret += sensor_write(sd,0x363c,0x07);
	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = sensor_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0x0f));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sensor_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3e08, (unsigned char)((value & 0xff00) >> 8));
	if (again < 0x740) ret += sensor_write(sd,0x363c,0x05);
	else ret += sensor_write(sd,0x363c,0x07);
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

	if (!init->enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	sensor->video.state = TX_ISP_MODULE_INIT;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sensor_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if (sensor->video.state == TX_ISP_MODULE_RUNNING) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
			ISP_WARNING("%s stream on\n", SENSOR_NAME);
		}
	}
	else {
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
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

	switch(sensor->info.default_boot) {
	case 0:
		sclk = SENSOR_SUPPORT_SCLK_25FPS;
		max_fps = TX_SENSOR_MAX_FPS_25;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}

	ret += sensor_read(sd, 0x320c, &val);
	hts = val << 8;
	ret += sensor_read(sd, 0x320d, &val);
	hts = (hts | val);
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);


	ret = sensor_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
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

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = -1;
	unsigned char val = 0x0;

	ret += sensor_read(sd, 0x3221, &val);
	switch(enable)
	{
		case 0:
			val &= 0x99;
			break;
		case 1:
			val &= 0x9F;
			val |= 0x06;
			break;
		case 2:
			val &= 0xF9;
			val |= 0x60;
			break;
		case 3:
			val |= 0x66;
			break;
	}
	ret += sensor_write(sd, 0x3221, val);
	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
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
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	struct clk *sclka;
	int ret = 0;
    switch(info->default_boot) {
		case 0:
            wsize = &sensor_win_sizes[0];
            sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
            sensor_attr.mipi.clk = 720;
			sensor_attr.again = 0;
            sensor_attr.integration_time = 0x540;
            break;
    default:
            ISP_ERROR("Have no this MCLK Source!!!\n");
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
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	rate = private_clk_get_rate(sensor->mclk);
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

	private_clk_set_rate(sensor->mclk, 27000000);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

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
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
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

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	//return 0;
	switch(cmd) {
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = sensor_set_expo(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		//if (arg)
		//	ret = sensor_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		//if (arg)
		//	ret = sensor_set_analog_gain(sd, sensor_val->value);
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
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if (arg)
			ret = sensor_set_vflip(sd, sensor_val->value);
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
	sensor->video.shvflip = shvflip;
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
		.owner = NULL,
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
