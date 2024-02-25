/*
 * os02b10.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG

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
#define OS02B10_CHIP_ID_H	(0x23)
#define OS02B10_CHIP_ID_L	(0x08)
#define OS02B10_REG_END		0xff
#define OS02B10_REG_DELAY	0x00
#define OS02B10_PAGE_REG		0xfd

#define OS02B10_SUPPORT_SCLK_FPS_30 (84000000)
#define OS02B10_SUPPORT_SCLK_FPS_15 (60000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20170911a"

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
	unsigned char reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	unsigned int value;
	unsigned int gain;
};
struct again_lut os02b10_again_lut[] = {
	{0x20, 65536},
	{0x22, 71267},
	{0x24, 76672},
	{0x26, 81784},
	{0x28, 86633},
	{0x2a, 91246},
	{0x2c, 95645},
	{0x2e, 99848},
	{0x30, 103872},
	{0x32, 107731},
	{0x34, 111440},
	{0x36, 115008},
	{0x38, 118446},
	{0x3a, 121764},
	{0x3c, 124969},
	{0x3e, 128070},
	{0x40, 131072},
	{0x44, 136803},
	{0x48, 142208},
	{0x4c, 147320},
	{0x50, 152169},
	{0x54, 156782},
	{0x58, 161181},
	{0x5c, 165384},
	{0x60, 169408},
	{0x64, 173267},
	{0x68, 176976},
	{0x6c, 180544},
	{0x70, 183982},
	{0x74, 187300},
	{0x78, 190505},
	{0x7c, 193606},
	{0x80, 196608},
	{0x88, 202339},
	{0x90, 207744},
	{0x98, 212856},
	{0xa0, 217705},
	{0xa8, 222318},
	{0xb0, 226717},
	{0xb8, 230920},
	{0xc0, 234944},
	{0xc8, 238803},
	{0xd0, 242512},
	{0xd8, 246080},
	{0xe0, 249518},
	{0xe8, 252836},
	{0xf0, 256041},
	{0xf8, 259142},
};

struct tx_isp_sensor_attribute os02b10_attr;

unsigned int os02b10_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = os02b10_again_lut;

	while(lut->gain <= os02b10_attr.max_again) {
		if(isp_gain <= 65535) {
			*sensor_again = lut->value;
			return lut->gain;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == os02b10_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int os02b10_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute os02b10_attr={
	.name = "os02b10",
	.chip_id = 0x2308,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x3c,
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
	.max_integration_time_native = 0x551-8,
	.integration_time_limit = 0x551-8,
	.total_width = 0x4d2,
	.total_height = 0x551,
	.max_integration_time = 0x551 - 8,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = os02b10_alloc_again,
	.sensor_ctrl.alloc_dgain = os02b10_alloc_dgain,
	.one_line_expr_in_us = 29,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list os02b10_init_regs_1920_1080_25fps_dvp[] = {
	{0xfd, 0x00},
	{0x1b, 0x00},
	{0x1d, 0xaa},
	{0x2f, 0x10},
	{0x30, 0x15},
	{0x33, 0x81},
	{0xfd, 0x01},
	{0x03, 0x01},
	{0x04, 0x54},
	{0x05, 0x00},
	{0x06, 0xf4},
	{0x09, 0x00},
	{0x0a, 0xab},
	{0x24, 0xf0},
	{0x39, 0x08},
	{0x01, 0x01},
	{0x11, 0x50},
	{0x16, 0xe8},
	{0x19, 0xa1},
	{0x1e, 0x09},
	{0x1f, 0xa4},
	{0x33, 0x60},
	{0x12, 0x04},
	{0x13, 0x60},//signal num:+10
	{0x1a, 0xfb},
	{0x20, 0x03},
	{0x25, 0xbe},
	{0x26, 0x09},
	{0x29, 0x0b},
	{0x2a, 0x6d},//adc_range:0.88v
	{0x2c, 0x60},
	{0x2e, 0x02},
	{0x44, 0x03},
	{0x45, 0xfe},//cnt_dds:+10
	{0x55, 0x0d},//rst:+10
	{0x57, 0x20},//tx on >1us
	{0x59, 0xef},
	{0x5a, 0xff},
	{0x5b, 0x01},
	{0x5c, 0xf0},
	{0x66, 0x34},
	{0x68, 0x36},
	{0x71, 0x4a},
	{0x73, 0x35},//tx on d3
	{0x7c, 0xbb},
	{0x8a, 0x55},
	{0x8b, 0x55},
	{0xc0, 0xe0},
	{0xc1, 0xe0},
	{0xc2, 0xe0},
	{0xc3, 0xe0},
	{0xc4, 0x36},
	{0xc5, 0x36},
	{0xc6, 0x36},
	{0xc7, 0x36},
	{0xfb, 0x53},
	{0xf0, 0x00},
	{0xf1, 0x00},
	{0xf2, 0x00},
	{0xf3, 0x00},
	{0xf5, 0x80},
	{0xfd, 0x02},
	{0x34, 0xff},
	{0x60, 0xff},
	{0xfd, 0x02},
	{0x14, 0x03},
	{0x15, 0xfc},
	{0x16, 0x03},
	{0x17, 0xfc},
	{0x18, 0xf0},
	{0x19, 0xf0},
	{0xa0, 0x00},
	{0xa1, 0x00},
	{0xa2, 0x04},
	{0xa3, 0x38},
	{0xa4, 0x00},
	{0xa5, 0x04},
	{0xa6, 0x03},
	{0xa7, 0xc0},
	{0xfd, 0x02},
	{0xc0, 0x40},
	{0xc1, 0x40},
	{0xc2, 0x42},
	{0xc3, 0x42},
	{0xc5, 0x40},
	{0xc6, 0x40},
	{0xc7, 0x42},
	{0xc8, 0x42},
	{0xca, 0x40},
	{0xcb, 0x40},
	{0xcc, 0x42},
	{0xcd, 0x42},
	{0xcf, 0x40},
	{0xd0, 0x40},
	{0xd1, 0x42},
	{0xd2, 0x42},
	{0xfd, 0x01},
	{0xfd, 0x00},
	{0x1b, 0x00},
	{OS02B10_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the os02b10_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting os02b10_win_sizes[] = {
	/* 1920*1080 */
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= os02b10_init_regs_1920_1080_25fps_dvp,
	}
};

