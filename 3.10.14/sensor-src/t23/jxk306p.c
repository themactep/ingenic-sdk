/*
 * jxk306p.c
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

#define JXK306P_CHIP_ID_H	(0x08)
#define JXK306P_CHIP_ID_L	(0x60)
#define JXK306P_REG_END		0xff
#define JXK306P_REG_DELAY	0xfe
#define JXK306P_SUPPORT_30FPS_SCLK (2560 * 1125 * 30)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20240222a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

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

struct again_lut jxk306p_again_lut[] = {
        {0x00, 0},
	{0x01, 5732},
	{0x02, 11136},
	{0x03, 16248},
	{0x04, 21098},
	{0x05, 25711},
	{0x06, 30109},
	{0x07, 34312},
	{0x08, 38336},
	{0x09, 42196},
	{0x0a, 45904},
	{0x0b, 49472},
	{0x0c, 52911},
	{0x0d, 56229},
	{0x0e, 59434},
	{0x0f, 62534},
	{0x10, 65536},
	{0x11, 71268},
	{0x12, 76672},
	{0x13, 81784},
	{0x14, 86634},
	{0x15, 91247},
	{0x16, 95645},
	{0x17, 99848},
	{0x18, 103872},
	{0x19, 107732},
	{0x1a, 111440},
	{0x1b, 115008},
	{0x1c, 118447},
	{0x1d, 121765},
	{0x1e, 124970},
	{0x1f, 128070},
	{0x20, 131072},
	{0x21, 136804},
	{0x22, 142208},
	{0x23, 147320},
	{0x24, 152170},
	{0x25, 156783},
	{0x26, 161181},
	{0x27, 165384},
	{0x28, 169408},
	{0x29, 173268},
	{0x2a, 176976},
	{0x2b, 180544},
	{0x2c, 183983},
	{0x2d, 187301},
	{0x2e, 190506},
	{0x2f, 193606},
	{0x30, 196608},
	{0x31, 202340},
	{0x32, 207744},
	{0x33, 212856},
	{0x34, 217706},
	{0x35, 222319},
	{0x36, 226717},
	{0x37, 230920},
	{0x38, 234944},
	{0x39, 238804},
	{0x3a, 242512},
	{0x3b, 246080},
	{0x3c, 249519},
	{0x3d, 252837},
	{0x3e, 256042},
	{0x3f, 259142},
	{0x40, 262144},
	{0x41, 267876},
	{0x42, 273280},
	{0x43, 278392},
	{0x44, 283242},
	{0x45, 287855},
	{0x46, 292253},
	{0x47, 296456},
	{0x48, 300480},
	{0x49, 304340},
	{0x4a, 308048},
	{0x4b, 311616},
	{0x4c, 315055},
	{0x4d, 318373},
	{0x4e, 321578},
	{0x4f, 324678},
};

struct tx_isp_sensor_attribute jxk306p_attr;

unsigned int jxk306p_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = jxk306p_again_lut;
	while(lut->gain <= jxk306p_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == jxk306p_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int jxk306p_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus jxk306p_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 216,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
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


struct tx_isp_sensor_attribute jxk306p_attr={
	.name = "jxk306p",
	.chip_id = 0x844,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x40,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 324678,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1125 - 4,
	.integration_time_limit = 1125 - 4,
	.total_width = 2560,
	.total_height = 1125,
	.max_integration_time = 1125 - 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = jxk306p_alloc_again,
	.sensor_ctrl.alloc_dgain = jxk306p_alloc_dgain,
	.one_line_expr_in_us = 28,
};

static struct regval_list jxk306p_init_regs_1920_1080_30fps_mipi[] = {
        {0x12,0x60},
        {0x48,0x8A},
        {0x48,0x0A},
        {0x0E,0x11},
        {0x0F,0x0C},
        {0x10,0x24},
        {0x0C,0x20},
        {0x0D,0xF0},
        {0x57,0x67},
        {0x58,0x1F},
        {0x5F,0x41},
        {0x60,0x20},
        {0x20,0x00},
        {0x21,0x05},
        {0x22,0x65},
        {0x23,0x04},
        {0x24,0xC0},
        {0x25,0x38},
        {0x26,0x43},
        {0x27,0x6C},
        {0x28,0x15},
        {0x29,0x04},
        {0x2A,0x60},
        {0x2B,0x14},
        {0x2C,0xA0},
        {0x2D,0x2D},
        {0x2E,0x42},
        {0x2F,0x04},
        {0x41,0xC4},
        {0x42,0x03},
        {0x47,0x46},
        {0x76,0x60},
        {0x77,0x09},
        {0x80,0x03},
        {0xAF,0x22},
        {0x46,0x08},
        {0xAA,0x80},
        {0x1D,0x00},
        {0x1E,0x04},
        {0x6C,0x40},
        {0x9E,0xB8},
        {0x6F,0x00},
        {0x6E,0x2C},
        {0x70,0x6D},
        {0x71,0x6D},
        {0x72,0x68},
        {0x73,0x46},
        {0x74,0x02},
        {0x78,0x1B},
        {0x89,0x01},
        {0x6B,0x20},
        {0x86,0x40},
        {0xB0,0x42},
        {0xBF,0x01},
        {0x0A,0xC3},
        {0xBF,0x00},
        {0x7F,0x46},
        {0x30,0x8D},
        {0x31,0x08},
        {0x32,0x20},
        {0x33,0x5C},
        {0x34,0x30},
        {0x35,0x30},
        {0x3A,0xB6},
        {0x56,0x92},
        {0x59,0x48},
        {0x5A,0x01},
        {0x61,0x00},
        {0x64,0xE0},
        {0x85,0x44},
        {0x8A,0x00},
        {0x91,0x40},
        {0x94,0xE0},
        {0x9B,0x8F},
        {0xA6,0x02},
        {0xA7,0xA0},
        {0xA9,0x4C},
        {0x45,0x09},
        {0x5B,0xA5},
        {0x5C,0x8C},
        {0x5D,0x67},
        {0x5E,0xCE},
        {0x65,0x3D},
        {0x66,0x80},
        {0x67,0x41},
        {0x68,0x00},
        {0x69,0x7C},
        {0x6A,0x2B},
        {0x7A,0xA4},
        {0x8D,0x6F},
        {0x8F,0x94},
        {0xA4,0xC7},
        {0xA5,0x0F},
        {0xB7,0x21},
        {0x97,0x20},
        {0x13,0x81},
        {0x96,0x84},
        {0x4A,0x01},
        {0xB1,0x00},
        {0xA1,0x0F},
        {0xB5,0xC4},
        {0xA3,0x40},
        {0xBF,0x01},
        {0x03,0x01},
        {0x04,0x80},
        {0x05,0x32},
        {0xBF,0x00},
        {0x50,0x02},
        {0x49,0x10},
        {0x7E,0x4C},
        {0x8C,0xFF},
        {0x8E,0x00},
        {0x8B,0x01},
        {0x9F,0x55},
        {0xBD,0x10},
        {0xA0,0x20},
        {0xBC,0x12},
        {0x82,0x00},
        {0x19,0x20},
        {0x1B,0x4F},
        {0x12,0x20},
        {0x48,0x8A},
        {0x48,0x0A},
        {JXK306P_REG_DELAY, 0x10},
        {JXK306P_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the jxk306p_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting jxk306p_win_sizes[] = {
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 30 << 16 | 2,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= jxk306p_init_regs_1920_1080_30fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &jxk306p_win_sizes[0];


/*
 * the part of driver was fixed.
 */
