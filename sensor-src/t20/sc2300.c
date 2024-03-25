// SPDX-License-Identifier: GPL-2.0+
/*
 * sc2300.c
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

#define SENSOR_NAME "sc2300"
#define SENSOR_CHIP_ID 0x2300
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x30
#define SENSOR_MAX_WIDTH 1920
#define SENSOR_MAX_HEIGHT 1080
#define SENSOR_CHIP_ID_H (0x23)
#define SENSOR_CHIP_ID_L (0x00)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_PCLK (72000*1000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "20180320"
#define DRIVE_CAPABILITY_1

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_12BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

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
	{0x0320, 0},
	{0x0321, 2909},
	{0x0322, 5419},
	{0x0323, 8269},
	{0x0324, 10960},
	{0x0325, 13345},
	{0x0326, 15785},
	{0x0327, 18882},
	{0x0328, 21361},
	{0x0329, 23774},
	{0x032a, 26292},
	{0x032b, 28423},
	{0x032c, 30588},
	{0x032d, 32545},
	{0x032e, 34770},
	{0x032f, 36714},
	{0x0330, 38769},
	{0x0331, 40639},
	{0x0332, 42545},
	{0x0333, 44342},
	{0x0334, 46170},
	{0x0335, 48100},
	{0x0336, 49728},
	{0x0337, 51460},
	{0x0338, 53282},
	{0x0339, 54890},
	{0x033a, 56587},
	{0x033b, 58140},
	{0x033c, 59664},
	{0x033d, 61282},
	{0x033e, 62815},
	{0x033f, 64434},
	{0x0720, 65535},
	{0x0721, 68444},
	{0x0722, 70954},
	{0x0723, 73804},
	{0x0724, 76495},
	{0x0725, 78880},
	{0x0726, 81320},
	{0x2320, 82772},
	{0x2321, 85682},
	{0x2322, 88192},
	{0x2323, 91042},
	{0x2324, 93733},
	{0x2325, 96118},
	{0x2326, 98557},
	{0x2327, 101655},
	{0x2328, 104134},
	{0x2329, 106547},
	{0x232a, 109065},
	{0x232b, 111195},
	{0x232c, 113361},
	{0x232d, 115318},
	{0x232e, 117543},
	{0x232f, 119487},
	{0x2330, 121542},
	{0x2331, 123412},
	{0x2332, 125318},
	{0x2333, 127115},
	{0x2334, 128943},
	{0x2335, 130873},
	{0x2336, 132501},
	{0x2337, 134233},
	{0x2338, 136055},
	{0x2339, 137663},
	{0x233a, 139359},
	{0x233b, 140913},
	{0x233c, 142437},
	{0x233d, 144055},
	{0x233e, 145588},
	{0x233f, 147207},
	{0x2720, 148307},
	{0x2721, 151217},
	{0x2722, 153727},
	{0x2723, 156577},
	{0x2724, 159268},
	{0x2725, 161653},
	{0x2726, 164092},
	{0x2727, 167190},
	{0x2728, 169669},
	{0x2729, 172082},
	{0x272a, 174600},
	{0x272b, 176730},
	{0x272c, 178896},
	{0x272d, 180853},
	{0x272e, 183078},
	{0x272f, 185022},
	{0x2730, 187077},
	{0x2731, 188947},
	{0x2732, 190853},
	{0x2733, 192650},
	{0x2734, 194478},
	{0x2735, 196408},
	{0x2736, 198036},
	{0x2737, 199768},
	{0x2738, 201590},
	{0x2739, 203198},
	{0x273a, 204894},
	{0x273b, 206448},
	{0x273c, 207972},
	{0x273d, 209590},
	{0x273e, 211123},
	{0x273f, 212742},
	{0x2f20, 213842},
	{0x2f21, 216752},
	{0x2f22, 219262},
	{0x2f23, 222112},
	{0x2f24, 224803},
	{0x2f25, 227188},
	{0x2f26, 229627},
	{0x2f27, 232725},
	{0x2f28, 235204},
	{0x2f29, 237617},
	{0x2f2a, 240135},
	{0x2f2b, 242265},
	{0x2f2c, 244431},
	{0x2f2d, 246388},
	{0x2f2e, 248613},
	{0x2f2f, 250557},
	{0x2f30, 252612},
	{0x2f31, 254482},
	{0x2f32, 256388},
	{0x2f33, 258185},
	{0x2f34, 260013},
	{0x2f35, 261943},
	{0x2f36, 263571},
	{0x2f37, 265303},
	{0x2f38, 267125},
	{0x2f39, 268733},
	{0x2f3a, 270429},
	{0x2f3b, 271983},
	{0x2f3c, 273507},
	{0x2f3d, 275125},
	{0x2f3e, 276658},
	{0x2f3f, 278277},
	{0x3f20, 279377},
	{0x3f21, 282287},
	{0x3f22, 284797},
	{0x3f23, 287647},
	{0x3f24, 290338},
	{0x3f25, 292723},
	{0x3f26, 295162},
	{0x3f27, 298260},
	{0x3f28, 300739},
	{0x3f29, 303152},
	{0x3f2a, 305670},
	{0x3f2b, 307800},
	{0x3f2c, 309966},
	{0x3f2d, 311923},
	{0x3f2e, 314148},
	{0x3f2f, 316092},
	{0x3f30, 318147},
	{0x3f31, 320017},
	{0x3f32, 321923},
	{0x3f33, 323720},
	{0x3f34, 325548},
	{0x3f35, 327478},
	{0x3f36, 329106},
	{0x3f37, 330838},
	{0x3f38, 332660},
	{0x3f39, 334268},
	{0x3f3a, 335964},
	{0x3f3b, 337518},
	{0x3f3c, 339042},
	{0x3f3d, 340660},
	{0x3f3e, 342193},
	{0x3f3f, 343812},
	{0x7f20, 344912},
	{0x7f21, 347822},
	{0x7f22, 350332},
	{0x7f23, 353182},
	{0x7f24, 355873},
	{0x7f25, 358258},
	{0x7f26, 360697},
	{0x7f27, 363795},
	{0x7f28, 366274},
	{0x7f29, 368687},
	{0x7f2a, 371205},
	{0x7f2b, 373335},
	{0x7f2c, 375501},
	{0x7f2d, 377458},
	{0x7f2e, 379683},
	{0x7f2f, 381627},
	{0x7f30, 383682},
	{0x7f31, 385552},
	{0x7f32, 387458},
	{0x7f33, 389255},
	{0x7f34, 391083},
	{0x7f35, 393013},
	{0x7f36, 394641},
	{0x7f37, 396373},
	{0x7f38, 398195},
	{0x7f39, 399803},
	{0x7f3a, 401499},
	{0x7f3b, 403053},
	{0x7f3c, 404577},
	{0x7f3d, 406195},
	{0x7f3e, 407728},
	{0x7f3f, 409347}
};

struct tx_isp_sensor_attribute sensor_attr;

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
	return isp_gain;
}

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = 0x2300,
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
	.max_again = 409347,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 2250 - 4,
	.integration_time_limit = 2250 - 4,
	.total_width = 2560,
	.total_height = 2250,
	.max_integration_time = 2250 - 4,
	.one_line_expr_in_us = 36,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};


static struct regval_list sensor_init_regs_1920_1080_25fps[] = {

	{0x0100, 0x00},
	{0x3e03, 0x03},
	{0x3620, 0x44},  //gain>2 0x46
	{0x3627, 0x04},
	{0x3621, 0x28},
	{0x3641, 0x03},
	{0x3d08, 0x01},
	{0x3640, 0x01},
	{0x3300, 0x20},
	{0x3e03, 0x0b},
	{0x3635, 0x88},
	{0x320c, 0x0a},
	{0x320d, 0x50}, //2640 hts for 25fps
	{0x320e, 0x04},
	{0x320f, 0x65},
	{0x3e0f, 0x05}, //11bit
	{0x3305, 0x00},
	{0x3306, 0xd0},
	{0x330a, 0x02},
	{0x330b, 0x38},
	{0x363a, 0x06}, //NVDD fullwell
	{0x3632, 0x42}, //TXVDD fpn
	{0x3622, 0x02}, //blksun
	{0x3630, 0x48},
	{0x3631, 0x80},
	{0x3334, 0xc0},
	{0x3e0e, 0x06}, //[1] 1:dcg gain in 3e08[5]
	{0x3637, 0x83},
	{0x3638, 0x83},
	{0x3620, 0x46},
	{0x3035, 0xca},
	{0x330b, 0x78},
	{0x3416, 0x44},
	{0x363a, 0x04},
	{0x3e09, 0x20}, // 1x gain
	{0x337f, 0x03}, //new auto precharge  330e in 3372
	{0x3368, 0x04},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x3367, 0x08},
	{0x330e, 0x80},
	{0x3620, 0x58},
	{0x3632, 0x41},
	{0x3639, 0x04},
	{0x363a, 0x08},
	{0x3333, 0x10},
	{0x3e01, 0x8c},
	{0x3e14, 0xb0},
	{0x3038, 0x41},
	{0x3309, 0x30},
	{0x331f, 0x27},
	{0x3321, 0x2a},
	{0x3620, 0x54},
	{0x3627, 0x03},
	{0x3632, 0x40},
	{0x363a, 0x07},
	{0x3635, 0x80},
	{0x3621, 0x28},
	{0x3626, 0x30},
	{0x3633, 0xf4},
	{0x3632, 0x00},
	{0x3630, 0x4f},
	{0x3637, 0x84},
	{0x3670, 0x20}, //bit[5] for auto 3635 in 0x36a5
	{0x3683, 0x88}, //3630 value <gain0
	{0x3684, 0x84}, //3630 value between gain0 and gain1
	{0x3685, 0x80}, //3630 value > gain1
	{0x369a, 0x07}, //gain0
	{0x369b, 0x0f}, //gain1
	{0x330e, 0x30}, //auto_precharge
	{0x3208, 0x07},
	{0x3209, 0x80},//1920
	{0x320a, 0x04},
	{0x320b, 0x38},//1080
	{0x320c, 0x0a},
	{0x320d, 0x00},
	{0x3039, 0x31},
	{0x303a, 0xa6},
	{0x3306, 0x90},
	{0x330b, 0x78},
	{0x0100, 0x01},

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
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_25fps,
	}
};

static enum v4l2_mbus_pixelcode sensor_mbus_code[] = {
	V4L2_MBUS_FMT_SBGGR10_1X10,
	V4L2_MBUS_FMT_SBGGR12_1X12,
};

/*
 * the part of driver was fixed.
 */

