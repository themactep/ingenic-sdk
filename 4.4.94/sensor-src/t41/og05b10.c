/*
 * og05b10.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          2560*1440       30        mipi_2lane             linear
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
#define SENSOR_VERSION  "H20250114a"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
// #define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */

#define SENSOR_CHIP_ID_H    (0x58)
#define SENSOR_CHIP_ID_M    (0x05)
#define SENSOR_CHIP_ID_L    (0x42)
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_MCLK 24000000

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

struct tx_isp_sensor_attribute og05b10_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};
//gain biao not sure
struct again_lut og05b10_again_lut[] = {
	// cnt_gain = 233 cnt_reg = 233
	{0x10, 0},
	{0x11, 5731},
	{0x12, 11136},
	{0x13, 16247},
	{0x14, 21097},
	{0x15, 25710},
	{0x16, 30108},
	{0x17, 34311},
	{0x18, 38335},
	{0x19, 42195},
	{0x1a, 45903},
	{0x1b, 49471},
	{0x1c, 52910},
	{0x1d, 56227},
	{0x1e, 59433},
	{0x1f, 62533},
	{0x20, 65535},
	{0x21, 68444},
	{0x22, 71266},
	{0x23, 74007},
	{0x24, 76671},
	{0x25, 79261},
	{0x26, 81782},
	{0x27, 84238},
	{0x28, 86632},
	{0x29, 88967},
	{0x2a, 91245},
	{0x2b, 93470},
	{0x2c, 95643},
	{0x2d, 97768},
	{0x2e, 99846},
	{0x2f, 101879},
	{0x30, 103870},
	{0x31, 105820},
	{0x32, 107730},
	{0x33, 109602},
	{0x34, 111438},
	{0x35, 113239},
	{0x36, 115006},
	{0x37, 116741},
	{0x38, 118445},
	{0x39, 120118},
	{0x3a, 121762},
	{0x3b, 123379},
	{0x3c, 124968},
	{0x3d, 126530},
	{0x3e, 128068},
	{0x3f, 129581},
	{0x40, 131070},
	{0x41, 132535},
	{0x42, 133979},
	{0x43, 135401},
	{0x44, 136801},
	{0x45, 138182},
	{0x46, 139542},
	{0x47, 140883},
	{0x48, 142206},
	{0x49, 143510},
	{0x4a, 144796},
	{0x4b, 146065},
	{0x4c, 147317},
	{0x4d, 148553},
	{0x4e, 149773},
	{0x4f, 150978},
	{0x50, 152167},
	{0x51, 153342},
	{0x52, 154502},
	{0x53, 155648},
	{0x54, 156780},
	{0x55, 157899},
	{0x56, 159005},
	{0x57, 160098},
	{0x58, 161178},
	{0x59, 162247},
	{0x5a, 163303},
	{0x5b, 164348},
	{0x5c, 165381},
	{0x5d, 166403},
	{0x5e, 167414},
	{0x5f, 168415},
	{0x60, 169405},
	{0x61, 170385},
	{0x62, 171355},
	{0x63, 172314},
	{0x64, 173265},
	{0x65, 174205},
	{0x66, 175137},
	{0x67, 176059},
	{0x68, 176973},
	{0x69, 177878},
	{0x6a, 178774},
	{0x6b, 179662},
	{0x6c, 180541},
	{0x6d, 181412},
	{0x6e, 182276},
	{0x6f, 183132},
	{0x70, 183980},
	{0x71, 184820},
	{0x72, 185653},
	{0x73, 186479},
	{0x74, 187297},
	{0x75, 188109},
	{0x76, 188914},
	{0x77, 189711},
	{0x78, 190503},
	{0x79, 191287},
	{0x7a, 192065},
	{0x7b, 192837},
	{0x7c, 193603},
	{0x7d, 194362},
	{0x7e, 195116},
	{0x7f, 195863},
	{0x80, 196605},
	{0x81, 197340},
	{0x82, 198070},
	{0x83, 198795},
	{0x84, 199514},
	{0x85, 200227},
	{0x86, 200936},
	{0x87, 201639},
	{0x88, 202336},
	{0x89, 203029},
	{0x8a, 203717},
	{0x8b, 204399},
	{0x8c, 205077},
	{0x8d, 205750},
	{0x8e, 206418},
	{0x8f, 207082},
	{0x90, 207741},
	{0x91, 208395},
	{0x92, 209045},
	{0x93, 209690},
	{0x94, 210331},
	{0x95, 210968},
	{0x96, 211600},
	{0x97, 212228},
	{0x98, 212852},
	{0x99, 213472},
	{0x9a, 214088},
	{0x9b, 214700},
	{0x9c, 215308},
	{0x9d, 215912},
	{0x9e, 216513},
	{0x9f, 217109},
	{0xa0, 217702},
	{0xa1, 218291},
	{0xa2, 218877},
	{0xa3, 219458},
	{0xa4, 220037},
	{0xa5, 220611},
	{0xa6, 221183},
	{0xa7, 221751},
	{0xa8, 222315},
	{0xa9, 222876},
	{0xaa, 223434},
	{0xab, 223988},
	{0xac, 224540},
	{0xad, 225088},
	{0xae, 225633},
	{0xaf, 226175},
	{0xb0, 226713},
	{0xb1, 227249},
	{0xb2, 227782},
	{0xb3, 228311},
	{0xb4, 228838},
	{0xb5, 229362},
	{0xb6, 229883},
	{0xb7, 230401},
	{0xb8, 230916},
	{0xb9, 231429},
	{0xba, 231938},
	{0xbb, 232445},
	{0xbc, 232949},
	{0xbd, 233451},
	{0xbe, 233950},
	{0xbf, 234446},
	{0xc0, 234940},
	{0xc1, 235431},
	{0xc2, 235920},
	{0xc3, 236406},
	{0xc4, 236890},
	{0xc5, 237371},
	{0xc6, 237849},
	{0xc7, 238326},
	{0xc8, 238800},
	{0xc9, 239271},
	{0xca, 239740},
	{0xcb, 240207},
	{0xcc, 240672},
	{0xcd, 241134},
	{0xce, 241594},
	{0xcf, 242052},
	{0xd0, 242508},
	{0xd1, 242961},
	{0xd2, 243413},
	{0xd3, 243862},
	{0xd4, 244309},
	{0xd5, 244754},
	{0xd6, 245197},
	{0xd7, 245637},
	{0xd8, 246076},
	{0xd9, 246513},
	{0xda, 246947},
	{0xdb, 247380},
	{0xdc, 247811},
	{0xdd, 248240},
	{0xde, 248667},
	{0xdf, 249091},
	{0xe0, 249515},
	{0xe1, 249936},
	{0xe2, 250355},
	{0xe3, 250772},
	{0xe4, 251188},
	{0xe5, 251602},
	{0xe6, 252014},
	{0xe7, 252424},
	{0xe8, 252832},
	{0xe9, 253239},
	{0xea, 253644},
	{0xeb, 254047},
	{0xec, 254449},
	{0xed, 254848},
	{0xee, 255246},
	{0xef, 255643},
	{0xf0, 256038},
	{0xf1, 256431},
	{0xf2, 256822},
	{0xf3, 257212},
	{0xf4, 257600},
	{0xf5, 257987},
	{0xf6, 258372},
	{0xf7, 258756},
	{0xf8, 259138},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int og05b10_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = og05b10_again_lut;
	while (lut->gain <= og05b10_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == og05b10_attr.max_again) && (isp_gain >= lut->gain)) {
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
unsigned int og05b10_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = og05b10_again_lut;
	while(lut->gain <= og05b10_attr.max_again_short) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == og05b10_attr.max_again_short) && (isp_gain >= lut->gain)) {
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

unsigned int og05b10_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus og05b10_30fps_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 960,
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

struct tx_isp_dvp_bus og05b10_dvp = {
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

struct tx_isp_sensor_attribute og05b10_attr = {
	.name = "og05b10",
	.chip_id = 0x580542,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x36,
	.sensor_ctrl.alloc_again = og05b10_alloc_again,
	.sensor_ctrl.alloc_dgain = og05b10_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = og05b10_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */
};

static struct regval_list og05b10_init_regs_2560_1440_30fps_mipi[] = {
	{0x0107, 0x01},
	{SENSOR_REG_DELAY, 10},
	{0x3020, 0x01},
	{0x0104, 0x00},
	{0x0301, 0x1a},
	{0x0304, 0x01},
	{0x0305, 0xe0},
	{0x0306, 0x04},
	{0x0307, 0x01},
	{0x0321, 0x03},
	{0x0324, 0x01},
	{0x0325, 0x80},
	{0x0341, 0x03},
	{0x0344, 0x01},
	{0x0345, 0xb0},
	{0x0347, 0x07},
	{0x034b, 0x06},
	{0x0360, 0x80},
	{0x0400, 0xe8},
	{0x0401, 0x00},
	{0x0402, 0x2b},
	{0x0403, 0x32},
	{0x0404, 0x3a},
	{0x0405, 0x00},
	{0x0406, 0x0c},
	{0x0407, 0xe8},
	{0x0408, 0x00},
	{0x0409, 0x2b},
	{0x040a, 0x32},
	{0x040b, 0x5c},
	{0x040c, 0xcd},
	{0x040d, 0x0c},
	{0x040e, 0xe7},
	{0x040f, 0xff},
	{0x0410, 0x2b},
	{0x0411, 0x32},
	{0x0412, 0x33},
	{0x0413, 0x8f},
	{0x0414, 0x0c},
	{0x2000, 0x04},
	{0x2805, 0xff},
	{0x2806, 0x0f},
	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x10},
	{0x3004, 0x00},
	{0x3009, 0x2e},
	{0x3010, 0x21},
	{0x3015, 0xf0},
	{0x3016, 0xf0},
	{0x3017, 0xf0},
	{0x3018, 0xf0},
	{0x301a, 0x78},
	{0x301b, 0xb4},
	{0x301f, 0xe9},
	{0x3024, 0x80},
	{0x3039, 0x00},
	{0x3044, 0x70},
	{0x3101, 0x32},
	{0x3182, 0x10},
	{0x3187, 0xff},
	{0x320a, 0x00},
	{0x320b, 0x00},
	{0x320c, 0x00},
	{0x320d, 0x00},
	{0x320e, 0x00},
	{0x320f, 0x00},
	{0x3211, 0x61},
	{0x3212, 0x00},
	{0x3215, 0xcc},
	{0x3218, 0x06},
	{0x3219, 0x08},
	{0x3251, 0x00},
	{0x3252, 0xe4},
	{0x3253, 0x00},
	{0x3304, 0x11},
	{0x3305, 0x00},
	{0x3306, 0x01},
	{0x3307, 0x00},
	{0x3308, 0x02},
	{0x3309, 0x00},
	{0x330a, 0x02},
	{0x330b, 0x00},
	{0x330c, 0x02},
	{0x330d, 0x00},
	{0x330e, 0x02},
	{0x330f, 0x00},
	{0x3310, 0x02},
	{0x3311, 0x00},
	{0x3312, 0x02},
	{0x3313, 0x00},
	{0x3314, 0x02},
	{0x3315, 0x00},
	{0x3316, 0x02},
	{0x3317, 0x11},
	{0x3400, 0x0c},
	{0x3421, 0x00},
	{0x3422, 0x00},
	{0x3423, 0x00},
	{0x3424, 0x00},
	{0x3425, 0x00},
	{0x3426, 0x00},
	{0x3427, 0x00},
	{0x3428, 0x00},
	{0x3429, 0x40},
	{0x342a, 0x55},
	{0x342b, 0x05},
	{0x342c, 0x00},
	{0x342d, 0x00},
	{0x342e, 0x00},
	{0x3500, 0x00},
	{0x3501, 0x00},
	{0x3502, 0x08},
	{0x3503, 0xa8},
	{0x3504, 0x08},
	{0x3505, 0x00},
	{0x3506, 0x00},
	{0x3507, 0x00},
	{0x3508, 0x01},
	{0x3509, 0x00},
	{0x350a, 0x01},
	{0x350b, 0x00},
	{0x350c, 0x00},
	{0x3541, 0x00},
	{0x3542, 0x08},
	{0x3603, 0x65},
	{0x3604, 0x24},
	{0x3608, 0x08},
	{0x3610, 0x00},
	{0x3612, 0x00},
	{0x3619, 0x09},
	{0x361a, 0x27},
	{0x3620, 0x40},
	{0x3622, 0x15},
	{0x3623, 0x0e},
	{0x3624, 0x1f},
	{0x3625, 0x1f},
	{0x362a, 0x01},
	{0x362b, 0x00},
	{0x3633, 0x88},
	{0x3634, 0x86},
	{0x3636, 0x80},
	{0x3638, 0x3b},
	{0x363a, 0x00},
	{0x363b, 0x22},
	{0x363c, 0x07},
	{0x363d, 0x11},
	{0x363e, 0x21},
	{0x363f, 0x24},
	{0x3640, 0xd3},
	{0x3641, 0x00},
	{0x3650, 0xe4},
	{0x3651, 0x80},
	{0x3652, 0xff},
	{0x3653, 0x00},
	{0x3654, 0x05},
	{0x3655, 0xf8},
	{0x3656, 0x00},
	{0x3660, 0x00},
	{0x3664, 0x00},
	{0x366a, 0x80},
	{0x366b, 0x00},
	{0x3670, 0x00},
	{0x3674, 0x00},
	{0x367b, 0x50},
	{0x3684, 0x6d},
	{0x3685, 0x6d},
	{0x3686, 0x6d},
	{0x3687, 0x6d},
	{0x368c, 0x07},
	{0x368d, 0x07},
	{0x368e, 0x07},
	{0x368f, 0x00},
	{0x3690, 0x04},
	{0x3691, 0x04},
	{0x3692, 0x04},
	{0x3693, 0x04},
	{0x3698, 0x00},
	{0x369e, 0x1f},
	{0x369f, 0x19},
	{0x36a0, 0x05},
	{0x36a2, 0x16},
	{0x36a3, 0x03},
	{0x36a4, 0x07},
	{0x36a5, 0x24},
	{0x36a6, 0x00},
	{0x36a7, 0x80},
	{0x36e3, 0x09},
	{0x3700, 0x07},
	{0x3701, 0x1b},
	{0x3702, 0x0a},
	{0x3703, 0x21},
	{0x3704, 0x19},
	{0x3705, 0x07},
	{0x3706, 0x36},
	{0x370a, 0x1c},
	{0x370b, 0x02},
	{0x370c, 0x00},
	{0x370d, 0x6e},
	{0x370f, 0x80},
	{0x3710, 0x10},
	{0x3712, 0x09},
	{0x3714, 0x42},
	{0x3715, 0x00},
	{0x3716, 0x02},
	{0x3717, 0xa2},
	{0x3718, 0x41},
	{0x371a, 0x80},
	{0x371b, 0x0a},
	{0x371c, 0x0a},
	{0x371d, 0x08},
	{0x371e, 0x01},
	{0x371f, 0x20},
	{0x3720, 0x0e},
	{0x3721, 0x22},
	{0x3722, 0x0c},
	{0x3727, 0x84},
	{0x3728, 0x03},
	{0x3729, 0x64},
	{0x372a, 0x0c},
	{0x372b, 0x14},
	{0x372d, 0x50},
	{0x372e, 0x14},
	{0x3731, 0x11},
	{0x3732, 0x24},
	{0x3733, 0x00},
	{0x3734, 0x00},
	{0x3735, 0x12},
	{0x3736, 0x00},
	{0x373b, 0x0b},
	{0x373c, 0x14},
	{0x373f, 0x3e},
	{0x3740, 0x12},
	{0x3741, 0x12},
	{0x3753, 0x80},
	{0x3754, 0x01},
	{0x3756, 0x11},
	{0x375c, 0x0f},
	{0x375d, 0x35},
	{0x375e, 0x0f},
	{0x375f, 0x37},
	{0x3760, 0x0f},
	{0x3761, 0x27},
	{0x3762, 0x3f},
	{0x3763, 0x5d},
	{0x3764, 0x01},
	{0x3765, 0x17},
	{0x3766, 0x02},
	{0x3768, 0x52},
	{0x376a, 0x30},
	{0x376b, 0x02},
	{0x376c, 0x08},
	{0x376d, 0x2a},
	{0x376e, 0x00},
	{0x376f, 0x18},
	{0x3770, 0x2c},
	{0x3771, 0x0c},
	{0x3772, 0x71},
	{0x3776, 0xc0},
	{0x3778, 0x00},
	{0x3779, 0x80},
	{0x377a, 0x00},
	{0x377d, 0x14},
	{0x377e, 0x0c},
	{0x379c, 0x25},
	{0x379f, 0x00},
	{0x37a3, 0x40},
	{0x37a4, 0x03},
	{0x37a5, 0x10},
	{0x37a6, 0x02},
	{0x37a7, 0x0e},
	{0x37a9, 0x00},
	{0x37aa, 0x08},
	{0x37ab, 0x08},
	{0x37ac, 0x36},
	{0x37ad, 0x40},
	{0x37b0, 0x48},
	{0x37d0, 0x00},
	{0x37d1, 0x0b},
	{0x37d2, 0x0c},
	{0x37d3, 0x10},
	{0x37d4, 0x10},
	{0x37d5, 0x10},
	{0x37d8, 0x0e},
	{0x37d9, 0x0e},
	{0x37da, 0x3c},
	{0x37db, 0x52},
	{0x37dc, 0x50},
	{0x37dd, 0x00},
	{0x37de, 0x55},
	{0x37df, 0x7d},
	{0x37e5, 0x88},
	{0x37e7, 0x68},
	{0x37e8, 0x07},
	{0x37f0, 0x00},
	{0x37f1, 0x0e},
	{0x37f2, 0x35},
	{0x37f3, 0x14},
	{0x37f4, 0x0c},
	{0x37f5, 0x14},
	{0x37f6, 0x0c},
	{0x37f7, 0x35},
	{0x37f8, 0x35},
	{0x37f9, 0x37},
	{0x37fa, 0x37},
	{0x37fb, 0x37},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x0a},
	{0x3805, 0x2f},
	{0x3806, 0x07},
	{0x3807, 0xa7},
	{0x3808, 0x0a},
	{0x3809, 0x20},
	{0x380a, 0x07},
	{0x380b, 0x98},
	{0x380c, 0x01},
	{0x380d, 0x78},
	{0x380e, 0x08},
	{0x380f, 0x50},
	{0x3810, 0x00},
	{0x3811, 0x05},
	{0x3812, 0x00},
	{0x3813, 0x08},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3820, 0x40},
	{0x3821, 0x04},
	{0x3822, 0x10},
	{0x3823, 0x00},
	{0x3826, 0x00},
	{0x3827, 0x00},
	{0x382b, 0x03},
	{0x382c, 0x0c},
	{0x382d, 0x15},
	{0x382e, 0x01},
	{0x3830, 0x00},
	{0x3838, 0x00},
	{0x383b, 0x00},
	{0x3840, 0x00},
	{0x384a, 0xa2},
	{0x3858, 0x00},
	{0x3859, 0x00},
	{0x3860, 0x00},
	{0x3861, 0x00},
	{0x3866, 0x10},
	{0x3867, 0x07},
	{0x3868, 0x01},
	{0x3869, 0x01},
	{0x386a, 0x01},
	{0x386b, 0x01},
	{0x386c, 0x46},
	{0x386d, 0x08},
	{0x386e, 0x7b},
	{0x3871, 0x01},
	{0x3872, 0x01},
	{0x3873, 0x01},
	{0x3874, 0x01},
	{0x3880, 0x00},
	{0x3881, 0x00},
	{0x3882, 0x00},
	{0x3883, 0x00},
	{0x3884, 0x00},
	{0x3885, 0x00},
	{0x3886, 0x00},
	{0x3887, 0x00},
	{0x3888, 0x00},
	{0x3889, 0x00},
	{0x3900, 0x13},
	{0x3901, 0x19},
	{0x3902, 0x05},
	{0x3903, 0x00},
	{0x3904, 0x00},
	{0x3908, 0x00},
	{0x3909, 0x18},
	{0x390a, 0x00},
	{0x390b, 0x11},
	{0x390c, 0x15},
	{0x390d, 0x84},
	{0x390f, 0x88},
	{0x3910, 0x00},
	{0x3911, 0x00},
	{0x3912, 0x03},
	{0x3913, 0x62},
	{0x3914, 0x00},
	{0x3915, 0x06},
	{0x3916, 0x0c},
	{0x3917, 0x81},
	{0x3918, 0xc8},
	{0x3919, 0x94},
	{0x391a, 0x17},
	{0x391b, 0x05},
	{0x391c, 0x81},
	{0x391d, 0x05},
	{0x391e, 0x81},
	{0x391f, 0x05},
	{0x3920, 0x81},
	{0x3921, 0x14},
	{0x3922, 0x0b},
	{0x3929, 0x00},
	{0x392a, 0x00},
	{0x392b, 0xc8},
	{0x392c, 0x81},
	{0x392f, 0x00},
	{0x3930, 0x00},
	{0x3931, 0x00},
	{0x3932, 0x00},
	{0x3933, 0x00},
	{0x3934, 0x1b},
	{0x3935, 0xc0},
	{0x3936, 0x1c},
	{0x3937, 0x21},
	{0x3938, 0x0d},
	{0x3939, 0x92},
	{0x393a, 0x85},
	{0x393b, 0x8a},
	{0x393c, 0x06},
	{0x393d, 0x8b},
	{0x393e, 0x0f},
	{0x393f, 0x14},
	{0x3940, 0x0f},
	{0x3941, 0x14},
	{0x3945, 0xc0},
	{0x3946, 0x05},
	{0x3947, 0xc0},
	{0x3948, 0x01},
	{0x3949, 0x00},
	{0x394a, 0x00},
	{0x394b, 0x0b},
	{0x394c, 0x0c},
	{0x394d, 0x0b},
	{0x394e, 0x09},
	{0x3951, 0xc7},
	{0x3952, 0x0f},
	{0x3953, 0x0f},
	{0x3954, 0x0f},
	{0x3955, 0x00},
	{0x3956, 0x27},
	{0x3957, 0x27},
	{0x3958, 0x27},
	{0x3959, 0x01},
	{0x395a, 0x02},
	{0x395b, 0x14},
	{0x395c, 0x36},
	{0x395e, 0xc0},
	{0x3964, 0x55},
	{0x3965, 0x55},
	{0x3966, 0x88},
	{0x3967, 0x88},
	{0x3968, 0x66},
	{0x3969, 0x66},
	{0x396d, 0x80},
	{0x396e, 0xff},
	{0x396f, 0x10},
	{0x3970, 0x80},
	{0x3971, 0x80},
	{0x3972, 0x00},
	{0x397a, 0x55},
	{0x397b, 0x10},
	{0x397c, 0x10},
	{0x397d, 0x10},
	{0x397e, 0x10},
	{0x3980, 0xfc},
	{0x3981, 0xfc},
	{0x3982, 0x66},
	{0x3983, 0xfc},
	{0x3984, 0xfc},
	{0x3985, 0x66},
	{0x3986, 0x00},
	{0x3987, 0x00},
	{0x3988, 0x00},
	{0x3989, 0x00},
	{0x398a, 0x00},
	{0x398b, 0x00},
	{0x398c, 0x00},
	{0x398d, 0x00},
	{0x398e, 0x00},
	{0x398f, 0x00},
	{0x3990, 0x00},
	{0x3991, 0x00},
	{0x3992, 0x00},
	{0x3993, 0x00},
	{0x3994, 0x00},
	{0x3995, 0x00},
	{0x3996, 0x00},
	{0x3997, 0x0f},
	{0x3998, 0x0c},
	{0x3999, 0x0c},
	{0x399a, 0x0c},
	{0x399b, 0xf0},
	{0x399c, 0x14},
	{0x399d, 0x0d},
	{0x399e, 0x00},
	{0x399f, 0x0c},
	{0x39a0, 0x0c},
	{0x39a1, 0x0c},
	{0x39a2, 0x00},
	{0x39a3, 0x0f},
	{0x39a4, 0x0c},
	{0x39a5, 0x0c},
	{0x39a6, 0x0c},
	{0x39a7, 0x0c},
	{0x39a8, 0x0f},
	{0x39a9, 0xff},
	{0x39aa, 0xbf},
	{0x39ab, 0x3f},
	{0x39ac, 0x7e},
	{0x39ad, 0xff},
	{0x39ae, 0x00},
	{0x39af, 0x00},
	{0x39b0, 0x00},
	{0x39b1, 0x00},
	{0x39b2, 0x00},
	{0x39b3, 0x00},
	{0x39b4, 0x00},
	{0x39b5, 0x00},
	{0x39b6, 0x00},
	{0x39b7, 0x00},
	{0x39b8, 0x00},
	{0x39b9, 0x00},
	{0x39ba, 0x00},
	{0x39bb, 0x00},
	{0x39bc, 0x00},
	{0x39c2, 0x00},
	{0x39c3, 0x00},
	{0x39c4, 0x00},
	{0x39c5, 0x00},
	{0x39c7, 0x00},
	{0x39c8, 0x00},
	{0x39c9, 0x00},
	{0x39ca, 0x01},
	{0x39cb, 0x00},
	{0x39cc, 0x85},
	{0x39cd, 0x09},
	{0x39cf, 0x04},
	{0x39d0, 0x85},
	{0x39d1, 0x09},
	{0x39d2, 0x04},
	{0x39d4, 0x02},
	{0x39d5, 0x0e},
	{0x39db, 0x00},
	{0x39dc, 0x01},
	{0x39dd, 0x0c},
	{0x39e5, 0xff},
	{0x39e6, 0xff},
	{0x39fa, 0x38},
	{0x39fb, 0x07},
	{0x39ff, 0x00},
	{0x3a05, 0x00},
	{0x3a06, 0x07},
	{0x3a07, 0x0d},
	{0x3a08, 0x08},
	{0x3a09, 0xb2},
	{0x3a0a, 0x0a},
	{0x3a0b, 0x3c},
	{0x3a0c, 0x0b},
	{0x3a0d, 0xe1},
	{0x3a0e, 0x03},
	{0x3a0f, 0x85},
	{0x3a10, 0x0b},
	{0x3a11, 0xff},
	{0x3a12, 0x00},
	{0x3a13, 0x01},
	{0x3a14, 0x0c},
	{0x3a15, 0x04},
	{0x3a17, 0x09},
	{0x3a18, 0x20},
	{0x3a19, 0x09},
	{0x3a1a, 0x9d},
	{0x3a1b, 0x09},
	{0x3a1e, 0x34},
	{0x3a1f, 0x09},
	{0x3a20, 0x89},
	{0x3a21, 0x09},
	{0x3a48, 0xbe},
	{0x3a52, 0x00},
	{0x3a53, 0x01},
	{0x3a54, 0x0c},
	{0x3a55, 0x04},
	{0x3a58, 0x0c},
	{0x3a59, 0x04},
	{0x3a5a, 0x01},
	{0x3a5b, 0x00},
	{0x3a5c, 0x01},
	{0x3a5d, 0xe8},
	{0x3a62, 0x03},
	{0x3a63, 0x86},
	{0x3a64, 0x0b},
	{0x3a65, 0xbe},
	{0x3a6a, 0xdc},
	{0x3a6b, 0x0b},
	{0x3a6c, 0x1a},
	{0x3a6d, 0x06},
	{0x3a6e, 0x01},
	{0x3a6f, 0x04},
	{0x3a70, 0xdc},
	{0x3a71, 0x0b},
	{0x3a83, 0x10},
	{0x3a84, 0x00},
	{0x3a85, 0x08},
	{0x3a87, 0x00},
	{0x3a88, 0x6b},
	{0x3a89, 0x01},
	{0x3a8a, 0x53},
	{0x3a8f, 0x00},
	{0x3a90, 0x00},
	{0x3a91, 0x00},
	{0x3a92, 0x00},
	{0x3a93, 0x60},
	{0x3a94, 0xea},
	{0x3a98, 0x00},
	{0x3a99, 0x31},
	{0x3a9a, 0x01},
	{0x3a9b, 0x04},
	{0x3a9c, 0xdc},
	{0x3a9d, 0x0b},
	{0x3aa4, 0x0f},
	{0x3aad, 0x00},
	{0x3aae, 0x3e},
	{0x3aaf, 0x02},
	{0x3ab0, 0x77},
	{0x3ab2, 0x00},
	{0x3ab3, 0x08},
	{0x3ab6, 0x0b},
	{0x3ab7, 0xff},
	{0x3aba, 0x0b},
	{0x3abb, 0xfa},
	{0x3abd, 0x05},
	{0x3abe, 0x09},
	{0x3abf, 0x1e},
	{0x3ac0, 0x00},
	{0x3ac1, 0x63},
	{0x3ac2, 0x01},
	{0x3ac3, 0x55},
	{0x3ac8, 0x00},
	{0x3ac9, 0x2a},
	{0x3aca, 0x01},
	{0x3acb, 0x36},
	{0x3acc, 0x00},
	{0x3acd, 0x6f},
	{0x3ad0, 0x00},
	{0x3ad1, 0x79},
	{0x3ad2, 0x02},
	{0x3ad3, 0x59},
	{0x3ad4, 0x06},
	{0x3ad5, 0x5a},
	{0x3ad6, 0x08},
	{0x3ad7, 0x3a},
	{0x3ad8, 0x00},
	{0x3ad9, 0x79},
	{0x3ada, 0x02},
	{0x3adb, 0x59},
	{0x3adc, 0x09},
	{0x3add, 0x89},
	{0x3ade, 0x0b},
	{0x3adf, 0x69},
	{0x3ae0, 0x03},
	{0x3ae1, 0xc1},
	{0x3ae2, 0x0b},
	{0x3ae3, 0xaf},
	{0x3ae4, 0x00},
	{0x3ae5, 0x3e},
	{0x3ae6, 0x02},
	{0x3ae7, 0x77},
	{0x3ae8, 0x00},
	{0x3aea, 0x0b},
	{0x3aeb, 0xbe},
	{0x3aee, 0x08},
	{0x3aef, 0x80},
	{0x3af0, 0x09},
	{0x3af1, 0x70},
	{0x3af2, 0x08},
	{0x3af3, 0x94},
	{0x3af4, 0x09},
	{0x3af5, 0x5c},
	{0x3af6, 0x03},
	{0x3af7, 0x85},
	{0x3af8, 0x08},
	{0x3af9, 0x80},
	{0x3afa, 0x0b},
	{0x3afb, 0xaf},
	{0x3afc, 0x01},
	{0x3afd, 0x5a},
	{0x3b1e, 0x00},
	{0x3b20, 0xa5},
	{0x3b21, 0x00},
	{0x3b22, 0x00},
	{0x3b23, 0x00},
	{0x3b24, 0x05},
	{0x3b25, 0x00},
	{0x3b26, 0x00},
	{0x3b27, 0x00},
	{0x3b28, 0x1a},
	{0x3b2f, 0x40},
	{0x3b40, 0x08},
	{0x3b41, 0x70},
	{0x3b42, 0x05},
	{0x3b43, 0xf0},
	{0x3b44, 0x01},
	{0x3b45, 0x54},
	{0x3b46, 0x01},
	{0x3b47, 0x54},
	{0x3b56, 0x08},
	{0x3b80, 0x00},
	{0x3b81, 0x00},
	{0x3b82, 0x64},
	{0x3b83, 0x00},
	{0x3b84, 0x00},
	{0x3b85, 0x64},
	{0x3b9d, 0x61},
	{0x3ba8, 0x38},
	{0x3c11, 0x33},
	{0x3c12, 0x3d},
	{0x3c13, 0x00},
	{0x3c14, 0xbe},
	{0x3c15, 0x0b},
	{0x3c16, 0xa8},
	{0x3c17, 0x03},
	{0x3c18, 0x9c},
	{0x3c19, 0x0b},
	{0x3c1a, 0x0f},
	{0x3c1b, 0x97},
	{0x3c1c, 0x00},
	{0x3c1d, 0x3c},
	{0x3c1e, 0x02},
	{0x3c1f, 0x78},
	{0x3c20, 0x06},
	{0x3c21, 0x80},
	{0x3c22, 0x08},
	{0x3c23, 0x0f},
	{0x3c24, 0x97},
	{0x3c25, 0x00},
	{0x3c26, 0x3c},
	{0x3c27, 0x02},
	{0x3c28, 0xa7},
	{0x3c29, 0x09},
	{0x3c2a, 0xaf},
	{0x3c2b, 0x0b},
	{0x3c2c, 0x38},
	{0x3c2d, 0xf9},
	{0x3c2e, 0x0b},
	{0x3c2f, 0xfd},
	{0x3c30, 0x05},
	{0x3c35, 0x8c},
	{0x3c3e, 0xc3},
	{0x3c43, 0xcb},
	{0x3c44, 0x00},
	{0x3c45, 0xff},
	{0x3c46, 0x0b},
	{0x3c48, 0x3b},
	{0x3c49, 0x40},
	{0x3c4a, 0x00},
	{0x3c4b, 0x5b},
	{0x3c4c, 0x02},
	{0x3c4d, 0x02},
	{0x3c4e, 0x00},
	{0x3c4f, 0x04},
	{0x3c50, 0x0c},
	{0x3c51, 0x00},
	{0x3c52, 0x3b},
	{0x3c53, 0x3a},
	{0x3c54, 0x07},
	{0x3c55, 0x9e},
	{0x3c56, 0x07},
	{0x3c57, 0x9e},
	{0x3c58, 0x07},
	{0x3c59, 0xe8},
	{0x3c5a, 0x03},
	{0x3c5b, 0x33},
	{0x3c5c, 0xa8},
	{0x3c5d, 0x07},
	{0x3c5e, 0xd0},
	{0x3c5f, 0x07},
	{0x3c60, 0x32},
	{0x3c61, 0x00},
	{0x3c62, 0xd0},
	{0x3c63, 0x07},
	{0x3c64, 0x80},
	{0x3c65, 0x80},
	{0x3c66, 0x3f},
	{0x3c67, 0x01},
	{0x3c68, 0x00},
	{0x3c69, 0xd0},
	{0x3c6a, 0x07},
	{0x3c6b, 0x01},
	{0x3c6c, 0x00},
	{0x3c6d, 0xcd},
	{0x3c6e, 0x07},
	{0x3c6f, 0xd1},
	{0x3c70, 0x07},
	{0x3c71, 0x01},
	{0x3c72, 0x00},
	{0x3c73, 0xc3},
	{0x3c74, 0x01},
	{0x3c75, 0x00},
	{0x3c76, 0xcd},
	{0x3c77, 0x07},
	{0x3c78, 0xea},
	{0x3c79, 0x03},
	{0x3c7a, 0xcd},
	{0x3c7b, 0x07},
	{0x3c7c, 0x08},
	{0x3c7d, 0x06},
	{0x3c7e, 0x03},
	{0x3c85, 0x3a},
	{0x3c86, 0x08},
	{0x3c87, 0x69},
	{0x3c88, 0x0b},
	{0x3c8f, 0xb2},
	{0x3c90, 0x08},
	{0x3c91, 0xe1},
	{0x3c92, 0x0b},
	{0x3c93, 0x06},
	{0x3c94, 0x03},
	{0x3c9b, 0x35},
	{0x3c9c, 0x08},
	{0x3c9d, 0x64},
	{0x3c9e, 0x0b},
	{0x3ca5, 0xb7},
	{0x3ca6, 0x08},
	{0x3ca7, 0xe6},
	{0x3ca8, 0x0b},
	{0x3ca9, 0x83},
	{0x3caa, 0x3c},
	{0x3cab, 0x01},
	{0x3cac, 0x00},
	{0x3cad, 0x9e},
	{0x3cae, 0x07},
	{0x3caf, 0x85},
	{0x3cb0, 0x03},
	{0x3cb1, 0xbc},
	{0x3cb2, 0x0b},
	{0x3cb7, 0x3c},
	{0x3cb8, 0x01},
	{0x3cb9, 0x00},
	{0x3cba, 0xbc},
	{0x3cbb, 0x07},
	{0x3cbc, 0xa3},
	{0x3cbd, 0x03},
	{0x3cbe, 0x9e},
	{0x3cbf, 0x0b},
	{0x3cc4, 0x66},
	{0x3cc5, 0xe6},
	{0x3cc6, 0x99},
	{0x3cc7, 0xe9},
	{0x3cc8, 0x33},
	{0x3cc9, 0x03},
	{0x3cca, 0x33},
	{0x3ccb, 0x03},
	{0x3cce, 0x66},
	{0x3ccf, 0x66},
	{0x3cd0, 0x00},
	{0x3cd1, 0x04},
	{0x3cd2, 0xf4},
	{0x3cd3, 0xb7},
	{0x3cd4, 0x03},
	{0x3cd5, 0x10},
	{0x3cd6, 0x06},
	{0x3cd7, 0x30},
	{0x3cd8, 0x08},
	{0x3cd9, 0x5f},
	{0x3cda, 0x0b},
	{0x3cdd, 0x88},
	{0x3cde, 0x88},
	{0x3cdf, 0x08},
	{0x3ce0, 0x00},
	{0x3ce1, 0x00},
	{0x3ce3, 0x00},
	{0x3ce4, 0x00},
	{0x3ce5, 0x00},
	{0x3ce6, 0x00},
	{0x3ce7, 0x00},
	{0x3ce8, 0x00},
	{0x3ce9, 0x00},
	{0x3cea, 0x00},
	{0x3ceb, 0x00},
	{0x3cec, 0x00},
	{0x3ced, 0x00},
	{0x3cee, 0x00},
	{0x3cef, 0x85},
	{0x3cf0, 0x03},
	{0x3cf1, 0xaf},
	{0x3cf2, 0x0b},
	{0x3cf3, 0x03},
	{0x3cf4, 0x2c},
	{0x3cf5, 0x00},
	{0x3cf6, 0x42},
	{0x3cf7, 0x00},
	{0x3cf8, 0x03},
	{0x3cf9, 0x2c},
	{0x3cfa, 0x00},
	{0x3cfb, 0x42},
	{0x3cfc, 0x00},
	{0x3cfd, 0x03},
	{0x3cfe, 0x01},
	{0x3d81, 0x00},
	{0x3e94, 0x0f},
	{0x3e95, 0x5f},
	{0x3e96, 0x02},
	{0x3e97, 0x3c},
	{0x3e98, 0x00},
	{0x3e9f, 0x00},
	{0x3f00, 0x00},
	{0x3f05, 0x03},
	{0x3f07, 0x01},
	{0x3f08, 0x55},
	{0x3f09, 0x25},
	{0x3f0a, 0x35},
	{0x3f0b, 0x20},
	{0x3f11, 0x05},
	{0x3f12, 0x05},
	{0x3f40, 0x00},
	{0x3f41, 0x03},
	{0x3f43, 0x10},
	{0x3f44, 0x02},
	{0x3f45, 0xe6},
	{0x4000, 0xf9},
	{0x4001, 0x2b},
	{0x4008, 0x04},
	{0x4009, 0x1b},
	{0x400a, 0x03},
	{0x400e, 0x10},
	{0x4010, 0x04},
	{0x4011, 0xf7},
	{0x4032, 0x3e},
	{0x4033, 0x02},
	{0x4050, 0x02},
	{0x4051, 0x0d},
	{0x40b0, 0x0f},
	{0x40f9, 0x00},
	{0x4200, 0x00},
	{0x4204, 0x00},
	{0x4205, 0x00},
	{0x4206, 0x00},
	{0x4207, 0x00},
	{0x4208, 0x00},
	{0x4244, 0x00},
	{0x4300, 0x00},
	{0x4301, 0xff},
	{0x4302, 0xf0},
	{0x4303, 0x00},
	{0x4304, 0xff},
	{0x4305, 0xf0},
	{0x4306, 0x00},
	{0x4307, 0x06},
	{0x4308, 0x00},
	{0x430a, 0x90},
	{0x430b, 0x11},
	{0x4310, 0x00},
	{0x4316, 0x00},
	{0x431c, 0x00},
	{0x431e, 0x00},
	{0x431f, 0x0a},
	{0x4320, 0x20},
	{0x4410, 0x08},
	{0x4433, 0x08},
	{0x4434, 0xf8},
	{0x4508, 0x80},
	{0x4509, 0x10},
	{0x450b, 0x83},
	{0x4511, 0x00},
	{0x4580, 0x08},
	{0x4587, 0x00},
	{0x458c, 0x00},
	{0x4640, 0x00},
	{0x4641, 0xc1},
	{0x4642, 0x00},
	{0x4643, 0x00},
	{0x4649, 0x00},
	{0x4681, 0x04},
	{0x4682, 0x10},
	{0x4683, 0xa0},
	{0x4698, 0x07},
	{0x4699, 0xf0},
	{0x4700, 0xe0},
	{0x4710, 0x00},
	{0x4718, 0x04},
	{0x4802, 0x00},
	{0x481b, 0x3c},
	{0x4837, 0x10},
	{0x4860, 0x00},
	{0x4881, 0x00},
	{0x4882, 0x00},
	{0x4883, 0x00},
	{0x4884, 0x09},
	{0x4885, 0x80},
	{0x4886, 0x00},
	{0x4888, 0x10},
	{0x488b, 0x00},
	{0x488c, 0x10},
	{0x4980, 0x03},
	{0x4981, 0x06},
	{0x4984, 0x00},
	{0x4985, 0x00},
	{0x4a14, 0x04},
	{0x4b01, 0x44},
	{0x4b03, 0x80},
	{0x4d06, 0xc8},
	{0x4d09, 0xdf},
	{0x4d12, 0x80},
	{0x4d15, 0x7d},
	{0x4d34, 0x7d},
	{0x4d3c, 0x7d},
	{0x4d5a, 0x14},
	{0x4e03, 0x06},
	{0x4e04, 0xb9},
	{0x4e05, 0x08},
	{0x4e06, 0x36},
	{0x4e07, 0x04},
	{0x4e08, 0x52},
	{0x4e09, 0x05},
	{0x4e0a, 0x47},
	{0x4e0b, 0x02},
	{0x4e0c, 0xe2},
	{0x4e0d, 0x03},
	{0x4e0e, 0x85},
	{0x4e18, 0xf3},
	{0x4e19, 0x0f},
	{0x4e1b, 0x08},
	{0x4e1c, 0x04},
	{0x4f00, 0x7f},
	{0x4f01, 0xff},
	{0x4f03, 0x00},
	{0x4f04, 0x18},
	{0x4f05, 0x13},
	{0x5000, 0x6e},
	{0x5001, 0x00},
	{0x500a, 0x00},
	{0x5080, 0x00},
	{0x5081, 0x00},
	{0x5082, 0x00},
	{0x5083, 0x00},
	{0x5100, 0x00},
	{0x5103, 0x00},
	{0x5180, 0x70},
	{0x5181, 0x70},
	{0x5182, 0x73},
	{0x5183, 0xff},
	{0x5240, 0x03},
	{0x5249, 0x06},
	{0x524a, 0x66},
	{0x524b, 0x66},
	{0x524c, 0x66},
	{0x524e, 0x66},
	{0x524f, 0x66},
	{0x5250, 0x10},
	{0x5281, 0x18},
	{0x5282, 0x08},
	{0x5283, 0x08},
	{0x5284, 0x18},
	{0x5285, 0x18},
	{0x5286, 0x08},
	{0x5287, 0x08},
	{0x5288, 0x18},
	{0x5289, 0x2d},
	{0x6000, 0x40},
	{0x6001, 0x40},
	{0x6002, 0x00},
	{0x6003, 0x00},
	{0x6004, 0x00},
	{0x6005, 0x00},
	{0x6006, 0x00},
	{0x6007, 0x00},
	{0x6008, 0x00},
	{0x6009, 0x00},
	{0x600a, 0x00},
	{0x600b, 0x00},
	{0x600c, 0x02},
	{0x600d, 0x00},
	{0x600e, 0x04},
	{0x600f, 0x00},
	{0x6010, 0x06},
	{0x6011, 0x00},
	{0x6012, 0x00},
	{0x6013, 0x00},
	{0x6014, 0x02},
	{0x6015, 0x00},
	{0x6016, 0x04},
	{0x6017, 0x00},
	{0x6018, 0x06},
	{0x6019, 0x00},
	{0x601a, 0x01},
	{0x601b, 0x00},
	{0x601c, 0x01},
	{0x601d, 0x00},
	{0x601e, 0x01},
	{0x601f, 0x00},
	{0x6020, 0x01},
	{0x6021, 0x00},
	{0x6022, 0x01},
	{0x6023, 0x00},
	{0x6024, 0x01},
	{0x6025, 0x00},
	{0x6026, 0x01},
	{0x6027, 0x00},
	{0x6028, 0x01},
	{0x6029, 0x00},
	{0x3501, 0x08},
	{0x3502, 0x32},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x0A},
	{0x3805, 0x2F},
	{0x3806, 0x07},
	{0x3807, 0xA7},
	{0x3810, 0x00},
	{0x3811, 0x0F},
	{0x3812, 0x00},
	{0x3813, 0xFA},
	{0x3808, 0x0A},
	{0x3809, 0x00},
	{0x380A, 0x05},
	{0x380B, 0xA0},
	{0x380c, 0x02},
	{0x380d, 0xF0},
	{0x3503, 0xa8},
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00}, /* END MARKER */
};

