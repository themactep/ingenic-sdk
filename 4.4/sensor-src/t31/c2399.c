// SPDX-License-Identifier: GPL-2.0+
/*
 * c2399.c
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
#include <txx-funcs.h>

#define SENSOR_NAME "c2399"
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x36
#define SENSOR_MAX_WIDTH 1920
#define SENSOR_MAX_HEIGHT 1080
#define SENSOR_CHIP_ID 0x020B
#define SENSOR_CHIP_ID_H (0x02)
#define SENSOR_CHIP_ID_L (0x0b)
#define SENSOR_REG_END 0xFFFF
#define SENSOR_REG_DELAY 0xFFFE
#define SENSOR_SUPPORT_30FPS_SCLK (65600000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20220704a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_LOW_10BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_30;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

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
	{0x0, 0},
	{0x1, 5731},
	{0x2, 11136},
	{0x3, 16247},
	{0x4, 21097},
	{0x5, 25710},
	{0x6, 30108},
	{0x7, 34311},
	{0x8, 38335},
	{0x9, 42195},
	{0xa, 45903},
	{0xb, 49471},
	{0xc, 52910},
	{0xd, 56227},
	{0xe, 59433},
	{0xf, 62533},
	{0x10, 65535},
	{0x12, 71266},
	{0x14, 76671},
	{0x16, 81782},
	{0x18, 86632},
	{0x1a, 91245},
	{0x1c, 95643},
	{0x1e, 99846},
	{0x20, 103870},
	{0x22, 107730},
	{0x24, 111438},
	{0x26, 115006},
	{0x28, 118445},
	{0x2a, 121762},
	{0x2c, 124968},
	{0x2e, 128068},
	{0x30, 131070},
	{0x34, 136801},
	{0x38, 142206},
	{0x3c, 147317},
	{0x40, 152167},
	{0x44, 156780},
	{0x48, 161178},
	{0x4c, 165381},
	{0x50, 169405},
	{0x54, 173265},
	{0x58, 176973},
	{0x5c, 180541},
	{0x60, 183980},
	{0x64, 187297},
	{0x68, 190503},
	{0x6c, 193603},
	{0x70, 196605},
	{0x78, 202336},
	{0x80, 207741},
	{0x88, 212852},
	{0x90, 217702},
	{0x98, 222315},
	{0xa0, 226713},
	{0xa8, 230916},
	{0xb0, 234940},
	{0xb8, 238800},
	{0xc0, 242508},
	{0xc8, 246076},
	{0xd0, 249515},
	{0xd8, 252832},
	{0xe0, 256038},
	{0xe8, 259138},
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

struct tx_isp_mipi_bus sensor_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 656,
	.lans = 1,
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
	.mipi_sc.data_type_value = 0,
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
	.dvp_hcomp_en = 0,
};

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x020B,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
		.dvp_hcomp_en = 0,
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 259138,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_short = 1,
	.max_integration_time_short = 512,
	.min_integration_time_native = 4,
	.max_integration_time_native = 1321,
	.integration_time_limit = 1321,
	.total_width = 1980,
	.total_height = 1325,
	.max_integration_time = 1321,
	.integration_time_apply_delay = 0,
	.again_apply_delay = 0,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	.one_line_expr_in_us = 30,
	// void priv; /* point to struct tx_isp_sensor_board_info */
};


static struct regval_list sensor_init_regs_1920_1080_30fps_mipi[] = {
	// SENSOR_1080p_mipi1lane_30fps_V2P0.ini
	{0x0103,0x01},
	{SENSOR_REG_DELAY, 0xa},
	{0x3288,0x50},
	{0x0401,0x3b},
	{0x0403,0x00},
	{0x3298,0x42},
	{0x3290,0x69},
	{0x3286,0x77},
	{0x32aa,0x0f},
	{0x3295,0x02},
	{0x328b,0x20},
	{0x3211,0x16},
	{0x3216,0xd8},
	{0x3217,0x16},
	{0x3224,0x99},
	{0x3212,0x80},
	{0x3d00,0x33},
	{0x3187,0x07},
	{0x3222,0x82},
	{0x322c,0x04},
	{0x3c01,0x13},
	{0x3087,0xB0},
	{0x3584,0x02},
	{0x3108,0xef},
	{0x3112,0xe0},
	{0x3115,0x00},
	{0x3118,0x50},
	{0x3118,0x50},
	{0x311b,0x01},
	{0x3126,0x02},
	{0x0202,0x00},
	{0x0203,0x64},
	{0x0205,0x00},
	{0x3584,0x22},
	{0x3009,0x05},
	{0x022d,0x1f},
	{0x0101,0x01},
	{0x0340,0x05},
	{0x0341,0x2d}, /* 25fps vts 1325*/
	{0x0342,0x07},
	{0x0343,0xbc}, /* hts 1980 */

