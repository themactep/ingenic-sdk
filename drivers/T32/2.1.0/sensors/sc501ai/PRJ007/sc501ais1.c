/*
 * sc501ais1.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps		dvdd
 *  0           2880*1620       30        mipi_2lane    linear  10        30
 *  1           1440*810        60        mipi_2lane    linear  10        60                binning
 *  2            896*504       100        mipi_2lane    linear  10        100               crop
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
#define SENSOR_VERSION  "H20250610a"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
//#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0xce)
#define SENSOR_CHIP_ID_L    (0x1f)
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

struct tx_isp_sensor_attribute sc501ais1_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sc501ais1_again_lut[] = {
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
	{0x352, 23414},
	{0x353, 24587},
	{0x354, 25746},
	{0x355, 26820},
	{0x356, 27953},
	{0x357, 29002},
	{0x358, 30109},
	{0x359, 31203},
	{0x35a, 32217},
	{0x35b, 33287},
	{0x35c, 34345},
	{0x35d, 35326},
	{0x35e, 36361},
	{0x35f, 37322},
	{0x360, 38336},
	{0x2340, 39339},
	{0x2341, 40824},
	{0x2342, 42226},
	{0x2343, 43666},
	{0x2344, 45085},
	{0x2345, 46426},
	{0x2346, 47804},
	{0x2347, 49163},
	{0x2348, 50503},
	{0x2349, 51769},
	{0x234a, 53072},
	{0x234b, 54358},
	{0x234c, 55574},
	{0x234d, 56826},
	{0x234e, 58062},
	{0x234f, 59232},
	{0x2350, 60437},
	{0x2351, 61627},
	{0x2352, 62753},
	{0x2353, 63914},
	{0x2354, 65062},
	{0x2355, 66148},
	{0x2356, 67269},
	{0x2357, 68376},
	{0x2358, 69471},
	{0x2359, 70508},
	{0x235a, 71578},
	{0x235b, 72637},
	{0x235c, 73640},
	{0x235d, 74676},
	{0x235e, 75700},
	{0x235f, 76672},
	{0x2360, 77675},
	{0x2361, 78668},
	{0x2362, 79609},
	{0x2363, 80582},
	{0x2364, 81545},
	{0x2365, 82458},
	{0x2366, 83402},
	{0x2367, 84337},
	{0x2368, 85262},
	{0x2369, 86140},
	{0x236a, 87048},
	{0x236b, 87948},
	{0x236c, 88802},
	{0x236d, 89685},
	{0x236e, 90560},
	{0x236f, 91390},
	{0x2370, 92250},
	{0x2371, 93101},
	{0x2372, 93910},
	{0x2373, 94747},
	{0x2374, 95576},
	{0x2375, 96364},
	{0x2376, 97179},
	{0x2377, 97988},
	{0x2378, 98789},
	{0x2379, 99551},
	{0x237a, 100340},
	{0x237b, 101122},
	{0x237c, 101865},
	{0x237d, 102634},
	{0x237e, 103398},
	{0x237f, 104123},
	{0x2740, 104875},
	{0x2741, 106329},
	{0x2742, 107792},
	{0x2743, 109202},
	{0x2744, 110621},
	{0x2745, 111991},
	{0x2746, 113340},
	{0x2747, 114699},
	{0x2748, 116011},
	{0x2749, 117305},
	{0x274a, 118608},
	{0x274b, 119867},
	{0x274c, 121136},
	{0x274d, 122362},
	{0x274e, 123573},
	{0x274f, 124793},
	{0x2750, 125973},
	{0x2751, 127138},
	{0x2752, 128313},
	{0x2753, 129450},
	{0x2754, 130598},
	{0x2755, 131708},
	{0x2756, 132805},
	{0x2757, 133912},
	{0x2758, 134984},
	{0x2759, 136044},
	{0x275a, 137114},
	{0x275b, 138151},
	{0x275c, 139198},
	{0x275d, 140212},
	{0x275e, 141215},
	{0x275f, 142229},
	{0x2760, 143211},
	{0x2761, 144183},
	{0x2762, 145166},
	{0x2763, 146118},
	{0x2764, 147081},
	{0x2765, 148014},
	{0x2766, 148938},
	{0x2767, 149873},
	{0x2768, 150779},
	{0x2769, 151676},
	{0x276a, 152584},
	{0x276b, 153465},
	{0x276c, 154356},
	{0x276d, 155221},
	{0x276e, 156077},
	{0x276f, 156944},
	{0x2770, 157786},
	{0x2771, 158619},
	{0x2772, 159463},
	{0x2773, 160283},
	{0x2774, 161112},
	{0x2775, 161917},
	{0x2776, 162715},
	{0x2777, 163524},
	{0x2778, 164309},
	{0x2779, 165087},
	{0x277a, 165876},
	{0x277b, 166641},
	{0x277c, 167417},
	{0x277d, 168170},
	{0x277e, 168918},
	{0x277f, 169675},
	{0x2f40, 170411},
	{0x2f41, 171881},
	{0x2f42, 173328},
	{0x2f43, 174738},
	{0x2f44, 176143},
	{0x2f45, 177527},
	{0x2f46, 178891},
	{0x2f47, 180221},
	{0x2f48, 181547},
	{0x2f49, 182855},
	{0x2f4a, 184144},
	{0x2f4b, 185403},
	{0x2f4c, 186659},
	{0x2f4d, 187898},
	{0x2f4e, 189121},
	{0x2f4f, 190316},
	{0x2f50, 191509},
	{0x2f51, 192686},
	{0x2f52, 193849},
	{0x2f53, 194986},
	{0x2f54, 196122},
	{0x2f55, 197244},
	{0x2f56, 198352},
	{0x2f57, 199437},
	{0x2f58, 200520},
	{0x2f59, 201591},
	{0x2f5a, 202650},
	{0x2f5b, 203687},
	{0x2f5c, 204723},
	{0x2f5d, 205748},
	{0x2f5e, 206762},
	{0x2f5f, 207754},
	{0x2f60, 208747},
	{0x2f61, 209729},
	{0x2f62, 210702},
	{0x2f63, 211654},
	{0x2f64, 212607},
	{0x2f65, 213550},
	{0x2f66, 214484},
	{0x2f67, 215399},
	{0x2f68, 216315},
	{0x2f69, 217222},
	{0x2f6a, 218120},
	{0x2f6b, 219001},
	{0x2f6c, 219883},
	{0x2f6d, 220757},
	{0x2f6e, 221623},
	{0x2f6f, 222471},
	{0x2f70, 223322},
	{0x2f71, 224164},
	{0x2f72, 224999},
	{0x2f73, 225819},
	{0x2f74, 226639},
	{0x2f75, 227453},
	{0x2f76, 228260},
	{0x2f77, 229051},
	{0x2f78, 229845},
	{0x2f79, 230631},
	{0x2f7a, 231412},
	{0x2f7b, 232177},
	{0x2f7c, 232945},
	{0x2f7d, 233706},
	{0x2f7e, 234462},
	{0x2f7f, 235203},
	{0x3f40, 235947},
	{0x3f41, 237417},
	{0x3f42, 238856},
	{0x3f43, 240282},
	{0x3f44, 241679},
	{0x3f45, 243063},
	{0x3f46, 244419},
	{0x3f47, 245764},
	{0x3f48, 247083},
	{0x3f49, 248391},
	{0x3f4a, 249674},
	{0x3f4b, 250946},
	{0x3f4c, 252195},
	{0x3f4d, 253434},
	{0x3f4e, 254651},
	{0x3f4f, 255859},
	{0x3f50, 257045},
	{0x3f51, 258222},
	{0x3f52, 259379},
	{0x3f53, 260528},
	{0x3f54, 261658},
	{0x3f55, 262780},
	{0x3f56, 263882},
	{0x3f57, 264978},
	{0x3f58, 266056},
	{0x3f59, 267127},
	{0x3f5a, 268181},
	{0x3f5b, 269228},
	{0x3f5c, 270259},
	{0x3f5d, 271284},
	{0x3f5e, 272292},
	{0x3f5f, 273295},
	{0x3f60, 274283},
	{0x3f61, 275265},
	{0x3f62, 276232},
	{0x3f63, 277195},
	{0x3f64, 278143},
	{0x3f65, 279086},
	{0x3f66, 280015},
	{0x3f67, 280940},
	{0x3f68, 281851},
	{0x3f69, 282758},
	{0x3f6a, 283652},
	{0x3f6b, 284542},
	{0x3f6c, 285419},
	{0x3f6d, 286293},
	{0x3f6e, 287154},
	{0x3f6f, 288012},
	{0x3f70, 288858},
	{0x3f71, 289700},
	{0x3f72, 290531},
	{0x3f73, 291359},
	{0x3f74, 292175},
	{0x3f75, 292989},
	{0x3f76, 293792},
	{0x3f77, 294592},
	{0x3f78, 295381},
	{0x3f79, 296167},
	{0x3f7a, 296944},
	{0x3f7b, 297717},
	{0x3f7c, 298481},
	{0x3f7d, 299242},
	{0x3f7e, 299994},
	{0x3f7f, 300743},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int sc501ais1_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc501ais1_again_lut;
	while (lut->gain <= sc501ais1_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sc501ais1_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int sc501ais1_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = sc501ais1_again_lut;
	while(lut->gain <= sc501ais1_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc501ais1_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int sc501ais1_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int sc501ais1_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int sc501ais1_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus sc501ais1_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 792,
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

struct tx_isp_dvp_bus sc501ais1_dvp = {
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

struct tx_isp_sensor_attribute sc501ais1_attr = {
	.name = "sc501ais1",
	.chip_id = 0xce1f,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x32,
	.sensor_ctrl.alloc_again = sc501ais1_alloc_again,
	.sensor_ctrl.alloc_dgain = sc501ais1_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = sc501ais1_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list sc501ais1_init_regs_2880_1620_30fps_mipi[] = {
    //cleaned_0x17_SC500AI_24MInput_MIPI_2lane_792Mbps_10bit_2880x1620_30fps
    {0x0103,0x01},
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x36f9,0x80},
    {0x3018,0x32},
    {0x3019,0x0c},
    {0x301f,0x17},

    /* {0x320c,0x06},//hts = 0x640 = 1600 */
    /* {0x320d,0x40},// */
    /* {0x320e,0x06},//vts = 0x672 = 1650 */
    /* {0x320f,0x72},// */

 /*   {0x320e,0x0c},//vts = 0xce4 = 3300 15fps*/
