// SPDX-License-Identifier: GPL-2.0+
/*
 * sc202cs.c
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
#include <txx-funcs.h>

#define SENSOR_NAME "sc202cs"
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x36
#define SENSOR_MAX_WIDTH 1600
#define SENSOR_MAX_HEIGHT 1200
#define SENSOR_CHIP_ID 0xeb52
#define SENSOR_CHIP_ID_H (0xeb)
#define SENSOR_CHIP_ID_L (0x52)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_30FPS_SCLK (1920 * 1250 * 30)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20230110a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

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
	{0x80, 0},
	{0x84, 2886},
	{0x88, 5776},
	{0x8c, 8494},
	{0x90, 11136},
	{0x94, 13706},
	{0x98, 16287},
	{0x9c, 18723},
	{0xa0, 21097},
	{0xa4, 23414},
	{0xa8, 25746},
	{0xac, 27953},
	{0xb0, 30109},
	{0xb4, 32217},
	{0xb8, 34345},
	{0xbc, 36361},
	{0xc0, 38336},
	{0xc4, 40270},
	{0xc8, 42226},
	{0xcc, 44082},
	{0xd0, 45904},
	{0xd4, 47690},
	{0xd8, 49500},
	{0xdc, 51220},
	{0xe0, 52910},
	{0xe4, 54571},
	{0xe8, 56254},
	{0xec, 57857},
	{0xf0, 59433},
	{0xf4, 60984},
	{0xf8, 62558},
	{0xfc, 64059},
	{0x180, 65536},
	{0x184, 68422},
	{0x188, 71312},
	{0x18c, 74030},
	{0x190, 76672},
	{0x194, 79242},
	{0x198, 81823},
	{0x19c, 84259},
	{0x1a0, 86633},
	{0x1a4, 88950},
	{0x1a8, 91282},
	{0x1ac, 93489},
	{0x1b0, 95645},
	{0x1b4, 97753},
	{0x1b8, 99881},
	{0x1bc, 101897},
	{0x1c0, 103872},
	{0x1c4, 105806},
	{0x1c8, 107762},
	{0x1cc, 109618},
	{0x1d0, 111440},
	{0x1d4, 113226},
	{0x1d8, 115036},
	{0x1dc, 116756},
	{0x1e0, 118446},
	{0x1e4, 120107},
	{0x1e8, 121790},
	{0x1ec, 123393},
	{0x1f0, 124969},
	{0x1f4, 126520},
	{0x1f8, 128094},
	{0x1fc, 129595},
	{0x380, 131072},
	{0x384, 133958},
	{0x388, 136848},
	{0x38c, 139566},
	{0x390, 142208},
	{0x394, 144778},
	{0x398, 147359},
	{0x39c, 149795},
	{0x3a0, 152169},
	{0x3a4, 154486},
	{0x3a8, 156818},
	{0x3ac, 159025},
	{0x3b0, 161181},
	{0x3b4, 163289},
	{0x3b8, 165417},
	{0x3bc, 167433},
	{0x3c0, 169408},
	{0x3c4, 171342},
	{0x3c8, 173298},
	{0x3cc, 175154},
	{0x3d0, 176976},
	{0x3d4, 178762},
	{0x3d8, 180572},
	{0x3dc, 182292},
	{0x3e0, 183982},
	{0x3e4, 185643},
	{0x3e8, 187326},
	{0x3ec, 188929},
	{0x3f0, 190505},
	{0x3f4, 192056},
	{0x3f8, 193630},
	{0x3fc, 195131},
	{0x780, 196608},
	{0x784, 199494},
	{0x788, 202384},
	{0x78c, 205102},
	{0x790, 207744},
	{0x794, 210314},
	{0x798, 212895},
	{0x79c, 215331},
	{0x7a0, 217705},
	{0x7a4, 220022},
	{0x7a8, 222354},
	{0x7ac, 224561},
	{0x7b0, 226717},
	{0x7b4, 228825},
	{0x7b8, 230953},
	{0x7bc, 232969},
	{0x7c0, 234944},
	{0x7c4, 236878},
	{0x7c8, 238834},
	{0x7cc, 240690},
	{0x7d0, 242512},
	{0x7d4, 244298},
	{0x7d8, 246108},
	{0x7dc, 247828},
	{0x7e0, 249518},
	{0x7e4, 251179},
	{0x7e8, 252862},
	{0x7ec, 254465},
	{0x7f0, 256041},
	{0x7f4, 257592},
	{0x7f8, 259166},
	{0x7fc, 260667},
	{0xf80, 262144},
	{0xf84, 265030},
	{0xf88, 267920},
	{0xf8c, 270638},
	{0xf90, 273280},
	{0xf94, 275850},
	{0xf98, 278431},
	{0xf9c, 280867},
	{0xfa0, 283241},
	{0xfa4, 285558},
	{0xfa8, 287890},
	{0xfac, 290097},
	{0xfb0, 292253},
	{0xfb4, 294361},
	{0xfb8, 296489},
	{0xfbc, 298505},
	{0xfc0, 300480},
	{0xfc4, 302414},
	{0xfc8, 304370},
	{0xfcc, 306226},
	{0xfd0, 308048},
	{0xfd4, 309834},
	{0xfd8, 311644},
	{0xfdc, 313364},
	{0xfe0, 315054},
	{0xfe4, 316715},
	{0xfe8, 318398},
	{0xfec, 320001},
	{0xff0, 321577},
	{0xff4, 323128},
	{0xff8, 324702},
	{0xffc, 326203},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
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

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}
struct tx_isp_mipi_bus sensor_mipi ={
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 720,
		.lans = 1,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 1600,
		.image_theight = 1200,
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
};

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0xeb52,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 326203,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 1250-6,
	.integration_time_limit = 1250-6,
	.total_width = 1920,
	.total_height = 1250,
	.max_integration_time = 1250-6,
	.one_line_expr_in_us = 27,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_1600_1200_30fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x36e9,0x24},
	{0x301f,0x01},
	{0x3301,0xff},
	{0x3304,0x68},
	{0x3306,0x40},
	{0x3308,0x08},
	{0x3309,0xa8},
	{0x330b,0xb0},
	{0x330c,0x18},
	{0x330d,0xff},
	{0x330e,0x20},
	{0x331e,0x59},
	{0x331f,0x99},
	{0x3333,0x10},
	{0x335e,0x06},
	{0x335f,0x08},
	{0x3364,0x1f},
	{0x337c,0x02},
	{0x337d,0x0a},
	{0x338f,0xa0},
	{0x3390,0x01},
	{0x3391,0x03},
	{0x3392,0x1f},
	{0x3393,0xff},
	{0x3394,0xff},
	{0x3395,0xff},
	{0x33a2,0x04},
	{0x33ad,0x0c},
	{0x33b1,0x20},
	{0x33b3,0x38},
	{0x33f9,0x40},
	{0x33fb,0x48},
	{0x33fc,0x0f},
	{0x33fd,0x1f},
	{0x349f,0x03},
	{0x34a6,0x03},
	{0x34a7,0x1f},
	{0x34a8,0x38},
	{0x34a9,0x30},
	{0x34ab,0xb0},
	{0x34ad,0xb0},
	{0x34f8,0x1f},
	{0x34f9,0x20},
	{0x3630,0xa0},
	{0x3631,0x92},
	{0x3632,0x64},
	{0x3633,0x43},
	{0x3637,0x49},
	{0x363a,0x85},
	{0x363c,0x0f},
	{0x3650,0x31},
	{0x3670,0x0d},
	{0x3674,0xc0},
	{0x3675,0xa0},
	{0x3676,0xa0},
	{0x3677,0x92},
	{0x3678,0x96},
	{0x3679,0x9a},
	{0x367c,0x03},
	{0x367d,0x0f},
	{0x367e,0x01},
	{0x367f,0x0f},
	{0x3698,0x83},
	{0x3699,0x86},
	{0x369a,0x8c},
	{0x369b,0x94},
	{0x36a2,0x01},
	{0x36a3,0x03},
	{0x36a4,0x07},
	{0x36ae,0x0f},
	{0x36af,0x1f},
	{0x36bd,0x22},
	{0x36be,0x22},
	{0x36bf,0x22},
	{0x36d0,0x01},
	{0x370f,0x02},
	{0x3721,0x6c},
	{0x3722,0x8d},
	{0x3725,0xc5},
	{0x3727,0x14},
	{0x3728,0x04},
	{0x37b7,0x04},
	{0x37b8,0x04},
	{0x37b9,0x06},
	{0x37bd,0x07},
	{0x37be,0x0f},
	{0x3901,0x02},
	{0x3903,0x40},
	{0x3905,0x8d},
	{0x3907,0x00},
	{0x3908,0x41},
	{0x391f,0x41},
	{0x3933,0x80},
	{0x3934,0x02},
	{0x3937,0x6f},
	{0x393a,0x01},
	{0x393d,0x01},
	{0x393e,0xc0},
	{0x39dd,0x41},
	{0x3e00,0x00},
	{0x3e01,0x4d},
	{0x3e02,0xc0},
	{0x3e09,0x00},
	{0x4509,0x28},
	{0x450d,0x61},
	{0x0100,0x01},

	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 1600,
		.height = 1200,
		.fps = 30 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1600_1200_30fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

static struct regval_list sensor_stream_on_mipi[] = {
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
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

#if 0
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
#endif

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
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	/*set integration time*/
	ret += sensor_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
	/*set analog gain*/
	ret = sensor_write(sd, 0x3e07, (unsigned char)(again & 0xff));
	ret += sensor_write(sd, 0x3e09, (unsigned char)(((again >> 8) & 0xff)));
	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	printk("[ %s %d ] %d\n", __func__, __LINE__, value);
	ret += sensor_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));

	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = sensor_write(sd, 0x3e07, (unsigned char)(again & 0xff));
	ret += sensor_write(sd, 0x3e09, (unsigned char)(((again >> 8) & 0xff)));
	if (ret < 0)
		return ret;


	return 0;
}

