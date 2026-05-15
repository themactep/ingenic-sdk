/*
 * sc500ai.c
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

#define SC500AI_CHIP_ID_H	(0xce)
#define SC500AI_CHIP_ID_L	(0x1f)
#define SC500AI_REG_END		0xffff
#define SC500AI_REG_DELAY	0xfffe
#define SC500AI_SUPPORT_SCLK_3M_FPS_30 (126000000)
#define SC500AI_SUPPORT_SCLK_4M_FPS_30 (158400000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20210615a"

typedef enum {
    SENSOR_RES_370 = 370,
    SENSOR_RES_400 = 400,
}sensor_resolution_mode;

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_resolution = SENSOR_RES_400;
module_param(sensor_resolution, int, S_IRUGO);
MODULE_PARM_DESC(sensor_resolution, "Sensor Resolution set interface");

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static bool dpc_flag = true;

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

struct again_lut sc500ai_again_lut[] = {
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
	{0x352, 23413},
	{0x353, 24587},
	{0x354, 25746},
	{0x355, 26820},
	{0x356, 27952},
	{0x357, 29002},
	{0x358, 30108},
	{0x359, 31202},
	{0x35a, 32216},
	{0x35b, 33286},
	{0x35c, 34344},
	{0x35d, 35325},
	{0x35e, 36361},
	{0x35f, 37321},
	{0x360, 38335},
	{0x2340, 39338},
	{0x2341, 40823},
	{0x2342, 42225},
	{0x2343, 43666},
	{0x2344, 45085},
	{0x2345, 46425},
	{0x2346, 47804},
	{0x2347, 49162},
	{0x2348, 50502},
	{0x2349, 51768},
	{0x234a, 53071},
	{0x234b, 54357},
	{0x234c, 55573},
	{0x234d, 56825},
	{0x234e, 58061},
	{0x234f, 59231},
	{0x2350, 60436},
	{0x2351, 61626},
	{0x2352, 62752},
	{0x2353, 63913},
	{0x2354, 65061},
	{0x2355, 66147},
	{0x2356, 67268},
	{0x2357, 68375},
	{0x2358, 69470},
	{0x2359, 70507},
	{0x235a, 71577},
	{0x235b, 72636},
	{0x235c, 73639},
	{0x235d, 74675},
	{0x235e, 75699},
	{0x235f, 76671},
	{0x2360, 77674},
	{0x2361, 78666},
	{0x2362, 79608},
	{0x2363, 80581},
	{0x2364, 81543},
	{0x2365, 82457},
	{0x2366, 83401},
	{0x2367, 84335},
	{0x2368, 85261},
	{0x2369, 86139},
	{0x236a, 87047},
	{0x236b, 87947},
	{0x236c, 88800},
	{0x236d, 89683},
	{0x236e, 90558},
	{0x236f, 91389},
	{0x2370, 92248},
	{0x2371, 93100},
	{0x2372, 93908},
	{0x2373, 94745},
	{0x2374, 95575},
	{0x2375, 96363},
	{0x2376, 97178},
	{0x2377, 97986},
	{0x2378, 98788},
	{0x2379, 99550},
	{0x237a, 100338},
	{0x237b, 101120},
	{0x237c, 101863},
	{0x237d, 102633},
	{0x237e, 103396},
	{0x237f, 104122},
	{0x2740, 104873},
	{0x2741, 106328},
	{0x2742, 107790},
	{0x2743, 109201},
	{0x2744, 110620},
	{0x2745, 111989},
	{0x2746, 113339},
	{0x2747, 114697},
	{0x2748, 116009},
	{0x2749, 117303},
	{0x274a, 118606},
	{0x274b, 119865},
	{0x274c, 121134},
	{0x274d, 122360},
	{0x274e, 123571},
	{0x274f, 124791},
	{0x2750, 125971},
	{0x2751, 127136},
	{0x2752, 128311},
	{0x2753, 129448},
	{0x2754, 130596},
	{0x2755, 131706},
	{0x2756, 132803},
	{0x2757, 133910},
	{0x2758, 134982},
	{0x2759, 136042},
	{0x275a, 137112},
	{0x275b, 138149},
	{0x275c, 139196},
	{0x275d, 140210},
	{0x275e, 141213},
	{0x275f, 142227},
	{0x2760, 143209},
	{0x2761, 144181},
	{0x2762, 145163},
	{0x2763, 146116},
	{0x2764, 147078},
	{0x2765, 148012},
	{0x2766, 148936},
	{0x2767, 149870},
	{0x2768, 150776},
	{0x2769, 151674},
	{0x276a, 152582},
	{0x276b, 153463},
	{0x276c, 154354},
	{0x276d, 155218},
	{0x276e, 156075},
	{0x276f, 156942},
	{0x2770, 157783},
	{0x2771, 158617},
	{0x2772, 159461},
	{0x2773, 160280},
	{0x2774, 161110},
	{0x2775, 161915},
	{0x2776, 162713},
	{0x2777, 163521},
	{0x2778, 164306},
	{0x2779, 165085},
	{0x277a, 165873},
	{0x277b, 166639},
	{0x277c, 167414},
	{0x277d, 168168},
	{0x277e, 168915},
	{0x277f, 169673},
	{0x2f40, 170408},
	{0x2f41, 171878},
	{0x2f42, 173325},
	{0x2f43, 174736},
	{0x2f44, 176140},
	{0x2f45, 177524},
	{0x2f46, 178888},
	{0x2f47, 180218},
	{0x2f48, 181544},
	{0x2f49, 182852},
	{0x2f4a, 184141},
	{0x2f4b, 185400},
	{0x2f4c, 186656},
	{0x2f4d, 187895},
	{0x2f4e, 189118},
	{0x2f4f, 190313},
	{0x2f50, 191506},
	{0x2f51, 192683},
	{0x2f52, 193846},
	{0x2f53, 194983},
	{0x2f54, 196119},
	{0x2f55, 197241},
	{0x2f56, 198349},
	{0x2f57, 199434},
	{0x2f58, 200517},
	{0x2f59, 201588},
	{0x2f5a, 202647},
	{0x2f5b, 203684},
	{0x2f5c, 204720},
	{0x2f5d, 205745},
	{0x2f5e, 206758},
	{0x2f5f, 207751},
	{0x2f60, 208744},
	{0x2f61, 209726},
	{0x2f62, 210698},
	{0x2f63, 211651},
	{0x2f64, 212603},
	{0x2f65, 213547},
	{0x2f66, 214480},
	{0x2f67, 215396},
	{0x2f68, 216311},
	{0x2f69, 217219},
	{0x2f6a, 218117},
	{0x2f6b, 218998},
	{0x2f6c, 219880},
	{0x2f6d, 220753},
	{0x2f6e, 221619},
	{0x2f6f, 222468},
	{0x2f70, 223318},
	{0x2f71, 224161},
	{0x2f72, 224996},
	{0x2f73, 225815},
	{0x2f74, 226636},
	{0x2f75, 227450},
	{0x2f76, 228256},
	{0x2f77, 229048},
	{0x2f78, 229841},
	{0x2f79, 230628},
	{0x2f7a, 231408},
	{0x2f7b, 232174},
	{0x2f7c, 232941},
	{0x2f7d, 233703},
	{0x2f7e, 234458},
	{0x2f7f, 235200},
	{0x3f40, 235943},
	{0x3f41, 237413},
	{0x3f42, 238853},
	{0x3f43, 240278},
	{0x3f44, 241675},
	{0x3f45, 243059},
	{0x3f46, 244416},
	{0x3f47, 245760},
	{0x3f48, 247079},
	{0x3f49, 248387},
	{0x3f4a, 249670},
	{0x3f4b, 250942},
	{0x3f4c, 252191},
	{0x3f4d, 253430},
	{0x3f4e, 254647},
	{0x3f4f, 255855},
	{0x3f50, 257041},
	{0x3f51, 258218},
	{0x3f52, 259375},
	{0x3f53, 260524},
	{0x3f54, 261654},
	{0x3f55, 262776},
	{0x3f56, 263878},
	{0x3f57, 264974},
	{0x3f58, 266052},
	{0x3f59, 267123},
	{0x3f5a, 268177},
	{0x3f5b, 269224},
	{0x3f5c, 270255},
	{0x3f5d, 271280},
	{0x3f5e, 272288},
	{0x3f5f, 273291},
	{0x3f60, 274279},
	{0x3f61, 275261},
	{0x3f62, 276228},
	{0x3f63, 277191},
	{0x3f64, 278138},
	{0x3f65, 279082},
	{0x3f66, 280011},
	{0x3f67, 280935},
	{0x3f68, 281846},
	{0x3f69, 282754},
	{0x3f6a, 283647},
	{0x3f6b, 284538},
	{0x3f6c, 285415},
	{0x3f6d, 286288},
	{0x3f6e, 287150},
	{0x3f6f, 288007},
	{0x3f70, 288853},
	{0x3f71, 289696},
	{0x3f72, 290527},
	{0x3f73, 291355},
	{0x3f74, 292171},
	{0x3f75, 292985},
	{0x3f76, 293787},
	{0x3f77, 294587},
	{0x3f78, 295376},
	{0x3f79, 296163},
	{0x3f7a, 296939},
	{0x3f7b, 297713},
	{0x3f7c, 298476},
	{0x3f7d, 299238},
	{0x3f7e, 299989},
	{0x3f7f, 300739}
};

struct tx_isp_sensor_attribute sc500ai_attr;

unsigned int sc500ai_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc500ai_again_lut;
	while(lut->gain <= sc500ai_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == sc500ai_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc500ai_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sc500ai_attr={
	.name = "sc500ai",
	.chip_id = 0xce1f,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 600,
		.lans = 2,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 2592,
		.image_theight = 1620,
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
	.max_again = 300739,
	.max_dgain = 0,
	.min_integration_time = 3,
	.min_integration_time_native = 3,
	.max_integration_time_native = 1980 - 8,
	.integration_time_limit = 1980 - 8,
	.total_width = 3200,
	.total_height = 1980,
	.max_integration_time = 1980 - 8,
	.one_line_expr_in_us = 20,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc500ai_alloc_again,
	.sensor_ctrl.alloc_dgain = sc500ai_alloc_dgain,
};

static struct regval_list sc500ai_init_regs_2592_1620_30fps_mipi[] = {
    {0x0103,0x01},
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x36f9,0x80},
    {0x3018,0x32},
    {0x3019,0x0c},
    {0x301f,0x58},
    {0x3200,0x00},
    {0x3201,0x90},
    {0x3202,0x00},
    {0x3203,0x00},
    {0x3204,0x0a},
    {0x3205,0xb7},
    {0x3206,0x06},
    {0x3207,0x5b},
    {0x3208,0x0a},
    {0x3209,0x20},
    {0x320a,0x06},
    {0x320b,0x54},
    {0x320e,0x07},
    {0x320f,0xbc},
    {0x3210,0x00},
    {0x3211,0x04},
    {0x3212,0x00},
    {0x3213,0x04},
    {0x3250,0x40},
    {0x3253,0x0a},
    {0x3301,0x0a},
    {0x3302,0x18},
    {0x3303,0x10},
    {0x3304,0x60},
    {0x3306,0x60},
    {0x3308,0x10},
    {0x3309,0x70},
    {0x330a,0x00},
    {0x330b,0xf0},
    {0x330d,0x18},
    {0x330e,0x20},
    {0x330f,0x02},
    {0x3310,0x02},
    {0x331c,0x04},
    {0x331e,0x51},
    {0x331f,0x61},
    {0x3320,0x09},
    {0x3333,0x10},
    {0x334c,0x08},
    {0x3356,0x09},
    {0x3364,0x17},
    {0x336d,0x03},
    {0x3390,0x08},
    {0x3391,0x18},
    {0x3392,0x38},
    {0x3393,0x0a},
    {0x3394,0x20},
    {0x3395,0x20},
    {0x3396,0x08},
    {0x3397,0x18},
    {0x3398,0x38},
    {0x3399,0x0a},
    {0x339a,0x20},
    {0x339b,0x20},
    {0x339c,0x20},
    {0x33ac,0x10},
    {0x33ae,0x10},
    {0x33af,0x19},
    {0x360f,0x01},
    {0x3622,0x03},
    {0x363a,0x1f},
    {0x363c,0x40},
    {0x3651,0x7d},
    {0x3670,0x0a},
    {0x3671,0x07},
    {0x3672,0x17},
    {0x3673,0x1e},
    {0x3674,0x82},
    {0x3675,0x64},
    {0x3676,0x66},
    {0x367a,0x48},
    {0x367b,0x78},
    {0x367c,0x58},
    {0x367d,0x78},
    {0x3690,0x34},
    {0x3691,0x34},
    {0x3692,0x54},
    {0x369c,0x48},
    {0x369d,0x78},
    {0x36ea,0x35},
    {0x36eb,0x0c},
    {0x36ec,0x0a},
    {0x36ed,0x34},
    {0x36fa,0xf5},
    {0x36fb,0x35},
    {0x36fc,0x10},
    {0x36fd,0x04},
    {0x3904,0x04},
    {0x3908,0x41},
    {0x391d,0x04},
    {0x39c2,0x30},
    {0x3e01,0xcd},
    {0x3e02,0xc0},
    {0x3e16,0x00},
    {0x3e17,0x80},
    {0x4500,0x88},
    {0x4509,0x20},
    {0x4800,0x04},
    {0x4837,0x14},
    {0x5799,0x00},
    {0x59e0,0x60},
    {0x59e1,0x08},
    {0x59e2,0x3f},
    {0x59e3,0x18},
    {0x59e4,0x18},
    {0x59e5,0x3f},
    {0x59e7,0x02},
    {0x59e8,0x38},
    {0x59e9,0x20},
    {0x59ea,0x0c},
    {0x59ec,0x08},
    {0x59ed,0x02},
    {0x59ee,0xa0},
    {0x59ef,0x08},
    {0x59f4,0x18},
    {0x59f5,0x10},
    {0x59f6,0x0c},
    {0x59f9,0x02},
    {0x59fa,0x18},
    {0x59fb,0x10},
    {0x59fc,0x0c},
    {0x59ff,0x02},
    {0x36e9,0x20},
    {0x36f9,0x57},
    {0x0100,0x01},

    {SC500AI_REG_DELAY, 0x10},
    {SC500AI_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sc500ai_init_regs_2560_1440_30fps_mipi[] = {
    {0x0103,0x01},
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x36f9,0x80},
    {0x3018,0x32},
    {0x3019,0x0c},
    {0x301f,0x55},
    {0x3200,0x00},
    {0x3201,0xa0},
    {0x3202,0x00},
    {0x3203,0x5a},
    {0x3204,0x0a},
    {0x3205,0xa7},
    {0x3206,0x06},
    {0x3207,0x01},
    {0x3208,0x0a},
    {0x3209,0x00},
    {0x320a,0x05},
    {0x320b,0xa0},
    {0x320c,0x05},
    {0x320d,0x78},
    {0x320e,0x07},
    {0x320f,0x08},
    {0x3210,0x00},
    {0x3211,0x04},
    {0x3212,0x00},
    {0x3213,0x04},
    {0x3250,0x40},
    {0x3253,0x0a},
    {0x3301,0x0a},
    {0x3302,0x18},
    {0x3303,0x10},
    {0x3304,0x50},
    {0x3306,0x60},
    {0x3308,0x10},
    {0x3309,0x60},
    {0x330a,0x00},
    {0x330b,0xf0},
    {0x330d,0x18},
    {0x330e,0x20},
    {0x330f,0x02},
    {0x3310,0x02},
    {0x331c,0x04},
    {0x331e,0x41},
    {0x331f,0x51},
    {0x3320,0x09},
    {0x3333,0x10},
    {0x334c,0x08},
    {0x3356,0x09},
    {0x3364,0x17},
    {0x336d,0x03},
    {0x3390,0x08},
    {0x3391,0x18},
    {0x3392,0x38},
    {0x3393,0x0a},
    {0x3394,0x20},
    {0x3395,0x20},
    {0x3396,0x08},
    {0x3397,0x18},
    {0x3398,0x38},
    {0x3399,0x0a},
    {0x339a,0x20},
    {0x339b,0x20},
    {0x339c,0x20},
    {0x33ac,0x10},
    {0x33ae,0x10},
    {0x33af,0x19},
    {0x360f,0x01},
    {0x3622,0x03},
    {0x363a,0x1f},
    {0x363c,0x40},
    {0x3651,0x7d},
    {0x3670,0x0a},
    {0x3671,0x07},
    {0x3672,0x17},
    {0x3673,0x1e},
    {0x3674,0x82},
    {0x3675,0x64},
    {0x3676,0x66},
    {0x367a,0x48},
    {0x367b,0x78},
    {0x367c,0x58},
    {0x367d,0x78},
    {0x3690,0x34},
    {0x3691,0x34},
    {0x3692,0x54},
    {0x369c,0x48},
    {0x369d,0x78},
    {0x36ea,0x39},
    {0x36eb,0x0c},
    {0x36ec,0x0a},
    {0x36ed,0x24},
    {0x36fa,0x79},
    {0x36fb,0x35},
    {0x36fc,0x10},
    {0x36fd,0x24},
    {0x3904,0x04},
    {0x3908,0x41},
    {0x391d,0x04},
    {0x39c2,0x30},
    {0x3e01,0xbb},
    {0x3e02,0xb0},
    {0x3e16,0x00},
    {0x3e17,0x80},
    {0x4500,0x88},
    {0x4509,0x20},
    {0x4837,0x19},
    {0x5799,0x00},
    {0x59e0,0x60},
    {0x59e1,0x08},
    {0x59e2,0x3f},
    {0x59e3,0x18},
    {0x59e4,0x18},
    {0x59e5,0x3f},
    {0x59e7,0x02},
    {0x59e8,0x38},
    {0x59e9,0x20},
    {0x59ea,0x0c},
    {0x59ec,0x08},
    {0x59ed,0x02},
    {0x59ee,0xa0},
    {0x59ef,0x08},
    {0x59f4,0x18},
    {0x59f5,0x10},
    {0x59f6,0x0c},
    {0x59f9,0x02},
    {0x59fa,0x18},
    {0x59fb,0x10},
    {0x59fc,0x0c},
    {0x59ff,0x02},
    {0x36e9,0x53},
    {0x36f9,0x53},
    {0x0100,0x01},
    {SC500AI_REG_END, 0x00},/* END MARKER */
};

