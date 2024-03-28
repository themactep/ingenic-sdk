// SPDX-License-Identifier: GPL-2.0+
/*
 * rn6752.c
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps      interface        mode
 *   0          720*240         30        mipi           YUV422
 *   1          720*288         25        mipi           YUV422
 *   2          1280*720        25        mipi           YUV422
 *   3          1920*1080       25        mipi           YUV422
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

#define SENSOR_NAME "rn6752"
#define SENSOR_CHIP_ID_H (0x26)
#define SENSOR_CHIP_ID_L (0x01)
#define SENSOR_OUTPUT_MIN_FPS 5
#define MCLK 27000000
#define SENSOR_VERSION "H20221205a"
#define SENSOR_REG_DELAY 0xfd
#define SENSOR_REG_END 0xfa
#define USE_BLUE_SCREEN 1
#define USE_CVBS_ONE_FIELD 1

static int reset_gpio = GPIO_PA(18);
//static int pwdn_gpio = -1;

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

static struct regval_list cvbs_ntsc_video[] = {
	{0xff, 0x08},
	{0x6c, 0x00},
	{0x81, 0x01}, // turn on video decoder
	{0xA3, 0x04},
	{0xDF, 0x0F}, // enable CVBS format
	{0x88, 0x40}, // disable SCLK1 out
	{0xFF, 0x00}, // ch0
	{0x00, 0x00}, // internal use*
	{0x06, 0x08}, // internal use*
	{0x07, 0x63}, // HD format
	{0x2A, 0x81}, // filter control
	{0x3A, 0x24},
	{0x3F, 0x10},
	{0x4C, 0x37}, // equalizer
	{0x4F, 0x00}, // sync control
	{0x50, 0x00}, // D1 resolution
	{0x56, 0x01}, // 72M mode and BT656 mode
	{0x5F, 0x00}, // blank level
	{0x63, 0x75}, // filter control
	{0x59, 0x00}, // extended register access
	{0x5A, 0x00}, // data for extended register
	{0x58, 0x01}, // enable extended register write
	{0x59, 0x33}, // extended register access
	{0x5A, 0x02}, // data for extended register
	{0x58, 0x01}, // enable extended register write
	{0x5B, 0x00}, // H-scaling control
	{0x28, 0xB2}, // cropping
	{0x51, 0xE8},
	{0x52, 0x08},
	{0x53, 0x11},
	{0x5E, 0x08},
	{0x6A, 0x89},
#ifdef USE_CVBS_ONE_FIELD
	{0x20, 0x24},
	{0x23, 0x11},
	{0x24, 0x05},
	{0x25, 0x11},
	{0x26, 0x00},
	{0x42, 0x00},
#elif (defined USE_CVBS_FRAME_MODE)
	{0x3F, 0x90},
	{0x20, 0xA4},
	{0x23, 0x11},
	{0x24, 0xFF},
	{0x25, 0x00},
	{0x26, 0xFF},
	{0x42, 0x00},
#else
	{0x28, 0x92},
#endif
	{0x03, 0x80}, // saturation
	{0x04, 0x80}, // hue
	{0x05, 0x03}, // sharpness
	{0x57, 0x20}, // black/white stretch
	{0x68, 0x32}, // coring
	{0x37, 0x33},
	{0x61, 0x6C},
#ifdef USE_BLUE_SCREEN
	{0x3A, 0x24},
#else
	{0x3A, 0x2C},
	{0x3B, 0x00},
	{0x3C, 0x80},
	{0x3D, 0x80},
#endif
	{0x33, 0x10},        // video in detection
	{0x4A, 0xA8}, // video in detection
	{0x2E, 0x30},        // force no video
	{0x2E, 0x00},        // return to normal

#ifdef USE_DVP
	{0x8E, 0x00},
	{0x8F, 0x80},
	{0x8D, 0x31},
	{0x89, 0x09},
	{0x88, 0x41},
#ifdef USE_BT601
	{0xFF, 0x00},
	{0x56, 0x05},
	{0x3A, 0x24},
	{0x3E, 0x32},
	{0x40, 0x04},
	{0x46, 0x23},
	{0x96, 0x00},
	{0x97, 0x0B},
	{0x98, 0x00},
	{0x9A, 0x40},
	{0x9B, 0xE1},
	{0x9C, 0x00},
#endif

#else
	{0xFF, 0x09},
	{0x00, 0x03},
	{0xFF, 0x08},
	{0x04, 0x03},
	{0x6C, 0x11},
#ifdef USE_MIPI_4LANES
	{0x06, 0x7C},
#else
	{0x06, 0x4C},
#endif
	{0x21, 0x01},
#ifdef USE_MIPI_DOWN_FREQ
	{0x28, 0x02},
	{0x29, 0x02},
	{0x2A, 0x01},
	{0x2B, 0x04},
	{0x32, 0x02},
	{0x33, 0x04},
	{0x34, 0x01},
	{0x35, 0x05},
	{0x36, 0x01},
	{0xAE, 0x09},
	{0x89, 0x0A},
	{0x88, 0x41},
#else
	{0x2B, 0x08},
	{0x34, 0x06},
	{0x35, 0x0B},
#endif
	{0x78, 0x68},
	{0x79, 0x01},
	{0x6C, 0x01},
	{0x04, 0x00},
	{0x20, 0xAA},
#ifdef USE_MIPI_NON_CONTINUOUS_CLOCK
	{0x07, 0x05},
#else
	{0x07, 0x04},
#endif
	{0xFF, 0x0A},
	{0x6C, 0x10},
#endif
	{SENSOR_REG_END, 0x00},
};


static struct regval_list cvbs_pal_video[] = {
	{0xff, 0x08},
	{0x6c, 0x00},
	{0x81, 0x01}, // turn on video decoder
	{0xA3, 0x04},
	{0xDF, 0x0F}, // enable CVBS format
	{0x88, 0x40}, // disable {SCLK1 out
	{0xFF, 0x00}, // ch0
	{0x00, 0x00}, // internal use*
	{0x06, 0x08}, // internal use*
	{0x07, 0x62}, // HD format
	{0x2A, 0x81}, // filter control
	{0x3A, 0x24},
	{0x3F, 0x10},
	{0x4C, 0x37}, // equalizer
	{0x4F, 0x00}, // sync control
	{0x50, 0x00}, // D1 resolution
	{0x56, 0x01}, // 72M mode and BT656 mode
	{0x5F, 0x00}, // blank level
	{0x63, 0x75}, // filter control
	{0x59, 0x00}, // extended register access
	{0x5A, 0x00}, // data for extended register
	{0x58, 0x01}, // enable extended register write
	{0x59, 0x33}, // extended register access
	{0x5A, 0x02}, // data for extended register
	{0x58, 0x01}, // enable extended register write
	{0x5B, 0x00}, // H-scaling control
	{0x28, 0xB2}, // cropping
	{0x53, 0x11},
	{0x54, 0x44},
	{0x55, 0x44},
	{0x5E, 0x08},
	{0x6A, 0x8C},
#ifdef USE_CVBS_ONE_FIELD
	{0x20, 0x24},
	{0x23, 0x17},
	{0x24, 0x37},
	{0x25, 0x17},
	{0x26, 0x00},
	{0x42, 0x00},
#elif (defined USE_CVBS_FRAME_MODE)
	{0x3F, 0x90},
	{0x20, 0xA4},
	{0x23, 0x17},
	{0x24, 0xFF},
	{0x25, 0x00},
	{0x26, 0xFF},
	{0x42, 0x00},
#else
	{0x28, 0x92},
#endif
	{0x03, 0x80}, // saturation
	{0x04, 0x80}, // hue
	{0x05, 0x03}, // sharpness
	{0x57, 0x20}, // black/white stretch
	{0x68, 0x32}, // coring
	{0x37, 0x33},
	{0x61, 0x6C},
#ifdef USE_BLUE_SCREEN
	{0x3A, 0x24},
#else
	{0x3A, 0x2C},
	{0x3B, 0x00},
	{0x3C, 0x80},
	{0x3D, 0x80},
#endif
	{0x33, 0x10}, // video in detection
	{0x4A, 0xA8}, // video in detection
	{0x2E, 0x30}, // force no video
	{0x2E, 0x00}, // return to normal

#ifdef USE_DVP
	{0x8E, 0x00},
	{0x8F, 0x80},
	{0x8D, 0x31},
	{0x89, 0x09},
	{0x88, 0x41},
#ifdef USE_BT601
	{0xFF, 0x00},
	{0x56, 0x05},
	{0x3A, 0x24},
	{0x3E, 0x32},
	{0x40, 0x04},
	{0x46, 0x23},
	{0x96, 0x00},
	{0x97, 0x0B},
	{0x98, 0x00},
	{0x9A, 0x40},
	{0x9B, 0xE1},
	{0x9C, 0x00},
#endif

#else
	{0xFF, 0x09},
	{0x00, 0x03},
	{0xFF, 0x08},
	{0x04, 0x03},
	{0x6C, 0x11},
#ifdef USE_MIPI_4LANES
	{0x06, 0x7C},
#else
	{0x06, 0x4C},
#endif
	{0x21, 0x01},
#ifdef USE_MIPI_DOWN_FREQ
	{0x28, 0x02},
	{0x29, 0x02},
	{0x2A, 0x01},
	{0x2B, 0x04},
	{0x32, 0x02},
	{0x33, 0x04},
	{0x34, 0x01},
	{0x35, 0x05},
	{0x36, 0x01},
	{0xAE, 0x09},
	{0x89, 0x0A},
	{0x88, 0x41},
#else
	{0x2B, 0x08},
	{0x34, 0x06},
	{0x35, 0x0B},
#endif
	{0x78, 0x68},
	{0x79, 0x01},
	{0x6C, 0x01},
	{0x04, 0x00},
	{0x20, 0xAA},
#ifdef USE_MIPI_NON_CONTINUOUS_CLOCK
	{0x07, 0x05},
#else
	{0x07, 0x04},
#endif
	{0xFF, 0x0A},
	{0x6C, 0x10},
#endif

	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1280_720_25fps_mipi[] = {
	{0x81, 0x01},
	{0xa3, 0x04},
	{0xdf, 0xfe},
	{0x88, 0x40},

	{0xff, 0x00},
	{0x00, 0x20},//20->80彩条模式
	{0x06, 0x08},
	{0x07, 0x63},
	{0x2a, 0x01},
	{0x3a, 0x24},
	{0x3f, 0x10},
	{0x4c, 0x37},
	{0x4f, 0x03},
	{0x50, 0x02},
	{0x56, 0x01},
	{0x5f, 0x40},
	{0x63, 0xf5},
	{0x59, 0x00},
	{0x5a, 0x42},
	{0x58, 0x01},
	{0x59, 0x33},
	{0x5a, 0x02},
	{0x58, 0x01},
	{0x51, 0xe1},
	{0x52, 0x88},
	{0x53, 0x12},
	{0x5b, 0x07},
	{0x5e, 0x08},
	{0x6a, 0x82},
	{0x28, 0x92},
	{0x03, 0x80},
	{0x04, 0x80},
	{0x05, 0x00},
	{0x57, 0x23},
	{0x68, 0x32},
	{0x37, 0x33},
	{0x61, 0x6c},
#ifdef USE_BLUE_SCREEN
	{0x3a, 0x24},
#else
	{0x3a,0x2c},
	{0x3b,0x00},
	{0x3c,0x80},
	{0x3d,0x80},
#endif
	{0x33, 0x10},
	{0x4a, 0xa8},
	{0x2e, 0x30},
	{0x2e, 0x00},
#ifdef USE_DVP
	{0x8e,0x00},
	{0x8f,0x80},
	{0x8d,0x31},
	{0x89,0x09},
	{0x88,0x41},
#ifdef USE_BT601
	{0xff,0x00},
	{0x56,0x05},
	{0x3a,0x24},
	{0x3e,0x32},
	{0x40,0x04},
	{0x46,0x23},
	{0x96,0x00},
	{0x97,0x0b},
	{0x98,0x00},
	{0x9a,0x40},
	{0x9b,0xe1},
	{0x9c,0x00},
#endif
#else
	{0xff, 0x09},
	{0x00, 0x03},
	{0xff, 0x08},
	{0x04, 0x03},
	{0x6c, 0x11},
	{0x2b, 0x08},
	{0x34, 0x06},
	{0x35, 0x0b},
#ifdef USE_MIPI_4LANES
	{0x06,0x7c},
#ifdef USE_MIPI_DOWN_FREQ
	{0x28,0x02},
	{0x29,0x02},
	{0x2a,0x01},
	{0x2b,0x04},
	{0x32,0x02},
	{0x33,0x04},
	{0x34,0x01},
	{0x35,0x05},
	{0x36,0x01},
	{0xae,0x09},
	{0x89,0x0a},
	{0x88,0x41},
#endif
#else
	{0x06, 0x4c},
#endif
	{0x21, 0x01},
	{0x78, 0x80},
	{0x79, 0x02},
	{0x6c, 0x01},
	{0x04, 0x00},
	{0x20, 0xaa},
#ifdef USE_MIPI_NON_CONTINUOUS_CLOCK
	{0x07,0x05},
#else
	{0x07, 0x04},
#endif
	{0xff, 0x0a},
	{0x6c, 0x10},
#endif
	{SENSOR_REG_END, 0x00},
};

static struct regval_list FHD_1080P25_video[] = {
	{0x81, 0x01}, // turn on video decoder
	{0xA3, 0x04}, //
	{0xDF, 0xFE}, // enable HD format
	{0xF0, 0xC0},
	{0x88, 0x40}, // disable SCLK1 out
	{0xFF, 0x00},
	{0x00, 0x20}, // internal use*
	{0x06, 0x08}, // internal use*
	{0x07, 0x63}, // HD format
	{0x2A, 0x01}, // filter control
	{0x3A, 0x24},
	{0x3F, 0x10},
	{0x4C, 0x37}, // equalizer
	{0x4F, 0x03}, // sync control
	{0x50, 0x03}, // 1080p resolution
	{0x56, 0x02}, // 144M and BT656 mode
	{0x5F, 0x44}, // blank level
	{0x63, 0xF8}, // filter control
	{0x59, 0x00}, // extended register access
	{0x5A, 0x48}, // data for extended register
	{0x58, 0x01}, // enable extended register write
	{0x59, 0x33}, // extended register access
	{0x5A, 0x23}, // data for extended register
	{0x58, 0x01}, // enable extended register write
	{0x51, 0xF4}, // scale factor1
	{0x52, 0x29}, // scale factor2
	{0x53, 0x15}, // scale factor3
	{0x5B, 0x01}, // H-scaling control
	{0x5E, 0x08}, // enable H-scaling control
	{0x6A, 0x87}, // H-scaling control
	{0x28, 0x92}, // cropping
	{0x03, 0x80}, // saturation
	{0x04, 0x80}, // hue
	{0x05, 0x04}, // sharpness
	{0x57, 0x23}, // black/white stretch
	{0x68, 0x00}, // coring
	{0x37, 0x33},
	{0x61, 0x6C},
#ifdef USE_BLUE_SCREEN
	{0x3A, 0x24},
#else
	{0x3A, 0x2C},
	{0x3B, 0x00},
	{0x3C, 0x80},
	{0x3D, 0x80},
#endif
	{0x33, 0x10},        // video in detection
	{0x4A, 0xA8}, // video in detection
	{0x2E, 0x30},        // force no video
	{0x2E, 0x00},        // return to normal

#ifdef USE_DVP
	{0x8E, 0x00},
	{0x8F, 0x80},
	{0x8D, 0x31},
	{0x89, 0x0A},
	{0x88, 0x41},
#ifdef USE_BT601
	{0xFF, 0x00},
	{0x56, 0x06},
	{0x3A, 0x24},
	{0x3E, 0x32},
	{0x40, 0x04},
	{0x46, 0x23},
	{0x96, 0x00},
	{0x97, 0x0B},
	{0x98, 0x00},
	{0x9A, 0x40},
	{0x9B, 0xE1},
	{0x9C, 0x00},
#endif

#else
	{0xFF, 0x09},
	{0x00, 0x03},
	{0xFF, 0x08},
	{0x04, 0x03},
	{0x6C, 0x11},
#ifdef USE_MIPI_4LANES
	{0x06, 0x7C},
#else
	{0x06, 0x4C},
#endif
	{0x21, 0x01},
	{0x34, 0x06},
	{0x35, 0x0B},
	{0x78, 0xC0},
	{0x79, 0x03},
	{0x6C, 0x01},
	{0x04, 0x00},
	{0x20, 0xAA},
#ifdef USE_MIPI_NON_CONTINUOUS_CLOCK
	{0x07, 0x05},
#else
	{0x07, 0x04},
#endif
	{0xFF, 0x0A},
	{0x6C, 0x10},
#endif
	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution]. */

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 720,
		.height = 240,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_UYVY8_2X8,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = cvbs_ntsc_video,
	},
	{
		.width = 720,
		.height = 288,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_UYVY8_2X8,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = cvbs_pal_video,
	},
	{
		.width = 1280,
		.height = 720,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_YUYV8_2X8,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1280_720_25fps_mipi,
	},
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_UYVY8_2X8,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = FHD_1080P25_video,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 648,
	.lans = 2,
	.index = 0,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_YUV422,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 1,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.image_twidth = 720,
	.image_theight = 240,
	.mipi_sc.mipi_crop_start0x = 0,
	.mipi_sc.mipi_crop_start0y = 0,
	.mipi_sc.mipi_crop_start1x = 0,
	.mipi_sc.mipi_crop_start1y = 0,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = YUV422_8BIT,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = 0X2c4c,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x2c,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

