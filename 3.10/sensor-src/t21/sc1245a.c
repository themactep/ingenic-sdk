// SPDX-License-Identifier: GPL-2.0+
/*
 * sc1245a.c
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
#define SENSOR_NAME "sc1245a"
#define SENSOR_VERSION "H20181024a"
#define SENSOR_CHIP_ID 0x1245a
#define SENSOR_CHIP_ID_H (0x12)
#define SENSOR_CHIP_ID_M (0x45)
#define SENSOR_CHIP_ID_L (0x02)

// ============================================================================
// HARDWARE INTERFACE
// ============================================================================
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x30

// ============================================================================
// SENSOR CAPABILITIES
// ============================================================================
#define SENSOR_MAX_WIDTH 1280
#define SENSOR_MAX_HEIGHT 720

// ============================================================================
// REGISTER DEFINITIONS
// ============================================================================
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe

// ============================================================================
// TIMING AND PERFORMANCE
// ============================================================================
#define SENSOR_SUPPORT_PCLK_FPS_30 (36000*1000)
#define SENSOR_SUPPORT_PCLK_FPS_15 (18000*1000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_1

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_LOW_10BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_25;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

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
    {0x10, 0},
    {0x11, 5731},
    {0x12, 11136},
    {0x13, 16248},
    {0x14, 21097},
    {0x15, 25710},
    {0x16, 30109},
    {0x17, 34312},
    {0x18, 38336},
    {0x19, 42195},
    {0x1a, 45904},
    {0x1b, 49472},
    {0x1c, 52910},
    {0x1d, 56228},
    {0x1e, 59433},
    {0x1f, 62534},
    {0x110, 65536},
    {0x111, 71267},
    {0x112, 76672},
    {0x113, 81784},
    {0x114, 86633},
    {0x115, 91246},
    {0x116, 95645},
    {0x117, 99848},
    {0x118, 103872},
    {0x119, 107731},
    {0x11a, 111440},
    {0x11b, 115008},
    {0x11c, 118446},
    {0x11d, 121764},
    {0x11e, 124969},
    {0x11f, 128070},
    {0x310, 131072},
    {0x311, 136803},
    {0x312, 142208},
    {0x313, 147320},
    {0x314, 152169},
    {0x315, 156782},
    {0x316, 161181},
    {0x317, 165384},
    {0x318, 169408},
    {0x319, 173267},
    {0x31a, 176976},
    {0x31b, 180544},
    {0x31c, 183982},
    {0x31d, 187300},
    {0x31e, 190505},
    {0x31f, 193606},
    {0x710, 196608},
    {0x711, 202339},
    {0x712, 207744},
    {0x713, 212856},
    {0x714, 217705},
    {0x715, 222318},
    {0x716, 226717},
    {0x717, 230920},
    {0x718, 234944},
    {0x719, 238803},
    {0x71a, 242512},
    {0x71b, 246080},
    {0x71c, 249518},
    {0x71d, 252836},
    {0x71e, 256041},
    {0x71f, 259142},
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
	.chip_id = 0x1245a,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
	},
	.max_again = 256041,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 0x384 - 2,
	.integration_time_limit = 0x384 - 2,
	.total_width = 0x640,
	.total_height = 0x384,
	.max_integration_time = 0x384 - 2,
	.one_line_expr_in_us = 28,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};


static struct regval_list sensor_init_regs_1280_720_25fps[] = {
    {0x0103, 0x01},
    {0x0100, 0x00},
    {0x4500, 0x51},
    {0x320e, 0x03},
    {0x320f, 0x84},
    {0x3e01, 0x38},
    {0x3635, 0xa0},
    {0x3620, 0x28},
    {0x3309, 0xa0},
    {0x331f, 0x9d},
    {0x3321, 0x9f},
    {0x3306, 0x65},
    {0x330b, 0x66},
    {0x3e08, 0x03},
    {0x3e07, 0x00},
    {0x3e09, 0x10},
    {0x3367, 0x18},
    {0x335e, 0x01},
    {0x335f, 0x03},
    {0x337c, 0x04},
    {0x337d, 0x06},
    {0x33a0, 0x05},
    {0x3301, 0x05},
    {0x3302, 0x7d},
    {0x3633, 0x43},
    {0x3908, 0x11},
    {0x3621, 0x28},
    {0x3622, 0x02},
    {0x3637, 0x1c},
    {0x3308, 0x1c},
    {0x3367, 0x10},
    {0x337f, 0x03},
    {0x3368, 0x04},
    {0x3369, 0x00},
    {0x336a, 0x00},
    {0x336b, 0x00},
    {0x3367, 0x08},
    {0x330e, 0x30},
    {0x3366, 0x7c},
    {0x3635, 0xc4},
    {0x3621, 0x20},
    {0x3334, 0x40},
    {0x3333, 0x10},
    {0x3302, 0x3d},
    {0x335e, 0x01},
    {0x335f, 0x02},
    {0x337c, 0x03},
    {0x337d, 0x05},
    {0x33a0, 0x04},
    {0x3301, 0x03},
    {0x3633, 0x48},
    {0x3670, 0x0a},
    {0x367c, 0x07},
    {0x367d, 0x0f},
    {0x3674, 0x20},
    {0x3675, 0x2f},
    {0x3676, 0x2f},
    {0x3631, 0x84},
    {0x3622, 0x06},
    {0x3630, 0x20},
    {0x3620, 0x08},
    {0x3638, 0x1f},
    {0x3625, 0x02},
    {0x3637, 0x1e},
    {0x3636, 0x25},
    {0x3306, 0x1c},
    {0x3637, 0x1a},
    {0x331e, 0x0c},
    {0x331f, 0x9c},
    {0x3320, 0x0c},
    {0x3321, 0x9c},
    {0x366e, 0x08},
    {0x366f, 0x2a},
    {0x3622, 0x16},
    {0x363b, 0x0c},
    {0x3639, 0x0a},
    {0x3632, 0x28},
    {0x3038, 0x84},
    {0x3635, 0xc0},
    {0x3f00, 0x07},
    {0x3f04, 0x02},
    {0x3f05, 0xfc},
    {0x3802, 0x01},
    {0x3235, 0x07},
    {0x3236, 0x06},
    {0x3639, 0x09},
    {0x3670, 0x02},
    {0x3635, 0x80},
    {0x320e, 0x03},
    {0x320f, 0x84},
    {0x3235, 0x05},
    {0x3236, 0xda},
    {0x3039, 0x51},
    {0x303a, 0xba},
    {0x3034, 0x06},
    {0x3035, 0xe2},
    {0x3208, 0x05},
    {0x3209, 0x00},
    {0x320a, 0x02},
    {0x320b, 0xd0},
    {0x3211, 0x04},
    {0x3213, 0x04},
    {0x3302, 0x7d},
    {0x3357, 0x51},
    {0x3326, 0x00},
    {0x3303, 0x20},
    {0x3320, 0x07},
    {0x331e, 0x11},
    {0x331f, 0x91},
    {0x366f, 0x2c},
    {0x3333, 0x30},
    {0x331b, 0x83},
    {0x330b, 0x5e},
    {0x3620, 0x28},
    {0x3631, 0x86},
    {0x3633, 0x42},
    {0x3301, 0x04},
    {0x3622, 0xd6},
    {0x3635, 0x84},
    {0x3e01, 0x5d},
    {0x3e02, 0x80},
    {0x3802, 0x00},
    {0x3e00, 0x00},
    {0x3e01, 0x5d},
    {0x3e02, 0x80},
    {0x3e03, 0x0b},
    {0x3e08, 0x03},
    {0x3e09, 0x10},
    {0x3303, 0x20},
    {0x3309, 0xa0},
    {0x3635, 0x84},
    {0x3633, 0x42},
    {0x3301, 0x04},
    {0x3622, 0xd6},
    {SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1280_720_15fps[] = {
    {SENSOR_REG_END, 0x00},
};
/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
    { .width = 1280, .height = 720, .fps = 25 << 16 | 1, .mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10, .colorspace = V4L2_COLORSPACE_SRGB, .regs = sensor_init_regs_1280_720_25fps, },
};

static enum v4l2_mbus_pixelcode sensor_mbus_code[] = {
	V4L2_MBUS_FMT_SBGGR10_1X10,
};


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
	uint8_t buf[3] = {(reg>>8)&0xff, reg&0xff, value};
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
	if (v != SENSOR_CHIP_ID_M)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	ret = sensor_read(sd, 0x3020, &v);
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

	value *= 2;
	ret = sensor_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0x0f));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));

	/* if (value < 160) { */
	/* 	ret += sensor_write(sd, 0x3314, 0x14); */
	/* } */
	/* else if (value > 320) { */
	/* 	ret += sensor_write(sd, 0x3314, 0x04); */
	/* } */

	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = sensor_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3e08, (unsigned char)((value >> 8 << 2) | 0x03));
	if (ret < 0)
		return ret;

	if (value < 0x110) {
		sensor_write(sd, 0x3812, 0x00);
		sensor_write(sd, 0x3633, 0x42);
		sensor_write(sd, 0x3301, 0x04);
		sensor_write(sd, 0x3622, 0xd6);
		sensor_write(sd, 0x3812, 0x30);
	}
	else if (value>=0x110&&value<0x310) {
		sensor_write(sd, 0x3812, 0x00);
		sensor_write(sd, 0x3633, 0x42);
		sensor_write(sd, 0x3301, 0x05);
		sensor_write(sd, 0x3622, 0xd6);
		sensor_write(sd, 0x3812, 0x30);
	}
	else if (value>=0x310&&value<0x710) {
		sensor_write(sd, 0x3812, 0x00);
		sensor_write(sd, 0x3633, 0x42);
		sensor_write(sd, 0x3301, 0x05);
		sensor_write(sd, 0x3622, 0xd6);
		sensor_write(sd, 0x3812, 0x30);
	}
	else if (value>=0x710&&value<=0x71e) {
		sensor_write(sd, 0x3812, 0x00);
		sensor_write(sd, 0x3633, 0x42);
		sensor_write(sd, 0x3301, 0x07);
		sensor_write(sd, 0x3622, 0x16);
		sensor_write(sd, 0x3812, 0x30);
	}
	else {
		sensor_write(sd, 0x3812, 0x00);
		sensor_write(sd, 0x3633, 0x42);
		sensor_write(sd, 0x3301, 0x32);
		sensor_write(sd, 0x3622, 0x16);
		sensor_write(sd, 0x3812, 0x30);
	}

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

	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		wsize->fps = 25 << 16 | 1;
		wsize->regs = sensor_init_regs_1280_720_25fps;
		break;
	case TX_SENSOR_MAX_FPS_15:
		wsize->fps = 15 << 16 | 1;
		wsize->regs = sensor_init_regs_1280_720_15fps;
		break;
	default:
		printk("Now we do not support this framerate!!!\n");
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
		ret = sensor_write_array(sd, sensor_stream_on);
		pr_debug("%s stream on\n", SENSOR_NAME);
	}
	else {
		ret = sensor_write_array(sd, sensor_stream_off);
		pr_debug("%s stream off\n", SENSOR_NAME);
	}
	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int pclk = 0;
	unsigned short hts;
	unsigned short vts = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0; //the format is 24.8
	int ret = 0;

	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		pclk = SENSOR_SUPPORT_PCLK_FPS_30;
		max_fps = SENSOR_OUTPUT_MAX_FPS;
		break;
	case TX_SENSOR_MAX_FPS_15:
		pclk = SENSOR_SUPPORT_PCLK_FPS_15;
		max_fps = TX_SENSOR_MAX_FPS_15;
		break;
	default:
		printk("Now we do not support this framerate!!!\n");
	}

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		printk("warn: fps(%d) not in range\n", fps);
		return -1;
	}
	ret = sensor_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0x320d, &tmp);
	if (ret < 0)
		return -1;
	hts = ((hts << 8) + tmp);

	vts = pclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = sensor_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (ret < 0) {
		printk("err: sensor_write err\n");
		return ret;
	}

	sensor->video.fps = fps;


	sensor_update_actual_fps((fps >> 16) & 0xffff);
	sensor->video.attr->max_integration_time_native = vts - 2;
	sensor->video.attr->integration_time_limit = vts - 2;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 2;
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

		sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	char val = 0;

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
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		} else {
			printk("gpio request fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
		} else {
			printk("gpio request fail %d\n", pwdn_gpio);
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
			ret = sensor_write_array(sd, sensor_stream_off);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			ret = sensor_write_array(sd, sensor_stream_on);
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
	return 0;
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
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
	int ret;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		printk("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));
	/* request mclk of sensor */

//	*(volatile unsigned int*)(0xB0010100) = 0x1;
//	*(volatile unsigned int*)(0xB0010130) = 0xCAAAAAAA;


	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		printk("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		clk_set_rate(sensor->mclk, 24000000);
		break;
	case TX_SENSOR_MAX_FPS_15:
		clk_set_rate(sensor->mclk, 12000000);
		break;
	default:
		printk("Now we do not support this framerate!!!\n");
	}
	clk_enable(sensor->mclk);

	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;

	sensor_attr.dvp.gpio = sensor_gpio_func;

	 /*
		convert sensor-gain into isp-gain,
	 */
	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		break;
	case TX_SENSOR_MAX_FPS_15:
		sensor_attr.max_integration_time_native = 1121;
		sensor_attr.integration_time_limit = 1121;
		sensor_attr.total_width = 2000;
		sensor_attr.total_height = 1125;
		sensor_attr.max_integration_time = 1121;
		break;
	default:
		printk("Now we do not support this framerate!!!\n");
	}
	sensor_attr.max_again = 256041;
	sensor_attr.max_dgain = 0; //sensor_attr.max_dgain;
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

	sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);
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
    { },
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
