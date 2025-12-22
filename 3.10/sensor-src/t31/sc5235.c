// SPDX-License-Identifier: GPL-2.0+
/*
 * sc5235.c
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
#define SENSOR_NAME "sc5235"
#define SENSOR_VERSION "H20220914a"
#define SENSOR_CHIP_ID 0x5235
#define SENSOR_CHIP_ID_H (0x52)
#define SENSOR_CHIP_ID_L (0x35)

// ============================================================================
// HARDWARE INTERFACE
// ============================================================================
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x30

// ============================================================================
// SENSOR CAPABILITIES
// ============================================================================
#define SENSOR_MAX_WIDTH 2560
#define SENSOR_MAX_HEIGHT 1920

// ============================================================================
// REGISTER DEFINITIONS
// ============================================================================
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe

// ============================================================================
// TIMING AND PERFORMANCE
// ============================================================================
#define SENSOR_SUPPORT_SCLK_5M_FPS_15 (120000000)
#define SENSOR_SUPPORT_SCLK_4M_FPS_13 (64350000)
#define SENSOR_SUPPORT_SCLK_15FPS_HDR (0xbb8 * 0x546 * 30)
#define SENSOR_OUTPUT_MAX_FPS 15
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_1

typedef enum {
	SENSOR_RES_500 = 500,
	SENSOR_RES_400 = 400,
}sensor_resolution_mode;

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
module_param(data_type, int, S_IRUGO);
MODULE_PARM_DESC(data_type, "Sensor Date Type");

static int sensor_resolution = SENSOR_RES_500;
module_param(sensor_resolution, int, S_IRUGO);
MODULE_PARM_DESC(sensor_resolution, "Sensor Resolution set interface");

static int wdr_bufsize = 756000; /* 280 * hts * 2 */
module_param(wdr_bufsize, int, S_IRUGO);
MODULE_PARM_DESC(wdr_bufsize, "Wdr Buf Size");

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static unsigned int expo_val = 0x031f0320;

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
	{0x320, 0},
	{0x321, 2886},
	{0x322, 5776},
	{0x323, 8494},
	{0x324, 11136},
	{0x325, 13706},
	{0x326, 16287},
	{0x327, 18723},
	{0x328, 21097},
	{0x329, 23413},
	{0x32a, 25746},
	{0x32b, 27952},
	{0x32c, 30108},
	{0x32d, 32216},
	{0x32e, 34344},
	{0x32f, 36361},
	{0x330, 38335},
	{0x331, 40269},
	{0x332, 42225},
	{0x333, 44082},
	{0x334, 45903},
	{0x335, 47689},
	{0x336, 49499},
	{0x337, 51220},
	{0x338, 52910},
	{0x339, 54570},
	{0x33a, 56253},
	{0x33b, 57856},
	{0x33c, 59433},
	{0x33d, 60983},
	{0x33e, 62557},
	{0x33f, 64058},
	{0x720, 65535},
	{0x721, 68467},
	{0x722, 71266},
	{0x723, 74029},
	{0x724, 76671},
	{0x725, 79281},
	{0x726, 81782},
	{0x727, 84258},
	{0x728, 86632},
	{0x729, 88985},
	{0x72a, 91245},
	{0x72b, 93487},
	{0x72c, 95643},
	{0x72d, 97785},
	{0x72e, 99846},
	{0x72f, 101896},
	{0x730, 103870},
	{0x731, 105835},
	{0x732, 107730},
	{0x733, 109617},
	{0x734, 111438},
	{0x735, 113253},
	{0x736, 115006},
	{0x737, 116755},
	{0x738, 118445},
	{0x739, 120131},
	{0x73a, 121762},
	{0x73b, 123391},
	{0x73c, 124968},
	{0x73d, 126543},
	{0x73e, 128068},
	{0x73f, 129593},
	{0xf20, 131070},
	{0xf21, 133979},
	{0xf22, 136801},
	{0xf23, 139542},
	{0xf24, 142206},
	{0xf25, 144796},
	{0xf26, 147317},
	{0xf27, 149773},
	{0xf28, 152167},
	{0xf29, 154502},
	{0xf2a, 156780},
	{0xf2b, 159005},
	{0xf2c, 161178},
	{0xf2d, 163303},
	{0xf2e, 165381},
	{0xf2f, 167414},
	{0xf30, 169405},
	{0xf31, 171355},
	{0xf32, 173265},
	{0xf33, 175137},
	{0xf34, 176973},
	{0xf35, 178774},
	{0xf36, 180541},
	{0xf37, 182276},
	{0xf38, 183980},
	{0xf39, 185653},
	{0xf3a, 187297},
	{0xf3b, 188914},
	{0xf3c, 190503},
	{0xf3d, 192065},
	{0xf3e, 193603},
	{0xf3f, 195116},
	{0x1f20, 196605},
	{0x1f21, 199514},
	{0x1f22, 202336},
	{0x1f23, 205077},
	{0x1f24, 207741},
	{0x1f25, 210331},
	{0x1f26, 212852},
	{0x1f27, 215308},
	{0x1f28, 217702},
	{0x1f29, 220037},
	{0x1f2a, 222315},
	{0x1f2b, 224540},
	{0x1f2c, 226713},
	{0x1f2d, 228838},
	{0x1f2e, 230916},
	{0x1f2f, 232949},
	{0x1f30, 234940},
	{0x1f31, 236890},
	{0x1f32, 238800},
	{0x1f33, 240672},
	{0x1f34, 242508},
	{0x1f35, 244309},
	{0x1f36, 246076},
	{0x1f37, 247811},
	{0x1f38, 249515},
	{0x1f39, 251188},
	{0x1f3a, 252832},
	{0x1f3b, 254449},
	{0x1f3c, 256038},
	{0x1f3d, 257600},
	{0x1f3e, 259138},
	{0x1f3f, 260651}
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

