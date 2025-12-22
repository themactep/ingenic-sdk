// SPDX-License-Identifier: GPL-2.0+
/*
 * sc200ai.c
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
#include <tx-isp-common.h>
#include <sensor-common.h>
#include <sensor-info.h>

// ============================================================================
// SENSOR IDENTIFICATION
// ============================================================================
#define SENSOR_NAME "sc200ai"
#define SENSOR_VERSION "H20210622a"
#define SENSOR_CHIP_ID 0xcb1c
#define SENSOR_CHIP_ID_H (0xcb)
#define SENSOR_CHIP_ID_L (0x1c)

// ============================================================================
// HARDWARE INTERFACE
// ============================================================================
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x30

// ============================================================================
// SENSOR CAPABILITIES
// ============================================================================
#define SENSOR_MAX_WIDTH 1920
#define SENSOR_MAX_HEIGHT 1080

// ============================================================================
// REGISTER DEFINITIONS
// ============================================================================
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe

// ============================================================================
// TIMING AND PERFORMANCE
// ============================================================================
#define SENSOR_SUPPORT_PCLK_FPS_25 (74250*1000)
#define SENSOR_SUPPORT_PCLK_FPS_60 (148500*1000)
#define SENSOR_OUTPUT_MAX_FPS 60
#define SENSOR_OUTPUT_MIN_FPS 5

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_60;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static bool dpc_flag = true;

static struct sensor_info sensor_info = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.version = SENSOR_VERSION,
	.min_fps = SENSOR_OUTPUT_MIN_FPS,
	.max_fps = SENSOR_OUTPUT_MAX_FPS,
	.actual_fps = 0,
	.chip_i2c_addr = SENSOR_I2C_ADDRESS,
	.width = SENSOR_MAX_WIDTH,
	.height = SENSOR_MAX_HEIGHT,
};

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
	{0x757, 94571},
	{0x758, 95643},
	{0x759, 96703},
	{0x75a, 97785},
	{0x75b, 98821},
	{0x75c, 99846},
	{0x75d, 100860},
	{0x75e, 101896},
	{0x75f, 102888},
	{0x760, 103870},
	{0x761, 104842},
	{0x762, 105835},
	{0x763, 106787},
	{0x764, 107730},
	{0x765, 108663},
	{0x766, 109617},
	{0x767, 110532},
	{0x768, 111438},
	{0x769, 112335},
	{0x76a, 113253},
	{0x76b, 114134},
	{0x76c, 115006},
	{0x2340, 115704},
	{0x2341, 117166},
	{0x2342, 118606},
	{0x2343, 120025},
	{0x2344, 121449},
	{0x2345, 122826},
	{0x2346, 124183},
	{0x2347, 125521},
	{0x2348, 126840},
	{0x2349, 128141},
	{0x234a, 129424},
	{0x234b, 130691},
	{0x234c, 131963},
	{0x234d, 133196},
	{0x234e, 134413},
	{0x234f, 135615},
	{0x2350, 136801},
	{0x2351, 137973},
	{0x2352, 139131},
	{0x2353, 140274},
	{0x2354, 141425},
	{0x2355, 142541},
	{0x2356, 143644},
	{0x2357, 144735},
	{0x2358, 145813},
	{0x2359, 146879},
	{0x235a, 147932},
	{0x235b, 148975},
	{0x235c, 150025},
	{0x235d, 151045},
	{0x235e, 152054},
	{0x235f, 153052},
	{0x2360, 154039},
	{0x2361, 155017},
	{0x2362, 155984},
	{0x2363, 156942},
	{0x2364, 157908},
	{0x2365, 158846},
	{0x2366, 159776},
	{0x2367, 160696},
	{0x2368, 161607},
	{0x2369, 162510},
	{0x236a, 163404},
	{0x236b, 164290},
	{0x236c, 165184},
	{0x236d, 166053},
	{0x236e, 166914},
	{0x236f, 167768},
	{0x2370, 168614},
	{0x2371, 169452},
	{0x2372, 170283},
	{0x2373, 171107},
	{0x2374, 171939},
	{0x2375, 172749},
	{0x2376, 173552},
	{0x2377, 174348},
	{0x2378, 175137},
	{0x2379, 175920},
	{0x237a, 176696},
	{0x237b, 177466},
	{0x237c, 178244},
	{0x237d, 179002},
	{0x237e, 179753},
	{0x237f, 180499},
	{0x2740, 181239},
	{0x2741, 182701},
	{0x2742, 184155},
	{0x2743, 185573},
	{0x2744, 186971},
	{0x2745, 188348},
	{0x2746, 189718},
	{0x2747, 191056},
	{0x2748, 192375},
	{0x2749, 193676},
	{0x274a, 194971},
	{0x274b, 196237},
	{0x274c, 197487},
	{0x274d, 198720},
	{0x274e, 199948},
	{0x274f, 201150},
	{0x2750, 202336},
	{0x2751, 203508},
	{0x2752, 204676},
	{0x2753, 205820},
	{0x2754, 206949},
	{0x2755, 208066},
	{0x2756, 209179},
	{0x2757, 210270},
	{0x2758, 211348},
	{0x2759, 212414},
	{0x275a, 213477},
	{0x275b, 214520},
	{0x275c, 215550},
	{0x275d, 216570},
	{0x275e, 217589},
	{0x275f, 218549},
	{0x2760, 219574},
	{0x2761, 220497},
	{0x2762, 221501},
	{0x2763, 222405},
	{0x2764, 223389},
	{0x2765, 224364},
	{0x2766, 225241},
	{0x2767, 226196},
	{0x2768, 227142},
	{0x2769, 227994},
	{0x276a, 228922},
	{0x276b, 229758},
	{0x276c, 230669},
	{0x276d, 231572},
	{0x276e, 232385},
	{0x276f, 233271},
	{0x2770, 234149},
	{0x2771, 234940},
	{0x2772, 235803},
	{0x2773, 236580},
	{0x2774, 237428},
	{0x2775, 238269},
	{0x2776, 239026},
	{0x2777, 239853},
	{0x2778, 240672},
	{0x2779, 241411},
	{0x277a, 242216},
	{0x277b, 242943},
	{0x277c, 243736},
	{0x277d, 244523},
	{0x277e, 245232},
	{0x277f, 246006},
	{0x2f40, 246774},
	{0x2f41, 248223},
	{0x2f42, 249649},
	{0x2f43, 251055},
	{0x2f44, 252506},
	{0x2f45, 253870},
	{0x2f46, 255215},
	{0x2f47, 256540},
	{0x2f48, 257910},
	{0x2f49, 259199},
	{0x2f4a, 260470},
	{0x2f4b, 261725},
	{0x2f4c, 263022},
	{0x2f4d, 264243},
	{0x2f4e, 265449},
	{0x2f4f, 266640},
	{0x2f50, 267871},
	{0x2f51, 269032},
	{0x2f52, 270179},
	{0x2f53, 271312},
	{0x2f54, 272484},
	{0x2f55, 273590},
	{0x2f56, 274683},
	{0x2f57, 275764},
	{0x2f58, 276883},
	{0x2f59, 277939},
	{0x2f5a, 278983},
	{0x2f5b, 280015},
	{0x2f5c, 281085},
	{0x2f5d, 282096},
	{0x2f5e, 283095},
	{0x2f5f, 284084},
	{0x2f60, 285109},
	{0x2f61, 286078},
	{0x2f62, 287036},
	{0x2f63, 287985},
	{0x2f64, 288969},
	{0x2f65, 289899},
	{0x2f66, 290819},
	{0x2f67, 291731},
	{0x2f68, 292677},
	{0x2f69, 293571},
	{0x2f6a, 294457},
	{0x2f6b, 295335},
	{0x2f6c, 296245},
	{0x2f6d, 297107},
	{0x2f6e, 297960},
	{0x2f6f, 298806},
	{0x2f70, 299684},
	{0x2f71, 300514},
	{0x2f72, 301338},
	{0x2f73, 302154},
	{0x2f74, 303002},
	{0x2f75, 303804},
	{0x2f76, 304599},
	{0x2f77, 305388},
	{0x2f78, 306207},
	{0x2f79, 306982},
	{0x2f7a, 307751},
	{0x2f7b, 308514},
	{0x2f7c, 309307},
	{0x2f7d, 310058},
	{0x2f7e, 310802},
	{0x2f7f, 311541},
	{0x3f40, 312309},
	{0x3f41, 313758},
	{0x3f42, 315218},
	{0x3f43, 316623},
	{0x3f44, 318041},
	{0x3f45, 319405},
	{0x3f46, 320781},
	{0x3f47, 322107},
	{0x3f48, 323445},
	{0x3f49, 324734},
	{0x3f4a, 326035},
	{0x3f4b, 327290},
	{0x3f4c, 328557},
	{0x3f4d, 329778},
	{0x3f4e, 331013},
	{0x3f4f, 332203},
	{0x3f50, 333406},
	{0x3f51, 334567},
	{0x3f52, 335741},
	{0x3f53, 336874},
	{0x3f54, 338019},
	{0x3f55, 339125},
	{0x3f56, 340244},
	{0x3f57, 341324},
	{0x3f58, 342418},
	{0x3f59, 343474},
	{0x3f5a, 344542},
	{0x3f5b, 345575},
	{0x3f5c, 346620},
	{0x3f5d, 347631},
	{0x3f5e, 348654},
	{0x3f5f, 349643},
	{0x3f60, 350644},
	{0x3f61, 351613},
	{0x3f62, 352594},
	{0x3f63, 353542},
	{0x3f64, 354504},
	{0x3f65, 356376},
	{0x3f66, 357299},
	{0x3f67, 358212},
	{0x3f68, 359117},
	{0x3f69, 360013},
	{0x3f6a, 360901},
	{0x3f6b, 361780},
	{0x3f6c, 362652},
	{0x3f6d, 363515},
	{0x3f6e, 364371},
	{0x3f6f, 365219},
	{0x3f70, 366059},
	{0x3f71, 366892},
	{0x3f72, 367718},
	{0x3f73, 368537},
	{0x3f74, 369348},
	{0x3f75, 370153},
	{0x3f76, 370951},
	{0x3f77, 371742},
	{0x3f78, 372527},
	{0x3f79, 373305},
	{0x3f7a, 374077},
	{0x3f7b, 374842},
	{0x3f7c, 375379},
	{0x3f7d, 375601},
	{0x3f7e, 376355},
	{0x3f7f, 377102},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
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

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 371,
		.lans = 2,
		.settle_time_apative_en = 1,
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
	.max_again = 377102,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 0x546 - 4,
	.integration_time_limit = 0x546 - 4,
	.total_width = 2200,
	.total_height = 0x546,
	.max_integration_time = 0x546 - 4,
	.one_line_expr_in_us = 29,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_1920_1080_25fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x301f, 0x03},
	{0x3243, 0x01},
	{0x3248, 0x02},
	{0x3249, 0x09},
	{0x3253, 0x08},
	{0x3271, 0x0a},
	{0x3301, 0x20},
	{0x3304, 0x40},
	{0x3306, 0x32},
	{0x330b, 0x88},
	{0x330f, 0x02},
	{0x331e, 0x39},
	{0x3333, 0x10},
	{0x3621, 0xe8},
	{0x3622, 0x16},
	{0x3637, 0x1b},
	{0x363a, 0x1f},
	{0x363b, 0xc6},
	{0x363c, 0x0e},
	{0x3670, 0x0a},
	{0x3674, 0x82},
	{0x3675, 0x76},
	{0x3676, 0x78},
	{0x367c, 0x48},
	{0x367d, 0x58},
	{0x3690, 0x34},
	{0x3691, 0x33},
	{0x3692, 0x44},
	{0x369c, 0x40},
	{0x369d, 0x48},
	{0x3901, 0x02},
	{0x3904, 0x04},
	{0x3908, 0x41},
	{0x391d, 0x14},
	{0x391f, 0x18},
	{0x3e01, 0x8c},
	{0x3e02, 0x20},
	{0x3e16, 0x00},
	{0x3e17, 0x80},
	{0x3f09, 0x48},
	{0x4827, 0x02},
	{0x4819, 0x30},
	{0x481b, 0x0b},
	{0x5787, 0x10},
	{0x5788, 0x06},
	{0x578a, 0x10},
	{0x578b, 0x06},
	{0x5790, 0x10},
	{0x5791, 0x10},
	{0x5792, 0x00},
	{0x5793, 0x10},
	{0x5794, 0x10},
	{0x5795, 0x00},
	{0x5799, 0x00},
	{0x57c7, 0x10},
	{0x57c8, 0x06},
	{0x57ca, 0x10},
	{0x57cb, 0x06},
	{0x57d1, 0x10},
	{0x57d4, 0x10},
	{0x57d9, 0x00},
	{0x59e0, 0x60},
	{0x59e1, 0x08},
	{0x59e2, 0x3f},
	{0x59e3, 0x18},
	{0x59e4, 0x18},
	{0x59e5, 0x3f},
	{0x59e6, 0x06},
	{0x59e7, 0x02},
	{0x59e8, 0x38},
	{0x59e9, 0x10},
	{0x59ea, 0x0c},
	{0x59eb, 0x10},
	{0x59ec, 0x04},
	{0x59ed, 0x02},
	{0x59ee, 0xa0},
	{0x59ef, 0x08},
	{0x59f4, 0x18},
	{0x59f5, 0x10},
	{0x59f6, 0x0c},
	{0x59f7, 0x10},
	{0x59f8, 0x06},
	{0x59f9, 0x02},
	{0x59fa, 0x18},
	{0x59fb, 0x10},
	{0x59fc, 0x0c},
	{0x59fd, 0x10},
	{0x59fe, 0x04},
	{0x59ff, 0x02},
	{0x320e, 0x05},
	{0x320f, 0x46},
	{0x36e9, 0x20},
	{0x36f9, 0x27},
	{0x0100, 0x01},
 	{SENSOR_REG_DELAY, 0x10},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_60fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x301f, 0x01},
	{0x3243, 0x01},
	{0x3248, 0x02},
	{0x3249, 0x09},
	{0x3253, 0x08},
	{0x3271, 0x0a},
	{0x3301, 0x06},
	{0x3302, 0x0c},
	{0x3303, 0x08},
	{0x3304, 0x60},
	{0x3306, 0x30},
	{0x3308, 0x10},
	{0x3309, 0x70},
	{0x330b, 0x80},
	{0x330d, 0x16},
	{0x330e, 0x1c},
	{0x330f, 0x02},
	{0x3310, 0x02},
	{0x331c, 0x04},
	{0x331e, 0x51},
	{0x331f, 0x61},
	{0x3320, 0x07},
	{0x3333, 0x10},
	{0x334c, 0x08},
	{0x3356, 0x09},
	{0x3364, 0x17},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x06},
	{0x3394, 0x06},
	{0x3395, 0x06},
	{0x3396, 0x08},
	{0x3397, 0x18},
	{0x3398, 0x38},
	{0x3399, 0x06},
	{0x339a, 0x0a},
	{0x339b, 0x10},
	{0x339c, 0x20},
	{0x33ac, 0x08},
	{0x33ae, 0x10},
	{0x33af, 0x19},
	{0x3621, 0xe8},
	{0x3622, 0x16},
	{0x3630, 0xa0},
	{0x3637, 0x36},
	{0x363a, 0x1f},
	{0x363b, 0xc6},
	{0x363c, 0x0e},
	{0x3670, 0x0a},
	{0x3674, 0x82},
	{0x3675, 0x76},
	{0x3676, 0x78},
	{0x367c, 0x48},
	{0x367d, 0x58},
	{0x3690, 0x34},
	{0x3691, 0x33},
	{0x3692, 0x44},
	{0x369c, 0x40},
	{0x369d, 0x48},
	{0x36eb, 0x0c},
	{0x36ec, 0x0c},
	{0x36fd, 0x14},
	{0x3901, 0x02},
	{0x3904, 0x04},
	{0x3908, 0x41},
	{0x391f, 0x10},
	{0x3e01, 0x8c},
	{0x3e02, 0x20},
	{0x3e16, 0x00},
	{0x3e17, 0x80},
	{0x3f09, 0x48},
	{0x4819, 0x09},
	{0x481b, 0x05},
	{0x481d, 0x14},
	{0x481f, 0x04},
	{0x4821, 0x0a},
	{0x4823, 0x05},
	{0x4825, 0x04},
	{0x4827, 0x05},
	{0x4829, 0x08},
	{0x5787, 0x10},
	{0x5788, 0x06},
	{0x578a, 0x10},
	{0x578b, 0x06},
	{0x5790, 0x10},
	{0x5791, 0x10},
	{0x5792, 0x00},
	{0x5793, 0x10},
	{0x5794, 0x10},
	{0x5795, 0x00},
	{0x5799, 0x00},
	{0x57c7, 0x10},
	{0x57c8, 0x06},
	{0x57ca, 0x10},
	{0x57cb, 0x06},
	{0x57d1, 0x10},
	{0x57d4, 0x10},
	{0x57d9, 0x00},
	{0x59e0, 0x60},
	{0x59e1, 0x08},
	{0x59e2, 0x3f},
	{0x59e3, 0x18},
	{0x59e4, 0x18},
	{0x59e5, 0x3f},
	{0x59e6, 0x06},
	{0x59e7, 0x02},
	{0x59e8, 0x38},
	{0x59e9, 0x10},
	{0x59ea, 0x0c},
	{0x59eb, 0x10},
	{0x59ec, 0x04},
	{0x59ed, 0x02},
	{0x59ee, 0xa0},
	{0x59ef, 0x08},
	{0x59f4, 0x18},
	{0x59f5, 0x10},
	{0x59f6, 0x0c},
	{0x59f7, 0x10},
	{0x59f8, 0x06},
	{0x59f9, 0x02},
	{0x59fa, 0x18},
	{0x59fb, 0x10},
	{0x59fc, 0x0c},
	{0x59fd, 0x10},
	{0x59fe, 0x04},
	{0x59ff, 0x02},
	{0x36e9, 0x20},
	{0x36f9, 0x24},
	{0x0100, 0x01},

 	{SENSOR_REG_DELAY, 0x10},
	{SENSOR_REG_END, 0x00},
};
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_25fps_mipi,
	},
	{
		.width = 1920,
		.height = 1080,
		.fps = 60 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_60fps_mipi,
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

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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
	int ret = 0;
	int it = (value & 0xffff) * 2;
	int again = (value & 0xffff0000) >> 16;

	ret = sensor_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0x0f));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
	ret += sensor_write(sd, 0x3e09, (unsigned char)(again & 0xff));
	ret += sensor_write(sd, 0x3e08, (unsigned char)((again >> 8 & 0xff)));

	if ((again >= 0x3f47) && dpc_flag) {
		ret += sensor_write(sd, 0x5799, 0x07);
		dpc_flag = false;
	} else if ((again <= 0x2f5f) && (!dpc_flag)) {
		ret += sensor_write(sd, 0x5799, 0x00);
		dpc_flag = true;
	}
	if (ret != 0) {
		ISP_ERROR("err: sc200ai write err %d\n",__LINE__);
		return ret;
	}

	return 0;
}

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
	unsigned int again = value;

	ret = sensor_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3e08, (unsigned char)((value >> 8 & 0xff)));
	if ((again >= 0x3f47) && dpc_flag) {
		ret += sensor_write(sd, 0x5799, 0x07);
		dpc_flag = false;
	} else if ((again <= 0x2f5f) && (!dpc_flag)) {
		ret += sensor_write(sd, 0x5799, 0x00);
		dpc_flag = true;
	}
	if (ret != 0)
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
	int ret = 0;

	if (!enable)
		return ISP_SUCCESS;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);
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
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("%s stream on\n", SENSOR_NAME);

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
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
	unsigned char tmp = 0;
	unsigned int max_fps = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;
	switch(sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		sclk = SENSOR_SUPPORT_PCLK_FPS_25;
		max_fps = TX_SENSOR_MAX_FPS_25;
		break;
	case TX_SENSOR_MAX_FPS_60:
		sclk = SENSOR_SUPPORT_PCLK_FPS_60;
		max_fps = TX_SENSOR_MAX_FPS_60;
		break;
	default:
		ISP_ERROR("do not support max framerate %d in mipi mode\n",sensor_max_fps);
	}
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}
	ret = sensor_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}
	hts = ((hts << 8) + tmp) << 1;
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += sensor_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}
	sensor->video.fps = fps;
	sensor_update_actual_fps((fps >> 16) & 0xffff);
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = -1;
	unsigned char val = 0x0;

	ret = sensor_read(sd, 0x3221, &val);
	if (enable & 0x2)
		val |= 0x60;
	else
		val &= 0x9f;
	ret += sensor_write(sd, 0x3221, val);
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}

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
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio request fail %d\n",reset_gpio);
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
			ISP_ERROR("gpio request fail %d\n",pwdn_gpio);
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
	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = sensor_set_expo(sd, *(int*)arg);
		break;
	/*
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sensor_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sensor_set_analog_gain(sd, *(int*)arg);
		break;
	*/
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
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off);

		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on);

		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
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
	int ret;
	unsigned long rate=0;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));

	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	rate = clk_get_rate(clk_get_parent(sensor->mclk));
	if (((rate / 1000) % 27000) != 0) {
		struct clk *vpll;
		vpll = clk_get(NULL,"vpll");
		if (IS_ERR(vpll)) {
			pr_err("get vpll failed\n");
		} else {
			rate = clk_get_rate(vpll);
			if (((rate / 1000) % 27000) != 0) {
				clk_set_rate(vpll,1080000027);
			}
			ret = clk_set_parent(sensor->mclk, vpll);
			if (ret < 0)
				pr_err("set mclk parent as epll err\n");
		}
	}
	private_clk_set_rate(sensor->mclk, 27000000);
	private_clk_enable(sensor->mclk);
	switch(sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		wsize = &sensor_win_sizes[0];
		sensor_info.max_fps = 25;
		sensor_attr.max_integration_time_native = 0x546 - 4;
                sensor_attr.integration_time_limit = 0x546 - 4;
                sensor_attr.total_width = 2200;
                sensor_attr.total_height = 0x546;
                sensor_attr.max_integration_time = 0x546 - 4;
		break;
	case TX_SENSOR_MAX_FPS_60:
		wsize = &sensor_win_sizes[1];
		sensor_info.max_fps = 60;
		sensor_attr.max_integration_time_native = 1125 - 4;
		sensor_attr.integration_time_limit = 1125 - 4;
		sensor_attr.total_width = 2200;
		sensor_attr.total_height = 1125;
		sensor_attr.max_integration_time = 1125 - 4;
		break;
	default:
		ISP_ERROR("do not support max framerate %d in mipi mode\n",sensor_max_fps);
	}

	sensor_attr.max_again = 377102;
	sensor_attr.max_dgain = 0; //sensor_attr.max_dgain;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor_attr.expo_fs = 1;
	sensor->video.attr = &sensor_attr;
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

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
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
	sensor_common_init(&sensor_info);

	ret = private_driver_get_interface();
	if (ret) {
		ISP_ERROR("Failed to init %s driver.\n", SENSOR_NAME);
		return -1;
	}
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	private_i2c_del_driver(&sensor_driver);
	sensor_common_exit();
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
