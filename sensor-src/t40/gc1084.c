/*
 * gc1084.c
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

#define GC1084_CHIP_ID_H	(0x10)
#define GC1084_CHIP_ID_L	(0x84)
#define GC1084_REG_END		0xffff
#define GC1084_REG_DELAY	0xfffe
#define GC1084_SUPPORT_30FPS_SCLK (49500000)

#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20230614a"

static int reset_gpio = GPIO_PC(28);
static int pwdn_gpio = GPIO_PA(20);

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	int index;
	unsigned int again_pga_l;
	unsigned int again_pga_m;
	unsigned int again_pga_h;
	unsigned int gain_col_h;
	unsigned int gain_col_l;
	unsigned int gain_l;
	unsigned int gain;
};



struct again_lut gc1084_again_lut[] = {
	{0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0},             //1.000000
	{0x01, 0x0a, 0x00, 0x00, 0x01, 0x0c, 0x00, 16247},             //1.187500
	{0x02, 0x00, 0x01, 0x00, 0x01, 0x1a, 0x00, 32233},             //1.406250
	{0x03, 0x0a, 0x01, 0x00, 0x01, 0x2a, 0x00, 47704},             //1.656250
	{0x04, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 65536},             //2.000000
	{0x05, 0x0a, 0x02, 0x00, 0x02, 0x18, 0x00, 81783},             //2.375000
	{0x06, 0x00, 0x03, 0x00, 0x02, 0x33, 0x00, 97243},             //2.796875
	{0x07, 0x0a, 0x03, 0x00, 0x03, 0x14, 0x00, 113240},             //3.312500
	{0x08, 0x00, 0x04, 0x00, 0x04, 0x00, 0x02, 131072},             //4.000000
	{0x09, 0x0a, 0x04, 0x00, 0x04, 0x2f, 0x02, 147007},             //4.734375
	{0x0a, 0x00, 0x05, 0x00, 0x05, 0x26, 0x02, 162779},             //5.593750
	{0x0b, 0x0a, 0x05, 0x00, 0x06, 0x29, 0x02, 178999},             //6.640625
	{0x0c, 0x00, 0x06, 0x00, 0x08, 0x00, 0x02, 196608},             //8.000000
	{0x0d, 0x0a, 0x06, 0x00, 0x09, 0x1f, 0x04, 212700},             //9.484375
	{0x0e, 0x12, 0x46, 0x00, 0x0b, 0x0d, 0x04, 228446},             //11.203125
	{0x0f, 0x19, 0x66, 0x00, 0x0d, 0x12, 0x06, 244423},             //13.265625
	{0x10, 0x00, 0x04, 0x01, 0x10, 0x00, 0x06, 262144},             //16.000000
	{0x11, 0x0a, 0x04, 0x01, 0x12, 0x3e, 0x08, 278158},             //18.953125
	{0x12, 0x00, 0x05, 0x01, 0x16, 0x1a, 0x0a, 293982},             //22.406250
	{0x13, 0x0a, 0x05, 0x01, 0x1a, 0x23, 0x0c, 310015},             //26.546875
	{0x14, 0x00, 0x06, 0x01, 0x20, 0x00, 0x0c, 327680},             //32.000000
	{0x15, 0x0a, 0x06, 0x01, 0x25, 0x3b, 0x0f, 343732},             //37.921875
	{0x16, 0x12, 0x46, 0x01, 0x2c, 0x33, 0x12, 359485},             //44.796875
	{0x17, 0x19, 0x66, 0x01, 0x35, 0x06, 0x14, 375551},             //53.093750
	{0x18, 0x20, 0x06, 0x01, 0x3f, 0x3f, 0x15, 393216},             //64.000000

};
struct tx_isp_sensor_attribute gc1084_attr;

unsigned int gc1084_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;

	*sensor_it = expo;

	return isp_it;
}

unsigned int gc1084_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = gc1084_again_lut;
	while(lut->gain <= gc1084_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return lut[0].gain;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == gc1084_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
				return lut->gain;
			}
		}

		lut++;
	}

	return 0;
}

unsigned int gc1084_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus gc1084_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 396,
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
	.mipi_sc.data_type_value = 0,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute gc1084_attr={
	.name = "gc1084",
	.chip_id = 0x1084,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.cbus_device = 0x37,
	.max_again = 393216,
	.max_dgain = 0,
	.expo_fs = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = gc1084_alloc_again,
	.sensor_ctrl.alloc_dgain = gc1084_alloc_dgain,
};

static struct regval_list gc1084_init_regs_1280_720_30fps_mipi[] = {
	{0x03fe,0xf0},
	{0x03fe,0xf0},
	{0x03fe,0xf0},
	{0x03fe,0x00},
	{0x03f2,0x00},
	{0x03f3,0x00},
	{0x03f4,0x36},
	{0x03f5,0xc0},
	{0x03f6,0x13},
	{0x03f7,0x01},
	{0x03f8,0x2c},
	{0x03f9,0x21},
	{0x03fc,0xae},
	{0x0d05,0x08},//hts 0x898 = 2200
	{0x0d06,0x98},//
	{0x0d08,0x10},
	{0x0d0a,0x02},
	{0x000c,0x03},
	{0x0d0d,0x02},
	{0x0d0e,0xd4},
	{0x000f,0x05},
	{0x0010,0x08},
	{0x0017,0x08},
	{0x0d73,0x92},
	{0x0076,0x00},
	{0x0d76,0x00},
	{0x0d41,0x02},//vts 0x2ee = 750
	{0x0d42,0xee},
	{0x0d7a,0x0a},
	{0x006b,0x18},
	{0x0db0,0x9d},
	{0x0db1,0x00},
	{0x0db2,0xac},
	{0x0db3,0xd5},
	{0x0db4,0x00},
	{0x0db5,0x97},
	{0x0db6,0x09},
	{0x00d2,0xfc},
	{0x0d19,0x31},
	{0x0d20,0x40},
	{0x0d25,0xcb},
	{0x0d27,0x03},
	{0x0d29,0x40},
	{0x0d43,0x20},
	{0x0058,0x60},
	{0x00d6,0x66},
	{0x00d7,0x19},
	{0x0093,0x02},
	{0x00d9,0x14},
	{0x00da,0xc1},
	{0x0d2a,0x00},
	{0x0d28,0x04},
	{0x0dc2,0x84},
	{0x0050,0x30},
	{0x0080,0x07},
	{0x008c,0x05},
	{0x008d,0xa8},
	{0x0077,0x01},
	{0x0078,0xee},
	{0x0079,0x02},
	{0x0067,0xc0},
	{0x0054,0xff},
	{0x0055,0x02},
	{0x0056,0x00},
	{0x0057,0x04},
	{0x005a,0xff},
	{0x005b,0x07},
	{0x00d5,0x03},
	{0x0102,0xa9},
	{0x0d03,0x02},
	{0x0d04,0xd0},
	{0x007a,0x60},
	{0x04e0,0xff},
	{0x0414,0x75},
	{0x0415,0x75},
	{0x0416,0x75},
	{0x0417,0x75},
	{0x0122,0x00},
	{0x0121,0x80},
	{0x0428,0x10},
	{0x0429,0x10},
	{0x042a,0x10},
	{0x042b,0x10},
	{0x042c,0x14},
	{0x042d,0x14},
	{0x042e,0x18},
	{0x042f,0x18},
	{0x0430,0x05},
	{0x0431,0x05},
	{0x0432,0x05},
	{0x0433,0x05},
	{0x0434,0x05},
	{0x0435,0x05},
	{0x0436,0x05},
	{0x0437,0x05},
	{0x0153,0x00},
	{0x0190,0x01},
	{0x0192,0x02},
	{0x0194,0x04},
	{0x0195,0x02},
	{0x0196,0xd0},
	{0x0197,0x05},
	{0x0198,0x00},
	{0x0201,0x23},
	{0x0202,0x53},//0x53
	{0x0203,0xce},
	{0x0208,0x39},
	{0x0212,0x06},
	{0x0213,0x40},
	{0x0215,0x12},
	{0x0229,0x05},
	{0x023e,0x98},
	{0x031e,0x3e},
	{GC1084_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the jxf23_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting gc1084_win_sizes[] = {
	/* [0] 1920*1080 @max 25fps*/
	{
		.width		= 1280,
		.height		= 720,
		.fps		= 30 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SGRBG10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= gc1084_init_regs_1280_720_30fps_mipi,
	},
};

