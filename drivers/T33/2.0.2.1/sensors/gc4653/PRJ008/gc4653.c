/*
 * gc4653.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps        dvdd
 *  0           2560*1440       30        mipi_2lane    linear  10        30           null
 *
 * @I2C addr:0x29,0x21
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
#define SENSOR_VERSION  "H20251101a"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x46)
#define SENSOR_CHIP_ID_L    (0x53)
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

struct tx_isp_sensor_attribute gc4653_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int index;
	unsigned char reg2b3;
	unsigned char reg2b4;
	unsigned char reg2b8;
	unsigned char reg2b9;
	unsigned char reg515;
	unsigned char reg519;
	unsigned char reg2d9;
	unsigned int gain;
};

struct again_lut gc4653_again_lut[] = {
	{0x00, 0x00, 0x0, 0x01, 0x00, 0x30, 0x1e, 0x5c, 0},   // 1.000000
	{0x01, 0x20, 0x0, 0x01, 0x0b, 0x30, 0x1e, 0x5c, 14995},   // 1.171875
	{0x02, 0x01, 0x0, 0x01, 0x19, 0x30, 0x1d, 0x5b, 31177},   // 1.390625
	{0x03, 0x21, 0x0, 0x01, 0x2a, 0x30, 0x1e, 0x5c, 47704},   // 1.656250
	{0x04, 0x02, 0x0, 0x02, 0x00, 0x30, 0x1e, 0x5c, 65536},   // 2.000000
	{0x05, 0x22, 0x0, 0x02, 0x17, 0x30, 0x1d, 0x5b, 81159},   // 2.359375
	{0x06, 0x03, 0x0, 0x02, 0x33, 0x20, 0x16, 0x54, 97243},   // 2.796875
	{0x07, 0x23, 0x0, 0x03, 0x14, 0x20, 0x17, 0x55, 113240},   // 3.312500
	{0x08, 0x04, 0x0, 0x04, 0x00, 0x20, 0x17, 0x55, 131072},   // 4.000000
	{0x09, 0x24, 0x0, 0x04, 0x2f, 0x20, 0x19, 0x57, 147007},   // 4.734375
	{0x0a, 0x05, 0x0, 0x05, 0x26, 0x20, 0x19, 0x57, 162779},   // 5.593750
	{0x0b, 0x25, 0x0, 0x06, 0x28, 0x20, 0x1b, 0x59, 178776},   // 6.625000
	{0x0c, 0x0c, 0x0, 0x08, 0x00, 0x20, 0x1d, 0x5b, 196608},   // 8.000000
	{0x0d, 0x2c, 0x0, 0x09, 0x1e, 0x20, 0x1f, 0x5d, 212543},   // 9.468750
	{0x0e, 0x0d, 0x0, 0x0b, 0x0c, 0x20, 0x21, 0x5f, 228315},   // 11.187500
	{0x0f, 0x2d, 0x0, 0x0d, 0x11, 0x20, 0x24, 0x62, 244423},   // 13.265625
	{0x10, 0x1c, 0x0, 0x10, 0x00, 0x20, 0x26, 0x64, 262144},   // 16.000000
	{0x11, 0x3c, 0x0, 0x12, 0x3d, 0x18, 0x2a, 0x68, 278158},   // 18.953125
	{0x12, 0x5c, 0x0, 0x16, 0x19, 0x18, 0x2c, 0x6a, 293916},   // 22.390625
	{0x13, 0x7c, 0x0, 0x1a, 0x22, 0x18, 0x2e, 0x6c, 309959},   // 26.531250
	{0x14, 0x9c, 0x0, 0x20, 0x00, 0x18, 0x32, 0x70, 327680},   // 32.000000
	{0x15, 0xbc, 0x0, 0x25, 0x3a, 0x18, 0x35, 0x73, 343694},   // 37.906250
	{0x16, 0xdc, 0x0, 0x2c, 0x33, 0x10, 0x36, 0x74, 359485},   // 44.796875
	{0x17, 0xfc, 0x0, 0x35, 0x05, 0x10, 0x38, 0x76, 375523},   // 53.078125
	{0x18, 0x1c, 0x1, 0x40, 0x00, 0x10, 0x3c, 0x7a, 393216},   // 64.000000
	{0x19, 0x3c, 0x1, 0x4b, 0x35, 0x10, 0x42, 0x80, 409249},   // 75.828125
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int gc4653_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = gc4653_again_lut;
	while (lut->gain <= gc4653_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].index;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == gc4653_attr.again.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
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
unsigned int gc4653_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = gc4653_again_lut;
	while(lut->gain <= gc4653_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == gc4653_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int gc4653_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int gc4653_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int gc4653_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus gc4653_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 864,
	.lans = 2,
	.image_twidth = 2560,
	.image_theight = 1440,
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

struct tx_isp_dvp_bus gc4653_dvp = {
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

struct tx_isp_sensor_attribute gc4653_attr = {
	.name = "gc4653",
	.chip_id = 0x4023,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x29,
	.sensor_ctrl.alloc_again = gc4653_alloc_again,
	.sensor_ctrl.alloc_dgain = gc4653_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = gc4653_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list gc4653_init_regs_2560_1440_30fps_mipi[] = {
	//version 6.8
	//mclk 24Mhz
	//mipi_data_rate 648Mbps
	//framelength 1500
	//linelength 4800
	//pclk 216Mhz
	//rowtime 22.2222us
	//pattern grbg
	{0x03fe,0xf0},
	{0x03fe,0x00},
	{0x0317,0x00},
	{0x0320,0x77},
	{0x0324,0xc8},
	{0x0325,0x06},
	{0x0326,0x6c},
	{0x0327,0x03},
	{0x0334,0x40},
	{0x0336,0x6c},
	{0x0337,0x82},
	{0x0315,0x25},
	{0x031c,0xc6},
	{0x0287,0x18},
	{0x0084,0x00},
	{0x0087,0x50},
	{0x029d,0x08},
	{0x0290,0x00},
	{0x0340,0x05},//vts = 0x5dc = 1500
	{0x0341,0xdc},//
	{0x0345,0x06},
	{0x034b,0xb0},
	{0x0352,0x08},
	{0x0354,0x08},
	{0x02d1,0xe0},
	{0x0223,0xf2},
	{0x0238,0xa4},
	{0x02ce,0x7f},
	{0x0232,0xc4},
	{0x02d3,0x05},
	{0x0243,0x06},
	{0x02ee,0x30},
	{0x026f,0x70},
	{0x0257,0x09},
	{0x0211,0x02},
	{0x0219,0x09},
	{0x023f,0x2d},
	{0x0518,0x00},
	{0x0519,0x01},
	{0x0515,0x08},
	{0x02d9,0x3f},
	{0x02da,0x02},
	{0x02db,0xe8},
	{0x02e6,0x20},
	{0x021b,0x10},
	{0x0252,0x22},
	{0x024e,0x22},
	{0x02c4,0x01},
	{0x021d,0x17},
	{0x024a,0x01},
	{0x02ca,0x02},
	{0x0262,0x10},
	{0x029a,0x20},
	{0x021c,0x0e},
	{0x0298,0x03},
	{0x029c,0x00},
	{0x027e,0x14},
	{0x02c2,0x10},
	{0x0540,0x20},
	{0x0546,0x01},
	{0x0548,0x01},
	{0x0544,0x01},
	{0x0242,0x1b},
	{0x02c0,0x1b},
	{0x02c3,0x20},
	{0x02e4,0x10},
	{0x022e,0x00},
	{0x027b,0x3f},
	{0x0269,0x0f},
	{0x02d2,0x40},
	{0x027c,0x08},
	{0x023a,0x2e},
	{0x0245,0xce},
	{0x0530,0x20},
	{0x0531,0x02},
	{0x0228,0x50},
	{0x02ab,0x00},
	{0x0250,0x00},
	{0x0221,0x50},
	{0x02ac,0x00},
	{0x02a5,0x02},
	{0x0260,0x0b},
	{0x0216,0x04},
	{0x0299,0x1C},
	{0x02bb,0x0d},
	{0x02a3,0x02},
	{0x02a4,0x02},
	{0x021e,0x02},
	{0x024f,0x08},
	{0x028c,0x08},
	{0x0532,0x3f},
	{0x0533,0x02},
	{0x0277,0xc0},
	{0x0276,0xc0},
	{0x0239,0xc0},
	{0x0202,0x05},
	{0x0203,0xd0},
	{0x0205,0xc0},
	{0x02b0,0x68},
	{0x0002,0xa9},
	{0x0004,0x01},
	{0x021a,0x98},
	{0x0266,0xa0},
	{0x0020,0x01},
	{0x0021,0x03},
	{0x0022,0x00},
	{0x0023,0x04},
	{0x0342,0x06},//hts = 0x640 = 1600
	{0x0343,0x40},//
	{0x03fe,0x10},
	{0x03fe,0x00},
	{0x0106,0x78},
	{0x0108,0x0c},
	{0x0114,0x01},
	{0x0115,0x12},
	{0x0180,0x46},
	{0x0181,0x30},
	{0x0182,0x05},
	{0x0185,0x01},
	{0x03fe,0x10},
	{0x03fe,0x00},
	{0x000f,0x00},
	{0x0100,0x09},//stream on
	{0x0080,0x02},
	{0x0097,0x0a},
	{0x0098,0x10},
	{0x0099,0x05},
	{0x009a,0xb0},
	{0x0317,0x08},
	{0x0a67,0x80},
	{0x0a70,0x03},
	{0x0a82,0x00},
	{0x0a83,0x10},
	{0x0a80,0x2b},
	{0x05be,0x00},
	{0x05a9,0x01},
	{0x0313,0x80},
	{0x05be,0x01},
	{0x0317,0x00},
	{0x0a67,0x00},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the gc4653_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting gc4653_win_sizes[] = {
	{
		.width          = 2560,
		.height         = 1440,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SGRBG10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = gc4653_init_regs_2560_1440_30fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &gc4653_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list gc4653_stream_on_mipi[] = {
	{0x0320, 0x77},
	{0x0334, 0x40},
	{0x0324, 0xc8},
	{0x0317, 0x00},
	{0x0180, 0x46},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x031d, 0x2d},
	{0x0100, 0x09},
	{0x031d, 0x28},
	{SENSOR_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list gc4653_stream_off_mipi[] = {
	{0x031d, 0x2d},
	{0x0180, 0x06},
	{0x0100, 0x08},
	{0x0324, 0x88},
	{0x0334, 0x00},
	{0x0320, 0x66},
	{0x0317, 0x01},
	{0x031d, 0x28},
	{SENSOR_REG_END, 0x00}, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int gc4653_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int gc4653_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int gc4653_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc4653_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int gc4653_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc4653_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int gc4653_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int gc4653_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int gc4653_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc4653_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int gc4653_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc4653_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int gc4653_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = ISP_SUCCESS;

	rate = private_clk_get_rate(sensor->mclk);
	if(((rate / 1000) % (mclk / 1000)) != 0) {
		switch(mclk) {
		case 24000000:
			ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
			break;
		case 27000000:
		case 37125000:
			ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK1));
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

	private_clk_set_rate(sensor->mclk, mclk);
	private_clk_prepare_enable(sensor->mclk);

error:
	return ret;
}

static int gc4653_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int gc4653_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &gc4653_win_sizes[0];
		gc4653_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		gc4653_attr.again.value = 65536;
		gc4653_attr.again.max = 409249;
		gc4653_attr.again.min = 0;
		gc4653_attr.again.apply_delay = 2;

		gc4653_attr.integration_time.value = 0x5b8;
		gc4653_attr.integration_time.max = 1500 - 8;
		gc4653_attr.integration_time.min = 2;
		gc4653_attr.integration_time.apply_delay = 2;

		gc4653_attr.total_width = 1600;
		gc4653_attr.total_height = 1500;

		gc4653_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
		gc4653_attr.hcg.base_gain = ;
		gc4653_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		gc4653_attr.again_short.value = ;
		gc4653_attr.again_short.max = ;
		gc4653_attr.again_short.min = ;
		gc4653_attr.again_short.apply_delay = ;

		gc4653_attr.integration_time_short.value = ;
		gc4653_attr.integration_time_short.max = ;
		gc4653_attr.integration_time_short.min = ;
		gc4653_attr.integration_time_short.apply_delay = ;

		gc4653_attr.wdr_cache = wdr_line * gc4653_attr.total_width;

#ifdef SENSOR_HCG
		gc4653_attr.hcg_short.base_gain = ;
		gc4653_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(gc4653_attr.mipi)), (void *)(&gc4653_mipi_linear), sizeof(gc4653_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int gc4653_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	gc4653_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		gc4653_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc4653_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		gc4653_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		gc4653_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		gc4653_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = gc4653_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.apply_delay = 1;
	gc4653_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int gc4653_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = gc4653_read(sd, 0x03f0, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = gc4653_read(sd, 0x03f1, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int gc4653_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	gc4653_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "gc4653_reset");
		if (!ret) {
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->rst_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
		}
	}
	if (info->pwdn_gpio != -1) {
		ret = private_gpio_request(info->pwdn_gpio, "gc4653_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = gc4653_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc4653 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", gc4653_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
		    info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "gc4653", sizeof("gc4653"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int gc4653_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int gc4653_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = gc4653_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int gc4653_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = gc4653_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int gc4653_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc4653_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int gc4653_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = gc4653_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = gc4653_write_array(sd, gc4653_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("gc4653 stream on\n");
		}

	} else {
		ret = gc4653_write_array(sd, gc4653_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("gc4653 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int gc4653_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;
	struct again_lut *val_lut = gc4653_again_lut;

	/*set integration time*/
	ret = gc4653_write(sd, 0x0203, it & 0xff);
	ret += gc4653_write(sd, 0x0202, it >> 8);

	/*set sensor analog gain*/
	ret = gc4653_write(sd, 0x02b3, val_lut[again].reg2b3);
	ret = gc4653_write(sd, 0x02b4, val_lut[again].reg2b4);
	ret = gc4653_write(sd, 0x02b8, val_lut[again].reg2b8);
	ret = gc4653_write(sd, 0x02b9, val_lut[again].reg2b9);
	ret = gc4653_write(sd, 0x0515, val_lut[again].reg515);
	ret = gc4653_write(sd, 0x0519, val_lut[again].reg519);
	ret = gc4653_write(sd, 0x02d9, val_lut[again].reg2d9);

	return ret;
}
#else
static int gc4653_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	/*set integration time*/
	ret = gc5603_write(sd, 0x0203, value & 0xff);
	ret += gc5603_write(sd, 0x0202, value >> 8);

	return ret;
}