/*    {0x320f,0xe4},//   */
    {0x3253,0x0a},
    {0x3301,0x0a},
    {0x3302,0x18},
    {0x3303,0x10},
    {0x3304,0x60},
    {0x3306,0x60},
    {0x3308,0x10},
    {0x3309,0x70},
    {0x330a,0x00},
    {0x330b,0xf0},
    {0x330d,0x18},
    {0x330e,0x20},
    {0x330f,0x02},
    {0x3310,0x02},
    {0x331c,0x04},
    {0x331e,0x51},
    {0x331f,0x61},
    {0x3320,0x09},
    {0x3333,0x10},
    {0x334c,0x08},
    {0x3356,0x09},
    {0x3364,0x17},
    {0x336d,0x03},
    {0x3390,0x08},
    {0x3391,0x18},
    {0x3392,0x38},
    {0x3393,0x0a},
    {0x3394,0x20},
    {0x3395,0x20},
    {0x3396,0x08},
    {0x3397,0x18},
    {0x3398,0x38},
    {0x3399,0x0a},
    {0x339a,0x20},
    {0x339b,0x20},
    {0x339c,0x20},
    {0x33ac,0x10},
    {0x33ae,0x10},
    {0x33af,0x19},
    {0x360f,0x01},
    {0x3622,0x03},
    {0x363a,0x1f},
    {0x363c,0x40},
    {0x3651,0x7d},
    {0x3670,0x0a},
    {0x3671,0x07},
    {0x3672,0x17},
    {0x3673,0x1e},
    {0x3674,0x82},
    {0x3675,0x64},
    {0x3676,0x66},
    {0x367a,0x48},
    {0x367b,0x78},
    {0x367c,0x58},
    {0x367d,0x78},
    {0x3690,0x34},
    {0x3691,0x34},
    {0x3692,0x54},
    {0x369c,0x48},
    {0x369d,0x78},
    {0x36ea,0x35},
    {0x36eb,0x0c},
    {0x36ec,0x0a},
    {0x36ed,0x34},
    {0x36fa,0xf5},
    {0x36fb,0x35},
    {0x36fc,0x10},
    {0x36fd,0x04},
    {0x3904,0x04},
    {0x3908,0x41},
    {0x391d,0x04},
    {0x39c2,0x30},
    {0x3e01,0xcd},
    {0x3e02,0xc0},
    {0x3e16,0x00},
    {0x3e17,0x80},
    {0x4500,0x88},
    {0x4509,0x20},
    {0x4837,0x14},
    {0x5799,0x00},
    {0x59e0,0x60},
    {0x59e1,0x08},
    {0x59e2,0x3f},
    {0x59e3,0x18},
    {0x59e4,0x18},
    {0x59e5,0x3f},
    {0x59e7,0x02},
    {0x59e8,0x38},
    {0x59e9,0x20},
    {0x59ea,0x0c},
    {0x59ec,0x08},
    {0x59ed,0x02},
    {0x59ee,0xa0},
    {0x59ef,0x08},
    {0x59f4,0x18},
    {0x59f5,0x10},
    {0x59f6,0x0c},
    {0x59f9,0x02},
    {0x59fa,0x18},
    {0x59fb,0x10},
    {0x59fc,0x0c},
    {0x59ff,0x02},
    {0x36e9,0x20},
    {0x36f9,0x57},
    {0x0100,0x01},
    {SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sc501ais1_init_regs_1440_810_60fps_mipi[] = {
    //Cleaned_0x165_SC500AI_24MInput_MIPI_2lane_432Mbps_10bit_1440x810_60fps_hsub_vbin
    //VTS=900.000000,HTS=1600.000000,SCLK=86.400000,PCLK=86.400000,MipiCLK=432.000000,Tline=18.518519us,TExp_step=0.5*Tline=9.259259us,TExp_offset=3.634259us
    {0x0103,0x01},
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x36f9,0x80},
    {0x3000,0x01},
    {0x3018,0x32},
    {0x3019,0x0c},
    {0x301f,0x65},
    {0x3208,0x05},
    {0x3209,0xa0},
    {0x320a,0x03},
    {0x320b,0x2a},
    {0x320e,0x03},//vts = 0x384 = 900
    {0x320f,0x84},//
    {0x3211,0x02},
    {0x3213,0x02},
    {0x3215,0x31},
    {0x3220,0x17},
    {0x3253,0x0a},
    {0x3301,0x0a},
    {0x3302,0x18},
    {0x3303,0x10},
    {0x3304,0x60},
    {0x3306,0x60},
    {0x3308,0x10},
    {0x3309,0x70},
    {0x330a,0x00},
    {0x330b,0xf0},
    {0x330d,0x18},
    {0x330e,0x20},
    {0x330f,0x02},
    {0x3310,0x02},
    {0x331c,0x04},
    {0x331e,0x51},
    {0x331f,0x61},
    {0x3320,0x09},
    {0x3333,0x10},
    {0x334c,0x08},
    {0x3356,0x09},
    {0x3364,0x17},
    {0x336d,0x03},
    {0x3390,0x08},
    {0x3391,0x18},
    {0x3392,0x38},
    {0x3393,0x0a},
    {0x3394,0x20},
    {0x3395,0x20},
    {0x3396,0x08},
    {0x3397,0x18},
    {0x3398,0x38},
    {0x3399,0x0a},
    {0x339a,0x20},
    {0x339b,0x20},
    {0x339c,0x20},
    {0x33ac,0x10},
    {0x33ae,0x10},
    {0x33af,0x19},
    {0x360f,0x01},
    {0x3622,0x03},
    {0x363a,0x1f},
    {0x363c,0x40},
    {0x3651,0x7d},
    {0x3670,0x0a},
    {0x3671,0x07},
    {0x3672,0x17},
    {0x3673,0x1e},
    {0x3674,0x82},
    {0x3675,0x64},
    {0x3676,0x66},
    {0x367a,0x48},
    {0x367b,0x78},
    {0x367c,0x58},
    {0x367d,0x78},
    {0x3690,0x34},
    {0x3691,0x34},
    {0x3692,0x54},
    {0x369c,0x48},
    {0x369d,0x78},
    {0x36ea,0x34},
    {0x36eb,0x0c},
    {0x36ec,0x1a},
    {0x36ed,0x14},
    {0x36fa,0xf4},
    {0x36fb,0x35},
    {0x36fc,0x00},
    {0x36fd,0x04},
    {0x3904,0x04},
    {0x3908,0x41},
    {0x391d,0x04},
    {0x39c2,0x30},
    {0x3e01,0x6f},
    {0x3e02,0xe0},
    {0x3e16,0x00},
    {0x3e17,0x80},
    {0x4500,0x88},
    {0x4509,0x20},
    {0x4837,0x25},
    {0x5000,0x46},
    {0x5799,0x00},
    {0x5900,0x01},
    {0x5901,0x04},
    {0x59e0,0x60},
    {0x59e1,0x08},
    {0x59e2,0x3f},
    {0x59e3,0x18},
    {0x59e4,0x18},
    {0x59e5,0x3f},
    {0x59e7,0x02},
    {0x59e8,0x38},
    {0x59e9,0x20},
    {0x59ea,0x0c},
    {0x59ec,0x08},
    {0x59ed,0x02},
    {0x59ee,0xa0},
    {0x59ef,0x08},
    {0x59f4,0x18},
    {0x59f5,0x10},
    {0x59f6,0x0c},
    {0x59f9,0x02},
    {0x59fa,0x18},
    {0x59fb,0x10},
    {0x59fc,0x0c},
    {0x59ff,0x02},
    {0x36e9,0x53},
    {0x36f9,0x53},
    {0x0100,0x01},
    {SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sc501ais1_init_regs_896_504_100fps_mipi[] = {
    //Cleaned_0x166_SC500AI_24MInput_MIPI_2lane_432Mbps_10bit_896x504_100fps_hsub_vbin
    //VTS=540.000000,HTS=1600.000000,SCLK=86.400000,PCLK=86.400000,MipiCLK=432.000000,Tline=18.518519us,TExp_step=0.5*Tline=9.259259us,TExp_offset=3.634259us
    {0x0103,0x01},
    {0x0100,0x00},
    {0x36e9,0x80},
    {0x36f9,0x80},
    {0x3000,0x01},
    {0x3018,0x32},
    {0x3019,0x0c},
    {0x301f,0x66},
    {0x3200,0x02},
    {0x3201,0x20},
    {0x3202,0x01},
    {0x3203,0x32},
    {0x3204,0x09},
    {0x3205,0x27},
    {0x3206,0x05},
    {0x3207,0x29},
    {0x3208,0x03},
    {0x3209,0x80},
    {0x320a,0x01},
    {0x320b,0xf8},
    {0x320e,0x02},
    {0x320f,0x1c},
    {0x3210,0x00},
    {0x3211,0x02},
    {0x3212,0x00},
    {0x3213,0x02},
    {0x3215,0x31},
    {0x3220,0x17},
    {0x3253,0x0a},
    {0x3301,0x0a},
    {0x3302,0x18},
    {0x3303,0x10},
    {0x3304,0x60},
    {0x3306,0x60},
    {0x3308,0x10},
    {0x3309,0x70},
    {0x330a,0x00},
    {0x330b,0xf0},
    {0x330d,0x18},
    {0x330e,0x20},
    {0x330f,0x02},
    {0x3310,0x02},
    {0x331c,0x04},
    {0x331e,0x51},
    {0x331f,0x61},
    {0x3320,0x09},
    {0x3333,0x10},
    {0x334c,0x08},
    {0x3356,0x09},
    {0x3364,0x17},
    {0x336d,0x03},
    {0x3390,0x08},
    {0x3391,0x18},
    {0x3392,0x38},
    {0x3393,0x0a},
    {0x3394,0x20},
    {0x3395,0x20},
    {0x3396,0x08},
    {0x3397,0x18},
    {0x3398,0x38},
    {0x3399,0x0a},
    {0x339a,0x20},
    {0x339b,0x20},
    {0x339c,0x20},
    {0x33ac,0x10},
    {0x33ae,0x10},
    {0x33af,0x19},
    {0x360f,0x01},
    {0x3622,0x03},
    {0x363a,0x1f},
    {0x363c,0x40},
    {0x3651,0x7d},
    {0x3670,0x0a},
    {0x3671,0x07},
    {0x3672,0x17},
    {0x3673,0x1e},
    {0x3674,0x82},
    {0x3675,0x64},
    {0x3676,0x66},
    {0x367a,0x48},
    {0x367b,0x78},
    {0x367c,0x58},
    {0x367d,0x78},
    {0x3690,0x34},
    {0x3691,0x34},
    {0x3692,0x54},
    {0x369c,0x48},
    {0x369d,0x78},
    {0x36ea,0x34},
    {0x36eb,0x0c},
    {0x36ec,0x1a},
    {0x36ed,0x14},
    {0x36fa,0xf4},
    {0x36fb,0x35},
    {0x36fc,0x00},
    {0x36fd,0x04},
    {0x3904,0x04},
    {0x3908,0x41},
    {0x391d,0x04},
    {0x39c2,0x30},
    {0x3e01,0x42},
    {0x3e02,0xe0},
    {0x3e16,0x00},
    {0x3e17,0x80},
    {0x4500,0x88},
    {0x4509,0x20},
    {0x4837,0x25},
    {0x5000,0x46},
    {0x5799,0x00},
    {0x5900,0x01},
    {0x5901,0x04},
    {0x59e0,0x60},
    {0x59e1,0x08},
    {0x59e2,0x3f},
    {0x59e3,0x18},
    {0x59e4,0x18},
    {0x59e5,0x3f},
    {0x59e7,0x02},
    {0x59e8,0x38},
    {0x59e9,0x20},
    {0x59ea,0x0c},
    {0x59ec,0x08},
    {0x59ed,0x02},
    {0x59ee,0xa0},
    {0x59ef,0x08},
    {0x59f4,0x18},
    {0x59f5,0x10},
    {0x59f6,0x0c},
    {0x59f9,0x02},
    {0x59fa,0x18},
    {0x59fb,0x10},
    {0x59fc,0x0c},
    {0x59ff,0x02},
    {0x36e9,0x53},
    {0x36f9,0x53},
    {0x0100,0x01},
    {SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the sc501ais1_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc501ais1_win_sizes[] = {
	{
		.width          = 2880,
		.height         = 1620,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc501ais1_init_regs_2880_1620_30fps_mipi,
	},
	{
		.width          = 1440,
		.height         = 810,
		.fps            = 60 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc501ais1_init_regs_1440_810_60fps_mipi,
	},
	{
		.width          = 896,
		.height         = 504,
		.fps            = 100 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = sc501ais1_init_regs_896_504_100fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &sc501ais1_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sc501ais1_stream_on_mipi[] = {
	{0x0100, 0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list sc501ais1_stream_off_mipi[] = {
	{0x0100, 0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int sc501ais1_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int sc501ais1_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int sc501ais1_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc501ais1_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc501ais1_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc501ais1_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int sc501ais1_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int sc501ais1_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc501ais1_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc501ais1_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int sc501ais1_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc501ais1_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int sc501ais1_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int sc501ais1_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int sc501ais1_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &sc501ais1_win_sizes[0];
		sc501ais1_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc501ais1_attr.again.value = 0;
		sc501ais1_attr.again.max = 300743;
		sc501ais1_attr.again.min = 0;
		sc501ais1_attr.again.apply_delay = 2;

		sc501ais1_attr.integration_time.value = 1645;
		sc501ais1_attr.integration_time.max = 1650 - 5;
		sc501ais1_attr.integration_time.min = 2;
		sc501ais1_attr.integration_time.apply_delay = 2;

		sc501ais1_attr.total_width = 1600;
		sc501ais1_attr.total_height = 1650;

		sc501ais1_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		sc501ais1_attr.hcg.base_gain = ;
		sc501ais1_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc501ais1_attr.again_short.value = ;
		sc501ais1_attr.again_short.max = ;
		sc501ais1_attr.again_short.min = ;
		sc501ais1_attr.again_short.apply_delay = ;

		sc501ais1_attr.integration_time_short.value = ;
		sc501ais1_attr.integration_time_short.max = ;
		sc501ais1_attr.integration_time_short.min = ;
		sc501ais1_attr.integration_time_short.apply_delay = ;

		sc501ais1_attr.wdr_cache = wdr_line * sc501ais1_attr.total_width;

#ifdef SENSOR_HCG
		sc501ais1_attr.hcg_short.base_gain = ;
		sc501ais1_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(sc501ais1_attr.mipi)), (void *)(&sc501ais1_mipi_linear), sizeof(sc501ais1_attr.mipi));
		break;
	case 1:
		wsize = &sc501ais1_win_sizes[1];
		sc501ais1_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc501ais1_attr.again.value = 0;
		sc501ais1_attr.again.max = 300743;
		sc501ais1_attr.again.min = 0;
		sc501ais1_attr.again.apply_delay = 2;

		sc501ais1_attr.integration_time.value = 164;
		sc501ais1_attr.integration_time.max = 900 - 5;
		sc501ais1_attr.integration_time.min = 2;
		sc501ais1_attr.integration_time.apply_delay = 2;

		sc501ais1_attr.total_width = 1600;
		sc501ais1_attr.total_height = 900;

		sc501ais1_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		sc501ais1_attr.hcg.base_gain = ;
		sc501ais1_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc501ais1_attr.again_short.value = ;
		sc501ais1_attr.again_short.max = ;
		sc501ais1_attr.again_short.min = ;
		sc501ais1_attr.again_short.apply_delay = ;

		sc501ais1_attr.integration_time_short.value = ;
		sc501ais1_attr.integration_time_short.max = ;
		sc501ais1_attr.integration_time_short.min = ;
		sc501ais1_attr.integration_time_short.apply_delay = ;

		sc501ais1_attr.wdr_cache = wdr_line * sc501ais1_attr.total_width;

#ifdef SENSOR_HCG
		sc501ais1_attr.hcg_short.base_gain = ;
		sc501ais1_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

        memcpy((void *)(&(sc501ais1_attr.mipi)), (void *)(&sc501ais1_mipi_linear), sizeof(sc501ais1_attr.mipi));
        sc501ais1_attr.mipi.clk = 432;
        sc501ais1_attr.mipi.image_twidth = 1440;
        sc501ais1_attr.mipi.image_theight = 810;
        break;
	case 2:
		wsize = &sc501ais1_win_sizes[2];
		sc501ais1_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		sc501ais1_attr.again.value = 0;
		sc501ais1_attr.again.max = 300743;
		sc501ais1_attr.again.min = 0;
		sc501ais1_attr.again.apply_delay = 2;

		sc501ais1_attr.integration_time.value = 164;
		sc501ais1_attr.integration_time.max = 540 - 5;
		sc501ais1_attr.integration_time.min = 2;
		sc501ais1_attr.integration_time.apply_delay = 2;

		sc501ais1_attr.total_width = 1600;
		sc501ais1_attr.total_height = 540;

		sc501ais1_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		sc501ais1_attr.hcg.base_gain = ;
		sc501ais1_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		sc501ais1_attr.again_short.value = ;
		sc501ais1_attr.again_short.max = ;
		sc501ais1_attr.again_short.min = ;
		sc501ais1_attr.again_short.apply_delay = ;

		sc501ais1_attr.integration_time_short.value = ;
		sc501ais1_attr.integration_time_short.max = ;
		sc501ais1_attr.integration_time_short.min = ;
		sc501ais1_attr.integration_time_short.apply_delay = ;

		sc501ais1_attr.wdr_cache = wdr_line * sc501ais1_attr.total_width;

#ifdef SENSOR_HCG
		sc501ais1_attr.hcg_short.base_gain = ;
		sc501ais1_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

        memcpy((void *)(&(sc501ais1_attr.mipi)), (void *)(&sc501ais1_mipi_linear), sizeof(sc501ais1_attr.mipi));
        sc501ais1_attr.mipi.clk = 432;
        sc501ais1_attr.mipi.image_twidth = 896;
        sc501ais1_attr.mipi.image_theight = 504;
        break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int sc501ais1_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	sc501ais1_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		sc501ais1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc501ais1_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		sc501ais1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc501ais1_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		sc501ais1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = sc501ais1_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

    sensor->video.mbus.code = wsize->mbus_code;
    sensor->video.fps.value = wsize->fps;
    sensor->video.fps.max = wsize->fps;
    sensor->video.fps.apply_delay = 1;
	sc501ais1_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int sc501ais1_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sc501ais1_read(sd, 0x3107, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc501ais1_read(sd, 0x3108, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc501ais1_g_chip_ident(struct tx_isp_subdev *sd,
								 struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sc501ais1_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "sc501ais1_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "sc501ais1_pwdn");
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
	ret = sc501ais1_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc501ais1 chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}


	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", sc501ais1_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "sc501ais1", sizeof("sc501ais1"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc501ais1_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sc501ais1_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = sc501ais1_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int sc501ais1_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc501ais1_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc501ais1_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc501ais1_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int sc501ais1_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = sc501ais1_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sc501ais1_write_array(sd, sc501ais1_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("sc501ais1 stream on\n");
		}

	} else {
		ret = sc501ais1_write_array(sd, sc501ais1_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("sc501ais1 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int sc501ais1_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	it = (it << 1) - 1;
	ret += sc501ais1_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc501ais1_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc501ais1_write(sd, 0x3e02, (unsigned char)((it & 0xf) << 4));

	ret += sc501ais1_write(sd, 0x3e08, (unsigned char)((again >> 8) & 0xff));
	ret += sc501ais1_write(sd, 0x3e09, (unsigned char)(again & 0xff));

	return ret;
}
#else
static int sc501ais1_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	value = (value << 1) - 1;
	ret += sc501ais1_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc501ais1_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc501ais1_write(sd, 0x3e02, (unsigned char)((value & 0xf) << 4));

	return ret;
}

static int sc501ais1_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	ret += sc501ais1_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
	ret += sc501ais1_write(sd, 0x3e09, (unsigned char)(value & 0xff));

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc501ais1_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc501ais1_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc501ais1_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = sc501ais1_attr_set(sd, wsize);
	}

	return ret;
}

static int sc501ais1_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 1600 * 1650 * 30;  /**< HTS * VTS * FPS */
		max_fps = wsize->fps;
		break;
	case 1:
		sclk = 1600 * 900 * 60;  /**< HTS * VTS * FPS */
		max_fps = wsize->fps;
		break;
	case 2:
		sclk = 1600 * 540 * 100;  /**< HTS * VTS * FPS */
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
	ret += sc501ais1_read(sd, 0x320c, &val);
	hts = val << 8;
	val = 0;
	ret += sc501ais1_read(sd, 0x320d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: sc501ais1 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	sc501ais1_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	sc501ais1_write(sd, 0x320e, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: sc501ais1_write err\n");
		return ret;
	}

	sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->integration_time.max = vts - 5;
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

static int sc501ais1_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = ISP_SUCCESS;
	unsigned char val = 0;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	ret = sc501ais1_read(sd, 0x3221, &val);
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
	ret += sc501ais1_write(sd, 0x3221, val);

	sensor->video.hvflip_mode = par->hvflip;
	sc501ais1_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int sc501ais1_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int sc501ais1_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int sc501ais1_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int sc501ais1_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = sc501ais1_write_array(sd, sc501ais1_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		sc501ais1_setting_select(sd, 1);
		sc501ais1_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		sc501ais1_setting_select(sd, 0);
		sc501ais1_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int sc501ais1_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = sc501ais1_write_array(sd, wsize->regs);
	ret = sc501ais1_write_array(sd, sc501ais1_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int sc501ais1_pm_s_stream_ctrl(struct tx_isp_subdev *sd, void *arg)
{
	int ret = ISP_SUCCESS;
	IMPISPUserCtl *data = (IMPISPUserCtl *)arg;
	if(data->cmd == TX_SENSOR_PM_RESUME){
		ret = sc501ais1_write_array(sd, sc501ais1_stream_on_mipi);
		if(ret != 0){
			printk("%s streamon failed\n",sc501ais1_attr.name);
			return ret;
		}
		printk("%s TX_SENSOR_PM_RESUME\n",sc501ais1_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_SUSPEND) {
		ret = sc501ais1_write_array(sd, sc501ais1_stream_off_mipi);
		if (ret < 0) {
			printk("\n TX_SENSOR_PM_SUSPEND error !!!\n");
			return ret;
		}
		printk("%s TX_SENSOR_PM_SUSPEND.\n",sc501ais1_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_PREPARE) {
		printk("%s TX_SENSOR_PM_PREPARE.\n",sc501ais1_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_COMPLETE) {
		printk("%s TX_SENSOR_PM_COMPLETE.\n",sc501ais1_attr.name);

	}else if(data->cmd == TX_SENSOR_PM_STREAMON) {
		ret = sc501ais1_write_array(sd, sc501ais1_stream_on_mipi);
		if (ret < 0) {
			printk("\n TX_SENSOR_PM_STREAMON error !!!\n");
			return ret;
		}
		printk("%s TX_SENSOR_PM_STREAMON.\n",sc501ais1_attr.name);
	}else if(data->cmd == TX_SENSOR_PM_STREAMOFF) {
		ret = sc501ais1_write_array(sd, sc501ais1_stream_off_mipi);
		if (ret < 0) {
			printk("\n [%s %d] TX_SENSOR_PM_STREAMOFF error !!!\n", __func__, __LINE__);
			return ret;
		}
		printk("%s TX_SENSOR_PM_STREAMOFF.\n",sc501ais1_attr.name);
	}else {
		printk("[%s]Don't Support this function.\n",__func__);
		return -EINVAL;
	}

	return ret;
}

static int sc501ais1_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = sc501ais1_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sc501ais1_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sc501ais1_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sc501ais1_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sc501ais1_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sc501ais1_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = sc501ais1_write_array(sd, sc501ais1_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = sc501ais1_write_array(sd, sc501ais1_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sc501ais1_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = sc501ais1_set_hvflip(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_PM_S_STREAM_CTRL:
		if(arg)
			ret = sc501ais1_pm_s_stream_ctrl(sd, arg);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = sc501ais1_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = sc501ais1_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = sc501ais1_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = sc501ais1_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = sc501ais1_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops sc501ais1_core_ops = {
	.g_chip_ident = sc501ais1_g_chip_ident,
	.reset = sc501ais1_reset,
	.init = sc501ais1_init,
	.g_register = sc501ais1_g_register,
	.s_register = sc501ais1_s_register,
};

static struct tx_isp_subdev_video_ops sc501ais1_video_ops = {
	.s_stream = sc501ais1_s_stream,
};

static struct tx_isp_subdev_sensor_ops sc501ais1_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = sc501ais1_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops sc501ais1_ops = {
	.core = &sc501ais1_core_ops,
	.video = &sc501ais1_video_ops,
	.sensor = &sc501ais1_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "sc501ais1",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc501ais1_probe(struct i2c_client *client,
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
	sensor->video.attr = &sc501ais1_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc501ais1_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc501ais1\n");

	return 0;
}

static int sc501ais1_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc501ais1_id[] = {
	{"sc501ais1", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, sc501ais1_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver sc501ais1_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "sc501ais1",
	},
	.probe          = sc501ais1_probe,
	.remove         = sc501ais1_remove,
	.id_table       = sc501ais1_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_sc501ais1(void) {
	return private_i2c_add_driver(&sc501ais1_driver);
}

static __exit void exit_sc501ais1(void) {
	private_i2c_del_driver(&sc501ais1_driver);
}

module_init(init_sc501ais1);
module_exit(exit_sc501ais1);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_sc501ais1(void) {
	return private_i2c_add_driver(&sc501ais1_driver);
}

static void exit_first_sc501ais1(void) {
	private_i2c_del_driver(&sc501ais1_driver);
}

struct tx_isp_sensor_fast_attr sensor1 = {
	.name = "sc501ais1",
	.i2c_addr = 0x30,
	.width = 2880,
	.height = 1620,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_sc501ais1,
	.exit_sensor = exit_first_sc501ais1
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for sc501ais1 sensor");
MODULE_LICENSE("GPL");