unsigned int sensor_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
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

struct tx_isp_mipi_bus sensor_mipi_wdr = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.settle_time_apative_en = 1,
	.clk = 608,
	.lans = 2,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 2304,
	.image_theight = 1296,
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
	.clk = 600,
	.lans = 2,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
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

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL,
	.max_again = 260651,
	.max_dgain = 0,
	.min_integration_time = 3,
	.min_integration_time_native = 3,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_again_short = sensor_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};


static struct regval_list sensor_init_regs_2560_1920_15fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x3039, 0xa4},
	{0x3029, 0xa7},
	{0x302a, 0x36},
	{0x302b, 0x10},
	{0x302c, 0x00},
	{0x302d, 0x03},
	{0x3038, 0x44},
	{0x303a, 0x3b},
	{0x303d, 0x20},
	{0x3200, 0x00},
	{0x3201, 0x14},
	{0x3202, 0x00},
	{0x3203, 0x10},
	{0x3204, 0x0a},
	{0x3205, 0x1c},
	{0x3206, 0x07},
	{0x3207, 0x97},
	{0x3208, 0x0a},
	{0x3209, 0x00},
	{0x320a, 0x07},
	{0x320b, 0x80},
	{0x320c, 0x05},/*hts*/
	{0x320d, 0xdc},//
	{0x320e, 0x0a},//
	{0x320f, 0x6a},/*vts =2666 for 15fps*/
	{0x3211, 0x04},
	{0x3213, 0x04},
	{0x3235, 0x0f},
	{0x3236, 0x9e},
	{0x3301, 0x1c},
	{0x3303, 0x28},
	{0x3304, 0x10},
	{0x3306, 0x50},
	{0x3308, 0x10},
	{0x3309, 0x70},
	{0x330a, 0x00},
	{0x330b, 0xb8},
	{0x330e, 0x20},
	{0x3314, 0x14},
	{0x3315, 0x02},
	{0x331b, 0x83},
	{0x331e, 0x19},
	{0x331f, 0x61},
	{0x3320, 0x01},
	{0x3321, 0x04},
	{0x3326, 0x00},
	{0x3333, 0x20},
	{0x3334, 0x40},
	{0x3364, 0x05},
	{0x3366, 0x78},
	{0x3367, 0x08},
	{0x3368, 0x03},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x336c, 0x01},
	{0x336d, 0x40},
	{0x337f, 0x03},
	{0x338f, 0x40},
	{0x33b6, 0x07},
	{0x33b7, 0x17},
	{0x33b8, 0x20},
	{0x33b9, 0x20},
	{0x33ba, 0x44},
	{0x3620, 0x28},
	{0x3621, 0xac},
	{0x3622, 0xf6},
	{0x3623, 0x10},
	{0x3624, 0x47},
	{0x3625, 0x0b},
	{0x3630, 0x30},
	{0x3631, 0x88},
	{0x3632, 0x18},
	{0x3633, 0x23},
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
	{0x3802, 0x00},/*1:group hold delay 1frm*/
	{0x3905, 0x98},
	{0x3907, 0x01},
	{0x3908, 0x11},
	{0x390a, 0x00},
	{0x391c, 0x9f},
	{0x391d, 0x00},
	{0x391e, 0x01},
	{0x391f, 0xc0},
	{0x3e00, 0x00},
	{0x3e01, 0xf9},
	{0x3e02, 0x80},
	{0x3e03, 0x0b},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x20},
	{0x3e1e, 0x30},
	{0x3e26, 0x20},
	{0x3f00, 0x0d},
	{0x3f04, 0x02},
	{0x3f05, 0xe6},
	{0x3f08, 0x04},
	{0x4500, 0x5d},
	{0x4509, 0x10},
	{0x4809, 0x01},
	{0x4837, 0x21},
	{0x5000, 0x06},
	{0x5002, 0x06},
	{0x5780, 0x7f},
	{0x5781, 0x06},
	{0x5782, 0x04},
	{0x5783, 0x00},
	{0x5784, 0x00},
	{0x5785, 0x16},
	{0x5786, 0x12},
	{0x5787, 0x08},
	{0x5788, 0x02},
	{0x578b, 0x07},
	{0x57a0, 0x00},
	{0x57a1, 0x72},
	{0x57a2, 0x01},
	{0x57a3, 0xf2},
	{0x6000, 0x20},
	{0x6002, 0x00},
	{0x3039, 0x24},
	{0x3029, 0x27},
	{0x0100, 0x01},
	{SENSOR_REG_DELAY, 0x10},

	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_2304_1296_15fps_mipi_dol[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x3039, 0x80},
	{0x3029, 0x80},
	{0x301f, 0x3c},
	{0x302a, 0x25},
	{0x302b, 0x10},
	{0x302c, 0x01},
	{0x302d, 0x00},
	{0x3038, 0x44},
	{0x303a, 0x77},
	{0x303d, 0x23},
	{0x3200, 0x00},
	{0x3201, 0x94},
	{0x3202, 0x00},
	{0x3203, 0x00},
	{0x3204, 0x09},
	{0x3205, 0x9b},
	{0x3206, 0x05},
	{0x3207, 0xaf},
	{0x3208, 0x09},
	{0x3209, 0x00},
	{0x320a, 0x05},
	{0x320b, 0x10},
	{0x320c, 0x05},/*hts*/
	{0x320d, 0x46},//
	{0x320e, 0x0b},/*vts*/
	{0x320f, 0xb8},//
	{0x3210, 0x00},
	{0x3211, 0x04},
	{0x3212, 0x00},
	{0x3213, 0x04},
	{0x3220, 0x50},
	{0x3235, 0x17},
	{0x3236, 0x6e},
	{0x3301, 0x1c},
	{0x3303, 0x28},
	{0x3304, 0x10},
	{0x3306, 0x50},
	{0x3307, 0x14},
	{0x3308, 0x10},
	{0x3309, 0x60},
	{0x330a, 0x00},
	{0x330b, 0xb8},
	{0x330d, 0x30},
	{0x330e, 0x20},
	{0x330f, 0x04},
	{0x3310, 0x51},
	{0x3314, 0x14},
	{0x3315, 0x02},
	{0x3318, 0x42},
	{0x3319, 0x82},
	{0x331b, 0x83},
	{0x331e, 0x19},
	{0x331f, 0x51},
	{0x3320, 0x01},
	{0x3321, 0x04},
	{0x3326, 0x00},
	{0x3332, 0x22},
	{0x3333, 0x20},
	{0x3334, 0x40},
	{0x3339, 0x44},
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
	{0x3633, 0x23},
	{0x3634, 0x86},
	{0x3635, 0x4d},
	{0x3636, 0x21},
	{0x3637, 0x20},
	{0x3638, 0x1a},
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
	{0x3802, 0x01},
	{0x3905, 0x98},
	{0x3907, 0x01},
	{0x3908, 0x11},
	{0x390a, 0x00},
	{0x391c, 0x9f},
	{0x391d, 0x00},
	{0x391e, 0x01},
	{0x391f, 0xc0},
	{0x3988, 0x11},
	{0x3e00, 0x00},
	{0x3e01, 0xf0},
	{0x3e02, 0x00},
	{0x3e03, 0x0b},
	{0x3e04, 0x00},//
	{0x3e05, 0xf0},//
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x20},
	{0x3e1e, 0x30},
	{0x3e23, 0x00},
	{0x3e24, 0xf0},
	{0x3e26, 0x20},
	{0x3f00, 0x0d},
	{0x3f02, 0x05},
	{0x3f04, 0x02},
	{0x3f05, 0x9b},
	{0x3f06, 0x21},
	{0x3f08, 0x04},
	{0x4500, 0x5d},
	{0x4502, 0x10},
	{0x4509, 0x10},
	{0x4602, 0x0f},
	{0x4809, 0x01},
	{0x4816, 0x51},
	{0x4837, 0x20},
	{0x5000, 0x06},
	{0x5002, 0x06},
	{0x5780, 0x7f},
	{0x5781, 0x06},
	{0x5782, 0x04},
	{0x5783, 0x00},
	{0x5784, 0x00},
	{0x5785, 0x16},
	{0x5786, 0x12},
	{0x5787, 0x08},
	{0x5788, 0x02},
	{0x578b, 0x07},
	{0x57a0, 0x00},
	{0x57a1, 0x72},
	{0x57a2, 0x01},
	{0x57a3, 0xf2},
	{0x6000, 0x20},
	{0x6002, 0x00},
	{0x3039, 0x51},
	{0x3029, 0x51},
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_2560_1440_13fps_mipi[] ={
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x3039, 0x80},
	{0x3029, 0x80},

	{0x302a, 0x75},
	{0x302b, 0x10},
	{0x302c, 0x03},
	{0x3037, 0x22},
	{0x3038, 0x22},

	{0x303a, 0x75},
	{0x303b, 0x1a},
	{0x303d, 0x23},
	{0x3200, 0x00},
	{0x3201, 0x0c},
	{0x3202, 0x00},
	{0x3203, 0xfc},
	{0x3204, 0x0a},
	{0x3205, 0x23},
	{0x3206, 0x06},
	{0x3207, 0xab},
	{0x3208, 0x0a},
	{0x3209, 0x00},
	{0x320a, 0x05},
	{0x320b, 0xa0},
	{0x320c, 0x06},
	{0x320d, 0x72},
	{0x320e, 0x05},
	{0x320f, 0xdc},
	{0x3211, 0x0c},
	{0x3213, 0x08},
	{0x3235, 0x0b},
	{0x3236, 0xb6},
	{0x3301, 0x10},
	{0x3303, 0x18},
	{0x3304, 0x10},
	{0x3306, 0x30},
	{0x3308, 0x10},
	{0x3309, 0x44},
	{0x330a, 0x00},
	{0x330b, 0x80},
	{0x330d, 0x30},
	{0x330e, 0x20},
	{0x3314, 0x14},
	{0x3315, 0x02},
	{0x331b, 0x83},
	{0x331e, 0x11},
	{0x331f, 0x3d},
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
	{0x3366, 0x80},
	{0x3367, 0x08},
	{0x3368, 0x03},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x336c, 0x01},
	{0x336d, 0x40},
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
	{0x3620, 0x28},
	{0x3621, 0xac},
	{0x3622, 0xf6},
	{0x3623, 0x08},
	{0x3624, 0x47},
	{0x3625, 0x0b},
	{0x3630, 0x30},
	{0x3631, 0x88},
	{0x3632, 0x18},
	{0x3633, 0x23},
	{0x3634, 0x86},
	{0x3635, 0x4d},
	{0x3636, 0x21},
	{0x3637, 0x20},
	{0x3638, 0x1a},
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
	{0x3802, 0x01},
	{0x3905, 0x98},
	{0x3907, 0x01},
	{0x3908, 0x11},
	{0x390a, 0x00},
	{0x391c, 0x9f},
	{0x391d, 0x00},
	{0x391e, 0x01},
	{0x391f, 0xc0},
	{0x3e00, 0x00},
	{0x3e01, 0xbb},
	{0x3e02, 0x00},
	{0x3e03, 0x0b},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x20},
	{0x3e1e, 0x30},
	{0x3e26, 0x20},
	{0x3f00, 0x0d},
	{0x3f02, 0x05},
	{0x3f04, 0x03},
	{0x3f05, 0x31},
	{0x3f06, 0x21},
	{0x3f08, 0x04},
	{0x4500, 0x5d},
	{0x4502, 0x10},
	{0x4509, 0x10},
	{0x4809, 0x01},
	{0x4837, 0x35},
	{0x5000, 0x06},
	{0x5002, 0x06},
	{0x5780, 0x7f},
	{0x5781, 0x06},
	{0x5782, 0x04},
	{0x5783, 0x00},
	{0x5784, 0x00},
	{0x5785, 0x16},
	{0x5786, 0x12},
	{0x5787, 0x08},
	{0x5788, 0x02},
	{0x578b, 0x07},
	{0x57a0, 0x00},
	{0x57a1, 0x71},
	{0x57a2, 0x01},
	{0x57a3, 0xf1},
	{0x6000, 0x20},
	{0x6002, 0x00},
	{0x3039, 0x51},
	{0x3029, 0x51},
	{0x0100, 0x01},

	{0x3301, 0x10},  //[c,12]    20180904
	{0x3630, 0x30},  //20180706
	{0x3633, 0x23},//20180419
	{0x3622, 0xf6},
	{0x363a, 0x83},

	{0x3301, 0x16},  //[10,18]   20180904
	{0x3630, 0x23},  //20180327
	{0x3633, 0x33},//20180419
	{0x3622, 0xf6},
	{0x363a, 0x87},

	{0x3301, 0x1a}, //[12,1a]     20180904
	{0x3630, 0x24}, //1114
	{0x3633, 0x33},//20180419
	{0x3622, 0xf6},
	{0x363a, 0x9f},

	{0x3301, 0x30}, //[15,a2]      20180904
	{0x3630, 0x16},
	{0x3633, 0x33},//20180419
	{0x3622, 0xf6},
	{0x363a, 0x9f},

	{0x3301, 0x44}, //  [13,a2]    20180904
	{0x3630, 0x19}, //0f  1226
	{0x3633, 0x45}, //0627
	{0x3622, 0x16}, //0627
	{0x363a, 0x9f},
	{SENSOR_REG_DELAY, 0x10},
	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 2560*1920 */
	{
		.width = 2560,
		.height = 1920,
		.fps = 15 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2560_1920_15fps_mipi,
	},
	/* 2304*1296 */
	{
		.width = 2304,
		.height = 1296,
		.fps = 15 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2304_1296_15fps_mipi_dol,
	},
	{
		.width = 2560,
		.height = 1440,
		.fps = 13 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2560_1440_13fps_mipi,
	}
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