/*
 * the order of the og05b10_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting og05b10_win_sizes[] = {
	{
		.width          = 2560,
		.height         = 1440,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = og05b10_init_regs_2560_1440_30fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &og05b10_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list og05b10_stream_on_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list og05b10_stream_off_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int og05b10_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int og05b10_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int og05b10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = og05b10_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int og05b10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = og05b10_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int og05b10_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int og05b10_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int og05b10_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = og05b10_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int og05b10_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = og05b10_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int og05b10_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

error:
	return ret;
}

static int og05b10_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int og05b10_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &og05b10_win_sizes[0];
		og05b10_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		og05b10_attr.max_dgain = 0;
		og05b10_attr.max_again = 259138;
		og05b10_attr.min_integration_time = 6;
		og05b10_attr.max_integration_time = 2128 - 30;
		og05b10_attr.total_width = 752;
		og05b10_attr.total_height = 2128;
		og05b10_attr.integration_time_apply_delay = 2;
		og05b10_attr.again_apply_delay = 2;
		og05b10_attr.dgain_apply_delay = 0;
		og05b10_attr.integration_time_limit = og05b10_attr.max_integration_time;
		og05b10_attr.max_integration_time_native = og05b10_attr.max_integration_time;
		og05b10_attr.min_integration_time_native = og05b10_attr.min_integration_time;
		og05b10_attr.expo_fs = 1;
#ifdef SENSOR_WDR_2_FRAME
		og05b10_attr.max_again_short = xxxx;
		og05b10_attr.min_integration_time_short = xx;
		og05b10_attr.max_integration_time_short = xx;
		og05b10_attr.wdr_cache = wdr_line * og05b10_attr.total_width;
#endif /* SENSOR_WDR_2_FRAME */
		memcpy((void *)(&(og05b10_attr.mipi)), (void *)(&og05b10_30fps_mipi_linear), sizeof(og05b10_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
	}

	return ret;
}