#endif
static int sensor_set_logic(struct tx_isp_subdev *sd, int value)
{
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
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("%s stream on\n", SENSOR_NAME);

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
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

	sclk = SENSOR_SUPPORT_30FPS_SCLK;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}

	ret = sensor_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}

	hts = (hts << 8) + tmp;
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += sensor_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 6;
	sensor->video.attr->integration_time_limit = vts - 6;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 6;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

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
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
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
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sensor_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an %s chip.\n",
			  client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}
	ISP_WARNING("%s chip found @ 0x%02x (%s)\n",
		    SENSOR_NAME, client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = -1;
	unsigned char val = 0x0;

	ret += sensor_read(sd, 0x3221, &val);

	if (enable & 0x2)
		val |= 0x60;
	else
		val &= 0x9f;

	ret += sensor_write(sd, 0x3221, val);

	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
	     	ret = sensor_set_expo(sd, *(int*)arg);
		break;
/*	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sensor_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sensor_set_analog_gain(sd, *(int*)arg);
		break;
*/
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
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);

		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);

		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if (arg)
			ret = sensor_set_vflip(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if (arg)
			ret = sensor_set_logic(sd, *(int*)arg);
		break;
	default:
		break;

	}

	return ret;
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

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));

	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	/*
	  convert sensor-gain into isp-gain,
	*/
	wsize = &sensor_win_sizes[0];
	memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi1),sizeof(sensor_mipi1));
	sensor_attr.max_integration_time_native = 1250 - 6;
	sensor_attr.integration_time_limit = 1250 - 6;
	sensor_attr.total_width = 1920;
	sensor_attr.total_height = 1250;
	sensor_attr.max_integration_time = 1250 - 6;

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sensor_attr;
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

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
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
	sensor_common_init(&sensor_info);

	ret = private_driver_get_interface();
	if (ret) {
		ISP_ERROR("Failed to init %s driver.\n", SENSOR_NAME);
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
