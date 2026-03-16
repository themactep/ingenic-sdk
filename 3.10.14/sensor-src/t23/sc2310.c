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
#include <tx-isp-common.h>
#include <sensor-common.h>
#include <sensor-info.h>

// ============================================================================
// SENSOR IDENTIFICATION
// ============================================================================
#define SENSOR_NAME "sc2310"
#define SENSOR_VERSION "H20230720a"
#define SENSOR_CHIP_ID_H (0x23)
#define SENSOR_CHIP_ID_L (0x11)
#define SENSOR_CHIP_ID 0x2311

// ============================================================================
// REGISTER DEFINITIONS
// ============================================================================
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe

// ============================================================================
// TIMING AND PERFORMANCE
// ============================================================================
#define SENSOR_SUPPORT_30FPS_SCLK (1100 * 1125 * 30 * 2) /* 74250000 */
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int shvflip = 0;
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
	{0x342, 2886},
	{0x344, 5776},
	{0x346, 8494},
	{0x348, 11136},
	{0x34a, 13706},
	{0x34c, 16287},
	{0x34e, 18723},
	{0x350, 21097},
	{0x352, 23414},
	{0x354, 25746},
	{0x356, 27953},
	{0x358, 30109},
	{0x35a, 32217},
	{0x35c, 34345},
	{0x35e, 36361},
	{0x360, 38336},
	{0x362, 40270},
	{0x364, 42226},
	{0x366, 44082},
	{0x368, 45904},
	{0x36a, 47690},
	{0x36c, 49500},
	{0x36e, 51220},
	{0x370, 52910},
	{0x372, 54571},
	{0x374, 56254},
	{0x376, 57857},
	{0x378, 59433},
	{0x37a, 60984},
	{0x37c, 62558},
	{0x37e, 64059},
	{0x740, 65536},
	{0x742, 68468},
	{0x744, 71267},
	{0x746, 74030},
	{0x748, 76672},
	{0x74a, 79283},
	{0x74c, 81784},
	{0x74e, 84259},
	{0x750, 86633},
	{0x752, 88986},
	{0x754, 91246},
	{0x756, 93489},
	{0x2341, 96091},
	{0x2343, 98956},
	{0x2345, 101736},
	{0x2347, 104437},
	{0x2349, 107063},
	{0x234b, 109618},
	{0x234d, 112106},
	{0x234f, 114530},
	{0x2351, 116894},
	{0x2353, 119200},
	{0x2355, 121451},
	{0x2357, 123649},
	{0x2359, 125798},
	{0x235b, 127899},
	{0x235d, 129954},
	{0x235f, 131965},
	{0x2361, 133935},
	{0x2363, 135864},
	{0x2365, 137755},
	{0x2367, 139609},
	{0x2369, 141427},
	{0x236b, 143211},
	{0x236d, 144962},
	{0x236f, 146681},
	{0x2371, 148369},
	{0x2373, 150027},
	{0x2375, 151657},
	{0x2377, 153260},
	{0x2379, 154836},
	{0x237b, 156385},
	{0x237d, 157910},
	{0x237f, 159411},
	{0x2741, 161610},
	{0x2743, 164475},
	{0x2745, 167256},
	{0x2747, 169958},
	{0x2749, 172584},
	{0x274b, 175140},
	{0x274d, 177628},
	{0x274f, 180052},
	{0x2751, 182416},
	{0x2753, 184722},
	{0x2755, 186974},
	{0x2757, 189172},
	{0x2759, 191321},
	{0x275b, 193423},
	{0x275d, 195478},
	{0x275f, 197490},
	{0x2761, 199460},
	{0x2763, 201389},
	{0x2765, 203280},
	{0x2767, 205134},
	{0x2769, 206953},
	{0x276b, 208736},
	{0x276d, 210487},
	{0x276f, 212207},
	{0x2771, 213895},
	{0x2773, 215554},
	{0x2775, 217184},
	{0x2777, 218786},
	{0x2779, 220362},
	{0x277b, 221912},
	{0x277d, 223437},
	{0x277f, 224938},
	{0x2f41, 227146},
	{0x2f43, 230011},
	{0x2f45, 232792},
	{0x2f47, 235494},
	{0x2f49, 238120},
	{0x2f4b, 240676},
	{0x2f4d, 243164},
	{0x2f4f, 245588},
	{0x2f51, 247952},
	{0x2f53, 250258},
	{0x2f55, 252510},
	{0x2f57, 254708},
	{0x2f59, 256857},
	{0x2f5b, 258959},
	{0x2f5d, 261014},
	{0x2f5f, 263026},
	{0x2f61, 264996},
	{0x2f63, 266925},
	{0x2f65, 268816},
	{0x2f67, 270670},
	{0x2f69, 272489},
	{0x2f6b, 274273},
	{0x2f6d, 276023},
	{0x2f6f, 277743},
	{0x2f71, 279431},
	{0x2f73, 281090},
	{0x2f75, 282720},
	{0x2f77, 284322},
	{0x2f79, 285898},
	{0x2f7b, 287448},
	{0x2f7d, 288973},
	{0x2f7f, 290474},
	{0x3f41, 292682},
	{0x3f43, 295547},
	{0x3f45, 298328},
	{0x3f47, 301030},
	{0x3f49, 303656},
	{0x3f4b, 306212},
	{0x3f4d, 308700},
	{0x3f4f, 311124},
	{0x3f51, 313488},
	{0x3f53, 315794},
	{0x3f55, 318046},
	{0x3f57, 320244},
	{0x3f59, 322393},
	{0x3f5b, 324495},
	{0x3f5d, 326550},
	{0x3f5f, 328562},
	{0x3f61, 330532},
	{0x3f63, 332461},
	{0x3f65, 334352},
	{0x3f67, 336206},
	{0x3f69, 338025},
	{0x3f6b, 339809},
	{0x3f6d, 341559},
	{0x3f6f, 343279},
	{0x3f71, 344967},
	{0x3f73, 346626},
	{0x3f75, 348256},
	{0x3f77, 349858},
	{0x3f79, 351434},
	{0x3f7b, 352984},
	{0x3f7d, 354509},
	{0x3f7f, 356010},
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


