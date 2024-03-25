// SPDX-License-Identifier: GPL-2.0+
/*
 * sc2310.c
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
#include <sensor-info.h>

#define SENSOR_NAME "sc2310"
#define SENSOR_CHIP_ID_H (0x23)
#define SENSOR_CHIP_ID_L (0x11)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK (81000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20170911a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_12BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

static int data_interface = TX_SENSOR_DATA_INTERFACE_DVP;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

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
	{0x361, 39338},
	{0x362, 40269},
	{0x363, 41252},
	{0x364, 42225},
	{0x365, 43128},
	{0x366, 44082},
	{0x367, 44967},
	{0x368, 45903},
	{0x369, 46829},
	{0x36a, 47689},
	{0x36b, 48599},
	{0x36c, 49499},
	{0x36d, 50336},
	{0x36e, 51220},
	{0x36f, 52041},
	{0x370, 52910},
	{0x371, 53770},
	{0x372, 54570},
	{0x373, 55415},
	{0x374, 56253},
	{0x375, 57032},
	{0x376, 57856},
	{0x377, 58622},
	{0x378, 59433},
	{0x379, 60236},
	{0x37a, 60983},
	{0x37b, 61773},
	{0x37c, 62557},
	{0x37d, 63286},
	{0x37e, 64058},
	{0x37f, 64775},
	{0x740, 65535},
	{0x741, 66989},
	{0x742, 68467},
	{0x743, 69877},
	{0x744, 71266},
	{0x745, 72636},
	{0x746, 74029},
	{0x747, 75359},
	{0x748, 76671},
	{0x749, 77964},
	{0x74a, 79281},
	{0x74b, 80540},
	{0x74c, 81782},
	{0x74d, 83009},
	{0x74e, 84258},
	{0x74f, 85452},
	{0x750, 86632},
	{0x751, 87797},
	{0x752, 88985},
	{0x753, 90122},
	{0x754, 91245},
	{0x755, 92355},
	{0x756, 93487},
	{0x2340, 94606},
	{0x2341, 96089},
	{0x2342, 97516},
	{0x2343, 98954},
	{0x2344, 100338},
	{0x2345, 101735},
	{0x2346, 103079},
	{0x2347, 104436},
	{0x2348, 105742},
	{0x2349, 107062},
	{0x234a, 108333},
	{0x234b, 109617},
	{0x234c, 110854},
	{0x234d, 112105},
	{0x234e, 113310},
	{0x234f, 114529},
	{0x2350, 115704},
	{0x2351, 116892},
	{0x2352, 118038},
	{0x2353, 119198},
	{0x2354, 120317},
	{0x2355, 121449},
	{0x2356, 122542},
	{0x2357, 123647},
	{0x2358, 124715},
	{0x2359, 125796},
	{0x235a, 126840},
	{0x235b, 127897},
	{0x235c, 128918},
	{0x235d, 129952},
	{0x235e, 130951},
	{0x235f, 131963},
	{0x2360, 132942},
	{0x2361, 133933},
	{0x2362, 134891},
	{0x2363, 135862},
	{0x2364, 136801},
	{0x2365, 137753},
	{0x2366, 138674},
	{0x2367, 139607},
	{0x2368, 140510},
	{0x2369, 141425},
	{0x236a, 142311},
	{0x236b, 143209},
	{0x236c, 144078},
	{0x236d, 144959},
	{0x236e, 145813},
	{0x236f, 146678},
	{0x2370, 147516},
	{0x2371, 148367},
	{0x2372, 149190},
	{0x2373, 150025},
	{0x2374, 150834},
	{0x2375, 151655},
	{0x2376, 152450},
	{0x2377, 153257},
	{0x2378, 154039},
	{0x2379, 154833},
	{0x237a, 155602},
	{0x237b, 156383},
	{0x237c, 157140},
	{0x237d, 157908},
	{0x237e, 158652},
	{0x237f, 159408},
	{0x2740, 160141},
	{0x2741, 161607},
	{0x2742, 163051},
	{0x2743, 164472},
	{0x2744, 165873},
	{0x2745, 167253},
	{0x2746, 168614},
	{0x2747, 169955},
	{0x2748, 171277},
	{0x2749, 172581},
	{0x274a, 173868},
	{0x274b, 175137},
	{0x274c, 176389},
	{0x274d, 177625},
	{0x274e, 178845},
	{0x274f, 180050},
	{0x2750, 181239},
	{0x2751, 182413},
	{0x2752, 183573},
	{0x2753, 184719},
	{0x2754, 185852},
	{0x2755, 186971},
	{0x2756, 188077},
	{0x2757, 189170},
	{0x2758, 190250},
	{0x2759, 191318},
	{0x275a, 192375},
	{0x275b, 193420},
	{0x275c, 194453},
	{0x275d, 195475},
	{0x275e, 196486},
	{0x275f, 197487},
	{0x2760, 198477},
	{0x2761, 199457},
	{0x2762, 200426},
	{0x2763, 201386},
	{0x2764, 202336},
	{0x2765, 203277},
	{0x2766, 204209},
	{0x2767, 205131},
	{0x2768, 206045},
	{0x2769, 206949},
	{0x276a, 207846},
	{0x276b, 208733},
	{0x276c, 209613},
	{0x276d, 210484},
	{0x276e, 211348},
	{0x276f, 212203},
	{0x2770, 213051},
	{0x2771, 213892},
	{0x2772, 214725},
	{0x2773, 215550},
	{0x2774, 216369},
	{0x2775, 217181},
	{0x2776, 217985},
	{0x2777, 218783},
	{0x2778, 219574},
	{0x2779, 220359},
	{0x277a, 221137},
	{0x277b, 221909},
	{0x277c, 222675},
	{0x277d, 223434},
	{0x277e, 224187},
	{0x277f, 224935},
	{0x2f40, 225676},
	{0x2f41, 227142},
	{0x2f42, 228586},
	{0x2f43, 230007},
	{0x2f44, 231408},
	{0x2f45, 232788},
	{0x2f46, 234149},
	{0x2f47, 235490},
	{0x2f48, 236812},
	{0x2f49, 238116},
	{0x2f4a, 239403},
	{0x2f4b, 240672},
	{0x2f4c, 241924},
	{0x2f4d, 243160},
	{0x2f4e, 244380},
	{0x2f4f, 245585},
	{0x2f50, 246774},
	{0x2f51, 247948},
	{0x2f52, 249108},
	{0x2f53, 250254},
	{0x2f54, 251387},
	{0x2f55, 252506},
	{0x2f56, 253612},
	{0x2f57, 254705},
	{0x2f58, 255785},
	{0x2f59, 256853},
	{0x2f5a, 257910},
	{0x2f5b, 258955},
	{0x2f5c, 259988},
	{0x2f5d, 261010},
	{0x2f5e, 262021},
	{0x2f5f, 263022},
	{0x2f60, 264012},
	{0x2f61, 264992},
	{0x2f62, 265961},
	{0x2f63, 266921},
	{0x2f64, 267871},
	{0x2f65, 268812},
	{0x2f66, 269744},
	{0x2f67, 270666},
	{0x2f68, 271580},
	{0x2f69, 272484},
	{0x2f6a, 273381},
	{0x2f6b, 274268},
	{0x2f6c, 275148},
	{0x2f6d, 276019},
	{0x2f6e, 276883},
	{0x2f6f, 277738},
	{0x2f70, 278586},
	{0x2f71, 279427},
	{0x2f72, 280260},
	{0x2f73, 281085},
	{0x2f74, 281904},
	{0x2f75, 282716},
	{0x2f76, 283520},
	{0x2f77, 284318},
	{0x2f78, 285109},
	{0x2f79, 285894},
	{0x2f7a, 286672},
	{0x2f7b, 287444},
	{0x2f7c, 288210},
	{0x2f7d, 288969},
	{0x2f7e, 289722},
	{0x2f7f, 290470},
	{0x3f40, 291211},
	{0x3f41, 292677},
	{0x3f42, 294121},
	{0x3f43, 295542},
	{0x3f44, 296943},
	{0x3f45, 298323},
	{0x3f46, 299684},
	{0x3f47, 301025},
	{0x3f48, 302347},
	{0x3f49, 303651},
	{0x3f4a, 304938},
	{0x3f4b, 306207},
	{0x3f4c, 307459},
	{0x3f4d, 308695},
	{0x3f4e, 309915},
	{0x3f4f, 311120},
	{0x3f50, 312309},
	{0x3f51, 313483},
	{0x3f52, 314643},
	{0x3f53, 315789},
	{0x3f54, 316922},
	{0x3f55, 318041},
	{0x3f56, 319147},
	{0x3f57, 320240},
	{0x3f58, 321320},
	{0x3f59, 322388},
	{0x3f5a, 323445},
	{0x3f5b, 324490},
	{0x3f5c, 325523},
	{0x3f5d, 326545},
	{0x3f5e, 327556},
	{0x3f5f, 328557},
	{0x3f60, 329547},
	{0x3f61, 330527},
	{0x3f62, 331496},
	{0x3f63, 332456},
	{0x3f64, 333406},
	{0x3f65, 334347},
	{0x3f66, 335279},
	{0x3f67, 336201},
	{0x3f68, 337115},
	{0x3f69, 338019},
	{0x3f6a, 338916},
	{0x3f6b, 339803},
	{0x3f6c, 340683},
	{0x3f6d, 341554},
	{0x3f6e, 342418},
	{0x3f6f, 343273},
	{0x3f70, 344121},
	{0x3f71, 344962},
	{0x3f72, 345795},
	{0x3f73, 346620},
	{0x3f74, 347439},
	{0x3f75, 348251},
	{0x3f76, 349055},
	{0x3f77, 349853},
	{0x3f78, 350644},
	{0x3f79, 351429},
	{0x3f7a, 352207},
	{0x3f7b, 352979},
	{0x3f7c, 353745},
	{0x3f7d, 354504},
	{0x3f7e, 355257},
	{0x3f7f, 356005},
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
	return isp_gain;
}

struct tx_isp_mipi_bus sensor_mipi={
	.clk = 600,
	.lans = 2,
};
struct tx_isp_dvp_bus sensor_dvp={
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking = {
		.vblanking = 0,
		.hblanking = 0,
	},
};

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x2311,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
	},
	.max_again = 356005,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 1437,
	.integration_time_limit = 1437,
	.total_width = 2250,
	.total_height = 1440,
	.max_integration_time = 1437,
	.one_line_expr_in_us = 28,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	// void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list sensor_init_regs_1920_1080_25fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0xa3},//bypass pll1
	{0x36f9, 0x85},//bypass pll2
	{0x337f, 0x03},
	{0x3368, 0x04},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x3367, 0x08},
	{0x330e, 0x30},
	{0x3326, 0x00},
	{0x3631, 0x88},
	{0x3018, 0x33},
	{0x3031, 0x0c},
	{0x3001, 0xfe},
	{0x4603, 0x00},
	{0x303f, 0x01},
	{0x3640, 0x00},
	{0x3636, 0x65},
	{0x3907, 0x01},
	{0x3908, 0x01},
	{0x3320, 0x01},
	{0x3366, 0x80},
	{0x57a4, 0xf0},
	{0x3333, 0x30},
	{0x331b, 0x83},
	{0x3334, 0x40},
	{0x3302, 0x10},
	{0x3308, 0x08},
	{0x3622, 0xe6},
	{0x3633, 0x22},
	{0x3630, 0xc8},
	{0x3301, 0x10},
	{0x33aa, 0x00},
	{0x391e, 0x00},
	{0x391f, 0xc0},
	{0x3634, 0x44},
	{0x4500, 0x59},
	{0x3623, 0x18},
	{0x3f08, 0x04},
	{0x3f00, 0x0d},
	{0x336c, 0x42},
	{0x3933, 0x28},
	{0x3934, 0x0a},
	{0x3940, 0x1b},
	{0x3941, 0x40},
	{0x3942, 0x08},
	{0x3943, 0x0e},
	{0x36ea, 0x77},
	{0x36eb, 0x0b},
	{0x36ec, 0x0f},
	{0x36ed, 0x03},
	{0x36fb, 0x10},
	{0x320c, 0x04},
	{0x320d, 0x65},
	{0x320e, 0x05},
	{0x320f, 0xa0},
	{0x3235, 0x09},
	{0x3236, 0x5e},
	{0x3f04, 0x02},
	{0x3f05, 0x2a},
	{0x3802, 0x00},
	{0x3624, 0x47},
	{0x3621, 0xac},
	{0x3638, 0x25},
	{0x3635, 0x40},
	{0x363b, 0x08},
	{0x363c, 0x05},
	{0x363d, 0x05},
	{0x3324, 0x02},
	{0x3325, 0x02},
	{0x333d, 0x08},
	{0x3314, 0x04},
	{0x36fa, 0x28},
	{0x3e14, 0xb0},
	{0x3e1e, 0x35},
	{0x3e0e, 0x66},
	{0x4837, 0x31},
	{0x6000, 0x00},
	{0x6002, 0x00},
	{0x301c, 0x78},
	{0x3037, 0x44},
	{0x3038, 0x44},
	{0x3632, 0x18},
	{0x4809, 0x01},
	{0x5000, 0x06},
	{0x5780, 0x7f},
	{0x57a0, 0x00},
	{0x57a1, 0x74},
	{0x57a2, 0x01},
	{0x57a3, 0xf4},
	{0x5781, 0x06},
	{0x5782, 0x04},
	{0x5783, 0x02},
	{0x5784, 0x01},
	{0x5785, 0x16},
	{0x5786, 0x12},
	{0x5787, 0x08},
	{0x5788, 0x02},
	{0x4501, 0xb4},
	{0x4509, 0x20},
	{0x3364, 0x1d},
	{0x33b6, 0x07},
	{0x33b7, 0x07},
	{0x33b8, 0x10},
	{0x33b9, 0x10},
	{0x33ba, 0x10},
	{0x33bb, 0x07},
	{0x33bc, 0x07},
	{0x33bd, 0x20},
	{0x33be, 0x20},
	{0x33bf, 0x20},
	{0x360f, 0x05},
	{0x367a, 0x40},
	{0x367b, 0x40},
	{0x3671, 0xf6},
	{0x3672, 0x16},
	{0x3673, 0x16},
	{0x366e, 0x04},
	{0x3670, 0x4a},
	{0x367c, 0x40},
	{0x367d, 0x58},
	{0x3674, 0xc8},
	{0x3675, 0x54},
	{0x3676, 0x18},
	{0x367e, 0x40},
	{0x367f, 0x58},
	{0x3677, 0x22},
	{0x3678, 0x33},
	{0x3679, 0x44},
	{0x36a0, 0x58},
	{0x36a1, 0x78},
	{0x3696, 0x83},
	{0x3697, 0x87},
	{0x3698, 0x9f},
	{0x3637, 0x17},
	{0x331e, 0x11},
	{0x331f, 0x21},
	{0x3303, 0x1c},
	{0x3309, 0x3c},
	{0x330a, 0x00},
	{0x330b, 0xc8},
	{0x3306, 0x68},
	{0x3200, 0x00},
	{0x3201, 0x04},
	{0x3202, 0x00},
	{0x3203, 0x04},
	{0x3204, 0x07},
	{0x3205, 0x8b},
	{0x3206, 0x04},
	{0x3207, 0x43},
	{0x3208, 0x07},
	{0x3209, 0x80},
	{0x320a, 0x04},
	{0x320b, 0x38},
	{0x3211, 0x04},
	{0x3213, 0x04},
	{0x3380, 0x1b},
	{0x3341, 0x07},
	{0x3343, 0x03},
	{0x3e25, 0x03},
	{0x3e26, 0x40},
	{0x3e00, 0x00},
	{0x3e01, 0x95},
	{0x3e02, 0xa0},
	{0x3e03, 0x0b},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x36e9, 0x23},
	{0x36f9, 0x05},
	{0x0100, 0x01},
	{SENSOR_REG_DELAY, 10},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_25fps_dvp[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0xa3},//bypass pll1
	{0x36f9, 0x85},//bypass pll2
	{0x337f, 0x03},
	{0x3368, 0x04},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x3367, 0x08},
	{0x330e, 0x30},
	{0x3326, 0x00},
	{0x3631, 0x88},
	{0x3640, 0x00},
	{0x3636, 0x65},
	{0x3907, 0x01},
	{0x3908, 0x01},
	{0x3320, 0x01},
	{0x3366, 0x80},
	{0x57a4, 0xf0},
	{0x3333, 0x30},
	{0x331b, 0x83},
	{0x3334, 0x40},
	{0x3302, 0x10},
	{0x3308, 0x08},
	{0x3622, 0xe6},
	{0x3633, 0x22},
	{0x3630, 0xc8},
	{0x3301, 0x10},
	{0x33aa, 0x00},
	{0x391e, 0x00},
	{0x391f, 0xc0},
	{0x3634, 0x44},
	{0x4500, 0x59},
	{0x3623, 0x18},
	{0x3f08, 0x04},
	{0x3f00, 0x0d},
	{0x336c, 0x42},
	{0x3933, 0x28},
	{0x3934, 0x0a},
	{0x3940, 0x1b},
	{0x3941, 0x40},
	{0x3942, 0x08},
	{0x3943, 0x0e},
	{0x36ea, 0x77},
	{0x36eb, 0x0b},
	{0x36ec, 0x0f},
	{0x36ed, 0x03},
	{0x36fb, 0x10},
	{0x320c, 0x04},
	{0x320d, 0x65},
	{0x320e, 0x05},
	{0x320f, 0xa0},
	{0x3235, 0x09},
	{0x3236, 0x5e},
	{0x3f04, 0x02},
	{0x3f05, 0x2a},
	{0x3802, 0x00},
	{0x3624, 0x47},
	{0x3621, 0xac},
	{0x3019, 0xff},
	{0x301c, 0xb4},
	{0x3018, 0x1f},
	{0x3031, 0x0a},
	{0x3001, 0xff},
	{0x4603, 0x01},
	{0x4837, 0x36},
	{0x303f, 0x81},
	{0x3d08, 0x01},
	{0x3638, 0x25},
	{0x3635, 0x40},
	{0x363b, 0x08},
	{0x363c, 0x05},
	{0x363d, 0x05},
	{0x3641, 0x01},
	{0x3324, 0x02},
	{0x3325, 0x02},
	{0x333d, 0x08},
	{0x3314, 0x04},
	{0x36fa, 0x28},
	{0x3e14, 0xb0},
	{0x3e1e, 0x35},
	{0x3e0e, 0x66},
	{0x6000, 0x00},
	{0x6002, 0x00},
	{0x3037, 0x24},
	{0x3038, 0x44},
	{0x3632, 0x18},
	{0x4809, 0x01},
	{0x5000, 0x06},
	{0x5780, 0x7f},
	{0x57a0, 0x00},
	{0x57a1, 0x74},
	{0x57a2, 0x01},
	{0x57a3, 0xf4},
	{0x5781, 0x06},
	{0x5782, 0x04},
	{0x5783, 0x02},
	{0x5784, 0x01},
	{0x5785, 0x16},
	{0x5786, 0x12},
	{0x5787, 0x08},
	{0x5788, 0x02},
	{0x4501, 0xb4},
	{0x4509, 0x20},
	{0x3364, 0x1d},
	{0x33b6, 0x07},
	{0x33b7, 0x07},
	{0x33b8, 0x10},
	{0x33b9, 0x10},
	{0x33ba, 0x10},
	{0x33bb, 0x07},
	{0x33bc, 0x07},
	{0x33bd, 0x20},
	{0x33be, 0x20},
	{0x33bf, 0x20},
	{0x360f, 0x05},
	{0x367a, 0x40},
	{0x367b, 0x40},
	{0x3671, 0xf6},
	{0x3672, 0x16},
	{0x3673, 0x16},
	{0x366e, 0x04},
	{0x3670, 0x4a},
	{0x367c, 0x40},
	{0x367d, 0x58},
	{0x3674, 0xc8},
	{0x3675, 0x54},
	{0x3676, 0x18},
	{0x367e, 0x40},
	{0x367f, 0x58},
	{0x3677, 0x22},
	{0x3678, 0x33},
	{0x3679, 0x44},
	{0x36a0, 0x58},
	{0x36a1, 0x78},
	{0x3696, 0x83},
	{0x3697, 0x87},
	{0x3698, 0x9f},
	{0x3637, 0x17},
	{0x331e, 0x11},
	{0x331f, 0x21},
	{0x3303, 0x1c},
	{0x3309, 0x3c},
	{0x330a, 0x00},
	{0x330b, 0xc8},
	{0x3306, 0x68},
	{0x3200, 0x00},
	{0x3201, 0x04},
	{0x3202, 0x00},
	{0x3203, 0x04},
	{0x3204, 0x07},
	{0x3205, 0x8b},
	{0x3206, 0x04},
	{0x3207, 0x43},
	{0x3208, 0x07},
	{0x3209, 0x80},
	{0x320a, 0x04},
	{0x320b, 0x38},
	{0x3211, 0x04},
	{0x3213, 0x04},
	{0x3380, 0x1b},
	{0x3341, 0x07},
	{0x3343, 0x03},
	{0x3e25, 0x03},
	{0x3e26, 0x40},
	{0x3e00, 0x00},
	{0x3e01, 0x95},
	{0x3e02, 0xa0},
	{0x3e03, 0x0b},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x36e9, 0x23},
	{0x36f9, 0x05},
	{0x0100, 0x01},
	{SENSOR_REG_DELAY, 10},
	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1920*1080 */
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR12_1X12,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_25fps_dvp,
	}
};

