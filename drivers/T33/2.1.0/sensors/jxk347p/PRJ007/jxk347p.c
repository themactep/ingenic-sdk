/*
 * jxk347p.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps    dvdd
 *   0          2880x1620       30          MIPI        Linear   10      30
 * @I2C addr:
 *
 * @FSync:
 *
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
#include <tx-isp-fast.h>

#define TVERSION "V20241115a"
#define SENSOR_VERSION  "H20250327a"

// #define SENSOR_TEST

#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x68)
#define SENSOR_CHIP_ID_M    (0x08)
#define SENSOR_CHIP_ID_L    (0x01)
#define SENSOR_OUTPUT_MIN_FPS 1
#define SENSOR_MCLK 24000000

#ifdef CONFIG_ZERATUL
#define ZRT_SENSOR_WITHOUT_INIT
#endif

#ifdef SENSOR_WDR_2_FRAME
static int wdr_line = xxx;
#endif

#ifndef SENSOR_I2C_REG_8BIT
#define SENSOR_I2C_REG_16BIT
#endif /* SENSOR_I2C_REG_8BIT */
#ifdef SENSOR_I2C_REG_8BIT
#define SENSOR_REG_END    0xff
#define SENSOR_REG_DELAY  0xfe
#endif /* SENSOR_I2C_REG_8BIT */
#ifdef SENSOR_I2C_REG_16BIT
#define SENSOR_REG_END    0xffff
#define SENSOR_REG_DELAY  0xfffe
#endif /* SENSOR_I2C_REG_16BIT */

struct regval_list {
#ifdef SENSOR_I2C_REG_8BIT
	uint8_t reg_num;
#endif /* SENSOR_I2C_REG_8BIT */
#ifdef SENSOR_I2C_REG_16BIT
	uint16_t reg_num;
#endif /* SENSOR_I2C_REG_16BIT */
	uint8_t value;
};

