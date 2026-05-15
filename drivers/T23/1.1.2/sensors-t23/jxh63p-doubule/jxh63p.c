/*
 * jxh63p.c
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

#define JXH63P_CHIP_ID_H	(0x08)
#define JXH63P_CHIP_ID_L	(0x48)
#define JXH63P_REG_END		0xff
#define JXH63P_REG_DELAY	0xfe
#define JXH63P_SUPPORT_15FPS_SCLK (85104000)
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

static int sensor_max_fps = TX_SENSOR_MAX_FPS_15;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static int fsync_mode = 3;
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

struct again_lut jxh63p_again_lut[] = {
	{0x0,  0 },
	{0x1,  5731 },
	{0x2,  11136},
	{0x3,  16248},
	{0x4,  21097},
	{0x5,  25710},
	{0x6,  30109},
	{0x7,  34312},
	{0x8,  38336},
	{0x9,  42195},
	{0xa,  45904},
	{0xb,  49472},
	{0xc,  52910},
	{0xd,  56228},
	{0xe,  59433},
	{0xf,  62534},
	{0x10,  65536},
	{0x11,	71267},
	{0x12,	76672},
	{0x13,	81784},
	{0x14,	86633},
	{0x15,	91246},
	{0x16,	95645},
	{0x17,	99848},
	{0x18,  103872},
	{0x19,	107731},
	{0x1a,	111440},
	{0x1b,	115008},
	{0x1c,	118446},
	{0x1d,	121764},
	{0x1e,	124969},
	{0x1f,	128070},
	{0x20,	131072},
	{0x21,	136803},
	{0x22,	142208},
	{0x23,	147320},
	{0x24,	152169},
	{0x25,	156782},
	{0x26,	161181},
	{0x27,	165384},
	{0x28,	169408},
	{0x29,	173267},
	{0x2a,	176976},
	{0x2b,	180544},
	{0x2c,	183982},
	{0x2d,	187300},
	{0x2e,	190505},
	{0x2f,	193606},
	{0x30,	196608},
	{0x31,	202339},
	{0x32,	207744},
	{0x33,	212856},
	{0x34,	217705},
	{0x35,	222318},
	{0x36,	226717},
	{0x37,	230920},
	{0x38,	234944},
	{0x39,	238803},
	{0x3a,	242512},
	{0x3b,	246080},
	{0x3c,	249518},
	{0x3d,	252836},
	{0x3e,	256041},
	{0x3f,	259142},
	{0x40,	262144},
	{0x41,	267875},
	{0x42,	273280},
	{0x43,	278392},
	{0x44,	283241},
	{0x45,	287854},
	{0x46,	292253},
	{0x47,	296456},
	{0x48,	300480},
	{0x49,	304339},
	{0x4a,	308048},
	{0x4b,	311616},
	{0x4c,	315054},
	{0x4d,	318372},
	{0x4e,	321577},
	{0x4f,	324678},
};

struct tx_isp_sensor_attribute jxh63p_attr;

unsigned int jxh63p_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = jxh63p_again_lut;
	while(lut->gain <= jxh63p_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == jxh63p_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int jxh63p_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus jxh63p_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 216,
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


struct tx_isp_sensor_attribute jxh63p_attr={
	.name = "jxh63p",
	.chip_id = 0x848,
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
	.max_again = 324678,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 0xbb8 - 4,
	.integration_time_limit = 0xbb8 - 4,
	.total_width = 1920,
	.total_height = 0xbb8,
	.max_integration_time = 0xbb8 - 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = jxh63p_alloc_again,
	.sensor_ctrl.alloc_dgain = jxh63p_alloc_dgain,
	.one_line_expr_in_us = 28,
        .fsync_attr = {
                .mode = TX_ISP_SENSOR_FSYNC_MODE_MS_REALTIME_MISPLACE,
                .call_times = 1,
                .sdelay = 1000,
        }
};


static struct regval_list jxh63p_init_regs_1280_720_15fps_mipi[] = {
#if 0
        /*15 fps VTS 1500,out 7.5; 10fps vts 2250 out 10*/
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
        {0x22, 0xca},	//;EE 30 //DC 15
        {0x23, 0x08},	//;02    //05
        {0x24, 0x80},
        {0x25, 0xD0},
        {0x26, 0x22},
        {0x27, 0xD6},
        {0x28, 0x15},
        {0x29, 0x02},
        {0x2A, 0xCB},
        {0x2B, 0x12},
        {0x2C, 0x00},
        {0x2D, 0x00},
        {0x2E, 0xBA},
        {0x2F, 0x60},
        {0x41, 0x84},
        {0x42, 0x02},
        {0x47, 0x46},
        {0x76, 0x40},
        {0x77, 0x06},
        {0x80, 0x01},
        {0xAF, 0x22},
        {0x8A, 0x00},
        {0xA6, 0x00},
        {0xAB, 0x00},
        {0x8D, 0x49},
        {0x1D, 0x00},
        {0x1E, 0x04},
        {0x6C, 0x50},
        {0x9E, 0xF8},
        {0x6E, 0x2C},
        {0x70, 0x8C},
        {0x71, 0x6D},
        {0x72, 0x6A},
        {0x73, 0x46},
        {0x74, 0x02},
        {0x78, 0x8E},
        {0x89, 0x01},
        {0x6B, 0x20},
        {0x86, 0x40},
        {0x9C, 0xE1},
        {0x3A, 0xAC},
        {0x3B, 0x18},
        {0x3C, 0x5D},
        {0x3D, 0x80},
        {0x3E, 0x6E},
        {0x31, 0x07},
        {0x32, 0x14},
        {0x33, 0x12},
        {0x34, 0x1C},
        {0x35, 0x1C},
        {0x56, 0x12},
        {0x59, 0x20},
        {0x85, 0x14},
        {0x64, 0xD2},
        {0x8F, 0x90},
        {0xA4, 0x87},
        {0xA7, 0x80},
        {0xA9, 0x48},
        {0x45, 0x01},
        {0x5B, 0xA0},
        {0x5C, 0x6C},
        {0x5D, 0x44},
        {0x5E, 0x81},
        {0x63, 0x0F},
        {0x65, 0x12},
        {0x66, 0x43},
        {0x67, 0x79},
        {0x68, 0x00},
        {0x69, 0x78},
        {0x6A, 0x28},
        {0x7A, 0x66},
        {0xA5, 0x03},
        {0x94, 0xC0},
        {0x13, 0x81},
        {0x96, 0x84},
        {0xB7, 0x4A},
        {0x4A, 0x01},
        {0xB5, 0x0C},
        {0xA1, 0x0F},
        {0xA3, 0x40},
        {0xB1, 0x00},
        {0x93, 0x00},
        {0x7E, 0x4C},
        {0x50, 0x02},
        {0x49, 0x10},
        {0x8E, 0x40},
        {0x7F, 0x56},
        {0x0C, 0x00},
        {0xBC, 0x11},
        {0x82, 0x00},
        {0x19, 0x20},
        {0x1F, 0x10},
        {0x1B, 0x4F},
        {0x46, 0x00},
        {0x47, 0x42},
        {0x1E, 0x0C},
        {0x12, 0x00},
