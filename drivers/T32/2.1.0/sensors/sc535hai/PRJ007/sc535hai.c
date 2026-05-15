/*
 * sc535hai.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps    dvdd
 *  0           1280*960        120        mipi_2lane    linear  10        120
 *  1           2560*1920        20        mipi_2lane    linear  10        20
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

#define TVERSION "V20241113a"
#define SENSOR_VERSION  "H20260116a"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0xce)
#define SENSOR_CHIP_ID_L    (0x78)
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

struct tx_isp_sensor_attribute sc535hai_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sc535hai_again_lut[] = {
	{0x20, 0},
	{0x21, 2886},
	{0x22, 5776},
	{0x23, 8494},
	{0x24, 11136},
	{0x25, 13706},
	{0x26, 16287},
	{0x27, 18723},
	{0x28, 21097},
	{0x29, 23413},
	{0x2a, 25746},
	{0x2b, 27952},
	{0x2c, 30108},
	{0x2d, 32216},
	{0x2e, 34344},
	{0x2f, 36361},
	{0x30, 38335},
	{0x31, 40269},
	{0x32, 42225},
	{0x33, 44082},
	{0x34, 45903},
	{0x35, 47689},
	{0x36, 49499},
	{0x37, 51220},
	{0x38, 52910},
	{0x39, 54570},
	{0x3a, 56253},
	{0x3b, 57856},
	{0x3c, 59433},
	{0x3d, 60983},
	{0x3e, 62557},
	{0x3f, 64058},
	{0x120, 65535},
	{0x121, 68467},
	{0x122, 71266},
	{0x123, 74029},
	{0x124, 76671},
	{0x125, 79281},
	{0x126, 81782},
	{0x127, 84258},
	{0x128, 86632},
	{0x8020, 87760},
	{0x8021, 90667},
	{0x8022, 93487},
	{0x8023, 96226},
	{0x8024, 98888},
	{0x8025, 101476},
	{0x8026, 103996},
	{0x8027, 106450},
	{0x8028, 108872},
	{0x8029, 111205},
	{0x802a, 113481},
	{0x802b, 115704},
	{0x802c, 117876},
	{0x802d, 119998},
	{0x802e, 122075},
	{0x802f, 124106},
	{0x8030, 126095},
	{0x8031, 128043},
	{0x8032, 129952},
	{0x8033, 131823},
	{0x8034, 133657},
	{0x8035, 135457},
	{0x8036, 137223},
	{0x8037, 138957},
	{0x8038, 140681},
	{0x8039, 142352},
	{0x803a, 143995},
	{0x803b, 145610},
	{0x803c, 147198},
	{0x803d, 148759},
	{0x803e, 150296},
	{0x803f, 151807},
	{0x8120, 153295},
	{0x8121, 156202},
	{0x8122, 159022},
	{0x8123, 161761},
	{0x8124, 164439},
	{0x8125, 167027},
	{0x8126, 169547},
	{0x8127, 172001},
	{0x8128, 174392},
	{0x8129, 176725},
	{0x812a, 179002},
	{0x812b, 181225},
	{0x812c, 183411},
	{0x812d, 185533},
	{0x812e, 187610},
	{0x812f, 189641},
	{0x8130, 191630},
	{0x8131, 193578},
	{0x8132, 195487},
	{0x8133, 197358},
	{0x8134, 199204},
	{0x8135, 201003},
	{0x8136, 202769},
	{0x8137, 204503},
	{0x8138, 206205},
	{0x8139, 207877},
	{0x813a, 209520},
	{0x813b, 211135},
	{0x813c, 212733},
	{0x813d, 214294},
	{0x813e, 215831},
	{0x813f, 217342},
	{0x8320, 218830},
	{0x8321, 221737},
	{0x8322, 224566},
	{0x8323, 227305},
	{0x8324, 229966},
	{0x8325, 232554},
	{0x8326, 235082},
	{0x8327, 237536},
	{0x8328, 239927},
	{0x8329, 242260},
	{0x832a, 244544},
	{0x832b, 246767},
	{0x832c, 248939},
	{0x832d, 251062},
	{0x832e, 253145},
	{0x832f, 255176},
	{0x8330, 257165},
	{0x8331, 259113},
	{0x8332, 261028},
	{0x8333, 262899},
	{0x8334, 264733},
	{0x8335, 266533},
	{0x8336, 268304},
	{0x8337, 270038},
	{0x8338, 271740},
	{0x8339, 273412},
	{0x833a, 275060},
	{0x833b, 276675},
	{0x833c, 278263},
	{0x833d, 279825},
	{0x833e, 281366},
	{0x833f, 282877},
	{0x8720, 284365},
	{0x8721, 287276},
	{0x8722, 290097},
	{0x8723, 292840},
	{0x8724, 295501},
	{0x8725, 298093},
	{0x8726, 300613},
	{0x8727, 303071},
	{0x8728, 305462},
	{0x8729, 307799},
	{0x872a, 310075},
	{0x872b, 312302},
	{0x872c, 314474},
	{0x872d, 316600},
	{0x872e, 318676},
	{0x872f, 320711},
	{0x8730, 322700},
	{0x8731, 324651},
	{0x8732, 326560},
	{0x8733, 328434},
	{0x8734, 330268},
	{0x8735, 332071},
	{0x8736, 333836},
	{0x8737, 335573},
	{0x8738, 337275},
	{0x8739, 338950},
	{0x873a, 340593},
	{0x873b, 342210},
	{0x873c, 343798},
	{0x873d, 345362},
	{0x873e, 346898},
	{0x873f, 348412},
	{0x8f20, 349900},
	{0x8f21, 352809},
	{0x8f22, 355632},
	{0x8f23, 358372},
	{0x8f24, 361036},
	{0x8f25, 363626},
	{0x8f26, 366148},
	{0x8f27, 368604},
	{0x8f28, 370997},
	{0x8f29, 373332},
	{0x8f2a, 375610},
	{0x8f2b, 377835},
	{0x8f2c, 380009},
	{0x8f2d, 382133},
	{0x8f2e, 384211},
	{0x8f2f, 386245},
	{0x8f30, 388235},
	{0x8f31, 390185},
	{0x8f32, 392095},
	{0x8f33, 393967},
	{0x8f34, 395803},
	{0x8f35, 397604},
	{0x8f36, 399371},
	{0x8f37, 401106},
	{0x8f38, 402810},
	{0x8f39, 404483},
	{0x8f3a, 406128},
	{0x8f3b, 407744},
	{0x8f3c, 409333},
	{0x8f3d, 410896},
	{0x8f3e, 412433},
	{0x8f3f, 413946},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int sc535hai_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc535hai_again_lut;
	while (lut->gain <= sc535hai_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sc535hai_attr.again.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

#else
	/* Non analog gain table */
