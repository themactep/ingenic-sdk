// SPDX-License-Identifier: GPL-2.0+
/*
 * jxf37p.c
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

#define SENSOR_CHIP_ID ((SENSOR_CHIP_ID_H << 8) | SENSOR_CHIP_ID_L)
#define SENSOR_CHIP_ID_H (0x08)
#define SENSOR_CHIP_ID_L (0x41)
#define SENSOR_I2C_ADDRESS 0x46
#define SENSOR_MAX_HEIGHT 1080
#define SENSOR_MAX_WIDTH 1920
#define SENSOR_NAME "jxf37p"
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_REG_DELAY 0xfe
#define SENSOR_REG_END 0xff
#define SENSOR_VERSION "H20241227a"


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
	uint16_t value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	// cnt_gain = 80 cnt_reg = 80
	{0x00, 0},
	{0x01, 5731},
	{0x02, 11136},
	{0x03, 16247},
	{0x04, 21097},
	{0x05, 25710},
	{0x06, 30108},
	{0x07, 34311},
	{0x08, 38335},
	{0x09, 42195},
	{0x0a, 45903},
	{0x0b, 49471},
	{0x0c, 52910},
	{0x0d, 56227},
	{0x0e, 59433},
	{0x0f, 62533},
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

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = 0;
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

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi_1lane = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 378,
	.lans = 1,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10, // RAW10
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

struct tx_isp_mipi_bus sensor_mipi = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 216,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10, // RAW10
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

struct tx_isp_dvp_bus sensor_dvp = {
	.gpio = DVP_PA_LOW_10BIT,
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking =
		{
			.hblanking = 0,
			.vblanking = 0,
		},
	.polar =
		{
			.hsync_polar = 0,
			.vsync_polar = 0,
			.pclk_polar = 0, /**< reserved */
		},
	.dvp_hcomp_en = 0,
};

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
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
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_1920_1080_25fps_mipi_1lane[] = {
	{0x12, 0x40},
	{0x48, 0x85},
	{0x48, 0x05},
	{0x0e, 0x19},
	{0x0f, 0x04},
	{0x10, 0x3f},
	{0x11, 0x80},
	{0x46, 0x09},
	{0x47, 0x66},
	{0x0d, 0xa2},
	{0x57, 0x6a},
	{0x58, 0x22},
	{0x5f, 0x41},
	{0x60, 0x28},
	{0xa5, 0xc0},
	{0x20, 0x40},
	{0x21, 0x05},
	{0x22, 0x65},
	{0x23, 0x04},
	{0x24, 0xc0},
	{0x25, 0x38},
	{0x26, 0x43},
	{0x27, 0xe6},
	{0x28, 0x15},
	{0x29, 0x04},
	{0x2a, 0xdb},
	{0x2b, 0x14},
	{0x2c, 0x02},
	{0x2d, 0x00},
	{0x2e, 0x14},
	{0x2f, 0x04},
	{0x41, 0xc5},
	{0x42, 0x33},
	{0x47, 0x46},
	{0x76, 0x60},
	{0x77, 0x09},
	{0x80, 0x01},
	{0xaf, 0x22},
	{0xab, 0x00},
	{0x1d, 0x00},
	{0x1e, 0x04},
	{0x6c, 0x50},
	{0x9e, 0xf8},
	{0x6e, 0x2c},
	{0x70, 0xd8},
	{0x71, 0xdb},
	{0x72, 0xd4},
	{0x73, 0x59},
	{0x74, 0x02},
	{0x78, 0x99},
	{0x89, 0x01},
	{0x6b, 0x20},
	{0x86, 0x40},
	{0x31, 0x0c},
	{0x32, 0x11},
	{0x33, 0xdc},
	{0x34, 0x46},
	{0x35, 0x46},
	{0x3a, 0xaf},
	{0x3b, 0x00},
	{0x3c, 0xff},
	{0x3d, 0xff},
	{0x3e, 0xff},
	{0x3f, 0xbb},
	{0x40, 0xff},
	{0x56, 0x92},
	{0x59, 0x80},
	{0x5a, 0x47},
	{0x61, 0x18},
	{0x6f, 0x04},
	{0x85, 0x44},
	{0x8a, 0x44},
	{0x91, 0x13},
	{0x94, 0xa0},
	{0x9b, 0x83},
	{0x9c, 0xe1},
	{0xa4, 0x80},
	{0xa6, 0x22},
	{0xa9, 0x1c},
	{0x5b, 0xe7},
	{0x5c, 0x28},
	{0x5d, 0x67},
	{0x5e, 0x11},
	{0x62, 0x21},
	{0x63, 0x0f},
	{0x64, 0xd0},
	{0x65, 0x02},
	{0x67, 0x49},
	{0x66, 0x00},
	{0x68, 0x00},
	{0x69, 0x72},
	{0x6a, 0x12},
	{0x7a, 0x00},
	{0x82, 0x20},
	{0x8d, 0x47},
	{0x8f, 0x90},
	{0x45, 0x01},
	{0x97, 0x20},
	{0x13, 0x81},
	{0x96, 0x84},
	{0x4a, 0x01},
	{0xb1, 0x00},
	{0xa1, 0x0f},
	{0xbe, 0x00},
	{0x7e, 0x48},
	{0xb5, 0xc0},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x7f, 0x57},
	{0x90, 0x00},
	{0x7b, 0x4a},
	{0x7c, 0x0c},
	{0x8c, 0xff},
	{0x8e, 0x00},
	{0x8b, 0x01},
	{0x0c, 0x00},
	{0xbc, 0x11},
	{0x19, 0x20},
	{0x1b, 0x4f},
	{0x12, 0x00},
	{0x00, 0x10},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_25fps_mipi[] = {
	{0x12, 0x40},
	{0x48, 0x8a},
	{0x48, 0x0a},
	{0x0e, 0x19},
	{0x0f, 0x04},
	{0x10, 0x24},
	{0x11, 0x80},
	{0x46, 0x09},
	{0x47, 0x66},
	{0x0d, 0xf2},
	{0x57, 0x6a},
	{0x58, 0x22},
	{0x5f, 0x41},
	{0x60, 0x28},
	{0xa5, 0xc0},
	{0x20, 0x00},
	{0x21, 0x05},
	{0x22, 0x46},
	{0x23, 0x05},
	{0x24, 0xc0},
	{0x25, 0x38},
	{0x26, 0x43},
	{0x27, 0xc6},
	{0x28, 0x15},
	{0x29, 0x04},
	{0x2a, 0xbb},
	{0x2b, 0x14},
	{0x2c, 0x02},
	{0x2d, 0x00},
	{0x2e, 0x14},
	{0x2f, 0x04},
	{0x41, 0xc5},
	{0x42, 0x33},
	{0x47, 0x46},
	{0x76, 0x60},
	{0x77, 0x09},
	{0x80, 0x01},
	{0xaf, 0x22},
	{0xab, 0x00},
	{0x1d, 0x00},
	{0x1e, 0x04},
	{0x6c, 0x40},
	{0x9e, 0xf8},
	{0x6e, 0x2c},
	{0x70, 0x6c},
	{0x71, 0x6d},
	{0x72, 0x6a},
	{0x73, 0x56},
	{0x74, 0x02},
	{0x78, 0x9d},
	{0x89, 0x01},
	{0x6b, 0x20},
	{0x86, 0x40},
	{0x31, 0x10},
	{0x32, 0x18},
	{0x33, 0xe8},
	{0x34, 0x5e},
	{0x35, 0x5e},
	{0x3a, 0xaf},
	{0x3b, 0x00},
	{0x3c, 0xff},
	{0x3d, 0xff},
	{0x3e, 0xff},
	{0x3f, 0xbb},
	{0x40, 0xff},
	{0x56, 0x92},
	{0x59, 0xaf},
	{0x5a, 0x47},
	{0x61, 0x18},
	{0x6f, 0x04},
	{0x85, 0x5f},
	{0x8a, 0x44},
	{0x91, 0x13},
	{0x94, 0xa0},
	{0x9b, 0x83},
	{0x9c, 0xe1},
	{0xa4, 0x80},
	{0xa6, 0x22},
	{0xa9, 0x1c},
	{0x5b, 0xe7},
	{0x5c, 0x28},
	{0x5d, 0x67},
	{0x5e, 0x11},
	{0x62, 0x21},
	{0x63, 0x0f},
	{0x64, 0xd0},
	{0x65, 0x02},
	{0x67, 0x49},
	{0x66, 0x00},
	{0x68, 0x00},
	{0x69, 0x72},
	{0x6a, 0x12},
	{0x7a, 0x00},
	{0x82, 0x20},
	{0x8d, 0x47},
	{0x8f, 0x90},
	{0x45, 0x01},
	{0x97, 0x20},
	{0x13, 0x81},
	{0x96, 0x84},
	{0x4a, 0x01},
	{0xb1, 0x00},
	{0xa1, 0x0f},
	{0xbe, 0x00},
	{0x7e, 0x48},
	{0xb5, 0xc0},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x7f, 0x57},
	{0x90, 0x00},
	{0x7b, 0x4a},
	{0x7c, 0x0c},
	{0x8c, 0xff},
	{0x8e, 0x00},
	{0x8b, 0x01},
	{0x0c, 0x00},
	{0xbc, 0x11},
	{0x19, 0x20},
	{0x1b, 0x4f},
	{0x12, 0x00},
	{0x00, 0x10},
	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 2,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_25fps_mipi_1lane,
	},
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 2,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_25fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

