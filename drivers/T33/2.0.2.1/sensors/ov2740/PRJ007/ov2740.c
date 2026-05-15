/*
 * ov2740.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps		dvdd
 *  0           960*540         30        mipi_1lane    linear  10        30
 *
 * @I2C addr: 0x36
 *
 * @FSync:none
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

#define TVERSION "V20241105a"
#define SENSOR_VERSION  "H20250211a"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x00)
#define SENSOR_CHIP_ID_L    (0x27)
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

struct tx_isp_sensor_attribute ov2740_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut ov2740_again_lut[] = {
	{0x080,0},
	{0x088,5732},
	{0x090,11136},
	{0x098,16248},
	{0x0A0,21098},
	{0x0A8,25711},
	{0x0B0,30109},
	{0x0B8,34312},
	{0x0C0,38336},
	{0x0C8,42196},
	{0x0D0,45904},
	{0x0D8,49472},
	{0x0E0,52911},
	{0x0E8,56229},
	{0x0F0,59434},
	{0x0F8,62534},
	{0x100,65536},
	{0x110,71268},
	{0x120,76672},
	{0x130,81784},
	{0x140,86634},
	{0x150,91247},
	{0x160,95645},
	{0x170,99848},
	{0x180,103872},
	{0x190,107732},
	{0x1A0,111440},
	{0x1B0,115008},
	{0x1C0,118447},
	{0x1D0,121765},
	{0x1E0,124970},
	{0x1F0,128070},
	{0x200,131072},
	{0x220,136804},
	{0x240,142208},
	{0x260,147320},
	{0x280,152170},
	{0x2A0,156783},
	{0x2C0,161181},
	{0x2E0,165384},
	{0x300,169408},
	{0x320,173268},
	{0x340,176976},
	{0x360,180544},
	{0x380,183983},
	{0x3A0,187301},
	{0x3C0,190506},
	{0x3E0,193606},
	{0x400,196608},
	{0x440,202340},
	{0x480,207744},
	{0x4C0,212856},
	{0x500,217706},
	{0x540,222319},
	{0x580,226717},
	{0x5C0,230920},
	{0x600,234944},
	{0x640,238804},
	{0x680,242512},
	{0x6C0,246080},
	{0x700,249519},
	{0x740,252837},
	{0x780,256042},
	{0x7C0,259142},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int ov2740_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = ov2740_again_lut;
	while (lut->gain <= ov2740_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == ov2740_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int ov2740_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = ov2740_again_lut;
	while(lut->gain <= ov2740_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == ov2740_attr.again_short.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

#else
	/* Non analog gain table */

#endif /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

	return isp_gain;
}
#endif /* SENSOR_WDR_2_FRAME */

unsigned int ov2740_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int ov2740_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int ov2740_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus ov2740_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 720,
	.lans = 1,
	.image_twidth = 960,
	.image_theight = 540,
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

