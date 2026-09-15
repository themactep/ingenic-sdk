// SPDX-License-Identifier: GPL-2.0+
/*
 * gc1084.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 * Settings:
 * sboot      resolution      fps      interface        mode
 *   0         1920*720       30       mipi_1lane      linear
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
#include <sensor-common.h>
#include <sensor-info.h>
#include <apical-isp/apical_math.h>

// ============================================================================
// SENSOR IDENTIFICATION
// ============================================================================
#define SENSOR_NAME "gc1084"
#define SENSOR_VERSION "H20231012a"
#define SENSOR_CHIP_ID 0x1084
#define SENSOR_CHIP_ID_H (0x10)
#define SENSOR_CHIP_ID_L (0x84)

// ============================================================================
// HARDWARE INTERFACE
// ============================================================================
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x37

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
#define SENSOR_SUPPORT_SCLK (49500000) /* 2200 * 750 * 30 */
#define SENSOR_OUTPUT_MAX_FPS 30
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
	uint16_t value;
};

struct again_lut {
	int index;
	unsigned char reg0d1;
	unsigned char reg0d0;
	unsigned char regdc1;
	unsigned char reg0b8;
	unsigned char reg0b9;
	unsigned char reg155;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0},	    //1.000000
	{0x01, 0x0a, 0x00, 0x00, 0x01, 0x0c, 0x00, 16208},  //1.187500
	{0x02, 0x00, 0x01, 0x00, 0x01, 0x1a, 0x00, 32217},  //1.406250
	{0x03, 0x0a, 0x01, 0x00, 0x01, 0x2a, 0x00, 47690},  //1.656250
	{0x04, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 65536},  //2.000000
	{0x05, 0x0a, 0x02, 0x00, 0x02, 0x18, 0x00, 81784},  //2.375000
	{0x06, 0x00, 0x03, 0x00, 0x02, 0x33, 0x00, 97213},  //2.796875
	{0x07, 0x0a, 0x03, 0x00, 0x03, 0x14, 0x00, 113226}, //3.312500
	{0x08, 0x00, 0x04, 0x00, 0x04, 0x00, 0x02, 131072}, //4.000000
	{0x09, 0x0a, 0x04, 0x00, 0x04, 0x2f, 0x02, 147001}, //4.734375
	{0x0a, 0x00, 0x05, 0x00, 0x05, 0x26, 0x02, 162766}, //5.593750
	{0x0b, 0x0a, 0x05, 0x00, 0x06, 0x29, 0x02, 178990}, //6.640625
	{0x0c, 0x00, 0x06, 0x00, 0x08, 0x00, 0x02, 196608}, //8.000000
	{0x0d, 0x0a, 0x06, 0x00, 0x09, 0x1f, 0x04, 212696}, //9.484375
	{0x0e, 0x12, 0x46, 0x00, 0x0b, 0x0d, 0x04, 228311}, //11.203125
	{0x0f, 0x19, 0x66, 0x00, 0x0d, 0x12, 0x06, 244312}, //13.265625
	{0x10, 0x00, 0x04, 0x01, 0x10, 0x00, 0x06, 262144}, //16.000000
	{0x11, 0x0a, 0x04, 0x01, 0x12, 0x3e, 0x08, 278232}, //18.953125
	{0x12, 0x00, 0x05, 0x01, 0x16, 0x1a, 0x0a, 293982}, //22.406250
	{0x13, 0x0a, 0x05, 0x01, 0x1a, 0x23, 0x0c, 310012}, //26.546875
	{0x14, 0x00, 0x06, 0x01, 0x20, 0x00, 0x0c, 327680}, //32.000000
	{0x15, 0x0a, 0x06, 0x01, 0x25, 0x3b, 0x0f, 343731}, //37.921875
	{0x16, 0x12, 0x46, 0x01, 0x2c, 0x33, 0x12, 359419}, //44.796875
	{0x17, 0x19, 0x66, 0x01, 0x35, 0x06, 0x14, 375411}, //53.093750
	{0x18, 0x20, 0x06, 0x01, 0x3f, 0x3f, 0x15, 393216}, //64.000000
	{0x19, 0x0a, 0x04, 0x01, 0x4c, 0x38, 0x17, 467589}, //75.840
	{0x1a, 0x00, 0x04, 0x01, 0x59, 0x26, 0x18, 551000}, //89.582
	{0x1b, 0x0a, 0x05, 0x01, 0x69, 0x0c, 0x1a, 643415}, //106.155
	{0x1c, 0x00, 0x05, 0x01, 0x80, 0x00, 0x1b, 744532}, //128.000
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	struct again_lut *lut = sensor_again_lut;

	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].index;
			return lut[0].gain;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return isp_gain;
}

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi =
		{
			.clk = 396,
			.lans = 1,
		},
	.max_again = 744532, // - 128x
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 734,
	.integration_time_limit = 734,
	.total_width = 2200,
	.total_height = 750,
	.max_integration_time = 734,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_1280_720_30fps_mipi[] = {
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03f2, 0x00},
	{0x03f3, 0x00},
	{0x03f4, 0x36},
	{0x03f5, 0xc0},
	{0x03f6, 0x13},
	{0x03f7, 0x01},
	{0x03f8, 0x2c},
	{0x03f9, 0x21},
	{0x03fc, 0xae},
	{0x0d05, 0x08}, //hts 0x898 = 2200
	{0x0d06, 0x98}, //
	{0x0d08, 0x10},
	{0x0d0a, 0x02},
	{0x000c, 0x03},
	{0x0d0d, 0x02},
	{0x0d0e, 0xd4},
	{0x000f, 0x05},
	{0x0010, 0x08},
	{0x0017, 0x08},
	{0x0d73, 0x92},
	{0x0076, 0x00},
	{0x0d76, 0x00},
	{0x0d41, 0x02}, //vts 0x2ee = 750
	{0x0d42, 0xee},
	{0x0d7a, 0x0a},
	{0x006b, 0x18},
	{0x0db0, 0x9d},
	{0x0db1, 0x00},
	{0x0db2, 0xac},
	{0x0db3, 0xd5},
	{0x0db4, 0x00},
	{0x0db5, 0x97},
	{0x0db6, 0x09},
	{0x00d2, 0xfc},
	{0x0d19, 0x31},
	{0x0d20, 0x40},
	{0x0d25, 0xcb},
	{0x0d27, 0x03},
	{0x0d29, 0x40},
	{0x0d43, 0x20},
	{0x0058, 0x60},
	{0x00d6, 0x66},
	{0x00d7, 0x19},
	{0x0093, 0x02},
	{0x00d9, 0x14},
	{0x00da, 0xc1},
	{0x0d2a, 0x00},
	{0x0d28, 0x04},
	{0x0dc2, 0x84},
	{0x0050, 0x30},
	{0x0080, 0x07},
	{0x008c, 0x05},
	{0x008d, 0xa8},
	{0x0077, 0x01},
	{0x0078, 0xee},
	{0x0079, 0x02},
	{0x0067, 0xc0},
	{0x0054, 0xff},
	{0x0055, 0x02},
	{0x0056, 0x00},
	{0x0057, 0x04},
	{0x005a, 0xff},
	{0x005b, 0x07},
	{0x00d5, 0x03},
	{0x0102, 0xa9},
	{0x0d03, 0x02},
	{0x0d04, 0xd0},
	{0x007a, 0x60},
	{0x04e0, 0xff},
	{0x0414, 0x75},
	{0x0415, 0x75},
	{0x0416, 0x75},
	{0x0417, 0x75},
	{0x0122, 0x00},
	{0x0121, 0x80},
	{0x0428, 0x10},
	{0x0429, 0x10},
	{0x042a, 0x10},
	{0x042b, 0x10},
	{0x042c, 0x14},
	{0x042d, 0x14},
	{0x042e, 0x18},
	{0x042f, 0x18},
	{0x0430, 0x05},
	{0x0431, 0x05},
	{0x0432, 0x05},
	{0x0433, 0x05},
	{0x0434, 0x05},
	{0x0435, 0x05},
	{0x0436, 0x05},
	{0x0437, 0x05},
	{0x0153, 0x00},
	{0x0190, 0x01},
	{0x0192, 0x02},
	{0x0194, 0x04},
	{0x0195, 0x02},
	{0x0196, 0xd0},
	{0x0197, 0x05},
	{0x0198, 0x00},
	{0x0201, 0x23},
	{0x0202, 0x53}, //0x53
	{0x0203, 0xce},
	{0x0208, 0x39},
	{0x0212, 0x06},
	{0x0213, 0x40},
	{0x0215, 0x12},
	{0x0229, 0x05},
	{0x023e, 0x98},
	{0x031e, 0x3e},
	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1280*720 */
	{
		.width = 1280,
		.height = 720,
		.fps = 30 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SGRBG10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1280_720_30fps_mipi,
	}};

