/*
 * sc431hai.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps		dvdd
 *  0           2560*1440       30        mipi_2lane    linear  10        30
 *
 * @I2C addr:0x30, 0x32
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
#define SENSOR_VERSION  "H20250610a"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< Switch of the image flipping function */
//#define SENSOR_HCG
//#define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0xcd)
#define SENSOR_CHIP_ID_L    (0x6b)
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

struct tx_isp_sensor_attribute sc431hai_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sc431hai_again_lut[] = {
    {0x0020, 0},
    {0x0021, 2886},
    {0x0022, 5776},
    {0x0023, 8494},
    {0x0024, 11136},
    {0x0025, 13706},
    {0x0026, 16288},
    {0x0027, 18724},
    {0x0028, 21098},
    {0x0029, 23414},
    {0x002a, 25747},
    {0x002b, 27953},
    {0x002c, 30109},
    {0x002d, 32217},
    {0x002e, 34345},
    {0x002f, 36362},
    {0x0030, 38336},
    {0x0031, 40270},
    {0x8020, 40824},
    {0x8021, 43726},
    {0x8022, 46542},
    {0x8023, 49276},
    {0x8024, 51988},
    {0x8025, 54571},
    {0x8026, 57085},
    {0x8027, 59535},
    {0x8028, 61922},
    {0x8029, 64251},
    {0x802a, 66524},
    {0x802b, 68743},
    {0x802c, 70956},
    {0x802d, 73075},
    {0x802e, 75147},
    {0x802f, 77175},
    {0x8030, 79160},
    {0x8031, 81105},
    {0x8032, 83010},
    {0x8033, 84878},
    {0x8034, 86747},
    {0x8035, 88543},
    {0x8036, 90306},
    {0x8037, 92036},
    {0x8038, 93735},
    {0x8039, 95404},
    {0x803a, 97045},
    {0x803b, 98657},
    {0x803c, 100275},
    {0x803d, 101833},
    {0x803e, 103366},
    {0x803f, 104875},
    {0x8120, 106360},
    {0x8121, 109262},
    {0x8122, 112107},
    {0x8123, 114840},
    {0x8124, 117497},
    {0x8125, 120080},
    {0x8126, 122621},
    {0x8127, 125071},
    {0x8128, 127458},
    {0x8129, 129787},
    {0x812a, 132083},
    {0x812b, 134302},
    {0x812c, 136470},
    {0x812d, 138589},
    {0x812e, 140683},
    {0x812f, 142711},
    {0x8130, 144696},
    {0x8131, 146641},
    {0x8132, 148566},
    {0x8133, 150433},
    {0x8134, 152264},
    {0x8135, 154061},
    {0x8136, 155842},
    {0x8137, 157572},
    {0x8138, 159271},
    {0x8139, 160940},
    {0x813a, 162597},
    {0x813b, 164209},
    {0x813c, 165794},
    {0x813d, 167353},
    {0x813e, 168902},
    {0x813f, 170411},
    {0x8320, 171896},
    {0x8321, 174813},
    {0x8322, 177628},
    {0x8323, 180376},
    {0x8324, 183033},
    {0x8325, 185630},
    {0x8326, 188145},
    {0x8327, 190607},
    {0x8328, 192994},
    {0x8329, 195335},
    {0x832a, 197607},
    {0x832b, 199838},
    {0x832c, 202006},
    {0x832d, 204136},
    {0x832e, 206209},
    {0x832f, 208247},
    {0x8330, 210232},
    {0x8331, 212187},
    {0x8332, 214092},
    {0x8333, 215969},
    {0x8334, 217800},
    {0x8335, 219606},
    {0x8336, 221369},
    {0x8337, 223108},
    {0x8338, 224807},
    {0x8339, 226485},
    {0x833a, 228125},
    {0x833b, 229745},
    {0x833c, 231330},
    {0x833d, 232897},
    {0x833e, 234431},
    {0x833f, 235947},
    {0x8720, 237432},
    {0x8721, 240342},
    {0x8722, 243164},
    {0x8723, 245905},
    {0x8724, 248569},
    {0x8725, 251159},
    {0x8726, 253681},
    {0x8727, 256136},
    {0x8728, 258530},
    {0x8729, 260865},
    {0x872a, 263143},
    {0x872b, 265368},
    {0x872c, 267542},
    {0x872d, 269666},
    {0x872e, 271744},
    {0x872f, 273778},
    {0x8730, 275768},
    {0x8731, 277718},
    {0x8732, 279628},
    {0x8733, 281500},
    {0x8734, 283336},
    {0x8735, 285137},
    {0x8736, 286905},
    {0x8737, 288640},
    {0x8738, 290343},
    {0x8739, 292017},
    {0x873a, 293661},
    {0x873b, 295277},
    {0x873c, 296866},
    {0x873d, 298429},
    {0x873e, 299967},
    {0x873f, 301479},
    {0x8f20, 302968},
    {0x8f21, 305878},
    {0x8f22, 308700},
    {0x8f23, 311441},
    {0x8f24, 314105},
    {0x8f25, 316695},
    {0x8f26, 319217},
    {0x8f27, 321672},
    {0x8f28, 324066},
    {0x8f29, 326401},
    {0x8f2a, 328679},
    {0x8f2b, 330904},
    {0x8f2c, 333078},
    {0x8f2d, 335202},
    {0x8f2e, 337280},
    {0x8f2f, 339314},
    {0x8f30, 341304},
    {0x8f31, 343254},
    {0x8f32, 345164},
    {0x8f33, 347036},
    {0x8f34, 348872},
    {0x8f35, 350673},
    {0x8f36, 352441},
    {0x8f37, 354176},
    {0x8f38, 355879},
    {0x8f39, 357553},
    {0x8f3a, 359197},
    {0x8f3b, 360813},
    {0x8f3c, 362402},
    {0x8f3d, 363965},
    {0x8f3e, 365503},
    {0x8f3f, 367015},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int sc431hai_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc431hai_again_lut;
	while (lut->gain <= sc431hai_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sc431hai_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int sc431hai_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc431hai_again_lut;
	while(lut->gain <= sc431hai_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc431hai_attr.again_short.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}
#else
	/* Non analog gain table */

	...;
#endif /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

	return isp_gain;
}
#endif /* SENSOR_WDR_2_FRAME */

unsigned int sc431hai_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int sc431hai_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int sc431hai_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus sc431hai_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 630,
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

struct tx_isp_dvp_bus sc431hai_dvp = {
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

struct tx_isp_sensor_attribute sc431hai_attr = {
	.name = "sc431hai",
	.chip_id = 0xcd2e,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.sensor_ctrl.alloc_again = sc431hai_alloc_again,
	.sensor_ctrl.alloc_dgain = sc431hai_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = sc431hai_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list sc431hai_init_regs_2560_1440_30fps_mipi[] = {
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x37f9,0x80},
    {0x3018,0x3a},
    {0x3019,0x0c},
    {0x301f,0x07},
    {0x3058,0x21},
    {0x3059,0x53},
    {0x305a,0x40},
    {0x320c,0x05},//hts = 0x578 = 1400
    {0x320d,0x78},//
    {0x320e,0x05},//vts = 0x5dc = 1500
    {0x320f,0xdc},//
    {0x3250,0x00},
    {0x3301,0x0c},
    {0x3304,0x50},
    {0x3305,0x00},
    {0x3306,0x50},
    {0x3307,0x04},
    {0x3308,0x0a},
    {0x3309,0x60},
    {0x330b,0xc8},
    {0x330d,0x08},
    {0x330e,0x38},
    {0x331e,0x41},
    {0x331f,0x51},
    {0x3333,0x10},
    {0x3334,0x40},
    {0x3364,0x5e},
    {0x338e,0xe2},
    {0x338f,0x80},
    {0x3390,0x08},
    {0x3391,0x18},
    {0x3392,0xb8},
    {0x3393,0x12},
    {0x3394,0x14},
    {0x3395,0x10},
    {0x3396,0x88},
    {0x3397,0x98},
    {0x3398,0xb8},
    {0x3399,0x10},
    {0x339a,0x16},
    {0x339b,0x1c},
    {0x339c,0x40},
    {0x33ac,0x0a},
    {0x33ad,0x10},
    {0x33ae,0x4f},
    {0x33af,0x5e},
    {0x33b2,0x50},
    {0x33b3,0x10},
    {0x33f8,0x00},
    {0x33f9,0x50},
    {0x33fa,0x00},
    {0x33fb,0x50},
    {0x33fc,0x48},
    {0x33fd,0x78},
    {0x349f,0x03},
    {0x34a6,0x40},
    {0x34a7,0x58},
    {0x34a8,0x10},
    {0x34a9,0x10},
    {0x34f8,0x78},
    {0x34f9,0x10},
    {0x3633,0x44},
    {0x363b,0x8f},
    {0x363c,0x02},
    {0x3641,0x08},
    {0x3654,0x20},
    {0x3674,0xc2},
    {0x3675,0xb4},
    {0x3676,0x88},
    {0x367c,0x88},
    {0x367d,0xb8},
    {0x3690,0x34},
    {0x3691,0x44},
    {0x3692,0x54},
    {0x3693,0x88},
    {0x3694,0x98},
    {0x3696,0x80},
    {0x3697,0x83},
    {0x3698,0x81},
    {0x3699,0x81},
    {0x369a,0x84},
    {0x369b,0x82},
    {0x36a2,0x80},
    {0x36a3,0x88},
    {0x36a4,0xf8},
    {0x36a5,0xb8},
    {0x36a6,0x98},
    {0x36d0,0x15},
    {0x36ea,0x23},
    {0x36eb,0x0d},
    {0x36ec,0x55},
    {0x36ed,0x18},
    {0x370f,0x01},
    {0x3722,0x03},
    {0x3724,0x92},
    {0x3727,0x14},
    {0x37b0,0x17},
    {0x37b1,0x9b},
    {0x37b2,0x9b},
    {0x37b3,0x88},
    {0x37b4,0xb8},
    {0x37fa,0x23},
    {0x37fb,0x54},
    {0x37fc,0x21},
    {0x37fd,0x1c},
    {0x391f,0x41},
    {0x3926,0xe0},
    {0x3933,0x80},
    {0x3934,0xf8},
    {0x3935,0x00},
    {0x3936,0x45},
    {0x3937,0x66},
    {0x3938,0x66},
    {0x3939,0x00},
    {0x393a,0x03},
    {0x393b,0x00},
    {0x393c,0x00},
    {0x393d,0x02},
    {0x393e,0x80},
    {0x3e00,0x00},
    {0x3e01,0xba},
    {0x3e02,0xd0},
    {0x3e16,0x00},
    {0x3e17,0xc5},
    {0x3e18,0x00},
    {0x3e19,0xc5},
    {0x4509,0x20},
    {0x450d,0x0b},
    {0x4819,0x08},
    {0x481b,0x05},
    {0x481d,0x11},
    {0x481f,0x04},
    {0x4821,0x09},
    {0x4823,0x05},
    {0x4825,0x04},
    {0x4827,0x04},
    {0x4829,0x07},
    {0x5780,0x76},
    {0x5784,0x0a},
    {0x5785,0x04},
    {0x5787,0x0a},
    {0x5788,0x0a},
    {0x5789,0x08},
    {0x578a,0x0a},
    {0x578b,0x0a},
    {0x578c,0x08},
    {0x578d,0x40},
    {0x5790,0x08},
    {0x5791,0x04},
    {0x5792,0x04},
    {0x5793,0x08},
    {0x5794,0x04},
    {0x5795,0x04},
    {0x57ac,0x00},
    {0x57ad,0x00},
    {0x36e9,0x53},
    {0x37f9,0x53},
    {0x0100,0x01},
    {SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the sc431hai_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc431hai_win_sizes[] = {
	/* 2560*1440 [0] */
	{
		.width          = 2560,
		.height         = 1440,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc431hai_init_regs_2560_1440_30fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &sc431hai_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sc431hai_stream_on_mipi[] = {
	{0x0100, 0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list sc431hai_stream_off_mipi[] = {
	{0x0100, 0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int sc431hai_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int sc431hai_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int sc431hai_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc431hai_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc431hai_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc431hai_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int sc431hai_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int sc431hai_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc431hai_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc431hai_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int sc431hai_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc431hai_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int sc431hai_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int sc431hai_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int sc431hai_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &sc431hai_win_sizes[0];
		sc431hai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc431hai_attr.again.value = 0;
		sc431hai_attr.again.max = 367015;
		sc431hai_attr.again.min = 0;
		sc431hai_attr.again.apply_delay = 2;

		sc431hai_attr.integration_time.value = 0xbad / 2;
		sc431hai_attr.integration_time.max = 1500 - 6;
		sc431hai_attr.integration_time.min = 2;
		sc431hai_attr.integration_time.apply_delay = 2;

		sc431hai_attr.total_width = 1400;
		sc431hai_attr.total_height = 1500;

		sc431hai_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		sc431hai_attr.hcg.base_gain = ;
		sc431hai_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc431hai_attr.again_short.value = ;
		sc431hai_attr.again_short.max = ;
		sc431hai_attr.again_short.min = ;
		sc431hai_attr.again_short.apply_delay = ;

		sc431hai_attr.integration_time_short.value = ;
		sc431hai_attr.integration_time_short.max = ;
		sc431hai_attr.integration_time_short.min = ;
		sc431hai_attr.integration_time_short.apply_delay = ;

		sc431hai_attr.wdr_cache = wdr_line * sc431hai_attr.total_width;

#ifdef SENSOR_HCG
		sc431hai_attr.hcg_short.base_gain = ;
		sc431hai_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(sc431hai_attr.mipi)), (void *)(&sc431hai_mipi_linear), sizeof(sc431hai_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int sc431hai_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	sc431hai_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		sc431hai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc431hai_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		sc431hai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc431hai_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		sc431hai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = sc431hai_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
	sc431hai_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int sc431hai_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sc431hai_read(sd, 0x3107, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc431hai_read(sd, 0x3108, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc431hai_g_chip_ident(struct tx_isp_subdev *sd,
								 struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sc431hai_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "sc431hai_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "sc431hai_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = sc431hai_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc431hai chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", sc431hai_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "sc431hai", sizeof("sc431hai"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc431hai_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sc431hai_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = sc431hai_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int sc431hai_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc431hai_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc431hai_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc431hai_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int sc431hai_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = sc431hai_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sc431hai_write_array(sd, sc431hai_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("sc431hai stream on\n");
		}

	} else {
		ret = sc431hai_write_array(sd, sc431hai_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("sc431hai stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int sc431hai_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	it = it << 1;
	ret += sc431hai_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc431hai_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc431hai_write(sd, 0x3e02, (unsigned char)((it & 0xf) << 4));

	ret += sc431hai_write(sd, 0x3e08, (unsigned char)((again >> 8) & 0xff));
	ret += sc431hai_write(sd, 0x3e09, (unsigned char)(again & 0xff));


return ret;
}
#else
static int sc431hai_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	value = value << 1;
	ret += sc431hai_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc431hai_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc431hai_write(sd, 0x3e02, (unsigned char)((value & 0xf) << 4));

	return ret;
}

static int sc431hai_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	ret += sc431hai_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
	ret += sc431hai_write(sd, 0x3e09, (unsigned char)(value & 0xff));

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc431hai_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc431hai_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc431hai_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = sc431hai_attr_set(sd, wsize);
	}

	return ret;
}

static int sc431hai_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 1400 * 1500 * 30;  /**< HTS * VTS * FPS */
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
	ret += sc431hai_read(sd, 0x320c, &val);
	hts = val << 8;
	val = 0;
	ret += sc431hai_read(sd, 0x320d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: sc431hai read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	sc431hai_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	sc431hai_write(sd, 0x320e, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: sc431hai_write err\n");
		return ret;
	}

	sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->integration_time.max = vts - 6;
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

static int sc431hai_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	ret = sc431hai_read(sd, 0x3221, &val);
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		val &= 0x99;
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		val = ((val & 0x99) | 0x06);
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		val = ((val & 0x99) | 0x60);
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		val |= 0x66;
		break;
	}
	ret += sc431hai_write(sd, 0x3221, val);

	sensor->video.hvflip_mode = par->hvflip;
	sc431hai_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int sc431hai_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int sc431hai_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int sc431hai_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc431hai_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = sc431hai_write_array(sd, sc431hai_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		sc431hai_setting_select(sd, 1);
		sc431hai_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		sc431hai_setting_select(sd, 0);
		sc431hai_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int sc431hai_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = sc431hai_write_array(sd, wsize->regs);
	ret = sc431hai_write_array(sd, sc431hai_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int sc431hai_pm_s_stream_ctrl(struct tx_isp_subdev *sd, void *arg)
{
	int ret = ISP_SUCCESS;
	IMPISPUserCtl *data = (IMPISPUserCtl *)arg;
	if(data->cmd == TX_SENSOR_PM_RESUME){
		ret = sc431hai_write_array(sd, sc431hai_stream_on_mipi);
		if(ret != 0){
			printk("%s streamon failed\n",sc431hai_attr.name);
			return ret;
		}
		printk("%s TX_SENSOR_PM_RESUME\n",sc431hai_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_SUSPEND) {
		ret = sc431hai_write_array(sd, sc431hai_stream_off_mipi);
		if (ret < 0) {
			printk("\n TX_SENSOR_PM_SUSPEND error !!!\n");
			return ret;
		}
		printk("%s TX_SENSOR_PM_SUSPEND.\n",sc431hai_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_PREPARE) {
		printk("%s TX_SENSOR_PM_PREPARE.\n",sc431hai_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_COMPLETE) {
		printk("%s TX_SENSOR_PM_COMPLETE.\n",sc431hai_attr.name);

	}else if(data->cmd == TX_SENSOR_PM_STREAMON) {
		ret = sc431hai_write_array(sd, sc431hai_stream_on_mipi);
		if (ret < 0) {
			printk("\n TX_SENSOR_PM_STREAMON error !!!\n");
			return ret;
		}
		printk("%s TX_SENSOR_PM_STREAMON.\n",sc431hai_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_STREAMOFF) {
		ret = sc431hai_write_array(sd, sc431hai_stream_off_mipi);
		if (ret < 0) {
			printk("\n [%s %d] TX_SENSOR_PM_STREAMOFF error !!!\n", __func__, __LINE__);
			return ret;
		}
		printk("%s TX_SENSOR_PM_STREAMOFF.\n",sc431hai_attr.name);
	}else {
		printk("[%s]Don't Support this function.\n",__func__);
		return -EINVAL;
	}

	return ret;
}

static int sc431hai_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = sc431hai_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sc431hai_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sc431hai_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sc431hai_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sc431hai_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sc431hai_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = sc431hai_write_array(sd, sc431hai_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = sc431hai_write_array(sd, sc431hai_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sc431hai_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = sc431hai_set_hvflip(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_PM_S_STREAM_CTRL:
		if(arg)
			ret = sc431hai_pm_s_stream_ctrl(sd, arg);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = sc431hai_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = sc431hai_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = sc431hai_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = sc431hai_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = sc431hai_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops sc431hai_core_ops = {
	.g_chip_ident = sc431hai_g_chip_ident,
	.reset = sc431hai_reset,
	.init = sc431hai_init,
	.g_register = sc431hai_g_register,
	.s_register = sc431hai_s_register,
};

static struct tx_isp_subdev_video_ops sc431hai_video_ops = {
	.s_stream = sc431hai_s_stream,
};

static struct tx_isp_subdev_sensor_ops sc431hai_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = sc431hai_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops sc431hai_ops = {
	.core = &sc431hai_core_ops,
	.video = &sc431hai_video_ops,
	.sensor = &sc431hai_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "sc431hai",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc431hai_probe(struct i2c_client *client,
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
	sensor->video.attr = &sc431hai_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc431hai_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc431hai\n");

	return 0;
}

static int sc431hai_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc431hai_id[] = {
	{"sc431hai", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, sc431hai_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver sc431hai_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "sc431hai",
	},
	.probe          = sc431hai_probe,
	.remove         = sc431hai_remove,
	.id_table       = sc431hai_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_sc431hai(void) {
	return private_i2c_add_driver(&sc431hai_driver);
}

static __exit void exit_sc431hai(void) {
	private_i2c_del_driver(&sc431hai_driver);
}

module_init(init_sc431hai);
module_exit(exit_sc431hai);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_sc431hai(void) {
	return private_i2c_add_driver(&sc431hai_driver);
}

static void exit_first_sc431hai(void) {
	private_i2c_del_driver(&sc431hai_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
    .name = "sc431hai",
    .i2c_addr = 0x30,
    .width = 0,
    .height = 0,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_sc431hai,
	.exit_sensor = exit_first_sc431hai
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for sc431hai sensor");
MODULE_LICENSE("GPL");
