// SPDX-License-Identifier: GPL-2.0+
/*
 * ov2732.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <sensor-common.h>
#include <sensor-info.h>
#include <apical-isp/apical_math.h>
#include <linux/proc_fs.h>
#include <soc/gpio.h>

#define SENSOR_NAME "ov2732"
#define SENSOR_CHIP_ID 0x2732
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x36
#define SENSOR_MAX_WIDTH 0
#define SENSOR_MAX_HEIGHT 0
#define SENSOR_CHIP_ID_H (0x00)
#define SENSOR_CHIP_ID_M (0x27)
#define SENSOR_CHIP_ID_L (0x32)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK (44897280)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "20180320"

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

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	unsigned int gain_one = 0;
	unsigned int gain_one1 = 0;
	uint16_t regs = 0;
	unsigned int isp_gain1 = 0;
	uint32_t mask;
	/* low 7 bits are fraction bits, max again 15.5x */
	gain_one = math_exp2(isp_gain, shift, TX_ISP_GAIN_FIXED_POINT);
	if (gain_one >= (uint32_t) (0x7c0 << (TX_ISP_GAIN_FIXED_POINT - 7)))
		gain_one = (uint32_t) (0x7c0 << (TX_ISP_GAIN_FIXED_POINT - 7));
	regs = gain_one >> (TX_ISP_GAIN_FIXED_POINT - 7);
	mask = ~0;
	mask = mask >> (TX_ISP_GAIN_FIXED_POINT - 7);
	mask = mask << (TX_ISP_GAIN_FIXED_POINT - 7);
	gain_one1 = gain_one & mask;
	isp_gain1 = log2_fixed_to_fixed(gain_one1, TX_ISP_GAIN_FIXED_POINT, shift);
	*sensor_again = regs;
	/* printk("info:  isp_gain = 0x%08x, isp_gain1 = 0x%08x, gain_one = 0x%08x, gain_one1 = 0x%08x, sensor_again = 0x%08x\n", isp_gain, isp_gain1, gain_one, gain_one1, regs); */
	return isp_gain1;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi = {
	.clk = 800,
	.lans = 2,
};
struct tx_isp_dvp_bus sensor_dvp = {
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking = {
		.vblanking = 0,
		.hblanking = 0,
	},
};

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = 0x2732,
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
	.max_again = 259142,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 0x58c - 4,
	.integration_time_limit = 0x58c - 4,
	.total_width = 0x4f0,
	.total_height = 0x58c,
	.max_integration_time = 0x58c - 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_1920_1080_25fps_mipi[] = {

	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_25fps_dvp[] = {
	{0x0103, 0x01},
	{0x0305, 0x3c},
	{0x0307, 0x00},
	{0x0308, 0x03},
	{0x0309, 0x06},
	{0x0327, 0x07},
	{0x3016, 0x3e},
	{0x3000, 0x0f},
	{0x3001, 0xff},
	{0x3002, 0xfe},
	{0x3013, 0x00},
	{0x301f, 0xf0},
	{0x3023, 0xf0},
	{0x3020, 0x93},
	{0x3022, 0x51},
	{0x3106, 0x11},
	{0x3107, 0x01},
	{0x3500, 0x00},
	{0x3501, 0x40},
	{0x3502, 0x00},
	{0x3503, 0x88},
	{0x3505, 0x83},
	{0x3508, 0x01},
	{0x3509, 0x80},
	{0x350a, 0x04},
	{0x350b, 0x00},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3510, 0x00},
	{0x3511, 0x00},
	{0x3512, 0x20},
	{0x3600, 0x55},
	{0x3601, 0x54},
	{0x3612, 0xb5},
	{0x3613, 0xb3},
	{0x3616, 0x83},
	{0x3621, 0x00},
	{0x3624, 0x06},
	{0x3642, 0x88},
	{0x3660, 0x00},
	{0x3661, 0x00},
	{0x366a, 0x64},
	{0x366c, 0x00},
	{0x366e, 0x44},
	{0x366f, 0x4f},
	{0x3677, 0x11},
	{0x3678, 0x11},
	{0x3680, 0xff},
	{0x3681, 0xd2},
	{0x3682, 0xa9},
	{0x3683, 0x91},
	{0x3684, 0x8a},
	{0x3620, 0x80},
	{0x3662, 0x10},
	{0x3663, 0x27},
	{0x3665, 0xa1},
	{0x3667, 0xa6},
	{0x3674, 0x00},
	{0x373d, 0x24},
	{0x3741, 0x28},
	{0x3743, 0x28},
	{0x3745, 0x28},
	{0x3747, 0x28},
	{0x3748, 0x00},
	{0x3749, 0x78},
	{0x374a, 0x00},
	{0x374b, 0x78},
	{0x374c, 0x00},
	{0x374d, 0x78},
	{0x374e, 0x00},
	{0x374f, 0x78},
	{0x3766, 0x12},
	{0x37e0, 0x00},
	{0x37e6, 0x04},
	{0x37e5, 0x04},
	{0x37e1, 0x04},
	{0x3737, 0x04},
	{0x37d0, 0x0a},
	{0x37d8, 0x04},
	{0x37e2, 0x08},
	{0x3739, 0x10},
	{0x37e4, 0x18},
	{0x37e3, 0x04},
	{0x37d9, 0x10},
	{0x4040, 0x04},
	{0x4041, 0x0f},
	{0x4008, 0x00},
	{0x4009, 0x0d},
	{0x37a1, 0x14},
	{0x37a8, 0x16},
	{0x37ab, 0x10},
	{0x37c2, 0x04},
	{0x3705, 0x00},
	{0x3706, 0x28},
	{0x370a, 0x00},
	{0x370b, 0x78},
	{0x3714, 0x24},
	{0x371a, 0x1e},
	{0x372a, 0x03},
	{0x3756, 0x00},
	{0x3757, 0x0e},
	{0x377b, 0x00},
	{0x377c, 0x0c},
	{0x377d, 0x20},
	{0x3790, 0x28},
	{0x3791, 0x78},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x04},
	{0x3804, 0x07},
	{0x3805, 0x8f},
	{0x3806, 0x04},
	{0x3807, 0x43},
	{0x3808, 0x07},
	{0x3809, 0x80},
	{0x380a, 0x04},
	{0x380b, 0x38},
	{0x380c, 0x04},
	{0x380d, 0xf0},
	{0x3811, 0x08},
	{0x3813, 0x04},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},
	{0x381d, 0x40},
	{0x381e, 0x02},
	{0x3820, 0x88},
	{0x3821, 0x00},
	{0x3822, 0x04},
	{0x3835, 0x00},
	{0x4303, 0x19},
	{0x4304, 0x19},
	{0x4305, 0x03},
	{0x4306, 0x81},
	{0x4503, 0x00},
	{0x4508, 0x14},
	{0x450a, 0x00},
	{0x450b, 0x40},
	{0x4833, 0x08},
	{0x5000, 0xa9},
	{0x5001, 0x09},
	{0x3b00, 0x00},
	{0x3b02, 0x00},
	{0x3b03, 0x00},
	{0x3c80, 0x08},
	{0x3c82, 0x00},
	{0x3c83, 0xb1},
	{0x3c87, 0x08},
	{0x3c8c, 0x10},
	{0x3c8d, 0x00},
	{0x3c90, 0x00},
	{0x3c91, 0x00},
	{0x3c92, 0x00},
	{0x3c93, 0x00},
	{0x3c94, 0x00},
	{0x3c95, 0x00},
	{0x3c96, 0x00},
	{0x3c97, 0x00},
	{0x3c98, 0x00},
	{0x4000, 0xf3},
	{0x4001, 0x60},
	{0x4002, 0x00},
	{0x4003, 0x40},
	{0x4090, 0x14},
	{0x4601, 0x10},
	{0x4701, 0x00},
	{0x4708, 0x09},
	{0x470a, 0x00},
	{0x470b, 0x40},
	{0x470c, 0x85},
	{0x480c, 0x12},
	{0x4710, 0x06},
	{0x4711, 0x00},
	{0x4837, 0x12},
	{0x4800, 0x00},
	{0x4c01, 0x00},
	{0x5036, 0x00},
	{0x5037, 0x00},
	{0x580b, 0x0f},
	{0x4903, 0x80},
	{0x4003, 0x40},
	{0x5000, 0xf9},
	{0x5200, 0x1b},
	{0x4837, 0x16},
	{0x380e, 0x05},
	{0x380f, 0x8c},
	{0x3500, 0x00},
	{0x3501, 0x49},
	{0x3502, 0x80},
	{0x3508, 0x02},
	{0x3509, 0x80},
	{0x3d8c, 0x11},
	{0x3d8d, 0xf0},
	{0x5180, 0x00},
	{0x5181, 0x10},
	{0x36a0, 0x16},
	{0x36a1, 0x50},
	{0x36a2, 0x60},
	{0x36a3, 0x80},
	{0x36a4, 0x00},
	{0x36a5, 0xa0},
	{0x36a6, 0x00},
	{0x36a7, 0x50},
	{0x36a8, 0x00},
	{0x36a9, 0x50},
	{0x36aa, 0x00},
	{0x36ab, 0x50},
	{0x36ac, 0x00},
	{0x36ad, 0x50},
	{0x36ae, 0x00},
	{0x36af, 0x50},
	{0x36b0, 0x00},
	{0x36b1, 0x50},
	{0x36b2, 0x00},
	{0x36b3, 0x50},
	{0x36b4, 0x00},
	{0x36b5, 0x50},
	{0x36b9, 0xee},
	{0x36ba, 0xee},
	{0x36bb, 0xee},
	{0x36bc, 0xee},
	{0x36bd, 0x0e},
	{0x36b6, 0x08},
	{0x36b7, 0x08},
	{0x36b8, 0x10},
	{0x0305, 0x48},
	{0x0309, 0x04},
	{0x3022, 0x61},
	{0x3600, 0x6a},
	{0x366a, 0x62},
	{0x380c, 0x04},
	{0x380d, 0xf0},
	{0x4002, 0x01},
	{0x4003, 0x00},
	{0x4090, 0x04},
	{0x3705, 0x00},
	{0x3706, 0x5a},
	{0x370a, 0x00},
	{0x370b, 0xfa},
	{0x3748, 0x00},
	{0x3749, 0xfa},
	{0x374a, 0x00},
	{0x374b, 0xfa},
	{0x374c, 0x00},
	{0x374d, 0xfa},
	{0x374e, 0x00},
	{0x374f, 0xfa},
	{0x0100, 0x00},
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

	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {

	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct v4l2_subdev *sd, uint16_t reg,
		unsigned char *value) {
	struct i2c_client *client = v4l2_get_subdevdata(sd);
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
	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct v4l2_subdev *sd, uint16_t reg,
		 unsigned char value) {
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 3,
		.buf = buf,
	};
	int ret;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int sensor_read_array(struct v4l2_subdev *sd, struct regval_list *vals) {
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
		printk("vals->reg_num:0x%02x, vals->value:0x%02x\n", vals->reg_num, val);
		vals++;
	}
	return 0;
}