/*
static int sensor_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = -1;
	int it = (value & 0xffff) * 2;
	int again = (value & 0xffff0000) >> 16;

	if (value != expo_val) {
		ret += sensor_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
		ret += sensor_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
		ret += sensor_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
		ret = sensor_write(sd, 0x3e09, (unsigned char)(again & 0xff));
		ret += sensor_write(sd, 0x3e08, (unsigned char)(((again >> 8) & 0xff)));

		ret += sensor_write(sd, 0x3812, 0x00);
		if (again < 0x720) {
			ret += sensor_write(sd, 0x3301, 0x1c);
			ret += sensor_write(sd, 0x3630, 0x30);
			ret += sensor_write(sd, 0x3633, 0x23);
			ret += sensor_write(sd, 0x3622, 0xf6);
			ret += sensor_write(sd, 0x363a, 0x83);
		} else if (again < 0xf20) {
			ret += sensor_write(sd, 0x3301, 0x26);
			ret += sensor_write(sd, 0x3630, 0x23);
			ret += sensor_write(sd, 0x3633, 0x33);
			ret += sensor_write(sd, 0x3622, 0xf6);
			ret += sensor_write(sd, 0x363a, 0x87);
		} else if (again < 0x1f20) {
			ret += sensor_write(sd, 0x3301, 0x2c);
			ret += sensor_write(sd, 0x3630, 0x24);
			ret += sensor_write(sd, 0x3633, 0x43);
			ret += sensor_write(sd, 0x3622, 0xf6);
			ret += sensor_write(sd, 0x363a, 0x9f);
		} else if (again < 0x1f3f) {
			ret += sensor_write(sd, 0x3301, 0x38);
			ret += sensor_write(sd, 0x3630, 0x28);
			ret += sensor_write(sd, 0x3633, 0x43);
			ret += sensor_write(sd, 0x3622, 0xf6);
			ret += sensor_write(sd, 0x363a, 0x9f);
		} else {
			ret += sensor_write(sd, 0x3301, 0x44);
			ret += sensor_write(sd, 0x3630, 0x19);
			ret += sensor_write(sd, 0x3633, 0x55);
			ret += sensor_write(sd, 0x3622, 0x16);
			ret += sensor_write(sd, 0x363a, 0x9f);
		}
		ret += sensor_write(sd, 0x3812, 0x30);
		if (ret < 0)
			return ret;
	}
	expo_val = value;

	return 0;
}
*/

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
	ret += sensor_write(sd, 0x3812, 0x00);

	if (value < 0x720) {
		ret += sensor_write(sd, 0x3301, 0x1c);
		ret += sensor_write(sd, 0x3630, 0x30);
		ret += sensor_write(sd, 0x3633, 0x23);
		ret += sensor_write(sd, 0x3622, 0xf6);
		ret += sensor_write(sd, 0x363a, 0x83);
	} else if (value < 0xf20) {
		ret += sensor_write(sd, 0x3301, 0x26);
		ret += sensor_write(sd, 0x3630, 0x23);
		ret += sensor_write(sd, 0x3633, 0x33);
		ret += sensor_write(sd, 0x3622, 0xf6);
		ret += sensor_write(sd, 0x363a, 0x87);
	} else if (value < 0x1f20) {
		ret += sensor_write(sd, 0x3301, 0x2c);
		ret += sensor_write(sd, 0x3630, 0x24);
		ret += sensor_write(sd, 0x3633, 0x43);
		ret += sensor_write(sd, 0x3622, 0xf6);
		ret += sensor_write(sd, 0x363a, 0x9f);
	} else if (value < 0x1f3f) {
		ret += sensor_write(sd, 0x3301, 0x38);
		ret += sensor_write(sd, 0x3630, 0x28);
		ret += sensor_write(sd, 0x3633, 0x43);
		ret += sensor_write(sd, 0x3622, 0xf6);
		ret += sensor_write(sd, 0x363a, 0x9f);
	} else {
		ret += sensor_write(sd, 0x3301, 0x44);
		ret += sensor_write(sd, 0x3630, 0x19);
		ret += sensor_write(sd, 0x3633, 0x55);
		ret += sensor_write(sd, 0x3622, 0x16);
		ret += sensor_write(sd, 0x363a, 0x9f);
	}
	ret += sensor_write(sd, 0x3812, 0x30);
	if (ret < 0)
		return ret;

	expo_val = value;

	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
