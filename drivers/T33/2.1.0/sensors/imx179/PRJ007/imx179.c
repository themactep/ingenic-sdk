/*
* imx179.c
*
* Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* @Settings:
* sboot        resolution      fps       interface     mode    raw     max_fps		dvdd
*   0           3280x2464      30        mipi_4lane    linear    10      30
*   1           1640x1232      30        mipi_4lane    linear    10      30
*   2           3280x2464      20        mipi_4lane    linear    10      20
*
* @I2C addr: 0x10
*
* @FSync: None
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
#define SENSOR_VERSION  "H20250512b"

// #define SENSOR_TEST

// #define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
// #define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
/* #define SENSOR_HCG */
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x09)
#define SENSOR_CHIP_ID_L    (0x87)
#define SENSOR_OUTPUT_MIN_FPS 1
#define SENSOR_MCLK 27000000
#define SENSOR_MCLK_24m 24000000
#define AGAIN_MAX_DB 0x64
#define DGAIN_MAX_DB 0x64
#define LOG2_GAIN_SHIFT 16

#ifdef CONFIG_ZERATUL
#define ZRT_SENSOR_WITHOUT_INIT
#endif

#ifdef SENSOR_WDR_2_FRAME
static int wdr_line = 1850;
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

struct tx_isp_sensor_attribute imx179_attr;

/*
* The part of driver maybe modify about different sensor and different board.
*/
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut imx179_again_lut[] = {
	{0,0},
	{1,327},
	{2,762},
	{3,1089},
	{4,1524},
	{5,1850},
	{6,2286},
	{7,2612},
	{8,3048},
	{9,3374},
	{10,3810},
	{11,4136},
	{12,4572},
	{13,4898},
	{14,5334},
	{15,5660},
	{16,6096},
	{17,6531},
	{18,6858},
	{19,7293},
	{20,7729},
	{21,8055},
	{22,8491},
	{23,8926},
	{24,9361},
	{25,9688},
	{26,10123},
	{27,10559},
	{28,10994},
	{29,11321},
	{30,11756},
	{31,12192},
	{32,12627},
	{33,13062},
	{34,13498},
	{35,13933},
	{36,14369},
	{37,14804},
	{38,15239},
	{39,15675},
	{40,16110},
	{41,16546},
	{42,16981},
	{43,17416},
	{44,17852},
	{45,18287},
	{46,18723},
	{47,19158},
	{48,19594},
	{49,20138},
	{50,20573},
	{51,21009},
	{52,21444},
	{53,21879},
	{54,22424},
	{55,22859},
	{56,23295},
	{57,23839},
	{58,24274},
	{59,24818},
	{60,25254},
	{61,25689},
	{62,26234},
	{63,26669},
	{64,27213},
	{65,27649},
	{66,28193},
	{67,28737},
	{68,29173},
	{69,29717},
	{70,30152},
	{71,30697},
	{72,31241},
	{73,31785},
	{74,32220},
	{75,32765},
	{76,33309},
	{77,33853},
	{78,34398},
	{79,34942},
	{80,35377},
	{81,35921},
	{82,36466},
	{83,37010},
	{84,37554},
	{85,38099},
	{86,38752},
	{87,39296},
	{88,39840},
	{89,40384},
	{90,40929},
	{91,41582},
	{92,42126},
	{93,42670},
	{94,43215},
	{95,43868},
	{96,44412},
	{97,45065},
	{98,45609},
	{99,46262},
	{100,46807},
	{101,47460},
	{102,48004},
	{103,48657},
	{104,49310},
	{105,49963},
	{106,50508},
	{107,51161},
	{108,51814},
	{109,52467},
	{110,53120},
	{111,53773},
	{112,54426},
	{113,55080},
	{114,55733},
	{115,56386},
	{116,57039},
	{117,57692},
	{118,58454},
	{119,59107},
	{120,59760},
	{121,60522},
	{122,61175},
	{123,61937},
	{124,62590},
	{125,63352},
	{126,64114},
	{127,64767},
	{128,65529},
	{129,66291},
	{130,67053},
	{131,67815},
	{132,68577},
	{133,69339},
	{134,70101},
	{135,70863},
	{136,71625},
	{137,72387},
	{138,73258},
	{139,74020},
	{140,74891},
	{141,75653},
	{142,76524},
	{143,77286},
	{144,78156},
	{145,79027},
	{146,79898},
	{147,80769},
	{148,81640},
	{149,82511},
	{150,83381},
	{151,84252},
	{152,85123},
	{153,86103},
	{154,86973},
	{155,87953},
	{156,88824},
	{157,89804},
	{158,90783},
	{159,91763},
	{160,92743},
	{161,93722},
	{162,94702},
	{163,95791},
	{164,96770},
	{165,97750},
	{166,98838},
	{167,99927},
	{168,101016},
	{169,101995},
	{170,103084},
	{171,104281},
	{172,105370},
	{173,106458},
	{174,107656},
	{175,108853},
	{176,109941},
	{177,111139},
	{178,112336},
	{179,113534},
	{180,114840},
	{181,116037},
	{182,117343},
	{183,118650},
	{184,119956},
	{185,121262},
	{186,122568},
	{187,123983},
	{188,125290},
	{189,126705},
	{190,128120},
	{191,129644},
	{192,131059},
	{193,132583},
	{194,134107},
	{195,135631},
	{196,137155},
	{197,138787},
	{198,140420},
	{199,142053},
	{200,143686},
	{201,145428},
	{202,147169},
	{203,148911},
	{204,150652},
	{205,152503},
	{206,154462},
	{207,156313},
	{208,158272},
	{209,160232},
	{210,162300},
	{211,164368},
	{212,166545},
	{213,168722},
	{214,170899},
	{215,173185},
	{216,175471},
	{217,177866},
	{218,180369},
	{219,182873},
	{220,185485},
	{221,188098},
	{222,190928},
	{223,193649},
	{224,196588},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int imx179_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = imx179_again_lut;
	while (lut->gain <= imx179_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == imx179_attr.again.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}
#else
	/* Non analog gain table */
	uint16_t again=(isp_gain*20)>>LOG2_GAIN_SHIFT;
	// Limit Max gain
	if(again>AGAIN_MAX_DB+DGAIN_MAX_DB) again=AGAIN_MAX_DB+DGAIN_MAX_DB;

	/* p_ctx->again=again; */
	*sensor_again=again;
	isp_gain= (((int32_t)again)<<LOG2_GAIN_SHIFT)/20;

	return isp_gain;
#endif  /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

	return isp_gain;
}

