// SPDX-License-Identifier: GPL-2.0+
/*
 * bf3115.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <sensor-common.h>
#include <sensor-info.h>
#include <apical-isp/apical_math.h>
#include <soc/gpio.h>

#define SENSOR_NAME "bf3115"
#define SENSOR_CHIP_ID 0x3116
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x6e
#define SENSOR_MAX_WIDTH 1280
#define SENSOR_MAX_HEIGHT 720
#define SENSOR_CHIP_ID_H (0x31)
#define SENSOR_CHIP_ID_L (0x16)
#define SENSOR_REG_END 0x6e
#define SENSOR_REG_DELAY 0xff
#define SENSOR_PAGE_REG 0xfe
#define SENSOR_SUPPORT_PCLK (37125*1000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "20180320"

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
	unsigned char reg_num;
	unsigned char value;
};

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_12BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

struct again_lut {
	unsigned char value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x0, 0},
	{0x1, 7171},
	{0x2, 10416},
	{0x3, 16402},
	{0x4, 21931},
	{0x5, 28705},
	{0x6, 32480},
	{0x7, 37081},
	{0x8, 41420},
	{0x9, 46047},
	{0xa, 49126},
	{0xb, 53613},
	{0xc, 56692},
	{0xd, 60578},
	{0xe, 62915},
	{0xf, 66284},
	{0x10, 67802},
	{0x11, 73251},
	{0x12, 76913},
	{0x13, 84146},
	{0x14, 88750},
	{0x15, 95691},
	{0x16, 98485},
	{0x17, 103207},
	{0x18, 106955},
	{0x19, 112051},
	{0x1a, 115152},
	{0x1b, 119652},
	{0x1c, 122145},
	{0x1d, 126245},
	{0x1e, 128230},
	{0x1f, 131707},
	{0x20, 132006},
	{0x21, 138485},
	{0x22, 141845},
	{0x23, 147176},
	{0x24, 150660},
	{0x25, 155533},
	{0x26, 158886},
	{0x27, 163505},
	{0x28, 166755},
	{0x29, 171145},
	{0x2a, 173668},
	{0x2b, 178832},
	{0x2c, 181863},
	{0x2d, 185709},
	{0x2e, 188188},
	{0x2f, 191595},
	{0x30, 194579},
	{0x31, 201401},
	{0x32, 204724},
	{0x33, 210680},
	{0x34, 214524},
	{0x35, 220337},
	{0x36, 223047},
	{0x37, 228368},
	{0x38, 231991},
	{0x39, 236433},
	{0x3a, 238851},
	{0x3b, 242818},
	{0x3c, 245680},
	{0x3d, 249674},
	{0x3e, 251914},
	{0x3f, 255344},
	{0x40, 259111},
	{0x41, 265774},
	{0x42, 269059},
	{0x43, 275177},
	{0x44, 279223},
	{0x45, 285352},
	{0x46, 288537},
	{0x47, 293819},
	{0x48, 297391},
	{0x49, 301837},
	{0x4a, 304191},
	{0x4b, 308176},
	{0x4c, 310861},
	{0x4d, 314519},
	{0x4e, 316419},
	{0x4f, 319872},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = 0;
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

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return isp_gain;
}

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
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
	.max_again = 255344,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 748,
	.integration_time_limit = 748,
	.total_width = 1980,
	.total_height = 750,
	.max_integration_time = 748,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_1280_720_25fps[] = {
	{0xfe, 0x00},
	{0x3d, 0xff},
	{0xe8, 0x10},
	{0x00, 0x60},
	{0x02, 0x10},
	{0xfe, 0x01},
	{0x04, 0x4f},
	{0x00, 0x38},
	{0x10, 0x10},
	{0x0e, 0x01},
	{0x0f, 0xcb},
	{0x09, 0x02},
	{0x1b, 0x10},
	{0x42, 0x50},
	{0x43, 0x20},
	{0x44, 0x04},
	{0x45, 0x04},
	{0x46, 0x08},
	{0x47, 0xd8},
	{0xfe, 0x00},
	{0xe1, 0xf7},
	{0xe2, 0x2b},
	{0xe3, 0x15},
	{0xe4, 0x58},
	{0xe5, 0x41},
	{0xe7, 0x00},
	{0xe9, 0x20},
	{0xec, 0x12},
	{0xb2, 0x91},
	{0xb0, 0x00},
	{0xb1, 0x00},
	{0xb2, 0x90},
	{0xb3, 0x00},
	{0xf1, 0x01},
	{0xf1, 0x01},
	{0x6f, 0xd2},
	{0x72, 0x00},
	{0x73, 0x00},
	{0xe8, 0x00},
	{0xea, 0x11},
	{0x82, 0x27},
	{0x80, 0x0c},
	{0x81, 0x8e},
	{0x85, 0x06},

	{0xfe, 0x00},
	{0x09, 0x01},
	{0x0a, 0xd0},
	{0x0b, 0x04},
	{0x0c, 0x00},
	{0x0d, 0x04},
	{0x0e, 0x00},

	{0xfe, 0x01},
	{0xf1, 0x80},/*disable all the isp function inside the sensor*/
	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1280*800 */
	{
		.width = 1280,
		.height = 720,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR12_1X12,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1280_720_25fps,
	}
};

