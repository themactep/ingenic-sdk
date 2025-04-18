// SPDX-License-Identifier: GPL-2.0+
/*
 * ps5260.c
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

#define SENSOR_NAME "ps5260"
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x48
#define SENSOR_MAX_WIDTH 1920
#define SENSOR_MAX_HEIGHT 1080
#define SENSOR_CHIP_ID 0x5260
#define SENSOR_CHIP_ID_H (0x52)
#define SENSOR_CHIP_ID_L (0x60)
#define SENSOR_REG_END 0xff
#define SENSOR_REG_DELAY 0xfe
#define SENSOR_BANK_REG 0xef
#define SENSOR_SUPPORT_PCLK_DVP (38002500)
#define SENSOR_SUPPORT_PCLK_MIPI (152010000)
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define AG_HS_MODE (32)	// 4.0x
#define AG_LS_MODE (24)	// 3.0x
#define NEPLS_LB (25)
#define NEPLS_UB (200)
#define NEPLS_SCALE (38)
#define NE_NEP_CONST_LINEAR (0x868+0x19)
#define NE_NEP_CONST_HDR (0x1134+0x50)
#define SENSOR_VERSION "H20210705a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_HIGH_10BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

static int data_interface = TX_SENSOR_DATA_INTERFACE_DVP;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static struct sensor_info sensor_info = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.version = SENSOR_VERSION,
	.min_fps = SENSOR_OUTPUT_MIN_FPS,
	.max_fps = SENSOR_OUTPUT_MAX_FPS,
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
	{0, 0     },
	{1, 5732  },
	{2, 11136 },
	{3, 16248 },
	{4, 21098 },
	{5, 25711 },
	{6, 30109 },
	{7, 34312 },
	{8, 38336 },
	{9, 42196 },
	{10, 45904 },
	{11, 49472 },
	{12, 52911 },
	{13, 56229 },
	{14, 59434 },
	{15, 62534 },
	{16, 65536 },
	{17, 71268 },
	{18, 76672 },
	{19, 81784 },
	{20, 86634 },
	{21, 91247 },
	{22, 95645 },
	{23, 99848 },
	{24, 103872},
	{25, 107732},
	{26, 111440},
	{27, 115008},
	{28, 118447},
	{29, 121765},
	{30, 124970},
	{31, 128070},
	{32, 131072},
	{33, 136804},
	{34, 142208},
	{35, 147320},
	{36, 152170},
	{37, 156783},
	{38, 161181},
	{39, 165384},
	{40, 169408},
	{41, 173268},
	{42, 176976},
	{43, 180544},
	{44, 183983},
	{45, 187301},
	{46, 190506},
	{47, 193606},
	{48, 196608},
	{49, 202340},
	{50, 207744},
	{51, 212856},
	{52, 217706},
	{53, 222319},
	{54, 226717},
	{55, 230920},
	{56, 234944},
	{57, 238804},
	{58, 242512},
	{59, 246080},
	{60, 249519},
	{61, 252837},
	{62, 256042},
	{63, 259142},
	{64, 262144},
	{65, 267876},
	{66, 273280},
	{67, 278392},
	{68, 283242},
	{69, 287855},
	{70, 292253},
	{71, 296456},
	{72, 300480},
	{73, 304340},
	{74, 308048},
	{75, 311616},
	{76, 315055},
	{77, 318373},
	{78, 321578},
	{79, 324678},
	{80, 327680},
	{81, 333412},
	{82, 338816},
	{83, 343928},
	{84, 348778},
	{85, 353391},
	{86, 357789},
	{87, 361992},
	{88, 366016},
	{89, 369876},
	{90, 373584},
	{91, 377152},
	{92, 380591},
	{93, 383909},
	{94, 387114},
	{95, 390214},
	{96, 393216},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain <= sensor_again_lut[2].gain) {
			*sensor_again = lut[2].value;
			return lut[2].gain;
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

struct tx_isp_mipi_bus sensor_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 648,
	.lans = 2,
	.settle_time_apative_en = 1,
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
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
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
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
	},
	.max_again = 393216,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1122,
	.integration_time_limit = 1122,
	.total_width = 2252,
	.total_height = 1125,
	.max_integration_time = 1122,
	.one_line_expr_in_us = 59,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};


static struct regval_list sensor_init_regs_1920_1080_15fps[] = {
	{0xEF, 0x00},
	{0x11, 0x80},
	{0x13, 0x01},
	{0x16, 0xBC},
	{0x38, 0x28},
	{0x3C, 0x02},
	{0x5F, 0x64},
	{0x61, 0xDE},
	{0x62, 0x14},
	{0x69, 0x10},
	{0x75, 0x0B},
	{0x77, 0x06},
	{0x7E, 0x19},
	{0x85, 0x88},
	{0x9E, 0x00},
	{0xA0, 0x01},
	{0xA2, 0x09},
	{0xBE, 0x15},// by ISP
	{0xBF, 0x80},
	{0xDF, 0x1E},
	{0xE1, 0x05},
	{0xE2, 0x03},
	{0xE3, 0x20},
	{0xE4, 0x0F},
	{0xE5, 0x07},
	{0xE6, 0x05},
	{0xF3, 0xB1},
	{0xF8, 0x5A},
	{0xED, 0x01},
	{0xEF, 0x01},
	{0x05, 0x01},
	{0x07, 0x01},
	{0x08, 0x46},
	{0x0A, 0x04},
	{0x0B, 0x64},
	{0x0C, 0x00},
	{0x0D, 0x03},
	{0x0E, 0x08},
	{0x0F, 0x68},
	{0x18, 0x01},
	{0x19, 0x23},
	{0x27, 0x08},
	{0x28, 0xCC},
	{0x37, 0x90},
	{0x3A, 0xF0},
	{0x3B, 0x20},
	{0x40, 0xF0},
	{0x42, 0xD6},
	{0x43, 0x20},
	{0x5C, 0x1E},
	{0x5D, 0x0A},
	{0x5E, 0x32},
	{0x67, 0x11},
	{0x68, 0xF4},
	{0x69, 0xC2},
	{0x7A, 0x00},
	{0x7B, 0x98},
	{0x7C, 0x07},
	{0x7D, 0xA0},
	{0x83, 0x02},
	{0x8F, 0x00},
	{0x90, 0x01},
	{0x92, 0x80},
	{0xA3, 0x00},
	{0xA4, 0x0E},
	{0xA5, 0x04},
	{0xA6, 0x38},
	{0xA7, 0x00},
	{0xA8, 0x00},
	{0xA9, 0x07},
	{0xAA, 0x80},
	{0xAB, 0x04},
	{0xAE, 0x28},
	{0xB0, 0x28},
	{0xB3, 0x0A},
	{0xB6, 0x08},
	{0xB7, 0x68},
	{0xB8, 0x04},
	{0xB9, 0xE4},
	{0xBE, 0x00},
	{0xBF, 0x00},
	{0xC0, 0x00},
	{0xC1, 0x00},
	{0xC4, 0x60},
	{0xC6, 0x60},
	{0xC7, 0x0A},
	{0xC8, 0xC8},
	{0xC9, 0x35},
	{0xCE, 0x6C},
	{0xCF, 0x82},
	{0xD0, 0x02},
	{0xD1, 0x60},
	{0xD5, 0x49},
	{0xD7, 0x0A},
	{0xD8, 0xE8},
	{0xDA, 0xC0},
	{0xDD, 0x42},
	{0xDE, 0x43},
	{0xE2, 0x9A},
	{0xE4, 0x00},
	{0xEA, 0x4A},/*improve high temperature performance*/
	{0xF0, 0x7C},
	{0xF1, 0x16},
	{0xF2, 0x24},
	{0xF3, 0x0C},
	{0xF4, 0x01},
	{0xF5, 0x19},
	{0xF6, 0x16},
	{0xF7, 0x00},
	{0xF8, 0x48},
	{0xF9, 0x05},
	{0xFA, 0x55},
	{0x09, 0x01},
	{0xEF, 0x02},
	{0x2E, 0x0C},
	{0x33, 0x8C},
	{0x3C, 0xC8},
	{0x4E, 0x02},
	{0xB0, 0x81},
	{0xED, 0x01},
	{0xEF, 0x05},
	{0x0F, 0x00},
	{0x43, 0x01},
	{0x44, 0x00},
	{0xED, 0x01},
	{0xEF, 0x06},
	{0x00, 0x0C},
	{0x02, 0x13},
	{0x06, 0x02},
	{0x07, 0x02},
	{0x08, 0x02},
	{0x09, 0x02},
	{0x0F, 0x12},
	{0x10, 0x6C},
	{0x11, 0x12},
	{0x12, 0x6C},
	{0x18, 0x26},
	{0x1A, 0x26},
	{0x28, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x6C},
	{0x2F, 0x6C},
	{0x4A, 0x26},
	{0x4B, 0x26},
	{0x9E, 0x80},
	{0xDF, 0x04},
	{0xED, 0x01},
	{0xEF, 0x00},
	{0x11, 0x00},
	{0xED, 0x01},
	{SENSOR_REG_DELAY, 0x02},
	{0xEF, 0x01},
	{0x02, 0xFB},
	{SENSOR_REG_DELAY, 0x02},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_25fps_mipi[] = {
	{0xEF, 0x05},
	{0x0F, 0x00},
	{0x44, 0x00},
	{0x43, 0x02},
	{0xED, 0x01},
	{0xEF, 0x01},
	{0xF5, 0x01},
	{0x09, 0x01},
	{0xEF, 0x00},
	{0x10, 0x80},
	{0x11, 0x80},
	{0x13, 0x01},
	{0x16, 0xBC},
	{0x38, 0x28},
	{0x3C, 0x02},
	{0x5F, 0x64},
	{0x61, 0xDE},
	{0x62, 0x14},
	{0x69, 0x10},
	{0x75, 0x0B},
	{0x77, 0x06},
	{0x7E, 0x19},
	{0x85, 0x88},
	{0x9E, 0x00},
	{0xA0, 0x01},
	{0xA2, 0x09},
	{0xBE, 0x05},
	{0xBF, 0x80},
	{0xDF, 0x1E},
	{0xE1, 0x05},
	{0xE2, 0x03},
	{0xE3, 0x20},
	{0xE4, 0x0F},
	{0xE5, 0x07},
	{0xE6, 0x05},
	{0xF3, 0xB1},
	{0xF8, 0x5A},
	{0xED, 0x01},
	{0xEF, 0x01},
	{0x05, 0x0B},
	{0x07, 0x01},
	{0x08, 0x46},
	{0x0A, 0x05},
	{0x0B, 0x45},//8c9 for 15fps
	{0x0C, 0x00},
	{0x0D, 0x03},
	{0x0E, 0x11},
	{0x0F, 0x34},
	{0x18, 0x01},
	{0x19, 0x23},
	{0x27, 0x11},
	{0x28, 0x98},
	{0x37, 0x90},
	{0x3A, 0xF0},
	{0x3B, 0x20},
	{0x40, 0xF0},
	{0x42, 0xD6},
	{0x43, 0x20},
	{0x5C, 0x1E},
	{0x5D, 0x0A},
	{0x5E, 0x32},
	{0x67, 0x11},
	{0x68, 0xF4},
	{0x69, 0xC2},
	{0x7B, 0x08},
	{0x7C, 0x08},
	{0x7D, 0x02},
	{0x83, 0x02},
	{0x8F, 0x07},
	{0x90, 0x01},
	{0x92, 0x80},
	{0xA3, 0x00},
	{0xA4, 0x12},
	{0xA5, 0x04},
	{0xA6, 0x38},	/*1080*/
	{0xA7, 0x00},
	{0xA8, 0x04},
	{0xA9, 0x07},
	{0xAA, 0x80},	/*1920*/
	{0xAB, 0x01},
	{0xAE, 0x28},
	{0xB0, 0x28},
	{0xB3, 0x0A},
	{0xB6, 0x11},
	{0xB7, 0x34},
	{0xBE, 0x00},
	{0xBF, 0x00},
	{0xC0, 0x00},
	{0xC1, 0x00},
	{0xC4, 0x60},
	{0xC6, 0x60},
	{0xC7, 0x0A},
	{0xC8, 0xC8},
	{0xC9, 0x35},
	{0xCE, 0x6C},
	{0xCF, 0x82},
	{0xD0, 0x02},
	{0xD1, 0x60},
	{0xD5, 0x49},
	{0xD7, 0x0A},
	{0xD8, 0xC8},
	{0xDA, 0xC0},
	{0xDD, 0x42},
	{0xDE, 0x43},
	{0xE2, 0x9A},
	{0xE4, 0x00},
	{0xF0, 0x7C},
	{0xF1, 0x16},
	{0xF2, 0x24},
	{0xF3, 0x0C},
	{0xF4, 0x01},
	{0xF5, 0x19},
	{0xF6, 0x16},
	{0xF7, 0x00},
	{0xF8, 0x48},
	{0xF9, 0x05},
	{0xFA, 0x55},
	{0x09, 0x01},
	{0xEF, 0x02},
	{0x2E, 0x0C},
	{0x33, 0x8C},
	{0x3C, 0xC8},
	{0x4E, 0x02},
	{0xB0, 0x81},
	{0xED, 0x01},
	{0xEF, 0x05},
	{0x06, 0x04},	/*0x06 for RAW14*//*0x05 for RAW12*//*0x04 for RAW10*/
	{0x09, 0x09},
	{0x0A, 0x05},
	{0x0B, 0x06},
	{0x0C, 0x04},
	{0x0D, 0x5E},
	{0x0E, 0x01},
	{0x0F, 0x00},
	{0x10, 0x02},
	{0x11, 0x01},
	{0x15, 0x07},
	{0x17, 0x05},
	{0x18, 0x03},
	{0x1B, 0x03},
	{0x1C, 0x04},
	{0x25, 0x01},
	{0x40, 0x16},
	{0x41, 0x12},	/*0x1a,560Mbps*//*0x16,480Mbps*//*0x12,400Mbps*///raw10 available for 560/480/400m, raw12 avilable for 560/480m,
	{0x43, 0x02},
	{0x44, 0x01},
	{0x4A, 0x02},
	{0x4F, 0x01},
	{0x5B, 0x10},
	{0x94, 0x04},
	{0xB0, 0x01},//disable short packet
	{0xB1, 0x00},
	{0xED, 0x01},
	{0xEF, 0x06},
	{0x00, 0x0C},
	{0x02, 0x13},
	{0x06, 0x02},
	{0x07, 0x02},
	{0x08, 0x02},
	{0x09, 0x02},
	{0x0F, 0x12},
	{0x10, 0x6C},
	{0x11, 0x12},
	{0x12, 0x6C},
	{0x18, 0x26},
	{0x1A, 0x26},
	{0x28, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x6C},
	{0x2F, 0x6C},
	{0x4A, 0x26},
	{0x4B, 0x26},
	{0x9E, 0x80},
	{0xDF, 0x04},
	{0xED, 0x01},
	{0xEF, 0x00},
	{0x11, 0x00},
	{0xEF, 0x05},
	{0x3B, 0x00},
	{0xED, 0x01},
	{0xEF, 0x01},
	{0x02, 0xFB},
	{0x09, 0x01},
	{0xEF, 0x05},
	{0x0F, 0x01},
	{0xED, 0x01},
	{SENSOR_REG_DELAY, 0x02},
	{0xEF, 0x00},
	{0x11, 0x00},
	{SENSOR_REG_DELAY, 0x02},
	{0xEF, 0x01},
	{0x02, 0xFB},
	{0xEF, 0x05},
	{SENSOR_REG_DELAY, 0x02},
	{0x25, 0x00},
	{0xED, 0x01},
	{0xEF, 0x00},
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
		.fps = 15 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_15fps,
	},
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_25fps_mipi,
	}
};
static struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];