struct tx_isp_sensor_win_setting *wsize = &gc1084_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list gc1084_stream_on[] = {
	{GC1084_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc1084_stream_off[] = {
	{GC1084_REG_END, 0x00},	/* END MARKER */
};

int gc1084_read(struct tx_isp_subdev *sd,  uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg>>8)&0xff, reg&0xff};
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

int gc1084_write(struct tx_isp_subdev *sd, uint16_t reg,
		 unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
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

#if 0
static int gc1084_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != GC1084_REG_END) {
		if (vals->reg_num == GC1084_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc1084_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}
	return 0;
}
#endif

static int gc1084_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != GC1084_REG_END) {
		if (vals->reg_num == GC1084_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc1084_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int gc1084_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int gc1084_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = gc1084_read(sd, 0x03f0, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != GC1084_CHIP_ID_H)
		return -ENODEV;
	ret = gc1084_read(sd, 0x03f1, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != GC1084_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}


static int gc1084_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;
	struct again_lut *val_lut = gc1084_again_lut;

	/*set analog gain*/
	ret += gc1084_write(sd, 0x00d1, val_lut[again].again_pga_l);
	ret += gc1084_write(sd, 0x00d0, val_lut[again].again_pga_m);
	ret += gc1084_write(sd, 0x0dc1, val_lut[again].again_pga_h);
	ret += gc1084_write(sd, 0x00b8, val_lut[again].gain_col_h);
	ret += gc1084_write(sd, 0x00b9, val_lut[again].gain_col_l);
	ret += gc1084_write(sd, 0x0155, val_lut[again].gain_l);

	/*integration time*/
	ret += gc1084_write(sd, 0x0d03, (unsigned char)((it >> 8) & 0xff));
	ret += gc1084_write(sd, 0x0d04, (unsigned char)(it & 0xff));
	if (0 != ret)
		ISP_ERROR("%s reg write err!!\n");

	return ret;
}

#if 0
static int gc1084_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = gc1084_write(sd, 0x0d04, value & 0xff);
	ret += gc1084_write(sd, 0x0d03, value >> 8);
	if (ret < 0)
		ISP_ERROR("gc1084_write error  %d\n" ,__LINE__ );

	return ret;
}

