/*
 * gc2093.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/* 1920*1080  carrier-server  --st=gc2093  data_interface=1  i2c=0x37  */
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

#define GC2093_CHIP_ID_H	(0x20)
#define GC2093_CHIP_ID_L	(0x93)
#define GC2093_REG_END	0xffff
#define GC2093_REG_DELAY   0x0000
#define GC2093_SUPPORT_30FPS_SCLK (74925000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20200816a"

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
	unsigned int index;
	unsigned char regb0;
	unsigned char regb1;
	unsigned char regb2;
	unsigned char regb3;
	unsigned char regb4;
	unsigned int gain;
};

struct again_lut gc2093_again_lut[] = {
	{0x0, 0x00, 0x00, 0x01, 0x00, 0x04, 0},
	{0x1, 0x00, 0x10, 0x01, 0x0c, 0x04, 13706},
	{0x2, 0x00, 0x20, 0x01, 0x1b, 0x04, 31134},
	{0x3, 0x00, 0x30, 0x01, 0x2c, 0x04, 45903},
	{0x4, 0x00, 0x40, 0x01, 0x3f, 0x2c, 64010},
	{0x5, 0x00, 0x50, 0x02, 0x16, 0x2c, 77964},
	{0x6, 0x00, 0x60, 0x02, 0x35, 0x2c, 97246},
	{0x7, 0x00, 0x70, 0x03, 0x16, 0x2c, 111902},
	{0x8, 0x00, 0x80, 0x04, 0x02, 0x28, 130334},
	{0x9, 0x00, 0x90, 0x04, 0x31, 0x28, 144468},
	{0xa, 0x00, 0xa0, 0x05, 0x32, 0x28, 157623},
	{0xb, 0x00, 0xb0, 0x06, 0x35, 0x28, 171832},
	{0xc, 0x00, 0xc0, 0x08, 0x04, 0x24, 190301},
	{0xd, 0x00, 0x5a, 0x09, 0x19, 0x24, 206077},
	{0xe, 0x00, 0x83, 0x0b, 0x0f, 0x24, 223709},
	{0xf, 0x00, 0x93, 0x0d, 0x12, 0x24, 237972},
	{0x10,0x00, 0x84, 0x10, 0x00, 0x20, 256333},
	{0x11,0x00, 0x94, 0x12, 0x3a, 0x20, 270526},
	{0x12,0x01, 0x2c, 0x1a, 0x02, 0x20, 292180},
	{0x13,0x01, 0x3c, 0x1b, 0x20, 0x20, 306322},
	{0x14,0x00, 0x8c, 0x20, 0x0f, 0x14, 321915},
	{0x16,0x00, 0x9c, 0x26, 0x07, 0x14, 336104},
	{0x17,0x02, 0x64, 0x36, 0x21, 0x14, 350927},
	{0x18,0x02, 0x74, 0x37, 0x3a, 0x14, 362691},
	{0x19,0x00, 0xc6, 0x3d, 0x02, 0x08, 376653},
	{0x1a,0x00, 0xdc, 0x3f, 0x3f, 0x08, 389634},
	{0x1b,0x02, 0x85, 0x3f, 0x3f, 0x08, 408756},
	{0x1c,0x02, 0x95, 0x3f, 0x3f, 0x04, 424087},
	{0x1e,0x00, 0xce, 0x3f, 0x3f, 0x04, 442174},
};

struct tx_isp_sensor_attribute gc2093_attr;

unsigned int gc2093_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = gc2093_again_lut;
	while(lut->gain <= gc2093_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return lut[0].gain;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == gc2093_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
				return lut->gain;
			}
		}

		lut++;
	}

	return 0;
}

