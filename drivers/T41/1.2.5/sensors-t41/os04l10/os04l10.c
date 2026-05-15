/*
 * os04l10.c
 *
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps       interface       bit        mode
 *   1          2560*1440       30        mipi_2lane     RAW10      linear
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

#define OS04L10_CHIP_ID_H	(0x53)
#define OS04L10_CHIP_ID_M	(0x04)
#define OS04L10_CHIP_ID_L	(0x4c)
#define OS04L10_REG_END		0xffff
#define OS04L10_REG_DELAY	0xfffe
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20240102a"

static int reset_gpio = -1;
static int pwdn_gpio = -1;

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

struct again_lut os04l10_again_lut[] = {
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
	{0x36, 114688},
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

struct tx_isp_sensor_attribute os04l10_attr;

unsigned int os04l10_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = os04l10_again_lut;
	while(lut->gain <= os04l10_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == os04l10_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int os04l10_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus os04l10_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 720,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.image_twidth = 2560,
	.image_theight = 1440,
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

struct tx_isp_sensor_attribute os04l10_attr={
	.name = "os04l10",
	.chip_id = 0x53044c,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x36,
	.max_again = 259142,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = os04l10_alloc_again,
	.sensor_ctrl.alloc_dgain = os04l10_alloc_dgain,
};

static struct regval_list os04l10_init_regs_2560_1440_25fps_mipi[] = {
        {0x36, 0x01},
        {0xfd, 0x00},
        {0x36, 0x00},
        {0xfd, 0x00},
        {0x20, 0x00},
        {OS04L10_REG_DELAY, 0x05},
        {0xfd, 0x00},
        {0x20, 0x01},
        {0x38, 0x15},
        {0xfd, 0x00},
        {0x31, 0x30},
        {0x41, 0x14},
        {0xfd, 0x05},
        {0x2f, 0x02},
        {0x37, 0x00},
        {0x39, 0x40},
        {0xfd, 0x01},
        {0x03, 0x00},
        {0x04, 0x02},
        {0x05, 0x01},
        {0x06, 0x28},
        {0x24, 0xff},
        {0x01, 0x03},
        {0x44, 0x81},
        {0x47, 0x0c},
        {0x48, 0x0c},
        {0xdb, 0x1f},
        {0xdc, 0x1a},
        {0xdf, 0xb5},
        {0xfd, 0x01},
        {0x50, 0x01},
        {0x51, 0x0a},
        {0x52, 0x0c},
        {0x53, 0x0e},
        {0x54, 0x01},
        {0x55, 0x0e},
        {0x57, 0x10},
        {0x5c, 0x3a},
        {0x5e, 0x08},
        {0x76, 0x12},
        {0x7c, 0x10},
        {0x7d, 0x3a},
        {0x7f, 0x1a},
        {0x90, 0xa1},
        {0x91, 0x0f},
        {0x92, 0x21},
        {0x93, 0x1e},
        {0x94, 0x0f},
        {0x95, 0x7a},
        {0x98, 0xa0},
        {0xca, 0x42},
        {0xcb, 0x48},
        {0xcc, 0x42},
        {0xcd, 0x52},
        {0xce, 0x40},
        {0xcf, 0x46},
        {0xd0, 0x40},
        {0xd1, 0x50},
        {0xfd, 0x01},
        {0x46, 0x77},
        {0xdd, 0x00},
        {0xde, 0x3f},
        {0xfd, 0x03},
        {0x2b, 0x45},
        {0x00, 0x0f},
        {0x01, 0x01},
        {0x02, 0x03},
        {0x2a, 0xa2},
        {0x29, 0x6f},
        {0x1e, 0x11},
        {0x1f, 0x11},
        {0x1a, 0x15},
        {0x1b, 0x2a},
        {0x1c, 0x51},
        {0x1d, 0x98},
        {0xfd, 0x03},
        {0x05, 0x0c},
        {0x08, 0x2c},
        {0x12, 0xd8},
        {0x13, 0x28},
        {0x14, 0x03},
        {0x16, 0x0c},
        {0x17, 0x0c},
        {0x18, 0x0c},
        {0x19, 0x0c},
        {0x21, 0x86},
        {0x22, 0x55},
        {0x23, 0x77},
        {0x2c, 0x99},
        {0x2d, 0x03},
        {0xfd, 0x05},
        {0xc4, 0x60},
        {0xc5, 0x60},
        {0xc6, 0x60},
        {0xc7, 0x60},
        {0xf0, 0x40},
        {0xf1, 0x40},
        {0xf2, 0x40},
        {0xf3, 0x40},
        {0xf4, 0x00},
        {0xf6, 0x00},
        {0xf7, 0xc0},
        {0xfc, 0x00},
        {0xfe, 0xc0},
        {0xff, 0xcc},
        {0xfa, 0x5c},
        {0xfb, 0x6b},
        {0xfd, 0x02},
        {0x34, 0x80},
        {0x5e, 0x22},
        {0xa1, 0x04},
        {0xa3, 0xa0},
        {0xa5, 0x04},
        {0xa7, 0x00},
        {0xfd, 0x05},
        {0x8e, 0x0a},
        {0x8f, 0x00},
        {0x90, 0x05},
        {0x91, 0xa0},
        {0x92, 0x19},
        {0x9d, 0x03},
        {0x9e, 0x5f},
        {0xa1, 0x03},
        {0xb1, 0x01},
        {0xae, 0x65},
        {0xfd, 0x05},
        {0xb1, 0x03},
        {0xfd, 0x01},
        {0x33, 0x03},
        {0x01, 0x02},
	{OS04L10_REG_END, 0x00},/* END MARKER */
};