struct tx_isp_dvp_bus ov2740_dvp = {
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

struct tx_isp_sensor_attribute ov2740_attr = {
	.name = "ov2740",
	.chip_id = 0x002740,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x36,
	.sensor_ctrl.alloc_again = ov2740_alloc_again,
	.sensor_ctrl.alloc_dgain = ov2740_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = ov2740_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list ov2740_init_regs_960_540_30fps_mipi[] = {
	{0x103,0x01},
	{0x302,0x1e},
	{0x30d,0x1e},
	{0x30e,0x02},
	{0x312,0x01},
	{0x3000,0x00},
	{0x3018,0x12},		//12=1LANE 32=2LANEW
	{0x3031,0x0a},
	{0x3080,0x08},
	{0x3083,0xB4},
	{0x3103,0x00},
	{0x3104,0x01},
	{0x3106,0x01},
	{0x3500,0x00},
	{0x3501,0x22},
	{0x3502,0x00},
	{0x3503,0x88},
	{0x3507,0x00},
	{0x3508,0x00},
	{0x3509,0x80},
	{0x350c,0x00},
	{0x350d,0x80},
	{0x3510,0x00},
	{0x3511,0x00},
	{0x3512,0x20},
	{0x3632,0x00},
	{0x3633,0x10},
	{0x3634,0x10},
	{0x3635,0x10},
	{0x3645,0x13},
	{0x3646,0x81},
	{0x3636,0x10},
	{0x3651,0x0a},
	{0x3656,0x02},
	{0x3659,0x04},
	{0x365a,0xda},
	{0x365b,0xa2},
	{0x365c,0x04},
	{0x365d,0x1d},
	{0x365e,0x1a},
	{0x3662,0xd7},
	{0x3667,0x78},
	{0x3669,0x0a},
	{0x366a,0x92},
	{0x3700,0x54},
	{0x3702,0x10},
	{0x3706,0x42},
	{0x3709,0x30},
	{0x370b,0xc2},
	{0x3714,0x26},
	{0x3715,0x01},
	{0x3716,0x00},
	{0x371a,0x3e},
	{0x3732,0x0e},
	{0x3733,0x10},
	{0x375f,0x0e},
	{0x3768,0x20},
	{0x3769,0x44},
	{0x376a,0x22},
	{0x377b,0x20},
	{0x377c,0x00},
	{0x377d,0x0c},
	{0x3798,0x00},
	{0x37a1,0x55},
	{0x37a8,0x6d},
	{0x37c2,0x14},
	{0x37c5,0x00},
	{0x37c8,0x00},
	{0x3800,0x00},
	{0x3801,0x00},
	{0x3802,0x00},
	{0x3803,0x00},
	{0x3804,0x07},
	{0x3805,0x9f},
	{0x3806,0x04},
	{0x3807,0x47},
	{0x3808,0x03},
	{0x3809,0xC0},		//c8
	{0x380a,0x02},
	{0x380b,0x1C},		//24
	{0x380c,0x08},		//08HTS
	{0x380d,0x70},		//70
	{0x380e,0x04},
	{0x380f,0x58},		//VTS
	{0x3810,0x00},
	{0x3811,0x00},
	{0x3812,0x00},
	{0x3813,0x00},
	{0x3814,0x03},
	{0x3815,0x01},
	{0x3820,0x80},
	{0x3821,0x61},		//0x67
	{0x3822,0x84},
	{0x3829,0x00},
	{0x382a,0x03},
	{0x382b,0x01},
	{0x3830,0x04},
	{0x3836,0x02},
	{0x3837,0x08},
	{0x3839,0x01},
	{0x383a,0x00},
	{0x383b,0x08},
	{0x383c,0x00},
	{0x3f0b,0x00},
	{0x4001,0x20},
	{0x4009,0x03},
	{0x4003,0x10},
	{0x4010,0xe0},
	{0x4016,0x00},
	{0x4017,0x10},
	{0x4044,0x02},
	{0x4304,0x08},
	{0x4307,0x30},
	{0x4320,0x80},
	{0x4322,0x00},
	{0x4323,0x00},
	{0x4324,0x00},
	{0x4325,0x00},
	{0x4326,0x00},
	{0x4327,0x00},
	{0x4328,0x00},
	{0x4329,0x00},
	{0x432c,0x03},
	{0x432d,0x81},
	{0x4501,0x84},
	{0x4502,0x40},
	{0x4503,0x18},
	{0x4504,0x04},
	{0x4508,0x02},
	{0x4601,0x50},
	{0x4800,0x00},
	{0x4816,0x52},
	{0x4837,0x16},
	{0x5000,0x7f},
	{0x5001,0x00},
	{0x5005,0x38},
	{0x501e,0x0d},
	{0x5040,0x00},
	{0x5901,0x00},
	{0x100,0x01},
    {SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the ov2740_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting ov2740_win_sizes[] = {
	/* 960*540 [0] */
	{
		.width          = 960,
		.height         = 540,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = ov2740_init_regs_960_540_30fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &ov2740_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list ov2740_stream_on_mipi[] = {
	// {0x0100, 0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list ov2740_stream_off_mipi[] = {
	// {0x0100, 0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int ov2740_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int ov2740_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int ov2740_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = ov2740_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int ov2740_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = ov2740_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int ov2740_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int ov2740_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int ov2740_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = ov2740_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int ov2740_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = ov2740_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int ov2740_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int ov2740_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int ov2740_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &ov2740_win_sizes[0];
		ov2740_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		ov2740_attr.again.value = 65536;
		ov2740_attr.again.max = 259142;
		ov2740_attr.again.min = 0;
		ov2740_attr.again.apply_delay = 2;

		ov2740_attr.integration_time.value = 0xb60;
		ov2740_attr.integration_time.max = 1112 - 4;
		ov2740_attr.integration_time.min = 1;
		ov2740_attr.integration_time.apply_delay = 1112 - 4;

		ov2740_attr.total_width = 2160;
		ov2740_attr.total_height = 1112;

		ov2740_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		ov2740_attr.hcg.base_gain = ;
		ov2740_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		ov2740_attr.again_short.value = ;
		ov2740_attr.again_short.max = ;
		ov2740_attr.again_short.min = ;
		ov2740_attr.again_short.apply_delay = ;

		ov2740_attr.integration_time_short.value = ;
		ov2740_attr.integration_time_short.max = ;
		ov2740_attr.integration_time_short.min = ;
		ov2740_attr.integration_time_short.apply_delay = ;

		ov2740_attr.wdr_cache = wdr_line * ov2740_attr.total_width;

#ifdef SENSOR_HCG
		ov2740_attr.hcg_short.base_gain = ;
		ov2740_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(ov2740_attr.mipi)), (void *)(&ov2740_mipi_linear), sizeof(ov2740_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int ov2740_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	ov2740_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		ov2740_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		ov2740_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		ov2740_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		ov2740_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		ov2740_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this interface!!!\n");
	}

#ifndef ZRT_SENSOR_WITHOUT_INIT
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

	ret = ov2740_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
	ov2740_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int ov2740_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = ov2740_read(sd, 0x300a, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = ov2740_read(sd, 0x300b, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int ov2740_g_chip_ident(struct tx_isp_subdev *sd,
								 struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	ov2740_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "ov2740_reset");
		if (!ret) {
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(info->rst_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
		}
	}
	if (info->pwdn_gpio != -1) {
		ret = private_gpio_request(info->pwdn_gpio, "ov2740_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = ov2740_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an ov2740 chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", ov2740_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "ov2740", sizeof("ov2740"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int ov2740_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int ov2740_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = ov2740_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int ov2740_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = ov2740_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int ov2740_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov2740_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int ov2740_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = ov2740_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = ov2740_write_array(sd, ov2740_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("ov2740 stream on\n");
		}

	} else {
		ret = ov2740_write_array(sd, ov2740_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("ov2740 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int ov2740_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = (value & 0xffff) << 4 ;
	int again = (value & 0xffff0000) >> 16;

	/*integration time*/
	ret += ov2740_write(sd, 0x3502, (unsigned char)(it & 0xff));
	ret += ov2740_write(sd, 0x3501, (unsigned char)((it >> 8) & 0xff));
	ret += ov2740_write(sd, 0x3500, (unsigned char)((it >> 16) & 0xff));

	/*set analog gain*/
	ret += ov2740_write(sd, 0x3508, (unsigned char)((again >> 8) & 0x1f));
	ret += ov2740_write(sd, 0x3509, (unsigned char)(again & 0xff));

	if (0 != ret)
		ISP_ERROR("%s reg write err!!\n");

    return ret;
}
#else
static int ov2740_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	ret += ov2740_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += ov2740_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += ov2740_write(sd, 0x3e02, (unsigned char)((value & 0xf) << 4));

	return ret;
}

static int ov2740_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	switch (info->default_boot) {
	case 0:
		ret += ov2740_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
		ret += ov2740_write(sd, 0x3e09, (unsigned char)(value & 0xff));
		break;
	case 1:
		ret += ov2740_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
		ret += ov2740_write(sd, 0x3e09, (unsigned char)(value & 0xff));
		ret += ov2740_write(sd, 0x3e82, (unsigned char)((value >> 8) & 0xff));
		ret += ov2740_write(sd, 0x3e83, (unsigned char)(value & 0xff));
		break;
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this sboot!!!\n");
    }

	return ret;
}
#endif /* SENSOR_EXPO */

static int ov2740_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int ov2740_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int ov2740_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = ov2740_attr_set(sd, wsize);
	}

	return ret;
}

static int ov2740_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 2160 * 1112 * 30;  /**< HTS * VTS * FPS */
		max_fps = wsize->fps;
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
	ret += ov2740_read(sd, 0x380c, &val);
	hts = val << 8;
	val = 0;
	ret += ov2740_read(sd, 0x380d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: ov2740 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);\

	ov2740_write(sd, 0x380e, (unsigned char) (vts >> 8));
	ov2740_write(sd, 0x380f, (unsigned char) (vts & 0xff));

	if (0 != ret) {
		ISP_ERROR("err: ov2740_write err\n");
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

static int ov2740_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = ISP_SUCCESS;
	unsigned char val_flip = 0x0;
	unsigned char val_mirror = 0x0;

	par->drop_frame = 0;
	par->reset = 0;


	/* 2'b01:mirror,2'b10:filp */
	ret = ov2740_read(sd, 0x3820, &val_flip);
	ret += ov2740_read(sd, 0x3821, &val_mirror);
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		val_flip &= 0xF9;
		val_mirror &= 0xF9;
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		val_mirror = ( (val_mirror & 0xF9) | 0x06);
		val_flip &= 0xF9;
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		val_flip = ( (val_flip & 0xF9) | 0x06);
		val_mirror &= 0xF9;
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		val_mirror = ( (val_mirror & 0xF9) | 0x06);
		val_flip = ( (val_flip & 0xF9) | 0x06);
		break;
	}
	// printk(" 0x3820=0x%x, 0x3821=0x%x\n",val_flip, val_mirror);
	ret += ov2740_write(sd, 0x3820, val_flip);
	ret += ov2740_write(sd, 0x3821, val_mirror);

	sensor->video.hvflip_mode = par->hvflip;
	ov2740_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int ov2740_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int ov2740_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int ov2740_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int ov2740_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = ov2740_write_array(sd, ov2740_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		ov2740_setting_select(sd, 1);
		ov2740_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		ov2740_setting_select(sd, 0);
		ov2740_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int ov2740_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = ov2740_write_array(sd, wsize->regs);
	ret = ov2740_write_array(sd, ov2740_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int ov2740_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = ov2740_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = ov2740_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = ov2740_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = ov2740_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = ov2740_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = ov2740_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = ov2740_write_array(sd, ov2740_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = ov2740_write_array(sd, ov2740_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = ov2740_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = ov2740_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = ov2740_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = ov2740_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = ov2740_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = ov2740_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = ov2740_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops ov2740_core_ops = {
	.g_chip_ident = ov2740_g_chip_ident,
	.reset = ov2740_reset,
	.init = ov2740_init,
	.g_register = ov2740_g_register,
	.s_register = ov2740_s_register,
};

static struct tx_isp_subdev_video_ops ov2740_video_ops = {
	.s_stream = ov2740_s_stream,
};

static struct tx_isp_subdev_sensor_ops ov2740_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = ov2740_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops ov2740_ops = {
	.core = &ov2740_core_ops,
	.video = &ov2740_video_ops,
	.sensor = &ov2740_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
	.name = "ov2740",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int ov2740_probe(struct i2c_client *client,
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
	sensor->video.attr = &ov2740_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &ov2740_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->ov2740\n");

	return 0;
}

static int ov2740_remove(struct i2c_client *client)
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

static const struct i2c_device_id ov2740_id[] = {
	{"ov2740", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, ov2740_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver ov2740_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "ov2740",
	},
	.probe          = ov2740_probe,
	.remove         = ov2740_remove,
	.id_table       = ov2740_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_ov2740(void) {
	return private_i2c_add_driver(&ov2740_driver);
}

static __exit void exit_ov2740(void) {
	private_i2c_del_driver(&ov2740_driver);
}

module_init(init_ov2740);
module_exit(exit_ov2740);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_ov2740(void) {
	return private_i2c_add_driver(&ov2740_driver);
}

static void exit_first_ov2740(void) {
	private_i2c_del_driver(&ov2740_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "ov2740",
	.i2c_addr = 0x36,
	.width = 960,
	.height = 540,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_ov2740,
	.exit_sensor = exit_first_ov2740
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for ov2740 sensor");
MODULE_LICENSE("GPL");
