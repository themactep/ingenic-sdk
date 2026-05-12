/*
* os04j10.c
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

#define OS04J10_CHIP_ID_H	(0x53)
#define OS04J10_CHIP_ID_M	(0x04)
#define OS04J10_CHIP_ID_L	(0x4a)
#define OS04J10_REG_END		0xffff
#define OS04J10_REG_DELAY	0xfffe
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20240729a"

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

struct again_lut os04j10_again_lut[] = {
	{0x0010, 0},
	{0x0011, 5732},
	{0x0012, 11136},
	{0x0013, 16248},
	{0x0014, 21098},
	{0x0015, 25711},
	{0x0016, 30109},
	{0x0017, 34312},
	{0x0018, 38336},
	{0x0019, 42196},
	{0x001a, 45904},
	{0x001b, 49472},
	{0x001c, 52911},
	{0x001d, 56229},
	{0x001e, 59434},
	{0x001f, 62534},
	{0x0020, 65536},
	{0x0022, 71268},
	{0x0024, 76672},
	{0x0026, 81784},
	{0x0028, 86634},
	{0x002a, 91247},
	{0x002c, 95645},
	{0x002e, 99848},
	{0x0030, 103872},
	{0x0032, 107732},
	{0x0034, 111440},
	{0x0036, 115008},
	{0x0038, 118447},
	{0x003a, 121765},
	{0x003c, 124970},
	{0x003e, 128070},
	{0x0040, 131072},
	{0x0044, 136804},
	{0x0048, 142208},
	{0x004c, 147320},
	{0x0050, 152170},
	{0x0054, 156783},
	{0x0058, 161181},
	{0x005c, 165384},
	{0x0060, 169408},
	{0x0064, 173268},
	{0x0068, 176976},
	{0x006c, 180544},
	{0x0070, 183983},
	{0x0074, 187301},
	{0x0078, 190506},
	{0x007c, 193606},
	{0x0080, 196608},
	{0x0088, 202340},
	{0x0090, 207744},
	{0x0098, 212856},
	{0x00a0, 217706},
	{0x00a8, 222319},
	{0x00b0, 226717},
	{0x00b8, 230920},
	{0x00c0, 234944},
	{0x00c8, 238804},
	{0x00d0, 242512},
	{0x00d8, 246080},
	{0x00e0, 249519},
	{0x00e8, 252837},
	{0x00f0, 256042},
	{0x00f8, 259142},
};

struct tx_isp_sensor_attribute os04j10_attr;

unsigned int os04j10_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = os04j10_again_lut;
	while(lut->gain <= os04j10_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == os04j10_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int os04j10_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus os04j10_mipi={
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

struct tx_isp_sensor_attribute os04j10_attr={
	.name = "os04j10",
	.chip_id = 0x53044a,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x3c,
	.max_again = 259142,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = os04j10_alloc_again,
	.sensor_ctrl.alloc_dgain = os04j10_alloc_dgain,
};

static struct regval_list os04j10_init_regs_2560_1440_30fps_mipi[] = {
		{0xfc,0x01},
		{0xfd,0x00},
		{0x09,0x06},
		{0x09,0x00},
		{0x14,0xde},
		{0x23,0x74},
		{0x1f,0x29},
		{0x20,0x01},
		{0x24,0x45},
		{0x25,0x21},
		{0x29,0x21},
		{0x30,0x0a},
		{0x31,0x08},
		{0x32,0x05},
		{0x33,0xa8},
		{0x3d,0x05},
		{0x3a,0x0c},
		{0x2d,0x0e},
		{0x36,0x0a},
		{0x2f,0x0c},
		{0x3b,0x2f},
		{0x35,0x13},
		{0x3c,0x0a},
		{0x37,0x09},
		{0x40,0x02},
		{0x41,0xff},
		{0x47,0x20},
		{0x58,0xd0},
		{0x5a,0x0d},
		{0x6b,0x01},
		{0x60,0x04},
		{0x61,0xa8},
		{0x62,0xbc},
		{0x63,0xf8},
		{0x64,0x07},
		{0x65,0xae},
		{0x0a,0x23},
		{0x0b,0xf4},
		{0xa1,0x16},
		{0xa2,0xca},
		{0xa4,0x3f},
		{0xa8,0x42},
		{0xaa,0x0b},
		{0xb2,0x01},
		{0xb4,0x00},
		{0xb5,0xdf},
		{0xb8,0xdd},
		{0xb9,0x0f},
		{0xab,0x07},
		{0xac,0x06},
		{0xad,0x0f},
		{0xaf,0xd9},
		{0xb0,0xcb},
		{0xb1,0x00},
		{0xbf,0x2e},
		{0xc0,0x5d},
		{0xc1,0xc1},
		{0xc2,0xc0},
		{0xc3,0x04},
		{0xc4,0x0a},
		{0xc5,0x1a},
		{0xc6,0x26},
		{0xd8,0x20},
		{0xd9,0x00},
		{0xc7,0x40},
		{0xc8,0xf8},
		{0xc9,0x00},
		{0xca,0xb0},
		{0xcb,0x77},
		{0xcc,0x67},
		{0xcd,0x44},
		{0xce,0x34},
		{0xcf,0xa9},
		{0xd0,0xea},
		{0xd2,0xda},
		{0xe0,0xff},
		{0xe6,0x00},
		{0xeb,0x55},
		{0xec,0x55},
		{0xed,0x0a},
		{0xfd,0x01},
		{0x0e,0x01},
		{0x0f,0x04},
		{0x15,0x08},
		{0x19,0x00},
		{0x1f,0x00},
		{0x20,0x40},
		{0x22,0x01},
		{0x23,0x00},
		{0x24,0xff},
		{0x5c,0x14},
		{0x5d,0x1c},
		{0x5e,0x13},
		{0x66,0x14},
		{0x69,0x13},
		{0x6d,0x7c},
		{0x6e,0x20},
		{0x74,0x01},
		{0x7a,0x51},
		{0x7b,0xff},
		{0x82,0x12},
		{0x83,0x06},
		{0x8e,0x32},
		{0x8f,0x06},
		{0x90,0x45},
		{0x91,0x20},
		{0x92,0x17},
		{0x93,0x40},
		{0x95,0x3c},
		{0xb0,0x15},
		{0xb1,0x08},
		{0xb6,0x02},
		{0xb7,0x11},
		{0xfd,0x02},
		{0x11,0x16},
		{0x12,0xbc},
		{0x13,0x55},
		{0x14,0x6f},
		{0x17,0xf2},
		{0x20,0x2a},
		{0x21,0x32},
		{0x22,0x32},
		{0x23,0x44},
		{0x1b,0x44},
		{0x28,0x2c},
		{0x29,0x30},
		{0x2a,0x30},
		{0x2b,0x42},
		{0x1d,0x42},
		{0x2e,0x2f},
		{0x2f,0x33},
		{0x30,0x33},
		{0x31,0x45},
		{0x1e,0x45},
		{0x2c,0x3f},
		{0x37,0x15},
		{0x38,0x15},
		{0xfd,0x03},
		{0x0d,0x40},
		{0x9a,0x00},
		{0xfd,0x04},
		{0x05,0x01},
		{0x0b,0x0c},
		{0x0c,0x0f},
		{0xfd,0x04},
		{0x1b,0x01},
		{0x9b,0x01},
		{0xfd,0x05},
		{0x10,0xd0},
		{0x17,0x08},
		{0x42,0x00},
		{0x43,0x7f},
		{0x44,0x00},
		{0x45,0x7f},
		{0x46,0x00},
		{0x47,0x7f},
		{0x48,0x00},
		{0x49,0x7f},
		{0xb3,0x07},
		{0xb4,0xff},
		{0xfd,0x05},
		{0xc5,0x06},
		{0xc9,0x06},
		{0xcd,0x06},
		{0xc6,0x13},
		{0xca,0x13},
		{0xce,0x13},
		{0xc3,0x05},
		{0xc7,0x05},
		{0xcb,0x05},
		{0xc4,0x04},
		{0xc8,0x04},
		{0xcc,0x04},
		{0xcf,0x04},
		{0xd3,0x04},
		{0xd7,0x04},
		{0xd0,0x08},
		{0xd4,0x08},
		{0xd8,0x08},
		{0xbb,0x47},
		{0xd1,0x01},
		{0xd5,0x01},
		{0xd9,0x01},
		{0xd2,0x0A},
		{0xd6,0x0A},
		{0xda,0x0A},
		{0xbc,0x3F},
		{0xfd,0x09},
		{0x9d,0xc1},
		{0x83,0x08},
		{0x8b,0x17},
		{0xc1,0x18},
		{0xc3,0x18},
		{0xfd,0x03},
		{0x16,0x00},
		{0x17,0x04},
		{0x18,0x0a},
		{0x19,0x00},
		{0x1a,0x00},
		{0x1b,0x04},
		{0x1c,0x05},
		{0x1d,0xa0},
		{0xfd,0x00},
		{0x30,0x0a},
		{0x31,0x00},
		{0x32,0x05},
		{0x33,0xa0},
		{0xfd,0x00},
		{0x59,0x00},
		{0x2c,0x01},
		{0x57,0x10},
		{0xfb,0x03},
	{OS04J10_REG_END, 0x00},/* END MARKER */
};