#ifdef SENSOR_WDR_2_FRAME
unsigned int imx179_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = imx179_again_lut;
	while(lut->gain <= imx179_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == imx179_attr.again_short.max) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	...;
#else
	/* Non analog gain table */
	uint16_t again=(isp_gain*20)>>LOG2_GAIN_SHIFT;
	// Limit Max gain
	if(again>AGAIN_MAX_DB+DGAIN_MAX_DB) again=AGAIN_MAX_DB+DGAIN_MAX_DB;

	/* p_ctx->again=again; */
	*sensor_again=again;
	isp_gain= (((int32_t)again)<<LOG2_GAIN_SHIFT)/20;

	return isp_gain;
#endif /* SENSOR_AGAIN_TABLE */
#endif /* SENSOR_TEST */

	return isp_gain;
}
#endif /* SENSOR_WDR_2_FRAME */

unsigned int imx179_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int imx179_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int imx179_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus imx179_mipi_linear = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 640,
	.lans = 4,
	.image_twidth = 3280,
	.image_theight = 2460,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.mipi_crop_start0x = 0,
	.mipi_sc.mipi_crop_start0y = 152,
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
	.mipi_sc.del_start = 4,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus imx179_mipi_linear_1232p = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 480,
	.lans = 4,
	.image_twidth = 1640,
	.image_theight = 1230,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.mipi_crop_start0x = 0,
	.mipi_sc.mipi_crop_start0y = 2,
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

