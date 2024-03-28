// SPDX-License-Identifier: GPL-2.0+
/*
 * sc5239.c
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
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
#include <txx-funcs.h>

#define SENSOR_NAME "sc5239"
#define SENSOR_CHIP_ID_H (0x52)
#define SENSOR_CHIP_ID_L (0x35)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_30FPS_SCLK 1380 * 2000 * 30
#define SENSOR_SUPPORT_15FPS_SCLK_HDR (1380 * 4000 * 15)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20230701a"

static int reset_gpio = -1;
static int pwdn_gpio = -1;
static int wdr_bufsize = 1380 * 4000 * 2;
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
	{0x320, 0},
	{0x321, 2886},
	{0x322, 5776},
	{0x323, 8494},
	{0x324, 11136},
	{0x325, 13706},
	{0x326, 16287},
	{0x327, 18723},
	{0x328, 21097},
	{0x329, 23414},
	{0x32a, 25746},
	{0x32b, 27953},
	{0x32c, 30109},
	{0x32d, 32217},
	{0x32e, 34345},
	{0x32f, 36361},
	{0x330, 38336},
	{0x331, 40270},
	{0x332, 42226},
	{0x333, 44082},
	{0x334, 45904},
	{0x335, 47690},
	{0x336, 49500},
	{0x337, 51220},
	{0x338, 52910},
	{0x339, 54571},
	{0x33a, 56254},
	{0x33b, 57857},
	{0x33c, 59433},
	{0x33d, 60984},
	{0x33e, 62558},
	{0x33f, 64059},
	{0x720, 65536},
	{0x721, 68468},
	{0x722, 71267},
	{0x723, 74030},
	{0x724, 76672},
	{0x725, 79283},
	{0x726, 81784},
	{0x727, 84259},
	{0x728, 86633},
	{0x729, 89538},
	{0x72a, 91246},
	{0x72b, 93489},
	{0x72c, 95645},
	{0x72d, 97686},
	{0x72e, 99848},
	{0x72f, 101961},
	{0x730, 103872},
	{0x731, 105744},
	{0x732, 107731},
	{0x733, 109678},
	{0x734, 111440},
	{0x735, 113169},
	{0x736, 115008},
	{0x737, 116811},
	{0x738, 118446},
	{0x739, 120053},
	{0x73a, 121764},
	{0x73b, 123444},
	{0x73c, 124969},
	{0x73d, 126470},
	{0x73e, 128070},
	{0x73f, 129643},
	{0xf20, 131072},
	{0xf21, 134095},
	{0xf22, 136803},
	{0xf23, 139652},
	{0xf24, 142208},
	{0xf25, 144900},
	{0xf26, 147320},
	{0xf27, 149873},
	{0xf28, 152169},
	{0xf29, 154596},
	{0xf2a, 156782},
	{0xf2b, 159095},
	{0xf2c, 161181},
	{0xf2d, 163390},
	{0xf2e, 165384},
	{0xf2f, 167497},
	{0xf30, 169408},
	{0xf31, 171434},
	{0xf32, 173267},
	{0xf33, 175214},
	{0xf34, 176976},
	{0xf35, 178848},
	{0xf36, 180544},
	{0xf37, 182347},
	{0xf38, 183982},
	{0xf39, 185722},
	{0xf3a, 187300},
	{0xf3b, 188980},
	{0xf3c, 190505},
	{0xf3d, 192130},
	{0xf3e, 193606},
	{0xf3f, 195179},
	{0x1f20, 196608},
	{0x1f21, 199517},
	{0x1f22, 202339},
	{0x1f23, 205080},
	{0x1f24, 207744},
	{0x1f25, 210334},
	{0x1f26, 212856},
	{0x1f27, 215312},
	{0x1f28, 217705},
	{0x1f29, 220040},
	{0x1f2a, 222318},
	{0x1f2b, 224543},
	{0x1f2c, 226717},
	{0x1f2d, 228842},
	{0x1f2e, 230920},
	{0x1f2f, 232953},
	{0x1f30, 234944},
	{0x1f31, 236893},
	{0x1f32, 238803},
	{0x1f33, 240676},
	{0x1f34, 242512},
	{0x1f35, 244313},
	{0x1f36, 246080},
	{0x1f37, 247815},
	{0x1f38, 249518},
	{0x1f39, 251192},
	{0x1f3a, 254452},
	{0x1f3b, 256041},
	{0x1f3c, 257604},
	{0x1f3d, 259142},
	{0x1f3e, 260655},
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

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
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

struct tx_isp_mipi_bus sensor_mipi = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 828,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10, // RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.image_twidth = 2560,
	.image_theight = 1920,
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
struct tx_isp_mipi_bus sensor_mipi_dol = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 828,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10, // RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 2560,
	.image_theight = 1920,
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
struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = 0x9c42,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.cbus_device = 0x30,
	.max_again = 260655,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.integration_time_apply_delay = 2,
	.max_integration_time_native = 4000 - 8,
	.integration_time_limit = 4000 - 8,
	.max_integration_time = 4000 - 8,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	.sensor_ctrl.alloc_again_short = sensor_alloc_again_short,
	.sensor_ctrl.alloc_integration_time_short = sensor_alloc_integration_time_short,
};

static struct regval_list sensor_init_regs_2560_1920_15fps_mipi_dol[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x3039, 0x80},
	{0x3029, 0x80},
	{0x301f, 0x1b},
	{0x302a, 0x69},
	{0x302b, 0x01},
	{0x302c, 0x00},
	{0x302d, 0x03},
	{0x3037, 0x26},
	{0x3038, 0x66},
	{0x303a, 0x29},
	{0x303b, 0x0a},
	{0x303c, 0x0e},
	{0x303d, 0x03},
	{0x3200, 0x00},
	{0x3201, 0x10},
	{0x3202, 0x00},
	{0x3203, 0x0c},
	{0x3204, 0x0a},
	{0x3205, 0x1f},
	{0x3206, 0x07},
	{0x3207, 0x93},
	{0x3208, 0x0a},
	{0x3209, 0x00},
	{0x320a, 0x07},
	{0x320b, 0x80},
	{0x320c, 0x05},
	{0x320d, 0x64},
	{0x320e, 0x0f},
	{0x320f, 0xa0},
	{0x3210, 0x00},
	{0x3211, 0x08},
	{0x3212, 0x00},
	{0x3213, 0x04},
	{0x3220, 0x50},
	{0x3235, 0x1f},
	{0x3236, 0x3e},
	{0x3301, 0x38},
	{0x3303, 0x20},
	{0x3304, 0x10},
	{0x3306, 0x58},
	{0x3308, 0x10},
	{0x3309, 0x60},
	{0x330a, 0x00},
	{0x330b, 0xb8},
	{0x330d, 0x30},
	{0x330e, 0x20},
	{0x3314, 0x14},
	{0x3315, 0x02},
	{0x331b, 0x83},
	{0x331e, 0x19},
	{0x331f, 0x59},
	{0x3320, 0x01},
	{0x3321, 0x04},
	{0x3326, 0x00},
	{0x3332, 0x22},
	{0x3333, 0x20},
	{0x3334, 0x40},
	{0x3350, 0x22},
	{0x3359, 0x22},
	{0x335c, 0x22},
	{0x3364, 0x05},
	{0x3366, 0xc8},
	{0x3367, 0x08},
	{0x3368, 0x03},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x336c, 0x01},
	{0x336d, 0x40},
	{0x337e, 0x88},
	{0x337f, 0x03},
	{0x338f, 0x40},
	{0x33ae, 0x22},
	{0x33af, 0x22},
	{0x33b0, 0x22},
	{0x33b4, 0x22},
	{0x33b6, 0x07},
	{0x33b7, 0x17},
	{0x33b8, 0x20},
	{0x33b9, 0x20},
	{0x33ba, 0x44},
	{0x3614, 0x00},
	{0x3620, 0x28},
	{0x3621, 0xac},
	{0x3622, 0xf6},
	{0x3623, 0x08},
	{0x3624, 0x47},
	{0x3625, 0x0b},
	{0x3630, 0x30},
	{0x3631, 0x88},
	{0x3632, 0x18},
	{0x3633, 0x34},
	{0x3634, 0x86},
	{0x3635, 0x4d},
	{0x3636, 0x21},
	{0x3637, 0x20},
	{0x3638, 0x18},
	{0x3639, 0x09},
	{0x363a, 0x83},
	{0x363b, 0x02},
	{0x363c, 0x07},
	{0x363d, 0x03},
	{0x3670, 0x00},
	{0x3677, 0x86},
	{0x3678, 0x86},
	{0x3679, 0xa8},
	{0x367e, 0x08},
	{0x367f, 0x18},
	{0x3905, 0x98},
	{0x3907, 0x01},
	{0x3908, 0x11},
	{0x390a, 0x00},
	{0x391c, 0x9f},
	{0x391d, 0x00},
	{0x391e, 0x01},
	{0x391f, 0xc0},
	{0x3988, 0x11},
	{0x3e00, 0x01},
	{0x3e01, 0xd4},
	{0x3e02, 0xe0},
	{0x3e03, 0x0b},
	{0x3e04, 0x1d},
	{0x3e05, 0xe0},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x20},
	{0x3e1e, 0x30},
	{0x3e23, 0x00},
	{0x3e24, 0xf2},
	{0x3e26, 0x20},
	{0x3f00, 0x0d},
	{0x3f02, 0x05},
	{0x3f04, 0x02},
	{0x3f05, 0xaa},
	{0x3f06, 0x21},
	{0x3f08, 0x04},
	{0x4500, 0x5d},
	{0x4502, 0x10},
	{0x4509, 0x10},
	{0x4602, 0x0f},
	{0x4809, 0x01},
	{0x4816, 0x51},
	{0x4837, 0x19},
	{0x5000, 0x20},
	{0x5002, 0x00},
	{0x6000, 0x26},
	{0x6002, 0x06},
	{0x3039, 0x23},
	{0x3029, 0x33},
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_2560_1920_30fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x3039, 0x80},
	{0x3029, 0x80},
	{0x301f, 0x29},
	{0x302a, 0x69},
	{0x302b, 0x01},
	{0x302c, 0x00},
	{0x302d, 0x03},
	{0x3037, 0x26},
	{0x3038, 0x66},
	{0x303a, 0x29},
	{0x303b, 0x0a},
	{0x303c, 0x0e},
	{0x303d, 0x03},
	{0x3200, 0x00},
	{0x3201, 0x00},
	{0x3202, 0x00},
	{0x3203, 0x00},
	{0x3204, 0x0a},
	{0x3205, 0x2b},
	{0x3206, 0x07},
	{0x3207, 0x9f},
	{0x3208, 0x0a},
	{0x3209, 0x20},
	{0x320a, 0x07},
	{0x320b, 0x98},
	{0x320c, 0x05},
	{0x320d, 0x64},
	{0x320e, 0x07},
	{0x320f, 0xd0},
	{0x3210, 0x00},
	{0x3211, 0x08},
	{0x3212, 0x00},
	{0x3213, 0x04},
	{0x3235, 0x0f},
	{0x3236, 0x9c},
	{0x3301, 0x38},
	{0x3303, 0x20},
	{0x3304, 0x10},
	{0x3306, 0x58},
	{0x3308, 0x10},
	{0x3309, 0x60},
	{0x330a, 0x00},
	{0x330b, 0xb8},
	{0x330d, 0x30},
	{0x330e, 0x20},
	{0x3314, 0x14},
	{0x3315, 0x02},
	{0x331b, 0x83},
	{0x331e, 0x19},
	{0x331f, 0x59},
	{0x3320, 0x01},
	{0x3321, 0x04},
	{0x3326, 0x00},
	{0x3332, 0x22},
	{0x3333, 0x20},
	{0x3334, 0x40},
	{0x3350, 0x22},
	{0x3359, 0x22},
	{0x335c, 0x22},
	{0x3364, 0x05},
	{0x3366, 0xc8},
	{0x3367, 0x08},
	{0x3368, 0x03},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x336c, 0x01},
	{0x336d, 0x40},
	{0x337e, 0x88},
	{0x337f, 0x03},
	{0x338f, 0x40},
	{0x33ae, 0x22},
	{0x33af, 0x22},
	{0x33b0, 0x22},
	{0x33b4, 0x22},
	{0x33b6, 0x07},
	{0x33b7, 0x17},
	{0x33b8, 0x20},
	{0x33b9, 0x20},
	{0x33ba, 0x44},
	{0x3614, 0x00},
	{0x3620, 0x28},
	{0x3621, 0xac},
	{0x3622, 0xf6},
	{0x3623, 0x08},
	{0x3624, 0x47},
	{0x3625, 0x0b},
	{0x3630, 0x30},
	{0x3631, 0x88},
	{0x3632, 0x18},
	{0x3633, 0x34},
	{0x3634, 0x86},
	{0x3635, 0x4d},
	{0x3636, 0x21},
	{0x3637, 0x20},
	{0x3638, 0x18},
	{0x3639, 0x09},
	{0x363a, 0x83},
	{0x363b, 0x02},
	{0x363c, 0x07},
	{0x363d, 0x03},
	{0x3670, 0x00},
	{0x3677, 0x86},
	{0x3678, 0x86},
	{0x3679, 0xa8},
	{0x367e, 0x08},
	{0x367f, 0x18},
	{0x3905, 0x98},
	{0x3907, 0x01},
	{0x3908, 0x11},
	{0x390a, 0x00},
	{0x391c, 0x9f},
	{0x391d, 0x00},
	{0x391e, 0x01},
	{0x391f, 0xc0},
	{0x3e01, 0xf9},
	{0x3e02, 0xa0},
	{0x3e05, 0xe0},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x20},
	{0x3e1e, 0x30},
	{0x3e26, 0x20},
	{0x3f00, 0x0d},
	{0x3f02, 0x05},
	{0x3f04, 0x02},
	{0x3f05, 0xaa},
	{0x3f06, 0x21},
	{0x3f08, 0x04},
	{0x4500, 0x5d},
	{0x4502, 0x10},
	{0x4509, 0x10},
	{0x4800, 0x64},
	{0x4809, 0x01},
	{0x4818, 0x00},
	{0x4819, 0x34},
	{0x481a, 0x00},
	{0x481b, 0x1c},
	{0x481c, 0x00},
	{0x481d, 0xc8},
	{0x4821, 0x02},
	{0x4822, 0x00},
	{0x4823, 0x03},
	{0x4828, 0x00},
	{0x4829, 0x02},
	{0x4837, 0x19},
	{0x5000, 0x20},
	{0x5002, 0x00},
	{0x5988, 0x02},
	{0x598e, 0x05},
	{0x598f, 0x30},
	{0x6000, 0x26},
	{0x6002, 0x06},
	{0x3039, 0x23},
	{0x3029, 0x33},
	{0x3200, 0x00},
	{0x3201, 0x10},
	{0x3202, 0x00},
	{0x3203, 0x0c},
	{0x3204, 0x0a},
	{0x3205, 0x1f},
	{0x3206, 0x07},
	{0x3207, 0x9b},
	{0x3208, 0x0a},
	{0x3209, 0x00},
	{0x320a, 0x07},
	{0x320b, 0x80},
	{0x3210, 0x00},
	{0x3211, 0x08},
	{0x3212, 0x00},
	{0x3213, 0x08},
	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 2560,
		.height = 1920,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2560_1920_30fps_mipi,
	},
	{
		.width = 2560,
		.height = 1920,
		.fps = 15 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2560_1920_15fps_mipi_dol,
	}};
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
		}};
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

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
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

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident) {

	int ret;
	unsigned char v;

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

static int sensor_set_expo(struct tx_isp_subdev *sd, int value) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 1;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;
	if (sensor->video.attr->data_type == TX_SENSOR_DATA_TYPE_LINEAR) {
		if (again < 0x0720) {
			ret += sensor_write(sd, 0x3301, 0x20);
			ret += sensor_write(sd, 0x3630, 0x30);
			ret += sensor_write(sd, 0x3633, 0x34);
			ret += sensor_write(sd, 0x3622, 0xf6);
			ret += sensor_write(sd, 0x363a, 0x83);
			if (ret < 0)
				return ret;
		} else if (again >= 0x0720 && again < 0x0f20) {
			ret += sensor_write(sd, 0x3301, 0x28);
			ret += sensor_write(sd, 0x3630, 0x34);
			ret += sensor_write(sd, 0x3633, 0x35);
			ret += sensor_write(sd, 0x3622, 0xf6);
			ret += sensor_write(sd, 0x363a, 0x87);
			if (ret < 0)
				return ret;
		} else if (again >= 0x0f20 && again < 0x1f20) {
			ret += sensor_write(sd, 0x3301, 0x28);
			ret += sensor_write(sd, 0x3630, 0x24);
			ret += sensor_write(sd, 0x3633, 0x35);
			ret += sensor_write(sd, 0x3622, 0xf6);
			ret += sensor_write(sd, 0x363a, 0x9f);
			if (ret < 0)
				return ret;
		} else if (again >= 0x1f20 && again < 0x1f3e) {
			ret += sensor_write(sd, 0x3301, 0x48);
			ret += sensor_write(sd, 0x3630, 0x16);
			ret += sensor_write(sd, 0x3633, 0x45);
			ret += sensor_write(sd, 0x3622, 0xf6);
			ret += sensor_write(sd, 0x363a, 0x9f);
			if (ret < 0)
				return ret;
		} else {
			ret += sensor_write(sd, 0x3301, 0x48);
			ret += sensor_write(sd, 0x3630, 0x09);
			ret += sensor_write(sd, 0x3633, 0x46);
			ret += sensor_write(sd, 0x3622, 0x16);
			ret += sensor_write(sd, 0x363a, 0x9f);
			if (ret < 0)
				return ret;
		}

		ret += sensor_write(sd, 0x3e00, (unsigned char) ((it >> 12) & 0xf));
		ret += sensor_write(sd, 0x3e01, (unsigned char) ((it >> 4) & 0xff));
		ret += sensor_write(sd, 0x3e02, (unsigned char) ((it & 0x0f) << 4));

		ret = sensor_write(sd, 0x3e09, (unsigned char) (again & 0xff));
		ret += sensor_write(sd, 0x3e08, (unsigned char) (((again >> 8) & 0xff)));
	} else if (sensor->video.attr->data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) {
		if (again < 0x0720) {
			ret += sensor_write(sd, 0x3301, 0x20);
			ret += sensor_write(sd, 0x3630, 0x30);
			ret += sensor_write(sd, 0x3633, 0x34);
			ret += sensor_write(sd, 0x3622, 0xf6);
			ret += sensor_write(sd, 0x363a, 0x83);
			if (ret < 0)
				return ret;
		} else if (again >= 0x0720 && again < 0x0f20) {
			ret += sensor_write(sd, 0x3301, 0x28);
			ret += sensor_write(sd, 0x3630, 0x34);
			ret += sensor_write(sd, 0x3633, 0x35);
			ret += sensor_write(sd, 0x3622, 0xf6);
			ret += sensor_write(sd, 0x363a, 0x87);

			if (ret < 0)
				return ret;
		} else if (again >= 0x0f20 && again < 0x1f20) {
			ret += sensor_write(sd, 0x3301, 0x28);
			ret += sensor_write(sd, 0x3630, 0x24);
			ret += sensor_write(sd, 0x3633, 0x35);
			ret += sensor_write(sd, 0x3622, 0xf6);
			ret += sensor_write(sd, 0x363a, 0x9f);

			if (ret < 0)
				return ret;
		} else if (again >= 0x1f20 && again < 0x1f3e) {
			ret += sensor_write(sd, 0x3301, 0x48);
			ret += sensor_write(sd, 0x3630, 0x16);
			ret += sensor_write(sd, 0x3633, 0x45);
			ret += sensor_write(sd, 0x3622, 0xf6);
			ret += sensor_write(sd, 0x363a, 0x9f);

			if (ret < 0)
				return ret;
		} else {
			ret += sensor_write(sd, 0x3301, 0x48);
			ret += sensor_write(sd, 0x3630, 0x09);
			ret += sensor_write(sd, 0x3633, 0x46);
			ret += sensor_write(sd, 0x3622, 0x16);
			ret += sensor_write(sd, 0x363a, 0x9f);

			if (ret < 0)
				return ret;
		}

		// it = it <<1;
		it = (it << 1) - 1;
		ret += sensor_write(sd, 0x3e00, (unsigned char) ((it >> 12) & 0xf));
		ret += sensor_write(sd, 0x3e01, (unsigned char) ((it >> 4) & 0xff));
		ret += sensor_write(sd, 0x3e02, (unsigned char) ((it & 0x0f) << 4));
		ret = sensor_write(sd, 0x3e09, (unsigned char) (again & 0xff));
		ret += sensor_write(sd, 0x3e08, (unsigned char) (((again >> 8) & 0xff)));
		ret = sensor_write(sd, 0x3e13, (unsigned char) (again & 0xff));
		ret += sensor_write(sd, 0x3e12, (unsigned char) (((again >> 8) & 0xff)));
	}
	return 0;
}

/*
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
		if (value<0x0720)
		{
			ret += sensor_write(sd, 0x3301,0x20);
			ret += sensor_write(sd, 0x3630,0x30);
			ret += sensor_write(sd, 0x3633,0x34);
			ret += sensor_write(sd, 0x3622,0xf6);
			ret += sensor_write(sd, 0x363a,0x83);
			if (ret < 0)
					return ret;
		}
		else if (value>=0x0720 && value<0x0f20)
		{
			ret += sensor_write(sd, 0x3301,0x28);
			ret += sensor_write(sd, 0x3630,0x34);
			ret += sensor_write(sd, 0x3633,0x35);
			ret += sensor_write(sd, 0x3622,0xf6);
			ret += sensor_write(sd, 0x363a,0x87);
			if (ret < 0)
					return ret;
		}
		else if (value>=0x0f20 && value<0x1f20)
		{
			ret += sensor_write(sd, 0x3301,0x28);
			ret += sensor_write(sd, 0x3630,0x24);
			ret += sensor_write(sd, 0x3633,0x35);
			ret += sensor_write(sd, 0x3622,0xf6);
			ret += sensor_write(sd, 0x363a,0x9f);
			if (ret < 0)
					return ret;
		}
		else if (value>=0x1f20 && value<0x1f3e)
		{
			ret += sensor_write(sd, 0x3301,0x48);
			ret += sensor_write(sd, 0x3630,0x16);
			ret += sensor_write(sd, 0x3633,0x45);
			ret += sensor_write(sd, 0x3622,0xf6);
			ret += sensor_write(sd, 0x363a,0x9f);
			if (ret < 0)
					return ret;
		}
		else
		{
			ret += sensor_write(sd, 0x3301,0x48);
			ret += sensor_write(sd, 0x3630,0x09);
			ret += sensor_write(sd, 0x3633,0x46);
			ret += sensor_write(sd, 0x3622,0x16);
			ret += sensor_write(sd, 0x363a,0x9f);
			if (ret < 0)
					return ret;
		}
		ret += sensor_write(sd, 0x3e09, (unsigned char)(value & 0xff));
		ret += sensor_write(sd, 0x3e08, (unsigned char)((value & 0xff00) >> 8));
	   return 0;
}

*/
static int sensor_set_integration_time_short(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	value = (value << 1) - 1;
	ret = sensor_write(sd, 0x3e04, (unsigned char) ((value >> 4) & 0xff));
	ret = sensor_write(sd, 0x3e05, (unsigned char) (value & 0x0f) << 4);

	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_set_attr(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise) {

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

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {

	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable)
		return ISP_SUCCESS;

	sensor_set_attr(sd, wsize);
	sensor->video.state = TX_ISP_MODULE_INIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {

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
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("%s stream on\n", SENSOR_NAME);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
	} else {
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps) {
	//unsigned int short_time;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int max_fps;
	unsigned char val = 0;
	unsigned int newformat = 0; // the format is 24.8
	int ret = 0;

	switch (sensor->info.default_boot) {
		case 0:
			sclk = 1380 * 2000 * 30 * 4;
			max_fps = SENSOR_OUTPUT_MAX_FPS;
			break;
		case 1:
			sclk = 4000 * 1380 * 15 * 4;
			max_fps = 15;
		default:
			ISP_ERROR("Now we do not support this framerate!!!\n");
			break;
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}

	ret += sensor_read(sd, 0x320c, &val);
	hts = val;
	ret += sensor_read(sd, 0x320d, &val);
	hts = (hts << 8 | val) * 4;

	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = sensor_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	ret += sensor_write(sd, 0x320e, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}
	if (sensor->video.attr->data_type == TX_SENSOR_DATA_TYPE_LINEAR) {
		sensor->video.fps = fps;
		sensor->video.attr->max_integration_time_native = 2 * vts - 8;
		sensor->video.attr->integration_time_limit = 2 * vts - 8;
		sensor->video.attr->total_height = 2 * vts;
		sensor->video.attr->max_integration_time = 2 * vts - 8;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
		printk("this is set fps\n");
	} else if (sensor->video.attr->data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) {
		sensor->video.attr->max_integration_time_native = vts - 121 - 7;
		sensor->video.attr->integration_time_limit = vts - 121 - 7;
		sensor->video.attr->max_integration_time = vts - 121 - 7;
		sensor->video.fps = fps;
		sensor->video.attr->total_height = vts;
	}
	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value) {

	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
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
			sensor_write(sd, 0x3221, val & 0x99);
			break;
		case 1:
			sensor_write(sd, 0x3221, val | 0x06);
			break;
		case 2:
			sensor_write(sd, 0x3221, val | 0x60);
			break;
		case 3:
			sensor_write(sd, 0x3221, val | 0x66);
			break;
	}

	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_attr_check(struct tx_isp_subdev *sd) {

	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	unsigned long rate;
	int ret;

	switch (info->default_boot) {
		case 0:
			wsize = &sensor_win_sizes[0];
			memcpy(&(sensor_attr.mipi), &sensor_mipi, sizeof(sensor_mipi));
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.max_integration_time_native = 4000 - 8;
			sensor_attr.integration_time_limit = 4000 - 8;
			sensor_attr.total_width = 1380 * 2;         // 4460
			sensor_attr.total_height = 2000 * 2; // 2250
			sensor_attr.max_integration_time = 4000 - 8;
			sensor_attr.again = 0;
			sensor_attr.min_integration_time = 1;
			sensor_attr.integration_time = 4000 - 8;
			break;
		case 1:
			sensor_attr.wdr_cache = wdr_bufsize;
			wsize = &sensor_win_sizes[1];
			memcpy(&sensor_attr.mipi, &sensor_mipi_dol, sizeof(sensor_mipi_dol));
			sensor_attr.mipi.clk = 828;
			sensor_attr.min_integration_time = 1;
			sensor_attr.min_integration_time_short = 1;
			sensor_attr.total_width = 1380 * 2;
			sensor_attr.total_height = 4000 * 2;
			sensor_attr.max_integration_time_native = 4000 - 121 - 7; /* 2*3300 - 121-2 -18*/
			sensor_attr.integration_time_limit = 4000 - 121 - 7;
			sensor_attr.max_integration_time = 4000 - 121 - 7;
			sensor_attr.max_integration_time_short = 121 - 7; /*2*199 -14*/
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
			printk("=================> 15fps_hdr is ok");
			break;
		default:
			ISP_ERROR("Have no this Setting Source!!!\n");
	}

	switch (info->video_interface) {
		case TISP_SENSOR_VI_MIPI_CSI0:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.mipi.index = 0;
			break;
		case TISP_SENSOR_VI_DVP:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
			break;
		default:
			ISP_ERROR("Have no this Interface Source!!!\n");
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

	rate = private_clk_get_rate(sensor->mclk);
	switch (info->default_boot) {
		case 0:
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
		case 1:
			if (((rate / 1000) % 27000) != 0) {
				ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
				sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
				if (IS_ERR(sclka)) {
					pr_err("get sclka failed\n");
				} else {
					rate = private_clk_get_rate(sclka);
					if (((rate / 1000) % 27000) != 0) {
						private_clk_set_rate(sclka, 1188000000);
					}
				}
			}
			private_clk_set_rate(sensor->mclk, 24000000);
			private_clk_prepare_enable(sensor->mclk);
			break;
		case 2:
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

	ISP_WARNING("\n====>[default_boot=%d] [resolution=%dx%d] [video_interface=%d] [MCLK=%d] \n", info->default_boot,
		    wsize->width, wsize->height, info->video_interface, info->mclk);
	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;
	return 0;
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
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
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
	ISP_WARNING("sensor driver version %s\n", SENSOR_VERSION);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int sensor_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en) {
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = 0;
	ret = sensor_write(sd, 0x0103, 0x1);

	if (wdr_en == 1) {

		info->default_boot = 0;
		memcpy(&sensor_attr.mipi, &sensor_mipi_dol, sizeof(sensor_mipi_dol));
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		sensor_attr.wdr_cache = wdr_bufsize;
		wsize = &sensor_win_sizes[1];
		sensor_attr.min_integration_time = 1;
		sensor_attr.min_integration_time_short = 1;
		sensor_attr.total_width = 1380 * 2;
		sensor_attr.total_height = 4000 * 2;
		sensor_attr.max_integration_time_native = 4000 - 121 - 7;
		sensor_attr.integration_time_limit = 4000 - 121 - 7;
		sensor_attr.max_integration_time = 4000 - 121 - 7;
		sensor_attr.max_integration_time_short = 121 - 7;
	} else if (wdr_en == 0) {
		switch_wdr = info->default_boot;
		info->default_boot = 1;
		memcpy(&sensor_attr.mipi, &sensor_mipi, sizeof(sensor_mipi));
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		wsize = &sensor_win_sizes[0];
		sensor_attr.min_integration_time = 1;
		sensor_attr.total_width = 1380 * 2;
		sensor_attr.total_height = 2000 * 2;
		sensor_attr.max_integration_time_native = 4000 - 8;
		sensor_attr.integration_time_limit = 4000 - 8;
		sensor_attr.max_integration_time = 4000 - 8;
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int sensor_set_wdr(struct tx_isp_subdev *sd, int wdr_en) {
	int ret = 0;
	private_gpio_direction_output(reset_gpio, 1);
	private_msleep(1);
	private_gpio_direction_output(reset_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(reset_gpio, 1);
	private_msleep(1);

	ret = sensor_write_array(sd, wsize->regs);
	ret = sensor_write_array(sd, sensor_stream_on_mipi);

	return 0;
}

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
			/*
			case TX_ISP_EVENT_SENSOR_INT_TIME:
				if (arg)
				ret = sensor_set_integration_time(sd, sensor_val->value);
				break;
			case TX_ISP_EVENT_SENSOR_AGAIN:
				if (arg)
				ret = sensor_set_analog_gain(sd, sensor_val->value);
				break;
			*/
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
	// printk("this is ioctl\n");
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

static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id) {
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *) kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->dev = &client->dev;
	sensor_attr.expo_fs = 1;
	sensor->video.shvflip = 1;
	sensor->video.attr = &sensor_attr;
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
	private_devm_clk_put(&client->dev, sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{SENSOR_NAME, 0},
	{}};
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