static enum v4l2_mbus_pixelcode sensor_mbus_code[] = {
	V4L2_MBUS_FMT_SGRBG10_1X10,
};

static struct regval_list sensor_stream_on_mipi[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct v4l2_subdev *sd, unsigned short reg, unsigned char *value) {
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};

	struct i2c_msg msg[2] = {[0] =
					 {
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

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct v4l2_subdev *sd, unsigned short reg, unsigned char value) {
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	unsigned char buf[3] = {reg >> 8, reg & 0xff, value};
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
	int ret;
	unsigned char v;

	ret = sensor_read(sd, 0x03f0, &v);
	ISP_INFO("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;

	*ident = v;

	ret = sensor_read(sd, 0x03f1, &v);
	ISP_INFO("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;

	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_integration_time(struct v4l2_subdev *sd, int value) {
	int ret = 0;

	ret += sensor_write(sd, 0x0d04, value & 0xff);
	ret += sensor_write(sd, 0x0d03, value >> 8);

	return ret;
}

static int sensor_set_analog_gain(struct v4l2_subdev *sd, int value) {
	int ret = 0;
	struct again_lut *val_lut = sensor_again_lut;

	ret += sensor_write(sd, 0x00d1, val_lut[value].reg0d1);
	ret += sensor_write(sd, 0x00d0, val_lut[value].reg0d0);
	ret += sensor_write(sd, 0x031d, 0x2d);
	ret += sensor_write(sd, 0x0dc1, val_lut[value].regdc1);
	ret += sensor_write(sd, 0x031d, 0x28);
	ret += sensor_write(sd, 0x00b8, val_lut[value].reg0b8);
	ret += sensor_write(sd, 0x00b9, val_lut[value].reg0b9);
	ret += sensor_write(sd, 0x0155, val_lut[value].reg155);

	return ret;
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

	arg.value = (int)&sensor->video;
	sd->v4l2_dev->notify(sd, TX_ISP_NOTIFY_SYNC_VIDEO_IN, &arg);
	sensor->priv = wsize;
	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable) {
	int ret = 0;
	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
		} else {
			ISP_INFO("Don't support this Sensor Data interface\n");
		}
		ISP_INFO("%s stream on\n", SENSOR_NAME);
	} else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);
		} else {
			ISP_INFO("Don't support this Sensor Data interface\n");
		}
		ISP_INFO("%s stream off\n", SENSOR_NAME);
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
	struct tx_isp_notify_argument arg;
	struct v4l2_subdev *sd = &sensor->sd;
	unsigned int pclk = SENSOR_SUPPORT_SCLK;
	unsigned short hts = 0;
	unsigned short vts = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;
	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8))
		return -1;
	ret += sensor_read(sd, 0xd05, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0xd06, &tmp);
	if (ret < 0)
		return -1;

	hts = ((hts << 8) + tmp);
	vts = pclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += sensor_write(sd, 0x0d41, (unsigned char)((vts & 0xff00) >> 8));
	ret += sensor_write(sd, 0x0d42, (unsigned char)(vts & 0xff));
	if (ret < 0)
		return -1;

	sensor->video.fps = fps;

	sensor->video.attr->max_integration_time_native = vts - 16;
	sensor->video.attr->integration_time_limit = vts - 16;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 16;
	arg.value = (int)&sensor->video;
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

		arg.value = (int)&sensor->video;
		sd->v4l2_dev->notify(sd, TX_ISP_NOTIFY_SYNC_VIDEO_IN, &arg);
	}
	return ret;
}