struct tx_isp_sensor_attribute jxk347p_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut jxk347p_again_lut[] = {
	{0x00, 0},
	{0x01, 5731},
	{0x02, 11136},
	{0x03, 16248},
	{0x04, 21097},
	{0x05, 25710},
	{0x06, 30109},
	{0x07, 34312},
	{0x08, 38336},
	{0x09, 42195},
	{0x0a, 45904},
	{0x0b, 49472},
	{0x0c, 52910},
	{0x0d, 56228},
	{0x0e, 59433},
	{0x0f, 62534},
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
	{0x40, 262144},
	{0x41, 267875},
	{0x42, 273280},
	{0x43, 278392},
	{0x44, 283241},
	{0x45, 287854},
	{0x46, 292253},
	{0x47, 296456},
	{0x48, 300480},
	{0x49, 304339},
	{0x4a, 308048},
	{0x4b, 311616},
	{0x4c, 315054},
	{0x4d, 318372},
	{0x4e, 321577},
	{0x4f, 324678},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int jxk347p_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = jxk347p_again_lut;
	while (lut->gain <= jxk347p_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == jxk347p_attr.again.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

#else
	/* Non analog gain table */
	...;
#endif  /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

	return isp_gain;
}

#ifdef SENSOR_WDR_2_FRAME
unsigned int jxk347p_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = jxk347p_again_lut;
	while(lut->gain <= jxk347p_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == jxk347p_attr.again_short.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	...;
#else
	/* Non analog gain table */

	...;
#endif /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

	return isp_gain;
}
#endif /* SENSOR_WDR_2_FRAME */

unsigned int jxk347p_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int jxk347p_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int jxk347p_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus jxk347p_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 532,
	.lans = 2,
	.image_twidth = 2880,
	.image_theight = 1620,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
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

struct tx_isp_dvp_bus jxk347p_dvp = {
	.gpio = DVP_PA_LOW_10BIT,
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking = {
		.hblanking = 0,
		.vblanking = 0,
	},
	.polar = {
		.hsync_polar = 0,
		.vsync_polar = 0,
		.pclk_polar = 0,
	},
	.dvp_hcomp_en = 0,
};

struct tx_isp_sensor_attribute jxk347p_attr = {
	.name = "jxk347p",
	.chip_id = 0x0868,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x40,
	.sensor_ctrl.alloc_again = jxk347p_alloc_again,
	.sensor_ctrl.alloc_dgain = jxk347p_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = jxk347p_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list jxk347p_init_regs_2880_1620_30fps_mipi[] = {
	{0x12, 0x50},
	{0x48, 0xA3},
	{0x48, 0x23},
	{0x0E, 0x11},
	{0x0F, 0x0C},
	{0x10, 0x48},
	{0x11, 0x80},
	{0x46, 0x18},
	{0x0D, 0xA0},
	{0x57, 0x60},
	{0x58, 0x18},
	{0x5F, 0x41},
	{0x60, 0x24},
	{0xB6, 0x00},
	{0x20, 0x64},//hts = 0x364 = 868
	{0x21, 0x03},//
	{0x22, 0x7C},//vts = 0x67c = 1660
	{0x23, 0x06},//
	{0x24, 0xD0},
	{0x25, 0x54},
	{0x26, 0x62},
	{0x27, 0x5B},
	{0x28, 0x15},
	{0x29, 0x03},
	{0x2A, 0x54},
	{0x2B, 0x13},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x9B},
	{0x2F, 0x04},
	{0x41, 0xA4},
	{0x42, 0x05},
	{0x47, 0x46},
	{0x76, 0x10},
	{0x77, 0x0E},
	{0x80, 0x01},
	{0xAF, 0x12},
	{0x9B, 0x8F},
	{0xAA, 0x00},
	{0x8D, 0x6F},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x9E, 0xF8},
	{0x0C, 0x70},
	{0x6F, 0x00},
	{0x6E, 0x2C},
	{0x70, 0xF9},
	{0x71, 0xBB},
	{0x72, 0xD3},
	{0x73, 0x7A},
	{0x74, 0x02},
	{0x78, 0x9B},
	{0x89, 0x01},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0xB0, 0xC4},
	{0x08, 0x22},
	{0xB1, 0x45},
	{0x30, 0x8D},
	{0x31, 0x0C},
	{0x32, 0x30},
	{0x33, 0x60},
	{0x34, 0x38},
	{0x35, 0x38},
	{0x3A, 0xAF},
	{0x45, 0x09},
	{0x56, 0x12},
	{0x59, 0x38},
	{0x61, 0x18},
	{0x64, 0xE0},
	{0x85, 0x24},
	{0x93, 0x20},
	{0x94, 0xE0},
	{0xA7, 0x80},
	{0xA9, 0x48},
	{0xBF, 0x01},
	{0x5E, 0x90},
	{0x5F, 0x40},
	{0x79, 0x82},
	{0x7F, 0x17},
	{0xBF, 0x00},
	{0x5A, 0x04},
	{0x5B, 0xA0},
	{0x5C, 0x40},
	{0x5D, 0x22},
	{0x5E, 0x80},
	{0x62, 0xC0},
	{0x63, 0x09},
	{0x66, 0x83},
	{0x67, 0x43},
	{0x68, 0x00},
	{0x69, 0x74},
	{0x6A, 0x25},
	{0x7A, 0x33},
	{0x8F, 0x94},
	{0xA5, 0x67},
	{0xB8, 0x80},
	{0xB9, 0x02},
	{0xBA, 0x81},
	{0xBF, 0x01},
	{0x67, 0xB0},
	{0xBF, 0x00},
	{0x97, 0x20},
	{0x13, 0x81},
	{0x4A, 0x01},
	{0x50, 0x02},
	{0x49, 0x40},
	{0xBF, 0x01},
	{0x75, 0xC0},
	{0x76, 0x32},
	{0x74, 0x01},
	{0xBF, 0x00},
	{0x7E, 0x4C},
	{0x7D, 0xFF},
	{0x7F, 0xD6},
	{0x8C, 0xFF},
	{0x8E, 0x00},
	{0xBF, 0x01},
	{0x5E, 0x10},
	{0xBF, 0x00},
	{0xBC, 0x11},
	{0x65, 0x32},
	{0x82, 0x00},
	{0x19, 0x20},
	{0xBF, 0x01},
	{0x58, 0x00},
	{0xBF, 0x00},
	{0xA3, 0x11},
	{0xB5, 0xC4},
	{0x12, 0x00},//0x10
	{0x48, 0xA3},
	{0x48, 0x23},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the jxk347p_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting jxk347p_win_sizes[] = {
	/* 2560*1440 [0] */
	{
		.width          = 2880,
		.height         = 1620,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = jxk347p_init_regs_2880_1620_30fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &jxk347p_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list jxk347p_stream_on_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list jxk347p_stream_off_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int jxk347p_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int jxk347p_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int jxk347p_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxk347p_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int jxk347p_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxk347p_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int jxk347p_read(struct tx_isp_subdev *sd, uint16_t reg,
		  unsigned char *value) {
	int ret;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr   = client->addr,
			.flags  = 0,
			.len    = 2,
			.buf    = buf,
		},
		[1] = {
			.addr   = client->addr,
			.flags  = I2C_M_RD,
			.len    = 1,
			.buf    = value,
		}
	};

	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int jxk347p_write(struct tx_isp_subdev *sd, uint16_t reg,
		   unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr   = client->addr,
		.flags  = 0,
		.len    = 3,
		.buf    = buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 0
static int jxk347p_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxk347p_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int jxk347p_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxk347p_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int jxk347p_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = ISP_SUCCESS;

	rate = private_clk_get_rate(sensor->mclk);
	if(((rate / 1000) % (mclk / 1000)) != 0) {
		ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
		sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
		if (IS_ERR(sclka)) {
			pr_err("get sclka failed\n");
		} else {
			rate = private_clk_get_rate(sclka);
			if(((rate / 1000) % (mclk / 1000)) != 0) {
				switch(mclk) {
				case 24000000:
					private_clk_set_rate(sclka, 1200000000);
					break;
				case 27000000:
				case 37125000:
					private_clk_set_rate(sclka, 1188000000);
					break;
				default:
					ret = -1;
					goto error;
				}
			} else {
				if ((mclk != 24000000) && (mclk != 27000000) && (mclk != 37125000)) {
					ret = -1;
					goto error;
				}
			}
		}
	}

	private_clk_set_rate(sensor->mclk, mclk);
	private_clk_prepare_enable(sensor->mclk);

error:
	return ret;
}

static int jxk347p_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;

	sensor->video.fps.min = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

#ifdef SENSOR_WDR_2_FRAME
	sensor->video.wdr = 1;
#else
	sensor->video.wdr = 0;
#endif

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int jxk347p_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &jxk347p_win_sizes[0];
		jxk347p_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		jxk347p_attr.again.value = 0x64;
		jxk347p_attr.again.max = 324678;
		jxk347p_attr.again.min = 0;
		jxk347p_attr.again.apply_delay = 2;

		jxk347p_attr.integration_time.value = 1660 - 4 ;
		jxk347p_attr.integration_time.max = 1660 - 4;
		jxk347p_attr.integration_time.min = 1;
		jxk347p_attr.integration_time.apply_delay = 2;

		jxk347p_attr.total_width = 3472;
		jxk347p_attr.total_height = 1660;

		jxk347p_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		jxk347p_attr.hcg.base_gain = ;
		jxk347p_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		jxk347p_attr.again_short.value = ;
		jxk347p_attr.again_short.max = ;
		jxk347p_attr.again_short.min = ;
		jxk347p_attr.again_short.apply_delay = ;

		jxk347p_attr.integration_time_short.value = ;
		jxk347p_attr.integration_time_short.max = ;
		jxk347p_attr.integration_time_short.min = ;
		jxk347p_attr.integration_time_short.apply_delay = ;

		jxk347p_attr.wdr_cache = wdr_line * jxk347p_attr.total_width;

#ifdef SENSOR_HCG
		jxk347p_attr.hcg_short.base_gain = ;
		jxk347p_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(jxk347p_attr.mipi)), (void *)(&jxk347p_mipi_linear), sizeof(jxk347p_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int jxk347p_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	jxk347p_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		jxk347p_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		jxk347p_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		jxk347p_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		jxk347p_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		jxk347p_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this interface!!!\n");
	}