#endif  /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

	return isp_gain;
}

#ifdef SENSOR_WDR_2_FRAME
unsigned int sc535hai_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc535hai_again_lut;
	while(lut->gain <= sc535hai_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc535hai_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int sc535hai_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int sc535hai_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int sc535hai_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus sc535hai_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 900,
	.lans = 2,
	.image_twidth = 1280,
	.image_theight = 960,
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

struct tx_isp_dvp_bus sc535hai_dvp = {
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

struct tx_isp_sensor_attribute sc535hai_attr = {
	.name = "sc535hai",
	.chip_id = 0xce1f,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x32,
	.sensor_ctrl.alloc_again = sc535hai_alloc_again,
	.sensor_ctrl.alloc_dgain = sc535hai_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = sc535hai_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list sc535hai_init_regs_1280_960_120fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x2386,0x8a},
	{0x23b0,0x00},
	{0x23b1,0x08},
	{0x23b2,0x00},
	{0x23b3,0x18},
	{0x23b4,0x00},
	{0x23b5,0x38},
	{0x23b6,0x04},
	{0x23b7,0x08},
	{0x23b8,0x04},
	{0x23b9,0x18},
	{0x23ba,0x04},
	{0x23bb,0x38},
	{0x23c0,0x04},
	{0x23c1,0x00},
	{0x23c2,0x04},
	{0x23c3,0x18},
	{0x23c4,0x04},
	{0x23c5,0x78},
	{0x23c6,0x04},
	{0x23c7,0x08},
	{0x23c8,0x04},
	{0x23c9,0x78},
	{0x3018,0x3b},
	{0x3019,0x0c},
	{0x301e,0xf0},
	{0x301f,0x21},
	{0x302c,0x00},
	{0x3032,0x20},
	{0x30b8,0x44},
	{0x3200,0x00},
	{0x3201,0x40},
	{0x3202,0x00},
	{0x3203,0x0c},
	{0x3204,0x0a},
	{0x3205,0x47},
	{0x3206,0x07},
	{0x3207,0x93},
	{0x3208,0x05},
	{0x3209,0x00},
	{0x320a,0x03},
	{0x320b,0xc0},
	{0x320c,0x05},//hts =  0x5dc = 1500
	{0x320d,0xdc},
	{0x320e,0x03},//vts = 0x3e8 = 1000
	{0x320f,0xe8},
	{0x3210,0x00},
	{0x3211,0x02},
	{0x3212,0x00},
	{0x3213,0x02},
	{0x3214,0x11},
	{0x3215,0x31},
	{0x3220,0x01},
	{0x3223,0xc0},
	{0x3250,0x40},
	{0x327f,0x3f},
	{0x32e0,0x00},
	{0x3301,0x1a},
	{0x3302,0x20},
	{0x3304,0xc0},
	{0x3306,0xe8},
	{0x3309,0xf0},
	{0x330a,0x01},
	{0x330b,0xa0},
	{0x330d,0x10},
	{0x3310,0x18},
	{0x331e,0xa9},
	{0x331f,0xd9},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x3364,0x56},
	{0x338f,0x80},
	{0x3393,0x24},
	{0x3394,0x2c},
	{0x3395,0x3c},
	{0x3399,0x14},
	{0x339a,0x20},
	{0x339b,0x2c},
	{0x339c,0x50},
	{0x33ac,0x10},
	{0x33ad,0x2c},
	{0x33ae,0xb0},
	{0x33af,0xe0},
	{0x33b0,0x0f},
	{0x33b1,0x80},
	{0x33b2,0x2c},
	{0x33b3,0x04},
	{0x349f,0x03},
	{0x34a8,0x06},
	{0x34a9,0x08},
	{0x34aa,0x01},
	{0x34ab,0xa0},
	{0x34ac,0x01},
	{0x34ad,0xa0},
	{0x34f9,0x0a},
	{0x3631,0x0f},
	{0x3632,0x8d},
	{0x3633,0x4d},
	{0x363b,0x58},
	{0x363c,0xd8},
	{0x363d,0x20},
	{0x3641,0x08},
	{0x3670,0x22},
	{0x3671,0x24},
	{0x3672,0x26},
	{0x3673,0x04},
	{0x3674,0x08},
	{0x3675,0x04},
	{0x3676,0x18},
	{0x367e,0x69},
	{0x367f,0x69},
	{0x3680,0x69},
	{0x3681,0x04},
	{0x3682,0x08},
	{0x3683,0x04},
	{0x3684,0x38},
	{0x3685,0xc1},
	{0x3686,0xc2},
	{0x3687,0xc1},
	{0x3688,0xc1},
	{0x3689,0xc1},
	{0x368a,0xc1},
	{0x368b,0xc4},
	{0x368c,0xc1},
	{0x368d,0x00},
	{0x368e,0x08},
	{0x368f,0x00},
	{0x3690,0x18},
	{0x3691,0x04},
	{0x3692,0x00},
	{0x3693,0x04},
	{0x3694,0x08},
	{0x3695,0x04},
	{0x3696,0x18},
	{0x3697,0x04},
	{0x3698,0x38},
	{0x3699,0x04},
	{0x369a,0x78},
	{0x36d0,0x0d},
	{0x36ea,0x0a},
	{0x36eb,0x04},
	{0x36ec,0x43},
	{0x36ed,0xaa},
	{0x370f,0x13},
	{0x3721,0x6c},
	{0x3722,0x8b},
	{0x3724,0xd1},
	{0x3729,0x34},
	{0x37b0,0x77},
	{0x37b1,0x17},
	{0x37b2,0x17},
	{0x37b3,0x04},
	{0x37b4,0x08},
	{0x37b5,0x04},
	{0x37b6,0x38},
	{0x37b7,0x1d},
	{0x37b8,0x1f},
	{0x37b9,0x1f},
	{0x37ba,0x04},
	{0x37bb,0x04},
	{0x37bc,0x04},
	{0x37bd,0x04},
	{0x37be,0x08},
	{0x37bf,0x04},
	{0x37c0,0x38},
	{0x37c1,0x04},
	{0x37c2,0x08},
	{0x37c3,0x04},
	{0x37c4,0x38},
	{0x37fa,0x09},
	{0x37fb,0x02},
	{0x37fc,0x20},
	{0x37fd,0x26},
	{0x3900,0x05},
	{0x3901,0x00},
	{0x3902,0xc0},
	{0x3903,0x40},
	{0x3905,0x2d},
	{0x391a,0x72},
	{0x391b,0x39},
	{0x391c,0x22},
	{0x391d,0x00},
	{0x391f,0x41},
	{0x3926,0xe0},
	{0x3933,0x80},
	{0x3934,0x03},
	{0x3935,0x01},
	{0x3936,0x00},
	{0x3937,0x68},
	{0x3938,0x6c},
	{0x3939,0x0f},
	{0x393a,0xf6},
	{0x393b,0x0f},
	{0x393c,0x70},
	{0x393d,0x05},
	{0x393e,0x80},
	{0x39dd,0x00},
	{0x39de,0x06},
	{0x39e7,0x04},//
	{0x39e8,0x04},//
	{0x39e9,0x80},//0x3e0 = 992
	{0x3e00,0x00},
	{0x3e01,0x3e},
	{0x3e02,0x00},
	{0x3e03,0x0b},
	{0x3e16,0x01},
	{0x3e17,0x44},
	{0x3e18,0x01},
	{0x3e19,0x44},
	{0x440e,0x02},
	{0x4509,0x18},
	{0x450d,0x07},
	{0x480f,0x03},
	{0x5000,0x46},
	{0x5780,0x76},
	{0x5784,0x10},
	{0x5785,0x08},
	{0x5787,0x16},
	{0x5788,0x16},
	{0x5789,0x15},
	{0x578a,0x16},
	{0x578b,0x16},
	{0x578c,0x15},
	{0x578d,0x41},
	{0x5790,0x11},
	{0x5791,0x0f},
	{0x5792,0x0f},
	{0x5793,0x11},
	{0x5794,0x0f},
	{0x5795,0x0f},
	{0x5799,0x46},
	{0x579a,0x77},
	{0x57a1,0x04},
	{0x57a8,0xd2},
	{0x57aa,0x2a},
	{0x57ab,0x7f},
	{0x57ac,0x00},
	{0x57ad,0x00},
	{0x5900,0x01},
	{0x5901,0x04},
	{0x36e9,0x53},
	{0x37f9,0x24},
	{0x0100,0x01},

	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sc535hai_init_regs_2560_1920_20fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x2386,0x8a},
	{0x23b0,0x00},
	{0x23b1,0x08},
	{0x23b2,0x00},
	{0x23b3,0x18},
	{0x23b4,0x00},
	{0x23b5,0x38},
	{0x23b6,0x04},
	{0x23b7,0x08},
	{0x23b8,0x04},
	{0x23b9,0x18},
	{0x23ba,0x04},
	{0x23bb,0x38},
	{0x23c0,0x04},
	{0x23c1,0x00},
	{0x23c2,0x04},
	{0x23c3,0x18},
	{0x23c4,0x04},
	{0x23c5,0x78},
	{0x23c6,0x04},
	{0x23c7,0x08},
	{0x23c8,0x04},
	{0x23c9,0x78},
	{0x3018,0x3b},
	{0x3019,0x0c},
	{0x301e,0xf0},
	{0x301f,0x21},
	{0x302c,0x00},
	{0x3032,0x20},
	{0x30b8,0x44},
	{0x3200,0x00},
	{0x3201,0x40},
	{0x3202,0x00},
	{0x3203,0x0c},
	{0x3204,0x0a},
	{0x3205,0x47},
	{0x3206,0x07},
	{0x3207,0x93},
	{0x3208,0x05},
	{0x3209,0x00},
	{0x320a,0x03},//
	{0x320b,0xc0},//hts =  0x5dc = 1500
	{0x320c,0x05},//
	{0x320d,0xdc},//vts = 0x3e8 = 1000
	{0x320e,0x03},
	{0x320f,0xe8},
	{0x3210,0x00},
	{0x3211,0x02},
	{0x3212,0x00},
	{0x3213,0x02},
	{0x3214,0x11},
	{0x3215,0x31},
	{0x3220,0x01},
	{0x3223,0xc0},
	{0x3250,0x40},
	{0x327f,0x3f},
	{0x32e0,0x00},
	{0x3301,0x1a},
	{0x3302,0x20},
	{0x3304,0xc0},
	{0x3306,0xe8},
	{0x3309,0xf0},
	{0x330a,0x01},
	{0x330b,0xa0},
	{0x330d,0x10},
	{0x3310,0x18},
	{0x331e,0xa9},
	{0x331f,0xd9},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x3364,0x56},
	{0x338f,0x80},
	{0x3393,0x24},
	{0x3394,0x2c},
	{0x3395,0x3c},
	{0x3399,0x14},
	{0x339a,0x20},
	{0x339b,0x2c},
	{0x339c,0x50},
	{0x33ac,0x10},
	{0x33ad,0x2c},
	{0x33ae,0xb0},
	{0x33af,0xe0},
	{0x33b0,0x0f},
	{0x33b1,0x80},
	{0x33b2,0x2c},
	{0x33b3,0x04},
	{0x349f,0x03},
	{0x34a8,0x06},
	{0x34a9,0x08},
	{0x34aa,0x01},
	{0x34ab,0xa0},
	{0x34ac,0x01},
	{0x34ad,0xa0},
	{0x34f9,0x0a},
	{0x3631,0x0f},
	{0x3632,0x8d},
	{0x3633,0x4d},
	{0x363b,0x58},
	{0x363c,0xd8},
	{0x363d,0x20},
	{0x3641,0x08},
	{0x3670,0x22},
	{0x3671,0x24},
	{0x3672,0x26},
	{0x3673,0x04},
	{0x3674,0x08},
	{0x3675,0x04},
	{0x3676,0x18},
	{0x367e,0x69},
	{0x367f,0x69},
	{0x3680,0x69},
	{0x3681,0x04},
	{0x3682,0x08},
	{0x3683,0x04},
	{0x3684,0x38},
	{0x3685,0xc1},
	{0x3686,0xc2},
	{0x3687,0xc1},
	{0x3688,0xc1},
	{0x3689,0xc1},
	{0x368a,0xc1},
	{0x368b,0xc4},
	{0x368c,0xc1},
	{0x368d,0x00},
	{0x368e,0x08},
	{0x368f,0x00},
	{0x3690,0x18},
	{0x3691,0x04},
	{0x3692,0x00},
	{0x3693,0x04},
	{0x3694,0x08},
	{0x3695,0x04},
	{0x3696,0x18},
	{0x3697,0x04},
	{0x3698,0x38},
	{0x3699,0x04},
	{0x369a,0x78},
	{0x36d0,0x0d},
	{0x36ea,0x0a},
	{0x36eb,0x04},
	{0x36ec,0x43},
	{0x36ed,0xaa},
	{0x370f,0x13},
	{0x3721,0x6c},
	{0x3722,0x8b},
	{0x3724,0xd1},
	{0x3729,0x34},
	{0x37b0,0x77},
	{0x37b1,0x17},
	{0x37b2,0x17},
	{0x37b3,0x04},
	{0x37b4,0x08},
	{0x37b5,0x04},
	{0x37b6,0x38},
	{0x37b7,0x1d},
	{0x37b8,0x1f},
	{0x37b9,0x1f},
	{0x37ba,0x04},
	{0x37bb,0x04},
	{0x37bc,0x04},
	{0x37bd,0x04},
	{0x37be,0x08},
	{0x37bf,0x04},
	{0x37c0,0x38},
	{0x37c1,0x04},
	{0x37c2,0x08},
	{0x37c3,0x04},
	{0x37c4,0x38},
	{0x37fa,0x09},
	{0x37fb,0x02},
	{0x37fc,0x20},
	{0x37fd,0x26},
	{0x3900,0x05},
	{0x3901,0x00},
	{0x3902,0xc0},
	{0x3903,0x40},
	{0x3905,0x2d},
	{0x391a,0x72},
	{0x391b,0x39},
	{0x391c,0x22},
	{0x391d,0x00},
	{0x391f,0x41},
	{0x3926,0xe0},
	{0x3933,0x80},
	{0x3934,0x03},
	{0x3935,0x01},
	{0x3936,0x00},
	{0x3937,0x68},
	{0x3938,0x6c},
	{0x3939,0x0f},
	{0x393a,0xf6},
	{0x393b,0x0f},
	{0x393c,0x70},
	{0x393d,0x05},
	{0x393e,0x80},
	{0x39dd,0x00},
	{0x39de,0x06},
	{0x39e7,0x04},//
	{0x39e8,0x04},//
	{0x39e9,0x80},//0x3e0 = 992
	{0x3e00,0x00},
	{0x3e01,0x3e},
	{0x3e02,0x00},
	{0x3e03,0x0b},
	{0x3e16,0x01},
	{0x3e17,0x44},
	{0x3e18,0x01},
	{0x3e19,0x44},
	{0x440e,0x02},
	{0x4509,0x18},
	{0x450d,0x07},
	{0x480f,0x03},
	{0x5000,0x46},
	{0x5780,0x76},
	{0x5784,0x10},
	{0x5785,0x08},
	{0x5787,0x16},
	{0x5788,0x16},
	{0x5789,0x15},
	{0x578a,0x16},
	{0x578b,0x16},
	{0x578c,0x15},
	{0x578d,0x41},
	{0x5790,0x11},
	{0x5791,0x0f},
	{0x5792,0x0f},
	{0x5793,0x11},
	{0x5794,0x0f},
	{0x5795,0x0f},
	{0x5799,0x46},
	{0x579a,0x77},
	{0x57a1,0x04},
	{0x57a8,0xd2},
	{0x57aa,0x2a},
	{0x57ab,0x7f},
	{0x57ac,0x00},
	{0x57ad,0x00},
	{0x5900,0x01},
	{0x5901,0x04},
	{0x36e9,0x53},
	{0x37f9,0x24},
	{0x0100,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x3208,0x0a},
	{0x320a,0x07},
	{0x320b,0x80},
	{0x320c,0x0b},//
	{0x320d,0xb8},//hts =  0xbb8 = 3000
	{0x320e,0x0b},//
	{0x320f,0xb8},//vts = 3000
	{0x3211,0x04},
	{0x3213,0x04},
	{0x3215,0x11},
	{0x3220,0x00},
	{0x5000,0x06},
	{0x5901,0x00},
	{0x36e9,0x53},
	{0x37f9,0x24},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the sc535hai_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc535hai_win_sizes[] = {
	{
		.width          = 1280,
		.height         = 960,
		.fps            = 120 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc535hai_init_regs_1280_960_120fps_mipi,
	},
	{
		.width          = 2560,
		.height         = 1920,
		.fps            = 20 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc535hai_init_regs_2560_1920_20fps_mipi,
	}
};