static struct regval_list sensor_stream_on[] = {
	{0xfe, 0x00},
	{0xe8, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	{0xfe, 0x00},
	{0xe8, 0x01},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct v4l2_subdev *sd, unsigned char reg, unsigned char *value) {
	struct i2c_client *client = v4l2_get_subdevdata(sd);
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
	int ret;
	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int sensor_write(struct v4l2_subdev *sd, unsigned char reg, unsigned char value) {
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
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
			if (vals->reg_num == SENSOR_PAGE_REG) {
				ret = sensor_write(sd, vals->reg_num, val);
				ret = sensor_read(sd, vals->reg_num, &val);
			}
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
	unsigned char v;
	int ret;
	ret = sensor_read(sd, 0xfc, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;

	*ident = v;
	ret = sensor_read(sd, 0xfd, &v);
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
	ret = sensor_write(sd, 0xfe, 0x01);
	if (ret < 0) {
		printk("sensor_write error\n");
		return ret;
	}

	ret = sensor_write(sd, 0x0f, value & 0xff);
	if (ret < 0) {
		printk("sensor_write error\n");
		return ret;
	}

	ret = sensor_write(sd, 0x0e, (value & 0xff00) >> 8);
	if (ret < 0) {
		printk("sensor_write error\n");
		return ret;
	}

	return 0;
}

static int sensor_set_analog_gain(struct v4l2_subdev *sd, int value) {
	int ret = 0;
	ret = sensor_write(sd, 0x10, (unsigned char) ((value + 0x10) & 0xff));
	if (ret < 0) {
		printk("sensor_write analog gain error\n");
		return ret;
	}

	return 0;
}

static int sensor_set_digital_gain(struct v4l2_subdev *sd, int value) {
	/* 0x00 bit[7] if gain > 2X set 0; if gain > 4X set 1 */
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
		ret = sensor_write_array(sd, sensor_stream_on);
		pr_debug("%s stream on\n", SENSOR_NAME);
	} else {
		ret = sensor_write_array(sd, sensor_stream_off);
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
	struct tx_isp_notify_argument arg;
	struct v4l2_subdev *sd = &sensor->sd;
	unsigned int pclk = SENSOR_SUPPORT_PCLK;
	unsigned short line_length_df = 1516;
	unsigned short frame_length_df = 742;
	unsigned short vts = 0;
	unsigned short hb = 0;
	unsigned short vb;
	unsigned short vb_af = 4;
	unsigned short vb_bf = 4;
	unsigned short hts = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;
	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		printk("warn: fps(%d) not in range\n", fps);
		return -1;
	}

	ret = sensor_write(sd, 0xfe, 0x00);
	if (ret < 0) {
		printk("sensor_write error\n");
		return ret;
	}

	ret = sensor_read(sd, 0x09, &tmp);
	hb = tmp;
	ret += sensor_read(sd, 0x0a, &tmp);
	if (ret < 0)
		return ret;

	hb = ((hb & 0x0f) << 8) + tmp;
	hts = line_length_df + hb;
	vts = pclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	vb = vts - frame_length_df;
	vb_bf = vb - vb_af;
	ret = sensor_write(sd, 0x0b, (unsigned char) (vb_bf & 0xff));
	ret += sensor_write(sd, 0x0c, (unsigned char) (vb_bf >> 8));
	if (ret < 0) {
		printk("err: sensor_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 2;
	sensor->video.attr->integration_time_limit = vts - 2;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 2;
	arg.value = (int) &sensor->video;
	sd->v4l2_dev->notify(sd, TX_ISP_NOTIFY_SYNC_VIDEO_IN, &arg);
	return 0;
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
		if (sensor->priv != wsize) {
			ret = sensor_write_array(sd, wsize->regs);
			if (!ret) {
				sensor->priv = wsize;
			}
		}
		sensor->video.fps = wsize->fps;
		arg.value = (int) &sensor->video;
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
			msleep(20);
			gpio_direction_output(reset_gpio, 0);
			msleep(20);
			gpio_direction_output(reset_gpio, 1);
			msleep(20);
		} else {
			printk("gpio requrest fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			gpio_direction_output(pwdn_gpio, 1);
			msleep(150);
			gpio_direction_output(pwdn_gpio, 0);
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
			ret = sensor_write_array(sd, sensor_stream_off);
			break;
		case TX_ISP_PRIVATE_IOCTL_SUBDEV_FINISH_CHANGE:
			ret = sensor_write_array(sd, sensor_stream_on);
			break;
		case TX_ISP_PRIVATE_IOCTL_SENSOR_FPS:
			ret = sensor_set_fps(sensor, ctrl->value);
			break;
		default:
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
static int sensor_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg) {
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
	int ret = 0;
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

	sensor_attr.dvp.gpio = sensor_gpio_func;
	/*
		   convert sensor-gain into isp-gain,
	*/
	sensor_attr.max_again = 255344;
	sensor_attr.max_dgain = sensor_attr.max_dgain;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sensor_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	v4l2_i2c_subdev_init(sd, client, &sensor_ops);
	v4l2_set_subdev_hostdata(sd, sensor);
	pr_debug("probe ok ------->%s\n", SENSOR_NAME);
	return 0;

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
	sensor_common_init(&sensor_info);
	return i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	sensor_common_exit();
	i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