static struct tx_isp_sensor_win_setting os04j10_win_sizes[] = {
	{
		.width		= 2560,
		.height		= 1440,
		.fps		= 30 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= os04j10_init_regs_2560_1440_30fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &os04j10_win_sizes[0];

static struct regval_list os04j10_stream_on_mipi[] = {
	{OS04J10_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list os04j10_stream_off_mipi[] = {
	{OS04J10_REG_END, 0x00},	/* END MARKER */
};

int os04j10_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int os04j10_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int os04j10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != OS04J10_REG_END) {
		if (vals->reg_num == OS04J10_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = os04j10_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int os04j10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != OS04J10_REG_END) {
		if (vals->reg_num == OS04J10_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = os04j10_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int os04j10_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int os04j10_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = os04j10_read(sd, 0x02, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS04J10_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = os04j10_read(sd, 0x03, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS04J10_CHIP_ID_M)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	ret = os04j10_read(sd, 0x04, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OS04J10_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 16) | v;

	return 0;
}

static int os04j10_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = -1;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret += os04j10_write(sd, 0xfd, 0x01);
	ret += os04j10_write(sd, 0x0e, (unsigned char)((it >> 8) & 0xff));
	ret += os04j10_write(sd, 0x0f, (unsigned char)(it & 0xff));

	ret += os04j10_write(sd, 0x24, (unsigned char)(again & 0xff));
	ret += os04j10_write(sd, 0xfe, 0x02);

	return 0;
}

#if 0
static int os04j10_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += os04j10_write(sd, 0xfd, 0x01);
	ret += os04j10_write(sd, 0x03, (unsigned char)((value >> 8) & 0xff));
	ret += os04j10_write(sd, 0x04, (unsigned char)(value & 0xff));
	ret += os04j10_write(sd, 0x01, 0x01);
	if (ret < 0)
		return ret;

	return 0;
}

static int os04j10_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += os04j10_write(sd, 0xfd, 0x01);
	ret += os04j10_write(sd, 0x24, (unsigned char)(value & 0xff));
	ret += os04j10_write(sd, 0x01, 0x01);
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int os04j10_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int os04j10_get_black_pedestal(struct tx_isp_subdev *sd, int value)
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

static int os04j10_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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

static int os04j10_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if(sensor->video.state == TX_ISP_MODULE_INIT){
			ret = os04j10_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if(sensor->video.state == TX_ISP_MODULE_RUNNING){

			ret = os04j10_write_array(sd, os04j10_stream_on_mipi);
			ISP_WARNING("os04j10 stream on\n");
		}
	}
	else {
		ret = os04j10_write_array(sd, os04j10_stream_off_mipi);
		ISP_WARNING("os04j10 stream off\n");
	}

	return ret;
}

static int os04j10_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 1208 * 1489 * 30;
		max_fps = TX_SENSOR_MAX_FPS_30;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps<< 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}