static int sensor_write_array(struct v4l2_subdev *sd, struct regval_list *vals) {
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

static int sensor_reset(struct v4l2_subdev *sd, u32 val) {
	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd, unsigned int *ident) {
	unsigned char v;
	int ret;

	ret = sensor_read(sd, 0x300a, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x300b, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_M)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	ret = sensor_read(sd, 0x300c, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_integration_time(struct v4l2_subdev *sd, int value) {
	int ret = 0;
	unsigned int expo = value << 4;

	ret = sensor_write(sd, 0x3208, 0x01);
	ret += sensor_write(sd, 0x3502, (unsigned char) (expo & 0xff));
	ret += sensor_write(sd, 0x3501, (unsigned char) ((expo >> 8) & 0xff));
	ret += sensor_write(sd, 0x3500, (unsigned char) ((expo >> 16) & 0xf));
	ret += sensor_write(sd, 0x3208, 0x11);
	ret += sensor_write(sd, 0x3208, 0xa1);
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct v4l2_subdev *sd, int value) {
	int ret = 0;

	ret = sensor_write(sd, 0x3208, 0x02);
	ret += sensor_write(sd, 0x3509, (unsigned char) (value & 0xff));
	ret += sensor_write(sd, 0x3508, (unsigned char) ((value >> 8) & 0xff));
	ret += sensor_write(sd, 0x3208, 0x12);
	ret += sensor_write(sd, 0x3208, 0xa2);
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_digital_gain(struct v4l2_subdev *sd, int value) {
	return 0;
}

static int sensor_get_black_pedestal(struct v4l2_subdev *sd, int value) {
	return 0;
}

static int sensor_init(struct v4l2_subdev *sd, u32 enable) {
	struct tx_isp_sensor *sensor = (container_of(sd, struct tx_isp_sensor, sd));
	struct tx_isp_notify_argument arg;
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
	arg.value = (int) &sensor->video;
	sd->v4l2_dev->notify(sd, TX_ISP_NOTIFY_SYNC_VIDEO_IN, &arg);
	sensor->priv = wsize;
	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable) {
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

	} else {
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

static int sensor_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms) {
	return 0;
}

static int sensor_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms) {
	return 0;
}

static int sensor_set_fps(struct tx_isp_sensor *sensor, int fps) {
	struct v4l2_subdev *sd = &sensor->sd;
	struct tx_isp_notify_argument arg;
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		return -1;
	}
	sclk = SENSOR_SUPPORT_SCLK;

	val = 0;
	ret += sensor_read(sd, 0x380c, &val);
	hts = val << 8;
	val = 0;
	ret += sensor_read(sd, 0x380d, &val);
	hts |= val;
	if (0 != ret) {
		printk("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += sensor_write(sd, 0x3208, 0x00);
	ret += sensor_write(sd, 0x380f, vts & 0xff);
	ret += sensor_write(sd, 0x380e, (vts >> 8) & 0xff);
	ret += sensor_write(sd, 0x3208, 0x10);
	ret += sensor_write(sd, 0x3208, 0xa0);
	if (0 != ret) {
		printk("err: sensor_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	arg.value = (int) &sensor->video;
	sd->v4l2_dev->notify(sd, TX_ISP_NOTIFY_SYNC_VIDEO_IN, &arg);

	return ret;
}

static int sensor_set_mode(struct tx_isp_sensor *sensor, int value) {
	struct tx_isp_notify_argument arg;
	struct v4l2_subdev *sd = &sensor->sd;
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
		arg.value = (int) &sensor->video;
		sd->v4l2_dev->notify(sd, TX_ISP_NOTIFY_SYNC_VIDEO_IN, &arg);
	}
	return ret;
}

static int sensor_g_chip_ident(struct v4l2_subdev *sd,
			       struct v4l2_dbg_chip_ident *chip) {
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if (reset_gpio != -1) {
		ret = gpio_request(reset_gpio, "sensor_reset");
		if (!ret) {
			gpio_direction_output(reset_gpio, 1);
			msleep(10);
			gpio_direction_output(reset_gpio, 0);
			msleep(10);
			gpio_direction_output(reset_gpio, 1);
			msleep(10);
		} else {
			printk("gpio requrest fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			gpio_direction_output(pwdn_gpio, 0);
			msleep(10);
			gpio_direction_output(pwdn_gpio, 1);
			msleep(10);
		} else {
			printk("gpio requrest fail %d\n", pwdn_gpio);
		}
	}
	ret = sensor_detect(sd, &ident);
	if (ret) {
		v4l_err(client, "chip found @ 0x%x (%s) is not an %s chip.\n",
			client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}
	v4l_info(client, "%s chip found @ 0x%02x (%s)\n",
		 SENSOR_NAME, client->addr, client->adapter->name);
	return v4l2_chip_ident_i2c_client(client, chip, ident, 0);
}

static int sensor_s_power(struct v4l2_subdev *sd, int on) {
	return 0;
}

static long sensor_ops_private_ioctl(struct tx_isp_sensor *sensor, struct isp_private_ioctl *ctrl) {
	struct v4l2_subdev *sd = &sensor->sd;
	long ret = 0;
	switch (ctrl->cmd) {
		case TX_ISP_PRIVATE_IOCTL_SENSOR_INT_TIME:
			ret = sensor_set_integration_time(sd, ctrl->value);
			break;
		case TX_ISP_PRIVATE_IOCTL_SENSOR_AGAIN:
			ret = sensor_set_analog_gain(sd, ctrl->value);
			break;
		case TX_ISP_PRIVATE_IOCTL_SENSOR_DGAIN:
			ret = sensor_set_digital_gain(sd, ctrl->value);
			break;
		case TX_ISP_PRIVATE_IOCTL_SENSOR_BLACK_LEVEL:
			ret = sensor_get_black_pedestal(sd, ctrl->value);
			break;
		case TX_ISP_PRIVATE_IOCTL_SENSOR_RESIZE:
			ret = sensor_set_mode(sensor, ctrl->value);
			break;
		case TX_ISP_PRIVATE_IOCTL_SUBDEV_PREPARE_CHANGE:
			if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
				ret = sensor_write_array(sd, sensor_stream_off_dvp);
			} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
				ret = sensor_write_array(sd, sensor_stream_off_mipi);

			} else {
				printk("Don't support this Sensor Data interface\n");
			}
			break;
		case TX_ISP_PRIVATE_IOCTL_SUBDEV_FINISH_CHANGE:
			if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
				ret = sensor_write_array(sd, sensor_stream_on_dvp);
			} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
				ret = sensor_write_array(sd, sensor_stream_on_mipi);

			} else {
				printk("Don't support this Sensor Data interface\n");
			}
			break;
		case TX_ISP_PRIVATE_IOCTL_SENSOR_FPS:
			ret = sensor_set_fps(sensor, ctrl->value);
			break;
		default:
			pr_debug("do not support ctrl->cmd ====%d\n", ctrl->cmd);
			break;
	}
	return 0;
}

static long sensor_ops_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg) {
	struct tx_isp_sensor *sensor = container_of(sd, struct tx_isp_sensor, sd);
	int ret;
	switch (cmd) {
		case VIDIOC_ISP_PRIVATE_IOCTL:
			ret = sensor_ops_private_ioctl(sensor, arg);
			break;
		default:
			return -1;
			break;
	}
	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int sensor_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = sensor_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int sensor_s_register(struct v4l2_subdev *sd, const struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	sensor_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops sensor_core_ops = {
	.g_chip_ident = sensor_g_chip_ident,
	.reset = sensor_reset,
	.init = sensor_init,
	.s_power = sensor_s_power,
	.ioctl = sensor_ops_ioctl,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = sensor_g_register,
	.s_register = sensor_s_register,
#endif
};

static const struct v4l2_subdev_video_ops sensor_video_ops = {
	.s_stream = sensor_s_stream,
	.s_parm = sensor_s_parm,
	.g_parm = sensor_g_parm,
};

static const struct v4l2_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
};

static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id) {
	struct v4l2_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
	int ret;

	sensor = (struct tx_isp_sensor *) kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		printk("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		printk("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	clk_set_rate(sensor->mclk, 24000000);
	clk_enable(sensor->mclk);

	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;

	sensor_attr.dbus_type = data_interface;
	if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
		wsize->regs = sensor_init_regs_1920_1080_25fps_dvp;
		memcpy((void *) (&(sensor_attr.dvp)), (void *) (&sensor_dvp), sizeof(sensor_dvp));
	} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
		wsize->regs = sensor_init_regs_1920_1080_25fps_mipi;
		memcpy((void *) (&(sensor_attr.mipi)), (void *) (&sensor_mipi), sizeof(sensor_mipi));
	} else {
		printk("Don't support this Sensor Data Output Interface.\n");
		goto err_set_sensor_data_interface;
	}

	sensor_attr.max_again = 259142;
	sensor_attr.max_dgain = 0;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sensor_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	v4l2_i2c_subdev_init(sd, client, &sensor_ops);
	v4l2_set_subdev_hostdata(sd, sensor);
	pr_debug("probe ok ------->%s\n", SENSOR_NAME);
	return 0;
err_set_sensor_data_interface:
err_set_sensor_gpio:
	clk_disable(sensor->mclk);
	clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int sensor_remove(struct i2c_client *client) {
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = v4l2_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		gpio_free(pwdn_gpio);

	clk_disable(sensor->mclk);
	clk_put(sensor->mclk);

	v4l2_device_unregister_subdev(sd);
	kfree(sensor);
	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{SENSOR_NAME, 0},
	{}
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

static __init int init_sensor(void) {
	return i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