struct tx_isp_mipi_bus imx179_mipi_linear_2464p = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 512,
	.lans = 4,
	.image_twidth = 3280,
	.image_theight = 2460,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.mipi_crop_start0x = 0,
	.mipi_sc.mipi_crop_start0y = 152,
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
	.mipi_sc.del_start = 4,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_dvp_bus imx179_dvp = {
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

struct tx_isp_sensor_attribute imx179_attr = {
	.name = "imx179",
	.chip_id = 0x2823,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x10,
	.sensor_ctrl.alloc_again = imx179_alloc_again,
	.sensor_ctrl.alloc_dgain = imx179_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = imx179_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list imx179_init_regs_3280_2464_30fps_mipi[] = {
	{0x0100,0x00},
	{0x0101,0x00},
	{0x0202,0x09},
	{0x0203,0xaa},
	{0x0301,0x05},
	{0x0303,0x01},
	{0x0305,0x06},
	{0x0309,0x05},
	{0x030b,0x01},
	{0x030c,0x00},
	{0x030d,0xa0},
	{0x0340,0x09},//
	{0x0341,0xae},//vts = 2478
	{0x0342,0x0d},//
	{0x0343,0x70},//hts =  3440
	{0x0344,0x00},
	{0x0345,0x00},
	{0x0346,0x00},
	{0x0347,0x00},
	{0x0348,0x0c},
	{0x0349,0xcf},
	{0x034a,0x09},
	{0x034b,0x9f},
	{0x034c,0x0c},
	{0x034d,0xd0},
	{0x034e,0x09},
	{0x034f,0xa0},
	{0x0383,0x01},
	{0x0387,0x01},
	{0x0390,0x00},
	{0x0401,0x00},
	{0x0405,0x10},
	{0x3020,0x10},
	{0x3041,0x15},
	{0x3042,0x87},
	{0x3089,0x4f},
	{0x3309,0x9a},
	{0x3344,0x57},
	{0x3345,0x1f},
	{0x3362,0x0a},
	{0x3363,0x0a},
	{0x3364,0x00},
	{0x3368,0x18},
	{0x3369,0x00},
	{0x3370,0x77},
	{0x3371,0x2f},
	{0x3372,0x4f},
	{0x3373,0x2f},
	{0x3374,0x2f},
	{0x3375,0x37},
	{0x3376,0x9f},
	{0x3377,0x37},
	{0x33c8,0x00},
	{0x33d4,0x0c},
	{0x33d5,0xd0},
	{0x33d6,0x09},
	{0x33d7,0xa0},
	{0x4100,0x0e},
	{0x4108,0x01},
	{0x4109,0x7c},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list imx179_init_regs_1640_1232_30fps_mipi[] = {
	{0x41C0,0x01},
	{0x0104,0x01},
	{0x0100,0x00},
	{0x0103,0x01},
	{0x0101,0x00},
	{0x0202,0x05},
	{0x0203,0x00},
	{0x0301,0x06},
	{0x0303,0x01},
	{0x0305,0x06},
	{0x0309,0x05},
	{0x030b,0x01},
	{0x030c,0x00},
	{0x030d,0x78},
	{0x0340,0x06},
	{0x0341,0x0e},
	{0x0342,0x0d},
	{0x0343,0x70},
	{0x0344,0x00},
	{0x0345,0x00},
	{0x0346,0x00},
	{0x0347,0x00},
	{0x0348,0x0c},
	{0x0349,0xcf},
	{0x034a,0x09},
	{0x034b,0x9f},
	{0x034c,0x06},
	{0x034d,0x68},
	{0x034e,0x04},
	{0x034f,0xd0},
	{0x0383,0x01},
	{0x0387,0x01},
	{0x0390,0x01},
	{0x0401,0x00},
	{0x0405,0x10},
	{0x3020,0x10},
	{0x3041,0x15},
	{0x3042,0x87},
	{0x3089,0x4f},
	{0x3309,0x9a},
	{0x3344,0x57},
	{0x3345,0x1f},
	{0x3362,0x0a},
	{0x3363,0x0a},
	{0x3364,0x00},
	{0x3368,0x18},
	{0x3369,0x00},
	{0x3370,0x77},
	{0x3371,0x2f},
	{0x3372,0x4f},
	{0x3373,0x2f},
	{0x3374,0x2f},
	{0x3375,0x37},
	{0x3376,0x9f},
	{0x3377,0x37},
	{0x33c8,0x00},
	{0x33d4,0x0c},
	{0x33d5,0xd0},
	{0x33d6,0x09},
	{0x33d7,0xa0},
	{0x4100,0x0e},
	{0x4108,0x01},
	{0x4109,0x7c},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list imx179_init_regs_3280_2464_20fps_mipi[] = {
	{0x41C0,0x01},
	{0x0104,0x01},
	{0x0100,0x00},
	{0x0103,0x01},
	{0x0101,0x00},
	{0x0202,0x02},
	{0x0203,0xaa},
	{0x0301,0x06},
	{0x0303,0x01},
	{0x0305,0x06},
	{0x0309,0x05},
	{0x030b,0x01},
	{0x030c,0x00},
	{0x030d,0x80},
	{0x0340,0x09},
	{0x0341,0xb0},
	{0x0342,0x0d},
	{0x0343,0x70},
	{0x0344,0x00},
	{0x0345,0x00},
	{0x0346,0x00},
	{0x0347,0x00},
	{0x0348,0x0c},
	{0x0349,0xcf},
	{0x034a,0x09},
	{0x034b,0x9f},
	{0x034c,0x0c},
	{0x034d,0xd0},
	{0x034e,0x09},
	{0x034f,0xa0},
	{0x0383,0x01},
	{0x0387,0x01},
	{0x0390,0x00},
	{0x0401,0x00},
	{0x0405,0x10},
	{0x3020,0x10},
	{0x3041,0x15},
	{0x3042,0x87},
	{0x3089,0x4f},
	{0x3309,0x9a},
	{0x3344,0x57},
	{0x3345,0x1f},
	{0x3362,0x0a},
	{0x3363,0x0a},
	{0x3364,0x00},
	{0x3368,0x18},
	{0x3369,0x00},
	{0x3370,0x77},
	{0x3371,0x2f},
	{0x3372,0x4f},
	{0x3373,0x2f},
	{0x3374,0x2f},
	{0x3375,0x37},
	{0x3376,0x9f},
	{0x3377,0x37},
	{0x33c8,0x00},
	{0x33d4,0x0c},
	{0x33d5,0xd0},
	{0x33d6,0x09},
	{0x33d7,0xa0},
	{0x4100,0x0e},
	{0x4108,0x01},
	{0x4109,0x7c},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
* the order of the imx179_win_sizes is [full_resolution, preview_resolution].
*/
static struct tx_isp_sensor_win_setting imx179_win_sizes[] = {
	/* 3280*2160@25fps [0] */
	{
		.width          = 3280,
		.height         = 2160,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = imx179_init_regs_3280_2464_30fps_mipi,
	},
	/* 1640*1232@30fps [1] */
	{
		.width          = 1632,
		.height         = 1232,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = imx179_init_regs_1640_1232_30fps_mipi,
	},
	/* 3280*2160@20fps [2] */
	{
		.width          = 3280,
		.height         = 2160,
		.fps            = 20 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = imx179_init_regs_3280_2464_20fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &imx179_win_sizes[0];

/*
* the part of driver was fixed.
*/

static struct regval_list imx179_stream_on_mipi[] = {
	{0x0100, 0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list imx179_stream_off_mipi[] = {
	{0x0100, 0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int imx179_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int imx179_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int imx179_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = imx179_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int imx179_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = imx179_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int imx179_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int imx179_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int imx179_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = imx179_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int imx179_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = imx179_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int imx179_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int imx179_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int imx179_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &imx179_win_sizes[0];
		imx179_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		imx179_attr.again.value = 0;
		imx179_attr.again.max = 196588;
		imx179_attr.again.min = 0;
		imx179_attr.again.apply_delay = 2;

		imx179_attr.integration_time.value = 0;
		imx179_attr.integration_time.max = 2478 - 4;
		imx179_attr.integration_time.min = 2;
		imx179_attr.integration_time.apply_delay = 2;

		imx179_attr.total_width = 3440;
		imx179_attr.total_height = 2478;

		imx179_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		imx179_attr.hcg.base_gain = ;
		imx179_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

		printk(" -------win_size[0]------ \n");
		memcpy((void *)(&(imx179_attr.mipi)), (void *)(&imx179_mipi_linear), sizeof(imx179_attr.mipi));
		break;
	case 1:
		wsize = &imx179_win_sizes[1];
		imx179_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		imx179_attr.again.value = 0;
		imx179_attr.again.max = 196588;
		imx179_attr.again.min = 0;
		imx179_attr.again.apply_delay = 2;

		imx179_attr.integration_time.value = 0;
		imx179_attr.integration_time.max = 1550 - 4;
		imx179_attr.integration_time.min = 2;
		imx179_attr.integration_time.apply_delay = 2;

		imx179_attr.total_width = 3440;
		imx179_attr.total_height = 1550;

		imx179_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		imx179_attr.hcg.base_gain = ;
		imx179_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

		printk(" -------win_size[1]------ \n");
		memcpy((void *)(&(imx179_attr.mipi)), (void *)(&imx179_mipi_linear_1232p), sizeof(imx179_attr.mipi));
		break;
	case 2:
		wsize = &imx179_win_sizes[2];
		imx179_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		imx179_attr.again.value = 0;
		imx179_attr.again.max = 196588;
		imx179_attr.again.min = 0;
		imx179_attr.again.apply_delay = 2;

		imx179_attr.integration_time.value = 0;
		imx179_attr.integration_time.max = 2315 - 4;
		imx179_attr.integration_time.min = 2;
		imx179_attr.integration_time.apply_delay = 2;

		imx179_attr.total_width = 3440;
		imx179_attr.total_height = 2315;

		imx179_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		imx179_attr.hcg.base_gain = ;
		imx179_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

	printk(" -------win_size[2]------ \n");
		memcpy((void *)(&(imx179_attr.mipi)), (void *)(&imx179_mipi_linear_2464p), sizeof(imx179_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int imx179_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	imx179_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		imx179_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		imx179_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		imx179_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		imx179_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		imx179_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	switch (info->default_boot)
	{
	case 0:
		ret = imx179_clk_set(sd, sclka, SENSOR_MCLK);
		break;
	case 1:
	case 2:
		ret = imx179_clk_set(sd, sclka, SENSOR_MCLK_24m);
		break;
	default:
		break;
	}

	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.apply_delay = 1;
	imx179_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int imx179_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v=0;
	int ret=0;

	ret = imx179_read(sd, 0x0008, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	/*if (v != SENSOR_CHIP_ID_H)*/
		/*return -ENODEV;*/
	*ident = v;

	ret = imx179_read(sd, 0x0009, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	/*if (v != SENSOR_CHIP_ID_L)*/
		/*return -ENODEV;*/
	*ident = (*ident << 8) | v;

	return 0;
}

static int imx179_g_chip_ident(struct tx_isp_subdev *sd,
								struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	imx179_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "imx179_reset");
		if (!ret) {
			private_gpio_direction_output(info->rst_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
		}
	}
	if (info->pwdn_gpio != -1) {
		ret = private_gpio_request(info->pwdn_gpio, "imx179_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = imx179_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an imx179 chip.\n",
				client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", imx179_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "imx179", sizeof("imx179"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int imx179_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int imx179_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = imx179_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int imx179_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = imx179_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int imx179_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	imx179_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int imx179_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = imx179_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
/*#ifndef ZRT_SENSOR_WITHOUT_INIT*/
			ret = imx179_write_array(sd, imx179_stream_on_mipi);
/*#endif [> ZRT_SENSOR_WITHOUT_INIT <]*/
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("imx179 stream on\n");
		}
	}
	else
	{
#ifndef ZRT_SENSOR_WITHOUT_INIT
		ret = imx179_write_array(sd, imx179_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("imx179 stream off\n");
#endif /* ZRT_SENSOR_WITHOUT_INIT */
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int imx179_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	// printk("\n==> it %d again 0x%x\n", it, again);
	ret += imx179_write(sd, 0x0401,0x01);
	ret += imx179_write(sd, 0x0202, (unsigned char)((it >> 8) & 0xff));
	ret += imx179_write(sd, 0x0203, (unsigned char)(it & 0xff));
	// ret += imx179_write(sd, 0x0204, (unsigned char)((again >> 8) & 0xff));
	ret += imx179_write(sd, 0x0205, (unsigned char)(again & 0xff));
	ret += imx179_write(sd, 0x0401,0x00);

	return ret;
}
#else
static int imx179_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	ret = imx179_write(sd, 0x0104, 0x01);
	ret += imx179_write(sd, 0x0202, (unsigned char)((value >> 8) & 0xff));
	ret += imx179_write(sd, 0x0203, (unsigned char)(value & 0xff));
	ret = imx179_write(sd, 0x0104, 0x00);
	return ret;
}

static int imx179_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	ret = imx179_write(sd, 0x0104, 0x01);
	ret += imx179_write(sd, 0x0204, (unsigned char)((value >> 8) & 0xff));
	ret += imx179_write(sd, 0x0205, (unsigned char)(value & 0xff));
	ret = imx179_write(sd, 0x0104, 0x00);
	return ret;
}
#endif /* SENSOR_EXPO */

static int imx179_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx179_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx179_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = imx179_attr_set(sd, wsize);
	}

	return ret;
}

static int imx179_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 3440 * 2478 * 30;
		break;
	case 1:
		sclk = 3440 * 1550 * 30;
		break;
	case 2:
		sclk = 3440 * 2315 * 20;
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
	ret += imx179_read(sd, 0x0342, &val);
	hts = val << 8;
	val = 0;
	ret += imx179_read(sd, 0x0343, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: imx179 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += imx179_write(sd, 0x0104, 0x01);
	ret += imx179_write(sd, 0x0340, (unsigned char)((vts & 0xff00) >> 8));
	ret += imx179_write(sd, 0x0341, (unsigned char)(vts & 0xff));
	ret += imx179_write(sd, 0x0104, 0x00);

	if (0 != ret) {
		ISP_ERROR("err: imx179_write err\n");
		return ret;
	}

	sensor->video.fps.max = fps;
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
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}
#endif /* SENSOR_WDR_2_FRAME */

	printk("\n==> [%s %d] fps 0x%x hts %d vts %d\n", __func__, __LINE__, fps, hts, vts);
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	if (ret) {
		ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
	}

	return ret;
}

static int imx179_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	int ret = ISP_SUCCESS;
	uint8_t val;

	par->drop_frame = 0;
	par->reset = 0;

	/* 2'b01:mirror,2'b10:filp */

	ret = imx179_read(sd, 0x0101, &val);
	val &= ~0x03;

	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		val |= 0x00;
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		val |= 0x01;
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		val |= 0x02;
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		val |= 0x03;
		break;
	}
	ret = imx179_write(sd, 0x0101, val);

	sensor->video.hvflip_mode = par->hvflip;
	imx179_attr_set(sd, wsize);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int imx179_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int imx179_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	return ret;
}

static int imx179_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	return ret;
}
#endif /* SENSOR_EXPO */

static int imx179_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = imx179_write_array(sd, imx179_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		imx179_setting_select(sd, 1);
		imx179_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		imx179_setting_select(sd, 0);
		imx179_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int imx179_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	/* struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg; */
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = imx179_write_array(sd, wsize->regs);
	ret = imx179_write_array(sd, imx179_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int imx179_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = imx179_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = imx179_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = imx179_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = imx179_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = imx179_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = imx179_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = imx179_write_array(sd, imx179_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = imx179_write_array(sd, imx179_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = imx179_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = imx179_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = imx179_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = imx179_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = imx179_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = imx179_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = imx179_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops imx179_core_ops = {
	.g_chip_ident = imx179_g_chip_ident,
	.reset = imx179_reset,
	.init = imx179_init,
	.g_register = imx179_g_register,
	.s_register = imx179_s_register,
};

static struct tx_isp_subdev_video_ops imx179_video_ops = {
	.s_stream = imx179_s_stream,
};

static struct tx_isp_subdev_sensor_ops imx179_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = imx179_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops imx179_ops = {
	.core = &imx179_core_ops,
	.video = &imx179_video_ops,
	.sensor = &imx179_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "imx179",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int imx179_probe(struct i2c_client *client,
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
	sensor->video.attr = &imx179_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &imx179_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->imx179\n");

	return 0;
}

static int imx179_remove(struct i2c_client *client)
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

static const struct i2c_device_id imx179_id[] = {
	{"imx179", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, imx179_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver imx179_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "imx179",
	},
	.probe          = imx179_probe,
	.remove         = imx179_remove,
	.id_table       = imx179_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_imx179(void) {
	return private_i2c_add_driver(&imx179_driver);
}

static __exit void exit_imx179(void) {
	private_i2c_del_driver(&imx179_driver);
}

module_init(init_imx179);
module_exit(exit_imx179);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_imx179(void) {
	return private_i2c_add_driver(&imx179_driver);
}

static void exit_first_imx179(void) {
	private_i2c_del_driver(&imx179_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "imx179",
	.i2c_addr = 0x10,
	.width = 3280,
	.height = 2464,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_imx179,
	.exit_sensor = exit_first_imx179
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for Sony imx179 sensor");
MODULE_LICENSE("GPL");