int sensor_read(struct tx_isp_subdev *sd, unsigned char reg,
		unsigned char *value) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
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
		}
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct tx_isp_subdev *sd, unsigned char reg,
		 unsigned char value) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};

//	printk("wangtg______>%s addr:0x%x reg:0x%x value:%d", __func__, client->addr, reg, value);

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
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
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
//	unsigned char val;

	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sensor_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
//		ret = sensor_read(sd, vals->reg_num, &val);
//		printk("	{0x%x,0x%x}\n",vals->reg_num, val);
		vals++;
	}

	return 0;
}

static int sensor_attr_check(struct tx_isp_subdev *sd) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	unsigned long rate;
	int ret;

	switch (info->default_boot) {
		case 0:
			wsize = &sensor_win_sizes[0];
			memcpy((void *) (&(sensor_attr.mipi)), (void *) (&sensor_mipi), sizeof(sensor_mipi));
			sensor_attr.total_width = 720;
			sensor_attr.total_height = 240;
			sensor_attr.max_integration_time = 240;
			sensor_attr.integration_time_limit = 240;
			sensor_attr.max_integration_time_native = 240;
			sensor_attr.mipi.image_twidth = 720,
			sensor_attr.mipi.image_theight = 240,
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			//memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi),sizeof(sensor_mipi));
			break;
		case 1:
			wsize = &sensor_win_sizes[1];
			memcpy((void *) (&(sensor_attr.mipi)), (void *) (&sensor_mipi), sizeof(sensor_mipi));
			sensor_attr.total_width = 720;
			sensor_attr.total_height = 288;
			sensor_attr.mipi.image_twidth = 720,
			sensor_attr.mipi.image_theight = 288,
			sensor_attr.max_integration_time = 288;
			sensor_attr.integration_time_limit = 288;
			sensor_attr.max_integration_time_native = 288;
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			//memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi),sizeof(sensor_mipi));
			break;
		case 2:
			wsize = &sensor_win_sizes[2];
			memcpy((void *) (&(sensor_attr.mipi)), (void *) (&sensor_mipi), sizeof(sensor_mipi));
			sensor_attr.total_width = 1280;
			sensor_attr.total_height = 720;
			sensor_attr.max_integration_time = 720;
			sensor_attr.integration_time_limit = 720;
			sensor_attr.max_integration_time_native = 720;
			sensor_attr.mipi.image_twidth = 1280,
			sensor_attr.mipi.image_theight = 720,
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			break;
		case 3:
			wsize = &sensor_win_sizes[3];
			memcpy((void *) (&(sensor_attr.mipi)), (void *) (&sensor_mipi), sizeof(sensor_mipi));
			sensor_attr.total_width = 1920;
			sensor_attr.total_height = 1080;
			sensor_attr.max_integration_time = 1080;
			sensor_attr.integration_time_limit = 1080;
			sensor_attr.max_integration_time_native = 1080;
			sensor_attr.mipi.image_twidth = 1920,
			sensor_attr.mipi.image_theight = 1080,
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			// memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi),sizeof(sensor_mipi));
			break;
		default:
			ISP_ERROR("Have no this setting!!!\n");
			break;
	}

	switch (info->video_interface) {
		case TISP_SENSOR_VI_MIPI_CSI0:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.mipi.index = 0;
			break;
		case TISP_SENSOR_VI_DVP:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
			break;
		default:
			ISP_ERROR("Have no this Interface Source!!!\n");
	}

	switch (info->mclk) {
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

	ISP_WARNING("[default_boot=%d] [resolution=%dx%d] [video_interface=%s] [MCLK=%d] \n",
		    info->default_boot, wsize->width, wsize->height, (info->video_interface ? "DVP" : "MIPI"),
		    info->mclk);
	reset_gpio = info->rst_gpio;
	//pwdn_gpio = info->pwdn_gpio;

	reset_gpio = info->rst_gpio;
	//pwdn_gpio = info->pwdn_gpio;
	private_clk_prepare_enable(sensor->mclk);
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	sensor->priv = wsize;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident) {
	unsigned char v;
//	unsigned char val;
	int ret;
	ret = sensor_write(sd, 0xFF, 0x01);
	if (ret < 0) {
		printk("ret = %d\n", ret);
	}
	msleep(10);
	ret = sensor_read(sd, 0xFE, &v);
//	printk("ret = %d, v = 0x%02x\n", ret, v);
	if (ret < 0 || v != SENSOR_CHIP_ID_H)
		return ret;
	*ident = v;
	ret = sensor_read(sd, 0xFD, &v);
//	printk("ret = %d, v = 0x%02x\n", ret, v);
	if (ret < 0 || v != SENSOR_CHIP_ID_L)
		return ret;
	*ident = (*ident << 8) | v;
#if 0 /*读取sensor的制式*/
	sensor_write(sd, 0xff, 0x00);
	sensor_read(sd, 0xff, &val);
	printk("[0x%x, 0x%x]\n",0xff, val);
	sensor_read(sd, 0x00, &val);
	printk("[0x%x, 0x%x]\n",0x00, val);
#endif
	return 0;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	info->rst_gpio = reset_gpio;
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "sensor_reset");
		if (!ret) {
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(50);
			private_gpio_direction_output(info->rst_gpio, 0);
			private_msleep(35);
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(35);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
		}
	}
