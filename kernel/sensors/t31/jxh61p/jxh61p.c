/*
 * jxh61p.c
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
#include <txx-funcs.h>

#define JXH61P_CHIP_ID_H	(0x08)
#define JXH61P_CHIP_ID_L	(0x49)
#define JXH61P_REG_END		0xff
#define JXH61P_REG_DELAY		0xfe
#define JXH61P_SUPPORT_PCLK (43200*1000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_1
#define SENSOR_VERSION	"H20220520a"

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
MODULE_PARM_DESC(data_interface, "Sensor Data interface");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_30;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};
/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut jxh61p_again_lut[] = {
	{0x0, 0},
	{0x1, 5731},
	{0x2, 11136},
	{0x3, 16248},
	{0x4, 21097},
	{0x5, 25710},
	{0x6, 30109},
	{0x7, 34312},
	{0x8, 38336},
	{0x9, 42195},
	{0xa, 45904},
	{0xb, 49472},
	{0xc, 52910},
	{0xd, 56228},
	{0xe, 59433},
	{0xf, 62534},
	{0x10, 65536},
	{0x11, 71267},
	{0x12, 76672},
	{0x13, 81784},
	{0x14, 86633},
	{0x15, 91246},
	{0x16, 95645},
	{0x17, 99848},
	{0x18, 103872},
	{0x19, 107731},
	{0x1a, 111440},
	{0x1b, 115008},
	{0x1c, 118446},
	{0x1d, 121764},
	{0x1e, 124969},
	{0x1f, 128070},
	{0x20, 131072},
	{0x21, 136803},
	{0x22, 142208},
	{0x23, 147320},
	{0x24, 152169},
	{0x25, 156782},
	{0x26, 161181},
	{0x27, 165384},
	{0x28, 169408},
	{0x29, 173267},
	{0x2a, 176976},
	{0x2b, 180544},
	{0x2c, 183982},
	{0x2d, 187300},
	{0x2e, 190505},
	{0x2f, 193606},
	{0x30, 196608},
	{0x31, 202339},
	{0x32, 207744},
	{0x33, 212856},
	{0x34, 217705},
	{0x35, 222318},
	{0x36, 226717},
	{0x37, 230920},
	{0x38, 234944},
	{0x39, 238803},
	{0x3a, 242512},
	{0x3b, 246080},
	{0x3c, 249518},
	{0x3d, 252836},
	{0x3e, 256041},
	{0x3f, 259142}
};
struct tx_isp_sensor_attribute jxh61p_attr;

unsigned int jxh61p_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = jxh61p_again_lut;
	while(lut->gain <= jxh61p_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == jxh61p_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}
		lut++;
	}
	return isp_gain;
}

unsigned int jxh61p_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return isp_gain;
}

struct tx_isp_mipi_bus jxh61p_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 440,
	.lans = 1,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 1280,
	.image_theight = 720,
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
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute jxh61p_attr={
	.name = "jxh61p",
	.chip_id = 0x0849,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x40,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
	},
	.expo_fs = 1,
	.max_again = 259142,
	.max_dgain = 0,
	.total_width = 1920,
	.total_height = 900,
	.min_integration_time = 2,
	.max_integration_time = 900 - 4,
	.min_integration_time_native = 2,
	.max_integration_time_native = 900 - 4,
	.integration_time_limit = 900 - 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = jxh61p_alloc_again,
	.sensor_ctrl.alloc_dgain = jxh61p_alloc_dgain,
	.one_line_expr_in_us = 44,
};

static struct regval_list jxh61p_init_regs_1280_720_30fps_mipi[] = {
	{0x12, 0x40},
	{0x48, 0x85},
	{0x48, 0x05},
	{0x0E, 0x11},
	{0x0F, 0x84},
	{0x10, 0x24},
	{0x11, 0x80},
	{0x57, 0x60},
	{0x58, 0x18},
	{0x61, 0x10},
	{0x46, 0x08},
	{0x0D, 0xA0},
	{0x20, 0xC0},
	{0x21, 0x03},
	{0x22, 0x84},
	{0x23, 0x03},/*vts for 25fps*/
	{0x24, 0x80},
	{0x25, 0xD0},
	{0x26, 0x22},
	{0x27, 0xD4},
	{0x28, 0x15},
	{0x29, 0x02},
	{0x2A, 0xC9},
	{0x2B, 0x12},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0xBA},
	{0x2F, 0x40},
	{0x41, 0x84},
	{0x42, 0x12},
	{0x47, 0x46},
	{0x76, 0x40},
	{0x77, 0x06},
	{0x80, 0x01},
	{0xAF, 0x22},
	{0xAB, 0x00},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x9E, 0xF8},
	{0x6E, 0x2C},
	{0x70, 0xB4},
	{0x71, 0x8E},
	{0x72, 0x89},
	{0x73, 0x56},
	{0x74, 0x02},
	{0x78, 0x10},
	{0x89, 0x01},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x0C, 0x30},
	{0x30, 0x86},
	{0x31, 0x08},
	{0x32, 0x18},
	{0x33, 0x52},
	{0x34, 0x1C},
	{0x35, 0x1C},
	{0x3A, 0xAF},
	{0x56, 0x12},
	{0x59, 0x20},
	{0x61, 0x18},
	{0x64, 0xD2},
	{0x85, 0x13},
	{0x8A, 0x00},
	{0x8D, 0x47},
	{0x94, 0xE0},
	{0xA4, 0x80},
	{0xA6, 0x00},
	{0xA7, 0x80},
	{0xA9, 0x48},
	{0x45, 0x01},
	{0x5B, 0xA4},
	{0x5C, 0x8C},
	{0x5D, 0x66},
	{0x5E, 0x03},
	{0x65, 0x12},
	{0x66, 0x00},
	{0x67, 0x44},
	{0x68, 0x10},
	{0x69, 0x74},
	{0x6A, 0x28},
	{0x7A, 0x02},
	{0x82, 0x10},
	{0x8F, 0x90},
	{0xA4, 0x87},
	{0xA5, 0x43},
	{0xB6, 0x86},
	{0x97, 0x00},
	{0x13, 0x81},
	{0x96, 0x84},
	{0x4A, 0x01},
	{0x7E, 0x4C},
	{0xB5, 0x0C},
	{0xB1, 0x00},
	{0xA1, 0x0F},
	{0xA3, 0x40},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x7F, 0x56},
	{0x8C, 0xFF},
	{0x8E, 0x40},
	{0x90, 0x01},
	{0xBC, 0x11},
	{0x19, 0x20},
	{0x1B, 0x4F},
	{0x12, 0x00},

	{JXH61P_REG_DELAY,0x10},
	{JXH61P_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxh61p_init_regs_1280_720_30fps_dvp[] = {
	{JXH61P_REG_END, 0x00},
};

/*
 * the order of the jxh61p_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting jxh61p_win_sizes[] = {
	/* dvp */
	{
		.width		= 1280,
		.height		= 720,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= jxh61p_init_regs_1280_720_30fps_dvp,
	},
	/* mipi */
	{
		.width		= 1280,
		.height		= 720,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= jxh61p_init_regs_1280_720_30fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &jxh61p_win_sizes[1];

static enum v4l2_mbus_pixelcode jxh61p_mbus_code[] = {
	V4L2_MBUS_FMT_SGBRG8_1X8,
	V4L2_MBUS_FMT_SBGGR10_1X10,
};

static struct regval_list jxh61p_stream_on[] = {
	{0x12, 0x00},
	{JXH61P_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxh61p_stream_off[] = {
	/* Sensor enter LP11*/
	{0x12, 0x40},
	{JXH61P_REG_END, 0x00},	/* END MARKER */
};

int jxh61p_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int jxh61p_write(struct tx_isp_subdev *sd, unsigned char reg,
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

static int jxh61p_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != JXH61P_REG_END) {
		if (vals->reg_num == JXH61P_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = jxh61p_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
static int jxh61p_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != JXH61P_REG_END) {
		if (vals->reg_num == JXH61P_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = jxh61p_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int jxh61p_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int jxh61p_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = jxh61p_read(sd, 0x0a, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != JXH61P_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = jxh61p_read(sd, 0x0b, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != JXH61P_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int jxh61p_set_expo(struct tx_isp_subdev* sd, int value)
{
	int ret = 0;
	int expo = (value & 0x0000ffff);
	int again = (value & 0xffff0000) >> 16;

	/*set integration time*/
	ret = jxh61p_write(sd, 0x01, (unsigned char)(expo & 0xff));
	ret += jxh61p_write(sd, 0x02, (unsigned char)((expo >> 8) & 0xff));
	/*set analog gain*/
	ret += jxh61p_write(sd, 0x00, (unsigned char)(again & 0x7f));
	if (0 != ret)
		ISP_ERROR("%s:%d, write reg err!\n",__func__,__LINE__);

	return ret;
}

#if 0
static int jxh61p_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	unsigned int expo = value;
	int ret = 0;

	jxh61p_write(sd, 0x01, (unsigned char)(expo & 0xff));
	jxh61p_write(sd, 0x02, (unsigned char)((expo >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}
static int jxh61p_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret += jxh61p_write(sd, 0x00, (unsigned char)(value & 0x7f));
	if (ret < 0) {
		ISP_ERROR("err:jxh61p_write analog gain error\n");
		return ret;
	}
	return 0;
}
#endif

static int jxh61p_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxh61p_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxh61p_init(struct tx_isp_subdev *sd, int enable)
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
	ret = jxh61p_write_array(sd, wsize->regs);

	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;
	return 0;
}

static int jxh61p_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = jxh61p_write_array(sd, jxh61p_stream_on);
		ISP_WARNING("jxh61p stream on\n");
	} else {
		ret = jxh61p_write_array(sd, jxh61p_stream_off);
		ISP_WARNING("jxh61p stream off\n");
	}

	return ret;
}

static int jxh61p_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int pclk = JXH61P_SUPPORT_PCLK;
	unsigned short hts;
	unsigned short vts = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)){
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	ret += jxh61p_read(sd, 0x21, &tmp);
	hts = tmp;
	ret += jxh61p_read(sd, 0x20, &tmp);
	hts = ((hts << 8) + tmp) << 1;
	if(ret < 0)
		return ret;

	vts = pclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += jxh61p_write(sd, 0x22, (unsigned char)(vts & 0xff));
	ret += jxh61p_write(sd, 0x23, (unsigned char)(vts >> 8));
	if(ret < 0){
		ISP_ERROR("err:jxh61p_write err\n");
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

static int jxh61p_set_mode(struct tx_isp_subdev *sd, int value)
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

static int jxh61p_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val = 0;
	unsigned char tmp;

	ret += jxh61p_read(sd, 0x12, &val);
	if (enable){
		val = val | 0x10;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SGRBG10_1X10;
	} else {
		val = val & 0xef;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SBGGR10_1X10;
	}
	sensor->video.mbus_change = 1;
	ret += jxh61p_write(sd, 0xc4, 0x12);
	ret += jxh61p_write(sd, 0xc5, val);
	ret += jxh61p_read(sd, 0x1f, &tmp);
	if(ret < 0)
		return -1;
	tmp |= (1 << 7);
	ret += jxh61p_write(sd, 0x1f, tmp);
	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int jxh61p_g_chip_ident(struct tx_isp_subdev *sd,
			      struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"jxh61p_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(1);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio, "jxh61p_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(50);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n", pwdn_gpio);
		}
	}
	ret = jxh61p_detect(sd, &ident);
	if (ret) {
		ISP_WARNING("chip found @ 0x%x (%s) is not an jxh61p chip.\n",
		       client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("jxh61p chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "jxh61p", sizeof("jxh61p"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int jxh61p_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = jxh61p_set_expo(sd, *(int*)arg);
		break;
#if 0
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = jxh61p_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = jxh61p_set_analog_gain(sd, *(int*)arg);
		break;
#endif
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = jxh61p_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = jxh61p_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = jxh61p_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = jxh61p_write_array(sd, jxh61p_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = jxh61p_write_array(sd, jxh61p_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = jxh61p_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = jxh61p_set_vflip(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int jxh61p_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = jxh61p_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int jxh61p_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	jxh61p_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}

static struct tx_isp_subdev_core_ops jxh61p_core_ops = {
	.g_chip_ident = jxh61p_g_chip_ident,
	.reset = jxh61p_reset,
	.init = jxh61p_init,
	/*.ioctl = jxh61p_ops_ioctl,*/
	.g_register = jxh61p_g_register,
	.s_register = jxh61p_s_register,
};

static struct tx_isp_subdev_video_ops jxh61p_video_ops = {
	.s_stream = jxh61p_s_stream,
};

static struct tx_isp_subdev_sensor_ops	jxh61p_sensor_ops = {
	.ioctl	= jxh61p_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops jxh61p_ops = {
	.core = &jxh61p_core_ops,
	.video = &jxh61p_video_ops,
	.sensor = &jxh61p_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "jxh61p",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int jxh61p_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	enum v4l2_mbus_pixelcode mbus;
	int ret;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);


	if(data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
		wsize = &jxh61p_win_sizes[1];
		jxh61p_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
			memcpy((void*)(&(jxh61p_attr.mipi)),(void*)(&jxh61p_mipi),sizeof(jxh61p_mipi));
		jxh61p_attr.total_width = 1920;
		jxh61p_attr.total_height = 900;
		jxh61p_attr.min_integration_time = 2;
		jxh61p_attr.max_integration_time = 900 -4;
		jxh61p_attr.min_integration_time_native = 2;
		jxh61p_attr.max_integration_time_native = 900 - 4;
		jxh61p_attr.integration_time_limit = 900 - 4;
		ISP_WARNING("----->mipi\n");
	} else if(data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
		ret = set_sensor_gpio_function(sensor_gpio_func);
		if (ret < 0)
			goto err_set_sensor_gpio;

		jxh61p_attr.dvp.gpio = sensor_gpio_func;
		switch(sensor_gpio_func){
			case DVP_PA_LOW_8BIT:
			case DVP_PA_HIGH_8BIT:
				mbus = jxh61p_mbus_code[0];
				break;
			case DVP_PA_LOW_10BIT:
			case DVP_PA_HIGH_10BIT:
				mbus = jxh61p_mbus_code[1];
				break;
			default:
				goto err_set_sensor_gpio;
		}
		wsize = &jxh61p_win_sizes[0];
		ISP_WARNING("----->dvp\n");
	} else {
		ISP_ERROR("%s %d not supported data_interface!!\n",__func__,__LINE__);
	}
	/*
	   convert sensor-gain into isp-gain,
	   */
	jxh61p_attr.max_again = 259142;
	jxh61p_attr.max_dgain = jxh61p_attr.max_dgain;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &jxh61p_attr;
	sensor->video.mbus_change = 0;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &jxh61p_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	ISP_WARNING("@@@@@@@probe ok ------->jxh61p\n");

	return 0;

err_set_sensor_gpio:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int jxh61p_remove(struct i2c_client *client)
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

static const struct i2c_device_id jxh61p_id[] = {
	{ "jxh61p", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, jxh61p_id);

static struct i2c_driver jxh61p_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "jxh61p",
	},
	.probe		= jxh61p_probe,
	.remove		= jxh61p_remove,
	.id_table	= jxh61p_id,
};

static __init int init_jxh61p(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init jxh61p dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&jxh61p_driver);
}

static __exit void exit_jxh61p(void)
{
	private_i2c_del_driver(&jxh61p_driver);
}

module_init(init_jxh61p);
module_exit(exit_jxh61p);

MODULE_DESCRIPTION("A low-level driver for SOI jxh61p sensors");
MODULE_LICENSE("GPL");