static struct tx_isp_sensor_win_setting *wsize = &sc535hai_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sc535hai_stream_on_mipi[] = {
	{0x0100, 0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list sc535hai_stream_off_mipi[] = {
	{0x0100, 0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int sc535hai_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int sc535hai_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int sc535hai_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc535hai_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc535hai_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc535hai_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int sc535hai_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int sc535hai_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc535hai_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc535hai_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int sc535hai_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc535hai_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int sc535hai_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = ISP_SUCCESS;

#ifndef ZRT_SENSOR_WITHOUT_INIT
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

#endif

error:
	return ret;
}

static int sc535hai_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int sc535hai_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &sc535hai_win_sizes[0];
		sc535hai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc535hai_attr.again.value = 0;
		sc535hai_attr.again.max = 413946;
		sc535hai_attr.again.min = 0;
		sc535hai_attr.again.apply_delay = 2;

		sc535hai_attr.integration_time.value = 992;
		sc535hai_attr.integration_time.max = 1000 - 8;
		sc535hai_attr.integration_time.min = 2;
		sc535hai_attr.integration_time.apply_delay = 2;

		sc535hai_attr.total_width = 1500;
		sc535hai_attr.total_height = 1000;

		sc535hai_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		sc535hai_attr.hcg.base_gain = ;
		sc535hai_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc535hai_attr.again_short.value = ;
		sc535hai_attr.again_short.max = ;
		sc535hai_attr.again_short.min = ;
		sc535hai_attr.again_short.apply_delay = ;

		sc535hai_attr.integration_time_short.value = ;
		sc535hai_attr.integration_time_short.max = ;
		sc535hai_attr.integration_time_short.min = ;
		sc535hai_attr.integration_time_short.apply_delay = ;

		sc535hai_attr.wdr_cache = wdr_line * sc535hai_attr.total_width;

#ifdef SENSOR_HCG
		sc535hai_attr.hcg_short.base_gain = ;
		sc535hai_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(sc535hai_attr.mipi)), (void *)(&sc535hai_mipi_linear), sizeof(sc535hai_attr.mipi));
		break;
	case 1:
		wsize = &sc535hai_win_sizes[1];
		sc535hai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc535hai_attr.again.value = 0;
		sc535hai_attr.again.max = 413946;
		sc535hai_attr.again.min = 0;
		sc535hai_attr.again.apply_delay = 2;

