/*
 * sc200ais1.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps		dvdd
 *  0           1920*1080       30        mipi_1lane    linear  10        30
 *  1           1920*1080       60        mipi_2lane    linear  10        60
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

#define TVERSION "V20251101a"
#define SENSOR_VERSION  "H20260130a"

/* #define SENSOR_TEST */

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
/* #define SENSOR_HCG */
#define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0xcb)
#define SENSOR_CHIP_ID_L    (0x1c)
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

struct tx_isp_sensor_attribute sc200ais1_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sc200ais1_again_lut[] = {
	{0x340, 0},
	{0x341, 1500},
	{0x342, 2886},
	{0x343, 4342},
	{0x344, 5776},
	{0x345, 7101},
	{0x346, 8494},
	{0x347, 9781},
	{0x348, 11136},
	{0x349, 12471},
	{0x34a, 13706},
	{0x34b, 15005},
	{0x34c, 16287},
	{0x34d, 17474},
	{0x34e, 18723},
	{0x34f, 19879},
	{0x350, 21097},
	{0x351, 22300},
	{0x352, 23413},
	{0x353, 24587},
	{0x354, 25746},
	{0x355, 26820},
	{0x356, 27952},
	{0x357, 29002},
	{0x358, 30108},
	{0x359, 31202},
	{0x35a, 32216},
	{0x35b, 33286},
	{0x35c, 34344},
	{0x35d, 35325},
	{0x35e, 36361},
	{0x35f, 37321},
	{0x360, 38335},
	{0x361, 39338},
	{0x362, 40269},
	{0x363, 41252},
	{0x364, 42225},
	{0x365, 43128},
	{0x366, 44082},
	{0x367, 44967},
	{0x368, 45903},
	{0x369, 46829},
	{0x36a, 47689},
	{0x36b, 48599},
	{0x36c, 49499},
	{0x36d, 50336},
	{0x36e, 51220},
	{0x36f, 52041},
	{0x370, 52910},
	{0x371, 53770},
	{0x372, 54570},
	{0x373, 55415},
	{0x374, 56253},
	{0x375, 57032},
	{0x376, 57856},
	{0x377, 58622},
	{0x378, 59433},
	{0x379, 60236},
	{0x37a, 60983},
	{0x37b, 61773},
	{0x37c, 62557},
	{0x37d, 63286},
	{0x37e, 64058},
	{0x37f, 64775},
	{0x740, 65535},
	{0x741, 66989},
	{0x742, 68467},
	{0x743, 69877},
	{0x744, 71266},
	{0x745, 72636},
	{0x746, 74029},
	{0x747, 75359},
	{0x748, 76671},
	{0x749, 77964},
	{0x74a, 79281},
	{0x74b, 80540},
	{0x74c, 81782},
	{0x74d, 83009},
	{0x74e, 84258},
	{0x74f, 85452},
	{0x750, 86632},
	{0x751, 87797},
	{0x752, 88985},
	{0x753, 90122},
	{0x754, 91245},
	{0x755, 92355},
	{0x756, 93487},
	{0x757, 94571},
	{0x758, 95643},
	{0x759, 96703},
	{0x75a, 97785},
	{0x75b, 98821},
	{0x75c, 99846},
	{0x75d, 100860},
	{0x75e, 101896},
	{0x75f, 102888},
	{0x760, 103870},
	{0x761, 104842},
	{0x762, 105835},
	{0x763, 106787},
	{0x764, 107730},
	{0x765, 108663},
	{0x766, 109617},
	{0x767, 110532},
	{0x768, 111438},
	{0x769, 112335},
	{0x76a, 113253},
	{0x76b, 114134},
	{0x76c, 115006},
	{0x2340, 115704},
	{0x2341, 117166},
	{0x2342, 118606},
	{0x2343, 120025},
	{0x2344, 121449},
	{0x2345, 122826},
	{0x2346, 124183},
	{0x2347, 125521},
	{0x2348, 126840},
	{0x2349, 128141},
	{0x234a, 129424},
	{0x234b, 130691},
	{0x234c, 131963},
	{0x234d, 133196},
	{0x234e, 134413},
	{0x234f, 135615},
	{0x2350, 136801},
	{0x2351, 137973},
	{0x2352, 139131},
	{0x2353, 140274},
	{0x2354, 141425},
	{0x2355, 142541},
	{0x2356, 143644},
	{0x2357, 144735},
	{0x2358, 145813},
	{0x2359, 146879},
	{0x235a, 147932},
	{0x235b, 148975},
	{0x235c, 150025},
	{0x235d, 151045},
	{0x235e, 152054},
	{0x235f, 153052},
	{0x2360, 154039},
	{0x2361, 155017},
	{0x2362, 155984},
	{0x2363, 156942},
	{0x2364, 157908},
	{0x2365, 158846},
	{0x2366, 159776},
	{0x2367, 160696},
	{0x2368, 161607},
	{0x2369, 162510},
	{0x236a, 163404},
	{0x236b, 164290},
	{0x236c, 165184},
	{0x236d, 166053},
	{0x236e, 166914},
	{0x236f, 167768},
	{0x2370, 168614},
	{0x2371, 169452},
	{0x2372, 170283},
	{0x2373, 171107},
	{0x2374, 171939},
	{0x2375, 172749},
	{0x2376, 173552},
	{0x2377, 174348},
	{0x2378, 175137},
	{0x2379, 175920},
	{0x237a, 176696},
	{0x237b, 177466},
	{0x237c, 178244},
	{0x237d, 179002},
	{0x237e, 179753},
	{0x237f, 180499},
	{0x2740, 181239},
	{0x2741, 182701},
	{0x2742, 184155},
	{0x2743, 185573},
	{0x2744, 186971},
	{0x2745, 188348},
	{0x2746, 189718},
	{0x2747, 191056},
	{0x2748, 192375},
	{0x2749, 193676},
	{0x274a, 194971},
	{0x274b, 196237},
	{0x274c, 197487},
	{0x274d, 198720},
	{0x274e, 199948},
	{0x274f, 201150},
	{0x2750, 202336},
	{0x2751, 203508},
	{0x2752, 204676},
	{0x2753, 205820},
	{0x2754, 206949},
	{0x2755, 208066},
	{0x2756, 209179},
	{0x2757, 210270},
	{0x2758, 211348},
	{0x2759, 212414},
	{0x275a, 213477},
	{0x275b, 214520},
	{0x275c, 215550},
	{0x275d, 216570},
	{0x275e, 217589},
	{0x275f, 218549},
	{0x2760, 219574},
	{0x2761, 220497},
	{0x2762, 221501},
	{0x2763, 222405},
	{0x2764, 223389},
	{0x2765, 224364},
	{0x2766, 225241},
	{0x2767, 226196},
	{0x2768, 227142},
	{0x2769, 227994},
	{0x276a, 228922},
	{0x276b, 229758},
	{0x276c, 230669},
	{0x276d, 231572},
	{0x276e, 232385},
	{0x276f, 233271},
	{0x2770, 234149},
	{0x2771, 234940},
	{0x2772, 235803},
	{0x2773, 236580},
	{0x2774, 237428},
	{0x2775, 238269},
	{0x2776, 239026},
	{0x2777, 239853},
	{0x2778, 240672},
	{0x2779, 241411},
	{0x277a, 242216},
	{0x277b, 242943},
	{0x277c, 243736},
	{0x277d, 244523},
	{0x277e, 245232},
	{0x277f, 246006},
	{0x2f40, 246774},
	{0x2f41, 248223},
	{0x2f42, 249649},
	{0x2f43, 251055},
	{0x2f44, 252506},
	{0x2f45, 253870},
	{0x2f46, 255215},
	{0x2f47, 256540},
	{0x2f48, 257910},
	{0x2f49, 259199},
	{0x2f4a, 260470},
	{0x2f4b, 261725},
	{0x2f4c, 263022},
	{0x2f4d, 264243},
	{0x2f4e, 265449},
	{0x2f4f, 266640},
	{0x2f50, 267871},
	{0x2f51, 269032},
	{0x2f52, 270179},
	{0x2f53, 271312},
	{0x2f54, 272484},
	{0x2f55, 273590},
	{0x2f56, 274683},
	{0x2f57, 275764},
	{0x2f58, 276883},
	{0x2f59, 277939},
	{0x2f5a, 278983},
	{0x2f5b, 280015},
	{0x2f5c, 281085},
	{0x2f5d, 282096},
	{0x2f5e, 283095},
	{0x2f5f, 284084},
	{0x2f60, 285109},
	{0x2f61, 286078},
	{0x2f62, 287036},
	{0x2f63, 287985},
	{0x2f64, 288969},
	{0x2f65, 289899},
	{0x2f66, 290819},
	{0x2f67, 291731},
	{0x2f68, 292677},
	{0x2f69, 293571},
	{0x2f6a, 294457},
	{0x2f6b, 295335},
	{0x2f6c, 296245},
	{0x2f6d, 297107},
	{0x2f6e, 297960},
	{0x2f6f, 298806},
	{0x2f70, 299684},
	{0x2f71, 300514},
	{0x2f72, 301338},
	{0x2f73, 302154},
	{0x2f74, 303002},
	{0x2f75, 303804},
	{0x2f76, 304599},
	{0x2f77, 305388},
	{0x2f78, 306207},
	{0x2f79, 306982},
	{0x2f7a, 307751},
	{0x2f7b, 308514},
	{0x2f7c, 309307},
	{0x2f7d, 310058},
	{0x2f7e, 310802},
	{0x2f7f, 311541},
	{0x3f40, 312309},
	{0x3f41, 313758},
	{0x3f42, 315218},
	{0x3f43, 316623},
	{0x3f44, 318041},
	{0x3f45, 319405},
	{0x3f46, 320781},
	{0x3f47, 322107},
	{0x3f48, 323445},
	{0x3f49, 324734},
	{0x3f4a, 326035},
	{0x3f4b, 327290},
	{0x3f4c, 328557},
	{0x3f4d, 329778},
	{0x3f4e, 331013},
	{0x3f4f, 332203},
	{0x3f50, 333406},
	{0x3f51, 334567},
	{0x3f52, 335741},
	{0x3f53, 336874},
	{0x3f54, 338019},
	{0x3f55, 339125},
	{0x3f56, 340244},
	{0x3f57, 341324},
	{0x3f58, 342418},
	{0x3f59, 343474},
	{0x3f5a, 344542},
	{0x3f5b, 345575},
	{0x3f5c, 346620},
	{0x3f5d, 347631},
	{0x3f5e, 348654},
	{0x3f5f, 349643},
	{0x3f60, 350644},
	{0x3f61, 351613},
	{0x3f62, 352594},
	{0x3f63, 353542},
	{0x3f64, 354504},
	{0x3f65, 355445},
	{0x3f66, 356376},
	{0x3f67, 357299},
	{0x3f68, 358212},
	{0x3f69, 359117},
	{0x3f6a, 360013},
	{0x3f6b, 360901},
	{0x3f6c, 361780},
	{0x3f6d, 362652},
	{0x3f6e, 363515},
	{0x3f6f, 364371},
	{0x3f70, 365219},
	{0x3f71, 366059},
	{0x3f72, 366892},
	{0x3f73, 367718},
	{0x3f74, 368537},
	{0x3f75, 369348},
	{0x3f76, 370153},
	{0x3f77, 370951},
	{0x3f78, 371742},
	{0x3f79, 372527},
	{0x3f7a, 373305},
	{0x3f7b, 374077},
	{0x3f7c, 374842},
	{0x3f7d, 375601},
	{0x3f7e, 376355},
	{0x3f7f, 377102},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int sc200ais1_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc200ais1_again_lut;
	while (lut->gain <= sc200ais1_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sc200ais1_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int sc200ais1_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc200ais1_again_lut;
	while(lut->gain <= sc200ais1_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc200ais1_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int sc200ais1_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int sc200ais1_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int sc200ais1_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus sc200ais1_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 300,
	.lans = 1,
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

struct tx_isp_dvp_bus sc200ais1_dvp = {
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

struct tx_isp_sensor_attribute sc200ais1_attr = {
	.name = "sc200ais1",
	.chip_id = 0xcd1c,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.sensor_ctrl.alloc_again = sc200ais1_alloc_again,
	.sensor_ctrl.alloc_dgain = sc200ais1_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = sc200ais1_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list sc200ais1_init_regs_1920_1080_30fps_mipi[] = {
	//cleaned_0x6b_sc200ais1_MIPI_24Minput_783Mbps_1lane_10bit_1920x1080_30fps_non-continue_test
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x36f9,0x80},
	{0x3018,0x12},
	{0x3019,0x0e},
	{0x301f,0x6b},
	{0x3200,0x00},
	{0x3201,0x00},
	{0x3202,0x00},
	{0x3203,0x00},
	{0x3204,0x07},
	{0x3205,0x87},
	{0x3206,0x04},
	{0x3207,0x3f},
	{0x3208,0x07},
	{0x3209,0x80},
	{0x320a,0x04},
	{0x320b,0x38},
	{0x320c,0x04},//hts = 0x488 = 1160
	{0x320d,0x88},//
	{0x320e,0x04},//10fps@vts =0xd2f=3375 30fps@vts=0x465=1125
	{0x320f,0x65},//
	{0x3210,0x00},
	{0x3211,0x04},
	{0x3212,0x00},
	{0x3213,0x04},
	{0x3243,0x01},
	{0x3248,0x02},
	{0x3249,0x09},
	{0x3253,0x08},
	{0x3271,0x0a},
	{0x3301,0x10},
	{0x3304,0x40},
	{0x3306,0x38},
	{0x330b,0x80},
	{0x330f,0x02},
	{0x331e,0x39},
	{0x3333,0x10},
	{0x3621,0xe8},
	{0x3622,0x16},
	{0x3637,0x1b},
	{0x363a,0x1f},
	{0x363b,0xc6},
	{0x363c,0x0e},
	{0x3670,0x0a},
	{0x3674,0x82},
	{0x3675,0x76},
	{0x3676,0x78},
	{0x367c,0x48},
	{0x367d,0x58},
	{0x3690,0x34},
	{0x3691,0x33},
	{0x3692,0x44},
	{0x369c,0x40},
	{0x369d,0x48},
	{0x36ea,0x63},
	{0x36eb,0x0d},
	{0x36ec,0x0c},
	{0x36ed,0x04},
	{0x36fa,0x63},
	{0x36fb,0x00},
	{0x36fc,0x10},
	{0x36fd,0x07},
	{0x3901,0x02},
	{0x3904,0x04},
	{0x3908,0x41},
	{0x391d,0x14},
	{0x391f,0x18},
	{0x3e01,0x8c},
	{0x3e02,0x20},
	{0x3e16,0x00},
	{0x3e17,0x80},
	{0x3f09,0x48},
	{0x4800,0x64},
	{0x5787,0x10},
	{0x5788,0x06},
	{0x578a,0x10},
	{0x578b,0x06},
	{0x5790,0x10},
	{0x5791,0x10},
	{0x5792,0x00},
	{0x5793,0x10},
	{0x5794,0x10},
	{0x5795,0x00},
	{0x5799,0x00},
	{0x57c7,0x10},
	{0x57c8,0x06},
	{0x57ca,0x10},
	{0x57cb,0x06},
	{0x57d1,0x10},
	{0x57d4,0x10},
	{0x57d9,0x00},
	{0x59e0,0x60},
	{0x59e1,0x08},
	{0x59e2,0x3f},
	{0x59e3,0x18},
	{0x59e4,0x18},
	{0x59e5,0x3f},
	{0x59e6,0x06},
	{0x59e7,0x02},
	{0x59e8,0x38},
	{0x59e9,0x10},
	{0x59ea,0x0c},
	{0x59eb,0x10},
	{0x59ec,0x04},
	{0x59ed,0x02},
	{0x59ee,0xa0},
	{0x59ef,0x08},
	{0x59f4,0x18},
	{0x59f5,0x10},
	{0x59f6,0x0c},
	{0x59f7,0x10},
	{0x59f8,0x06},
	{0x59f9,0x02},
	{0x59fa,0x18},
	{0x59fb,0x10},
	{0x59fc,0x0c},
	{0x59fd,0x10},
	{0x59fe,0x04},
	{0x59ff,0x02},
	{0x36e9,0x53},
	{0x36f9,0x53},
	{0x3021,0x87},
	/*{0x0100,0x01},*/
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sc200ais1_init_regs_1920_1080_60fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x36f9,0x80},
	{0x301f,0x68},
	{0x3200,0x00},
	{0x3201,0x00},
	{0x3202,0x00},
	{0x3203,0x00},
	{0x3204,0x07},
	{0x3205,0x87},
	{0x3206,0x04},
	{0x3207,0x3f},
	{0x3208,0x07},
	{0x3209,0x80},
	{0x320a,0x04},
	{0x320b,0x38},
	{0x320c,0x04},//{0x320c,0x320d}default = 0x44c = 1100     hts = 2*{0x320c,0x320d},
	{0x320d,0x4c},//
	{0x320e,0x04},//vts -> 0x4b0 = 1200
	{0x320f,0xb0},//
	{0x3210,0x00},
	{0x3211,0x04},
	{0x3212,0x00},
	{0x3213,0x04},
	{0x3243,0x01},
	{0x3248,0x02},
	{0x3249,0x09},
	{0x3253,0x08},
	{0x3271,0x0a},
	{0x3301,0x06},
	{0x3302,0x0c},
	{0x3303,0x08},
	{0x3304,0x60},
	{0x3306,0x28},
	{0x3308,0x10},
	{0x3309,0x70},
	{0x330b,0x80},
	{0x330d,0x16},
	{0x330e,0x1c},
	{0x330f,0x02},
	{0x3310,0x02},
	{0x331c,0x04},
	{0x331e,0x51},
	{0x331f,0x61},
	{0x3320,0x07},
	{0x3333,0x10},
	{0x334c,0x08},
	{0x3356,0x09},
	{0x3364,0x17},
	{0x3390,0x08},
	{0x3391,0x18},
	{0x3392,0x38},
	{0x3393,0x06},
	{0x3394,0x06},
	{0x3395,0x06},
	{0x3396,0x08},
	{0x3397,0x18},
	{0x3398,0x38},
	{0x3399,0x06},
	{0x339a,0x0a},
	{0x339b,0x10},
	{0x339c,0x20},
	{0x33ac,0x08},
	{0x33ae,0x10},
	{0x33af,0x19},
	{0x3621,0xe8},
	{0x3622,0x16},
	{0x3630,0xa0},
	{0x3637,0x36},
	{0x363a,0x1f},
	{0x363b,0xc6},
	{0x363c,0x0e},
	{0x3670,0x0a},
	{0x3674,0x82},
	{0x3675,0x76},
	{0x3676,0x78},
	{0x367c,0x48},
	{0x367d,0x58},
	{0x3690,0x34},
	{0x3691,0x33},
	{0x3692,0x44},
	{0x369c,0x40},
	{0x369d,0x48},
	{0x36eb,0x0c},
	{0x36ec,0x0c},
	{0x36ed,0x34},
	{0x36fa,0x35},
	{0x36fb,0x00},
	{0x36fc,0x00},
	{0x36fd,0x34},
	{0x3901,0x02},
	{0x3904,0x04},
	{0x3908,0x41},
	{0x391f,0x10},
	{0x3e01,0x95},
	{0x3e02,0x80},
	{0x3e16,0x00},
	{0x3e17,0x80},
	{0x3f09,0x48},
	{0x4819,0x0a},
	{0x481b,0x06},
	{0x481d,0x15},
	{0x481f,0x04},
	{0x4821,0x0a},
	{0x4823,0x05},
	{0x4825,0x04},
	{0x4827,0x05},
	{0x4829,0x08},
	{0x5000,0x00},
	{0x5787,0x10},
	{0x5788,0x06},
	{0x578a,0x10},
	{0x578b,0x06},
	{0x5790,0x10},
	{0x5791,0x10},
	{0x5792,0x00},
	{0x5793,0x10},
	{0x5794,0x10},
	{0x5795,0x00},
	{0x5799,0x00},
	{0x57c7,0x10},
	{0x57c8,0x06},
	{0x57ca,0x10},
	{0x57cb,0x06},
	{0x57d1,0x10},
	{0x57d4,0x10},
	{0x57d9,0x00},
	{0x59e0,0x60},
	{0x59e1,0x08},
	{0x59e2,0x3f},
	{0x59e3,0x18},
	{0x59e4,0x18},
	{0x59e5,0x3f},
	{0x59e6,0x06},
	{0x59e7,0x02},
	{0x59e8,0x38},
	{0x59e9,0x10},
	{0x59ea,0x0c},
	{0x59eb,0x10},
	{0x59ec,0x04},
	{0x59ed,0x02},
	{0x59ee,0xa0},
	{0x59ef,0x08},
	{0x59f4,0x18},
	{0x59f5,0x10},
	{0x59f6,0x0c},
	{0x59f7,0x10},
	{0x59f8,0x06},
	{0x59f9,0x02},
	{0x59fa,0x18},
	{0x59fb,0x10},
	{0x59fc,0x0c},
	{0x59fd,0x10},
	{0x59fe,0x04},
	{0x59ff,0x02},
	{0x36e9,0x20},
	{0x36f9,0x20},
	{0x3021,0x87},
	/*{0x0100,0x01},*/
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the sc200ais1_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc200ais1_win_sizes[] = {
	{
		.width          = 1920,
		.height         = 1080,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc200ais1_init_regs_1920_1080_30fps_mipi,
	},
	{
		.width          = 1920,
		.height         = 1080,
		.fps            = 60 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc200ais1_init_regs_1920_1080_60fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &sc200ais1_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sc200ais1_stream_on_mipi[] = {
	{0x0100, 0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list sc200ais1_stream_off_mipi[] = {
	{0x0100, 0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int sc200ais1_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int sc200ais1_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int sc200ais1_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc200ais1_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc200ais1_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc200ais1_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int sc200ais1_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int sc200ais1_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc200ais1_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc200ais1_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int sc200ais1_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc200ais1_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int sc200ais1_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int sc200ais1_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int sc200ais1_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &sc200ais1_win_sizes[0];
		sc200ais1_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc200ais1_attr.again.value = 65536;
		sc200ais1_attr.again.max = 377102;
		sc200ais1_attr.again.min = 0;
		sc200ais1_attr.again.apply_delay = 2;

		sc200ais1_attr.integration_time.value = 0xb60;
		sc200ais1_attr.integration_time.max = 1125 - 4;
		sc200ais1_attr.integration_time.min = 1;
		sc200ais1_attr.integration_time.apply_delay = 2;

		sc200ais1_attr.total_width = 1160;
		sc200ais1_attr.total_height = 1125;

		sc200ais1_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
		sc200ais1_attr.hcg.base_gain = ;
		sc200ais1_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc200ais1_attr.again_short.value = ;
		sc200ais1_attr.again_short.max = ;
		sc200ais1_attr.again_short.min = ;
		sc200ais1_attr.again_short.apply_delay = ;

		sc200ais1_attr.integration_time_short.value = ;
		sc200ais1_attr.integration_time_short.max = ;
		sc200ais1_attr.integration_time_short.min = ;
		sc200ais1_attr.integration_time_short.apply_delay = ;

		sc200ais1_attr.wdr_cache = wdr_line * sc200ais1_attr.total_width;

#ifdef SENSOR_HCG
		sc200ais1_attr.hcg_short.base_gain = ;
		sc200ais1_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(sc200ais1_attr.mipi)), (void *)(&sc200ais1_mipi_linear), sizeof(sc200ais1_attr.mipi));
		break;
	case 1:
		wsize = &sc200ais1_win_sizes[1];
		sc200ais1_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc200ais1_attr.again.value = 65536;
		sc200ais1_attr.again.max = 377102;
		sc200ais1_attr.again.min = 0;
		sc200ais1_attr.again.apply_delay = 2;

		sc200ais1_attr.integration_time.value = 0xb60;
		sc200ais1_attr.integration_time.max = 1200 - 4;
		sc200ais1_attr.integration_time.min = 1;
		sc200ais1_attr.integration_time.apply_delay = 2;

		sc200ais1_attr.total_width = 1100;
		sc200ais1_attr.total_height = 1200;

		sc200ais1_attr.expo_fs = TX_SENSOR_EXPO_FRAME_START;

#ifdef SENSOR_HCG
		sc200ais1_attr.hcg.base_gain = ;
		sc200ais1_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc200ais1_attr.again_short.value = ;
		sc200ais1_attr.again_short.max = ;
		sc200ais1_attr.again_short.min = ;
		sc200ais1_attr.again_short.apply_delay = ;

		sc200ais1_attr.integration_time_short.value = ;
		sc200ais1_attr.integration_time_short.max = ;
		sc200ais1_attr.integration_time_short.min = ;
		sc200ais1_attr.integration_time_short.apply_delay = ;

		sc200ais1_attr.wdr_cache = wdr_line * sc200ais1_attr.total_width;

#ifdef SENSOR_HCG
		sc200ais1_attr.hcg_short.base_gain = ;
		sc200ais1_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(sc200ais1_attr.mipi)), (void *)(&sc200ais1_mipi_linear), sizeof(sc200ais1_attr.mipi));
		sc200ais1_attr.mipi.clk = 792;
		sc200ais1_attr.mipi.lans = 2;
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int sc200ais1_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	sc200ais1_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		sc200ais1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc200ais1_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		sc200ais1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc200ais1_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		sc200ais1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = sc200ais1_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.apply_delay = 1;
	sc200ais1_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int sc200ais1_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sc200ais1_read(sd, 0x3107, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc200ais1_read(sd, 0x3108, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc200ais1_g_chip_ident(struct tx_isp_subdev *sd,
				  struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sc200ais1_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "sc200ais1_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "sc200ais1_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = sc200ais1_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc200ais1 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", sc200ais1_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
		    info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "sc200ais1", sizeof("sc200ais1"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc200ais1_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sc200ais1_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = sc200ais1_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int sc200ais1_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc200ais1_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc200ais1_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc200ais1_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int sc200ais1_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = sc200ais1_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sc200ais1_write_array(sd, sc200ais1_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("sc200ais1 stream on\n");
		}

	} else {
		ret = sc200ais1_write_array(sd, sc200ais1_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("sc200ais1 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int sc200ais1_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	it = (it << 1) - 1;
	ret += sc200ais1_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc200ais1_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc200ais1_write(sd, 0x3e02, (unsigned char)((it & 0xf) << 4));

	ret += sc200ais1_write(sd, 0x3e08, (unsigned char)((again >> 8) & 0xff));
	ret += sc200ais1_write(sd, 0x3e09, (unsigned char)(again & 0xff));

	return ret;
}
#else
static int sc200ais1_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	value = (value << 1) - 1;
	ret += sc200ais1_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc200ais1_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc200ais1_write(sd, 0x3e02, (unsigned char)((value & 0xf) << 4));

	return ret;
}

static int sc200ais1_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	ret += sc200ais1_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
	ret += sc200ais1_write(sd, 0x3e09, (unsigned char)(value & 0xff));

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc200ais1_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc200ais1_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc200ais1_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = sc200ais1_attr_set(sd, wsize);
	}

	return ret;
}

static int sc200ais1_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 1160 * 1125 * 30;  /**< HTS * VTS * FPS */
		max_fps = 30;
		break;
	case 1:
		sclk = 1100 * 1200 * 60;  /**< HTS * VTS * FPS */
		max_fps = 60;
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
	ret += sc200ais1_read(sd, 0x320c, &val);
	hts = val << 8;
	val = 0;
	ret += sc200ais1_read(sd, 0x320d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: sc200ais1 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	printk("hts=%d,vts=%d,fps=%d\n", hts, vts, ((fps >> 16) & 0xffff)/(fps & 0xffff));
	sc200ais1_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	sc200ais1_write(sd, 0x320e, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: sc200ais1_write err\n");
		return ret;
	}

	/* sensor->video.fps = fps; */
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
		sensor->video.attr->integration_time.max = vts - 4;
		break;
	case 1:
		sensor->video.attr->total_height = vts;
		sensor->video.attr->integration_time.max = vts - 4;
		sensor->video.attr->integration_time_short.max = vts - 4;
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

static int sc200ais1_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	ret = sc200ais1_read(sd, 0x3221, &val);
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
	ret += sc200ais1_write(sd, 0x3221, val);

	sensor->video.hvflip_mode = par->hvflip;
	sc200ais1_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

static int sc200ais1_pm_s_stream_ctrl(struct tx_isp_subdev *sd, void *arg)
{
	int ret = ISP_SUCCESS;
	IMPISPUserCtl *data = (IMPISPUserCtl *)arg;

	if(data->cmd == TX_SENSOR_PM_RESUME){
		printk("sc200ais1 TX_SENSOR_PM_RESUME\n");
	}else if(data->cmd == TX_SENSOR_PM_SUSPEND) {
		ret = sc200ais1_write_array(sd, sc200ais1_stream_off_mipi);
		if (ret < 0) {
			printk("\n TX_SENSOR_PM_SUSPEND error !!!\n");
			return ret;
		}
		printk("sc200ais1 TX_SENSOR_PM_SUSPEND !!!\n");
	}else if(data->cmd == TX_SENSOR_PM_PREPARE) {
		printk("\n sc200ais1 TX_SENSOR_PM_PREPARE !!!\n");
	}else if(data->cmd == TX_SENSOR_PM_COMPLETE) {
		printk("\n sc200ais1 TX_SENSOR_PM_COMPLETE !!!\n");
	}else if(data->cmd == TX_SENSOR_PM_STREAMON) {
		ret = sc200ais1_write_array(sd, sc200ais1_stream_on_mipi);
		if (ret < 0) {
			printk("\n TX_SENSOR_PM_STREAMON error !!!\n");
			return ret;
		}
		printk("\n sc200ais1 TX_SENSOR_PM_STREAMON !!!\n");
	}else if(data->cmd == TX_SENSOR_PM_STREAMOFF) {
		ret = sc200ais1_write_array(sd, sc200ais1_stream_off_mipi);
		if (ret < 0) {
			printk("\n [%s %d] TX_SENSOR_PM_STREAMOFF error !!!\n", __func__, __LINE__);
			return ret;
		}
		printk("\n sc200ais1 TX_SENSOR_PM_STREAMOFF !!!\n");
	}else {
		printk("\n==> Don't Support this function !!! \n");
		return -EINVAL;
	}

	return ret;
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int sc200ais1_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int sc200ais1_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int sc200ais1_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc200ais1_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = sc200ais1_write_array(sd, sc200ais1_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		sc200ais1_setting_select(sd, 1);
		sc200ais1_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		sc200ais1_setting_select(sd, 0);
		sc200ais1_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int sc200ais1_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = sc200ais1_write_array(sd, wsize->regs);
	ret = sc200ais1_write_array(sd, sc200ais1_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int sc200ais1_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = sc200ais1_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sc200ais1_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sc200ais1_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sc200ais1_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sc200ais1_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sc200ais1_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = sc200ais1_write_array(sd, sc200ais1_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = sc200ais1_write_array(sd, sc200ais1_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sc200ais1_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = sc200ais1_set_hvflip(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_PM_S_STREAM_CTRL:
		if(arg)
			ret =  sc200ais1_pm_s_stream_ctrl(sd, arg);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = sc200ais1_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = sc200ais1_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = sc200ais1_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = sc200ais1_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = sc200ais1_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops sc200ais1_core_ops = {
	.g_chip_ident = sc200ais1_g_chip_ident,
	.reset = sc200ais1_reset,
	.init = sc200ais1_init,
	.g_register = sc200ais1_g_register,
	.s_register = sc200ais1_s_register,
};

static struct tx_isp_subdev_video_ops sc200ais1_video_ops = {
	.s_stream = sc200ais1_s_stream,
};

static struct tx_isp_subdev_sensor_ops sc200ais1_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = sc200ais1_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops sc200ais1_ops = {
	.core = &sc200ais1_core_ops,
	.video = &sc200ais1_video_ops,
	.sensor = &sc200ais1_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "sc200ais1",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc200ais1_probe(struct i2c_client *client,
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
	sensor->video.attr = &sc200ais1_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc200ais1_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc200ais1\n");

	return 0;
}

static int sc200ais1_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc200ais1_id[] = {
	{"sc200ais1", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, sc200ais1_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver sc200ais1_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "sc200ais1",
	},
	.probe          = sc200ais1_probe,
	.remove         = sc200ais1_remove,
	.id_table       = sc200ais1_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_sc200ais1(void) {
	return private_i2c_add_driver(&sc200ais1_driver);
}

static __exit void exit_sc200ais1(void) {
	private_i2c_del_driver(&sc200ais1_driver);
}

module_init(init_sc200ais1);
module_exit(exit_sc200ais1);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_sc200ais1(void) {
	return private_i2c_add_driver(&sc200ais1_driver);
}

static void exit_first_sc200ais1(void) {
	private_i2c_del_driver(&sc200ais1_driver);
}

struct tx_isp_sensor_fast_attr sensor1 = {
	.name = "sc200ais1",
	.i2c_addr = 0x30,
	.width = 1920,
	.height = 1080,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_sc200ais1,
	.exit_sensor = exit_first_sc200ais1
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for sc200ais1 sensor");
MODULE_LICENSE("GPL");