static struct regval_list sensor_stream_on[] = {
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	{0x0100, 0x00},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct v4l2_subdev *sd, uint16_t reg, unsigned char *value) {
	int ret;
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
	struct i2c_client *client = v4l2_get_subdevdata(sd);
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

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct v4l2_subdev *sd, uint16_t reg, unsigned char value) {
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 3,
		.buf = buf,
	};

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
	ret = sensor_read(sd, 0x3107, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;
	ret = sensor_read(sd, 0x3108, &v);
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
	ret += sensor_write(sd, 0x3e00, (unsigned char) ((value & 0xf000) >> 12));
	ret += sensor_write(sd, 0x3e01, (unsigned char) ((value >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char) ((value & 0x0f) << 4));
	if (ret < 0)
		return ret;
	return 0;
}

static int sensor_set_analog_gain(struct v4l2_subdev *sd, int value) {
	int ret = 0;
	ret += sensor_write(sd, 0x3e09, (unsigned char) (value & 0xff));
	ret += sensor_write(sd, 0x3e08, (unsigned char) ((value & 0xff00) >> 8));
	if (ret < 0)
		return ret;
#if 1
	/* denoise logic */
	if (value <= 0x420) {
		ret += sensor_write(sd, 0x3620, 0x58);
		ret += sensor_write(sd, 0x3622, 0x06);
		if (ret < 0)
			return ret;
	} else if (value > 0x420) {
		ret += sensor_write(sd, 0x3620, 0x58);
		ret += sensor_write(sd, 0x3622, 0x02);
		if (ret < 0)
			return ret;
	}
/*	else {
		ret += sensor_write(sd, 0x3630, 0x84);
		ret += sensor_write(sd, 0x3635, 0x52);
		ret += sensor_write(sd, 0x3620, 0x62);
		ret += sensor_write(sd, 0x3315, 0x00);
	    if (ret < 0)
			return ret;
	}
*/
#endif
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
	struct v4l2_subdev *sd = &sensor->sd;
	struct tx_isp_notify_argument arg;
	unsigned int pclk = SENSOR_SUPPORT_PCLK;
	unsigned short hts;
	unsigned short vts = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
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

	ret = sensor_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	ret += sensor_write(sd, 0x320e, (unsigned char) (vts >> 8));
	if (ret < 0) {
		printk("err: sensor_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts * 2 - 4;
	sensor->video.attr->integration_time_limit = vts * 2 - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts * 2 - 4;
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
			msleep(1);
		} else {
			printk("gpio requrest fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			gpio_direction_output(pwdn_gpio, 1);
			msleep(50);
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
	enum v4l2_mbus_pixelcode mbus;
	int i = 0;
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

	sensor_attr.dvp.gpio = sensor_gpio_func;

	switch (sensor_gpio_func) {
		case DVP_PA_LOW_10BIT:
		case DVP_PA_HIGH_10BIT:
			mbus = sensor_mbus_code[0];
			break;
		case DVP_PA_12BIT:
			mbus = sensor_mbus_code[1];
			break;
		default:
			goto err_set_sensor_gpio;
	}

	for (i = 0; i < ARRAY_SIZE(sensor_win_sizes); i++)
		sensor_win_sizes[i].mbus_code = mbus;

	/*
	   convert sensor-gain into isp-gain,
	*/
	sensor_attr.max_again = 409347;
	sensor_attr.max_dgain = 0; //sensor_attr.max_dgain;
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