//	printk("\n-----------------------------> %s [%d] <---------------------------\n", __func__, __LINE__);
	int ret = 0;
	unsigned int expo = 0;

	expo = value * 1;
	ret += sensor_write(sd, 0x3e04, (unsigned char)((expo >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e05, (unsigned char)((expo & 0x0f) << 4));

	return 0;
}

static int sensor_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
//	printk("\n-----------------------------> %s [%d] <---------------------------\n", __func__, __LINE__);
	int ret = 0;

	ret = sensor_write(sd, 0x3e13, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3e12, (unsigned char)(value >> 8 & 0x3f));

	return 0;
}

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
	int ret = 0;
	private_msleep(5);

	ret = sensor_write_array(sd, wsize->regs);
	ret = sensor_write(sd, 0x3e00, 0);
	ret = sensor_write(sd, 0x3e01, 0);
	ret = sensor_write(sd, 0x3e02, 0);
	ret = sensor_write(sd, 0x3e04, 0);
	ret = sensor_write(sd, 0x3e05, 0);
	ret = sensor_write_array(sd, sensor_stream_on_mipi);

	return 0;
}

static int sensor_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = 0;

	ret = sensor_write(sd, 0x3e00, 0);
	ret = sensor_write(sd, 0x3e01, 0);
	ret = sensor_write(sd, 0x3e02, 0);
	ret = sensor_write(sd, 0x3e04, 0);
	ret = sensor_write(sd, 0x3e05, 0);

	if (wdr_en == 1) {
		wsize = &sensor_win_sizes[1];
		sensor_info.max_fps = 15;
		data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_wdr),sizeof(sensor_mipi_wdr));
		sensor_attr.data_type = data_type;
		sensor_attr.wdr_cache = wdr_bufsize;
		sensor_attr.one_line_expr_in_us = 25;
		sensor_attr.total_width = 0x546;
		sensor_attr.total_height = 0xbb8;
		sensor_attr.max_integration_time_native = 2880;
		sensor_attr.integration_time_limit = 2880;
		sensor_attr.max_integration_time = 2880;
		sensor_attr.max_integration_time_short = 240;
		sensor_attr.min_integration_time = 3;
		sensor_attr.min_integration_time_short = 3;
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		printk("------------> switch wdr ok <-------------\n");
	} else if (wdr_en == 0) {
		wsize = &sensor_win_sizes[0];
		sensor_info.max_fps = 15;
		data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_linear),sizeof(sensor_mipi_linear));
		sensor_attr.data_type = data_type;
		sensor_attr.one_line_expr_in_us = 25;
		sensor_attr.max_integration_time_native = 2662;
		sensor_attr.integration_time_limit = 2662;
		sensor_attr.total_width = 3000;
		sensor_attr.total_height = 2662;
		sensor_attr.max_integration_time = 2662;
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		printk("------------> switch linear ok <-------------\n");
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;

	sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);
	sensor->video.attr = &sensor_attr;

	return ret;

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

	if (data_type == TX_SENSOR_DATA_TYPE_LINEAR) {
		switch (sensor_resolution) {
			case SENSOR_RES_500:
				sclk = SENSOR_SUPPORT_SCLK_5M_FPS_15 ;
				break;
			case SENSOR_RES_400:
				sclk = SENSOR_SUPPORT_SCLK_4M_FPS_13 ;
				break;
			default:
				ISP_WARNING("Sensor selection failed to enable default configuration.\n");
				break;
		}
	} else if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) {
		sclk = SENSOR_SUPPORT_SCLK_15FPS_HDR;
	}
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

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val = 0x01;

	ret += sensor_read(sd, 0x3221, &val);
	if (enable & 0x02) {
		val |= 0x60;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SBGGR10_1X10;
	} else {
		val &= 0x9f;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SBGGR10_1X10;
	}
	sensor->video.mbus_change = 1;
	ret += sensor_write(sd, 0x3221, val);
	if (ret < 0)
		return -1;
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

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;

