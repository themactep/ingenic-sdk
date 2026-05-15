/*
 * sc4236.c
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
#include <tx-isp-debug.h>

#define SC4236_CHIP_ID_H	(0x32)
#define SC4236_CHIP_ID_L	(0x35)
#define SC4236_REG_END		0xffff
#define SC4236_REG_DELAY	0xfffe
#define SC4236_SUPPORT_PCLK_FPS_30 (120000*1000)
#define SC4236_SUPPORT_PCLK_FPS_15 (45000*1000)
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_1
#define SENSOR_VERSION	"H20180627a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");


static int sensor_max_fps = TX_SENSOR_MAX_FPS_25;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sc4236_again_lut[] = {
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
	{0x110,	65536},
	{0x111,	71267},
	{0x112,	76672},
	{0x113,	81784},
	{0x114,	86633},
	{0x115,	91246},
	{0x116,	95645},
	{0x117,	99848},
	{0x118, 103872},
	{0x119,	107731},
	{0x11a,	111440},
	{0x11b,	115008},
	{0x11c,	118446},
	{0x11d,	121764},
	{0x11e,	124969},
	{0x11f,	128070},
	{0x310, 131072},
	{0x311,	136803},
	{0x312,	142208},
	{0x313,	147320},
	{0x314,	152169},
	{0x315,	156782},
	{0x316,	161181},
	{0x317,	165384},
	{0x318,	169408},
	{0x319,	173267},
	{0x31a,	176976},
	{0x31b,	180544},
	{0x31c,	183982},
	{0x31d,	187300},
	{0x31e,	190505},
	{0x31f,	193606},
	{0x710, 196608},
	{0x711,	202339},
	{0x712,	207744},
	{0x713,	212856},
	{0x714,	217705},
	{0x715,	222318},
	{0x716,	226717},
	{0x717,	230920},
	{0x718,	234944},
	{0x719,	238803},
	{0x71a,	242512},
	{0x71b,	246080},
	{0x71c,	249518},
	{0x71d,	252836},
	{0x71e,	256041},
	/* {0x71f, 259142}, */
};

struct tx_isp_sensor_attribute sc4236_attr;

unsigned int sc4236_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc4236_again_lut;
	while(lut->gain <= sc4236_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc4236_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc4236_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sc4236_attr={
	.name = "sc4236",
	.chip_id = 0x3235,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 400,
		.lans = 2,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 2048,
		.image_theight = 1536,
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
	},
	.max_again = 256041,
	.max_dgain = 0,
	.min_integration_time = 3,
	.min_integration_time_native = 3,
	.max_integration_time_native = 1996,
	.integration_time_limit = 1996,
	.total_width = 2600,
	.total_height = 2000,
	.max_integration_time = 1996,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc4236_alloc_again,
	.sensor_ctrl.alloc_dgain = sc4236_alloc_dgain,
};



