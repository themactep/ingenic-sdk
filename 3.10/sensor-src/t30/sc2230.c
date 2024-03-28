// SPDX-License-Identifier: GPL-2.0+
/*
 * sc2230.c
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

#define SENSOR_NAME "sc2230"
#define SENSOR_CHIP_ID_H (0x22)
#define SENSOR_CHIP_ID_M (0x32)
#define SENSOR_CHIP_ID_L (0x20)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK (78000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20190110a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_LOW_10BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

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
	{0x729, 89537},
	{0x72a, 91245},
	{0x72b, 93487},
	{0x72c, 95643},
	{0x72d, 97684},
	{0x72e, 99846},
	{0x72f, 101960},
	{0x730, 103870},
	{0x731, 105742},
	{0x732, 107730},
	{0x733, 109676},
	{0x734, 111438},
	{0x735, 113167},
	{0x736, 115006},
	{0x737, 116810},
	{0x738, 118445},
	{0x739, 120052},
	{0x73a, 121762},
	{0x73b, 123443},
	{0x73c, 124968},
	{0x73d, 126468},
	{0x73e, 128068},
	{0x73f, 129641},
	{0xf20, 131070},
	{0xf21, 134093},
	{0xf22, 136801},
	{0xf23, 139650},
	{0xf24, 142206},
	{0xf25, 144898},
	{0xf26, 147317},
	{0xf27, 149870},
	{0xf28, 152167},
	{0xf29, 154594},
	{0xf2a, 156780},
	{0xf2b, 159093},
	{0xf2c, 161178},
	{0xf2d, 163387},
	{0xf2e, 165381},
	{0xf2f, 167495},
	{0xf30, 169405},
	{0xf31, 171432},
	{0xf32, 173265},
	{0xf33, 175211},
	{0xf34, 176973},
	{0xf35, 178845},
	{0xf36, 180541},
	{0xf37, 182345},
	{0xf38, 183980},
	{0xf39, 185719},
	{0xf3a, 187297},
	{0xf3b, 188978},
	{0xf3c, 190503},
	{0xf3d, 192127},
	{0xf3e, 193603},
	{0xf3f, 195176},
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

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x2230,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
	},
	.max_again = 260651,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 1246,
	.integration_time_limit = 1246,
	.total_width = 2496,
	.total_height = 1250,
	.max_integration_time = 1246,
	.one_line_expr_in_us = 32,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	// void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list sensor_init_regs_1920_1080_25fps_dvp[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x3641, 0x04},
	{0x3641, 0x00},
	{0x3039, 0x52},
	{0x303b, 0x14},
	{0x303d, 0x10},
	{0x3221, 0x80},
	{0x391e, 0x00},
	{0x5780, 0x7f},
	{0x5781, 0x04},
	{0x5782, 0x04},
	{0x5783, 0x02},
	{0x5784, 0x01},
	{0x57a0, 0x00},
	{0x57a1, 0x71},
	{0x57a2, 0x01},
	{0x57a3, 0xf1},
	{0x57a4, 0x60},
	{0x3e01, 0x46},
	{0x3e02, 0x50},
	{0x3641, 0x01},
	{0x3002, 0x80},
	{0x3622, 0x06},
	{0x3635, 0x1c},
	{0x363b, 0x16},
	{0x3306, 0x50},
	{0x330a, 0x00},
	{0x330b, 0xd0},
	{0x3309, 0x50},
	{0x331f, 0x49},
	{0x3f00, 0x0f},
	{0x3f04, 0x04},
	{0x3f05, 0x28},
	{0x3038, 0x66},
	{0x3637, 0x16},
	{0x3631, 0x88},
	{0x3630, 0x40},
	{0x3633, 0x24},
	{0x3638, 0x24},
	{0x330d, 0x36},
	{0x33b3, 0x32},
	{0x33b4, 0x32},
	{0x3338, 0x37},
	{0x333a, 0x37},
	{0x33ae, 0x32},
	{0x3339, 0x37},
	{0x333a, 0x33},
	{0x3301, 0x06},
	{0x363a, 0x80},
	{0x3314, 0x14},
	{0x330f, 0x01},
	{0x3310, 0x23},
	{0x330e, 0x18},
	{0x3367, 0x08},
	{0x335d, 0x20},
	{0x3e09, 0x20},
	{0x3637, 0x2c},
	{0x3614, 0x80},
	{0x3632, 0x88},
	{0x3303, 0x38},
	{0x331e, 0x31},
	{0x3309, 0x70},
	{0x331f, 0x69},
	{0x3933, 0x0a},
	{0x3934, 0x10},
	{0x3940, 0x60},
	{0x3942, 0x02},
	{0x3943, 0x18},
	{0x3038, 0x6e},
	{0x3614, 0x80},
	{0x3364, 0x1d},
	{0x33b6, 0x0f},
	{0x33b7, 0x3e},
	{0x33b8, 0x08},
	{0x33b9, 0x80},
	{0x33ba, 0xc0},
	{0x360f, 0x05},
	{0x367a, 0x38},
	{0x367b, 0x38},
	{0x3671, 0xf6},
	{0x3672, 0x16},
	{0x3673, 0x16},
	{0x366e, 0x04},
	{0x3670, 0x48},
	{0x369c, 0x38},
	{0x369d, 0x38},
	{0x3690, 0x24},
	{0x3691, 0x44},
	{0x3692, 0x44},
	{0x36a2, 0x00},
	{0x36a3, 0x3f},
	{0x3699, 0x80},
	{0x369a, 0x80},
	{0x369b, 0x9f},
	{0x3039, 0x52},
	{0x303a, 0x33},
	{0x3034, 0x01},
	{0x3035, 0x9b},
	{0x320c, 0x09},
	{0x320d, 0xc0}, /*hts = 2496*/
	{0x320e, 0x04}, /*vts = 1250*/
	{0x320f, 0xe2},
	{0x3f04, 0x04},
	{0x3f05, 0xcc},
	{0x3235, 0x09},
	{0x3236, 0xc2},
	{0x3209, 0x80},
	{0x320b, 0x38},
	{0x3e00, 0x00},
	{0x3e01, 0x9b},
	{0x3e02, 0xc0},
	{0x3e03, 0x0b},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x20},
	{0x3633, 0x24},
	{0x3622, 0xf6},
	{0x3301, 0x08},
	{0x363a, 0x80},
	{0x3630, 0x40},
	{0x3632, 0x88},

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

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,	unsigned char *value)
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

	ret = sensor_read(sd, 0x3109, &v);
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

	ret = sensor_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
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
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = sensor_write_array(sd, sensor_stream_on_dvp);
		pr_debug("%s stream on\n", SENSOR_NAME);
	} else {
		ret = sensor_write_array(sd, sensor_stream_off_dvp);
		pr_debug("%s stream off\n", SENSOR_NAME);
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

	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		printk("warn: fps(%d) not in range\n", fps);
		return -1;
	}
	sclk = SENSOR_SUPPORT_SCLK;

	ret = sensor_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0x320d, &tmp);
	hts = ((hts << 8) + tmp);
	if (0 != ret) {
		printk("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = sensor_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		printk("err: sensor_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
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
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		} else {
			printk("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		} else {
			printk("gpio requrest fail %d\n",pwdn_gpio);
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
				ret = sensor_write_array(sd, sensor_stream_off_dvp);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
				ret = sensor_write_array(sd, sensor_stream_on_dvp);
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if (arg)
				ret = sensor_set_fps(sd, *(int*)arg);
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
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
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
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;
	sensor_attr.dvp.gpio = sensor_gpio_func;

	/* convert sensor-gain into isp-gain */
	sensor_attr.max_again = 256041;
	sensor_attr.max_dgain = 0; //sc1245a_attr.max_dgain;
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
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
