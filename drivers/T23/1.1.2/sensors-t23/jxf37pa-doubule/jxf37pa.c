/*
 * jxf37pa.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @fsync Sync hardware connection: Primary Sensor vsync is directly connected to secondary Sensor vsync.
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

#define JXF37PA_CHIP_ID_H	(0x08)
#define JXF37PA_CHIP_ID_L	(0x41)
#define JXF37PA_REG_END		0xff
#define JXF37PA_REG_DELAY	0xfe
#define JXF37PA_SUPPORT_15FPS_SCLK (86400000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20230921a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static int fsync_mode = TX_ISP_SENSOR_FSYNC_MODE_MS_REALTIME_MISPLACE;
module_param(fsync_mode, int, S_IRUGO);
MODULE_PARM_DESC(fsync_mode, "Sensor Indicates the frame synchronization mode");

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

struct again_lut jxf37pa_again_lut[] = {
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

struct tx_isp_sensor_attribute jxf37pa_attr;

unsigned int jxf37pa_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = jxf37pa_again_lut;
	while(lut->gain <= jxf37pa_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == jxf37pa_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int jxf37pa_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus jxf37pa_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 432,
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


struct tx_isp_sensor_attribute jxf37pa_attr={
	.name = "jxf37pa",
	.chip_id = 0x841,
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
		.dvp_hcomp_en = 0,
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 259142,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 0xa8c - 4,
	.integration_time_limit = 0xa8c - 4,
	.total_width = 1920,
	.total_height = 0xa8c,
	.max_integration_time = 0xa8c - 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = jxf37pa_alloc_again,
	.sensor_ctrl.alloc_dgain = jxf37pa_alloc_dgain,
	.one_line_expr_in_us = 28,
        .fsync_attr = {
                .mode = TX_ISP_SENSOR_FSYNC_MODE_MS_REALTIME_MISPLACE,
                .call_times = 1,
                .sdelay = 0,
        }
};

//12.5
static struct regval_list jxf37pa_init_regs_1920_1080_12fps_mipi_sync3[] = {
        {0x12, 0x40},
        {0x48, 0x8A},
        {0x48, 0x0A},
        {0x0E, 0x19},
        {0x0F, 0x04},
        {0x10, 0x24},
        {0x11, 0x80},
        {0x46, 0x09},
        {0x47, 0x66},
        {0x0D, 0xF2},
        {0x57, 0x6A},
        {0x58, 0x22},
        {0x5F, 0x41},
        {0x60, 0x28},
        {0xA5, 0xC0},
        {0x20, 0x00},
        {0x21, 0x05},
        {0x22, 0x8C},	//;65
        {0x23, 0x0A},	//;04
        {0x24, 0xC0},
        {0x25, 0x38},
        {0x26, 0x43},
        {0x27, 0xC6},
        {0x28, 0x15},
        {0x29, 0x04},
        {0x2A, 0xBB},
        {0x2B, 0x14},
        {0x2C, 0x02},
        {0x2D, 0x00},
        {0x2E, 0x14},
        {0x2F, 0x04},
        {0x41, 0xC5},
        {0x42, 0x33},
        {0x47, 0x46},
        {0x76, 0x60},
        {0x77, 0x09},
        {0x80, 0x01},
        {0xAF, 0x22},
        {0xAB, 0x00},
        {0x1D, 0x00},
        {0x1E, 0x04},
        {0x6C, 0x40},
        {0x9E, 0xF8},
        {0x6E, 0x2C},
        {0x70, 0x6C},
        {0x71, 0x6D},
        {0x72, 0x6A},
        {0x73, 0x56},
        {0x74, 0x02},
        {0x78, 0x9D},
        {0x89, 0x01},
        {0x6B, 0x20},
        {0x86, 0x40},
        {0x31, 0x10},
        {0x32, 0x18},
        {0x33, 0xE8},
        {0x34, 0x5E},
        {0x35, 0x5E},
        {0x3A, 0xAF},
        {0x3B, 0x00},
        {0x3C, 0xFF},
        {0x3D, 0xFF},
        {0x3E, 0xFF},
        {0x3F, 0xBB},
        {0x40, 0xFF},
        {0x56, 0x92},
        {0x59, 0xAF},
        {0x5A, 0x47},
        {0x61, 0x18},
        {0x6F, 0x04},
        {0x85, 0x5F},
        {0x8A, 0x44},
        {0x91, 0x13},
        /* {0x94, 0xA0}, */
        {0x94, 0xe0},
        {0x9B, 0x83},
        {0x9C, 0xE1},
        {0xA4, 0x80},
        {0xA6, 0x22},
        {0xA9, 0x1C},
        {0x5B, 0xE7},
        {0x5C, 0x28},
        {0x5D, 0x67},
        {0x5E, 0x11},
        {0x62, 0x21},
        {0x63, 0x0F},
        {0x64, 0xD0},
        {0x65, 0x02},
        {0x67, 0x49},
        {0x66, 0x00},
        {0x68, 0x00},
        {0x69, 0x72},
        {0x6A, 0x12},
        {0x7A, 0x00},
        {0x82, 0x20},
        {0x8D, 0x47},
        {0x8F, 0x90},
        {0x45, 0x01},
        {0x97, 0x20},
        {0x13, 0x81},
        {0x96, 0x84},
        {0x4A, 0x01},
        {0xB1, 0x00},
        {0xA1, 0x0F},
        {0xBE, 0x00},
        {0x7E, 0x48},
        {0xB5, 0xC0},
        {0x50, 0x02},
        {0x49, 0x10},
        {0x7F, 0x57},
        {0x90, 0x00},
        {0x7B, 0x4A},
        {0x7C, 0x0C},
        {0x8C, 0xFF},
        {0x8E, 0x00},
        {0x8B, 0x01},
        {0x0C, 0x00},
        {0xBC, 0x11},
        {0x19, 0x20},
        {0x1B, 0x4F},
        {0x12, 0x00},
        //0x;
        {0x46, 0x01},
        {0x47, 0x42},
        {0x1E, 0x0C},
        {0x00, 0x10},
        {JXF37PA_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the jxf37pa_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting jxf37pa_win_sizes[] = {
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 2,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= jxf37pa_init_regs_1920_1080_12fps_mipi_sync3,
	},
};
struct tx_isp_sensor_win_setting *wsize = &jxf37pa_win_sizes[0];