		sc535hai_attr.integration_time.value = 992;
		sc535hai_attr.integration_time.max = 3000 - 8;
		sc535hai_attr.integration_time.min = 2;
		sc535hai_attr.integration_time.apply_delay = 2;

		sc535hai_attr.total_width = 3000;
		sc535hai_attr.total_height = 3000;

		sc535hai_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		sc535hai_attr.hcg.base_gain = ;
		sc535hai_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc535hai_attr.again_short.value = ;
		sc535hai_attr.again_short.max = ;
		sc535hai_attr.again_short.min = ;
		sc535hai_attr.again_short.apply_delay = ;

		sc535hai_attr.integration_time_short.value = ;
		sc535hai_attr.integration_time_short.max = ;
		sc535hai_attr.integration_time_short.min = ;
		sc535hai_attr.integration_time_short.apply_delay = ;

		sc535hai_attr.wdr_cache = wdr_line * sc535hai_attr.total_width;

#ifdef SENSOR_HCG
		sc535hai_attr.hcg_short.base_gain = ;
		sc535hai_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(sc535hai_attr.mipi)), (void *)(&sc535hai_mipi_linear), sizeof(sc535hai_attr.mipi));
		sc535hai_attr.mipi.image_twidth = 2560;
		sc535hai_attr.mipi.image_theight = 1920;
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int sc535hai_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	sc535hai_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		sc535hai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc535hai_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		sc535hai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc535hai_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		sc535hai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = sc535hai_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
	sc535hai_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int sc535hai_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sc535hai_read(sd, 0x3107, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc535hai_read(sd, 0x3108, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc535hai_g_chip_ident(struct tx_isp_subdev *sd,
								 struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sc535hai_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "sc535hai_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "sc535hai_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */
	ret = sc535hai_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc535hai chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}


	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", sc535hai_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "sc535hai", sizeof("sc535hai"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc535hai_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sc535hai_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = sc535hai_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int sc535hai_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc535hai_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc535hai_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc535hai_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int sc535hai_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = sc535hai_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sc535hai_write_array(sd, sc535hai_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("sc535hai stream on\n");
		}

	} else {
		ret = sc535hai_write_array(sd, sc535hai_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("sc535hai stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int sc535hai_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret += sc535hai_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc535hai_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc535hai_write(sd, 0x3e02, (unsigned char)((it & 0xf) << 4));

	ret += sc535hai_write(sd, 0x3e08, (unsigned char)((again >> 8) & 0xff));
	ret += sc535hai_write(sd, 0x3e09, (unsigned char)(again & 0xff));

	return ret;
}
#else
static int sc535hai_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	ret += sc535hai_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc535hai_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc535hai_write(sd, 0x3e02, (unsigned char)((value & 0xf) << 4));

	return ret;
}

static int sc535hai_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	ret += sc535hai_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
	ret += sc535hai_write(sd, 0x3e09, (unsigned char)(value & 0xff));

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc535hai_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc535hai_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc535hai_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = sc535hai_attr_set(sd, wsize);
	}

	return ret;
}