#endif
        {0x12, 0x40},
        {0x48, 0x85},
        {0x48, 0x05},
        {0x0E, 0x11},
        {0x0F, 0xA4},
        {0x10, 0x40},
        {0x11, 0x80},
        {0x57, 0x60},
        {0x58, 0x18},
        {0x61, 0x10},
        {0x46, 0x08},
        {0x0D, 0x50},
        {0x20, 0xC0},
        {0x21, 0x03},
        {0x22, 0xB8},	//;EE
        {0x23, 0x0B},	//;02
        {0x24, 0x80},
        {0x25, 0xD0},
        {0x26, 0x22},
        {0x27, 0x9B},
        {0x28, 0x15},
        {0x29, 0x03},
        {0x2A, 0x90},
        {0x2B, 0x13},
        {0x2C, 0x00},
        {0x2D, 0x00},
        {0x2E, 0xBA},
        {0x2F, 0x60},
        {0x41, 0x84},
        {0x42, 0x02},
        {0x47, 0x46},
        {0x76, 0x40},
        {0x77, 0x06},
        {0x80, 0x01},
        {0xAF, 0x22},
        {0x8A, 0x00},
        {0xA6, 0x00},
        {0xAB, 0x00},
        {0x8D, 0x49},
        {0x1D, 0x00},
        {0x1E, 0x04},
        {0x6C, 0x50},
        {0x9E, 0xF8},
        {0x6E, 0x2C},
        {0x70, 0xD8},
        {0x71, 0xDB},
        {0x72, 0xD4},
        {0x73, 0x59},
        {0x74, 0x02},
        {0x78, 0x99},
        {0x89, 0x01},
        {0x6B, 0x20},
        {0x86, 0x40},
        {0x9C, 0xE1},
        {0x3A, 0xAC},
        {0x3B, 0x24},
        {0x3C, 0xA6},
        {0x3D, 0xE0},
        {0x3E, 0xD0},
        {0x31, 0x0E},
        {0x32, 0x28},
        {0x33, 0x20},
        {0x34, 0x38},
        {0x35, 0x38},
        {0x56, 0x12},
        {0x59, 0x40},
        {0x85, 0x30},
        {0x64, 0xD2},
        {0x8F, 0x90},
        {0xA4, 0x87},
        {0xA7, 0x80},
        {0xA9, 0x48},
        {0x45, 0x01},
        {0x5B, 0xA0},
        {0x5C, 0x6C},
        {0x5D, 0x44},
        {0x5E, 0x81},
        {0x63, 0x0F},
        {0x65, 0x12},
        {0x66, 0x43},
        {0x67, 0x79},
        {0x68, 0x00},
        {0x69, 0x78},
        {0x6A, 0x28},
        {0x7A, 0x66},
        {0xA5, 0x03},
        {0x94, 0xC0},
        {0x13, 0x81},
        {0x96, 0x84},
        {0xB7, 0x4A},
        {0x4A, 0x01},
        {0xB5, 0x0C},
        {0xA1, 0x0F},
        {0xA3, 0x40},
        {0xB1, 0x00},
        {0x93, 0x00},
        {0x7E, 0x4C},
        {0x50, 0x02},
        {0x49, 0x10},
        {0x8E, 0x40},
        {0x7F, 0x56},
        {0x0C, 0x00},
        {0xBC, 0x11},
        {0x82, 0x00},
        {0x19, 0x20},
        {0x1F, 0x10},
        {0x1B, 0x4F},
        {0x46, 0x00},
        {0x47, 0x42},
        {0x1E, 0x0C},
        {0x12, 0x00},
        {JXH63P_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the jxh63p_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting jxh63p_win_sizes[] = {
	{
		.width		= 1280,
		.height		= 720,
		.fps		= 15 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= jxh63p_init_regs_1280_720_15fps_mipi,
	}
};
struct tx_isp_sensor_win_setting *wsize = &jxh63p_win_sizes[0];


/*
 * the part of driver was fixed.
 */

static struct regval_list jxh63p_stream_on_dvp[] = {
	{JXH63P_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxh63p_stream_off_dvp[] = {
	{JXH63P_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxh63p_stream_on_mipi[] = {

	{JXH63P_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxh63p_stream_off_mipi[] = {
	{JXH63P_REG_END, 0x00},	/* END MARKER */
};

int jxh63p_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int jxh63p_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int jxh63p_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != JXH63P_REG_END) {
		if (vals->reg_num == JXH63P_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxh63p_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif
static int jxh63p_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != JXH63P_REG_END) {
		if (vals->reg_num == JXH63P_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxh63p_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int jxh63p_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int jxh63p_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = jxh63p_read(sd, 0x0a, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != JXH63P_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = jxh63p_read(sd, 0x0b, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;

	if (v != JXH63P_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int jxh63p_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;
	ret = jxh63p_write(sd,  0x01, (unsigned char)(expo & 0xff));
	ret += jxh63p_write(sd, 0x02, (unsigned char)((expo >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int jxh63p_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret = jxh63p_write(sd, 0x00, (unsigned char)(value & 0x7f));
	if (ret < 0)
		return ret;

	return 0;
}

static int jxh63p_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxh63p_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxh63p_init(struct tx_isp_subdev *sd, int enable)
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

	ret = jxh63p_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int jxh63p_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = jxh63p_write_array(sd, jxh63p_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = jxh63p_write_array(sd, jxh63p_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("jxh63p stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = jxh63p_write_array(sd, jxh63p_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = jxh63p_write_array(sd, jxh63p_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("jxh63p stream off\n");
	}

	return ret;
}

static int jxh63p_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0;

	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_15:
		sclk = JXH63P_SUPPORT_15FPS_SCLK;
		max_fps = TX_SENSOR_MAX_FPS_15;
		break;
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}


	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}

	val = 0;
	ret += jxh63p_read(sd, 0x21, &val);
	hts = val<<8;
	val = 0;
	ret += jxh63p_read(sd, 0x20, &val);
	hts |= val;
	hts *= 2;
	if (0 != ret) {
		ISP_ERROR("err: jxh63p read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += jxh63p_write(sd, 0x22, (unsigned char)(vts & 0xff));
	ret += jxh63p_write(sd, 0x23, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: jxh63p_write err\n");
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

static int jxh63p_set_hvflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	unsigned char val = 0;

	ret += jxh63p_read(sd, 0x12, &val);

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
	ret += jxh63p_write(sd, 0x12, val);

	return ret;
}

static int jxh63p_set_mode(struct tx_isp_subdev *sd, int value)
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

static int jxh63p_g_chip_ident(struct tx_isp_subdev *sd,
			      struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"jxh63p_reset");
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
		ret = private_gpio_request(pwdn_gpio,"jxh63p_pwdn");
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
	ret = jxh63p_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an jxh63p chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("jxh63p chip found @ 0x%02x (%s) version %s\n", client->addr, client->adapter->name, SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "jxh63p", sizeof("jxh63p"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int jxh63p_fsync(struct tx_isp_subdev *sd, struct tx_isp_sensor_fsync *fsync)
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

static int jxh63p_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = jxh63p_set_expo(sd, *(int*)arg);
#endif
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = jxh63p_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = jxh63p_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = jxh63p_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = jxh63p_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = jxh63p_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = jxh63p_write_array(sd, jxh63p_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = jxh63p_write_array(sd, jxh63p_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = jxh63p_write_array(sd, jxh63p_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = jxh63p_write_array(sd, jxh63p_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = jxh63p_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = jxh63p_set_hvflip(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int jxh63p_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = jxh63p_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int jxh63p_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	jxh63p_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops jxh63p_core_ops = {
	.g_chip_ident = jxh63p_g_chip_ident,
	.reset = jxh63p_reset,
	.init = jxh63p_init,
	/*.ioctl = jxh63p_ops_ioctl,*/
	.g_register = jxh63p_g_register,
	.s_register = jxh63p_s_register,
};

static struct tx_isp_subdev_video_ops jxh63p_video_ops = {
	.s_stream = jxh63p_s_stream,
};

static struct tx_isp_subdev_sensor_ops	jxh63p_sensor_ops = {
        .fsync = jxh63p_fsync,
	.ioctl	= jxh63p_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops jxh63p_ops = {
	.core = &jxh63p_core_ops,
	.video = &jxh63p_video_ops,
	.sensor = &jxh63p_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "jxh63p",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int jxh63p_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

                want_rate=27000000;
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

        jxh63p_attr.dbus_type = data_interface;
        if((data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) && (sensor_max_fps == TX_SENSOR_MAX_FPS_15)){
                wsize = &jxh63p_win_sizes[0];
                memcpy((void*)(&(jxh63p_attr.mipi)),(void*)(&jxh63p_mipi),sizeof(jxh63p_mipi));
        } else {
                ISP_ERROR("Can not support this data interface and fps!!!\n");
                goto err_set_sensor_data_interface;
        }
        jxh63p_attr.fsync_attr.mode = fsync_mode;

        jxh63p_attr.expo_fs = 0;
        sd = &sensor->sd;
        video = &sensor->video;
        sensor->video.shvflip = shvflip;
        sensor->video.attr = &jxh63p_attr;
        sensor->video.vi_max_width = wsize->width;
        sensor->video.vi_max_height = wsize->height;
        sensor->video.mbus.width = wsize->width;
        sensor->video.mbus.height = wsize->height;
        sensor->video.mbus.code = wsize->mbus_code;
        sensor->video.mbus.field = V4L2_FIELD_NONE;
        sensor->video.mbus.colorspace = wsize->colorspace;
        sensor->video.fps = wsize->fps;
        tx_isp_subdev_init(&sensor_platform_device, sd, &jxh63p_ops);
        tx_isp_set_subdevdata(sd, client);
        tx_isp_set_subdev_hostdata(sd, sensor);
        private_i2c_set_clientdata(client, sd);

        pr_debug("probe ok ------->jxh63p\n");
        return 0;
err_set_sensor_data_interface:
        private_clk_disable(sensor->mclk);
        private_clk_put(sensor->mclk);
err_get_mclk:
        kfree(sensor);

        return -1;
}

static int jxh63p_remove(struct i2c_client *client)
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

static const struct i2c_device_id jxh63p_id[] = {
	{ "jxh63p", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, jxh63p_id);

static struct i2c_driver jxh63p_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "jxh63p",
	},
	.probe		= jxh63p_probe,
	.remove		= jxh63p_remove,
	.id_table	= jxh63p_id,
};

static __init int init_jxh63p(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init jxh63p dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&jxh63p_driver);
}

static __exit void exit_jxh63p(void)
{
	private_i2c_del_driver(&jxh63p_driver);
}

module_init(init_jxh63p);
module_exit(exit_jxh63p);

MODULE_DESCRIPTION("A low-level driver for galaxycore jxh63p sensors");
MODULE_LICENSE("GPL");