unsigned int gc2093_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute gc2093_attr={
	.name = "gc2093",
	.chip_id = 0x2093,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x37,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 600,
		.lans = 2,
		.settle_time_apative_en = 1,
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
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 442174,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 1350 - 4,
	.integration_time_limit = 1350 - 4,
	.total_width = 2220,
	.total_height = 1125,
	.max_integration_time = 1350 - 4,
	.integration_time_apply_delay = 4,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = gc2093_alloc_again,
	.sensor_ctrl.alloc_dgain = gc2093_alloc_dgain,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list gc2093_init_regs_1920_1080_30fps_mipi[] = {
	{0x03fe, 0x80},/***system****/
	{0x03fe, 0x80},
	{0x03fe, 0x80},
	{0x03fe, 0x00},
	{0x03f2, 0x00},
	{0x03f3, 0x00},
	{0x03f4, 0x36},
	{0x03f5, 0xc0},
	{0x03f6, 0x0A},
	{0x03f7, 0x01},
	{0x03f8, 0x32},
	{0x03f9, 0x10},
	{0x03fc, 0x8e},
	{0x0087, 0x18},
	{0x00ee, 0x30},
	{0x00d0, 0xb7},
	{0x01a0, 0x00},
	{0x01a4, 0x40},
	{0x01a5, 0x40},
	{0x01a6, 0x40},
	{0x01af, 0x09},
	{0x0001, 0x00},
	{0x0002, 0x02},
	{0x0003, 0x00},
	{0x0004, 0x02},
	{0x0005, 0x04},
	{0x0006, 0x56},
	{0x0007, 0x00},
	{0x0008, 0x11},
	{0x0009, 0x00},
	{0x000a, 0x02},
	{0x000b, 0x00},
	{0x000c, 0x04},
	{0x000d, 0x04},
	{0x000e, 0x40},
	{0x000f, 0x07},
	{0x0010, 0x8c},
	{0x0013, 0x15},
	{0x0019, 0x0c},
	{0x0041, 0x05},
	{0x0042, 0x46},
	{0x0053, 0x60},
	{0x008d, 0x92},
	{0x0090, 0x00},
	{0x00c7, 0xe1},
	{0x001b, 0x73},
	{0x0028, 0x0d},
	{0x0029, 0x40},
	{0x002b, 0x04},
	{0x002e, 0x23},
	{0x0037, 0x03},
	{0x0038, 0x88},
	{0x0044, 0x20},
	{0x004b, 0x14},
	{0x0055, 0x20},
	{0x0068, 0x20},
	{0x0069, 0x20},
	{0x0077, 0x00},
	{0x0078, 0x04},
	{0x007c, 0x91},
	{0x00ce, 0x7c},
	{0x00d3, 0xdc},
	{0x00e6, 0x50},
	{0x00b6, 0xc0},// gain
	{0x00b0, 0x60},
	{0x0102, 0x89},
	{0x0104, 0x01},
	{0x0158, 0x00},
	{0x0026, 0x20},//blk 23 //[4]写0，全n mode  /* blk
	{0x0142, 0x00},
	{0x0149, 0x1e},
	{0x014a, 0x07},
	{0x014b, 0x80},
	{0x0155, 0x07},
	{0x0414, 0x7e},
	{0x0415, 0x7e},
	{0x0416, 0x7e},
	{0x0417, 0x7e},
	{0x0192, 0x02}, //00 //win y1
	{0x0194, 0x03},//win x1
	{0x0195, 0x04},
	{0x0196, 0x38}, //[10:0]out_height
	{0x0197, 0x07},
	{0x0198, 0x80}, //[11:0]out_width
	{0x019a, 0x06},
	{0x007b, 0x2a},
	{0x0023, 0x2d},
	{0x0201, 0x27},
	{0x0202, 0x56},
	{0x0203, 0xce},
	{0x0212, 0x80},
	{0x0213, 0x07},
	//0x0215, 0x12
	{0x003e, 0x91},

	{GC2093_REG_END, 0x00}, /* END MARKER */
};
/*
 * the order of the jxf23_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting gc2093_win_sizes[] = {
	/* 1920*1080 */
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= gc2093_init_regs_1920_1080_30fps_mipi,
	},
};