	ret += os04j10_write(sd, 0xfd, 0x01);

	ret += os04j10_read(sd, 0x2a, &val);
	hts = val;
	ret += os04j10_read(sd, 0x2b, &val);
	hts = ((hts << 8) | val) << 1;

	ret += os04j10_read(sd, 0x2f, &val);
	vts_old = val;
	ret += os04j10_read(sd, 0x30, &val);
	vts_old = ((vts_old << 8) | val);

	ret += os04j10_read(sd, 0x14, &val);
	vb_old = val;
	ret += os04j10_read(sd, 0x15, &val);
	vb_old = ((vb_old << 8) | val);
	if (0 != ret) {
		ISP_ERROR("err: os04j10 read err\n");
		return -1;
	}
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	vb = vts - vts_old + vb_old;
	printk("-----hts=%d,vts=%d,vb=%d,fps=%d-----",hts, vts, vb, (((fps >> 16) & 0xffff)/(fps & 0xffff)));
	ret = os04j10_write(sd, 0x15, (unsigned char)(vb & 0xff));
	ret += os04j10_write(sd, 0x14, (unsigned char)(vb >> 8));

	ret += os04j10_write(sd, 0xfe, 0x02);

	if (0 != ret) {
		ISP_ERROR("err: os04j10_write err\n");
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

static int os04j10_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int os04j10_set_hvflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t value;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	ret += os04j10_write(sd, 0xfd, 0x01);
	ret += os04j10_read(sd, 0x12, &value);
	switch(enable) {
	case 0:
		value &= 0xFC;
		sensor->video.mbus.code =TISP_VI_FMT_SBGGR10_1X10;
		break;
	case 1:
		value = ((value & 0xFE) | 0x02);
		sensor->video.mbus.code =TISP_VI_FMT_SGBRG10_1X10;
		break;
	case 2:
		value = ((value & 0xFD) | 0x01);
		sensor->video.mbus.code =TISP_VI_FMT_SGRBG10_1X10;
		break;
	case 3:
		value |= 0x03;
		sensor->video.mbus.code =TISP_VI_FMT_SRGGB10_1X10;
		break;
	}
	sensor->video.mbus_change = 1;
		ret += tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	ret += os04j10_write(sd, 0x12, value);
	ret += os04j10_write(sd, 0xfe, 0x02);

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
		wsize = &os04j10_win_sizes[0];
		memcpy(&(os04j10_attr.mipi), &os04j10_mipi, sizeof(os04j10_mipi));
		os04j10_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		os04j10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		os04j10_attr.max_integration_time_native = 1489 - 8;
		os04j10_attr.integration_time_limit = 1489 - 8;
		os04j10_attr.total_width = 1208;
		os04j10_attr.total_height = 1489;
		os04j10_attr.max_integration_time = 1489 - 8;
		os04j10_attr.again = 0x80;
		os04j10_attr.integration_time = 0x640;
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
	}

