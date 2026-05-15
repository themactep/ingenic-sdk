/*
 * mis40c1.c
 *
 * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @Settings:
 * sboot        resolution      fps       interface     mode    raw     max_fps     dvdd
 *  0           2560*1440       25        mipi_2lane    linear   10       25        1.2V
 * @I2C addr:0x31,0x30
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
#define SENSOR_CHIP_ID_L    (0x04)
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

struct tx_isp_sensor_attribute mis40c1_attr;

/*
 * The part of driver maybe modify about different sensor and different board.
 */
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut mis40c1_again_lut[] = {
	{0x0, 0},
	{0x10, 1465},
	{0x20, 2998},
	{0x30, 4506},
	{0x40, 6077},
	{0x50, 7623},
	{0x60, 9228},
	{0x70, 10888},
	{0x80, 12601},
	{0x90, 14283},
	{0xa0, 16014},
	{0xb0, 17789},
	{0xc0, 19607},
	{0xd0, 21465},
	{0xe0, 23287},
	{0xf0, 25216},
	{0x100, 27175},
	{0x108, 28140},
	{0x110, 29164},
	{0x118, 30175},
	{0x120, 31177},
	{0x128, 32233},
	{0x130, 33277},
	{0x138, 34311},
	{0x140, 35396},
	{0x148, 36470},
	{0x150, 37593},
	{0x158, 38703},
	{0x160, 39801},
	{0x168, 40945},
	{0x170, 42076},
	{0x178, 43252},
	{0x180, 44413},
	{0x188, 45618},
	{0x190, 46808},
	{0x198, 48037},
	{0x1a0, 49252},
	{0x1a8, 50505},
	{0x1b0, 51794},
	{0x1b8, 53068},
	{0x1c0, 54375},
	{0x1c8, 55716},
	{0x1d0, 57039},
	{0x1d8, 58392},
	{0x1e0, 59776},
	{0x1e8, 61189},
	{0x1f0, 62581},
	{0x1f8, 64046},
	{0x200, 65536},
	{0x204, 66271},
	{0x208, 67001},
	{0x20c, 67769},
	{0x210, 68534},
	{0x214, 69290},
	{0x218, 70042},
	{0x21c, 70831},
	{0x220, 71613},
	{0x224, 72390},
	{0x228, 73201},
	{0x22c, 74007},
	{0x230, 74805},
	{0x234, 75639},
	{0x238, 76466},
	{0x23c, 77284},
	{0x240, 78137},
	{0x244, 78982},
	{0x248, 79858},
	{0x24c, 80688},
	{0x250, 81588},
	{0x254, 82441},
	{0x258, 83364},
	{0x25c, 84239},
	{0x260, 85143},
	{0x264, 86076},
	{0x268, 87001},
	{0x26c, 87916},
	{0x270, 88859},
	{0x274, 89792},
	{0x278, 90752},
	{0x27c, 91737},
	{0x280, 92711},
	{0x284, 93710},
	{0x288, 94700},
	{0x28c, 95711},
	{0x290, 96745},
	{0x294, 97769},
	{0x298, 98813},
	{0x29c, 99879},
	{0x2a0, 100932},
	{0x2a4, 102037},
	{0x2a8, 103129},
	{0x2ac, 104239},
	{0x2b0, 105337},
	{0x2b4, 106481},
	{0x2b8, 107612},
	{0x2bc, 108788},
	{0x2c0, 109949},
	{0x2c4, 111154},
	{0x2c8, 112344},
	{0x2cc, 113573},
	{0x2d0, 114815},
	{0x2d4, 116067},
	{0x2d8, 117330},
	{0x2dc, 118630},
	{0x2e0, 119911},
	{0x2e4, 121252},
	{0x2e8, 122575},
	{0x2ec, 123954},
	{0x2f0, 125337},
	{0x2f4, 126725},
	{0x2f8, 128141},
	{0x2fc, 129582},
	{0x300, 131072},
	{0x302, 131807},
	{0x304, 132559},
	{0x306, 133305},
	{0x308, 134070},
	{0x30a, 134826},
	{0x30c, 135599},
	{0x30e, 136367},
	{0x310, 137171},
	{0x312, 137947},
	{0x314, 138760},
	{0x316, 139564},
	{0x318, 140363},
	{0x31a, 141196},
	{0x31c, 142022},
	{0x31e, 142840},
	{0x320, 143693},
	{0x322, 144537},
	{0x324, 145394},
	{0x326, 146243},
	{0x328, 147124},
	{0x32a, 147996},
	{0x32c, 148900},
	{0x32e, 149793},
	{0x330, 150698},
	{0x332, 151612},
	{0x334, 152537},
	{0x336, 153452},
	{0x338, 154395},
	{0x33a, 155346},
	{0x33c, 156306},
	{0x33e, 157290},
	{0x340, 158265},
	{0x342, 159246},
	{0x344, 160252},
	{0x346, 161264},
	{0x348, 162281},
	{0x34a, 163321},
	{0x34c, 164366},
	{0x34e, 165415},
	{0x350, 166484},
	{0x352, 167573},
	{0x354, 168665},
	{0x356, 169775},
	{0x358, 170888},
	{0x35a, 172017},
	{0x35c, 173162},
	{0x35e, 174324},
	{0x360, 175500},
	{0x362, 176690},
	{0x364, 177894},
	{0x366, 179109},
	{0x368, 180351},
	{0x36a, 181603},
	{0x36c, 182866},
	{0x36e, 184166},
	{0x370, 185460},
	{0x372, 186788},
	{0x374, 188123},
	{0x376, 189490},
	{0x378, 190873},
	{0x37a, 192273},
	{0x37c, 193688},
	{0x37e, 195129},
	{0x380, 196608},
	{0x382, 198095},
	{0x384, 199606},
	{0x386, 201135},
	{0x388, 202707},
	{0x38a, 204296},
	{0x38c, 205909},
	{0x38e, 207558},
	{0x390, 209229},
	{0x392, 210930},
	{0x394, 212670},
	{0x396, 214436},
	{0x398, 216234},
	{0x39a, 218073},
	{0x39c, 219940},
	{0x39e, 221850},
	{0x3a0, 223801},
	{0x3a2, 225796},
	{0x3a4, 227826},
	{0x3a6, 229902},
	{0x3a8, 232028},
	{0x3aa, 234201},
	{0x3ac, 236432},
	{0x3ae, 238706},
	{0x3b0, 241043},
	{0x3b2, 243436},
	{0x3b4, 245894},
	{0x3b6, 248409},
	{0x3b8, 251003},
	{0x3ba, 253666},
	{0x3bc, 256409},
	{0x3be, 259230},
	{0x3c0, 262144},
	{0x3c1, 263631},
	{0x3c2, 265142},
	{0x3c3, 266678},
	{0x3c4, 268243},
	{0x3c5, 269832},
	{0x3c6, 271445},
	{0x3c7, 273094},
	{0x3c8, 274765},
	{0x3c9, 276471},
	{0x3ca, 278206},
	{0x3cb, 279972},
	{0x3cc, 281770},
	{0x3cd, 283609},
	{0x3ce, 285481},
	{0x3cf, 287391},
	{0x3d0, 289341},
	{0x3d1, 291332},
	{0x3d2, 293366},
	{0x3d3, 295442},
	{0x3d4, 297567},
	{0x3d5, 299741},
	{0x3d6, 301968},
	{0x3d7, 304245},
	{0x3d8, 306579},
	{0x3d9, 308972},
	{0x3da, 311430},
	{0x3db, 313949},
	{0x3dc, 316542},
	{0x3dd, 319205},
	{0x3de, 321945},
	{0x3df, 324769},
	{0x3e0, 327680},
	{0x3E1, 330682},
	{0x3E2, 333782},
	{0x3E3, 336987},
	{0x3E4, 340305},
	{0x3E5, 343744},
	{0x3E6, 347312},
	{0x3E7, 351020},
	{0x3E8, 354880},
	{0x3E9, 358904},
	{0x3EA, 363107},
	{0x3EB, 367505},
	{0x3EC, 372118},
	{0x3ED, 376968},
	{0x3EE, 382080},
	{0x3EF, 387484},
	{0x3F0, 393216},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int mis40c1_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = mis40c1_again_lut;
	while (lut->gain <= mis40c1_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == mis40c1_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int mis40c1_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = mis40c1_again_lut;
	while(lut->gain <= mis40c1_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == mis40c1_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int mis40c1_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int mis40c1_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int mis40c1_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus mis40c1_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 660,
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

struct tx_isp_dvp_bus mis40c1_dvp = {
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

struct tx_isp_sensor_attribute mis40c1_attr = {
	.name = "mis40c1",
	.chip_id = 0x0004,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.sensor_ctrl.alloc_again = mis40c1_alloc_again,
	.sensor_ctrl.alloc_dgain = mis40c1_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = mis40c1_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list mis40c1_init_regs_2560_1440_25fps_mipi[] = {
	{0x302d,0x01},
	{SENSOR_REG_DELAY,50},
	{0x3006,0x02},
	{0x3014,0x00},
	{0x301f,0x01},
	{0x3c1d,0x0a},
	{0x3c1e,0x00},
	{0x3c1f,0x05},
	{0x3c20,0xa0},
	{0x3106,0x40},
	{0x3105,0x06},
	{0x3108,0xe4},
	{0x3107,0x0c},
	{0x310a,0x04},
	{0x3109,0x00},
	{0x310c,0xa3},
	{0x310b,0x05},
	{0x310e,0x04},
	{0x310d,0x00},
	{0x3110,0x03},
	{0x310f,0x0a},
	{0x3112,0x0c},
	{0x61a8,0x00},
	{0x4201,0x01},
	{0x4200,0x6e},
	{0x4203,0x04},
	{0x4210,0x00},
	{0x420a,0x01},
	{0x4202,0x01},
	{0x4208,0x01},
	{0x4204,0x01},
	{0x6101,0x3c},
	{0x6100,0x00},
	{0x6105,0xff},
	{0x6104,0x1f},
	{0x6103,0xc3},
	{0x6102,0x0c},
	{0x6107,0xff},
	{0x6106,0x1f},
	{0x6109,0x7c},
	{0x6108,0x04},
	{0x610d,0xff},
	{0x610c,0x1f},
	{0x610b,0xa5},
	{0x610a,0x05},
	{0x610f,0xff},
	{0x610e,0x1f},
	{0x6111,0x00},
	{0x6110,0x00},
	{0x6113,0x35},
	{0x6112,0x02},
	{0x6115,0x6d},
	{0x6114,0x04},
	{0x6117,0xa8},
	{0x6116,0x04},
	{0x6119,0x7c},
	{0x6118,0x04},
	{0x611b,0xb7},
	{0x611a,0x04},
	{0x611d,0x1e},
	{0x611c,0x00},
	{0x611f,0x99},
	{0x611e,0x04},
	{0x6121,0x00},
	{0x6120,0x00},
	{0x6123,0xd2},
	{0x6122,0x0c},
	{0x6125,0x00},
	{0x6124,0x00},
	{0x6127,0xd2},
	{0x6126,0x0c},
	{0x6129,0x1e},
	{0x6128,0x00},
	{0x612b,0xab},
	{0x612a,0x0c},
	{0x612d,0x00},
	{0x612c,0x00},
	{0x612f,0x84},
	{0x612e,0x04},
	{0x6131,0x3c},
	{0x6130,0x00},
	{0x6133,0xbe},
	{0x6132,0x01},
	{0x6135,0x3c},
	{0x6134,0x00},
	{0x6137,0xaf},
	{0x6136,0x01},
	{0x61ad,0x17},
	{0x61ac,0x02},
	{0x61b1,0x5e},
	{0x61b0,0x04},
	{0x61af,0x35},
	{0x61ae,0x02},
	{0x61b3,0x39},
	{0x61b2,0x06},
	{0x6139,0x70},
	{0x6138,0x02},
	{0x613d,0x74},
	{0x613c,0x06},
	{0x613b,0x40},
	{0x613a,0x04},
	{0x613f,0xb4},
	{0x613e,0x0c},
	{0x6141,0x61},
	{0x6140,0x02},
	{0x6143,0x4f},
	{0x6142,0x04},
	{0x6145,0x66},
	{0x6144,0x06},
	{0x6147,0xc3},
	{0x6146,0x0c},
	{0x6149,0x52},
	{0x6148,0x02},
	{0x614d,0x57},
	{0x614c,0x06},
	{0x614b,0x3c},
	{0x614a,0x04},
	{0x614f,0xb0},
	{0x614e,0x0c},
	{0x6151,0x00},
	{0x6150,0x00},
	{0x6155,0x52},
	{0x6154,0x02},
	{0x6159,0x57},
	{0x6158,0x06},
	{0x6153,0x77},
	{0x6152,0x00},
	{0x6157,0x49},
	{0x6156,0x04},
	{0x615b,0xbd},
	{0x615a,0x0c},
	{0x615d,0x38},
	{0x615c,0x03},
	{0x6161,0x3c},
	{0x6160,0x07},
	{0x615f,0x40},
	{0x615e,0x04},
	{0x6163,0xb4},
	{0x6162,0x0c},
	{0x6165,0x00},
	{0x6164,0x00},
	{0x6169,0x7c},
	{0x6168,0x04},
	{0x6167,0x70},
	{0x6166,0x02},
	{0x616b,0x74},
	{0x616a,0x06},
	{0x616d,0xf9},
	{0x616c,0x01},
	{0x6171,0x5e},
	{0x6170,0x04},
	{0x616f,0x49},
	{0x616e,0x04},
	{0x6173,0xbd},
	{0x6172,0x0c},
	{0x6175,0x00},
	{0x6174,0x00},
	{0x6177,0x0f},
	{0x6176,0x00},
	{0x6179,0x01},
	{0x6178,0x00},
	{0x617b,0xbd},
	{0x617a,0x0c},
	{0x617d,0x00},
	{0x617c,0x00},
	{0x617f,0x29},
	{0x617e,0x01},
	{0x6181,0x00},
	{0x6180,0x00},
	{0x6183,0x29},
	{0x6182,0x01},
	{0x6185,0x00},
	{0x6184,0x00},
	{0x6187,0x29},
	{0x6186,0x01},
	{0x61b5,0x5e},
	{0x61b4,0x04},
	{0x61b7,0x6d},
	{0x61b6,0x04},
	{0x6189,0xd2},
	{0x6188,0x0c},
	{0x618b,0xe1},
	{0x618a,0x0c},
	{0x618d,0xd0},
	{0x618c,0x00},
	{0x6191,0x7c},
	{0x6190,0x04},
	{0x618f,0xee},
	{0x618e,0x00},
	{0x6193,0x99},
	{0x6192,0x04},
	{0x6195,0x0d},
	{0x6194,0x0d},
	{0x61a3,0xd0},
	{0x61a2,0x01},
	{0x61a5,0xcc},
	{0x61a4,0x05},
	{0x5400,0x2B},
	{0x5403,0x08},
	{0x5406,0x00},
	{0x5408,0x3e},
	{0x3902,0x02},
	{0x3a04,0x10},
	{0x3a17,0x00},
	{0x3a18,0x00},
	{0x3048,0x01},
	{0x6207,0x00},
	{0x6208,0x00},
	{0x3103,0x03},
	{0x3104,0xe0},
	{0x3118,0x00},
	{0x3700,0x01},
	{0x3701,0x00},
	{0x3702,0x40},
	{0x3703,0x00},
	{0x3704,0x40},
	{0x3705,0x00},
	{0x3706,0x40},
	{0x3707,0x00},
	{0x3708,0x40},
	{0x300c,0x01},
	{0x610b,0x40},
	{0x613d,0x70},
	{0x61a1,0x90},
	{0x6202,0x0c},
	{0x3a03,0x39},
	{0x3a02,0xfc},
	{0x3a08,0x0c},
	{0x3048,0x01},
	{0x61ae,0x02},
	{0x61af,0x50},
	{0x61b2,0x06},
	{0x61b3,0x50},
	{0x3040,0x01},
	{0x6200,0x09},
	{0x6201,0x09},
	{0x3a02,0x2d},
	{0x3a03,0x19},
	{0x6200,0x00},
	{0x6201,0x09},
	{0x6124,0x1f},
	{0x6125,0xff},
	{0x6126,0x00},
	{0x6127,0x00},
	{0x3a13,0x39},
	{0x3a15,0x08},
	{0x3a02,0x2d},
	{0x5400,0x23},
	{0x3a0c,0x07},
	{0x3040,0x01},
	{0x6199,0x90},
	{0x619b,0x90},
	{0x619d,0x90},
	{0x619f,0x90},
	{0x61a1,0x90},
	{0x420c,0x00},
	{0x420d,0x0a},
	{0x3a01,0x01},
	{0x6209,0x12},
	{0x3a02,0xad},
	{0x610b,0x08},
	{0x3a14,0x0c},
	{0x3a0e,0xe0},
	{0x3a05,0x6a},
	{0x3048,0x01},
	{0x3a0d,0x32},
	{0x3040,0x01},
	{0x5407,0x02},
	{0x540d,0x01},
	{0x540d,0x03},
	{0x540e,0xff},
	{0x5403,0x0f},
	{0x5404,0xff},
	{0x3006,0x00},
	{0x302d,0x00},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
 * the order of the mis40c1_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting mis40c1_win_sizes[] = {
	/* 2560*1440 [0] */
	{
		.width          = 2560,
		.height         = 1440,
		.fps            = 25 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SGRBG10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = mis40c1_init_regs_2560_1440_25fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &mis40c1_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list mis40c1_stream_on_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list mis40c1_stream_off_mipi[] = {
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int mis40c1_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int mis40c1_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int mis40c1_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = mis40c1_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int mis40c1_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = mis40c1_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int mis40c1_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int mis40c1_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int mis40c1_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = mis40c1_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int mis40c1_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = mis40c1_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int mis40c1_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int mis40c1_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.min = SENSOR_OUTPUT_MIN_FPS << 16 | 1;
	sensor->video.fps.apply_delay = 1;

#ifdef SENSOR_WDR_2_FRAME
	sensor->video.wdr = 1;
#else
	sensor->video.wdr = 0;
#endif

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int mis40c1_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &mis40c1_win_sizes[0];
		mis40c1_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		mis40c1_attr.again.value = 3968;
		mis40c1_attr.again.max = 393216;
		mis40c1_attr.again.min = 0;
		mis40c1_attr.again.apply_delay = 2;

		mis40c1_attr.integration_time.value = 0x20;
		mis40c1_attr.integration_time.max = 1600 - 3;
		mis40c1_attr.integration_time.min = 2;
		mis40c1_attr.integration_time.apply_delay = 2;

		mis40c1_attr.total_width = 3300;
		mis40c1_attr.total_height = 1600;

		mis40c1_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		mis40c1_attr.hcg.base_gain = ;
		mis40c1_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

#ifdef SENSOR_WDR_2_FRAME
		mis40c1_attr.again_short.value = ;
		mis40c1_attr.again_short.max = ;
		mis40c1_attr.again_short.min = ;
		mis40c1_attr.again_short.apply_delay = ;

		mis40c1_attr.integration_time_short.value = ;
		mis40c1_attr.integration_time_short.max = ;
		mis40c1_attr.integration_time_short.min = ;
		mis40c1_attr.integration_time_short.apply_delay = ;

		mis40c1_attr.wdr_cache = wdr_line * mis40c1_attr.total_width;

#ifdef SENSOR_HCG
		mis40c1_attr.hcg_short.base_gain = ;
		mis40c1_attr.hcg_short.apply_delay = ;
#endif /* SENSOR_HCG */
#endif /* SENSOR_WDR_2_FRAME */

		memcpy((void *)(&(mis40c1_attr.mipi)), (void *)(&mis40c1_mipi_linear), sizeof(mis40c1_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int mis40c1_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	mis40c1_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		mis40c1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		mis40c1_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		mis40c1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		mis40c1_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		mis40c1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = mis40c1_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	mis40c1_attr_set(sd, wsize);
	sensor->video.fps.apply_delay = 1;
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int mis40c1_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = mis40c1_read(sd, 0x3004, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = mis40c1_read(sd, 0x3005, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int mis40c1_g_chip_ident(struct tx_isp_subdev *sd,
								struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	mis40c1_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "mis40c1_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "mis40c1_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = mis40c1_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an mis40c1 chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("MIS40C1 version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", mis40c1_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "mis40c1", sizeof("mis40c1"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int mis40c1_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int mis40c1_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (!init->enable) {
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		return ISP_SUCCESS;
	} else {
		ret = mis40c1_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int mis40c1_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = mis40c1_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int mis40c1_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	mis40c1_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int mis40c1_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = mis40c1_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = mis40c1_write_array(sd, mis40c1_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("mis40c1 stream on\n");
		}

	} else {
		ret = mis40c1_write_array(sd, mis40c1_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("mis40c1 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int mis40c1_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;
	unsigned int vts = 0;
	unsigned char tmp;

	ret += mis40c1_write(sd,  0x3100, (unsigned char)((it >> 8)& 0xff));
	ret += mis40c1_write(sd, 0x3101, (unsigned char)(it & 0xff));
	ret += mis40c1_write(sd, 0x3103, (unsigned char)(again >> 8 & 0x03));
	ret += mis40c1_write(sd, 0x3104, (unsigned char)(again & 0xff));
	ret += mis40c1_write(sd, 0x300c, 0x01);

	ret += mis40c1_read(sd, 0x3105, &tmp);
	vts = tmp;
	ret += mis40c1_read(sd, 0x3106, &tmp);
	vts = ((vts << 8) + tmp);

	if((again >= 0x3f0) && (vts >= 0xbb8))  {
		ret += mis40c1_write(sd, 0x3a13, 0x29);//优化低帧率的竖纹
	}
	else{
		ret += mis40c1_write(sd, 0x3a13, 0x3d);
	}

	return ret;
}
#else
static int mis40c1_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int mis40c1_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int mis40c1_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int mis40c1_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int mis40c1_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = mis40c1_attr_set(sd, wsize);
	}

	return ret;
}

static int mis40c1_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 3300 * 1600 * 25;  /**< HTS * VTS * FPS */
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
	ret += mis40c1_read(sd, 0x3107, &val);
	hts = val << 8;
	val = 0;
	ret += mis40c1_read(sd, 0x3108, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: mis40c1 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	mis40c1_write(sd, 0x3106, (unsigned char) (vts & 0xff));
	mis40c1_write(sd, 0x3105, (unsigned char) (vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: mis40c1_write err\n");
		return ret;
	}

	sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->integration_time.max = vts - 3;
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

static int mis40c1_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	int ret = ISP_SUCCESS;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */
	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		ret += mis40c1_write(sd, 0x3006, 0x02);//3007翻转后还需要移动起始和终止位，让bayer格式对齐，下同
		msleep(50);
		ret += mis40c1_write(sd, 0x3007, 0x00);
		ret += mis40c1_write(sd, 0x310a, 0x04);
		ret += mis40c1_write(sd, 0x310c, 0xa3);
		ret += mis40c1_write(sd, 0x310e, 0x04);
		ret += mis40c1_write(sd, 0x3110, 0x03);
		ret += mis40c1_write(sd, 0x3006, 0x00);
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		ret += mis40c1_write(sd, 0x3006, 0x02);
		msleep(50);
		ret += mis40c1_write(sd, 0x3007, 0x01);
		ret += mis40c1_write(sd, 0x310a, 0x04);
		ret += mis40c1_write(sd, 0x310c, 0xa3);
		ret += mis40c1_write(sd, 0x310e, 0x05);
		ret += mis40c1_write(sd, 0x3110, 0x04);
		ret += mis40c1_write(sd, 0x3006, 0x00);
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		ret += mis40c1_write(sd, 0x3006, 0x02);
		msleep(50);
		ret += mis40c1_write(sd, 0x3007, 0x02);
		ret += mis40c1_write(sd, 0x310a, 0x05);
		ret += mis40c1_write(sd, 0x310c, 0xa4);
		ret += mis40c1_write(sd, 0x310e, 0x04);
		ret += mis40c1_write(sd, 0x3110, 0x03);
		ret += mis40c1_write(sd, 0x3006, 0x00);
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		ret += mis40c1_write(sd, 0x3006, 0x02);
		msleep(50);
		ret += mis40c1_write(sd, 0x3007, 0x03);
		ret += mis40c1_write(sd, 0x310a, 0x05);
		ret += mis40c1_write(sd, 0x310c, 0xa4);
		ret += mis40c1_write(sd, 0x310e, 0x05);
		ret += mis40c1_write(sd, 0x3110, 0x04);
		ret += mis40c1_write(sd, 0x3006, 0x00);
		break;
	}

	sensor->video.hvflip_mode = par->hvflip;
	mis40c1_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int mis40c1_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int mis40c1_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}

static int mis40c1_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#endif /* SENSOR_EXPO */

static int mis40c1_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = mis40c1_write_array(sd, mis40c1_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		mis40c1_setting_select(sd, 1);
		mis40c1_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		mis40c1_setting_select(sd, 0);
		mis40c1_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int mis40c1_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = mis40c1_write_array(sd, wsize->regs);
	ret = mis40c1_write_array(sd, mis40c1_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int mis40c1_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = mis40c1_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = mis40c1_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = mis40c1_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = mis40c1_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = mis40c1_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = mis40c1_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = mis40c1_write_array(sd, mis40c1_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = mis40c1_write_array(sd, mis40c1_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = mis40c1_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = mis40c1_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = mis40c1_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = mis40c1_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = mis40c1_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = mis40c1_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = mis40c1_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops mis40c1_core_ops = {
	.g_chip_ident = mis40c1_g_chip_ident,
	.reset = mis40c1_reset,
	.init = mis40c1_init,
	.g_register = mis40c1_g_register,
	.s_register = mis40c1_s_register,
};

static struct tx_isp_subdev_video_ops mis40c1_video_ops = {
	.s_stream = mis40c1_s_stream,
};

static struct tx_isp_subdev_sensor_ops mis40c1_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = mis40c1_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops mis40c1_ops = {
	.core = &mis40c1_core_ops,
	.video = &mis40c1_video_ops,
	.sensor = &mis40c1_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
	.name = "mis40c1",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int mis40c1_probe(struct i2c_client *client,
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
	sensor->video.attr = &mis40c1_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &mis40c1_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->mis40c1\n");

	return 0;
}

static int mis40c1_remove(struct i2c_client *client)
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

static const struct i2c_device_id mis40c1_id[] = {
	{"mis40c1", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, mis40c1_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver mis40c1_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "mis40c1",
	},
	.probe          = mis40c1_probe,
	.remove         = mis40c1_remove,
	.id_table       = mis40c1_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_mis40c1(void) {
	return private_i2c_add_driver(&mis40c1_driver);
}

static __exit void exit_mis40c1(void) {
	private_i2c_del_driver(&mis40c1_driver);
}

module_init(init_mis40c1);
module_exit(exit_mis40c1);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_mis40c1(void) {
	return private_i2c_add_driver(&mis40c1_driver);
}

static void exit_first_mis40c1(void) {
	private_i2c_del_driver(&mis40c1_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "mis40c1",
	.i2c_addr = 0x30,
	.width = 2560,
	.height = 1440,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_mis40c1,
	.exit_sensor = exit_first_mis40c1
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for mis40c1 sensor");
MODULE_LICENSE("GPL");