//	return 0;
	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
	case TX_ISP_EVENT_SENSOR_EXPO:
//		if (arg)
//			ret = sensor_set_expo(sd, *(int*)arg);
		break;
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
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if (arg)
			ret = sensor_set_integration_time_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if (arg)
			ret = sensor_set_analog_gain_short(sd, *(int*)arg);
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
	case TX_ISP_EVENT_SENSOR_WDR:
		if (arg)
			ret = sensor_set_wdr(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if (arg)
			ret = sensor_set_wdr_stop(sd, *(int*)arg);
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

	if (data_type == TX_SENSOR_DATA_TYPE_LINEAR) {
		if ((data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) && (sensor_resolution == SENSOR_RES_500)) {
			wsize = &sensor_win_sizes[0];
			sensor_info.max_fps = 15;
			memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_linear),sizeof(sensor_mipi_linear));
			sensor_attr.max_integration_time_native = 2662;
			sensor_attr.integration_time_limit = 2662;
			sensor_attr.total_width = 3000;
			sensor_attr.total_height = 2662;
			sensor_attr.max_integration_time = 2662;
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			printk("------------> linear is ok <-------------\n");
		} else if ((data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) && (sensor_resolution == SENSOR_RES_400)) {
			wsize = &sensor_win_sizes[2];
			sensor_info.max_fps = 13;
			memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_linear),sizeof(sensor_mipi_linear));
			sensor_attr.max_integration_time_native = 1650;
			sensor_attr.integration_time_limit = 1650;
			sensor_attr.total_width = 3000;
			sensor_attr.total_height = 1650;
			sensor_attr.max_integration_time = 1650;
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			printk("------------> linear is ok <-------------\n");
		} else {
			ISP_ERROR("Can not support this data type!!!\n");
		}
	} else if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) {
		sensor_attr.wdr_cache = wdr_bufsize;
		wsize = &sensor_win_sizes[1];
		sensor_info.max_fps = 15;
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_wdr),sizeof(sensor_mipi_wdr));
		sensor_attr.one_line_expr_in_us = 25;
		sensor_attr.max_again = 260651;
		sensor_attr.max_again_short = 260651;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 3;
		sensor_attr.min_integration_time_short = 3;
		sensor_attr.max_integration_time_short = 240;
		sensor_attr.max_integration_time_native = 2880;
		sensor_attr.integration_time_limit = 2880;
		sensor_attr.max_integration_time = 2880;
		sensor_attr.total_width = 0x546;
		sensor_attr.total_height = 0xbb8;
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		printk("------------> wdr is ok <-------------\n");
	} else {
		ISP_ERROR("Can not support this data type!!!\n");
	}

	/*
	  convert sensor-gain into isp-gain,
	*/
	sd = &sensor->sd;
	video = &sensor->video;
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

	sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	ISP_WARNING("probe ok ------->%s\n", SENSOR_NAME);

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