static enum v4l2_mbus_pixelcode sensor_mbus_code[] = {
	V4L2_MBUS_FMT_SBGGR10_1X10,
	V4L2_MBUS_FMT_SBGGR12_1X12,
};

/*
 * the part of driver was fixed.
 */

static struct regval_list sensor_stream_on_dvp[] = {
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_dvp[] = {
	{0x0100, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_on_mipi[] = {
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,	unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
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

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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
		vals++;
	}

	return 0;
}
static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
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

static int sensor_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sensor_read(sd, 0x3107, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x3108, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
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
	unsigned int expo = 0;
	expo = value*2;
	ret += sensor_write(sd, 0x3e00, (unsigned char)((expo >> 12) & 0x0f));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((expo >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((expo & 0x0f) << 4));
	if (value < 0x50) {
		ret += sensor_write(sd, 0x3812, 0x00);
		ret += sensor_write(sd, 0x3314, 0x14);
		ret += sensor_write(sd, 0x3812, 0x30);
	}
	else if (value > 0xa0) {
		ret += sensor_write(sd, 0x3812, 0x00);
		ret += sensor_write(sd, 0x3314, 0x04);
		ret += sensor_write(sd, 0x3812, 0x30);
	}
	if (ret < 0)
		return ret;
	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret = sensor_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3e08, (unsigned char)(value >> 8 & 0x3f));
//	ret += sensor_write(sd, 0x3e08, (unsigned char)((value >> 8 << 2) | 0x03));
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

static int sensor_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
	int ret = 0;

	if (!enable)
		return ISP_SUCCESS;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = sensor_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);

		} else {
			printk("Don't support this Sensor Data interface\n");
		}
		pr_debug("%s stream on\n", SENSOR_NAME);

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);

		} else {
			printk("Don't support this Sensor Data interface\n");
		}
		pr_debug("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		printk("warn: fps(%d) not in range\n", fps);
		return -1;
	}
	sclk = SENSOR_SUPPORT_SCLK;

	ret = sensor_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0x320d, &tmp);
	if (ret < 0)
		return -1;
	hts = ((hts << 8) + tmp)*2;
	if (0 != ret) {
		printk("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = sensor_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x320e, (unsigned char)(vts >> 8));

	if (ret < 0)
		return -1;

	if (0 != ret) {
		printk("err: sensor_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native =vts - 3;
	sensor->video.attr->integration_time_limit =vts - 3;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time =vts - 3;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;

	if (value == TX_ISP_SENSOR_FULL_RES_MAX_FPS) {
		wsize = &sensor_win_sizes[0];
	} else if (value == TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS) {
		wsize = &sensor_win_sizes[0];
	}

	if (wsize) {
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

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val = 0;

	val = enable ? 0x60 : 0;
	ret += sensor_write(sd, 0x3221, val);
	sensor->video.mbus_change = 0;
	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
		struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		} else {
			printk("gpio requrest fail %d\n",reset_gpio);
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
			printk("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sensor_detect(sd, &ident);
	if (ret) {
		printk("chip found @ 0x%x (%s) is not an %s chip.\n",
		       client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}
	printk("%s chip found @ 0x%02x (%s)\n",
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

	if (IS_ERR_OR_NULL(sd)) {
		printk("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			if (arg)
				ret = sensor_set_integration_time(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			if (arg)
				ret = sensor_set_analog_gain(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_DGAIN:
			if (arg)
				ret = sensor_set_digital_gain(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
			if (arg)
				ret = sensor_get_black_pedestal(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if (arg)
				ret = sensor_set_mode(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
			if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
				ret = sensor_write_array(sd, sensor_stream_off_dvp);
			} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
				ret = sensor_write_array(sd, sensor_stream_off_mipi);

			} else {
				printk("Don't support this Sensor Data interface\n");
			}
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
				ret = sensor_write_array(sd, sensor_stream_on_dvp);
			} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
				ret = sensor_write_array(sd, sensor_stream_on_mipi);

			} else {
				printk("Don't support this Sensor Data interface\n");
				ret = -1;
			}
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if (arg)
				ret = sensor_set_fps(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_VFLIP:
			if (arg)
				ret = sensor_set_vflip(sd, *(int*)arg);
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

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
	enum v4l2_mbus_pixelcode mbus;
	int i = 0;
	int ret;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		printk("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		printk("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	sensor_attr.dbus_type = data_interface;
	if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
		ret = set_sensor_gpio_function(sensor_gpio_func);
		if (ret < 0)
			goto err_set_sensor_gpio;
		switch(sensor_gpio_func) {
		case DVP_PA_LOW_10BIT:
		case DVP_PA_HIGH_10BIT:
			mbus = sensor_mbus_code[0];
			break;
		case DVP_PA_12BIT:
			mbus = sensor_mbus_code[1];
			break;
		default:
			goto err_set_sensor_gpio;
		}
		for(i = 0; i < ARRAY_SIZE(sensor_win_sizes); i++)
			sensor_win_sizes[i].mbus_code = mbus;
		wsize->regs = sensor_init_regs_1920_1080_25fps_dvp;
		memcpy((void*)(&(sensor_attr.dvp)),(void*)(&sensor_dvp),sizeof(sensor_dvp));
		sensor_attr.dvp.gpio = sensor_gpio_func;
	} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
		wsize->regs = sensor_init_regs_1920_1080_25fps_mipi;
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi),sizeof(sensor_mipi));
	} else {
		printk("Don't support this Sensor Data Output Interface.\n");
		goto err_set_sensor_data_interface;
	}
	 /*
		convert sensor-gain into isp-gain,
	 */
	sensor_attr.max_again = 356005;
	sensor_attr.max_dgain = 0;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sensor_attr;
	sensor->video.mbus_change = 0;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

	return 0;
err_set_sensor_data_interface:
err_set_sensor_gpio:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int sensor_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
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
	int ret = 0;
	ret = private_driver_get_interface();
	if (ret) {
		printk("Failed to init %s driver.\n", SENSOR_NAME);
		return -1;
	}

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