static struct tx_isp_sensor_win_setting sc500ai_win_sizes[] = {
	/* 2592*1620@ Max30fps*/
	{
		.width		= 2592,
		.height		= 1620,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc500ai_init_regs_2592_1620_30fps_mipi,
	},
	/* 2560*1440@ Max25fps */
	{
		.width		= 2560,
		.height		= 1440,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc500ai_init_regs_2560_1440_30fps_mipi,
	}
};
struct tx_isp_sensor_win_setting *wsize = &sc500ai_win_sizes[0];

static struct regval_list sc500ai_stream_on[] = {
	{0x0100, 0x01},
	{SC500AI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc500ai_stream_off[] = {
	{0x0100, 0x00},
	{SC500AI_REG_END, 0x00},	/* END MARKER */
};

int sc500ai_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int sc500ai_write(struct tx_isp_subdev *sd, uint16_t reg,
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

static int sc500ai_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC500AI_REG_END) {
		if (vals->reg_num == SC500AI_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc500ai_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc500ai_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC500AI_REG_END) {
		if (vals->reg_num == SC500AI_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc500ai_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc500ai_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sc500ai_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sc500ai_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC500AI_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc500ai_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC500AI_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc500ai_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff) * 2;
	int again = (value & 0xffff0000) >> 16;

	/*set integration time*/
	ret = sc500ai_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0x0f));
	ret += sc500ai_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc500ai_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
	/*set analog gain*/
	ret += sc500ai_write(sd, 0x3e09, (unsigned char)(again & 0xff));
	ret += sc500ai_write(sd, 0x3e08, (unsigned char)(((again >> 8) & 0xff)));

	if ((again >= 0x3f7f) && dpc_flag) {//24x
		ret += sc500ai_write(sd, 0x5799, 0x07);
		dpc_flag = false;
	} else if ((again <= 0x3f69) && (!dpc_flag)) {//20x
		ret += sc500ai_write(sd, 0x5799, 0x00);
		dpc_flag = true;
	}
	if (ret != 0) {
		ISP_ERROR("err: sc500ai write err %d\n",__LINE__);
		return ret;
	}
	return 0;
}

#if 0
static int sc500ai_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
	ret = sc500ai_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0x0f));
	ret += sc500ai_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc500ai_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));
	if (ret < 0)
		return ret;

	return 0;
}

