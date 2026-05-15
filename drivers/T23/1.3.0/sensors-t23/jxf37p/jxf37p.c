/*
 * jxf37p.c
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

#define SENSOR_CHIP_ID_H	(0x08)
#define SENSOR_CHIP_ID_L	(0x41)
#define SENSOR_REG_END		0xff
#define SENSOR_REG_DELAY	0xfe
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20241227a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = GPIO_PA(06);
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static int sboot = 0;
module_param(sboot, int, S_IRUGO);
MODULE_PARM_DESC(sboot, "Sensor HV Flip Enable interface");

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

struct again_lut jxf37p_again_lut[] = {
	// cnt_gain = 80 cnt_reg = 80
	{0x0, 0},
	{0x1, 5731},
	{0x2, 11136},
	{0x3, 16247},
	{0x4, 21097},
	{0x5, 25710},
	{0x6, 30108},
	{0x7, 34311},
	{0x8, 38335},
	{0x9, 42195},
	{0xa, 45903},
	{0xb, 49471},
	{0xc, 52910},
	{0xd, 56227},
	{0xe, 59433},
	{0xf, 62533},
	{0x10, 65535},
	{0x11, 71266},
	{0x12, 76671},
	{0x13, 81782},
	{0x14, 86632},
	{0x15, 91245},
	{0x16, 95643},
	{0x17, 99846},
	{0x18, 103870},
	{0x19, 107730},
	{0x1a, 111438},
	{0x1b, 115006},
	{0x1c, 118445},
	{0x1d, 121762},
	{0x1e, 124968},
	{0x1f, 128068},
	{0x20, 131070},
	{0x21, 136801},
	{0x22, 142206},
	{0x23, 147317},
	{0x24, 152167},
	{0x25, 156780},
	{0x26, 161178},
	{0x27, 165381},
	{0x28, 169405},
	{0x29, 173265},
	{0x2a, 176973},
	{0x2b, 180541},
	{0x2c, 183980},
	{0x2d, 187297},
	{0x2e, 190503},
	{0x2f, 193603},
	{0x30, 196605},
	{0x31, 202336},
	{0x32, 207741},
	{0x33, 212852},
	{0x34, 217702},
	{0x35, 222315},
	{0x36, 226713},
	{0x37, 230916},
	{0x38, 234940},
	{0x39, 238800},
	{0x3a, 242508},
	{0x3b, 246076},
	{0x3c, 249515},
	{0x3d, 252832},
	{0x3e, 256038},
	{0x3f, 259138},
	{0x40, 262140},
	{0x41, 267871},
	{0x42, 273276},
	{0x43, 278387},
	{0x44, 283237},
	{0x45, 287850},
	{0x46, 292248},
	{0x47, 296451},
	{0x48, 300475},
	{0x49, 304335},
	{0x4a, 308043},
	{0x4b, 311611},
	{0x4c, 315050},
	{0x4d, 318367},
	{0x4e, 321573},
	{0x4f, 324673},
};

struct tx_isp_sensor_attribute jxf37p_attr;

unsigned int jxf37p_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = jxf37p_again_lut;
	while(lut->gain <= jxf37p_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == jxf37p_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int jxf37p_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus jxf37p_mipi_1lane={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 378,
	.lans = 1,
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

struct tx_isp_mipi_bus jxf37p_mipi={
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

struct tx_isp_dvp_bus jxf37p_dvp = {
	.gpio = DVP_PA_LOW_10BIT,
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking = {
		.hblanking = 0,
		.vblanking = 0,
	},
	.polar = {
		.hsync_polar = 0,
		.vsync_polar = 0,
		.pclk_polar = 0, /**< reserved */
	},
	.dvp_hcomp_en = 0,
};

struct tx_isp_sensor_attribute jxf37p_attr={
	.name = "jxf37p",
	.chip_id = 0x841,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x46,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 324673,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 1350,
	.integration_time_limit = 1350,
	.total_width = 1280,
	.total_height = 1350,
	.max_integration_time = 1350,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = jxf37p_alloc_again,
	.sensor_ctrl.alloc_dgain = jxf37p_alloc_dgain,
};