static int sensor_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip) {
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if (reset_gpio != -1) {
		ret = gpio_request(reset_gpio, "sensor_reset");
		if (!ret) {
			gpio_direction_output(reset_gpio, 1);
			msleep(5);
			gpio_direction_output(reset_gpio, 0);
			msleep(10);
			gpio_direction_output(reset_gpio, 1);
			msleep(10);
		} else {
			ISP_INFO("gpio request fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			gpio_direction_output(pwdn_gpio, 1);
			msleep(10);
			gpio_direction_output(pwdn_gpio, 0);
			msleep(10);
		} else {
			ISP_INFO("gpio request fail %d\n", pwdn_gpio);
		}
	}
	ret = sensor_detect(sd, &ident);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an %s chip.\n",
			client->addr,
			client->adapter->name,
			SENSOR_NAME);
		return ret;
	}

	v4l_info(client, "%s chip found @ 0x%02x (%s)\n", SENSOR_NAME, client->addr, client->adapter->name);
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
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);
		} else {
			ISP_INFO("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_PRIVATE_IOCTL_SUBDEV_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
		} else {
			ISP_INFO("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_PRIVATE_IOCTL_SENSOR_FPS:
		ret = sensor_set_fps(sensor, ctrl->value);
		break;
	default:
		ISP_INFO("do not support ctrl->cmd ====%d\n", ctrl->cmd);
		break;
	}
	return ret;
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
static int sensor_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg) {
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val;
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

static int sensor_s_register(struct v4l2_subdev *sd, const struct v4l2_dbg_register *reg) {
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

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id) {
	struct v4l2_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
	unsigned int rate = 0;
	int ret;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_INFO("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_INFO("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	rate = clk_get_rate(clk_get_parent(sensor->mclk));
	if (((rate / 1000) % 27000) != 0) {
		struct clk *vpll;
		vpll = clk_get(NULL, "vpll");
		if (IS_ERR(vpll)) {
			ISP_INFO("get vpll failed\n");
		} else {
			rate = clk_get_rate(vpll);
			if (((rate / 1000) % 27000) != 0) {
				clk_set_rate(vpll, 1080000000);
			}
			ret = clk_set_parent(sensor->mclk, vpll);
			if (ret < 0)
				ISP_INFO("set mclk parent as epll err\n");
		}
	}

	clk_set_rate(sensor->mclk, 27000000);
	clk_enable(sensor->mclk);

	/*
	 * convert sensor-gain into isp-gain,
	 */
	sensor_attr.dbus_type = data_interface;
	sensor_attr.max_dgain = 0;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sensor_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	v4l2_i2c_subdev_init(sd, client, &sensor_ops);
	v4l2_set_subdev_hostdata(sd, sensor);
	ISP_INFO("probe ok ------->%s\n", SENSOR_NAME);
	return 0;

err_get_mclk:
	clk_disable(sensor->mclk);
	clk_put(sensor->mclk);
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

static const struct i2c_device_id sensor_id[] = {{SENSOR_NAME, 0}, {}};
MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver =
		{
			.owner = THIS_MODULE,
			.name = SENSOR_NAME,
		},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

static __init int init_sensor(void) {
	sensor_common_init(&sensor_info);
	return i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	sensor_common_exit();
	i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for " SENSOR_NAME " sensor");
MODULE_LICENSE("GPL");