	/*AECAWB*/
	{0x3082,0x00},
	{0x3083,0x0a},
	{0x3084,0x80},
	{0x3089,0x10},
	{0x3f10,0x00},
	{0x0303,0x01},
	{0x0309,0x20},
	{0x0307,0x52},
	{0x3805,0x07},
	{0x3806,0x06},
	{0x3807,0x06},
	{0x3808,0x16},
	{0x3809,0x85},
	{0x380b,0xaa},
	{0x380e,0x31},
	{0x3600,0xc8},
	{0x3602,0x01},
	{0x3605,0x22},
	{0x3607,0x22},
	{0x3583,0x10},
	{0x3584,0x02},
	{0xa000,0x0b},
	{0xa001,0x50},
	{0xa002,0x80},
	{0xa003,0x00},
	{0xa004,0x00},
	{0xa005,0x80},
	{0xa006,0x80},
	{0xa007,0x80},
	{0xa008,0x4e},
	{0xa009,0x01},
	{0xa00a,0x03},
	{0xa00b,0x1c},
	{0xa00c,0x83},
	{0xa00d,0xa5},
	{0xa00e,0x8c},
	{0xa00f,0x81},
	{0xa010,0x0a},
	{0xa011,0x90},
	{0xa012,0x0d},
	{0xa013,0x81},
	{0xa014,0x0c},
	{0xa015,0x50},
	{0xa016,0x70},
	{0xa017,0x50},
	{0xa018,0xa9},
	{0xa019,0xe1},
	{0xa01a,0x05},
	{0xa01b,0x81},
	{0xa01c,0xb0},
	{0xa01d,0xe0},
	{0xa01e,0x86},
	{0xa01f,0x01},
	{0xa020,0x08},
	{0xa021,0x10},
	{0xa022,0x8f},
	{0xa023,0x81},
	{0xa024,0x88},
	{0xa025,0x90},
	{0xa026,0x8f},
	{0xa027,0x01},
	{0xa028,0xd0},
	{0xa029,0xe1},
	{0xa02a,0x0f},
	{0xa02b,0x84},
	{0xa02c,0x03},
	{0xa02d,0x38},
	{0xa02e,0x1a},
	{0xa02f,0x50},
	{0xa030,0x8e},
	{0xa031,0x03},
	{0xa032,0xaf},
	{0xa033,0xd0},
	{0xa034,0xb0},
	{0xa035,0x61},
	{0xa036,0x0f},
	{0xa037,0x84},
	{0xa038,0x83},
	{0xa039,0x38},
	{0xa03a,0xa0},
	{0xa03b,0x50},
	{0xa03c,0x90},
	{0xa03d,0xe0},
	{0xa03e,0x2e},
	{0xa03f,0xd0},
	{0xa040,0x90},
	{0xa041,0x61},
	{0xa042,0x8f},
	{0xa043,0x84},
	{0xa044,0x03},
	{0xa045,0x38},
	{0xa046,0xa6},
	{0xa047,0xd0},
	{0xa048,0x20},
	{0xa049,0x60},
	{0xa04a,0x2e},
	{0xa04b,0x50},
	{0xa04c,0x70},
	{0xa04d,0xe0},
	{0xa04e,0x0f},
	{0xa04f,0x04},
	{0xa050,0x03},
	{0xa051,0x38},
	{0xa052,0xac},
	{0xa053,0xd0},
	{0xa054,0x30},
	{0xa055,0x60},
	{0xa056,0xae},
	{0xa057,0x50},
	{0xa058,0x8f},
	{0xa059,0x10},
	{0xa05a,0xf0},
	{0xa05b,0x79},
	{0xa05c,0x8e},
	{0xa05d,0x01},
	{0xa05e,0x27},
	{0xa05f,0xe0},
	{0xa060,0x05},
	{0xa061,0x01},
	{0xa062,0x31},
	{0xa063,0x60},
	{0xa064,0x86},
	{0xa065,0x01},
	{0xa066,0x0e},
	{0xa067,0x90},
	{0xa068,0x08},
	{0xa069,0x01},
	{0xa06a,0xe0},
	{0xa06b,0xe1},
	{0xa06c,0x8f},
	{0xa06d,0x04},
	{0xa06e,0x8a},
	{0xa06f,0xe0},
	{0xa070,0x03},
	{0xa071,0x38},
	{0xa072,0xc6},
	{0xa073,0xd0},
	{0xa074,0x85},
	{0xa075,0x81},
	{0xa076,0xb1},
	{0xa077,0xe0},
	{0xa078,0x86},
	{0xa079,0x01},
	{0xa07a,0xa0},
	{0xa07b,0xe1},
	{0xa07c,0x08},
	{0xa07d,0x81},
	{0xa07e,0x0b},
	{0xa07f,0x60},
	{0xa080,0xe5},
	{0xa081,0xc0},
	{0xa082,0x8c},
	{0xa083,0x60},
	{0xa084,0x65},
	{0xa085,0xc0},
	{0xa086,0x8d},
	{0xa087,0xe0},
	{0xa088,0xe5},
	{0xa089,0x40},
	{0xa08a,0x59},
	{0xa08b,0x50},
	{0xa08c,0x05},
	{0xa08d,0x81},
	{0xa08e,0x31},
	{0xa08f,0xe0},
	{0xa090,0x86},
	{0xa091,0x01},
	{0xa092,0x08},
	{0xa093,0x83},
	{0xa094,0x0b},
	{0xa095,0xe0},
	{0xa096,0x05},
	{0xa097,0x81},
	{0xa098,0xb1},
	{0xa099,0x60},
	{0xa09a,0x86},
	{0xa09b,0x01},
	{0xa09c,0x88},
	{0xa09d,0x83},
	{0xa09e,0x0c},
	{0xa09f,0xe0},
	{0xa0a0,0x05},
	{0xa0a1,0x01},
	{0xa0a2,0xb1},
	{0xa0a3,0xe0},
	{0xa0a4,0x06},
	{0xa0a5,0x81},
	{0xa0a6,0x88},
	{0xa0a7,0x03},
	{0xa0a8,0x8d},
	{0xa0a9,0x60},
	{0xa0aa,0x05},
	{0xa0ab,0x01},
	{0xa0ac,0x31},
	{0xa0ad,0xe0},
	{0xa0ae,0x86},
	{0xa0af,0x81},
	{0xa0b0,0x08},
	{0xa0b1,0x03},
	{0xa0b2,0x90},
	{0xa0b3,0xe0},
	{0xa0b4,0x85},
	{0xa0b5,0x81},
	{0xa0b6,0x36},
	{0xa0b7,0x60},
	{0xa0b8,0x06},
	{0xa0b9,0x81},
	{0xa0ba,0x88},
	{0xa0bb,0x03},
	{0xa0bc,0x0d},
	{0xa0bd,0x10},
	{0xa0be,0x8a},
	{0xa0bf,0x01},
	{0xa0c0,0x0c},
	{0xa0c1,0x9c},
	{0xa0c2,0x03},
	{0xa0c3,0x81},
	{0xa0c4,0xce},
	{0xa0c5,0x9d},
	{0xa0c6,0x4e},
	{0xa0c7,0x1c},
	{0xa0c8,0x09},
	{0xa0c9,0x80},
	{0xa0ca,0x05},
	{0xa0cb,0x81},
	{0xa0cc,0x31},
	{0xa0cd,0xe0},
	{0xa0ce,0x06},
	{0xa0cf,0x01},
	{0xa0d0,0x20},
	{0xa0d1,0x61},
	{0xa0d2,0x88},
	{0xa0d3,0x81},
	{0xa0d4,0x08},
	{0xa0d5,0x00},
	{0xa0d6,0x83},
	{0xa0d7,0x2d},
	{0xa0d8,0x01},
	{0xa0d9,0xae},
	{0xa0da,0x8b},
	{0xa0db,0x2c},
	{0xa0dc,0x0b},
	{0xa0dd,0x2f},
	{0xa0de,0xef},
	{0xa0df,0x50},
	{0xa0e0,0x83},
	{0xa0e1,0x83},
	{0xa0e2,0xeb},
	{0xa0e3,0x50},
	{0x3583,0x00},
	{0x3584,0x22},