static struct regval_list jxf37p_init_regs_1920_1080_25fps_mipi_1lane[] = {
	{0x12,0x40},
	{0x48,0x85},
	{0x48,0x05},
	{0x0E,0x19},
	{0x0F,0x04},
	{0x10,0x3F},
	{0x11,0x80},
	{0x46,0x09},
	{0x47,0x66},
	{0x0D,0xA2},
	{0x57,0x6A},
	{0x58,0x22},
	{0x5F,0x41},
	{0x60,0x28},
	{0xA5,0xC0},
	{0x20,0x40},
	{0x21,0x05},
	{0x22,0x65},
	{0x23,0x04},
	{0x24,0xC0},
	{0x25,0x38},
	{0x26,0x43},
	{0x27,0xE6},
	{0x28,0x15},
	{0x29,0x04},
	{0x2A,0xDB},
	{0x2B,0x14},
	{0x2C,0x02},
	{0x2D,0x00},
	{0x2E,0x14},
	{0x2F,0x04},
	{0x41,0xC5},
	{0x42,0x33},
	{0x47,0x46},
	{0x76,0x60},
	{0x77,0x09},
	{0x80,0x01},
	{0xAF,0x22},
	{0xAB,0x00},
	{0x1D,0x00},
	{0x1E,0x04},
	{0x6C,0x50},
	{0x9E,0xF8},
	{0x6E,0x2C},
	{0x70,0xD8},
	{0x71,0xDB},
	{0x72,0xD4},
	{0x73,0x59},
	{0x74,0x02},
	{0x78,0x99},
	{0x89,0x01},
	{0x6B,0x20},
	{0x86,0x40},
	{0x31,0x0C},
	{0x32,0x11},
	{0x33,0xDC},
	{0x34,0x46},
	{0x35,0x46},
	{0x3A,0xAF},
	{0x3B,0x00},
	{0x3C,0xFF},
	{0x3D,0xFF},
	{0x3E,0xFF},
	{0x3F,0xBB},
	{0x40,0xFF},
	{0x56,0x92},
	{0x59,0x80},
	{0x5A,0x47},
	{0x61,0x18},
	{0x6F,0x04},
	{0x85,0x44},
	{0x8A,0x44},
	{0x91,0x13},
	{0x94,0xA0},
	{0x9B,0x83},
	{0x9C,0xE1},
	{0xA4,0x80},
	{0xA6,0x22},
	{0xA9,0x1C},
	{0x5B,0xE7},
	{0x5C,0x28},
	{0x5D,0x67},
	{0x5E,0x11},
	{0x62,0x21},
	{0x63,0x0F},
	{0x64,0xD0},
	{0x65,0x02},
	{0x67,0x49},
	{0x66,0x00},
	{0x68,0x00},
	{0x69,0x72},
	{0x6A,0x12},
	{0x7A,0x00},
	{0x82,0x20},
	{0x8D,0x47},
	{0x8F,0x90},
	{0x45,0x01},
	{0x97,0x20},
	{0x13,0x81},
	{0x96,0x84},
	{0x4A,0x01},
	{0xB1,0x00},
	{0xA1,0x0F},
	{0xBE,0x00},
	{0x7E,0x48},
	{0xB5,0xC0},
	{0x50,0x02},
	{0x49,0x10},
	{0x7F,0x57},
	{0x90,0x00},
	{0x7B,0x4A},
	{0x7C,0x0C},
	{0x8C,0xFF},
	{0x8E,0x00},
	{0x8B,0x01},
	{0x0C,0x00},
	{0xBC,0x11},
	{0x19,0x20},
	{0x1B,0x4F},
	{0x12,0x00},
	{0x00,0x10},
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxf37p_init_regs_1920_1080_25fps_mipi[] = {
	{0x12,0x40},
	{0x48,0x8A},
	{0x48,0x0A},
	{0x0E,0x19},
	{0x0F,0x04},
	{0x10,0x24},
	{0x11,0x80},
	{0x46,0x09},
	{0x47,0x66},
	{0x0D,0xF2},
	{0x57,0x6A},
	{0x58,0x22},
	{0x5F,0x41},
	{0x60,0x28},
	{0xA5,0xC0},
	{0x20,0x00},
	{0x21,0x05},
	{0x22,0x46},
	{0x23,0x05},
	{0x24,0xC0},
	{0x25,0x38},
	{0x26,0x43},
	{0x27,0xC6},
	{0x28,0x15},
	{0x29,0x04},
	{0x2A,0xBB},
	{0x2B,0x14},
	{0x2C,0x02},
	{0x2D,0x00},
	{0x2E,0x14},
	{0x2F,0x04},
	{0x41,0xC5},
	{0x42,0x33},
	{0x47,0x46},
	{0x76,0x60},
	{0x77,0x09},
	{0x80,0x01},
	{0xAF,0x22},
	{0xAB,0x00},
	{0x1D,0x00},
	{0x1E,0x04},
	{0x6C,0x40},
	{0x9E,0xF8},
	{0x6E,0x2C},
	{0x70,0x6C},
	{0x71,0x6D},
	{0x72,0x6A},
	{0x73,0x56},
	{0x74,0x02},
	{0x78,0x9D},
	{0x89,0x01},
	{0x6B,0x20},
	{0x86,0x40},
	{0x31,0x10},
	{0x32,0x18},
	{0x33,0xE8},
	{0x34,0x5E},
	{0x35,0x5E},
	{0x3A,0xAF},
	{0x3B,0x00},
	{0x3C,0xFF},
	{0x3D,0xFF},
	{0x3E,0xFF},
	{0x3F,0xBB},
	{0x40,0xFF},
	{0x56,0x92},
	{0x59,0xAF},
	{0x5A,0x47},
	{0x61,0x18},
	{0x6F,0x04},
	{0x85,0x5F},
	{0x8A,0x44},
	{0x91,0x13},
	{0x94,0xA0},
	{0x9B,0x83},
	{0x9C,0xE1},
	{0xA4,0x80},
	{0xA6,0x22},
	{0xA9,0x1C},
	{0x5B,0xE7},
	{0x5C,0x28},
	{0x5D,0x67},
	{0x5E,0x11},
	{0x62,0x21},
	{0x63,0x0F},
	{0x64,0xD0},
	{0x65,0x02},
	{0x67,0x49},
	{0x66,0x00},
	{0x68,0x00},
	{0x69,0x72},
	{0x6A,0x12},
	{0x7A,0x00},
	{0x82,0x20},
	{0x8D,0x47},
	{0x8F,0x90},
	{0x45,0x01},
	{0x97,0x20},
	{0x13,0x81},
	{0x96,0x84},
	{0x4A,0x01},
	{0xB1,0x00},
	{0xA1,0x0F},
	{0xBE,0x00},
	{0x7E,0x48},
	{0xB5,0xC0},
	{0x50,0x02},
	{0x49,0x10},
	{0x7F,0x57},
	{0x90,0x00},
	{0x7B,0x4A},
	{0x7C,0x0C},
	{0x8C,0xFF},
	{0x8E,0x00},
	{0x8B,0x01},
	{0x0C,0x00},
	{0xBC,0x11},
	{0x19,0x20},
	{0x1B,0x4F},
	{0x12,0x00},
	{0x00,0x10},
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the jxf37p_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting jxf37p_win_sizes[] = {
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 2,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= jxf37p_init_regs_1920_1080_25fps_mipi_1lane,
	},
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 2,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= jxf37p_init_regs_1920_1080_25fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &jxf37p_win_sizes[0];


/*
 * the part of driver was fixed.
 */

static struct regval_list jxf37p_stream_on_dvp[] = {
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxf37p_stream_off_dvp[] = {
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxf37p_stream_on_mipi[] = {

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list jxf37p_stream_off_mipi[] = {
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

int jxf37p_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int jxf37p_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int jxf37p_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != jxf37p_REG_END) {
		if (vals->reg_num == jxf37p_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxf37p_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif
static int jxf37p_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxf37p_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int jxf37p_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int jxf37p_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = jxf37p_read(sd, 0x0a, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = jxf37p_read(sd, 0x0b, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int jxf37p_init(struct tx_isp_subdev *sd, int enable)
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
	ret = jxf37p_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int jxf37p_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = jxf37p_write_array(sd, jxf37p_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = jxf37p_write_array(sd, jxf37p_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("jxf37p stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = jxf37p_write_array(sd, jxf37p_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = jxf37p_write_array(sd, jxf37p_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("jxf37p stream off\n");
	}

	return ret;
}

static int jxf37p_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	jxf37p_write(sd, 0x02, it >> 8);
	jxf37p_write(sd, 0x01, it & 0xff);

	jxf37p_write(sd, 0x00, again & 0xff);

	return ret;
}

#if 0
static int jxf37p_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;
	ret = jxf37p_write(sd,  0x01, (unsigned char)(expo & 0xff));
	ret += jxf37p_write(sd, 0x02, (unsigned char)((expo >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int jxf37p_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret = jxf37p_write(sd, 0x00, (unsigned char)(value & 0x7f));
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int jxf37p_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxf37p_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxf37p_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0;

	switch(sboot){
	case 0:
		sclk = 1344 * 1125 * 25;  /**< HTS * VTS * FPS */
		max_fps = 25;
		break;
	case 1:
		sclk = 1280 * 1350 * 25;  /**< HTS * VTS * FPS */
		max_fps = 25;
		break;
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}

	val = 0;
	ret += jxf37p_read(sd, 0x21, &val);
	hts = val<<8;
	val = 0;
	ret += jxf37p_read(sd, 0x20, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: jxf37p read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += jxf37p_write(sd, 0x22, (unsigned char)(vts & 0xff));
	ret += jxf37p_write(sd, 0x23, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: jxf37p_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts;
	sensor->video.attr->integration_time_limit = vts;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;
}

static int jxf37p_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	unsigned char val = 0;

	ret += jxf37p_read(sd, 0x12, &val);

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
	ret += jxf37p_write(sd, 0x12, val);

	return ret;
}

static int jxf37p_set_mode(struct tx_isp_subdev *sd, int value)
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

static int jxf37p_g_chip_ident(struct tx_isp_subdev *sd,
							   struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"jxf37p_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"jxf37p_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = jxf37p_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an jxf37p chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("jxf37p chip found @ 0x%02x (%s) version %s\n", client->addr, client->adapter->name, SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "jxf37p", sizeof("jxf37p"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}


static int jxf37p_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}

	switch(cmd){
#if 1
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = jxf37p_set_expo(sd, *(int*)arg);
#else
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = jxf37p_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = jxf37p_set_analog_gain(sd, *(int*)arg);
		break;
#endif
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = jxf37p_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = jxf37p_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = jxf37p_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = jxf37p_write_array(sd, jxf37p_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = jxf37p_write_array(sd, jxf37p_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = jxf37p_write_array(sd, jxf37p_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = jxf37p_write_array(sd, jxf37p_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = jxf37p_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = jxf37p_set_vflip(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int jxf37p_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = jxf37p_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int jxf37p_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	jxf37p_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops jxf37p_core_ops = {
	.g_chip_ident = jxf37p_g_chip_ident,
	.reset = jxf37p_reset,
	.init = jxf37p_init,
	/*.ioctl = jxf37p_ops_ioctl,*/
	.g_register = jxf37p_g_register,
	.s_register = jxf37p_s_register,
};

static struct tx_isp_subdev_video_ops jxf37p_video_ops = {
	.s_stream = jxf37p_s_stream,
};

static struct tx_isp_subdev_sensor_ops	jxf37p_sensor_ops = {
	.ioctl	= jxf37p_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops jxf37p_ops = {
	.core = &jxf37p_core_ops,
	.video = &jxf37p_video_ops,
	.sensor = &jxf37p_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "jxf37p",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sensor_mclk_config(struct tx_isp_sensor *sensor, unsigned long want_rate)
{
	unsigned long rate = 0;
	struct clk *pll = NULL;
	char *plls[] = {"mpll", "sclka"};
	int psize = sizeof(plls) / sizeof(char *);
	char *ppll = plls[psize - 1];
	int ret = 0, i = 0;

	pll = clk_get_parent(sensor->mclk);
	rate = clk_get_rate(pll);
	if (rate % want_rate) {
		for (i = 0; i < psize; i++) {
			pll = clk_get(NULL, plls[i]);
			rate = clk_get_rate(pll);
			if (!(rate % want_rate)) {
				ret = clk_set_parent(sensor->mclk, pll);
				if (ret) {
					ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
								__func__, __LINE__, plls[i]);
					continue;
				} else {
					break;
				}
			}
		}
		if (i == psize) {
			if (!ret) {
				pll = clk_get(NULL, ppll);
				rate = clk_get_rate(pll);
				if(want_rate == 37125000){
					if((rate >= 1188000000)) {
						rate = 1188000000;
					} else if (rate >= 891000000) {
						rate = 891000000;
					} else {
						ISP_ERROR("[%s %d] The %s clock setting failed !!!\n",
								  __func__, __LINE__, ppll);
						ret = -1;
						goto error;
					}
				} else if (want_rate == 24000000 || want_rate == 27000000) {
					rate -= rate % want_rate;
				} else {
					ret = -1;
					goto error;
				}
				ret = private_clk_set_rate(pll, rate);
				if (ret) {
					ISP_WARNING("[%s %d] Failed to set %s !!!\n",
								__func__, __LINE__, ppll);
					goto error;
				} else {
					ISP_WARNING("[%s %d] !!!!!!!!!!! The %s frequency has been changed to %ld !!!\n",
								__func__, __LINE__, ppll, rate);
				}
				ret = clk_set_parent(sensor->mclk, pll);
				if (ret) {
					ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
								__func__, __LINE__, ppll);
					goto error;
				}
			} else {
				goto error;
			}
		}
	}
	private_clk_set_rate(sensor->mclk, want_rate);
	private_clk_enable(sensor->mclk);

	rate = clk_get_rate(sensor->mclk);
	if (rate % want_rate) {
		ret = -1;
		goto error;
	}

	return ret;

error:
	ISP_ERROR("[%s %d] Unable to allocate the required MCLK %ld !!!\n",
			  __func__, __LINE__, want_rate);
	return ret;
}

static int jxf37p_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

#ifdef CONFIG_KERNEL_4_4_94
	sensor->mclk = clk_get(NULL, "div_cim");
#else
	sensor->mclk = clk_get(NULL, "cgu_cim");
#endif
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	sensor_mclk_config(sensor, 24000000);

	switch(sboot){
	case 0:
		wsize = &jxf37p_win_sizes[0];
		jxf37p_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		jxf37p_attr.max_dgain = 0;
		jxf37p_attr.max_again = 324673;
		jxf37p_attr.min_integration_time = 1;
		jxf37p_attr.max_integration_time = 1125;
		jxf37p_attr.total_width = 1344;
		jxf37p_attr.total_height = 1125;
		jxf37p_attr.integration_time_apply_delay = 2;
		jxf37p_attr.again_apply_delay = 2;
		jxf37p_attr.dgain_apply_delay = 0;
		jxf37p_attr.integration_time_limit = jxf37p_attr.max_integration_time;
		jxf37p_attr.max_integration_time_native = jxf37p_attr.max_integration_time;
		jxf37p_attr.min_integration_time_native = jxf37p_attr.min_integration_time;
		jxf37p_attr.expo_fs = 1;
		memcpy((void*)(&(jxf37p_attr.mipi)),(void*)(&jxf37p_mipi_1lane),sizeof(jxf37p_mipi));
		break;
	case 1:
		wsize = &jxf37p_win_sizes[1];
		jxf37p_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		jxf37p_attr.max_dgain = 0;
		jxf37p_attr.max_again = 324673;
		jxf37p_attr.min_integration_time = 1;
		jxf37p_attr.max_integration_time = 1350;
		jxf37p_attr.total_width = 1280;
		jxf37p_attr.total_height = 1350;
		jxf37p_attr.integration_time_apply_delay = 2;
		jxf37p_attr.again_apply_delay = 2;
		jxf37p_attr.dgain_apply_delay = 0;
		jxf37p_attr.integration_time_limit = jxf37p_attr.max_integration_time;
		jxf37p_attr.max_integration_time_native = jxf37p_attr.max_integration_time;
		jxf37p_attr.min_integration_time_native = jxf37p_attr.min_integration_time;
		jxf37p_attr.expo_fs = 1;
		memcpy((void*)(&(jxf37p_attr.mipi)),(void*)(&jxf37p_mipi),sizeof(jxf37p_mipi));
		break;
	}

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &jxf37p_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &jxf37p_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->jxf37p\n");
	return 0;
	// err_set_sensor_data_interface:

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int jxf37p_remove(struct i2c_client *client)
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

static const struct i2c_device_id jxf37p_id[] = {
	{ "jxf37p", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, jxf37p_id);

static struct i2c_driver jxf37p_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "jxf37p",
	},
	.probe		= jxf37p_probe,
	.remove		= jxf37p_remove,
	.id_table	= jxf37p_id,
};

static __init int init_jxf37p(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init jxf37p dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&jxf37p_driver);
}

static __exit void exit_jxf37p(void)
{
	private_i2c_del_driver(&jxf37p_driver);
}

module_init(init_jxf37p);
module_exit(exit_jxf37p);

MODULE_DESCRIPTION("A low-level driver for galaxycore jxf37p sensors");
MODULE_LICENSE("GPL");
