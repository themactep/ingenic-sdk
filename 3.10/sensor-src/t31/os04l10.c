// SPDX-License-Identifier: GPL-2.0+
/*
 * os04l10.c
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
#include <tx-isp-common.h>
#include <sensor-common.h>
#include <sensor-info.h>

// ============================================================================
// SENSOR IDENTIFICATION
// ============================================================================
#define SENSOR_NAME "os04l10"
#define SENSOR_VERSION "H20230417a"
#define SENSOR_CHIP_ID 0x4c10
#define SENSOR_CHIP_ID_H (0x4c)
#define SENSOR_CHIP_ID_L (0x10)

// ============================================================================
// HARDWARE INTERFACE
// ============================================================================
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x3c

// ============================================================================
// SENSOR CAPABILITIES
// ============================================================================
#define SENSOR_MAX_WIDTH 2560
#define SENSOR_MAX_HEIGHT 1440

// ============================================================================
// REGISTER DEFINITIONS
// ============================================================================
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe

// ============================================================================
// TIMING AND PERFORMANCE
// ============================================================================
#define SENSOR_SUPPORT_SCLK (101046360)
#define SENSOR_SUPPORT_SCLK_1M (101008000)
#define SENSOR_SUPPORT_SCLK_110FPS (101133010)
#define SENSOR_SUPPORT_SCLK_WDR (54000000)
#define SENSOR_OUTPUT_MAX_FPS 60
#define SENSOR_OUTPUT_MIN_FPS 5

typedef enum
{
	SENSOR_RES_100 = 100,
	SENSOR_RES_200 = 200,
	SENSOR_RES_300 = 300,
	SENSOR_RES_370 = 370,
	SENSOR_RES_400 = 400,
} sensor_res_mode;

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_resolution = SENSOR_RES_400;
module_param(sensor_resolution, int, S_IRUGO);
MODULE_PARM_DESC(sensor_resolution, "Sensor Resolution set interface");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_30;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
module_param(data_type, int, S_IRUGO);
MODULE_PARM_DESC(data_type, "Sensor Date Type");

static int wdr_bufsize = 10077696; // cache lines corrponding on VPB1
module_param(wdr_bufsize, int, S_IRUGO);
MODULE_PARM_DESC(wdr_bufsize, "Wdr Buf Size");

static struct sensor_info sensor_info = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.version = SENSOR_VERSION,
	.min_fps = SENSOR_OUTPUT_MIN_FPS,
	.max_fps = SENSOR_OUTPUT_MAX_FPS,
	.actual_fps = 0,
	.chip_i2c_addr = SENSOR_I2C_ADDRESS,
	.width = SENSOR_MAX_WIDTH,
	.height = SENSOR_MAX_HEIGHT,
};

struct regval_list
{
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut
{
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
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

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;

	*sensor_it = expo;

	return it;
}

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;

	while (lut->gain <= sensor_attr.max_again)
	{
		if (isp_gain == 0)
		{
			*sensor_again = lut->value;
			return 0;
		}
		else if (isp_gain < lut->gain)
		{
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else
		{
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain))
			{
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

struct tx_isp_mipi_bus sensor_mipi = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 144,
	.lans = 2,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10, // RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
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
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 144,
		.lans = 2,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
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
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.mipi_sc.data_type_en = 0,
		.mipi_sc.data_type_value = RAW10,
		.mipi_sc.del_start = 0,
		.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
		.mipi_sc.sensor_fid_mode = 0,
		.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
	},
	.max_again = 259142,
	.max_again_short = 259142,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1764 - 9,
	.integration_time_limit = 1764 - 9,
	.total_width = 816,
	.total_height = 1468,
	.max_integration_time = 1764 - 9,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	//	.sensor_ctrl.alloc_integration_time = sensor_alloc_integration_time,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	//	.sensor_ctrl.alloc_integration_time_short = sensor_alloc_integration_time_short,
};

static struct regval_list sensor_init_regs_2560_1440_25fps[] = {
	{0xfd, 0x00},
	{0x36, 0x01},
	{0xfd, 0x00},
	{0x36, 0x00},
	{0xfd, 0x00},
	{0x20, 0x00},
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
	{0x05, 0x01},
	{0x06, 0x28},
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
	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* [0] 5M @30fps*/
	{
		.width = 2560,
		.height = 1440,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2560_1440_25fps,
	},
	/* [1] 2M @60fps*/

};

static struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];