static int og05b10_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	og05b10_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
	case TISP_SENSOR_VI_MIPI_CSI1:
		og05b10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		og05b10_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_DVP:
		og05b10_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = og05b10_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}

	og05b10_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int og05b10_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = og05b10_read(sd, 0x300a, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = og05b10_read(sd, 0x300b, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_M)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	ret = og05b10_read(sd, 0x300c, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 16) | v;

	return 0;
}

static int og05b10_g_chip_ident(struct tx_isp_subdev *sd,
								struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	og05b10_attr_check(sd);
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "og05b10_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "og05b10_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = og05b10_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an og05b10 chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}

	ISP_WARNING("===================================================\n");
	ISP_WARNING("OG05B10 version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", og05b10_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "og05b10", sizeof("og05b10"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int og05b10_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int og05b10_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (!init->enable) {
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		return ISP_SUCCESS;
	} else {
		ret = og05b10_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int og05b10_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = og05b10_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int og05b10_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	og05b10_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int og05b10_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
			ret = og05b10_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = og05b10_write_array(sd, og05b10_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("og05b10 stream on\n");
		}

	} else {
		ret = og05b10_write_array(sd, og05b10_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("og05b10 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int og05b10_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret += og05b10_write(sd, 0x3502, (unsigned char)(it & 0xff));
	ret += og05b10_write(sd, 0x3501, (unsigned char)(((it >> 8) & 0xff)));

	ret += og05b10_write(sd, 0x3509, (unsigned char)((again & 0x0f) << 4));
	ret += og05b10_write(sd, 0x3508, (unsigned char)(((again >> 4) & 0x0f)));

	return ret;
}
#else
static int og05b10_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int og05b10_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int og05b10_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int og05b10_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int og05b10_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = og05b10_attr_set(sd, wsize);
	}

	return ret;
}

static int og05b10_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 752 * 2128 * 30;  /**< HTS * VTS * FPS */
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
	ret += og05b10_read(sd, 0x380c, &val);
	hts = val << 8;
	val = 0;
	ret += og05b10_read(sd, 0x380d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: og05b10 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	og05b10_write(sd, 0x380f, (unsigned char) (vts & 0xff));
	og05b10_write(sd, 0x380e, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: og05b10_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 30;
	sensor->video.attr->integration_time_limit = vts - 30;
	sensor->video.attr->max_integration_time_native = vts - 30;
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
static int og05b10_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = ISP_SUCCESS;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	switch(enable) {
	case 0:
		og05b10_write(sd, 0x3820, 0x40);
		og05b10_write(sd, 0x3821, 0x04);
		sensor->video.mbus.code = TISP_VI_FMT_SBGGR10_1X10;
		break;
	case 1:
		og05b10_write(sd, 0x3820, 0x40);
		og05b10_write(sd, 0x3821, 0x00);
		sensor->video.mbus.code = TISP_VI_FMT_SGBRG10_1X10;
		break;
	case 2:
		og05b10_write(sd, 0x3820, 0x44);
		og05b10_write(sd, 0x3821, 0x04);
		sensor->video.mbus.code = TISP_VI_FMT_SGRBG10_1X10;
		break;
	case 3:
		og05b10_write(sd, 0x3820, 0x44);
		og05b10_write(sd, 0x3821, 0x00);
		sensor->video.mbus.code = TISP_VI_FMT_SRGGB10_1X10;
		break;
	}
	sensor->video.mbus_change = 1;
	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}
#endif /* SENSOR_MIR_FLIP */

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int og05b10_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int og05b10_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int og05b10_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int og05b10_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = ISP_SUCCESS;

	ret = og05b10_write_array(sd, og05b10_stream_off_mipi);
	if (wdr_en == 1) {
		og05b10_setting_select(sd, 1);
		og05b10_attr_set(sd, wsize);
	} else if (wdr_en == 0) {
		og05b10_setting_select(sd, 0);
		og05b10_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int og05b10_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = og05b10_write_array(sd, wsize->regs);
	ret = og05b10_write_array(sd, og05b10_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int og05b10_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = og05b10_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = og05b10_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = og05b10_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = og05b10_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = og05b10_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = og05b10_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = og05b10_write_array(sd, og05b10_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = og05b10_write_array(sd, og05b10_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = og05b10_set_fps(sd, sensor_val->value);
		break;
#ifdef SENSOR_MIR_FLIP
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = og05b10_set_vflip(sd, sensor_val->value);
		break;
#endif /* SENSOR_MIR_FLIP */
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = og05b10_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = og05b10_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = og05b10_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = og05b10_set_wdr(sd, init->enable);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = og05b10_set_wdr_stop(sd, init->enable);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return 0;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops og05b10_core_ops = {
	.g_chip_ident = og05b10_g_chip_ident,
	.reset = og05b10_reset,
	.init = og05b10_init,
	.g_register = og05b10_g_register,
	.s_register = og05b10_s_register,
};

static struct tx_isp_subdev_video_ops og05b10_video_ops = {
	.s_stream = og05b10_s_stream,
};

static struct tx_isp_subdev_sensor_ops og05b10_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = og05b10_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops og05b10_ops = {
	.core = &og05b10_core_ops,
	.video = &og05b10_video_ops,
	.sensor = &og05b10_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
	.name = "og05b10",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int og05b10_probe(struct i2c_client *client,
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

	sensor->video.attr = &og05b10_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &og05b10_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->og05b10\n");

	return 0;
}

static int og05b10_remove(struct i2c_client *client)
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

static const struct i2c_device_id og05b10_id[] = {
	{"og05b10", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, og05b10_id
				   );

static struct i2c_driver og05b10_driver = {
	.driver = {
		.owner  = THIS_MODULE,
		.name   = "og05b10",
	},
	.probe          = og05b10_probe,
	.remove         = og05b10_remove,
	.id_table       = og05b10_id,
};

static __init int init_og05b10(void) {
	return private_i2c_add_driver(&og05b10_driver);
}

static __exit void exit_og05b10(void) {
	private_i2c_del_driver(&og05b10_driver);
}

module_init(init_og05b10);
module_exit(exit_og05b10);

MODULE_DESCRIPTION("A low-level driver for Sony og05b10 sensor");
MODULE_LICENSE("GPL");