struct tx_isp_mipi_bus sensor_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 371,
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
};

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 356010,
	.max_dgain = 0,
	.min_integration_time = 3,
	.min_integration_time_native = 3,
	.max_integration_time_native = 2244, /* 2 * 1125 - 6 */
	.integration_time_limit = 2244,
	.total_width = 2200,
	.total_height = 1125,
	.max_integration_time = 2244,
	.one_line_expr_in_us = 25,
	.expo_fs = 1,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_1920_1080_30fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x3001, 0xfe},
	{0x3018, 0x33},
	{0x3019, 0x0c},
	{0x301c, 0x78},
	{0x301f, 0x40},
	{0x3031, 0x0a},
	{0x3037, 0x22},
	{0x3038, 0x22},
	{0x303f, 0x01},
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
	{0x320c, 0x04},//hts -> 0x44c = 1100
	{0x320d, 0x4c},//
	{0x320e, 0x04},//vts -> 0x465 = 1125
	{0x320f, 0x65},//
	{0x3210, 0x00},
	{0x3211, 0x04},
	{0x3212, 0x00},
	{0x3213, 0x04},
	{0x3301, 0x10},
	{0x3302, 0x10},
	{0x3303, 0x18},
	{0x3306, 0x60},
	{0x3308, 0x08},
	{0x3309, 0x30},
	{0x330a, 0x00},
	{0x330b, 0xc8},
	{0x330e, 0x28},
	{0x3314, 0x04},
	{0x331b, 0x83},
	{0x331e, 0x11},
	{0x331f, 0x29},
	{0x3320, 0x01},
	{0x3324, 0x02},
	{0x3325, 0x02},
	{0x3326, 0x00},
	{0x3333, 0x30},
	{0x3334, 0x40},
	{0x333d, 0x08},
	{0x3341, 0x07},
	{0x3343, 0x03},
	{0x3364, 0x1d},
	{0x3366, 0x80},
	{0x3367, 0x08},
	{0x3368, 0x04},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x336c, 0x42},
	{0x337f, 0x03},
	{0x3380, 0x1b},
	{0x33aa, 0x00},
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
	{0x3621, 0xac},
	{0x3622, 0xe6},
	{0x3623, 0x18},
	{0x3624, 0x47},
	{0x3630, 0xc8},
	{0x3631, 0x88},
	{0x3632, 0x18},
	{0x3633, 0x22},
	{0x3634, 0x44},
	{0x3635, 0x40},
	{0x3636, 0x65},
	{0x3637, 0x17},
	{0x3638, 0x25},
	{0x363b, 0x08},
	{0x363c, 0x05},
	{0x363d, 0x05},
	{0x3640, 0x00},
	{0x366e, 0x04},
	{0x3670, 0x4a},
	{0x3671, 0xf6},
	{0x3672, 0x16},
	{0x3673, 0x16},
	{0x3674, 0xc8},
	{0x3675, 0x54},
	{0x3676, 0x18},
	{0x3677, 0x22},
	{0x3678, 0x33},
	{0x3679, 0x44},
	{0x367a, 0x40},
	{0x367b, 0x40},
	{0x367c, 0x40},
	{0x367d, 0x58},
	{0x367e, 0x40},
	{0x367f, 0x58},
	{0x3696, 0x83},
	{0x3697, 0x87},
	{0x3698, 0x9f},
	{0x36a0, 0x58},
	{0x36a1, 0x78},
	{0x36ea, 0x9f},
	{0x36eb, 0x0e},
	{0x36ec, 0x1e},
	{0x36ed, 0x03},
	{0x36fa, 0xf8},
	{0x36fb, 0x10},
	{0x3802, 0x00},
	{0x3907, 0x01},
	{0x3908, 0x01},
	{0x391e, 0x00},
	{0x391f, 0xc0},
	{0x3933, 0x28},
	{0x3934, 0x0a},
	{0x3940, 0x1b},
	{0x3941, 0x40},
	{0x3942, 0x08},
	{0x3943, 0x0e},
	{0x3e00, 0x00},
	{0x3e01, 0x8c},
	{0x3e02, 0x40},
	{0x3e03, 0x0b},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3e0e, 0x66},
	{0x3e14, 0xb0},
	{0x3e1e, 0x35},
	{0x3e25, 0x03},
	{0x3e26, 0x40},
	{0x3f00, 0x0d},
	{0x3f04, 0x02},
	{0x3f05, 0x1e},
	{0x3f08, 0x04},
	{0x4500, 0x59},
	{0x4501, 0xb4},
	{0x4509, 0x20},
	{0x4603, 0x00},
	{0x4809, 0x01},
	{0x4837, 0x35},
	{0x5000, 0x06},
	{0x5780, 0x7f},
	{0x5781, 0x06},
	{0x5782, 0x04},
	{0x5783, 0x02},
	{0x5784, 0x01},
	{0x5785, 0x16},
	{0x5786, 0x12},
	{0x5787, 0x08},
	{0x5788, 0x02},
	{0x57a0, 0x00},
	{0x57a1, 0x74},
	{0x57a2, 0x01},
	{0x57a3, 0xf4},
	{0x57a4, 0xf0},
	{0x6000, 0x00},
	{0x6002, 0x00},
	{0x36e9, 0x51},
	{0x36f9, 0x04},
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* resolution 2048*1536 @25fps max*/
	{
		.width = 1920,
		.height = 1080,
		.fps = 30 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_mipi,
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
     int it = (value & 0xffff);
     int again = (value & 0xffff0000) >> 16;

     ret += sensor_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
     ret += sensor_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));

     ret += sensor_write(sd, 0x3e08, (unsigned char)(((again >> 8) & 0xff)));
     ret += sensor_write(sd, 0x3e09, (unsigned char)(again & 0xff));
     if (ret < 0)
         return ret;

     return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

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
	ret += sensor_write(sd, 0x3e08, (unsigned char)(((value >> 8) & 0xff)));
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int sensor_set_logic(struct tx_isp_subdev *sd, int value)
{
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
		ret = sensor_write_array(sd, sensor_stream_on_mipi);
		ISP_WARNING("%s stream on\n", SENSOR_NAME);

	} else {
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
	unsigned char tmp = 0;
	unsigned int max_fps = 0;

	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	sclk = sensor_SUPPORT_30FPS_SCLK;
	max_fps = TX_SENSOR_MAX_FPS_50;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(0x%x) not in range\n", fps);
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
	sensor->video.attr->max_integration_time_native = 2*vts - 6;
	sensor->video.attr->integration_time_limit = 2*vts - 6;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = 2*vts - 6;

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
			private_msleep(50);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(35);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(35);
		} else {
			ISP_ERROR("gpio request fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(150);
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
	if (0 != ret)
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
//		if (arg)
//			ret = sensor_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
//		if (arg)
//			ret = sensor_set_analog_gain(sd, *(int*)arg);
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
		break;
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if (arg)
			ret = sensor_set_logic(sd, *(int*)arg);
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
    {
        unsigned int arate = 0,mrate = 0;
        unsigned int want_rate = 0;
	    struct clk *clka = NULL;
	    struct clk *clkm = NULL;

        want_rate=24000000;
        clka = clk_get(NULL, "sclka");
        clkm = clk_get(NULL, "mpll");
        arate = clk_get_rate(clka);
        mrate = clk_get_rate(clkm);
        if ((arate%want_rate) && (mrate%want_rate)) {
            if (want_rate == 37125000) {
                if (arate >= 1400000000) {
                    arate = 1485000000;
                } else if ((arate >= 1100) || (arate < 1400)) {
                    arate = 1188000000;
                } else if (arate <= 1100) {
                    arate = 891000000;
                }
            } else {
                mrate = arate%want_rate;
                arate = arate-mrate;
            }
            clk_set_rate(clka, arate);
            clk_set_parent(sensor->mclk, clka);
        } else if (!(arate%want_rate)) {
            clk_set_parent(sensor->mclk, clka);
        } else if (!(mrate%want_rate)) {
            clk_set_parent(sensor->mclk, clkm);
        }
        private_clk_set_rate(sensor->mclk, want_rate);
        private_clk_enable(sensor->mclk);
    }

	wsize = &sensor_win_sizes[0];
	sensor_attr.max_fps = 30;
	sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
	sensor_attr.max_integration_time_native = 2244;
	sensor_attr.integration_time_limit = 2244;
	sensor_attr.total_width = 2200;
	sensor_attr.total_height = 1125;
	sensor_attr.max_integration_time = 2244;
	memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi),sizeof(sensor_mipi));


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

	ISP_WARNING("probe ok ------->%s\n", SENSOR_NAME);

	return 0;
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
		ISP_ERROR("Failed to init %s driver.\n", SENSOR_NAME);
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