static enum v4l2_mbus_pixelcode os02b10_mbus_code[] = {
	V4L2_MBUS_FMT_SBGGR10_1X10,
};

/*
 * the part of driver was fixed.
 */

static struct regval_list os02b10_stream_on[] = {

	{OS02B10_REG_END, 0x00},
};

static struct regval_list os02b10_stream_off[] = {

	{OS02B10_REG_END, 0x00},
};

int os02b10_read(struct tx_isp_subdev *sd, unsigned char reg,
		 unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= &reg,
		},
		[1] = {
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= value,
		}
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int os02b10_write(struct tx_isp_subdev *sd, unsigned char reg,
			 unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 2,
		.buf	= buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int os02b10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != OS02B10_REG_END) {
		if (vals->reg_num == OS02B10_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = os02b10_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
			if (vals->reg_num == OS02B10_PAGE_REG){
				val &= 0xf8;
				val |= (vals->value & 0x07);
				ret = os02b10_write(sd, vals->reg_num, val);
				ret = os02b10_read(sd, vals->reg_num, &val);
			}
		}
		vals++;
	}
	return 0;
}
static int os02b10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != OS02B10_REG_END) {
		if (vals->reg_num == OS02B10_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = os02b10_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int os02b10_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int os02b10_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = os02b10_read(sd, 0x02, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS02B10_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = os02b10_read(sd, 0x03, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS02B10_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int os02b10_set_integration_time(struct tx_isp_subdev *sd, int value)
{

	int ret = 0;
	ret = os02b10_write(sd, 0xfd, 0x01);
	ret += os02b10_write(sd, 0x4, (unsigned char)(value & 0xff));
	ret += os02b10_write(sd, 0x3, (unsigned char)((value & 0xff00) >> 8));
	ret += os02b10_write(sd, 0x01, 0x01);
	if (ret < 0) {
		printk("os02b10_write error  %d\n" ,__LINE__);
		return ret;
	}

	return 0;
}

static int os02b10_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret = os02b10_write(sd, 0xfd, 0x01);
	ret += os02b10_write(sd, 0x24, (unsigned char)value);
	ret += os02b10_write(sd, 0x01, 0x01);
	if (ret < 0){
		printk("os02b10_write error  %d\n" ,__LINE__ );
		return ret;
	}
	return 0;
}

static int os02b10_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int os02b10_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int os02b10_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = &os02b10_win_sizes[0];
	int ret = 0;
	if(!enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = os02b10_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;
	return 0;
}

static int os02b10_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = os02b10_write_array(sd, os02b10_stream_on);
		pr_debug("os02b10 stream on\n");
	}
	else {
		ret = os02b10_write_array(sd, os02b10_stream_off);
		pr_debug("os02b10 stream off\n");
	}
	return ret;
}

static int os02b10_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0; //the format is 24.8
	int ret = 0;

	sclk = OS02B10_SUPPORT_SCLK_FPS_30;
	max_fps = SENSOR_OUTPUT_MAX_FPS;
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		printk("warn: fps(%d) no in range\n", fps);
		return -1;
	}

	ret = os02b10_write(sd, 0xfd, 0x01);
	if(ret < 0)
		return ret;
	ret = os02b10_read(sd, 0x8c, &tmp);
	hts = tmp;
	ret += os02b10_read(sd, 0x8d, &tmp);
	if (0 != ret) {
		printk("err: os02b10_write err\n");
		return ret;
	}
	hts = (hts << 8) + tmp;
	vts = sclk * (fps & 0xffff) / 2 / hts / ((fps & 0xffff0000) >> 16);
	ret = 0;
	ret += os02b10_write(sd, 0xfd, 0x01);
	ret += os02b10_write(sd, 0x0d, 0x10);//frame_exp_seperate_en
	ret += os02b10_write(sd, 0x0e, (vts >> 8) & 0xff);
	ret += os02b10_write(sd, 0x0f, vts & 0xff);
	ret += os02b10_write(sd, 0x01, 0x01);
	if (0 != ret) {
		printk("err: os02b10_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 8;
	sensor->video.attr->integration_time_limit = vts - 8;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 8;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int os02b10_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;

	if(value == TX_ISP_SENSOR_FULL_RES_MAX_FPS){
		wsize = &os02b10_win_sizes[0];
	}else if(value == TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS){
		wsize = &os02b10_win_sizes[0];
	}

	if(wsize){
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

static int os02b10_g_chip_ident(struct tx_isp_subdev *sd,
				struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"os02b10_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		}else{
			printk("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"os02b10_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(15);
		}else{
			printk("gpio requrest fail %d\n",reset_gpio);
		}
	}

	ret = os02b10_detect(sd, &ident);
	if (ret) {
		printk("chip found @ 0x%x (%s) is not an os02b10 chip.\n",
		       client->addr, client->adapter->name);
		return ret;
	}
	printk("ov2735 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "os02b10", sizeof("os02b10"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}
static int os02b10_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		printk("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = os02b10_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = os02b10_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = os02b10_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = os02b10_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = os02b10_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = os02b10_write_array(sd, os02b10_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = os02b10_write_array(sd, os02b10_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = os02b10_set_fps(sd, *(int*)arg);
		break;
	default:
		break;;
	}
	return 0;
}

static int os02b10_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = os02b10_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int os02b10_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	os02b10_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}

static struct tx_isp_subdev_core_ops os02b10_core_ops = {
	.g_chip_ident = os02b10_g_chip_ident,
	.reset = os02b10_reset,
	.init = os02b10_init,
	.g_register = os02b10_g_register,
	.s_register = os02b10_s_register,
};

static struct tx_isp_subdev_video_ops os02b10_video_ops = {
	.s_stream = os02b10_s_stream,
};

static struct tx_isp_subdev_sensor_ops	os02b10_sensor_ops = {
	.ioctl	= os02b10_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops os02b10_ops = {
	.core = &os02b10_core_ops,
	.video = &os02b10_video_ops,
	.sensor = &os02b10_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "os02b10",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};


static int os02b10_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &os02b10_win_sizes[0];
	int ret;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
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

	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;

	os02b10_attr.dvp.gpio = sensor_gpio_func;

	os02b10_attr.max_again = 259142;
	os02b10_attr.max_dgain = 0;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &os02b10_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &os02b10_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->os02b10\n");
	return 0;
err_set_sensor_gpio:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int os02b10_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);
	return 0;
}

static const struct i2c_device_id os02b10_id[] = {
	{ "os02b10", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, os02b10_id);

static struct i2c_driver os02b10_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "os02b10",
	},
	.probe		= os02b10_probe,
	.remove		= os02b10_remove,
	.id_table	= os02b10_id,
};

static __init int init_os02b10(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		printk("Failed to init os02b10 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&os02b10_driver);
}

static __exit void exit_os02b10(void)
{
	private_i2c_del_driver(&os02b10_driver);
}

module_init(init_os02b10);
module_exit(exit_os02b10);

MODULE_DESCRIPTION("A low-level driver for OmniVision os02b10 sensors");
MODULE_LICENSE("GPL");
