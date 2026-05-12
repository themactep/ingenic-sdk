/*
 * jxf35.c
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

#define jxf35_CHIP_ID_H	(0x0f)
#define jxf35_CHIP_ID_L	(0x35)
#define jxf35_REG_END		0xff
#define jxf35_REG_DELAY		0xfe
#define jxf35_SUPPORT_30FPS_SCLK_MIPI (86400000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20240829a"

/* 1080p@30fps: insmod sensor_jxf35_t31.ko data_interface=1 sensor_max_fps=30 sensor_resolution=200 */

static int reset_gpio = GPIO_PC(28);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

// static int sensor_gpio_func = DVP_PA_LOW_10BIT;
// module_param(sensor_gpio_func, int, S_IRUGO);
// MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
module_param(data_type, int, S_IRUGO);
MODULE_PARM_DESC(data_type, "Sensor Date Type");


static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static unsigned char reg_2f = 0x44;
static unsigned char reg_0c = 0x00;
static unsigned char reg_80 = 0x06;

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

struct again_lut jxf35_again_lut[] = {
	{0x0, 0 },
	{0x1, 5731 },
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
	{0x3f, 259142},
};

struct tx_isp_sensor_attribute jxf35_attr;

unsigned int jxf35_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;

	if(data_type == TX_SENSOR_DATA_TYPE_WDR_DOL){
		if (expo % 2 == 0)
			expo = expo - 1;
		if(expo < jxf35_attr.min_integration_time)
			expo = 3;
	}
	isp_it = expo << shift;
	*sensor_it = expo;

	return isp_it;
}

unsigned int jxf35_alloc_integration_time_short(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;

	if (expo % 2 == 0)
		expo = expo - 1;
	if(expo < jxf35_attr.min_integration_time_short)
		expo = 3;
	isp_it = expo << shift;
	expo = (expo - 1) / 2;
	if(expo < 0)
		expo = 0;
	*sensor_it = expo;

	return isp_it;
}