/*
 * the part of driver was fixed.
 */

static struct regval_list jxf37pa_stream_on_dvp[] = {
	{JXF37PA_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxf37pa_stream_off_dvp[] = {
	{JXF37PA_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxf37pa_stream_on_mipi[] = {

	{JXF37PA_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxf37pa_stream_off_mipi[] = {
	{JXF37PA_REG_END, 0x00},	/* END MARKER */
};

int jxf37pa_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int jxf37pa_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int jxf37pa_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != JXF37PA_REG_END) {
		if (vals->reg_num == JXF37PA_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxf37pa_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif
static int jxf37pa_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != JXF37PA_REG_END) {
		if (vals->reg_num == JXF37PA_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxf37pa_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int jxf37pa_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int jxf37pa_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = jxf37pa_read(sd, 0x0a, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != JXF37PA_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = jxf37pa_read(sd, 0x0b, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;

	if (v != JXF37PA_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int jxf37pa_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;
	ret = jxf37pa_write(sd,  0x01, (unsigned char)(expo & 0xff));
	ret += jxf37pa_write(sd, 0x02, (unsigned char)((expo >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int jxf37pa_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret = jxf37pa_write(sd, 0x00, (unsigned char)(value & 0x7f));
	if (ret < 0)
		return ret;

	return 0;
}

static int jxf37pa_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxf37pa_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxf37pa_init(struct tx_isp_subdev *sd, int enable)
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

	ret = jxf37pa_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int jxf37pa_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = jxf37pa_write_array(sd, jxf37pa_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = jxf37pa_write_array(sd, jxf37pa_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("jxf37pa stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = jxf37pa_write_array(sd, jxf37pa_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = jxf37pa_write_array(sd, jxf37pa_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("jxf37pa stream off\n");
	}

	return ret;
}

static int jxf37pa_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 13;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}

	val = 0;
	ret += jxf37pa_read(sd, 0x21, &val);
	hts = val<<8;
	val = 0;
	ret += jxf37pa_read(sd, 0x20, &val);
	hts |= val;
	hts *= 2;
	if (0 != ret) {
		ISP_ERROR("err: jxf37pa read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += jxf37pa_write(sd, 0x22, (unsigned char)(vts & 0xff));
	ret += jxf37pa_write(sd, 0x23, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: jxf37pa_write err\n");
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

static int jxf37pa_set_hvflip(struct tx_isp_subdev *sd, int enable)
{
        //struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val;
	//int ret = 0;
	//unsigned char val = 0;

	ret += jxf37pa_read(sd, 0x12, &val);

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
	ret += jxf37pa_write(sd, 0x12, val);

	return ret;
}

static int jxf37pa_set_mode(struct tx_isp_subdev *sd, int value)
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

static int jxf37pa_g_chip_ident(struct tx_isp_subdev *sd,
			      struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"jxf37pa_reset");
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
		ret = private_gpio_request(pwdn_gpio,"jxf37pa_pwdn");
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
	ret = jxf37pa_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an jxf37pa chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("jxf37pa chip found @ 0x%02x (%s) version %s\n", client->addr, client->adapter->name, SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "jxf37pa", sizeof("jxf37pa"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int jxf37pa_fsync(struct tx_isp_subdev *sd, struct tx_isp_sensor_fsync *fsync)
{

        if (fsync->place != TX_ISP_SENSOR_FSYNC_PLACE_STREAMON_AFTER)
                return 0;
        switch (fsync->call_index) {
        case 0:
                switch (fsync_mode) {
                case 2:

                        break;
                case 3:
                        break;
                }
                break;
        }

        return 0;
}

static int jxf37pa_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
#if 0
		if(arg)
			ret = jxf37pa_set_expo(sd, *(int*)arg);
#endif
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = jxf37pa_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = jxf37pa_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = jxf37pa_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = jxf37pa_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = jxf37pa_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = jxf37pa_write_array(sd, jxf37pa_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = jxf37pa_write_array(sd, jxf37pa_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = jxf37pa_write_array(sd, jxf37pa_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = jxf37pa_write_array(sd, jxf37pa_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = jxf37pa_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = jxf37pa_set_hvflip(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int jxf37pa_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = jxf37pa_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int jxf37pa_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	jxf37pa_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops jxf37pa_core_ops = {
	.g_chip_ident = jxf37pa_g_chip_ident,
	.reset = jxf37pa_reset,
	.init = jxf37pa_init,
	/*.ioctl = jxf37pa_ops_ioctl,*/
	.g_register = jxf37pa_g_register,
	.s_register = jxf37pa_s_register,
};

static struct tx_isp_subdev_video_ops jxf37pa_video_ops = {
	.s_stream = jxf37pa_s_stream,
};

static struct tx_isp_subdev_sensor_ops	jxf37pa_sensor_ops = {
        .fsync = jxf37pa_fsync,
	.ioctl	= jxf37pa_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops jxf37pa_ops = {
	.core = &jxf37pa_core_ops,
	.video = &jxf37pa_video_ops,
	.sensor = &jxf37pa_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "jxf37pa",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int jxf37pa_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

        sensor->mclk = clk_get(NULL, "cgu_cim");
        if (IS_ERR(sensor->mclk)) {
                ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
                goto err_get_mclk;
        }
        {
                unsigned int arate = 0,mrate = 0;
                unsigned int want_rate = 0;
                struct clk *clka = NULL;
                struct clk *clkm = NULL;

                want_rate=24000000;
                clka = clk_get(NULL, "sclka");
                clkm = clk_get(NULL, "mpll");
                arate = clk_get_rate(clka);
                mrate = clk_get_rate(clkm);
                if((arate%want_rate) && (mrate%want_rate)) {
                        if(want_rate == 37125000){
                                if(arate >= 1400000000) {
                                        arate = 1485000000;
                                } else if((arate >= 1100) || (arate < 1400)) {
                                        arate = 1188000000;
                                } else if(arate <= 1100) {
                                        arate = 891000000;
                                }
                        } else {
                                mrate = arate%want_rate;
                                arate = arate-mrate;
                        }
                        clk_set_rate(clka, arate);
                        clk_set_parent(sensor->mclk, clka);
                } else if(!(arate%want_rate)) {
                        clk_set_parent(sensor->mclk, clka);
                } else if(!(mrate%want_rate)) {
                        clk_set_parent(sensor->mclk, clkm);
                }
                private_clk_set_rate(sensor->mclk, want_rate);
                private_clk_enable(sensor->mclk);
        }

        jxf37pa_attr.dbus_type = data_interface;
        if(fsync_mode == TX_ISP_SENSOR_FSYNC_MODE_MS_REALTIME_MISPLACE){
                wsize = &jxf37pa_win_sizes[0];
                memcpy((void*)(&(jxf37pa_attr.mipi)),(void*)(&jxf37pa_mipi),sizeof(jxf37pa_mipi));
        } else {
                ISP_ERROR("Can not support this data interface and fps!!!\n");
                goto err_set_sensor_data_interface;
        }
        jxf37pa_attr.fsync_attr.mode = fsync_mode;

        jxf37pa_attr.expo_fs = 0;
        sd = &sensor->sd;
        video = &sensor->video;
        sensor->video.shvflip = shvflip;
        sensor->video.attr = &jxf37pa_attr;
        sensor->video.vi_max_width = wsize->width;
        sensor->video.vi_max_height = wsize->height;
        sensor->video.mbus.width = wsize->width;
        sensor->video.mbus.height = wsize->height;
        sensor->video.mbus.code = wsize->mbus_code;
        sensor->video.mbus.field = V4L2_FIELD_NONE;
        sensor->video.mbus.colorspace = wsize->colorspace;
        sensor->video.fps = wsize->fps;
        tx_isp_subdev_init(&sensor_platform_device, sd, &jxf37pa_ops);
        tx_isp_set_subdevdata(sd, client);
        tx_isp_set_subdev_hostdata(sd, sensor);
        private_i2c_set_clientdata(client, sd);

        pr_debug("probe ok ------->jxf37pa\n");
        return 0;
err_set_sensor_data_interface:
        private_clk_disable(sensor->mclk);
        private_clk_put(sensor->mclk);
err_get_mclk:
        kfree(sensor);

        return -1;
}

static int jxf37pa_remove(struct i2c_client *client)
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

static const struct i2c_device_id jxf37pa_id[] = {
	{ "jxf37pa", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, jxf37pa_id);

static struct i2c_driver jxf37pa_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "jxf37pa",
	},
	.probe		= jxf37pa_probe,
	.remove		= jxf37pa_remove,
	.id_table	= jxf37pa_id,
};

static __init int init_jxf37pa(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init jxf37pa dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&jxf37pa_driver);
}

static __exit void exit_jxf37pa(void)
{
	private_i2c_del_driver(&jxf37pa_driver);
}

module_init(init_jxf37pa);
module_exit(exit_jxf37pa);

MODULE_DESCRIPTION("A low-level driver for galaxycore jxf37pa sensors");
MODULE_LICENSE("GPL");