static int sc500ai_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int again = value;

	ret = sc500ai_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sc500ai_write(sd, 0x3e08, (unsigned char)((value >> 8 & 0xff)));
	if ((again >= 0x3f7f) && dpc_flag) {//24x
		ret += sc500ai_write(sd, 0x5799, 0x07);
		dpc_flag = false;
	} else if ((again <= 0x3f69) && (!dpc_flag)) {//20x
		ret += sc500ai_write(sd, 0x5799, 0x00);
		dpc_flag = true;
	}
	if (ret != 0)
		return ret;

	return 0;
}
#endif

static int sc500ai_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc500ai_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc500ai_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val;

	if(!enable)
		return ISP_SUCCESS;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = sc500ai_write_array(sd, wsize->regs);
	if (ret != 0)
		return ret;
	/*verion diff*/
	ret = sc500ai_read(sd, 0x3109, &val);
	if(1 == val) {
		ret += sc500ai_write(sd, 0x336d, 0x23);
	} else {
		ret += sc500ai_write(sd, 0x336d, 0x03);
	}
	ret += sc500ai_read(sd, 0x3004, &val);
	if(1 == val) {
		ret += sc500ai_write(sd, 0x363c, 0x42);
	} else {
		ret += sc500ai_write(sd, 0x363c, 0x40);
	}
	if (ret != 0)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc500ai_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = sc500ai_write_array(sd, sc500ai_stream_on);
	} else {
		ret = sc500ai_write_array(sd, sc500ai_stream_off);
	}

	return ret;
}