static int gc1084_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	struct again_lut *val_lut = gc1084_again_lut;

	ret += gc1084_write(sd, 0x00d1, val_lut[again].again_pga_l);
	ret += gc1084_write(sd, 0x00d0, val_lut[again].again_pga_m);
	ret += gc1084_write(sd, 0x0dc1, val_lut[again].again_pga_h);
	ret += gc1084_write(sd, 0x00b8, val_lut[again].gain_col_h);
	ret += gc1084_write(sd, 0x00b9, val_lut[again].gain_col_l);
	ret += gc1084_write(sd, 0x0155, val_lut[again].gain_l);
	if (ret < 0) {
		ISP_ERROR("gc1084_write error  %d" ,__LINE__ );
		return ret;
	}

	return 0;
}
#endif
static int gc1084_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc1084_get_black_pedestal(struct tx_isp_subdev *sd, int value)
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

	return 0;
}

static int gc1084_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!init->enable)
		return ISP_SUCCESS;

	sensor_set_attr(sd, wsize);
	sensor->video.state = TX_ISP_MODULE_DEINIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int gc1084_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT){
			ret = gc1084_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = gc1084_write_array(sd, gc1084_stream_on);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("gc1084 stream on\n");
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
	} else {
		ret = gc1084_write_array(sd, gc1084_stream_off);
		pr_debug("gc1084 stream off\n");
	}

	return ret;
}