unsigned int jxf35_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = jxf35_again_lut;
	while(lut->gain <= jxf35_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == jxf35_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

// unsigned int jxf35_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
// {
// 	struct again_lut *lut = jxf35_again_lut;
// 	while(lut->gain <= jxf35_attr.max_again_short) {
// 		if(isp_gain == 0) {
// 			*sensor_again = 0;
// 			return 0;
// 		}
// 		else if(isp_gain < lut->gain) {
// 			*sensor_again = (lut - 1)->value;
// 			return (lut - 1)->gain;
// 		}
// 		else{
// 			if((lut->gain == jxf35_attr.max_again_short) && (isp_gain >= lut->gain)) {
// 				*sensor_again = lut->value;
// 				return lut->gain;
// 			}
// 		}

// 		lut++;
// 	}

// 	return isp_gain;
// }

unsigned int jxf35_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus jxf35_mipi={
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
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute jxf35_attr={
	.name = "jxf35",
	.chip_id = 0xf35,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x40,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 259142,
	.max_again_short = 0,
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
	.dgain_apply_delay = 0,
	.one_line_expr_in_us = 30,
	.sensor_ctrl.alloc_again = jxf35_alloc_again,
	.sensor_ctrl.alloc_dgain = jxf35_alloc_dgain,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list jxf35_init_regs_1920_1080_30fps_mipi[] = {
	{0x12,0x40},
	{0x48,0x8A},
	{0x48,0x0A},
	{0x0E,0x11},
	{0x0F,0x14},
	{0x10,0x24},
	{0x11,0x80},
	{0x0D,0xF0},
	{0x5F,0x41},
	{0x60,0x20},
	{0x58,0x18},
	{0x57,0x60},
	{0x64,0xE0},
	{0x20,0x00},
	{0x21,0x05},
	{0x22,0x65},
	{0x23,0x04},
	{0x24,0xC0},
	{0x25,0x38},
	{0x26,0x43},
	{0x27,0x0F},
	{0x28,0x15},
	{0x29,0x02},
	{0x2A,0x00},
	{0x2B,0x12},
	{0x2C,0x01},
	{0x2D,0x00},
	{0x2E,0x14},
	{0x2F,0x44},
	{0x41,0xC8},
	{0x42,0x13},
	{0x76,0x60},
	{0x77,0x09},
	{0x80,0x06},
	{0x1D,0x00},
	{0x1E,0x04},
	{0x6C,0x40},
	{0x68,0x00},
	{0x70,0x6D},
	{0x71,0x6D},
	{0x72,0x6A},
	{0x73,0x36},
	{0x74,0x02},
	{0x78,0x9E},
	{0x89,0x81},
	{0x6E,0x2C},
	{0x32,0x4F},
	{0x33,0x58},
	{0x34,0x5F},
	{0x35,0x5F},
	{0x3A,0xAF},
	{0x3B,0x00},
	{0x3C,0x70},
	{0x3D,0x8F},
	{0x3E,0xFF},
	{0x3F,0x85},
	{0x40,0xFF},
	{0x56,0x32},
	{0x59,0x67},
	{0x85,0x3C},
	{0x8A,0x04},
	{0x91,0x10},
	{0x9C,0xE1},
	{0x5A,0x09},
	{0x5C,0x4C},
	{0x5D,0xF4},
	{0x5E,0x1E},
	{0x62,0x04},
	{0x63,0x0F},
	{0x66,0x04},
	{0x67,0x30},
	{0x6A,0x12},
	{0x7A,0xA0},
	{0x9D,0x10},
	{0x4A,0x05},
	{0x7E,0xCD},
	{0x50,0x02},
	{0x49,0x10},
	{0x47,0x02},
	{0x7B,0x4A},
	{0x7C,0x0C},
	{0x7F,0x57},
	{0x8F,0x80},
	{0x90,0x00},
	{0x8C,0xFF},
	{0x8D,0xC7},
	{0x8E,0x00},
	{0x8B,0x01},
	{0x0C,0x00},
	{0x69,0x74},
	{0x65,0x02},
	{0x81,0x74},
	{0x19,0x20},
	{0x12,0x00},
	{jxf35_REG_END, 0x00},	/* END MARKER */
};


/*
 * the order of the jxf35_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting jxf35_win_sizes[] = {
	/* [0] */
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 30 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= jxf35_init_regs_1920_1080_30fps_mipi,
	},
};
static struct tx_isp_sensor_win_setting *wsize = &jxf35_win_sizes[0];

/*
 * the part of driver was fixed.
 */
static struct regval_list jxf35_stream_on_mipi[] = {
	{0x12, 0x00},
	{jxf35_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxf35_stream_off_mipi[] = {
	{0x12, 0x40},
	{jxf35_REG_END, 0x00},	/* END MARKER */
};

int jxf35_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int jxf35_write(struct tx_isp_subdev *sd, unsigned char reg,
		unsigned char value)
{
	int ret;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 2,
		.buf	= buf,
	};
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 0
static int jxf35_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;

	while (vals->reg_num != jxf35_REG_END) {
		if (vals->reg_num == jxf35_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxf35_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int jxf35_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	// unsigned char val;
	while (vals->reg_num != jxf35_REG_END) {
		if (vals->reg_num == jxf35_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxf35_write(sd, vals->reg_num, vals->value);
			// ret+= jxf35_read(sd, vals->reg_num, &val);
			// printk("{0x%2x, 0x%2x},\n", vals->reg_num, val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int jxf35_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int jxf35_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = jxf35_read(sd, 0x0a, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != jxf35_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = jxf35_read(sd, 0x0b, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;

	if (v != jxf35_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int it_last = -1;
static int ag_last = -1;
#if 0
static int jxf35_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = jxf35_write(sd,  0x01, (unsigned char)(value & 0xff));
	ret += jxf35_write(sd, 0x02, (unsigned char)((value >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}


static int jxf35_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = jxf35_write(sd, 0x05, (unsigned char)(value & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}


static int jxf35_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	unsigned char tmp1;
	unsigned char tmp2;
	unsigned char tmp3;
	int ret = 0;

	if(value < 0x10){
		tmp1 = reg_2f | 0x20;
		tmp2 = reg_0c | 0x40;
		tmp3 = reg_82 | 0x02;
	}else{
		tmp1 = reg_2f & 0xdf;
		tmp2 = reg_0c & 0xbf;
		tmp3 = reg_82 & 0xfd;
	}

	ret += jxf35_write(sd, 0x00, (unsigned char)(value & 0x7f));

	/*black sun cancellation strategy*/
	if((((ag_last < 0x10) && (value >= 0x10)) || ((ag_last >= 0x10) && (value < 0x10))) || (ag_last == -1)){
		ret = jxf35_write(sd, 0x2f, tmp1);
		ret += jxf35_write(sd, 0x0c, tmp2);
		ret += jxf35_write(sd, 0x82, tmp3);
	}
	ag_last = value;
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int jxf35_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int expo = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	/*expo*/
	if(it_last != expo){
		ret = jxf35_write(sd,  0x01, (unsigned char)(expo & 0xff));
		ret += jxf35_write(sd, 0x02, (unsigned char)((expo >> 8) & 0xff));
	}

	/*gain*/
	if(ag_last != again){

		ret += jxf35_write(sd, 0x00, (unsigned char)(again & 0x7f));

		/*black sun cancellation strategy*/
		if(value < 0x20) {
		ret += jxf35_write(sd, 0x2f, reg_2f | 0x20);
		ret += jxf35_write(sd, 0x0c, reg_0c | 0x40);
		ret += jxf35_write(sd, 0x80, reg_80 | 0x01);
		} else {
		ret += jxf35_write(sd, 0x2f, reg_2f & 0xdf);
		ret += jxf35_write(sd, 0x0c, reg_0c & 0xbf);
		ret += jxf35_write(sd, 0x80, reg_80 & 0xfe);
		}
	}

	it_last = expo;
	ag_last = again;
	if (ret < 0)
		return ret;


	return 0;
}

static int jxf35_set_logic(struct tx_isp_subdev *sd, int value)
{

	return 0;
}

static int jxf35_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxf35_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxf35_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxf35_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!init->enable){
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		return ISP_SUCCESS;
	} else {
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = TISP_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		sensor->video.state = TX_ISP_MODULE_DEINIT;

		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
		sensor->priv = wsize;
	}

	return 0;
}

static int jxf35_s_stream(struct tx_isp_subdev *sd,struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if(sensor->video.state == TX_ISP_MODULE_DEINIT){
			ret = jxf35_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if(sensor->video.state == TX_ISP_MODULE_INIT){
			ret = jxf35_write_array(sd, jxf35_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("jxf35 stream on\n");
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
	} else {
		ret = jxf35_write_array(sd, jxf35_stream_off_mipi);
		//sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("jxf35 stream off\n");
	}

	return ret;
}

static int jxf35_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = jxf35_SUPPORT_30FPS_SCLK_MIPI;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = SENSOR_OUTPUT_MAX_FPS;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}

	val = 0;
	ret += jxf35_read(sd, 0x21, &val);
	hts = val<<8;
	val = 0;
	ret += jxf35_read(sd, 0x20, &val);
	hts = (hts | val) << 1;
	if (0 != ret) {
		ISP_ERROR("err: jxf35 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	jxf35_write(sd, 0x22, (unsigned char)(vts & 0xff));
	jxf35_write(sd, 0x23, (unsigned char)(vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: jxf35_write err\n");
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

#if 0
static unsigned char val0,val1,val2;
static int jxf35_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
	int ret = 0;

	ret = jxf35_write(sd, 0x12, 0x80);
	private_msleep(5);

	ret = jxf35_write_array(sd, wsize->regs);
	ret = jxf35_write(sd, 0x00, val0);
	ret = jxf35_write(sd, 0x01, val1);
	ret = jxf35_write(sd, 0x02, val2);
	ret = jxf35_write_array(sd, jxf35_stream_on_mipi);
	ret = jxf35_write(sd, 0x00, 0x00);

	return 0;
}

static int jxf35_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = 0;

	ret = jxf35_read(sd, 0x00, &val0);
	ret = jxf35_read(sd, 0x01, &val1);
	ret = jxf35_read(sd, 0x02, &val2);

	ret = jxf35_write(sd, 0x12, 0x40);
	if(wdr_en == 1){
		memcpy((void*)(&(jxf35_attr.mipi)),(void*)(&jxf35_mipi_dol),sizeof(jxf35_mipi_dol));
		data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		wsize = &jxf35_win_sizes[2];
		jxf35_attr.data_type = data_type;
		jxf35_attr.wdr_cache = wdr_bufsize;
		jxf35_attr.one_line_expr_in_us = 28;

		jxf35_attr.wdr_cache = wdr_bufsize;
		jxf35_attr.max_integration_time_native = 1883;//0x960*2 - 0xff * 2 - 3
		jxf35_attr.integration_time_limit = 1883;
		jxf35_attr.total_width = 0x47e * 2;
		jxf35_attr.total_height = 0x960;
		jxf35_attr.max_integration_time = 1883;

		sensor->video.attr = &jxf35_attr;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else if (wdr_en == 0){
		memcpy((void*)(&(jxf35_attr.mipi)),(void*)(&jxf35_mipi),sizeof(jxf35_mipi));

		data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		jxf35_attr.data_type = data_type;
		wsize = &jxf35_win_sizes[1];

		jxf35_attr.one_line_expr_in_us = 30;
		jxf35_attr.data_type = data_type;
		jxf35_attr.max_integration_time_native = 1500 - 4;
		jxf35_attr.integration_time_limit = 1500 - 4;
		jxf35_attr.total_width = 3456;
		jxf35_attr.total_height = 1500;
		jxf35_attr.max_integration_time = 1500 - 4;

		sensor->video.attr = &jxf35_attr;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}
#endif
static int jxf35_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = TISP_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
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
		memcpy((void*)(&(jxf35_attr.mipi)),(void*)(&jxf35_mipi),sizeof(jxf35_mipi));
		wsize = &jxf35_win_sizes[0];
		jxf35_mipi.clk = 216;
		jxf35_attr.mipi.settle_time_apative_en = 0;
		jxf35_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		jxf35_attr.total_width = 2560;
		jxf35_attr.total_height = 1125;
		jxf35_attr.max_integration_time_native = 1125 - 4;
		jxf35_attr.max_integration_time = 1125 - 4;
		jxf35_attr.integration_time_limit = 1125 - 4;
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	switch(info->video_interface){
	case TISP_SENSOR_VI_MIPI_CSI0:
		jxf35_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		jxf35_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		jxf35_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		jxf35_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		jxf35_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
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
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
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

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

        sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

	return 0;

err_get_mclk:
	return -1;
}

static int jxf35_g_chip_ident(struct tx_isp_subdev *sd,
			      struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"jxf35_reset");
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
		ret = private_gpio_request(pwdn_gpio,"jxf35_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(150);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}

	ret = jxf35_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an jxf35 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("jxf35 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "jxf35", sizeof("jxf35"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int jxf35_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val = 0;

	/* 2'b01: mirror; 2'b10:flip*/
	enable &= 0x3;
	switch (enable)
	{
	case 0: /*normal*/
		val &= 0xCF;
		val |= 0x00;
		break;
	case 1: /*mirror*/
		val &= 0xCF;
		val |= 0x20; /*mirror*/
		break;
	case 2: /*filp*/
		val &= 0xCF;
		val |= 0x10;
		break;
	case 3: /*mirror & filp*/
		val &= 0xCF;
		val |= 0x30;
		break;
	default:
		break;
	}

	ret = jxf35_write(sd, 0x12, val);
	if (0 != ret)
	{
		ISP_ERROR("%s:%d, jxf35_write err!!\n", __func__, __LINE__);
		return ret;
	}

	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int jxf35_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if(arg)
			ret = jxf35_set_logic(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = jxf35_set_expo(sd, sensor_val->value);
		break;
		/* case TX_ISP_EVENT_SENSOR_INT_TIME: */
		/* 	if(arg) */
		/* 		ret = jxf35_set_integration_time(sd, sensor_val->value); */
		/* 	break; */
	// case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
	// 	if(arg)
	// 		ret = jxf35_set_integration_time_short(sd, sensor_val->value);
	// 	break;
		/* case TX_ISP_EVENT_SENSOR_AGAIN: */
		/* 	if(arg) */
		/* 		ret = jxf35_set_analog_gain(sd, sensor_val->value); */
		/* 	break; */
	// case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		// if(arg)
		// 	ret = jxf35_set_analog_gain_short(sd, sensor_val->value);
		// break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = jxf35_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = jxf35_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = jxf35_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg){
			ret = jxf35_write_array(sd, jxf35_stream_off_mipi);
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg){
			ret = jxf35_write_array(sd, jxf35_stream_on_mipi);

		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = jxf35_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = jxf35_set_vflip(sd, sensor_val->value);
		break;
	// case TX_ISP_EVENT_SENSOR_WDR:
	// 	ret = jxf35_set_wdr(sd, init->enable);
	// 	break;
	// case TX_ISP_EVENT_SENSOR_WDR_STOP:
	// 	ret = jxf35_set_wdr_stop(sd, init->enable);
	// 	break;
	default:
		break;
	}

	return ret;
}

static int jxf35_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = jxf35_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int jxf35_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	jxf35_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}

static struct tx_isp_subdev_core_ops jxf35_core_ops = {
	.g_chip_ident = jxf35_g_chip_ident,
	.reset = jxf35_reset,
	.init = jxf35_init,
	/*.ioctl = jxf35_ops_ioctl,*/
	.g_register = jxf35_g_register,
	.s_register = jxf35_s_register,
};

static struct tx_isp_subdev_video_ops jxf35_video_ops = {
	.s_stream = jxf35_s_stream,
};

static struct tx_isp_subdev_sensor_ops	jxf35_sensor_ops = {
	.ioctl	= jxf35_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops jxf35_ops = {
	.core = &jxf35_core_ops,
	.video = &jxf35_video_ops,
	.sensor = &jxf35_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "jxf35",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int jxf35_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

	jxf35_attr.expo_fs = 1;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->dev = &client->dev;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &jxf35_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &jxf35_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->jxf35\n");

	return 0;
}

static int jxf35_remove(struct i2c_client *client)
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

static const struct i2c_device_id jxf35_id[] = {
	{ "jxf35", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, jxf35_id);

static struct i2c_driver jxf35_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "jxf35",
	},
	.probe		= jxf35_probe,
	.remove		= jxf35_remove,
	.id_table	= jxf35_id,
};

static __init int init_jxf35(void)
{
	return private_i2c_add_driver(&jxf35_driver);
}

static __exit void exit_jxf35(void)
{
	private_i2c_del_driver(&jxf35_driver);
}

module_init(init_jxf35);
module_exit(exit_jxf35);

MODULE_DESCRIPTION("A low-level driver for Sonic jxf35 sensors");
MODULE_LICENSE("GPL");