static int sc500ai_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	switch(sensor_resolution){
	case SENSOR_RES_400:
		sclk = SC500AI_SUPPORT_SCLK_4M_FPS_30;
		break;
	case SENSOR_RES_370:
		sclk = SC500AI_SUPPORT_SCLK_3M_FPS_30;
		break;
	}
	ret = sc500ai_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sc500ai_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("err: sc500ai read err\n");
		return ret;
	}
	hts = ((hts << 8) + tmp) << 1;
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += sc500ai_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc500ai_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: sc500ai_write err\n");
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

static int sc500ai_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = -1;
	unsigned char val = 0x0;

	ret = sc500ai_read(sd, 0x3221, &val);
	if(enable & 0x2)
		val |= 0x60;
	else
		val &= 0x9f;
	ret += sc500ai_write(sd, 0x3221, val);
	if (0 != ret) {
		ISP_ERROR("err: sc500ai_write err\n");
		return ret;
	}

	return ret;
}

static int sc500ai_set_mode(struct tx_isp_subdev *sd, int value)
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

static int sc500ai_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc500ai_reset");
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
		ret = private_gpio_request(pwdn_gpio,"sc500ai_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc500ai_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc500ai chip.\n",
		       client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("sc500ai chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc500ai", sizeof("sc500ai"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc500ai_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = sc500ai_set_expo(sd, *(int*)arg);
		break;
	/*
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = sc500ai_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = sc500ai_set_analog_gain(sd, *(int*)arg);
		break;
	*/
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc500ai_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc500ai_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc500ai_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = sc500ai_write_array(sd, sc500ai_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sc500ai_write_array(sd, sc500ai_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc500ai_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = sc500ai_set_vflip(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int sc500ai_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc500ai_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc500ai_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc500ai_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc500ai_core_ops = {
	.g_chip_ident = sc500ai_g_chip_ident,
	.reset = sc500ai_reset,
	.init = sc500ai_init,
	.g_register = sc500ai_g_register,
	.s_register = sc500ai_s_register,
};

static struct tx_isp_subdev_video_ops sc500ai_video_ops = {
	.s_stream = sc500ai_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc500ai_sensor_ops = {
	.ioctl	= sc500ai_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc500ai_ops = {
	.core = &sc500ai_core_ops,
	.video = &sc500ai_video_ops,
	.sensor = &sc500ai_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc500ai",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};


static int sc500ai_probe(struct i2c_client *client,
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

	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	switch(sensor_resolution){
	case SENSOR_RES_400:
		wsize = &sc500ai_win_sizes[0];
		break;
	case SENSOR_RES_370:
		wsize = &sc500ai_win_sizes[1];
		sc500ai_attr.mipi.image_twidth = 2560;
		sc500ai_attr.mipi.image_theight = 1440;
		sc500ai_attr.max_integration_time_native = 1800-4;
		sc500ai_attr.integration_time_limit = 1800-4;
		sc500ai_attr.total_width = 2800;
		sc500ai_attr.total_height = 1800;
		sc500ai_attr.max_integration_time = 1800-4;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate !!!\n");
	}
	/*
	  convert sensor-gain into isp-gain,
	*/
	sc500ai_attr.max_again = 300739;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sc500ai_attr.expo_fs = 1;
	sensor->video.attr = &sc500ai_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc500ai_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc500ai\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int sc500ai_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc500ai_id[] = {
	{ "sc500ai", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc500ai_id);

static struct i2c_driver sc500ai_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc500ai",
	},
	.probe		= sc500ai_probe,
	.remove		= sc500ai_remove,
	.id_table	= sc500ai_id,
};

static __init int init_sc500ai(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init sc500ai dirver.\n");
		return -1;
	}

	return private_i2c_add_driver(&sc500ai_driver);
}

static __exit void exit_sc500ai(void)
{
	private_i2c_del_driver(&sc500ai_driver);
}

module_init(init_sc500ai);
module_exit(exit_sc500ai);

MODULE_DESCRIPTION("A low-level driver for Smartsens sc500ai sensors");
MODULE_LICENSE("GPL");