	{0x0101,0x01},
	{0x3009,0x05},
	{0x300b,0x10}, //for bayer=BGGR
	{0x0100,0x01}, //stream on
	{SENSOR_REG_END, 0x00},
};


/*
 * the order of the jxf23_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* resolution 1920*1080 */
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_mipi,
	},
};

struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sensor_stream_on_dvp[] = {
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_dvp[] = {
	{0x0100, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_on_mipi[] = {
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, unsigned short reg,
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

int sensor_write(struct tx_isp_subdev *sd, unsigned short reg,
		 unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	int ret;
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 3,
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
	unsigned char v;
	int ret;
	ret = sensor_read(sd, 0x0000, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	ret = sensor_read(sd, 0x0001, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	/*integration time  0x0203/0x0203 WO, 0x30a7/0x30a6 RO*/
	ret = sensor_write(sd, 0x0203, value & 0xff);
	ret += sensor_write(sd, 0x0202, value >> 8);
	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	/*analog gain 0x0205 WO, 0x30a9 RO*/
	ret = sensor_write(sd, 0x0205, value);
	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d" ,__LINE__ );
		return ret;
	}
	return 0;
}
#endif

static int sensor_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	ret = sensor_write(sd, 0x0203, it & 0xff);
	ret += sensor_write(sd, 0x0202, it >> 8);
	ret = sensor_write(sd, 0x0205, again);
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
		}
		pr_debug("%s stream on\n", SENSOR_NAME);
	} else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);
		}
		pr_debug("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int wpclk = 0;
	unsigned short vts = 0;
	unsigned short hts=0;
	unsigned int max_fps = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_30:
		wpclk = SENSOR_SUPPORT_30FPS_SCLK;
		max_fps = SENSOR_OUTPUT_MAX_FPS;
		break;
	/*
	case TX_SENSOR_MAX_FPS_15:
		wpclk = SENSOR_SUPPORT_15FPS_SCLK;
		max_fps = TX_SENSOR_MAX_FPS_15;
		break;
		*/
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}
	ret += sensor_read(sd, 0x0342, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0x0343, &tmp);
	if (ret < 0)
		return -1;

	hts = (hts << 8) + tmp;

	vts = wpclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = sensor_write(sd, 0x0340, (unsigned char)((vts & 0xff00) >> 8));
	ret += sensor_write(sd, 0x0341, (unsigned char)(vts & 0xff));
	if (ret < 0)
		return -1;

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;
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
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
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
			private_gpio_direction_output(pwdn_gpio, 1);
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
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = -1;
	unsigned char val = 0x0;
	unsigned char startx = 0;
	unsigned char starty = 0;

	ret = sensor_read(sd, 0x0101, &val);

	if (enable & 0x2) {
		val |= 0x02;
		startx = 0x05;
		starty = 0x0f;
	} else {
		val &= 0xfd;
		startx = 0x05;
		starty = 0x10;
	}

	ret += sensor_write(sd, 0x0101, val);
	ret += sensor_write(sd, 0x3009, startx);
	ret += sensor_write(sd, 0x300b, starty);
	if (ret < 0)
		return ret;

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

	/*
	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;

	sensor_attr.dvp.gpio = sensor_gpio_func;
	*/
	sensor_attr.dbus_type = data_interface;

	if ((data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) && (sensor_max_fps == TX_SENSOR_MAX_FPS_30)) {
		wsize = &sensor_win_sizes[0];
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi),sizeof(sensor_mipi));
	} else {
		ISP_ERROR("Can not support this data interface and fps!!!\n");
		goto err_set_sensor_data_interface;
	}

	/*
	  convert sensor-gain into isp-gain,
	*/
	sensor_attr.expo_fs = 1;
	sd = &sensor->sd;
	video = &sensor->video;
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
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);
	return 0;
err_set_sensor_data_interface:
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