static int gc4653_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	struct again_lut *val_lut = gc4653_again_lut;


	/*set sensor analog gain*/
	ret = gc4653_write(sd, 0x02b3, val_lut[value].reg2b3);
	ret = gc4653_write(sd, 0x02b4, val_lut[value].reg2b4);
	ret = gc4653_write(sd, 0x02b8, val_lut[value].reg2b8);
	ret = gc4653_write(sd, 0x02b9, val_lut[value].reg2b9);
	ret = gc4653_write(sd, 0x0515, val_lut[value].reg515);
	ret = gc4653_write(sd, 0x0519, val_lut[value].reg519);
	ret = gc4653_write(sd, 0x02d9, val_lut[value].reg2d9);

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc4653_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc4653_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc4653_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = gc4653_attr_set(sd, wsize);
	}

	return ret;
}

static int gc4653_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 1600 * 1500 * 30;  /**< HTS * VTS * FPS */
		max_fps = 30;
		break;
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	/* max_fps = wsize->fps; */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d/%d) no in range\n", fps >> 16, fps & 0xffff);
		return -1;
	}

	val = 0;
	ret += gc4653_read(sd, 0x0342, &val);
	hts = (val & 0x0f) << 8;
	val = 0;
	ret += gc4653_read(sd, 0x0343, &val);
	hts  = (hts | val);
	if (0 != ret) {
		ISP_ERROR("err: gc4653 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	gc4653_write(sd, 0x0340, (unsigned char) ((vts >> 8) & 0x3f));
	gc4653_write(sd, 0x0341, (unsigned char) (vts & 0xff));

	/* gc4653_write(sd, 0x0259, (unsigned char)(((vts - 32) >> 8 ) & 0x3f)); */
	/* gc4653_write(sd, 0x025a, (unsigned char)((vts - 32) & 0xff)); */

	if (0 != ret) {
		ISP_ERROR("err: gc4653_write err\n");
		return ret;
	}

	sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->integration_time.max = vts - 8;
#else
	/* WDR mode */
	switch (info->default_boot) {
	case 0:
		sensor->video.attr->total_height = vts;
		sensor->video.attr->integration_time.max = vts - 8;
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

static int gc4653_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = ISP_SUCCESS;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	ret = gc4653_write(sd, 0x031d, 0x2d);
	ret = gc4653_write(sd, 0x008e, 0x00);
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		ret = gc4653_write(sd, 0x0101, 0x00);
		ret = gc4653_write(sd, 0x0a73, 0x60);
		ret = gc4653_write(sd, 0x0101, 0x00);
		ret = gc4653_write(sd, 0x0347, 0x02);
		ret = gc4653_write(sd, 0x0352, 0x08);
		ret = gc4653_write(sd, 0x0354, 0x08);
		ret = gc4653_write(sd, 0x0a73, 0x60);
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		ret = gc4653_write(sd, 0x0101, 0x01);
		ret = gc4653_write(sd, 0x0a73, 0x61);
		ret = gc4653_write(sd, 0x0101, 0x01);
		ret = gc4653_write(sd, 0x0347, 0x03);
		ret = gc4653_write(sd, 0x0352, 0x09);
		ret = gc4653_write(sd, 0x0354, 0x09);
		ret = gc4653_write(sd, 0x0a73, 0x61);
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		ret = gc4653_write(sd, 0x0101, 0x02);
		ret = gc4653_write(sd, 0x0a73, 0x62);
		ret = gc4653_write(sd, 0x0101, 0x02);
		ret = gc4653_write(sd, 0x0347, 0x02);
		ret = gc4653_write(sd, 0x0352, 0x09);
		ret = gc4653_write(sd, 0x0354, 0x08);
		ret = gc4653_write(sd, 0x0a73, 0x62);
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		ret = gc4653_write(sd, 0x0101, 0x03);
		ret = gc4653_write(sd, 0x0a73, 0x63);
		ret = gc4653_write(sd, 0x0101, 0x03);
		ret = gc4653_write(sd, 0x0347, 0x03);
		ret = gc4653_write(sd, 0x0352, 0x08);
		ret = gc4653_write(sd, 0x0354, 0x09);
		ret = gc4653_write(sd, 0x0a73, 0x63);
		break;
	}
	ret = gc4653_write(sd, 0x0080, 0x02);
	ret = gc4653_write(sd, 0x0097, 0x0a);
	ret = gc4653_write(sd, 0x0098, 0x10);
	ret = gc4653_write(sd, 0x0099, 0x05);
	ret = gc4653_write(sd, 0x009a, 0xb0);
	ret = gc4653_write(sd, 0x0317, 0x08);
	ret = gc4653_write(sd, 0x0a67, 0x80);
	ret = gc4653_write(sd, 0x0a70, 0x03);
	ret = gc4653_write(sd, 0x0a82, 0x00);
	ret = gc4653_write(sd, 0x0a83, 0x10);
	ret = gc4653_write(sd, 0x0a80, 0x2b);
	ret = gc4653_write(sd, 0x05be, 0x00);
	ret = gc4653_write(sd, 0x05a9, 0x01);
	ret = gc4653_write(sd, 0x0313, 0x80);
	ret = gc4653_write(sd, 0x05be, 0x01);
	ret = gc4653_write(sd, 0x0317, 0x00);
	ret = gc4653_write(sd, 0x0a67, 0x00);
	ret = gc4653_write(sd, 0x031d, 0x28);

	sensor->video.hvflip_mode = par->hvflip;
	gc4653_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int gc4653_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int gc4653_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int gc4653_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int gc4653_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = gc4653_write_array(sd, gc4653_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		gc4653_setting_select(sd, 1);
		gc4653_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		gc4653_setting_select(sd, 0);
		gc4653_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int gc4653_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = gc4653_write_array(sd, wsize->regs);
	ret = gc4653_write_array(sd, gc4653_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int gc4653_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = gc4653_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = gc4653_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = gc4653_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = gc4653_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = gc4653_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = gc4653_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = gc4653_write_array(sd, gc4653_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = gc4653_write_array(sd, gc4653_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = gc4653_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = gc4653_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = gc4653_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = gc4653_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = gc4653_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = gc4653_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = gc4653_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops gc4653_core_ops = {
	.g_chip_ident = gc4653_g_chip_ident,
	.reset = gc4653_reset,
	.init = gc4653_init,
	.g_register = gc4653_g_register,
	.s_register = gc4653_s_register,
};

static struct tx_isp_subdev_video_ops gc4653_video_ops = {
	.s_stream = gc4653_s_stream,
};

static struct tx_isp_subdev_sensor_ops gc4653_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = gc4653_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops gc4653_ops = {
	.core = &gc4653_core_ops,
	.video = &gc4653_video_ops,
	.sensor = &gc4653_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "gc4653",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc4653_probe(struct i2c_client *client,
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
	sensor->video.attr = &gc4653_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc4653_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->gc4653\n");

	return 0;
}

static int gc4653_remove(struct i2c_client *client)
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

static const struct i2c_device_id gc4653_id[] = {
	{"gc4653", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, gc4653_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver gc4653_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "gc4653",
	},
	.probe          = gc4653_probe,
	.remove         = gc4653_remove,
	.id_table       = gc4653_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_gc4653(void) {
	return private_i2c_add_driver(&gc4653_driver);
}

static __exit void exit_gc4653(void) {
	private_i2c_del_driver(&gc4653_driver);
}

module_init(init_gc4653);
module_exit(exit_gc4653);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_gc4653(void) {
	return private_i2c_add_driver(&gc4653_driver);
}

static void exit_first_gc4653(void) {
	private_i2c_del_driver(&gc4653_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "gc4653",
	.i2c_addr = 0x29,
	.width = 2560,
	.height = 1440,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_gc4653,
	.exit_sensor = exit_first_gc4653
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for gc4653 sensor");
MODULE_LICENSE("GPL");