#if 0
	if (info->pwdn_gpio != -1) {
		ret = private_gpio_request(info->pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(150);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",info->pwdn_gpio);
		}
	}
#endif
	ret = sensor_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an %s chip.\n",
			  client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}
	ISP_WARNING("%s chip found @ 0x%02x (%s)\n",
		    SENSOR_NAME, client->addr, client->adapter->name);
//	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable) {
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		return ISP_SUCCESS;
	}

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	sensor->video.state = TX_ISP_MODULE_DEINIT;
	ret = sensor_write_array(sd, wsize->regs);
	sensor_write(sd, 0xff, 0x08),
		sensor_write(sd, 0x6c, 0x01),
		private_msleep(1000);
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg) {
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN)) {
		return -EPERM;
	}
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

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
			sensor->video.state = TX_ISP_MODULE_INIT;
		} else {
			ISP_WARNING("init failed, state error!\n");
		}

/**		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sensor_write_array(sd, wsize->regs);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
	*/
		ISP_WARNING("%s stream on\n", SENSOR_NAME);
		sensor->video.state = TX_ISP_MODULE_RUNNING;
		private_msleep(1000);
		sensor_write(sd, 0xff, 0x00);
#if 0
		unsigned char val;
		sensor_read(sd, 0x28, &val);
		printk("[0x%x, 0x%x]\n",0x28, val);
		sensor_read(sd, 0x20, &val);
		printk("[0x%x, 0x%x]\n",0x20, val);
		sensor_read(sd, 0x23, &val);
		printk("[0x%x, 0x%x]\n",0x23, val);
		sensor_read(sd, 0x24, &val);
		printk("[0x%x, 0x%x]\n",0x24, val);
		sensor_read(sd, 0x25, &val);
		printk("[0x%x, 0x%x]\n",0x25, val);
		sensor_read(sd, 0x26, &val);
		printk("[0x%x, 0x%x]\n",0x26, val);
		sensor_read(sd, 0x42, &val);
		printk("[0x%x, 0x%x]\n",0x42, val);