static struct regval_list sc4236_init_regs_2048_1536_30fps_mipi_3m[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x301c, 0x78},
	{0x3640, 0x02},
	{0x3641, 0x02},
	{0x3018, 0x33},
	{0x3031, 0x0a},
	{0x3037, 0x20},
	{0x3001, 0xfe},
	{0x4603, 0x00},
	{0x303f, 0x01},
	{0x337f, 0x03},
	{0x3368, 0x04},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x3367, 0x08},
	{0x3307, 0x18},
	{0x3632, 0x18},
	{0x3038, 0xcc},
	{0x363c, 0x05},
	{0x363d, 0x05},
	{0x3639, 0x09},
	{0x363a, 0x1f},
	{0x363b, 0x0c},
	{0x3908, 0x15},
	{0x3620, 0x28},
	{0x3631, 0x88},
	{0x3636, 0x24},
	{0x3638, 0x18},
	{0x3625, 0x03},
	{0x4837, 0x20},
	{0x3333, 0x20},
	{0x330a, 0x01},
	{0x366e, 0x08},
	{0x366f, 0x2f},
	{0x3306, 0x60},
	{0x330b, 0x10},
	{0x3f00, 0x0f},
	{0x3320, 0x06},
	{0x3326, 0x00},
	{0x331e, 0x21},
	{0x331f, 0x71},
	{0x3308, 0x18},
	{0x3303, 0x30},
	{0x3309, 0x80},
	{0x3933, 0x0a},
	{0x3942, 0x02},
	{0x3941, 0x14},
	{0x394c, 0x08},
	{0x394d, 0x14},
	{0x3954, 0x18},
	{0x3955, 0x18},
	{0x3958, 0x10},
	{0x3959, 0x20},
	{0x395a, 0x38},
	{0x395b, 0x38},
	{0x395c, 0x20},
	{0x395d, 0x10},
	{0x395e, 0x24},
	{0x395f, 0x00},
	{0x3960, 0xc4},
	{0x3961, 0xb1},
	{0x3637, 0x63},
	{0x3366, 0x78},
	{0x33aa, 0x00},
	{0x320c, 0x09},
	{0x320d, 0xc4},
	{0x3f04, 0x04},
	{0x3f05, 0xbe},
	{0x320e, 0x07},
	{0x320f, 0x80},
	{0x3235, 0x0c},
	{0x3236, 0x7e},
	{0x36e9, 0x02},
	{0x36ea, 0x36},
	{0x36eb, 0x06},
	{0x36ec, 0x0e},
	{0x36ed, 0x03},
	{0x36f9, 0x01},
	{0x36fa, 0x8a},
	{0x36fb, 0x00},
	{0x3222, 0x29},
	{0x3901, 0x02},
	{0x3635, 0xe2},
	{0x3963, 0x80},
	{0x3e1e, 0x34},
	{0x3211, 0x04},
	{0x3213, 0x04},
	{0x320a, 0x06},
	{0x320b, 0x00},
	{0x3202, 0x00},
	{0x3203, 0x00},
	{0x3206, 0x06},
	{0x3207, 0x0b},
	{0x330f, 0x04},
	{0x3310, 0x20},
	{0x3314, 0x04},
	{0x330e, 0x50},
	{0x4827, 0x46},
	{0x3650, 0x42},
	{0x5000, 0x06},
	{0x5780, 0x7f},
	{0x5781, 0x04},
	{0x5782, 0x03},
	{0x5783, 0x02},
	{0x5784, 0x01},
	{0x5785, 0x18},
	{0x5786, 0x10},
	{0x5787, 0x08},
	{0x5788, 0x02},
	{0x5789, 0x20},
	{0x578a, 0x30},
	{0x3962, 0x09},
	{0x3946, 0x48},
	{0x3947, 0x20},
	{0x3948, 0x0a},
	{0x3949, 0x10},
	{0x394a, 0x28},
	{0x394b, 0x48},
	{0x394e, 0x28},
	{0x3950, 0x20},
	{0x3951, 0x10},
	{0x3940, 0x15},
	{0x3934, 0x16},
	{0x3943, 0x20},
	{0x3952, 0x68},
	{0x3953, 0x38},
	{0x3956, 0x38},
	{0x3957, 0x78},
	{0x394f, 0x40},
	{0x3200, 0x00},
	{0x3201, 0x84},
	{0x3204, 0x08},
	{0x3205, 0x8b},
	{0x3208, 0x08},
	{0x3209, 0x00},
	{0x3802, 0x00},
	{0x3e00, 0x00},
	{0x3e01, 0xc7},
	{0x3e02, 0xc0},
	{0x3e03, 0x0b},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x10},
	{0x3301, 0x1e},
	{0x3633, 0x23},
	{0x3630, 0x80},
	{0x3622, 0xf6},

	{SC4236_REG_END, 0x00},/* END MARKER */
};
/*
 * the order of the sc4236_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc4236_win_sizes[] = {
	/* 1920*1080 */
	{
		.width		= 2048,
		.height		= 1536,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc4236_init_regs_2048_1536_30fps_mipi_3m,
	}
};

static enum v4l2_mbus_pixelcode sc4236_mbus_code[] = {
	V4L2_MBUS_FMT_SBGGR10_1X10,
};

/*
 * the part of driver was fixed.
 */

static struct regval_list sc4236_stream_on[] = {
	{0x0100, 0x01},
	{SC4236_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc4236_stream_off[] = {
	{0x0100, 0x00},
	{SC4236_REG_END, 0x00},	/* END MARKER */
};

int sc4236_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= buf,
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

int sc4236_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg>>8)&0xff, reg&0xff, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int sc4236_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC4236_REG_END) {
		if (vals->reg_num == SC4236_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc4236_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int sc4236_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC4236_REG_END) {
		if (vals->reg_num == SC4236_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc4236_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int sc4236_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sc4236_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sc4236_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC4236_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc4236_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC4236_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc4236_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
	ret = sc4236_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0x0f));
	ret += sc4236_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc4236_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));

	if (value < 160) {
		ret += sc4236_write(sd, 0x3314, 0x14);
	}
	else if(value > 320){
		ret += sc4236_write(sd, 0x3314, 0x04);
	}
	if (ret < 0)
		return ret;

	return 0;
}

