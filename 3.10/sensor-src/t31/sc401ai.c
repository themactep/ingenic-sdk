// SPDX-License-Identifier: GPL-2.0+
/*
 * sc401ai.c
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
#define SENSOR_NAME "sc401ai"
#define SENSOR_VERSION "H20210125a"
#define SENSOR_CHIP_ID 0xcd2e
#define SENSOR_CHIP_ID_H (0xcd)
#define SENSOR_CHIP_ID_L (0x2e)

// ============================================================================
// HARDWARE INTERFACE
// ============================================================================
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x30

// ============================================================================
// SENSOR CAPABILITIES
// ============================================================================
#define SENSOR_MAX_WIDTH 2560
#define SENSOR_MAX_HEIGHT 1440

// ============================================================================
// REGISTER DEFINITIONS
// ============================================================================
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe

// ============================================================================
// TIMING AND PERFORMANCE
// ============================================================================
#define SENSOR_SUPPORT_SCLK (126000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_1

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static int sensor_resolution = TX_SENSOR_RES_400;
module_param(sensor_resolution, int, S_IRUGO);
MODULE_PARM_DESC(sensor_resolution, "Sensor resolution");

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
	{0x2340, 36361},
	{0x2341, 37829},
	{0x2342, 39276},
	{0x2343, 40700},
	{0x2344, 42104},
	{0x2345, 43487},
	{0x2346, 44850},
	{0x2347, 46193},
	{0x2348, 47518},
	{0x2349, 48825},
	{0x234a, 50113},
	{0x234b, 51330},
	{0x234c, 52585},
	{0x234d, 53824},
	{0x234e, 55046},
	{0x234f, 56253},
	{0x2350, 57445},
	{0x2351, 58622},
	{0x2352, 59785},
	{0x2353, 60933},
	{0x2354, 62068},
	{0x2355, 63189},
	{0x2356, 64297},
	{0x2357, 65393},
	{0x2358, 66475},
	{0x2359, 67546},
	{0x235a, 68604},
	{0x235b, 69651},
	{0x235c, 70686},
	{0x235d, 71710},
	{0x235e, 72723},
	{0x235f, 73726},
	{0x2360, 74718},
	{0x2361, 75657},
	{0x2362, 76629},
	{0x2363, 77591},
	{0x2364, 78543},
	{0x2365, 79486},
	{0x2366, 80419},
	{0x2367, 81344},
	{0x2368, 82259},
	{0x2369, 83166},
	{0x236a, 84064},
	{0x236b, 84953},
	{0x236c, 85835},
	{0x236d, 86708},
	{0x236e, 87573},
	{0x236f, 88430},
	{0x2370, 89280},
	{0x2371, 90122},
	{0x2372, 90956},
	{0x2373, 91784},
	{0x2374, 92604},
	{0x2375, 93417},
	{0x2376, 94188},
	{0x2377, 94988},
	{0x2378, 95781},
	{0x2379, 96567},
	{0x237a, 97347},
	{0x237b, 98120},
	{0x237c, 98888},
	{0x237d, 99649},
	{0x237e, 100404},
	{0x237f, 101153},
	{0x2740, 101896},
	{0x2741, 103364},
	{0x2742, 104811},
	{0x2743, 106235},
	{0x2744, 107639},
	{0x2745, 109022},
	{0x2746, 110355},
	{0x2747, 111699},
	{0x2748, 113024},
	{0x2749, 114331},
	{0x274a, 115620},
	{0x274b, 116892},
	{0x274c, 118147},
	{0x274d, 119385},
	{0x274e, 120608},
	{0x274f, 121814},
	{0x2750, 123006},
	{0x2751, 124157},
	{0x2752, 125320},
	{0x2753, 126468},
	{0x2754, 127603},
	{0x2755, 128724},
	{0x2756, 129832},
	{0x2757, 130928},
	{0x2758, 132010},
	{0x2759, 133081},
	{0x275a, 134139},
	{0x275b, 135163},
	{0x275c, 136199},
	{0x275d, 137223},
	{0x275e, 138236},
	{0x275f, 139239},
	{0x2760, 140231},
	{0x2761, 141213},
	{0x2762, 142185},
	{0x2763, 143146},
	{0x2764, 144098},
	{0x2765, 145041},
	{0x2766, 145954},
	{0x2767, 146879},
	{0x2768, 147794},
	{0x2769, 148701},
	{0x276a, 149599},
	{0x276b, 150488},
	{0x276c, 151370},
	{0x276d, 152243},
	{0x276e, 153108},
	{0x276f, 153965},
	{0x2770, 154815},
	{0x2771, 155639},
	{0x2772, 156473},
	{0x2773, 157301},
	{0x2774, 158121},
	{0x2775, 158934},
	{0x2776, 159741},
	{0x2777, 160540},
	{0x2778, 161333},
	{0x2779, 162119},
	{0x277a, 162899},
	{0x277b, 163655},
	{0x277c, 164423},
	{0x277d, 165184},
	{0x277e, 165939},
	{0x277f, 166688},
	{0x2f40, 167431},
	{0x2f41, 168899},
	{0x2f42, 170346},
	{0x2f43, 171755},
	{0x2f44, 173159},
	{0x2f45, 174542},
	{0x2f46, 175905},
	{0x2f47, 177249},
	{0x2f48, 178574},
	{0x2f49, 179866},
	{0x2f4a, 181155},
	{0x2f4b, 182427},
	{0x2f4c, 183682},
	{0x2f4d, 184920},
	{0x2f4e, 186129},
	{0x2f4f, 187336},
	{0x2f50, 188528},
	{0x2f51, 189705},
	{0x2f52, 190867},
	{0x2f53, 192003},
	{0x2f54, 193138},
	{0x2f55, 194259},
	{0x2f56, 195367},
	{0x2f57, 196463},
	{0x2f58, 197545},
	{0x2f59, 198604},
	{0x2f5a, 199663},
	{0x2f5b, 200710},
	{0x2f5c, 201745},
	{0x2f5d, 202769},
	{0x2f5e, 203771},
	{0x2f5f, 204774},
	{0x2f60, 205766},
	{0x2f61, 206748},
	{0x2f62, 207720},
	{0x2f63, 208671},
	{0x2f64, 209623},
	{0x2f65, 210566},
	{0x2f66, 211499},
	{0x2f67, 212424},
	{0x2f68, 213339},
	{0x2f69, 214236},
	{0x2f6a, 215134},
	{0x2f6b, 216023},
	{0x2f6c, 216905},
	{0x2f6d, 217778},
	{0x2f6e, 218633},
	{0x2f6f, 219491},
	{0x2f70, 220341},
	{0x2f71, 221183},
	{0x2f72, 222017},
	{0x2f73, 222836},
	{0x2f74, 223656},
	{0x2f75, 224469},
	{0x2f76, 225276},
	{0x2f77, 226075},
	{0x2f78, 226868},
	{0x2f79, 227646},
	{0x2f7a, 228425},
	{0x2f7b, 229199},
	{0x2f7c, 229966},
	{0x2f7d, 230727},
	{0x2f7e, 231474},
	{0x2f7f, 232223},
	{0x3f40, 232966},
	{0x3f41, 234434},
	{0x3f42, 235873},
	{0x3f43, 237298},
	{0x3f44, 238701},
	{0x3f45, 240077},
	{0x3f46, 241440},
	{0x3f47, 242777},
	{0x3f48, 244102},
	{0x3f49, 245408},
	{0x3f4a, 246690},
	{0x3f4b, 247962},
	{0x3f4c, 249217},
	{0x3f4d, 250449},
	{0x3f4e, 251671},
	{0x3f4f, 252871},
	{0x3f50, 254063},
	{0x3f51, 255240},
	{0x3f52, 256396},
	{0x3f53, 257545},
	{0x3f54, 258679},
	{0x3f55, 259794},
	{0x3f56, 260902},
	{0x3f57, 261992},
	{0x3f58, 263074},
	{0x3f59, 264145},
	{0x3f5a, 265198},
	{0x3f5b, 266245},
	{0x3f5c, 267280},
	{0x3f5d, 268299},
	{0x3f5e, 269312},
	{0x3f5f, 270309},
	{0x3f60, 271301},
	{0x3f61, 272283},
	{0x3f62, 273249},
	{0x3f63, 274211},
	{0x3f64, 275163},
	{0x3f65, 276101},
	{0x3f66, 277034},
	{0x3f67, 277954},
	{0x3f68, 278869},
	{0x3f69, 279775},
	{0x3f6a, 280669},
	{0x3f6b, 281558},
	{0x3f6c, 282440},
	{0x3f6d, 283308},
	{0x3f6e, 284173},
	{0x3f6f, 285026},
	{0x3f70, 285876},
	{0x3f71, 286718},
	{0x3f72, 287548},
	{0x3f73, 288375},
	{0x3f74, 289196},
	{0x3f75, 290004},
	{0x3f76, 290811},
	{0x3f77, 291606},
	{0x3f78, 292399},
	{0x3f79, 293185},
	{0x3f7a, 293960},
	{0x3f7b, 294734},
	{0x3f7c, 295501},
	{0x3f7d, 296258},
	{0x3f7e, 297013},
	{0x3f7f, 297758},

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
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 630,
		.lans = 2,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
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
	},
	.max_again = 297758,
	.max_dgain = 0,
	.min_integration_time = 3,
	.min_integration_time_native = 3,
	.max_integration_time_native = 1796,
	.integration_time_limit = 1796,
	.total_width = 2800,
	.total_height = 1800,
	.max_integration_time = 1796,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

struct tx_isp_mipi_bus sensor_1280_720_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 315,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 1280,
	.image_theight = 720,
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

struct tx_isp_mipi_bus sensor_2560_1440_mipi={
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 630,
		.lans = 2,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
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

static struct regval_list sensor_init_regs_2560_1440_25fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x3018, 0x3a},
	{0x3019, 0x0c},
	{0x301c, 0x78},
	{0x301f, 0x08},
	{0x3208, 0x0a},
	{0x3209, 0x00},
	{0x320a, 0x05},
	{0x320b, 0xa0},
	{0x320e, 0x07},
	{0x320f, 0x08},
	{0x3214, 0x11},
	{0x3215, 0x11},
	{0x3223, 0x80},
	{0x3250, 0x00},
	{0x3253, 0x08},
	{0x3274, 0x01},
	{0x3301, 0x20},
	{0x3302, 0x18},
	{0x3303, 0x10},
	{0x3304, 0x50},
	{0x3306, 0x38},
	{0x3308, 0x18},
	{0x3309, 0x60},
	{0x330b, 0xc0},
	{0x330d, 0x10},
	{0x330e, 0x18},
	{0x330f, 0x04},
	{0x3310, 0x02},
	{0x331c, 0x04},
	{0x331e, 0x41},
	{0x331f, 0x51},
	{0x3320, 0x09},
	{0x3333, 0x10},
	{0x334c, 0x08},
	{0x3356, 0x09},
	{0x3364, 0x17},
	{0x338e, 0xfd},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x20},
	{0x3394, 0x20},
	{0x3395, 0x20},
	{0x3396, 0x08},
	{0x3397, 0x18},
	{0x3398, 0x38},
	{0x3399, 0x20},
	{0x339a, 0x20},
	{0x339b, 0x20},
	{0x339c, 0x20},
	{0x33ac, 0x10},
	{0x33ae, 0x18},
	{0x33af, 0x19},
	{0x360f, 0x01},
	{0x3620, 0x08},
	{0x3637, 0x25},
	{0x363a, 0x12},/*auto logic enable*/
	{0x3670, 0x0a},
	{0x3671, 0x07},
	{0x3672, 0x57},
	{0x3673, 0x5e},
	{0x3674, 0x84},
	{0x3675, 0x88},
	{0x3676, 0x8a},
	{0x367a, 0x58},
	{0x367b, 0x78},
	{0x367c, 0x58},
	{0x367d, 0x78},
	{0x3690, 0x33},
	{0x3691, 0x43},
	{0x3692, 0x34},
	{0x369c, 0x40},
	{0x369d, 0x78},
	{0x36ea, 0x39},
	{0x36eb, 0x0d},
	{0x36ec, 0x1c},
	{0x36ed, 0x24},
	{0x36fa, 0x39},
	{0x36fb, 0x33},
	{0x36fc, 0x10},
	{0x36fd, 0x14},
	{0x3908, 0x41},
	{0x396c, 0x0e},
	{0x3e00, 0x00},
	{0x3e01, 0xb6},
	{0x3e02, 0x00},
	{0x3e03, 0x0b},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3e1b, 0x2a},
	{0x4509, 0x30},
	{0x4819, 0x08},
	{0x481b, 0x05},
	{0x481d, 0x11},
	{0x481f, 0x04},
	{0x4821, 0x09},
	{0x4823, 0x04},
	{0x4825, 0x04},
	{0x4827, 0x04},
	{0x4829, 0x07},
	{0x57a8, 0xd0},
	{0x36e9, 0x23},
	{0x36f9, 0x23},
	{0x0100, 0x01},
	{SENSOR_REG_DELAY, 0x10},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1280_720_30fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x3018, 0x3a},
	{0x3019, 0x0c},
	{0x301c, 0x78},
	{0x301f, 0x37},
	{0x3208, 0x05},
	{0x3209, 0x00},
	{0x320a, 0x02},
	{0x320b, 0xd0},
	{0x320e, 0x05},
	{0x320f, 0xdc},
	{0x3211, 0x02},
	{0x3213, 0x02},
	{0x3214, 0x11},
	{0x3215, 0x31},
	{0x3220, 0x17},
	{0x3223, 0x80},
	{0x3250, 0x00},
	{0x3253, 0x08},
	{0x3274, 0x01},
	{0x3301, 0x20},
	{0x3302, 0x18},
	{0x3303, 0x10},
	{0x3304, 0x50},
	{0x3306, 0x38},
	{0x3308, 0x18},
	{0x3309, 0x60},
	{0x330b, 0xc0},
	{0x330d, 0x10},
	{0x330e, 0x18},
	{0x330f, 0x04},
	{0x3310, 0x02},
	{0x331c, 0x04},
	{0x331e, 0x41},
	{0x331f, 0x51},
	{0x3320, 0x09},
	{0x3333, 0x10},
	{0x334c, 0x08},
	{0x3356, 0x09},
	{0x3364, 0x17},
	{0x338e, 0xfd},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x20},
	{0x3394, 0x20},
	{0x3395, 0x20},
	{0x3396, 0x08},
	{0x3397, 0x18},
	{0x3398, 0x38},
	{0x3399, 0x20},
	{0x339a, 0x20},
	{0x339b, 0x20},
	{0x339c, 0x20},
	{0x33ac, 0x10},
	{0x33ae, 0x18},
	{0x33af, 0x19},
	{0x360f, 0x01},
	{0x3620, 0x08},
	{0x3637, 0x25},
	{0x363a, 0x12},
	{0x3670, 0x0a},
	{0x3671, 0x07},
	{0x3672, 0x57},
	{0x3673, 0x5e},
	{0x3674, 0x84},
	{0x3675, 0x88},
	{0x3676, 0x8a},
	{0x367a, 0x58},
	{0x367b, 0x78},
	{0x367c, 0x58},
	{0x367d, 0x78},
	{0x3690, 0x33},
	{0x3691, 0x43},
	{0x3692, 0x34},
	{0x369c, 0x40},
	{0x369d, 0x78},
	{0x36ea, 0x39},
	{0x36eb, 0x0d},
	{0x36ec, 0x2c},
	{0x36ed, 0x24},
	{0x36fa, 0x39},
	{0x36fb, 0x33},
	{0x36fc, 0x10},
	{0x36fd, 0x14},
	{0x3908, 0x41},
	{0x396c, 0x0e},
	{0x3e00, 0x00},
	{0x3e01, 0xb6},
	{0x3e02, 0x00},
	{0x3e03, 0x0b},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3e1b, 0x2a},
	{0x4509, 0x30},
	{0x4819, 0x04},
	{0x481b, 0x03},
	{0x481d, 0x09},
	{0x481f, 0x02},
	{0x4821, 0x08},
	{0x4823, 0x02},
	{0x4825, 0x02},
	{0x4827, 0x02},
	{0x4829, 0x04},
	{0x5000, 0x46},
	{0x5001, 0x44},
	{0x57a8, 0xd0},
	{0x5900, 0xf2},
	{0x5901, 0x04},
	{0x36e9, 0x23},
	{0x36f9, 0x23},
	{0x0100, 0x01},
	//[gain<2xdcg]
	{0x363a, 0x0b},
	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 2560*1920 */
	{
		.width = 2560,
		.height = 1440,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2560_1440_25fps_mipi,
	},
	/* 1280*720 */
	{
		.width = 1280,
		.height = 720,
		.fps = 30 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1280_720_30fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

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
	int ret = -1;
	int it = (value & 0xffff) * 2;
	int again = (value & 0xffff0000) >> 16;

	ret = sensor_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
	ret += sensor_write(sd, 0x3e09, (unsigned char)(again & 0xff));
	ret += sensor_write(sd, 0x3e08, (unsigned char)(((again >> 8) & 0xff)));

	/* cut off logic when 0x363a==0x12*/
	if (again < 0x2740) {
		ret += sensor_write(sd, 0x363a, 0x0b);
	} else {
		ret += sensor_write(sd, 0x363a, 0x17);
	}

	if (ret < 0)
		return ret;


	return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
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

static int sensor_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!enable)
		return ISP_SUCCESS;

	if (sensor_resolution == TX_SENSOR_RES_400) {
		wsize = &sensor_win_sizes[0];
		sensor_info.max_fps = 25;
	} else if (sensor_resolution == TX_SENSOR_RES_100) {
		wsize = &sensor_win_sizes[1];
		sensor_info.max_fps = 30;
	}

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
		ret = sensor_write_array(sd, sensor_stream_on_mipi);
		ISP_WARNING("%s stream on\n", SENSOR_NAME);

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
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}
	 sclk = SENSOR_SUPPORT_SCLK;

	ret += sensor_read(sd, 0x320c, &val);
	hts = val << 8;
	ret += sensor_read(sd, 0x320d, &val);
	hts = (hts | val) << 1;
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

	sensor_update_actual_fps((fps >> 16) & 0xffff);
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (sensor_resolution == TX_SENSOR_RES_400) {
		wsize = &sensor_win_sizes[0];
		sensor_info.max_fps = 25;
	} else if (sensor_resolution == TX_SENSOR_RES_100) {
		wsize = &sensor_win_sizes[1];
		sensor_info.max_fps = 30;
	}

	if (wsize) {
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;

		sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

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
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio request fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
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

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val;

	ret += sensor_read(sd, 0x3221, &val);
	if (enable & 0x02) {
		val |= 0x60;
	} else {
		val &= 0x9f;
	}
	ret = sensor_write(sd, 0x3221, val);
	if (ret < 0)
		return -1;

	return ret;
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
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		//if (arg)
		//	ret = sensor_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		//if (arg)
		//	ret = sensor_set_analog_gain(sd, *(int*)arg);
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
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sensor_write_array(sd, sensor_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if (arg)
			ret = sensor_set_vflip(sd, *(int*)arg);
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

	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	if (sensor_resolution == TX_SENSOR_RES_400) {
		wsize = &sensor_win_sizes[0];
		sensor_info.max_fps = 25;
		memcpy(&sensor_attr.mipi, &sensor_2560_1440_mipi, sizeof(sensor_2560_1440_mipi));
		sensor_attr.max_integration_time_native = 1796;
		sensor_attr.integration_time_limit = 1796;
		sensor_attr.total_width = 2800;
		sensor_attr.total_height = 1800;
		sensor_attr.max_integration_time = 1796;
	} else if (sensor_resolution == TX_SENSOR_RES_100) {
		wsize = &sensor_win_sizes[1];
		sensor_info.max_fps = 30;
		memcpy(&sensor_attr.mipi, &sensor_1280_720_mipi, sizeof(sensor_1280_720_mipi));
		sensor_attr.max_integration_time_native = 1496;
		sensor_attr.integration_time_limit = 1496;
		sensor_attr.total_width = 2800;
		sensor_attr.total_height = 1500;
		sensor_attr.max_integration_time = 1496;
	} else {
		ISP_WARNING("Sensor Configuration not found.\n");
		return -ENODEV;
	}

	/*
	  convert sensor-gain into isp-gain,
	*/
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
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;

	sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);
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