#ifndef ZRT_SENSOR_WITHOUT_INIT
	switch (info->mclk) {
	case TISP_SENSOR_MCLK0:
		sclka = private_devm_clk_get(&client->dev, SEN_MCLK);
		sensor->mclk = private_devm_clk_get(sensor->dev, SEN_BCLK);
		set_sensor_mclk_function(0);
		break;
	case TISP_SENSOR_MCLK1:
		sclka = private_devm_clk_get(&client->dev, SEN_MCLK);
		sensor->mclk = private_devm_clk_get(sensor->dev, SEN_BCLK);
		set_sensor_mclk_function(1);
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	ret = jxk347p_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.apply_delay = 1;
	jxk347p_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int jxk347p_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = jxk347p_read(sd, 0x0b, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = jxk347p_read(sd, 0x0a, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_M)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	ret = jxk347p_read(sd, 0x09, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = v;

	return 0;
}

static int jxk347p_g_chip_ident(struct tx_isp_subdev *sd,
				 struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	jxk347p_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "jxk347p_reset");
		if (!ret) {
			private_gpio_direction_output(info->rst_gpio, 0);
			private_msleep(100);
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(100);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
		}
	}
	if (info->pwdn_gpio != -1) {
		ret = private_gpio_request(info->pwdn_gpio, "jxk347p_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = jxk347p_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an jxk347p chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("jxk347p version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", jxk347p_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
		    info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "jxk347p", sizeof("jxk347p"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int jxk347p_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int jxk347p_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (!init->enable) {
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		return ISP_SUCCESS;
	} else {
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.fps.value = wsize->fps;
		sensor->video.fps.max = wsize->fps;
		sensor->video.fps.apply_delay = 1;
		ret = jxk347p_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int jxk347p_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = ISP_SUCCESS;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = jxk347p_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int jxk347p_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	jxk347p_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int jxk347p_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = jxk347p_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = jxk347p_write_array(sd, jxk347p_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("jxk347p stream on\n");
		}

	} else {
		ret = jxk347p_write_array(sd, jxk347p_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("jxk347p stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int jxk347p_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret = jxk347p_write(sd,  0x01, (unsigned char)(it & 0xff));
	ret += jxk347p_write(sd, 0x02, (unsigned char)((it >> 8) & 0xff));
	ret += jxk347p_write(sd, 0x00, (unsigned char)(again & 0x7f));

	return ret;
}
#else
static int jxk347p_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int jxk347p_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int jxk347p_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxk347p_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxk347p_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = jxk347p_attr_set(sd, wsize);
	}

	return ret;
}

static int jxk347p_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0;
	int ret = ISP_SUCCESS;

	switch (info->default_boot) {
	case 0:
		sclk = 3472 * 1660 * 30;  /**< HTS * VTS * FPS */
		max_fps = 30;
		break;
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d/%d) no in range\n", fps >> 16, fps & 0xffff);
		return -1;
	}

	val = 0;
	ret += jxk347p_read(sd, 0x21, &val);
	hts = val;
	val = 0;
	ret += jxk347p_read(sd, 0x20, &val);
	hts = ((hts << 8) | val) << 2;
	if (0 != ret) {
		ISP_ERROR("err: jxk347p read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	jxk347p_write(sd, 0x22, (unsigned char) (vts & 0xff));
	jxk347p_write(sd, 0x23, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: jxk347p_write err\n");
		return ret;
	}

	sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->integration_time.max = vts - 4;
#else
	/* WDR mode */
	switch (info->default_boot) {
	case 0:
		sensor->video.attr->total_height = vts;
		sensor->video.attr->integration_time.max = vts - xx;
		break;
	case 1:
		sensor->video.attr->total_height = vts;
		sensor->video.attr->integration_time.max = vts - xx;
		sensor->video.attr->integration_time_short.max = vts - xx;
		break;
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}
#endif /* SENSOR_WDR_2_FRAME */

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	if (ret) {
		ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
	}

	return ret;
}

static int jxk347p_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

	par->drop_frame = 0;
	par->reset = 0;

	ret = jxk347p_read(sd, 0x12, &val);
	/* 2'b01:mirror,2'b10:filp */
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		/* sensor->video.mbus.code = wsize->mbus_code; */
		val &= 0xCF;
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		/* sensor->video.mbus.code = wsize->mbus_code; */
		val = (val & 0xCF) | 0x10;
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		/* sensor->video.mbus.code = wsize->mbus_code; */
		val = (val & 0xCF) | 0x20;
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		/* sensor->video.mbus.code = wsize->mbus_code; */
		val |= 0x30;
		break;
	}
	ret = jxk347p_write(sd, 0x12, val);

	sensor->video.hvflip_mode = par->hvflip;
	jxk347p_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int jxk347p_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int jxk347p_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int jxk347p_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int jxk347p_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = jxk347p_write_array(sd, jxk347p_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		jxk347p_setting_select(sd, 1);
		jxk347p_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		jxk347p_setting_select(sd, 0);
		jxk347p_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int jxk347p_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = jxk347p_write_array(sd, wsize->regs);
	ret = jxk347p_write_array(sd, jxk347p_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int jxk347p_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}

	switch (cmd) {
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = jxk347p_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = jxk347p_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = jxk347p_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = jxk347p_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = jxk347p_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = jxk347p_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = jxk347p_write_array(sd, jxk347p_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = jxk347p_write_array(sd, jxk347p_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = jxk347p_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = jxk347p_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = jxk347p_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = jxk347p_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = jxk347p_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = jxk347p_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = jxk347p_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops jxk347p_core_ops = {
	.g_chip_ident = jxk347p_g_chip_ident,
	.reset = jxk347p_reset,
	.init = jxk347p_init,
	.g_register = jxk347p_g_register,
	.s_register = jxk347p_s_register,
};

static struct tx_isp_subdev_video_ops jxk347p_video_ops = {
	.s_stream = jxk347p_s_stream,
};

static struct tx_isp_subdev_sensor_ops jxk347p_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = jxk347p_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops jxk347p_ops = {
	.core = &jxk347p_core_ops,
	.video = &jxk347p_video_ops,
	.sensor = &jxk347p_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "jxk347p",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int jxk347p_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
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

#ifdef SENSOR_MIR_FLIP
	sensor->video.hvflip_mode = TX_ISP_SENSOR_HVFLIP_NOMAL;
#endif /* SENSOR_MIR_FLIP */
	sensor->video.attr = &jxk347p_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &jxk347p_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->jxk347p\n");

	return 0;
}

static int jxk347p_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;

	if (info->rst_gpio != -1)
		private_gpio_free(info->rst_gpio);
	if (info->pwdn_gpio != -1)
		private_gpio_free(info->pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	tx_isp_subdev_deinit(sd);

	kfree(sensor);

	return 0;
}

static const struct i2c_device_id jxk347p_id[] = {
	{"jxk347p", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, jxk347p_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver jxk347p_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "jxk347p",
	},
	.probe          = jxk347p_probe,
	.remove         = jxk347p_remove,
	.id_table       = jxk347p_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_jxk347p(void) {
	return private_i2c_add_driver(&jxk347p_driver);
}

static __exit void exit_jxk347p(void) {
	private_i2c_del_driver(&jxk347p_driver);
}

module_init(init_jxk347p);
module_exit(exit_jxk347p);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_jxk347p(void) {
	return private_i2c_add_driver(&jxk347p_driver);
}

static void exit_first_jxk347p(void) {
	private_i2c_del_driver(&jxk347p_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "jxk347p",
	.i2c_addr = 0x40,
	.width = 2880,
	.height =  1620,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_jxk347p,
	.exit_sensor = exit_first_jxk347p
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for jxk347p sensor");
MODULE_LICENSE("GPL");