#endif
	} else {
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}
	return ret;
}

static struct tx_isp_subdev_video_ops sensor_video_ops = {
	.s_stream = sensor_s_stream,
};

static int sensor_set_mode(struct tx_isp_subdev *sd, int value) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
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


static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg) {
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd) {
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if (arg)
				ret = sensor_set_mode(sd, sensor_val->value);
			break;
		default:
			break;
	}

	return ret;
}


static struct tx_isp_subdev_sensor_ops sensor_sensor_ops = {
	.ioctl = sensor_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.sensor = &sensor_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
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

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id) {

	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *) kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->dev = &client->dev;
	sensor->video.attr = &sensor_attr;
	sensor_attr.expo_fs = 1;
	sensor->dev = &client->dev;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;

	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	ISP_WARNING("rn6752 probe ok\n");
	return 0;
}

static int sensor_remove(struct i2c_client *client) {
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;

	if (info->rst_gpio != -1)
		private_gpio_free(info->rst_gpio);
	if (info->pwdn_gpio != -1)
		private_gpio_free(info->pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	private_devm_clk_put(&client->dev, sensor->mclk);
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


static __init int init_sensor(void) {
	/* ret = private_driver_get_interface(); */
	/* if (ret) { */
	/* 	ISP_ERROR("Failed to init %s driver.\n", SENSOR_NAME); */
	/* 	return -1; */
	/* } */
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