static struct regval_list sensor_stream_on_dvp[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_dvp[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_on_mipi[] = {

	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, unsigned char reg, unsigned char *value) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct i2c_msg msg[2] = {[0] =
					 {
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

int sensor_write(struct tx_isp_subdev *sd, unsigned char reg, unsigned char value) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
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
static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
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

static int sensor_reset(struct tx_isp_subdev *sd, int val) {
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident) {
	unsigned char v;
	int ret;

	ret = sensor_read(sd, 0x0a, &v);
	ISP_INFO("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x0b, &v);
	ISP_INFO("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, int enable) {
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

static int sensor_s_stream(struct tx_isp_subdev *sd, int enable) {
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);

		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_INFO("%s stream on\n", SENSOR_NAME);

	} else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);

		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_INFO("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_expo(struct tx_isp_subdev *sd, int value) {
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	sensor_write(sd, 0x02, it >> 8);
	sensor_write(sd, 0x01, it & 0xff);

	sensor_write(sd, 0x00, again & 0xff);

	return ret;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;
	ret = sensor_write(sd,  0x01, (unsigned char)(expo & 0xff));
	ret += sensor_write(sd, 0x02, (unsigned char)((expo >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret = sensor_write(sd, 0x00, (unsigned char)(value & 0x7f));
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; // the format is 24.8
	unsigned int max_fps = 0;

	switch (sboot) {
	case 0:
		sclk = 1344 * 1125 * 25; /**< HTS * VTS * FPS */
		max_fps = 25;
		break;
	case 1:
		sclk = 1280 * 1350 * 25; /**< HTS * VTS * FPS */
		max_fps = 25;
		break;
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}

	val = 0;
	ret += sensor_read(sd, 0x21, &val);
	hts = val << 8;
	val = 0;
	ret += sensor_read(sd, 0x20, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: %s read err\n", SENSOR_NAME);
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += sensor_write(sd, 0x22, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x23, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: sensor_write err\n");
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

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable) {
	int ret = 0;
	unsigned char val = 0;

	ret += sensor_read(sd, 0x12, &val);

	enable &= 0x03;
	switch (enable) {
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
	ret += sensor_write(sd, 0x12, val);

	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value) {
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

static int sensor_g_chip_ident(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio, "sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio request failed %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio request failed %d\n", pwdn_gpio);
		}
	}
	ret = sensor_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an %s chip.\n", client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}
	ISP_INFO("%s chip found @ 0x%02x (%s) version %s\n", SENSOR_NAME, client->addr, client->adapter->name, SENSOR_VERSION);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg) {
	long ret = 0;
	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}

	switch (cmd) {
#if 1
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = sensor_set_expo(sd, *(int *)arg);
#else
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sensor_set_integration_time(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sensor_set_analog_gain(sd, *(int *)arg);
		break;
#endif
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
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);

		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);

		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if (arg)
			ret = sensor_set_vflip(sd, *(int *)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int sensor_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg) {
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

static int sensor_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg) {
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

static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = SENSOR_NAME,
	.id = -1,
	.dev =
		{
			.dma_mask = &tx_isp_module_dma_mask,
			.coherent_dma_mask = 0xffffffff,
			.platform_data = NULL,
		},
	.num_resources = 0,
};

static int sensor_mclk_config(struct tx_isp_sensor *sensor, unsigned long want_rate) {
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
						__func__,
						__LINE__,
						plls[i]);
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
				if (want_rate == 37125000) {
					if ((rate >= 1188000000)) {
						rate = 1188000000;
					} else if (rate >= 891000000) {
						rate = 891000000;
					} else {
						ISP_ERROR("[%s %d] The %s clock setting failed !!!\n",
							__func__,
							__LINE__,
							ppll);
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
					ISP_WARNING("[%s %d] Failed to set %s !!!\n", __func__, __LINE__, ppll);
					goto error;
				} else {
					ISP_WARNING("[%s %d] !!!!!!!!!!! The %s frequency has been changed to %ld "
						    "!!!\n",
						__func__,
						__LINE__,
						ppll,
						rate);
				}
				ret = clk_set_parent(sensor->mclk, pll);
				if (ret) {
					ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
						__func__,
						__LINE__,
						ppll);
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
	ISP_ERROR("[%s %d] Unable to allocate the required MCLK %ld !!!\n", __func__, __LINE__, want_rate);
	return ret;
}

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id) {
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));

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

	switch (sboot) {
	case 0:
		wsize = &sensor_win_sizes[0];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.max_dgain = 0;
		sensor_attr.max_again = 324673;
		sensor_attr.min_integration_time = 1;
		sensor_attr.max_integration_time = 1125;
		sensor_attr.total_width = 1344;
		sensor_attr.total_height = 1125;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;
		sensor_attr.integration_time_limit = sensor_attr.max_integration_time;
		sensor_attr.max_integration_time_native = sensor_attr.max_integration_time;
		sensor_attr.min_integration_time_native = sensor_attr.min_integration_time;
		sensor_attr.expo_fs = 1;
		memcpy((void *)(&(sensor_attr.mipi)), (void *)(&sensor_mipi_1lane), sizeof(sensor_mipi));
		break;
	case 1:
		wsize = &sensor_win_sizes[1];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.max_dgain = 0;
		sensor_attr.max_again = 324673;
		sensor_attr.min_integration_time = 1;
		sensor_attr.max_integration_time = 1350;
		sensor_attr.total_width = 1280;
		sensor_attr.total_height = 1350;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;
		sensor_attr.integration_time_limit = sensor_attr.max_integration_time;
		sensor_attr.max_integration_time_native = sensor_attr.max_integration_time;
		sensor_attr.min_integration_time_native = sensor_attr.min_integration_time;
		sensor_attr.expo_fs = 1;
		memcpy((void *)(&(sensor_attr.mipi)), (void *)(&sensor_mipi), sizeof(sensor_mipi));
		break;
	}

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

	ISP_INFO("probe ok ------->%s\n", SENSOR_NAME);
	return 0;
	// err_set_sensor_data_interface:

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int sensor_remove(struct i2c_client *client) {
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

static const struct i2c_device_id sensor_id[] = {{SENSOR_NAME, 0}, {}};
MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver =
		{
			.owner = THIS_MODULE,
			.name = SENSOR_NAME,
		},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

static __init int init_sensor(void) {
	sensor_common_init(&sensor_info);
	int ret = 0;
	ret = private_driver_get_interface();
	if (ret) {
		ISP_ERROR("Failed to init %s driver.\n", SENSOR_NAME);
		return -1;
	}
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	sensor_common_exit();
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for " SENSOR_NAME " sensor");
MODULE_LICENSE("GPL");