static struct regval_list sensor_stream_on[] = {
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	{0x0100, 0x00},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,
				 unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	// uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		[1] = {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = value,
		}};
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg,
				  unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
		.buf = buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END)
	{
		if (vals->reg_num == SENSOR_REG_DELAY)
		{
			msleep(vals->value);
		}
		else
		{
			ret = sensor_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%02x, vals->value:0x%02x\n", vals->reg_num, val);
		vals++;
	}
	return 0;
}
static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char v1;
	while (vals->reg_num != SENSOR_REG_END)
	{
		if (vals->reg_num == SENSOR_REG_DELAY)
		{
			private_msleep(vals->value);
		}
		else
		{
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
	unsigned char v;
	int ret;
	ret = sensor_read(sd, 0x04, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x05, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
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
	int expo = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	ret = sensor_write(sd, 0xfd, 0x01);
	/*expo*/
	ret += sensor_write(sd, 0x04, (unsigned char)(expo & 0xff));
	ret += sensor_write(sd, 0x03, (unsigned char)((expo >> 8) & 0xff));

	/*gain*/
	ret += sensor_write(sd, 0x24, (unsigned char)((again & 0xff)));
	ret = sensor_write(sd, 0x01, 0x01);

	if (ret < 0)
		return ret;

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

	sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);
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

	if (enable)
	{
		ret = sensor_write_array(sd, sensor_stream_on);
		pr_debug("%s stream on\n", SENSOR_NAME);
	}
	else
	{
		ret = sensor_write_array(sd, sensor_stream_off);
		pr_debug("%s stream off\n", SENSOR_NAME);
	}
	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; // the format is 24.8
	unsigned int max_fps = 0;
	ret += sensor_write(sd, 0xfd, 0x01);

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8))
	{
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}
	sclk = 1764 * 816 * 25;
	ret += sensor_read(sd, 0x37, &val);
	hts = val << 8;
	ret += sensor_read(sd, 0x38, &val);
	hts |= val;
	if (0 != ret)
	{
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	if (0 != ret)
	{
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}
	sensor->video.fps = fps;

	sensor_update_actual_fps((fps >> 16) & 0xffff);
	sensor->video.attr->max_integration_time_native = vts - 9;
	sensor->video.attr->integration_time_limit = vts - 9;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 9;
	vts = vts - 1440;
	ret += sensor_write(sd, 0x06, vts & 0xff);
	ret += sensor_write(sd, 0x05, (vts >> 8) & 0xff);

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor_write(sd, 0x01, 0x01);
	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize)
	{
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;

		sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);
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
	if (reset_gpio != -1)
	{
		ret = private_gpio_request(reset_gpio, "sensor_reset");
		if (!ret)
		{
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(15);
		}
		else
		{
			ISP_ERROR("gpio request fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1)
	{
		ret = private_gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret)
		{
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}
		else
		{
			ISP_ERROR("gpio request fail %d\n", pwdn_gpio);
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
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if (IS_ERR_OR_NULL(sd))
	{
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd)
	{
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = sensor_set_expo(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		// if (arg)
		// ret = sensor_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		// if (arg)
		// ret = sensor_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sensor_set_digital_gain(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sensor_get_black_pedestal(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sensor_set_mode(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = sensor_write_array(sd, sensor_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = sensor_write_array(sd, sensor_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, *(int *)arg);
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
	if (len && strncmp(sd->chip.name, reg->name, len))
	{
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
	if (len && strncmp(sd->chip.name, reg->name, len))
	{
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

/*It's the sensor device */
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

static int sensor_probe(struct i2c_client *client,
						 const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
	{
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk))
	{
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	wsize = &sensor_win_sizes[0];
	sensor_info.max_fps = 25;
	memcpy((void *)(&(sensor_attr.mipi)), (void *)(&sensor_mipi), sizeof(sensor_mipi));
	sensor_attr.max_integration_time_native = 1764 - 9;
	sensor_attr.integration_time_limit = 1764 - 9;
	sensor_attr.total_width = 816;
	sensor_attr.total_height = 1468;
	sensor_attr.max_integration_time = 1764 - 9;
	sensor_attr.mipi.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10;

	sensor_attr.data_type = data_type;
	sensor_attr.max_again = 259142;
	sensor_attr.expo_fs = 1;
	sensor_attr.max_dgain = 0;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sensor_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;

	sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);
	return 0;
err_get_mclk:
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
	{SENSOR_NAME, 0},
	{}
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
	if (ret)
	{
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
