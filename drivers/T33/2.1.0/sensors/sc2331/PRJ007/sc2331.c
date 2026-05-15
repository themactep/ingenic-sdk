/*
 * sc2331.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode 		raw     max_fps		dvdd
 *   0          1920x1080       20         MIPI         Linear          RAW10    25
 *
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
#define SENSOR_VERSION  "H20250226a"

//#define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
//#define SENSOR_HCG
//#define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0xcb)
#define SENSOR_CHIP_ID_L    (0x5c)
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

struct tx_isp_sensor_attribute sc2331_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sc2331_again_lut[] = {
	{0x80,0},
	{0x84,2886},
	{0x88,5776},
	{0x8c,8494},
	{0x90,11136},
	{0x94,13706},
	{0x98,16287},
	{0x9c,18723},
	{0xa0,21097},
	{0xa4,23414},
	{0xa8,25746},
	{0xac,27953},
	{0xb0,30109},
	{0xb4,32217},
	{0xb8,34345},
	{0xbc,36361},
	{0xc0,38336},
	{0xc4,40270},
	{0xc8,42226},
	{0xcc,44082},
	{0xd0,45904},
	{0xd4,47690},
	{0xd8,49500},
	{0xdc,51220},
	{0xe0,52910},
	{0xe4,54571},
	{0xe8,56254},
	{0xec,57857},
	{0xf0,59433},
	{0xf4,60984},
	{0xf8,62558},
	{0xfc,64059},
	{0x880,65536},
	{0x884,68422},
	{0x888,71312},
	{0x88c,74030},
	{0x890,76672},
	{0x894,79242},
	{0x898,81823},
	{0x89c,84259},
	{0x8a0,86633},
	{0x8a4,88950},
	{0x8a8,91282},
	{0x8ac,93489},
	{0x8b0,95645},
	{0x8b4,97753},
	{0x8b8,99881},
	{0x8bc,101897},
	{0x8c0,103872},
	{0x8c4,105806},
	{0x8c8,107762},
	{0x8cc,109618},
	{0x8d0,111440},
	{0x8d4,113226},
	{0x8d8,115036},
	{0x8dc,116756},
	{0x8e0,118446},
	{0x8e4,120107},
	{0x8e8,121790},
	{0x8ec,123393},
	{0x8f0,124969},
	{0x8f4,126520},
	{0x8f8,128094},
	{0x8fc,129595},
	{0x980,131072},
	{0x984,133958},
	{0x988,136848},
	{0x98c,139566},
	{0x990,142208},
	{0x994,144778},
	{0x998,147359},
	{0x99c,149795},
	{0x9a0,152169},
	{0x9a4,154486},
	{0x9a8,156818},
	{0x9ac,159025},
	{0x9b0,161181},
	{0x9b4,163289},
	{0x9b8,165417},
	{0x9bc,167433},
	{0x9c0,169408},
	{0x9c4,171342},
	{0x9c8,173298},
	{0x9cc,175154},
	{0x9d0,176976},
	{0x9d4,178762},
	{0x9d8,180572},
	{0x9dc,182292},
	{0x9e0,183982},
	{0x9e4,185643},
	{0x9e8,187326},
	{0x9ec,188929},
	{0x9f0,190505},
	{0x9f4,192056},
	{0x9f8,193630},
	{0x9fc,195131},
	{0xb80,196608},
	{0xb84,199494},
	{0xb88,202384},
	{0xb8c,205102},
	{0xb90,207744},
	{0xb94,210314},
	{0xb98,212895},
	{0xb9c,215331},
	{0xba0,217705},
	{0xba4,220022},
	{0xba8,222354},
	{0xbac,224561},
	{0xbb0,226717},
	{0xbb4,228825},
	{0xbb8,230953},
	{0xbbc,232969},
	{0xbc0,234944},
	{0xbc4,236878},
	{0xbc8,238834},
	{0xbcc,240690},
	{0xbd0,242512},
	{0xbd4,244298},
	{0xbd8,246108},
	{0xbdc,247828},
	{0xbe0,249518},
	{0xbe4,251179},
	{0xbe8,252862},
	{0xbec,254465},
	{0xbf0,256041},
	{0xbf4,257592},
	{0xbf8,259166},
	{0xbfc,260667},
	{0xf80,262144},
	{0xf84,265030},
	{0xf88,267920},
	{0xf8c,270638},
	{0xf90,273280},
	{0xf94,275850},
	{0xf98,278431},
	{0xf9c,280867},
	{0xfa0,283241},
	{0xfa4,285558},
	{0xfa8,287890},
	{0xfac,290097},
	{0xfb0,292253},
	{0xfb4,294361},
	{0xfb8,296489},
	{0xfbc,298505},
	{0xfc0,300480},
	{0xfc4,302414},
	{0xfc8,304370},
	{0xfcc,306226},
	{0xfd0,308048},
	{0xfd4,309834},
	{0xfd8,311644},
	{0xfdc,313364},
	{0xfe0,315054},
	{0xfe4,316715},
	{0xfe8,318398},
	{0xfec,320001},
	{0xff0,321577},
	{0xff4,323128},
	{0xff8,324702},
	{0xffc,326203},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int sc2331_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc2331_again_lut;
	while (lut->gain <= sc2331_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sc2331_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int sc2331_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc2331_again_lut;
	while(lut->gain <= sc2331_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc2331_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int sc2331_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int sc2331_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int sc2331_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus sc2331_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 276,
	.lans = 2,
	.image_twidth = 1920,
	.image_theight = 1080,
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

struct tx_isp_dvp_bus sc2331_dvp = {
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

struct tx_isp_sensor_attribute sc2331_attr = {
	.name = "sc2331",
	.chip_id = 0xcb5c,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.sensor_ctrl.alloc_again = sc2331_alloc_again,
	.sensor_ctrl.alloc_dgain = sc2331_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = sc2331_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list sc2331_init_regs_1920_1080_20fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x56},
	{0x320c,0x09},
	{0x320d,0xc4},
	{0x320e,0x04},
	{0x320f,0x50},
	{0x3250,0xff},
	{0x3258,0x0e},
	{0x3301,0x06},
	{0x3302,0x10},
	{0x3304,0x68},
	{0x3306,0x88},
	{0x3308,0x18},
	{0x3309,0x80},
	{0x330a,0x01},
	{0x330b,0x40},
	{0x330d,0x18},
	{0x331c,0x02},
	{0x331e,0x59},
	{0x331f,0x71},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x3364,0x56},
	{0x3390,0x08},
	{0x3391,0x09},
	{0x3392,0x0b},
	{0x3393,0x0a},
	{0x3394,0x2a},
	{0x3395,0x2a},
	{0x3396,0x48},
	{0x3397,0x49},
	{0x3398,0x4b},
	{0x3399,0x06},
	{0x339a,0x0a},
	{0x339b,0x30},
	{0x339c,0x48},
	{0x33ad,0x2c},
	{0x33ae,0x38},
	{0x33b3,0x40},
	{0x349f,0x02},
	{0x34a6,0x09},
	{0x34a7,0x0f},
	{0x34a8,0x30},
	{0x34a9,0x28},
	{0x34f8,0x5f},
	{0x34f9,0x28},
	{0x3630,0xc6},
	{0x3633,0x33},
	{0x3637,0x6b},
	{0x363c,0xc1},
	{0x363e,0xc2},
	{0x3670,0x2e},
	{0x3674,0xc5},
	{0x3675,0xc7},
	{0x3676,0xcb},
	{0x3677,0x44},
	{0x3678,0x48},
	{0x3679,0x48},
	{0x367c,0x08},
	{0x367d,0x0b},
	{0x367e,0x0b},
	{0x367f,0x0f},
	{0x3690,0x33},
	{0x3691,0x33},
	{0x3692,0x33},
	{0x3693,0x84},
	{0x3694,0x85},
	{0x3695,0x8d},
	{0x3696,0x9c},
	{0x369c,0x0b},
	{0x369d,0x0f},
	{0x369e,0x09},
	{0x369f,0x0b},
	{0x36a0,0x0f},
	{0x36ea,0x17},
	{0x36eb,0x0c},
	{0x36ec,0x1c},
	{0x36ed,0x18},
	{0x370f,0x01},
	{0x3722,0x05},
	{0x3724,0x20},
	{0x3725,0x91},
	{0x3771,0x05},
	{0x3772,0x05},
	{0x3773,0x05},
	{0x377a,0x0b},
	{0x377b,0x0f},
	{0x37fa,0xd7},
	{0x37fb,0x32},
	{0x37fc,0x11},
	{0x37fd,0x07},
	{0x3900,0x19},
	{0x3905,0xb8},
	{0x391b,0x80},
	{0x391c,0x04},
	{0x391d,0x81},
	{0x3933,0xc0},
	{0x3934,0x08},
	{0x3940,0x72},
	{0x3941,0x00},
	{0x3942,0x00},
	{0x3943,0x09},
	{0x3946,0x10},
	{0x3957,0x86},
	{0x3e00,0x00},
	{0x3e01,0x89},
	{0x3e02,0x30},
	{0x3e08,0x00},
	{0x440e,0x02},
	{0x4509,0x28},
	{0x450d,0x10},
	{0x4819,0x04},
	{0x481b,0x03},
	{0x481d,0x08},
	{0x481f,0x02},
	{0x4821,0x07},
	{0x4823,0x02},
	{0x4825,0x02},
	{0x4827,0x02},
	{0x4829,0x03},
	{0x5780,0x66},
	{0x578d,0x40},
	{0x5799,0x06},
	{0x36e9,0x50},
	{0x37f9,0x30},
	{0x440d,0x10},
	{0x440e,0x01},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the sc2331_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc2331_win_sizes[] = {
	/* 1920*1080 [0] */
	{
		.width          = 1920,
		.height         = 1080,
		.fps            = 20 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc2331_init_regs_1920_1080_20fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &sc2331_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sc2331_stream_on_mipi[] = {
	// {0x0100, 0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list sc2331_stream_off_mipi[] = {
	// {0x0100, 0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int sc2331_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int sc2331_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int sc2331_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2331_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc2331_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2331_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int sc2331_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int sc2331_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc2331_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2331_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int sc2331_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2331_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int sc2331_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int sc2331_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int sc2331_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &sc2331_win_sizes[0];
		sc2331_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc2331_attr.again.value = 0;
		sc2331_attr.again.max = 326203;
		sc2331_attr.again.min = 0;
		sc2331_attr.again.apply_delay = 2;

		sc2331_attr.integration_time.value = 0x40;
		sc2331_attr.integration_time.max = 0x450 - 7;
		sc2331_attr.integration_time.min = 2;
		sc2331_attr.integration_time.apply_delay = 2;

		sc2331_attr.total_width = 0x9c4;
		sc2331_attr.total_height = 0x450;

		sc2331_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		sc2331_attr.hcg.base_gain = ;
		sc2331_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc2331_attr.again_short.value = ;
		sc2331_attr.again_short.max = ;
		sc2331_attr.again_short.min = ;
		sc2331_attr.again_short.apply_delay = ;

		sc2331_attr.integration_time_short.value = ;
		sc2331_attr.integration_time_short.max = ;
		sc2331_attr.integration_time_short.min = ;
		sc2331_attr.integration_time_short.apply_delay = ;

		sc2331_attr.wdr_cache = wdr_line * sc2331_attr.total_width;

#ifdef SENSOR_HCG
		sc2331_attr.hcg_short.base_gain = ;
		sc2331_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(sc2331_attr.mipi)), (void *)(&sc2331_mipi_linear), sizeof(sc2331_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int sc2331_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	sc2331_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		sc2331_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc2331_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		sc2331_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc2331_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		sc2331_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = sc2331_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.apply_delay = 1;
	sc2331_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int sc2331_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sc2331_read(sd, 0x3107, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc2331_read(sd, 0x3108, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc2331_g_chip_ident(struct tx_isp_subdev *sd,
				 struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sc2331_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "sc2331_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "sc2331_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = sc2331_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc2331 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("sc2331 version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", sc2331_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
		    info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "sc2331", sizeof("sc2331"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc2331_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sc2331_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = sc2331_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int sc2331_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc2331_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc2331_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc2331_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int sc2331_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

    printk("[%s %d]\n", __func__, __LINE__);

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = sc2331_write_array(sd,wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sc2331_write_array(sd, sc2331_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("sc2331 stream on\n");
		}

	} else {
		ret = sc2331_write_array(sd, sc2331_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("sc2331 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int sc2331_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;
	it = it << 1;

	//integration time
	ret = sc2331_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc2331_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc2331_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));

	//sensor analog gain
	ret += sc2331_write(sd, 0x3e08, (unsigned char)(((again >> 8) & 0xff)));
	ret += sc2331_write(sd, 0x3e07, (unsigned char)(again & 0xff));
	if (ret < 0)
		return ret;

	return ret;
}
#else
static int sc2331_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	ret += sc2331_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc2331_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc2331_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));

	if (ret < 0)
		return ret;

	return ret;
}

static int sc2331_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	//sensor analog gain
	ret += sc2331_write(sd, 0x3e09, (unsigned char)(((again >> 8) & 0xff)));
	//sensor dig fine gain
	ret += sc2331_write(sd, 0x3e07, (unsigned char)(again & 0xff));
	if (ret < 0)
		return ret;

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc2331_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc2331_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc2331_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = sc2331_attr_set(sd, wsize);
	}

	return ret;
}

static int sc2331_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 0x9c4 * 0x450 * 20;  /**< HTS * VTS * FPS */
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
	ret += sc2331_read(sd, 0x320c, &val);
	hts = val << 8;
	val = 0;
	ret += sc2331_read(sd, 0x320d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: sc2331 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	sc2331_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	sc2331_write(sd, 0x320e, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: sc2331_write err\n");
		return ret;
	}

	sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->integration_time.max = vts - 7;
#else
	/* WDR mode */
	switch (info->default_boot) {
	case 0:
		sensor->video.attr->total_height = vts;
		sensor->video.attr->integration_time.max = vts - 6;
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

static int sc2331_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	int ret = ISP_SUCCESS;
	uint8_t val;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		/* sensor->video.mbus.code = wsize->mbus_code; */
		val &= 0x99;
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		/* sensor->video.mbus.code = wsize->mbus_code; */
		val = ((val & 0x9F) | 0x06);
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		/* sensor->video.mbus.code = wsize->mbus_code; */
		val = ((val & 0xF9) | 0x60);
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		/* sensor->video.mbus.code = wsize->mbus_code; */
		val = 0x66;
		break;
	}
	sc2331_write(sd, 0x3221, val);

	sensor->video.hvflip_mode = par->hvflip;
	sc2331_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int sc2331_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int sc2331_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int sc2331_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc2331_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = sc2331_write_array(sd, sc2331_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		sc2331_setting_select(sd, 1);
		sc2331_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		sc2331_setting_select(sd, 0);
		sc2331_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int sc2331_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = sc2331_write_array(sd, wsize->regs);
	ret = sc2331_write_array(sd, sc2331_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int sc2331_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = sc2331_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sc2331_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sc2331_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sc2331_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sc2331_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sc2331_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = sc2331_write_array(sd, sc2331_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = sc2331_write_array(sd, sc2331_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sc2331_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = sc2331_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = sc2331_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = sc2331_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = sc2331_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = sc2331_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = sc2331_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops sc2331_core_ops = {
	.g_chip_ident = sc2331_g_chip_ident,
	.reset = sc2331_reset,
	.init = sc2331_init,
	.g_register = sc2331_g_register,
	.s_register = sc2331_s_register,
};

static struct tx_isp_subdev_video_ops sc2331_video_ops = {
	.s_stream = sc2331_s_stream,
};

static struct tx_isp_subdev_sensor_ops sc2331_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = sc2331_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops sc2331_ops = {
	.core = &sc2331_core_ops,
	.video = &sc2331_video_ops,
	.sensor = &sc2331_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "sc2331",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc2331_probe(struct i2c_client *client,
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
	sensor->video.attr = &sc2331_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc2331_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc2331\n");

	return 0;
}

static int sc2331_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc2331_id[] = {
	{"sc2331", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, sc2331_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver sc2331_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "sc2331",
	},
	.probe          = sc2331_probe,
	.remove         = sc2331_remove,
	.id_table       = sc2331_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_sc2331(void) {
	return private_i2c_add_driver(&sc2331_driver);
}

static __exit void exit_sc2331(void) {
	private_i2c_del_driver(&sc2331_driver);
}

module_init(init_sc2331);
module_exit(exit_sc2331);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_sc2331(void) {
	return private_i2c_add_driver(&sc2331_driver);
}

static void exit_first_sc2331(void) {
	private_i2c_del_driver(&sc2331_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "sc2331",
	.i2c_addr = 0x30,
	.width = 1920,
	.height = 1080,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_sc2331,
	.exit_sensor = exit_first_sc2331
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for sc2331 sensor");
MODULE_LICENSE("GPL");