	switch(info->video_interface){
	case TISP_SENSOR_VI_MIPI_CSI0:
		os04j10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		os04j10_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_DVP:
		os04j10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

static int os04j10_g_chip_ident(struct tx_isp_subdev *sd,
				struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"os04j10_reset");
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
		ret = private_gpio_request(pwdn_gpio,"os04j10_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = os04j10_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an os04j10 chip.\n",
			client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("os04j10 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "os04j10", sizeof("os04j10"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int os04j10_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = os04j10_set_expo(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		//if(arg)
		//	ret = os04j10_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		//if(arg)
		//	ret = os04j10_set_analog_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = os04j10_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = os04j10_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = os04j10_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = os04j10_write_array(sd, os04j10_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = os04j10_write_array(sd, os04j10_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = os04j10_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = os04j10_set_hvflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int os04j10_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = os04j10_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int os04j10_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	os04j10_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops os04j10_core_ops = {
	.g_chip_ident = os04j10_g_chip_ident,
	.reset = os04j10_reset,
	.init = os04j10_init,
	.g_register = os04j10_g_register,
	.s_register = os04j10_s_register,
};

static struct tx_isp_subdev_video_ops os04j10_video_ops = {
	.s_stream = os04j10_s_stream,
};

static struct tx_isp_subdev_sensor_ops	os04j10_sensor_ops = {
	.ioctl	= os04j10_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops os04j10_ops = {
	.core = &os04j10_core_ops,
	.video = &os04j10_video_ops,
	.sensor = &os04j10_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "os04j10",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int os04j10_probe(struct i2c_client *client,
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
	os04j10_attr.expo_fs = 1;
	sensor->video.shvflip = 1;
	sensor->video.attr = &os04j10_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &os04j10_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->os04j10\n");

	return 0;
}

static int os04j10_remove(struct i2c_client *client)
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

static const struct i2c_device_id os04j10_id[] = {
	{ "os04j10", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, os04j10_id);

static struct i2c_driver os04j10_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "os04j10",
	},
	.probe		= os04j10_probe,
	.remove		= os04j10_remove,
	.id_table	= os04j10_id,
};

static __init int init_os04j10(void)
{
	return private_i2c_add_driver(&os04j10_driver);
}

static __exit void exit_os04j10(void)
{
	private_i2c_del_driver(&os04j10_driver);
}

module_init(init_os04j10);
module_exit(exit_os04j10);

MODULE_DESCRIPTION("A low-level driver for os04j10 sensors");
MODULE_LICENSE("GPL");