static int sc4236_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret += sc4236_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sc4236_write(sd, 0x3e08, (unsigned char)((value >> 8 << 2) | 0x03));
	if (ret < 0)
		return ret;
	/* denoise logic */
	if (value < 0x110) {
		sc4236_write(sd,0x3812,0x00);
		sc4236_write(sd,0x3301,0x1e);
		sc4236_write(sd,0x3633,0x23);
		sc4236_write(sd,0x3630,0x80);
		sc4236_write(sd,0x3622,0xf6);
		sc4236_write(sd,0x3812,0x30);
	}
	else if (value>=0x110&&value<0x310){
		sc4236_write(sd,0x3812,0x00);
		sc4236_write(sd,0x3301,0x50);
		sc4236_write(sd,0x3633,0x23);
		sc4236_write(sd,0x3630,0x80);
		sc4236_write(sd,0x3622,0xf6);
		sc4236_write(sd,0x3812,0x30);
	}
	else if(value>=0x310&&value<0x710){
		sc4236_write(sd,0x3812,0x00);
		sc4236_write(sd,0x3301,0x50);
		sc4236_write(sd,0x3633,0x23);
		sc4236_write(sd,0x3630,0x80);
		sc4236_write(sd,0x3622,0xf6);
		sc4236_write(sd,0x3812,0x30);
	}
	else if(value>=0x710&&value<=0x71e){
		sc4236_write(sd,0x3812,0x00);
		sc4236_write(sd,0x3301,0x50);
		sc4236_write(sd,0x3633,0x23);
		sc4236_write(sd,0x3630,0x80);
		sc4236_write(sd,0x3622,0xf6);
		sc4236_write(sd,0x3812,0x30);
	}
	else{ //may be flick
		sc4236_write(sd,0x3812,0x00);
		sc4236_write(sd,0x3301,0x50);
		sc4236_write(sd,0x3633,0x43);
		sc4236_write(sd,0x3630,0x82);
		sc4236_write(sd,0x3622,0x16);
		sc4236_write(sd,0x3812,0x30);
	}

	return 0;
}

static int sc4236_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc4236_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc4236_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = &sc4236_win_sizes[0];
	int ret = 0;

	if(!enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;

	ret = sc4236_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc4236_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = sc4236_write_array(sd, sc4236_stream_on);
		pr_debug("sc4236 stream on\n");
	}
	else {
		ret = sc4236_write_array(sd, sc4236_stream_off);
		pr_debug("sc4236 stream off\n");
	}
	return ret;
}

static int sc4236_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int pclk = 0;
	unsigned short hts;
	unsigned short vts = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0; //the format is 24.8
	int ret = 0;
	unsigned char val = 0;

	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		pclk = SC4236_SUPPORT_PCLK_FPS_30;
		max_fps = SENSOR_OUTPUT_MAX_FPS;
		break;
	case TX_SENSOR_MAX_FPS_15:
		pclk = SC4236_SUPPORT_PCLK_FPS_15;
		max_fps = TX_SENSOR_MAX_FPS_15;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)){
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	ret += sc4236_read(sd, 0x320c, &val);
	hts = val<<8;
	val = 0;
	ret += sc4236_read(sd, 0x320d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: sc4236 read err\n");
		return ret;
	}

	vts = pclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = sc4236_write(sd,0x3812,0x00);
	ret += sc4236_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc4236_write(sd, 0x320e, (unsigned char)(vts >> 8));
	ret += sc4236_write(sd,0x3812,0x30);
	if (0 != ret) {
		ISP_ERROR("err: sc4236_write err\n");
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

static int sc4236_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;

	if(value == TX_ISP_SENSOR_FULL_RES_MAX_FPS){
		wsize = &sc4236_win_sizes[0];
	}else if(value == TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS){
		wsize = &sc4236_win_sizes[0];
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

static int sc4236_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc4236_reset");
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
		ret = private_gpio_request(pwdn_gpio, "sc4236_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc4236_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc4236 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("sc4236 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "sc4236", sizeof("sc4236"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc4236_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = sc4236_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = sc4236_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc4236_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc4236_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc4236_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = sc4236_write_array(sd, sc4236_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sc4236_write_array(sd, sc4236_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc4236_set_fps(sd, *(int*)arg);
		break;
	default:
		break;;
	}

	return 0;
}

static int sc4236_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc4236_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc4236_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;
	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc4236_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc4236_core_ops = {
	.g_chip_ident = sc4236_g_chip_ident,
	.reset = sc4236_reset,
	.init = sc4236_init,
	.g_register = sc4236_g_register,
	.s_register = sc4236_s_register,
};

static struct tx_isp_subdev_video_ops sc4236_video_ops = {
	.s_stream = sc4236_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc4236_sensor_ops = {
	.ioctl	= sc4236_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc4236_ops = {
	.core = &sc4236_core_ops,
	.video = &sc4236_video_ops,
	.sensor = &sc4236_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc4236",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc4236_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &sc4236_win_sizes[0];

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

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sc4236_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc4236_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	ISP_WARNING("probe ok ------->sc4236\n");

	return 0;
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int sc4236_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc4236_id[] = {
	{ "sc4236", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc4236_id);

static struct i2c_driver sc4236_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc4236",
	},
	.probe		= sc4236_probe,
	.remove		= sc4236_remove,
	.id_table	= sc4236_id,
};

static __init int init_sc4236(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init sc4236 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&sc4236_driver);
}

static __exit void exit_sc4236(void)
{
	private_i2c_del_driver(&sc4236_driver);
}

module_init(init_sc4236);
module_exit(exit_sc4236);

MODULE_DESCRIPTION("A low-level driver for Smartsenstech sc4236 sensors");
MODULE_LICENSE("GPL");
