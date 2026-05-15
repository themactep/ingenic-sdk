/*
* jxk308p.c
*
* Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* Settings:
* sboot        resolution      fps       interface              mode
*   0          3840*2160        15       mipi_2lane           linear
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

#define TVERSION "V20231226a"
#define SENSOR_VERSION  "H20250218a"

// #define SENSOR_TEST

#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */

#define SENSOR_CHIP_ID_H    (0x08)
#define SENSOR_CHIP_ID_L    (0x47)
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_MCLK 27000000

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

struct tx_isp_sensor_attribute jxk308p_attr;

/*
* The part of driver maybe modify about different sensor and different board.
*/
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut jxk308p_again_lut[] = {
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
	{0xc, 49471},
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
#endif /* SENSOR_AGAIN_TABLE */

unsigned int jxk308p_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = jxk308p_again_lut;
	while (lut->gain <= jxk308p_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == jxk308p_attr.max_again) && (isp_gain >= lut->gain)) {
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
unsigned int jxk308p_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
		/* Analog gain table */
		struct again_lut *lut = jxk308p_again_lut;
		while(lut->gain <= jxk308p_attr.max_again_short) {
				if(isp_gain == 0) {
						*sensor_again = 0;
						return 0;
				}
				else if(isp_gain < lut->gain) {
						*sensor_again = (lut - 1)->value;
						return (lut - 1)->gain;
				}
				else{
						if((lut->gain == jxk308p_attr.max_again_short) && (isp_gain >= lut->gain)) {
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

unsigned int jxk308p_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
		return 0;
}

struct tx_isp_mipi_bus jxk308p_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 432,
	.lans = 2,
	.image_twidth = 3840,
	.image_theight = 2160,
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

struct tx_isp_dvp_bus jxk308p_dvp = {
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

struct tx_isp_sensor_attribute jxk308p_attr = {
	.name = "jxk308p",
	.chip_id = 0x0847,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x40,
	.sensor_ctrl.alloc_again = jxk308p_alloc_again,
	.sensor_ctrl.alloc_dgain = jxk308p_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = jxk308p_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */
};

static struct regval_list jxk308p_init_regs_3840_2160_15fps_mipi[] = {
	{0x12,0x40},
	{0x48,0x8A},
	{0x48,0x0A},
	{0x0E,0x11},
	{0x0F,0x04},
	{0x10,0x40},
	{0x11,0x80},
	{0x0D,0xA0},
	{0x57,0x60},
	{0x58,0x18},
	{0x5F,0x42},
	{0x60,0x2B},
	{0xBF,0x01},
	{0x59,0x04},
	{0xBF,0x00},
	{0x20,0xB0},
	{0x21,0x04},
	{0x22,0x60},
	{0x23,0x09},
	{0x24,0xC0},
	{0x25,0x70},
	{0x26,0x83},
	{0x27,0xAE},
	{0x28,0x15},
	{0x29,0x04},
	{0x2A,0xA0},
	{0x2B,0x14},
	{0x2C,0x00},
	{0x2D,0x01},
	{0x2E,0x23},
	{0x2F,0x08},
	{0x41,0xC4},
	{0x42,0x23},
	{0x46,0x18},
	{0x47,0x43},
	{0x80,0x03},
	{0xAF,0x32},
	{0xBD,0x00},
	{0xBE,0x0F},
	{0x82,0x30},
	{0x8A,0x04},
	{0xA7,0x00},
	{0x1D,0x00},
	{0x1E,0x04},
	{0x6C,0x40},
	{0x70,0xD5},
	{0x71,0x9B},
	{0x72,0x6D},
	{0x73,0x49},
	{0x75,0x96},
	{0x74,0x12},
	{0x89,0x0F},
	{0x0C,0xE0},
	{0x6B,0x20},
	{0x86,0x00},
	{0x78,0x44},
	{0xAA,0x80},
	{0xA1,0x1D},
	{0x31,0x10},
	{0x32,0x44},
	{0x33,0x60},
	{0x35,0x56},
	{0x3A,0xAF},
	{0x56,0x0A},
	{0x59,0xFF},
	{0x61,0x20},
	{0x85,0xB0},
	{0x8C,0x02},
	{0x8D,0x00},
	{0x91,0x12},
	{0x9F,0x50},
	{0xBA,0x60},
	{0xBF,0x01},
	{0x57,0x26},
	{0xBF,0x00},
	{0x5A,0x01},
	{0x5B,0xB9},
	{0x5C,0x32},
	{0x5D,0x21},
	{0x5E,0x84},
	{0x64,0xC0},
	{0x65,0x02},
	{0x66,0x80},
	{0x67,0x43},
	{0x68,0x30},
	{0x69,0x74},
	{0x6A,0x2A},
	{0x7A,0x00},
	{0x8F,0x90},
	{0x9B,0x9F},
	{0x9D,0x79},
	{0xBF,0x01},
	{0x4D,0x00},
	{0xBF,0x00},
	{0x97,0x7A},
	{0x13,0x01},
	{0x96,0x04},
	{0x4A,0xF5},
	{0x50,0x02},
	{0x49,0x10},
	{0xBF,0x01},
	{0x4E,0x11},
	{0x50,0x00},
	{0x51,0x8F},
	{0x55,0x00},
	{0x58,0x00},
	{0x66,0x15},
	{0x67,0x80},
	{0x68,0x32},
	{0xBF,0x00},
	{0x7E,0x4C},
	{0x19,0x20},
	{0xC0,0x4A},
	{0xC1,0x01},
	{0x12,0x00},
	{0x1F,0xA0},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
* the order of the jxk308p_win_sizes is [full_resolution, preview_resolution].
*/
static struct tx_isp_sensor_win_setting jxk308p_win_sizes[] = {
	/* 3840*2160 [0] */
	{
			.width          = 3840,
			.height         = 2160,
			.fps            = 15 << 16 | 1,
			.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
			.colorspace     = TISP_COLORSPACE_SRGB,
			.regs           = jxk308p_init_regs_3840_2160_15fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &jxk308p_win_sizes[0];

/*
* the part of driver was fixed.
*/

static struct regval_list jxk308p_stream_on_mipi[] = {
	{0x12,0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list jxk308p_stream_off_mipi[] = {
	{0x12,0x40},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int jxk308p_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int jxk308p_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int jxk308p_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
		int ret;
		unsigned char val;
		while (vals->reg_num != SENSOR_REG_END) {
				if (vals->reg_num == SENSOR_REG_DELAY) {
						private_msleep(vals->value);
				} else {
						ret = jxk308p_read(sd, vals->reg_num, &val);
						/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
						if (ret < 0)
								return ret;
				}
				vals++;
		}

		return 0;
}
#endif

static int jxk308p_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = jxk308p_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int jxk308p_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int jxk308p_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int jxk308p_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
		int ret;
		unsigned char val;
		while (vals->reg_num != SENSOR_REG_END) {
				if (vals->reg_num == SENSOR_REG_DELAY) {
						private_msleep(vals->value);
				} else {
						ret = jxk308p_read(sd, vals->reg_num, &val);
						if (ret < 0)
								return ret;
				}
				vals++;
		}
		return 0;
}
#endif

static int jxk308p_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
		int ret;
		while (vals->reg_num != SENSOR_REG_END) {
				if (vals->reg_num == SENSOR_REG_DELAY) {
						private_msleep(vals->value);
				} else {
						ret = jxk308p_write(sd, vals->reg_num, vals->value);
						if (ret < 0)
								return ret;
				}
				vals++;
		}

		return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int jxk308p_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = ISP_SUCCESS;

	rate = private_clk_get_rate(sensor->mclk);
	if(((rate / 1000) % mclk) != 0) {
		ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
		sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
		if (IS_ERR(sclka)) {
				pr_err("get sclka failed\n");
		} else {
			rate = private_clk_get_rate(sclka);
			if(((rate / 1000) % mclk) != 0) {
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

	private_clk_set_rate(sensor->mclk, SENSOR_MCLK);
	private_clk_prepare_enable(sensor->mclk);
	return ret;

error:
	return ret;
}

static int jxk308p_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;

	sensor->video.fps = wsize->fps;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

#ifdef SENSOR_MIR_FLIP
	sensor->video.shvflip = 1;
#endif

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int jxk308p_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &jxk308p_win_sizes[0];
		jxk308p_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		jxk308p_attr.max_dgain = 0;
		jxk308p_attr.max_again = 324673;
		jxk308p_attr.min_integration_time = 2;
		jxk308p_attr.max_integration_time = 2400 - 4;
		jxk308p_attr.total_width = 4800;
		jxk308p_attr.total_height = 2400;
		jxk308p_attr.integration_time_apply_delay = 2;
		jxk308p_attr.again_apply_delay = 2;
		jxk308p_attr.dgain_apply_delay = 0;
		jxk308p_attr.integration_time_limit = jxk308p_attr.max_integration_time;
		jxk308p_attr.max_integration_time_native = jxk308p_attr.max_integration_time;
		jxk308p_attr.min_integration_time_native = jxk308p_attr.min_integration_time;
		jxk308p_attr.expo_fs = 1;
#ifdef SENSOR_WDR_2_FRAME
		jxk308p_attr.max_again_short = xxxx;
		jxk308p_attr.min_integration_time_short = xx;
		jxk308p_attr.max_integration_time_short = xx;
		jxk308p_attr.wdr_cache = wdr_line * jxk308p_attr.total_width;
#endif /* SENSOR_WDR_2_FRAME */
		memcpy((void *)(&(jxk308p_attr.mipi)), (void *)(&jxk308p_mipi_linear), sizeof(jxk308p_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		}

	return ret;
}

static int jxk308p_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	jxk308p_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
	case TISP_SENSOR_VI_MIPI_CSI1:
		jxk308p_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		jxk308p_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_DVP:
		jxk308p_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this interface!!!\n");
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

	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	ret = jxk308p_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}

	jxk308p_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
		return -1;
}

static int jxk308p_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = jxk308p_read(sd, 0x0a, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = jxk308p_read(sd, 0x0b, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int jxk308p_g_chip_ident(struct tx_isp_subdev *sd,
								struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	jxk308p_attr_check(sd);
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "jxk308p_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "jxk308p_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = jxk308p_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an jxk308p chip.\n",
			client->addr, client->adapter->name);
		return ret;
	}

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", jxk308p_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "jxk308p", sizeof("jxk308p"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int jxk308p_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int jxk308p_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (!init->enable) {
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		return ISP_SUCCESS;
	} else {
		ret = jxk308p_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int jxk308p_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = jxk308p_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int jxk308p_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	jxk308p_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int jxk308p_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
			ret = jxk308p_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = jxk308p_write_array(sd, jxk308p_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("jxk308p stream on\n");
		}

	} else {
		ret = jxk308p_write_array(sd, jxk308p_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("jxk308p stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int jxk308p_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	// set integration_time
	ret += jxk308p_write(sd, 0x01, (unsigned char)(it & 0xff));
	ret += jxk308p_write(sd, 0x02, (unsigned char)((it >> 8) & 0xff));

	// set sensor again
	ret = jxk308p_write(sd, 0x00, (unsigned char)(again & 0x3f));

	if (ret < 0)
		ISP_ERROR("err: jxk308p_write err\n");

	return ret;
}
#else
static int jxk308p_set_integration_time(struct tx_isp_subdev *sd, int value)
{
		int ret = ISP_SUCCESS;

		...;

		return ret;
}

static int jxk308p_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
		int ret = ISP_SUCCESS;

		...;

		return ret;
}
#endif /* SENSOR_EXPO */

static int jxk308p_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxk308p_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxk308p_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
			ret = jxk308p_attr_set(sd, wsize);
	}

	return ret;
}

static int jxk308p_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 4800 * 2400 * 15;  /**< HTS * VTS * FPS */
		break;
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	max_fps = wsize->fps;
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d/%d) no in range\n", fps >> 16, fps & 0xffff);
		return -1;
	}

	val = 0;
	ret += jxk308p_read(sd, 0x21, &val);
	hts = val << 8;
	val = 0;
	ret += jxk308p_read(sd, 0x20, &val);
	hts = (hts | val) << 2;
	if (0 != ret) {
		ISP_ERROR("err: jxk308p read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	jxk308p_write(sd, 0x22, (unsigned char) (vts & 0xff));
	jxk308p_write(sd, 0x23, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: jxk308p_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->max_integration_time_native = vts - 4;
#else
	/* WDR mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - xx;
	sensor->video.attr->integration_time_limit = vts - xx;
	sensor->video.attr->max_integration_time_native = vts - xx;
	sensor->video.attr->max_integration_time_short = vts - xx;
#endif /* SENSOR_WDR_2_FRAME */
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	if (ret) {
			ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
	}

	return ret;
}

#ifdef SENSOR_MIR_FLIP
static int jxk308p_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;
	unsigned char val;

	ret += jxk308p_read(sd, 0x12, &val);

	/* 2'b01:mirror,2'b10:filp */
	switch(enable) {
	case 0:
		val &= 0xCF;
		break;
	case 1:
		val &= 0xCF;
		val |= 0x20; /*mirror*/
		break;
	case 2:
		val &= 0xCF;
		val |= 0x10;
		break;
	case 3:
		val &= 0xCF;
		val |= 0x30;
		break;
	}

	ret = jxk308p_write(sd, 0x12, val);
	if (0 != ret)
	{
		ISP_ERROR("%s:%d, jxk308p_write err!!\n", __func__, __LINE__);
		return ret;
	}

	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}
#endif /* SENSOR_MIR_FLIP */

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int jxk308p_set_expo_short(struct tx_isp_subdev *sd, int value)
{
		int ret = ISP_SUCCESS;

		...;

		return ret;
}
#else
static int jxk308p_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
		int ret = ISP_SUCCESS;

		...;

		return ret;
}

static int jxk308p_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
		int ret = ISP_SUCCESS;

		...;

		return ret;
}
#endif /* SENSOR_EXPO */

static int jxk308p_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
		struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
		int ret = ISP_SUCCESS;

		ret = jxk308p_write_array(sd, jxk308p_stream_off_mipi);
		if (wdr_en == 1) {
				jxk308p_setting_select(sd, 1);
				jxk308p_attr_set(sd, wsize);
		} else if (wdr_en == 0) {
				jxk308p_setting_select(sd, 0);
				jxk308p_attr_set(sd, wsize);
		} else {
				ISP_ERROR("Can not support this data type!!!");
				return -1;
		}

		return 0;
}

static int jxk308p_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
		struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
		struct tx_isp_sensor_register_info *info = &sensor->info;
		int ret = ISP_SUCCESS;

		private_gpio_direction_output(info->rst_gpio, 0);
		private_msleep(1);
		private_gpio_direction_output(info->rst_gpio, 1);
		private_msleep(1);

		ret = jxk308p_write_array(sd, wsize->regs);
		ret = jxk308p_write_array(sd, jxk308p_stream_on_mipi);

		return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int jxk308p_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	// struct tx_isp_initarg *init = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}

	switch (cmd) {
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = jxk308p_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = jxk308p_set_integration_time(sd, sensor_val->value);
		break;
case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = jxk308p_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = jxk308p_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = jxk308p_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = jxk308p_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = jxk308p_write_array(sd, jxk308p_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = jxk308p_write_array(sd, jxk308p_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = jxk308p_set_fps(sd, sensor_val->value);
		break;
#ifdef SENSOR_MIR_FLIP
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = jxk308p_set_vflip(sd, sensor_val->value);
		break;
#endif /* SENSOR_MIR_FLIP */
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
			if (arg)
					ret = jxk308p_set_expo_short(sd, sensor_val->value);
			break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
			if(arg)
					ret = jxk308p_set_integration_time_short(sd, sensor_val->value);
			break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
			if(arg)
					ret = jxk308p_set_analog_gain_short(sd, sensor_val->value);
			break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
			if(arg)
					ret = jxk308p_set_wdr(sd, init->enable);
			break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
			if(arg)
					ret = jxk308p_set_wdr_stop(sd, init->enable);
			break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
			break;
	}

	return 0;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops jxk308p_core_ops = {
	.g_chip_ident = jxk308p_g_chip_ident,
	.reset = jxk308p_reset,
	.init = jxk308p_init,
	.g_register = jxk308p_g_register,
	.s_register = jxk308p_s_register,
};

static struct tx_isp_subdev_video_ops jxk308p_video_ops = {
	.s_stream = jxk308p_s_stream,
};

static struct tx_isp_subdev_sensor_ops jxk308p_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = jxk308p_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops jxk308p_ops = {
	.core = &jxk308p_core_ops,
	.video = &jxk308p_video_ops,
	.sensor = &jxk308p_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
	.name = "jxk308p",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int jxk308p_probe(struct i2c_client *client,
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

	sensor->video.attr = &jxk308p_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &jxk308p_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->jxk308p\n");

	return 0;
}

static int jxk308p_remove(struct i2c_client *client)
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

static const struct i2c_device_id jxk308p_id[] = {
	{"jxk308p", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, jxk308p_id);

static struct i2c_driver jxk308p_driver = {
	.driver = {
			.owner  = THIS_MODULE,
			.name   = "jxk308p",
	},
	.probe          = jxk308p_probe,
	.remove         = jxk308p_remove,
	.id_table       = jxk308p_id,
};

static __init int init_jxk308p(void) {
	return private_i2c_add_driver(&jxk308p_driver);
}

static __exit void exit_jxk308p(void) {
	private_i2c_del_driver(&jxk308p_driver);
}

module_init(init_jxk308p);
module_exit(exit_jxk308p);

MODULE_DESCRIPTION("A low-level driver for jxk308p sensor");
MODULE_LICENSE("GPL");