static int sc535hai_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 1500 * 1000 * 120;  /**< HTS * VTS * FPS */
		max_fps = wsize->fps;
		break;
	case 1:
		sclk = 3000 * 3000 * 20;  /**< HTS * VTS * FPS */
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
	ret += sc535hai_read(sd, 0x320c, &val);
	hts = val << 8;
	val = 0;
	ret += sc535hai_read(sd, 0x320d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: sc535hai read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	sc535hai_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	sc535hai_write(sd, 0x320e, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: sc535hai_write err\n");
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

	printk("\n==>[%s %d] fps 0x%x hts 0x%x vts 0x%x\n", __func__, __LINE__, fps, hts, vts);
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	if (ret) {
		ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
	}

	return ret;
}

static int sc535hai_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	ret = sc535hai_read(sd, 0x3221, &val);
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
	ret += sc535hai_write(sd, 0x3221, val);

	sensor->video.hvflip_mode = par->hvflip;
	sc535hai_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int sc535hai_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int sc535hai_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int sc535hai_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc535hai_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = sc535hai_write_array(sd, sc535hai_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		sc535hai_setting_select(sd, 1);
		sc535hai_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		sc535hai_setting_select(sd, 0);
		sc535hai_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int sc535hai_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = sc535hai_write_array(sd, wsize->regs);
	ret = sc535hai_write_array(sd, sc535hai_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int sc535hai_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = sc535hai_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sc535hai_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sc535hai_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sc535hai_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sc535hai_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sc535hai_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = sc535hai_write_array(sd, sc535hai_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = sc535hai_write_array(sd, sc535hai_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sc535hai_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = sc535hai_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = sc535hai_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = sc535hai_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = sc535hai_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = sc535hai_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = sc535hai_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops sc535hai_core_ops = {
	.g_chip_ident = sc535hai_g_chip_ident,
	.reset = sc535hai_reset,
	.init = sc535hai_init,
	.g_register = sc535hai_g_register,
	.s_register = sc535hai_s_register,
};

static struct tx_isp_subdev_video_ops sc535hai_video_ops = {
	.s_stream = sc535hai_s_stream,
};

static struct tx_isp_subdev_sensor_ops sc535hai_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = sc535hai_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops sc535hai_ops = {
	.core = &sc535hai_core_ops,
	.video = &sc535hai_video_ops,
	.sensor = &sc535hai_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
	.name = "sc535hai",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc535hai_probe(struct i2c_client *client,
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
	sensor->video.attr = &sc535hai_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc535hai_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc535hai\n");

	return 0;
}

static int sc535hai_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc535hai_id[] = {
	{"sc535hai", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, sc535hai_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver sc535hai_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "sc535hai",
	},
	.probe          = sc535hai_probe,
	.remove         = sc535hai_remove,
	.id_table       = sc535hai_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_sc535hai(void) {
	return private_i2c_add_driver(&sc535hai_driver);
}

static __exit void exit_sc535hai(void) {
	private_i2c_del_driver(&sc535hai_driver);
}

module_init(init_sc535hai);
module_exit(exit_sc535hai);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_sc535hai(void) {
	return private_i2c_add_driver(&sc535hai_driver);
}

static void exit_first_sc535hai(void) {
	private_i2c_del_driver(&sc535hai_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "sc535hai",
	.i2c_addr = 0x30,
	.width = 2880,
	.height = 1620,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_sc535hai,
	.exit_sensor = exit_first_sc535hai
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for sc535hai sensor");
MODULE_LICENSE("GPL");
