/*
 * sc231hais3.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps		dvdd
 *  0           1920*1080       30        mipi_2lane    linear  10        30        external
 *
 * @I2C addr:0x30, 0x32
 *
 * @FSync:Direct connection between the main EFSYNC and the slave EFSYNC
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
#define SENSOR_VERSION  "H20260130a"

//#define SENSOR_TEST
#define SENSOR_SLAVE

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
/* #define SENSOR_HCG */
#define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0xcb)
#define SENSOR_CHIP_ID_L    (0x6a)
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

struct tx_isp_sensor_attribute sc231hais3_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sc231hais3_again_lut[] = {
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
	{0x0032, 42226},
	{0x0033, 44083},
	{0x0034, 45904},
	{0x0035, 47691},
	{0x0036, 49500},
	{0x0037, 51221},
	{0x0038, 52911},
	{0x0039, 54571},
	{0x003a, 56255},
	{0x003b, 57858},
	{0x003c, 59434},
	{0x003d, 60984},
	{0x003e, 62559},
	{0x003f, 64059},
	{0x0120, 65536},
	{0x0121, 68468},
	{0x0122, 71268},
	{0x0123, 74030},
	{0x0124, 76672},
	{0x0125, 79283},
	{0x0126, 81784},
	{0x0127, 84260},
	{0x0128, 86634},
	{0x0129, 88987},
	{0x012a, 91247},
	{0x012b, 93489},
	{0x012c, 95645},
	{0x012d, 97787},
	{0x012e, 99848},
	{0x012f, 101898},
	{0x0130, 103872},
	{0x0131, 105837},
	{0x0132, 107732},
	{0x0133, 109619},
	{0x0134, 111440},
	{0x0135, 113255},
	{0x0136, 115008},
	{0x0137, 116757},
	{0x0138, 118447},
	{0x0139, 120134},
	{0x013a, 121765},
	{0x013b, 123394},
	{0x8020, 123701},
	{0x8021, 126620},
	{0x8022, 129427},
	{0x8023, 132176},
	{0x8024, 134848},
	{0x8025, 137425},
	{0x8026, 139954},
	{0x8027, 142397},
	{0x8028, 144799},
	{0x8029, 147141},
	{0x802a, 149407},
	{0x802b, 151639},
	{0x802c, 153819},
	{0x802d, 155933},
	{0x802e, 158017},
	{0x802f, 160040},
	{0x8030, 162037},
	{0x8031, 163993},
	{0x8032, 165893},
	{0x8033, 167771},
	{0x8034, 169613},
	{0x8035, 171404},
	{0x8036, 173177},
	{0x8037, 174902},
	{0x8038, 176612},
	{0x8039, 178291},
	{0x803a, 179926},
	{0x803b, 181547},
	{0x803c, 183142},
	{0x803d, 184696},
	{0x803e, 186238},
	{0x803f, 187743},
	{0x8120, 189237},
	{0x8121, 192143},
	{0x8122, 194975},
	{0x8123, 197712},
	{0x8124, 200373},
	{0x8125, 202961},
	{0x8126, 205490},
	{0x8127, 207944},
	{0x8128, 210335},
	{0x8129, 212667},
	{0x812a, 214953},
	{0x812b, 217175},
	{0x812c, 219346},
	{0x812d, 221469},
	{0x812e, 223553},
	{0x812f, 225585},
	{0x8130, 227573},
	{0x8131, 229520},
	{0x8132, 231437},
	{0x8133, 233307},
	{0x8134, 235141},
	{0x8135, 236940},
	{0x8136, 238713},
	{0x8137, 240446},
	{0x8138, 242148},
	{0x8139, 243819},
	{0x813a, 245469},
	{0x813b, 247083},
	{0x813c, 248671},
	{0x813d, 250232},
	{0x813e, 251774},
	{0x813f, 253286},
	{0x8320, 254773},
	{0x8321, 257685},
	{0x8322, 260505},
	{0x8323, 263248},
	{0x8324, 265909},
	{0x8325, 268502},
	{0x8326, 271021},
	{0x8327, 273480},
	{0x8328, 275871},
	{0x8329, 278208},
	{0x832a, 280484},
	{0x832b, 282711},
	{0x832c, 284882},
	{0x832d, 287009},
	{0x832e, 289085},
	{0x832f, 291121},
	{0x8330, 293109},
	{0x8331, 295061},
	{0x8332, 296969},
	{0x8333, 298843},
	{0x8334, 300677},
	{0x8335, 302480},
	{0x8336, 304245},
	{0x8337, 305982},
	{0x8338, 307684},
	{0x8339, 309359},
	{0x833a, 311002},
	{0x833b, 312620},
	{0x833c, 314207},
	{0x833d, 315771},
	{0x833e, 317307},
	{0x833f, 318822},
	{0x8720, 320309},
	{0x8721, 323218},
	{0x8722, 326041},
	{0x8723, 328782},
	{0x8724, 331445},
	{0x8725, 334036},
	{0x8726, 336557},
	{0x8727, 339013},
	{0x8728, 341407},
	{0x8729, 343741},
	{0x872a, 346020},
	{0x872b, 348245},
	{0x872c, 350418},
	{0x872d, 352543},
	{0x872e, 354621},
	{0x872f, 356654},
	{0x8730, 358645},
	{0x8731, 360594},
	{0x8732, 362505},
	{0x8733, 364377},
	{0x8734, 366213},
	{0x8735, 368014},
	{0x8736, 369781},
	{0x8737, 371516},
	{0x8738, 373220},
	{0x8739, 374893},
	{0x873a, 376538},
	{0x873b, 378154},
	{0x873c, 379743},
	{0x873d, 381306},
	{0x873e, 382843},
	{0x873f, 384356},
	{0x8f20, 385845},
	{0x8f21, 388754},
	{0x8f22, 391577},
	{0x8f23, 394318},
	{0x8f24, 396981},
	{0x8f25, 399572},
	{0x8f26, 402093},
	{0x8f27, 404549},
	{0x8f28, 406943},
	{0x8f29, 409277},
	{0x8f2a, 411556},
	{0x8f2b, 413781},
	{0x8f2c, 415954},
	{0x8f2d, 418079},
	{0x8f2e, 420157},
	{0x8f2f, 422190},
	{0x8f30, 424181},
	{0x8f31, 426130},
	{0x8f32, 428041},
	{0x8f33, 429913},
	{0x8f34, 431749},
	{0x8f35, 433550},
	{0x8f36, 435317},
	{0x8f37, 437052},
	{0x8f38, 438756},
	{0x8f39, 440429},
	{0x8f3a, 442074},
	{0x8f3b, 443690},
	{0x8f3c, 445279},
	{0x8f3d, 446842},
	{0x8f3e, 448379},
	{0x8f3f, 449892},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int sc231hais3_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc231hais3_again_lut;
	while (lut->gain <= sc231hais3_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sc231hais3_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int sc231hais3_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc231hais3_again_lut;
	while(lut->gain <= sc231hais3_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc231hais3_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int sc231hais3_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int sc231hais3_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int sc231hais3_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus sc231hais3_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 396,
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

struct tx_isp_dvp_bus sc231hais3_dvp = {
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

struct tx_isp_sensor_attribute sc231hais3_attr = {
	.name = "sc231hais3",
	.chip_id = 0xcb6a,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.sensor_ctrl.alloc_again = sc231hais3_alloc_again,
	.sensor_ctrl.alloc_dgain = sc231hais3_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = sc231hais3_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list sc231hais3_init_regs_1920_1080_30fps_mipi[] = {
	//Cleaned_0x14_SC231AI_MIPI_24Minput_2lane_396Mbps_10bit_1920x1080_30fps_外供
	{0x0103,0x01},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x14},
	{0x3021,0x07},
	{0x3058,0x21},
	{0x3059,0x53},
	{0x305a,0x40},
	{0x320e,0x04},//vts = 0x4b0 = 1200
	{0x320f,0xb0},//
	{0x3210,0x00},
	{0x3211,0x04},
	{0x3212,0x00},
	{0x3213,0x04},
	{0x3250,0x00},
	{0x3301,0x0a},
	{0x3302,0x20},
	{0x3304,0x90},
	{0x3305,0x00},
	{0x3306,0x68},
	{0x3309,0xd0},
	{0x330b,0xd8},
	{0x330d,0x08},
	{0x331c,0x04},
	{0x331e,0x81},
	{0x331f,0xc1},
	{0x3323,0x06},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x3364,0x5e},
	{0x336c,0x8e},
	{0x337f,0x13},
	{0x338f,0x80},
	{0x3390,0x08},
	{0x3391,0x18},
	{0x3392,0xb8},
	{0x3393,0x0e},
	{0x3394,0x14},
	{0x3395,0x10},
	{0x3396,0x88},
	{0x3397,0x98},
	{0x3398,0xf8},
	{0x3399,0x0a},
	{0x339a,0x0e},
	{0x339b,0x10},
	{0x339c,0x3c},
	{0x33ae,0x80},
	{0x33af,0xc0},
	{0x33b2,0x50},
	{0x33b3,0x14},
	{0x33f8,0x00},
	{0x33f9,0x68},
	{0x33fa,0x00},
	{0x33fb,0x68},
	{0x33fc,0x48},
	{0x33fd,0x78},
	{0x349f,0x03},
	{0x34a6,0x40},
	{0x34a7,0x58},
	{0x34a8,0x10},
	{0x34a9,0x10},
	{0x34f8,0x78},
	{0x34f9,0x10},
	{0x3619,0x20},
	{0x361a,0x90},
	{0x3633,0x44},
	{0x3637,0x5c},
	{0x363c,0xc0},
	{0x363d,0x02},
	{0x3660,0x80},
	{0x3661,0x81},
	{0x3662,0x8f},
	{0x3663,0x81},
	{0x3664,0x81},
	{0x3665,0x82},
	{0x3666,0x8f},
	{0x3667,0x08},
	{0x3668,0x80},
	{0x3669,0x88},
	{0x366a,0x98},
	{0x366b,0xb8},
	{0x366c,0xf8},
	{0x3670,0xb2},
	{0x3671,0xa2},
	{0x3672,0x88},
	{0x3680,0x33},
	{0x3681,0x33},
	{0x3682,0x43},
	{0x36c0,0x80},
	{0x36c1,0x88},
	{0x36c8,0x88},
	{0x36c9,0xb8},
	{0x36ea,0x0b},
	{0x36eb,0x0c},
	{0x36ec,0x5c},
	{0x36ed,0x04},
	{0x3718,0x04},
	{0x3722,0x8b},
	{0x3724,0xd1},
	{0x3741,0x08},
	{0x3770,0x17},
	{0x3771,0x9b},
	{0x3772,0x9b},
	{0x37c0,0x88},
	{0x37c1,0xb8},
	{0x37fa,0x0b},
	{0x37fc,0x10},
	{0x37fd,0x04},
	{0x3902,0xc0},
	{0x3903,0x40},
	{0x3909,0x00},
	{0x391f,0x41},
	{0x3926,0xe0},
	{0x3933,0x80},
	{0x3934,0x02},
	{0x3937,0x6f},
	{0x3e00,0x00},
	{0x3e01,0x95},
	{0x3e02,0x50},
	{0x3e08,0x00},
	{0x4509,0x20},
	{0x450d,0x07},
	{0x4837,0x33},
	{0x5780,0x76},
	{0x5784,0x10},
	{0x5787,0x0a},
	{0x5788,0x0a},
	{0x5789,0x08},
	{0x578a,0x0a},
	{0x578b,0x0a},
	{0x578c,0x08},
	{0x578d,0x40},
	{0x5792,0x04},
	{0x5795,0x04},
	{0x57ac,0x00},
	{0x57ad,0x00},
	{0x33b1,0x80},
	{0x36e9,0x27},
	{0x37f9,0x27},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the sc231hais3_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc231hais3_win_sizes[] = {
	{
		.width          = 1920,
		.height         = 1080,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc231hais3_init_regs_1920_1080_30fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &sc231hais3_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sc231hais3_stream_on_mipi[] = {
	{0x0100, 0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list sc231hais3_stream_off_mipi[] = {
	{0x0100, 0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int sc231hais3_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int sc231hais3_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int sc231hais3_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc231hais3_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc231hais3_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc231hais3_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int sc231hais3_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int sc231hais3_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc231hais3_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc231hais3_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int sc231hais3_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc231hais3_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int sc231hais3_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned long rate = 0;
	struct clk *pll = NULL;
	char *plls[] = {"mpll", "sclka"};
	int psize = sizeof(plls) / sizeof(char *);
	char *ppll = plls[psize - 1];
	int ret = 0, i = 0;

	if (mclk == 24000000) {
		ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
		goto set_clk;
	}

	pll = clk_get_parent(sclka);
	rate = clk_get_rate(pll);
	if (rate % mclk) {
		for (i = 0; i < psize; i++) {
			pll = clk_get(NULL, plls[i]);
			rate = clk_get_rate(pll);
			if (!(rate % mclk)) {
				ret = clk_set_parent(sclka, pll);
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
				if(mclk == 37125000){
					if (rate >= 891000000) {
						rate = 891000000;
					} else {
						ISP_ERROR("[%s %d] The %s clock setting failed !!!\n",
							  __func__, __LINE__, ppll);
						ret = -1;
						goto error;
					}
				} else if (mclk == 27000000) {
					rate -= rate % mclk;
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
				ret = clk_set_parent(sclka, pll);
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
set_clk:
	private_clk_set_rate(sensor->mclk, mclk);
	private_clk_prepare_enable(sensor->mclk);

	rate = clk_get_rate(sensor->mclk);
	if (rate % mclk) {
		ret = -1;
		goto error;
	}

	return ret;

error:
	ISP_ERROR("[%s %d] Unable to allocate the required MCLK %ld !!!\n",
		  __func__, __LINE__, mclk);
	return ret;
}

static int sc231hais3_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int sc231hais3_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &sc231hais3_win_sizes[0];
		sc231hais3_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc231hais3_attr.again.value = 65536;
		sc231hais3_attr.again.max = 449892;
		sc231hais3_attr.again.min = 0;
		sc231hais3_attr.again.apply_delay = 2;

		sc231hais3_attr.integration_time.value = 1194;
		sc231hais3_attr.integration_time.max = 1200 - 6;
		sc231hais3_attr.integration_time.min = 2;
		sc231hais3_attr.integration_time.apply_delay = 2;

		sc231hais3_attr.total_width = 2200;
		sc231hais3_attr.total_height = 1200;

		sc231hais3_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
		sc231hais3_attr.hcg.base_gain = ;
		sc231hais3_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc231hais3_attr.again_short.value = ;
		sc231hais3_attr.again_short.max = ;
		sc231hais3_attr.again_short.min = ;
		sc231hais3_attr.again_short.apply_delay = ;

		sc231hais3_attr.integration_time_short.value = ;
		sc231hais3_attr.integration_time_short.max = ;
		sc231hais3_attr.integration_time_short.min = ;
		sc231hais3_attr.integration_time_short.apply_delay = ;

		sc231hais3_attr.wdr_cache = wdr_line * sc231hais3_attr.total_width;

#ifdef SENSOR_HCG
		sc231hais3_attr.hcg_short.base_gain = ;
		sc231hais3_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(sc231hais3_attr.mipi)), (void *)(&sc231hais3_mipi_linear), sizeof(sc231hais3_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int sc231hais3_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	sc231hais3_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		sc231hais3_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc231hais3_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		sc231hais3_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc231hais3_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		sc231hais3_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = sc231hais3_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	sensor->video.attr->fsync_attr.call_times = 1;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.apply_delay = 1;
	sc231hais3_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int sc231hais3_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sc231hais3_read(sd, 0x3107, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc231hais3_read(sd, 0x3108, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc231hais3_g_chip_ident(struct tx_isp_subdev *sd,
				 struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sc231hais3_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "sc231hais3_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "sc231hais3_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = sc231hais3_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc231hais3 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", sc231hais3_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
		    info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "sc231hais3", sizeof("sc231hais3"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc231hais3_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sc231hais3_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = sc231hais3_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int sc231hais3_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc231hais3_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc231hais3_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc231hais3_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int sc231hais3_fsync(struct tx_isp_subdev *sd, struct tx_isp_frame_sync *sync)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

#ifndef SENSOR_SLAVE
	/* master */
	uint8_t val;
	uint16_t ret_val;

	sc231hais3_read(sd, 0x320e, &val);
	ret_val = val << 8;
	sc231hais3_read(sd, 0x320f, &val);
	ret_val |= val;
	ret_val = ret_val * 2;
	sc231hais3_write(sd, 0x320e, ret_val >> 8);
	sc231hais3_write(sd, 0x320f, (ret_val & 0xff) + 4);
	sc231hais3_write(sd, 0x300a, 0x40);
	sc231hais3_write(sd, 0x3032, 0xa0);
	sc231hais3_write(sd, 0x329a, 0x04);
	sc231hais3_write(sd, 0x329b, 0xb0);
#else
	/* slave */
	uint8_t val;
	uint16_t ret_val;

	sc231hais3_read(sd, 0x320e, &val);
	ret_val = val << 8;
	sc231hais3_read(sd, 0x320f, &val);
	ret_val |= val;
	ret_val = ret_val * 2;
	sc231hais3_write(sd, 0x320e, ret_val >> 8);
	sc231hais3_write(sd, 0x320f, ret_val & 0xff);
	sc231hais3_write(sd, 0x3222, 0x01);
	sc231hais3_write(sd, 0x322e, 0x00);
	sc231hais3_write(sd, 0x322f, 0x00);
	sc231hais3_write(sd, 0x3230, 0x00);
	sc231hais3_write(sd, 0x3231, 0x04);
	sc231hais3_write(sd, 0x3224, 0x83);
	sc231hais3_write(sd, 0x300a, 0x24);
#endif /* SENSOR_SLAVE */
	sensor->video.fps.value /= 2;

	return 0;
}

static int sc231hais3_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = sc231hais3_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sc231hais3_write_array(sd, sc231hais3_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("sc231hais3 stream on\n");
		}

	} else {
		ret = sc231hais3_write_array(sd, sc231hais3_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("sc231hais3 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int sc231hais3_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	it = it << 1;
	ret += sc231hais3_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc231hais3_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc231hais3_write(sd, 0x3e02, (unsigned char)((it & 0xf) << 4));

	ret += sc231hais3_write(sd, 0x3e08, (unsigned char)((again >> 8) & 0xff));
	ret += sc231hais3_write(sd, 0x3e09, (unsigned char)(again & 0xff));

	return ret;
}
#else
static int sc231hais3_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	value = value << 1;
	ret += sc231hais3_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc231hais3_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc231hais3_write(sd, 0x3e02, (unsigned char)((value & 0xf) << 4));

	return ret;
}

static int sc231hais3_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	ret += sc231hais3_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
	ret += sc231hais3_write(sd, 0x3e09, (unsigned char)(value & 0xff));

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc231hais3_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc231hais3_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc231hais3_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = sc231hais3_attr_set(sd, wsize);
	}

	return ret;
}

static int sc231hais3_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 2200 * 1200 * 30;  /**< HTS * VTS * FPS */
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
	ret += sc231hais3_read(sd, 0x320c, &val);
	hts = val << 8;
	val = 0;
	ret += sc231hais3_read(sd, 0x320d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: sc231hais3 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

#ifndef SENSOR_SLAVE
	vts += 4;
#endif /* SENSOR_SLAVE */

	sc231hais3_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	sc231hais3_write(sd, 0x320e, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: sc231hais3_write err\n");
		return ret;
	}

	/* sensor->video.fps = fps; */
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

static int sc231hais3_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	ret = sc231hais3_read(sd, 0x3221, &val);
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
	ret += sc231hais3_write(sd, 0x3221, val);

	sensor->video.hvflip_mode = par->hvflip;
	sc231hais3_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int sc231hais3_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int sc231hais3_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int sc231hais3_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc231hais3_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = sc231hais3_write_array(sd, sc231hais3_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		sc231hais3_setting_select(sd, 1);
		sc231hais3_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		sc231hais3_setting_select(sd, 0);
		sc231hais3_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int sc231hais3_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = sc231hais3_write_array(sd, wsize->regs);
	ret = sc231hais3_write_array(sd, sc231hais3_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int sc231hais3_pm_s_stream_ctrl(struct tx_isp_subdev *sd, void *arg)
{
	int ret = ISP_SUCCESS;
	IMPISPUserCtl *data = (IMPISPUserCtl *)arg;
	if(data->cmd == TX_SENSOR_PM_RESUME){
		printk("%s TX_SENSOR_PM_RESUME\n",sc231hais3_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_SUSPEND) {
		ret = sc231hais3_write_array(sd, sc231hais3_stream_off_mipi);
		if (ret < 0) {
			printk("\n TX_SENSOR_PM_SUSPEND error !!!\n");
			return ret;
		}
		printk("%s TX_SENSOR_PM_SUSPEND.\n",sc231hais3_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_PREPARE) {
		printk("%s TX_SENSOR_PM_PREPARE.\n",sc231hais3_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_COMPLETE) {
		printk("%s TX_SENSOR_PM_COMPLETE.\n",sc231hais3_attr.name);

	}else if(data->cmd == TX_SENSOR_PM_STREAMON) {
		ret = sc231hais3_write_array(sd, sc231hais3_stream_on_mipi);
		if (ret < 0) {
			printk("\n TX_SENSOR_PM_STREAMON error !!!\n");
			return ret;
		}
		printk("%s TX_SENSOR_PM_STREAMON.\n",sc231hais3_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_STREAMOFF) {
		ret = sc231hais3_write_array(sd, sc231hais3_stream_off_mipi);
		if (ret < 0) {
			printk("\n [%s %d] TX_SENSOR_PM_STREAMOFF error !!!\n", __func__, __LINE__);
			return ret;
		}
		printk("%s TX_SENSOR_PM_STREAMOFF.\n",sc231hais3_attr.name);
	}else {
		printk("[%s]Don't Support this function.\n",__func__);
		return -EINVAL;
	}

	return ret;
}

static int sc231hais3_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = sc231hais3_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sc231hais3_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sc231hais3_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sc231hais3_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sc231hais3_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sc231hais3_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = sc231hais3_write_array(sd, sc231hais3_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = sc231hais3_write_array(sd, sc231hais3_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sc231hais3_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = sc231hais3_set_hvflip(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_PM_S_STREAM_CTRL:
		if(arg)
			ret = sc231hais3_pm_s_stream_ctrl(sd, arg);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = sc231hais3_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = sc231hais3_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = sc231hais3_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = sc231hais3_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = sc231hais3_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops sc231hais3_core_ops = {
	.g_chip_ident = sc231hais3_g_chip_ident,
	.reset = sc231hais3_reset,
	.init = sc231hais3_init,
	.g_register = sc231hais3_g_register,
	.s_register = sc231hais3_s_register,
	.fsync = sc231hais3_fsync,
};

static struct tx_isp_subdev_video_ops sc231hais3_video_ops = {
	.s_stream = sc231hais3_s_stream,
};

static struct tx_isp_subdev_sensor_ops sc231hais3_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = sc231hais3_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops sc231hais3_ops = {
	.core = &sc231hais3_core_ops,
	.video = &sc231hais3_video_ops,
	.sensor = &sc231hais3_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "sc231hais3",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc231hais3_probe(struct i2c_client *client,
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
	sensor->video.attr = &sc231hais3_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc231hais3_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc231hais3\n");

	return 0;
}

static int sc231hais3_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc231hais3_id[] = {
	{"sc231hais3", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, sc231hais3_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver sc231hais3_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "sc231hais3",
	},
	.probe          = sc231hais3_probe,
	.remove         = sc231hais3_remove,
	.id_table       = sc231hais3_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_sc231hais3(void) {
	return private_i2c_add_driver(&sc231hais3_driver);
}

static __exit void exit_sc231hais3(void) {
	private_i2c_del_driver(&sc231hais3_driver);
}

module_init(init_sc231hais3);
module_exit(exit_sc231hais3);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_sc231hais3(void) {
	return private_i2c_add_driver(&sc231hais3_driver);
}

static void exit_first_sc231hais3(void) {
	private_i2c_del_driver(&sc231hais3_driver);
}

struct tx_isp_sensor_fast_attr sensor3 = {
	.name = "sc231hais3",
	.i2c_addr = 0x32,
	.width = 1920,
	.height = 1080,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_sc231hais3,
	.exit_sensor = exit_first_sc231hais3
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for Sony sc231hais3 sensor");
MODULE_LICENSE("GPL");