static struct tx_isp_sensor_win_setting os04l10_win_sizes[] = {
	{
		.width		= 2560,
		.height		= 1440,
		.fps		= 25 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= os04l10_init_regs_2560_1440_25fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &os04l10_win_sizes[0];

static struct regval_list os04l10_stream_on_mipi[] = {
	{OS04L10_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list os04l10_stream_off_mipi[] = {
	{OS04L10_REG_END, 0x00},	/* END MARKER */
};

int os04l10_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[1] = {reg};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
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

int os04l10_write(struct tx_isp_subdev *sd, uint16_t reg,
		 unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg & 0xff), value};
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
static int os04l10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != OS04L10_REG_END) {
		if (vals->reg_num == OS04L10_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = os04l10_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int os04l10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != OS04L10_REG_END) {
		if (vals->reg_num == OS04L10_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = os04l10_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int os04l10_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int os04l10_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = os04l10_read(sd, 0x02, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS04L10_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = os04l10_read(sd, 0x03, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS04L10_CHIP_ID_M)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	ret = os04l10_read(sd, 0x04, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS04L10_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int os04l10_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = -1;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret += os04l10_write(sd, 0xfd, 0x01);
	ret += os04l10_write(sd, 0x03, (unsigned char)((it >> 8) & 0xff));
	ret += os04l10_write(sd, 0x04, (unsigned char)(it & 0xff));

	ret += os04l10_write(sd, 0x24, (unsigned char)(again & 0xff));
	ret += os04l10_write(sd, 0x01, 0x01);

	return 0;
}

#if 0
static int os04l10_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += os04l10_write(sd, 0xfd, 0x01);
	ret += os04l10_write(sd, 0x03, (unsigned char)((value >> 8) & 0xff));
	ret += os04l10_write(sd, 0x04, (unsigned char)(value & 0xff));
	ret += os04l10_write(sd, 0x01, 0x01);
	if (ret < 0)
		return ret;

	return 0;
}

static int os04l10_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += os04l10_write(sd, 0xfd, 0x01);
	ret += os04l10_write(sd, 0x24, (unsigned char)(value & 0xff));
	ret += os04l10_write(sd, 0x01, 0x01);
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int os04l10_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int os04l10_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_set_attr(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

	return 0;
}

static int os04l10_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!init->enable)
		return ISP_SUCCESS;

	sensor_set_attr(sd, wsize);
	sensor->video.state = TX_ISP_MODULE_INIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int os04l10_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if(sensor->video.state == TX_ISP_MODULE_INIT){
			ret = os04l10_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if(sensor->video.state == TX_ISP_MODULE_RUNNING){

			ret = os04l10_write_array(sd, os04l10_stream_on_mipi);
			ISP_WARNING("os04l10 stream on\n");
		}
	}
	else {
		ret = os04l10_write_array(sd, os04l10_stream_off_mipi);
		ISP_WARNING("os04l10 stream off\n");
	}

	return ret;
}

static int os04l10_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int vb = 0;
	unsigned int vts_old = 0;
	unsigned int vb_old = 0;
	unsigned int max_fps = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch(sensor->info.default_boot){
	case 0:
		sclk = 816 * 1764 * 25;
		max_fps = TX_SENSOR_MAX_FPS_25;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps<< 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}

	ret += os04l10_write(sd, 0xfd, 0x01);

	ret += os04l10_read(sd, 0x37, &val);
	hts = val;
	ret += os04l10_read(sd, 0x38, &val);
	hts = ((hts << 8) | val);

	ret += os04l10_read(sd, 0x35, &val);
	vts_old = val;
	ret += os04l10_read(sd, 0x36, &val);
	vts_old = ((vts_old << 8) | val);

	ret += os04l10_read(sd, 0x05, &val);
	vb_old = val;
	ret += os04l10_read(sd, 0x06, &val);
	vb_old = ((vb_old << 8) | val);
	if (0 != ret) {
		ISP_ERROR("err: os04l10 read err\n");
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

        vb = vts - vts_old + vb_old;
	ret = os04l10_write(sd, 0x06, (unsigned char)(vb & 0xff));
	ret += os04l10_write(sd, 0x05, (unsigned char)(vb >> 8));

	ret += os04l10_write(sd, 0x01, 0x01);

	if (0 != ret) {
		ISP_ERROR("err: os04l10_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 9;
	sensor->video.attr->integration_time_limit = vts - 9;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 9;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int os04l10_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int os04l10_set_hvflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t value;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	ret += os04l10_write(sd, 0xfd, 0x01);
	ret += os04l10_read(sd, 0x32, &value);
	switch(enable) {
	case 0:
		value &= 0xFC;
                sensor->video.mbus.code =TISP_VI_FMT_SBGGR10_1X10;
		break;
	case 1:
		value = ((value & 0xFD) | 0x01);
                sensor->video.mbus.code =TISP_VI_FMT_SGBRG10_1X10;
		break;
	case 2:
		value = ((value & 0xFE) | 0x02);
                sensor->video.mbus.code =TISP_VI_FMT_SGRBG10_1X10;
		break;
	case 3:
		value |= 0x03;
                sensor->video.mbus.code =TISP_VI_FMT_SRGGB10_1X10;
		break;
	}
	sensor->video.mbus_change = 1;
        ret += tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	ret += os04l10_write(sd, 0x32, value);
	ret += os04l10_write(sd, 0x01, 0x01);

	return ret;
}

static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	unsigned long rate;
	int ret;

	switch(info->default_boot){
	case 0:
		wsize = &os04l10_win_sizes[0];
		memcpy(&(os04l10_attr.mipi), &os04l10_mipi, sizeof(os04l10_mipi));
		os04l10_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		os04l10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		os04l10_attr.max_integration_time_native = 1764 - 9;
		os04l10_attr.integration_time_limit = 1764 - 9;
		os04l10_attr.total_width = 816;
		os04l10_attr.total_height = 1764;
		os04l10_attr.max_integration_time = 1764 - 9;
		os04l10_attr.again = 0x80;
		os04l10_attr.integration_time = 0x640;
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
	}

	switch(info->video_interface){
	case TISP_SENSOR_VI_MIPI_CSI0:
		os04l10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		os04l10_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_DVP:
		os04l10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this Interface Source!!!\n");
	}

	switch(info->mclk){
	case TISP_SENSOR_MCLK0:
	case TISP_SENSOR_MCLK1:
	case TISP_SENSOR_MCLK2:
                sclka = private_devm_clk_get(&client->dev, SEN_MCLK);
                sensor->mclk = private_devm_clk_get(sensor->dev, SEN_BCLK);
		set_sensor_mclk_function(0);
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	rate = private_clk_get_rate(sensor->mclk);
	switch(info->default_boot){
	case 0:
                if (((rate / 1000) % 24000) != 0) {
                        ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
                        sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
                        if (IS_ERR(sclka)) {
                                pr_err("get sclka failed\n");
                        } else {
                                rate = private_clk_get_rate(sclka);
                                if (((rate / 1000) % 24000) != 0) {
                                        private_clk_set_rate(sclka, 1200000000);
                                }
                        }
                }
                private_clk_set_rate(sensor->mclk, 24000000);
                private_clk_prepare_enable(sensor->mclk);
                break;
	}

	ISP_WARNING("\n====>[default_boot=%d] [resolution=%dx%d] [video_interface=%d] [MCLK=%d] \n", info->default_boot, wsize->width, wsize->height, info->video_interface, info->mclk);
	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;
        sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

	return 0;

}

static int os04l10_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"os04l10_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"os04l10_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = os04l10_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an os04l10 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("os04l10 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "os04l10", sizeof("os04l10"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int os04l10_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = os04l10_set_expo(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		//if(arg)
		//	ret = os04l10_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		//if(arg)
		//	ret = os04l10_set_analog_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = os04l10_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = os04l10_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = os04l10_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = os04l10_write_array(sd, os04l10_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = os04l10_write_array(sd, os04l10_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = os04l10_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = os04l10_set_hvflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int os04l10_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = os04l10_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int os04l10_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	os04l10_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops os04l10_core_ops = {
	.g_chip_ident = os04l10_g_chip_ident,
	.reset = os04l10_reset,
	.init = os04l10_init,
	.g_register = os04l10_g_register,
	.s_register = os04l10_s_register,
};

static struct tx_isp_subdev_video_ops os04l10_video_ops = {
	.s_stream = os04l10_s_stream,
};

static struct tx_isp_subdev_sensor_ops	os04l10_sensor_ops = {
	.ioctl	= os04l10_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops os04l10_ops = {
	.core = &os04l10_core_ops,
	.video = &os04l10_video_ops,
	.sensor = &os04l10_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "os04l10",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int os04l10_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
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

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->dev = &client->dev;
	os04l10_attr.expo_fs = 1;
	sensor->video.shvflip = 1;
	sensor->video.attr = &os04l10_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &os04l10_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->os04l10\n");

	return 0;
}

static int os04l10_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	private_devm_clk_put(&client->dev, sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id os04l10_id[] = {
	{ "os04l10", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, os04l10_id);

static struct i2c_driver os04l10_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "os04l10",
	},
	.probe		= os04l10_probe,
	.remove		= os04l10_remove,
	.id_table	= os04l10_id,
};

static __init int init_os04l10(void)
{
	return private_i2c_add_driver(&os04l10_driver);
}

static __exit void exit_os04l10(void)
{
	private_i2c_del_driver(&os04l10_driver);
}

module_init(init_os04l10);
module_exit(exit_os04l10);

MODULE_DESCRIPTION("A low-level driver for os04l10 sensors");
MODULE_LICENSE("GPL");