static int gc1084_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int wpclk = 0;
	unsigned short vts = 0;
	unsigned short hts = 0;
	unsigned char tmp;
	unsigned int sensor_max_fps;
	unsigned char sensor_min_fps;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	wpclk = GC1084_SUPPORT_30FPS_SCLK;
	sensor_max_fps = TX_SENSOR_MAX_FPS_30;
	sensor_min_fps = TX_SENSOR_MAX_FPS_5;

	/* the format of fps is 16/16. for example 30 << 16 | 2, the value is 30/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (sensor_max_fps << 8) || newformat < (sensor_min_fps<< 8)){
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}

    ret += gc1084_read(sd, 0xd05, &tmp);
	hts = tmp ;
	ret += gc1084_read(sd, 0xd06, &tmp);
	if(ret < 0)
		return -1;
	hts = (hts << 8) + tmp;

	vts = wpclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += gc1084_write(sd, 0x0d41, (unsigned char)((vts & 0xff00) >> 8));
	ret += gc1084_write(sd, 0x0d42, (unsigned char)(vts & 0xff));
	if(ret < 0)
		return -1;

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 16;
	sensor->video.attr->integration_time_limit = vts - 16;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 16;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;
}

static int gc1084_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = -1;
	unsigned char val = 0x0;
	unsigned char col_start = 0x0;

	ret = gc1084_read(sd, 0x0d15, &val);
	if (enable & 0x2) {
		val |= 0x02;
		col_start = 0x01;
	} else {
		val &= 0xfd;
		col_start = 0x00;
	}
	ret += gc1084_write(sd, 0x0d15, val);
	ret += gc1084_write(sd, 0x0192, col_start);

	return ret;
}

static int gc1084_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}
	return ret;
}

struct clk *sclka;
static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = 0;

	switch(info->default_boot){
	case 0:
		wsize = &gc1084_win_sizes[0];
		gc1084_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		memcpy(&gc1084_attr.mipi, &gc1084_mipi_linear, sizeof(gc1084_mipi_linear));
		gc1084_attr.one_line_expr_in_us = 22;
		gc1084_attr.total_width = 2200;
		gc1084_attr.total_height = 750;
		gc1084_attr.max_integration_time_native = 734;
		gc1084_attr.integration_time_limit = 734;
		gc1084_attr.max_integration_time = 734;
        gc1084_attr.integration_time = 720;
		break;
	default:
		ISP_ERROR("Have no this setting!!!\n");
	}

	switch(info->video_interface){
	case TISP_SENSOR_VI_MIPI_CSI0:
		gc1084_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc1084_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		gc1084_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc1084_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		gc1084_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this interface!!!\n");
	}

	switch(info->mclk){
	case TISP_SENSOR_MCLK0:
		sclka = private_devm_clk_get(&client->dev, "mux_cim0");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim0");
		set_sensor_mclk_function(0);
		break;
	case TISP_SENSOR_MCLK1:
		sclka = private_devm_clk_get(&client->dev, "mux_cim1");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim1");
		set_sensor_mclk_function(1);
		break;
	case TISP_SENSOR_MCLK2:
		sclka = private_devm_clk_get(&client->dev, "mux_cim2");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim2");
		set_sensor_mclk_function(2);
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	rate = private_clk_get_rate(sensor->mclk);
	if (((rate / 1000) % 27000) != 0) {
		ret = clk_set_parent(sclka, clk_get(NULL, "epll"));
		sclka = private_devm_clk_get(&client->dev, "epll");
		if (IS_ERR(sclka)) {
			pr_err("get sclka failed\n");
		} else {
			rate = private_clk_get_rate(sclka);
			if (((rate / 1000) % 27000) != 0) {
				private_clk_set_rate(sclka, 891000000);
			}
		}
	}

	private_clk_set_rate(sensor->mclk, 27000000);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int gc1084_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"gc1084_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"gc1084_pwdn");
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
	ret = gc1084_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc1084 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("gc1084 chip found @ 0x%02x (%s)\n sensor drv version %s", client->addr, client->adapter->name, SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "gc1084", sizeof("gc1084"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int gc1084_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = gc1084_set_expo(sd,sensor_val->value);
		break;

#if 0
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = gc1084_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = gc1084_set_analog_gain(sd, sensor_val->value);
		break;
#endif
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = gc1084_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = gc1084_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = gc1084_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = gc1084_write_array(sd, gc1084_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = gc1084_write_array(sd, gc1084_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = gc1084_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = gc1084_set_vflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int gc1084_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = gc1084_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int gc1084_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc1084_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops gc1084_core_ops = {
	.g_chip_ident = gc1084_g_chip_ident,
	.reset = gc1084_reset,
	.init = gc1084_init,
	/*.ioctl = gc1084_ops_ioctl,*/
	.g_register = gc1084_g_register,
	.s_register = gc1084_s_register,
};

static struct tx_isp_subdev_video_ops gc1084_video_ops = {
	.s_stream = gc1084_s_stream,
};

static struct tx_isp_subdev_sensor_ops	gc1084_sensor_ops = {
	.ioctl	= gc1084_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops gc1084_ops = {
	.core = &gc1084_core_ops,
	.video = &gc1084_video_ops,
	.sensor = &gc1084_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "gc1084",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc1084_probe(struct i2c_client *client, const struct i2c_device_id *id)
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
	sensor->dev = &client->dev;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	//gc1084_attr.expo_fs = 0;
	sensor->video.attr = &gc1084_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc1084_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->gc1084\n");

	return 0;
}

static int gc1084_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id gc1084_id[] = {
	{ "gc1084", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc1084_id);

static struct i2c_driver gc1084_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "gc1084",
	},
	.probe		= gc1084_probe,
	.remove		= gc1084_remove,
	.id_table	= gc1084_id,
};

static __init int init_gc1084(void)
{
	return private_i2c_add_driver(&gc1084_driver);
}

static __exit void exit_gc1084(void)
{
	private_i2c_del_driver(&gc1084_driver);
}

module_init(init_gc1084);
module_exit(exit_gc1084);

MODULE_DESCRIPTION("A low-level driver for gc1084 sensors");
MODULE_LICENSE("GPL");