static struct regval_list sensor_stream_on_dvp[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_dvp[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_on_mipi[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, unsigned char reg, unsigned char *value)
{
	int ret;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct i2c_msg msg[2] = {
		[0] = {
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		[1] = {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = value,
		}
	};

	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct tx_isp_subdev *sd, unsigned char reg, unsigned char value)
{
	int ret;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
		.buf = buf,
	};

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
			if (vals->reg_num == SENSOR_BANK_REG) {
				val &= 0xe0;
				val |= (vals->value & 0x1f);
				ret = sensor_write(sd, vals->reg_num, val);
				ret = sensor_read(sd, vals->reg_num, &val);
			}
			pr_debug("sensor_read_array ->> vals->reg_num:0x%02x, vals->reg_value:0x%02x\n",vals->reg_num, val);
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
			if (ret < 0) {
				printk("sensor_write error  %d\n" ,__LINE__);
				return ret;
			}
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
	ret = sensor_read(sd, 0x00, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0) {
		printk("err: ps5260 write error, ret= %d \n",ret);
		return ret;
	}
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x01, &v);
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
	unsigned int Cmd_OffNy = 0;
	unsigned int IntNep = 0;
	unsigned int IntNe = 0;
	unsigned int Const;

	Cmd_OffNy = sensor_attr.total_height - value - 1;
	IntNep = NEPLS_LB + ((Cmd_OffNy*NEPLS_SCALE)>>8);
	IntNep = (IntNep > NEPLS_LB)?((IntNep < NEPLS_UB)?IntNep:NEPLS_UB):NEPLS_LB;
    switch (data_interface) {
	case TX_SENSOR_DATA_INTERFACE_DVP:
		Const = NE_NEP_CONST_LINEAR;
		break;
	case TX_SENSOR_DATA_INTERFACE_MIPI:
		Const = NE_NEP_CONST_HDR;
		break;
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this data interface!!!\n");
	}
	IntNe = Const - IntNep;

	ret = sensor_write(sd, 0xef, 0x01);
	ret += sensor_write(sd, 0x0c, (unsigned char)(Cmd_OffNy >> 8));
	ret += sensor_write(sd, 0x0d, (unsigned char)(Cmd_OffNy & 0xff));
	/*Exp Pixel Control*/
	ret += sensor_write(sd, 0x0e, (unsigned char)(IntNe >> 8));
	ret += sensor_write(sd, 0x0f, (unsigned char)(IntNe & 0xff));
	ret += sensor_write(sd, 0x10, (unsigned char)((IntNep >> 8) << 2));
	ret += sensor_write(sd, 0x12, (unsigned char)(IntNep & 0xff));
	ret += sensor_write(sd, 0x09, 0x01);
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int gain = value;
	static unsigned int mode = 0;

	if (gain > AG_HS_MODE) {
		mode = 0;
	} else if (gain < AG_LS_MODE) {
		mode = 1;
	}
	if (mode == 0)	gain -= 16;		// For 2x ratio
	ret = sensor_write(sd, 0xef, 0x01);
	ret += sensor_write(sd, 0x18, (unsigned char)(mode & 0x01));
	ret += sensor_write(sd, 0x83, (unsigned char)(gain & 0xff));
	ret += sensor_write(sd, 0x09, 0x01);
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
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		pr_debug("%s stream on\n", SENSOR_NAME);

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);

		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		pr_debug("%s stream off\n", SENSOR_NAME);
	}
	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int pclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int Cmd_Lpf = 0;
	unsigned int Cur_OffNy = 0;
	unsigned int Cur_ExpLine = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

    switch (data_interface) {
	case TX_SENSOR_DATA_INTERFACE_DVP:
		pclk = SENSOR_SUPPORT_PCLK_DVP;
		break;
	case TX_SENSOR_DATA_INTERFACE_MIPI:
		pclk = SENSOR_SUPPORT_PCLK_MIPI;
		break;
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this data interface!!!\n");
	}

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		printk("warn: fps(%d) not in range\n", fps);
		return -1;
	}
	ret = sensor_write(sd, 0xef, 0x01);
	if (ret < 0)
		return -1;
	ret = sensor_read(sd, 0x27, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0x28, &tmp);
	if (ret < 0)
		return -1;
	hts = (((hts & 0x1f) << 8) | tmp) ;

	vts = (pclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16));
	Cmd_Lpf = vts -1;
	ret = sensor_write(sd, 0xef, 0x01);
	ret += sensor_write(sd, 0x0b, (unsigned char)(Cmd_Lpf & 0xff));
	ret += sensor_write(sd, 0x0a, (unsigned char)(Cmd_Lpf >> 8));
	ret += sensor_write(sd, 0x09, 0x01);
	if (ret < 0) {
		printk("err: sensor_write err\n");
		return ret;
	}
	ret = sensor_read(sd, 0x0c, &tmp);
	Cur_OffNy = tmp;
	ret += sensor_read(sd, 0x0d, &tmp);
	if (ret < 0)
		return -1;
	Cur_OffNy = (((Cur_OffNy & 0xff) << 8) | tmp);
	Cur_ExpLine = sensor_attr.total_height - Cur_OffNy - 1;

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 3;
	sensor->video.attr->integration_time_limit = vts - 3;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 3;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	ret = sensor_set_integration_time(sd, Cur_ExpLine);
	if (ret < 0)
		return -1;

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

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val = 0;

	ret = sensor_write(sd, 0xef, 0x01);
	ret += sensor_read(sd, 0x1d, &val);
	if (enable)
		val = val | 0x80;
	else
		val = val & 0x7F;

	ret += sensor_write(sd, 0xef, 0x01);
	ret += sensor_write(sd, 0x1d, val);
	ret += sensor_write(sd, 0x09, 0x01);
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
	/*if (pwdn_gpio != -1) {
	  ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
	  if (!ret) {
	  private_gpio_direction_output(pwdn_gpio, 1);
	  private_msleep(50);
	  private_gpio_direction_output(pwdn_gpio, 0);
	  private_msleep(10);
	  } else {
	  printk("gpio request fail %d\n",pwdn_gpio);
	  }
	  }*/
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(20);
		} else {
			printk("gpio request fail %d\n",reset_gpio);
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
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);

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
	if (len && strncmp(sd->chip.name, reg->name, len))
		return -EINVAL;
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
	if (len && strncmp(sd->chip.name, reg->name, len))
		return -EINVAL;
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
	clk_set_rate(sensor->mclk, 24000000);
	clk_enable(sensor->mclk);

	if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
		wsize = &sensor_win_sizes[0];
		memcpy((void*)(&(sensor_attr.dvp)),(void*)(&sensor_dvp),sizeof(sensor_dvp));
		ret = set_sensor_gpio_function(sensor_gpio_func);
		if (ret < 0)
			goto err_set_sensor_gpio;
		sensor_attr.dvp.gpio = sensor_gpio_func;
	} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
		wsize = &sensor_win_sizes[1];
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi),sizeof(sensor_mipi));
		sensor_attr.max_integration_time_native = 1347;
	    sensor_attr.integration_time_limit = 1347;
	    sensor_attr.total_width = 4504;
	    sensor_attr.total_height = 1350;
	    sensor_attr.max_integration_time = 1347;
	} else {
		ISP_ERROR("Can not support this data interface!!!\n");
	}

	sensor_attr.dbus_type = data_interface;

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
	sensor_common_init(&sensor_info);

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
	sensor_common_exit();
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