static struct regval_list jxk306p_stream_on_mipi[] = {

	{JXK306P_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxk306p_stream_off_mipi[] = {
	{JXK306P_REG_END, 0x00},	/* END MARKER */
};

int jxk306p_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int jxk306p_write(struct tx_isp_subdev *sd, unsigned char reg,
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

#if 0
static int jxk306p_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != JXK306P_REG_END) {
		if (vals->reg_num == JXK306P_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxk306p_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif
static int jxk306p_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != JXK306P_REG_END) {
		if (vals->reg_num == JXK306P_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxk306p_write(sd, vals->reg_num, vals->value);
                        //printk("write:{0x%4x,0x%2x}\n", vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int jxk306p_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int jxk306p_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = jxk306p_read(sd, 0x0a, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != JXK306P_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = jxk306p_read(sd, 0x0b, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;

	if (v != JXK306P_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int jxk306p_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	ret = jxk306p_write(sd,  0x01, (unsigned char)(it & 0xff));
	ret += jxk306p_write(sd, 0x02, (unsigned char)((it >> 8) & 0xff));

	ret = jxk306p_write(sd, 0x00, (unsigned char)(again & 0x7f));

        return 0;
}

#if 0
static int jxk306p_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;
	ret = jxk306p_write(sd,  0x01, (unsigned char)(expo & 0xff));
	ret += jxk306p_write(sd, 0x02, (unsigned char)((expo >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int jxk306p_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret = jxk306p_write(sd, 0x00, (unsigned char)(value & 0x7f));
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int jxk306p_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxk306p_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxk306p_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = jxk306p_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int jxk306p_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
                ret = jxk306p_write_array(sd, jxk306p_stream_on_mipi);
		ISP_WARNING("jxk306p stream on\n");

	} else {
                ret = jxk306p_write_array(sd, jxk306p_stream_off_mipi);
		ISP_WARNING("jxk306p stream off\n");
	}

	return ret;
}

static int jxk306p_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = JXK306P_SUPPORT_30FPS_SCLK;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 30;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}

	val = 0;
	ret += jxk306p_read(sd, 0x21, &val);
	hts = val;
	val = 0;
	ret += jxk306p_read(sd, 0x20, &val);
	hts = (((hts << 8) | val) << 1);
	if (0 != ret) {
		ISP_ERROR("err: jxk306p read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += jxk306p_write(sd, 0x22, (unsigned char)(vts & 0xff));
	ret += jxk306p_write(sd, 0x23, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: jxk306p_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;
}

static int jxk306p_set_vflip(struct tx_isp_subdev *sd, int enable)
{
        int ret = 0;
	unsigned char val;

	ret += jxk306p_read(sd, 0x12, &val);

	enable &= 0x03;
	switch(enable) {
	case 0:
		val = 0x00; /*normal*/
		break;
	case 1:
		val = 0x20; /*mirror*/
		break;
	case 2:
		val = 0x10; /*vflip*/
		break;
	case 3:
		val = 0x30; /*vflip & mirror*/
		break;
	default:
		break;
	}
	ret += jxk306p_write(sd, 0x12, val);

	return ret;
}

static int jxk306p_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

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

static int jxk306p_g_chip_ident(struct tx_isp_subdev *sd,
			      struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"jxk306p_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(50);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(35);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(35);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"jxk306p_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = jxk306p_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an jxk306p chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("jxk306p chip found @ 0x%02x (%s) version %s\n", client->addr, client->adapter->name, SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "jxk306p", sizeof("jxk306p"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}


static int jxk306p_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = jxk306p_set_expo(sd, *(int*)arg);
		break;
	/* case TX_ISP_EVENT_SENSOR_INT_TIME: */
	/* 	if(arg) */
	/* 		ret = jxk306p_set_integration_time(sd, *(int*)arg); */
	/* 	break; */
	/* case TX_ISP_EVENT_SENSOR_AGAIN: */
	/* 	if(arg) */
	/* 		ret = jxk306p_set_analog_gain(sd, *(int*)arg); */
	/* 	break; */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = jxk306p_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = jxk306p_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = jxk306p_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
                        ret = jxk306p_write_array(sd, jxk306p_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			ret = jxk306p_write_array(sd, jxk306p_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = jxk306p_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = jxk306p_set_vflip(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int jxk306p_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = jxk306p_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int jxk306p_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	jxk306p_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops jxk306p_core_ops = {
	.g_chip_ident = jxk306p_g_chip_ident,
	.reset = jxk306p_reset,
	.init = jxk306p_init,
	/*.ioctl = jxk306p_ops_ioctl,*/
	.g_register = jxk306p_g_register,
	.s_register = jxk306p_s_register,
};

static struct tx_isp_subdev_video_ops jxk306p_video_ops = {
	.s_stream = jxk306p_s_stream,
};

static struct tx_isp_subdev_sensor_ops	jxk306p_sensor_ops = {
	.ioctl	= jxk306p_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops jxk306p_ops = {
	.core = &jxk306p_core_ops,
	.video = &jxk306p_video_ops,
	.sensor = &jxk306p_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "jxk306p",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sensor_mclk_config(struct tx_isp_sensor *sensor, unsigned long want_rate)
{
        unsigned long rate = 0;
        struct clk *pll = NULL;
        char *plls[] = {"mpll", "sclka"};
        int psize = sizeof(plls) / sizeof(char *);
        char *ppll = plls[psize - 1];
        int ret = 0, i = 0;

        pll = clk_get_parent(sensor->mclk);
        rate = clk_get_rate(pll);
        if (rate % want_rate) {
                for (i = 0; i < psize; i++) {
                        pll = clk_get(NULL, plls[i]);
                        rate = clk_get_rate(pll);
                        if (!(rate % want_rate)) {
                                ret = clk_set_parent(sensor->mclk, pll);
                                if (ret) {
                                        ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
                                                    __func__, __LINE__, plls[i]);
                                        continue;
                                } else {
                                        break;
                                }
                        }
                }
                if (i == psize) {
                        if (!ret) {
                                pll = clk_get(NULL, ppll);
                                rate = clk_get_rate(pll);
                                if(want_rate == 37125000){
                                        if((rate >= 1188000000)) {
                                                rate = 1188000000;
                                        } else if (rate >= 891000000) {
                                                rate = 891000000;
                                        } else {
                                                ISP_ERROR("[%s %d] The %s clock setting failed !!!\n",
                                                          __func__, __LINE__, ppll);
                                                ret = -1;
                                                goto error;
                                        }
                                } else if (want_rate == 24000000 || want_rate == 27000000) {
                                        rate -= rate % want_rate;
                                } else {
                                        ret = -1;
                                        goto error;
                                }
                                ret = private_clk_set_rate(pll, rate);
                                if (ret) {
                                        ISP_WARNING("[%s %d] Failed to set %s !!!\n",
                                                    __func__, __LINE__, ppll);
                                        goto error;
                                } else {
                                        ISP_WARNING("[%s %d] !!!!!!!!!!! The %s frequency has been changed to %ld !!!\n",
                                                    __func__, __LINE__, ppll, rate);
                                }
                                ret = clk_set_parent(sensor->mclk, pll);
                                if (ret) {
                                        ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
                                                    __func__, __LINE__, ppll);
                                        goto error;
                                }
                        } else {
                                goto error;
                        }
                }
        }
        private_clk_set_rate(sensor->mclk, want_rate);
        private_clk_enable(sensor->mclk);

        rate = clk_get_rate(sensor->mclk);
        if (rate % want_rate) {
                ret = -1;
                goto error;
        }

        return ret;

error:
        ISP_ERROR("[%s %d] Unable to allocate the required MCLK %ld !!!\n",
                  __func__, __LINE__, want_rate);
        return ret;
}

static int jxk306p_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        struct tx_isp_subdev *sd;
        struct tx_isp_video_in *video;
        struct tx_isp_sensor *sensor;

        sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
        if(!sensor){
                ISP_ERROR("Failed to allocate sensor subdev.\n");
                return -ENOMEM;
        }
        memset(sensor, 0 ,sizeof(*sensor));

#ifdef CONFIG_KERNEL_4_4_94
		sensor->mclk = clk_get(NULL, "div_cim");
#else
		sensor->mclk = clk_get(NULL, "cgu_cim");
#endif
        if (IS_ERR(sensor->mclk)) {
                ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
                goto err_get_mclk;
        }

        sensor_mclk_config(sensor, 24000000);

	wsize = &jxk306p_win_sizes[0];
	memcpy((void*)(&(jxk306p_attr.mipi)),(void*)(&jxk306p_mipi),sizeof(jxk306p_mipi));

        jxk306p_attr.expo_fs = 1;
        sd = &sensor->sd;
        video = &sensor->video;
        sensor->video.shvflip = shvflip;
        sensor->video.attr = &jxk306p_attr;
        sensor->video.vi_max_width = wsize->width;
        sensor->video.vi_max_height = wsize->height;
        sensor->video.mbus.width = wsize->width;
        sensor->video.mbus.height = wsize->height;
        sensor->video.mbus.code = wsize->mbus_code;
        sensor->video.mbus.field = V4L2_FIELD_NONE;
        sensor->video.mbus.colorspace = wsize->colorspace;
        sensor->video.fps = wsize->fps;
        tx_isp_subdev_init(&sensor_platform_device, sd, &jxk306p_ops);
        tx_isp_set_subdevdata(sd, client);
        tx_isp_set_subdev_hostdata(sd, sensor);
        private_i2c_set_clientdata(client, sd);

        pr_debug("probe ok ------->jxk306p\n");
        return 0;

err_get_mclk:
        kfree(sensor);

        return -1;
}

static int jxk306p_remove(struct i2c_client *client)
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

static const struct i2c_device_id jxk306p_id[] = {
	{ "jxk306p", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, jxk306p_id);

static struct i2c_driver jxk306p_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "jxk306p",
	},
	.probe		= jxk306p_probe,
	.remove		= jxk306p_remove,
	.id_table	= jxk306p_id,
};

static __init int init_jxk306p(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init jxk306p dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&jxk306p_driver);
}

static __exit void exit_jxk306p(void)
{
	private_i2c_del_driver(&jxk306p_driver);
}

module_init(init_jxk306p);
module_exit(exit_jxk306p);

MODULE_DESCRIPTION("A low-level driver for galaxycore jxk306p sensors");
MODULE_LICENSE("GPL");