struct tx_isp_sensor_win_setting *wsize = &gc2093_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list gc2093_stream_on[] = {
	{GC2093_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc2093_stream_off[] = {
	{GC2093_REG_END, 0x00},	/* END MARKER */
};

int gc2093_read(struct tx_isp_subdev *sd,  uint16_t reg,
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

int gc2093_write(struct tx_isp_subdev *sd, uint16_t reg,
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

static int gc2093_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != GC2093_REG_END) {
		if (vals->reg_num == GC2093_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc2093_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}
	return 0;
}

static int gc2093_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != GC2093_REG_END) {
		if (vals->reg_num == GC2093_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc2093_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int gc2093_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int gc2093_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = gc2093_read(sd, 0x03f0, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != GC2093_CHIP_ID_H)
		return -ENODEV;
	ret = gc2093_read(sd, 0x03f1, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != GC2093_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int gc2093_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = gc2093_write(sd, 0x03, value & 0xff);
	ret += gc2093_write(sd, 0x04, value >> 8);
	if (ret < 0) {
		ISP_ERROR("gc2093_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int gc2093_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	struct again_lut *val_lut = gc2093_again_lut;

	ret += gc2093_write(sd,0xb4,val_lut[value].regb0);
	ret += gc2093_write(sd,0xb3,val_lut[value].regb1);
	ret += gc2093_write(sd,0xb8,val_lut[value].regb2);
	ret += gc2093_write(sd,0xb9,val_lut[value].regb3);
	ret += gc2093_write(sd,0x78,val_lut[value].regb4);

	if (ret < 0) {
		ISP_ERROR("gc2093_write error  %d" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int gc2093_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc2093_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc2093_init(struct tx_isp_subdev *sd, int enable)
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
	ret = gc2093_write_array(sd, wsize->regs);

	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int gc2093_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = gc2093_write_array(sd, gc2093_stream_on);
		pr_debug("gc2093 stream on\n");
	} else {
		ret = gc2093_write_array(sd, gc2093_stream_off);
		pr_debug("gc2093 stream off\n");
	}

	return ret;
}

static int gc2093_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int wpclk = 0;
	unsigned short vts = 0;
	unsigned short hts=0;
	unsigned int max_fps = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	wpclk = GC2093_SUPPORT_30FPS_SCLK;
	max_fps = SENSOR_OUTPUT_MAX_FPS;

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)){
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}
	ret = gc2093_write(sd, 0xfe, 0x0);
	ret += gc2093_read(sd, 0x05, &tmp);
	hts = tmp & 0x0f;
	ret += gc2093_read(sd, 0x06, &tmp);
	if(ret < 0)
		return -1;
	hts = ((hts << 8) + tmp) << 1;

	vts = wpclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = gc2093_write(sd, 0x41, (unsigned char)((vts & 0x3f00) >> 8));
	ret += gc2093_write(sd, 0x42, (unsigned char)(vts & 0xff));
	if(ret < 0)
		return -1;
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;
}

static int gc2093_set_mode(struct tx_isp_subdev *sd, int value)
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

static int gc2093_g_chip_ident(struct tx_isp_subdev *sd,struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"gc2093_reset");
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
		ret = private_gpio_request(pwdn_gpio,"gc2093_pwdn");
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
	ret = gc2093_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc2093 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("gc2093 chip found @ 0x%02x (%s)\n sensor drv version %s", client->addr, client->adapter->name, SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "gc2093", sizeof("gc2093"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int gc2093_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = gc2093_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = gc2093_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = gc2093_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = gc2093_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = gc2093_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = gc2093_write_array(sd, gc2093_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = gc2093_write_array(sd, gc2093_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = gc2093_set_fps(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int gc2093_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = gc2093_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int gc2093_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc2093_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops gc2093_core_ops = {
	.g_chip_ident = gc2093_g_chip_ident,
	.reset = gc2093_reset,
	.init = gc2093_init,
	/*.ioctl = gc2093_ops_ioctl,*/
	.g_register = gc2093_g_register,
	.s_register = gc2093_s_register,
};

static struct tx_isp_subdev_video_ops gc2093_video_ops = {
	.s_stream = gc2093_s_stream,
};

static struct tx_isp_subdev_sensor_ops	gc2093_sensor_ops = {
	.ioctl	= gc2093_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops gc2093_ops = {
	.core = &gc2093_core_ops,
	.video = &gc2093_video_ops,
	.sensor = &gc2093_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "gc2093",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc2093_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	/* unsigned long rate = 0; */
	/* int ret;  */
	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));

	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	/*rate = clk_get_rate(clk_get_parent(sensor->mclk)); */
	/* if (((rate / 1000) % 27000) != 0) {
	   struct clk *vpll;
	   vpll = clk_get(NULL,"vpll");
	   if (IS_ERR(vpll)) {
	   pr_err("get vpll failed\n");
	   } else {
	   rate = clk_get_rate(vpll);
	   if (((rate / 1000) % 27000) != 0) {
	   clk_set_rate(vpll,1080000027);
	   }
	   ret = clk_set_parent(sensor->mclk, vpll);
	   if (ret < 0)
	   pr_err("set mclk parent as epll err\n");
	   }
	   } */

	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	/*
	  convert sensor-gain into isp-gain,
	*/

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &gc2093_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc2093_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->gc2093\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int gc2093_remove(struct i2c_client *client)
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

static const struct i2c_device_id gc2093_id[] = {
	{ "gc2093", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc2093_id);

static struct i2c_driver gc2093_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "gc2093",
	},
	.probe		= gc2093_probe,
	.remove		= gc2093_remove,
	.id_table	= gc2093_id,
};

static __init int init_gc2093(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init gc2093 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&gc2093_driver);
}

static __exit void exit_gc2093(void)
{
	private_i2c_del_driver(&gc2093_driver);
}

module_init(init_gc2093);
module_exit(exit_gc2093);

MODULE_DESCRIPTION("A low-level driver for gc2093 sensors");
MODULE_LICENSE("GPL");
