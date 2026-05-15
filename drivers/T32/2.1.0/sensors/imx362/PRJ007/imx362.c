/*
* imx362.c
*
* Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* @Settings:
* sboot        resolution      fps       interface     mode    raw     max_fps		dvdd
*   0           3840x2160      30          mipi       linear    10      30
*   1           1920*1080      60          mipi       linear    10      60
*   2           3840x2160      30        mipi_4lane   linear    10      30
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
#define SENSOR_VERSION  "H20250618a"

// #define SENSOR_TEST

//#define SENSOR_I2C_REG_8BIT   /**< 选择Sensor寄存器地址位宽(8bit/16bit) */
#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
// #define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
// #define SENSOR_HCG
// #define SENSOR_HCG_SHORT

#define SENSOR_CHIP_ID_H    (0x03)
#define SENSOR_CHIP_ID_L    (0x62)
#define SENSOR_OUTPUT_MIN_FPS 1
#define SENSOR_MCLK 24000000
// #define AGAIN_MAX_DB 0x64
// #define DGAIN_MAX_DB 0x64
// #define LOG2_GAIN_SHIFT 16

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

struct tx_isp_sensor_attribute imx362_attr;

/*
* The part of driver maybe modify about different sensor and different board.
*/
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut imx362_again_lut[] = {
	{0,0},
	{1,185},
	{2,370},
	{3,556},
	{4,742},
	{5,928},
	{6,1115},
	{7,1302},
	{8,1489},
	{9,1677},
	{10,1865},
	{11,2053},
	{12,2242},
	{13,2432},
	{14,2621},
	{15,2811},
	{16,3002},
	{17,3193},
	{18,3384},
	{19,3575},
	{20,3767},
	{21,3960},
	{22,4152},
	{23,4346},
	{24,4539},
	{25,4733},
	{26,4927},
	{27,5122},
	{28,5317},
	{29,5513},
	{30,5709},
	{31,5905},
	{32,6102},
	{33,6299},
	{34,6497},
	{35,6695},
	{36,6893},
	{37,7092},
	{38,7291},
	{39,7491},
	{40,7691},
	{41,7892},
	{42,8093},
	{43,8294},
	{44,8496},
	{45,8698},
	{46,8901},
	{47,9104},
	{48,9307},
	{49,9511},
	{50,9716},
	{51,9921},
	{52,10126},
	{53,10332},
	{54,10538},
	{55,10745},
	{56,10952},
	{57,11159},
	{58,11367},
	{59,11576},
	{60,11785},
	{61,11994},
	{62,12204},
	{63,12414},
	{64,12625},
	{65,12836},
	{66,13048},
	{67,13260},
	{68,13473},
	{69,13686},
	{70,13900},
	{71,14114},
	{72,14329},
	{73,14544},
	{74,14760},
	{75,14976},
	{76,15192},
	{77,15409},
	{78,15627},
	{79,15845},
	{80,16064},
	{81,16283},
	{82,16502},
	{83,16723},
	{84,16943},
	{85,17164},
	{86,17386},
	{87,17608},
	{88,17831},
	{89,18054},
	{90,18278},
	{91,18502},
	{92,18727},
	{93,18953},
	{94,19179},
	{95,19405},
	{96,19632},
	{97,19860},
	{98,20088},
	{99,20316},
	{100,20546},
	{101,20775},
	{102,21006},
	{103,21236},
	{104,21468},
	{105,21700},
	{106,21933},
	{107,22166},
	{108,22399},
	{109,22634},
	{110,22869},
	{111,23104},
	{112,23340},
	{113,23577},
	{114,23814},
	{115,24052},
	{116,24290},
	{117,24530},
	{118,24769},
	{119,25009},
	{120,25250},
	{121,25492},
	{122,25734},
	{123,25977},
	{124,26220},
	{125,26464},
	{126,26709},
	{127,26954},
	{128,27200},
	{129,27446},
	{130,27694},
	{131,27941},
	{132,28190},
	{133,28439},
	{134,28689},
	{135,28939},
	{136,29190},
	{137,29442},
	{138,29695},
	{139,29948},
	{140,30202},
	{141,30456},
	{142,30711},
	{143,30967},
	{144,31224},
	{145,31481},
	{146,31739},
	{147,31998},
	{148,32257},
	{149,32517},
	{150,32778},
	{151,33040},
	{152,33302},
	{153,33565},
	{154,33829},
	{155,34093},
	{156,34358},
	{157,34624},
	{158,34891},
	{159,35158},
	{160,35427},
	{161,35696},
	{162,35965},
	{163,36236},
	{164,36507},
	{165,36779},
	{166,37052},
	{167,37326},
	{168,37600},
	{169,37876},
	{170,38152},
	{171,38428},
	{172,38706},
	{173,38985},
	{174,39264},
	{175,39544},
	{176,39825},
	{177,40107},
	{178,40390},
	{179,40673},
	{180,40957},
	{181,41243},
	{182,41529},
	{183,41816},
	{184,42103},
	{185,42392},
	{186,42682},
	{187,42972},
	{188,43264},
	{189,43556},
	{190,43849},
	{191,44143},
	{192,44438},
	{193,44734},
	{194,45031},
	{195,45329},
	{196,45627},
	{197,45927},
	{198,46228},
	{199,46529},
	{200,46832},
	{201,47135},
	{202,47440},
	{203,47745},
	{204,48052},
	{205,48359},
	{206,48668},
	{207,48977},
	{208,49288},
	{209,49599},
	{210,49912},
	{211,50226},
	{212,50540},
	{213,50856},
	{214,51173},
	{215,51490},
	{216,51809},
	{217,52129},
	{218,52450},
	{219,52772},
	{220,53096},
	{221,53420},
	{222,53745},
	{223,54072},
	{224,54400},
	{225,54729},
	{226,55059},
	{227,55390},
	{228,55722},
	{229,56056},
	{230,56390},
	{231,56726},
	{232,57063},
	{233,57402},
	{234,57741},
	{235,58082},
	{236,58424},
	{237,58767},
	{238,59111},
	{239,59457},
	{240,59804},
	{241,60152},
	{242,60502},
	{243,60853},
	{244,61205},
	{245,61558},
	{246,61913},
	{247,62269},
	{248,62627},
	{249,62985},
	{250,63346},
	{251,63707},
	{252,64070},
	{253,64434},
	{254,64800},
	{255,65167},
	{256,65536},
	{257,65906},
	{258,66278},
	{259,66651},
	{260,67025},
	{261,67401},
	{262,67778},
	{263,68157},
	{264,68538},
	{265,68920},
	{266,69303},
	{267,69688},
	{268,70075},
	{269,70463},
	{270,70853},
	{271,71245},
	{272,71638},
	{273,72033},
	{274,72429},
	{275,72827},
	{276,73227},
	{277,73629},
	{278,74032},
	{279,74437},
	{280,74843},
	{281,75252},
	{282,75662},
	{283,76074},
	{284,76488},
	{285,76903},
	{286,77321},
	{287,77740},
	{288,78161},
	{289,78584},
	{290,79009},
	{291,79436},
	{292,79865},
	{293,80296},
	{294,80728},
	{295,81163},
	{296,81600},
	{297,82038},
	{298,82479},
	{299,82922},
	{300,83367},
	{301,83814},
	{302,84263},
	{303,84715},
	{304,85168},
	{305,85624},
	{306,86082},
	{307,86542},
	{308,87004},
	{309,87469},
	{310,87935},
	{311,88405},
	{312,88876},
	{313,89350},
	{314,89826},
	{315,90305},
	{316,90786},
	{317,91270},
	{318,91756},
	{319,92245},
	{320,92736},
	{321,93230},
	{322,93726},
	{323,94225},
	{324,94726},
	{325,95231},
	{326,95738},
	{327,96247},
	{328,96760},
	{329,97275},
	{330,97793},
	{331,98314},
	{332,98838},
	{333,99365},
	{334,99894},
	{335,100427},
	{336,100963},
	{337,101501},
	{338,102043},
	{339,102588},
	{340,103136},
	{341,103688},
	{342,104242},
	{343,104800},
	{344,105361},
	{345,105926},
	{346,106493},
	{347,107065},
	{348,107639},
	{349,108218},
	{350,108800},
	{351,109385},
	{352,109974},
	{353,110567},
	{354,111163},
	{355,111764},
	{356,112368},
	{357,112976},
	{358,113588},
	{359,114204},
	{360,114824},
	{361,115448},
	{362,116076},
	{363,116709},
	{364,117345},
	{365,117986},
	{366,118632},
	{367,119281},
	{368,119936},
	{369,120595},
	{370,121258},
	{371,121926},
	{372,122599},
	{373,123277},
	{374,123960},
	{375,124647},
	{376,125340},
	{377,126038},
	{378,126741},
	{379,127449},
	{380,128163},
	{381,128882},
	{382,129606},
	{383,130336},
	{384,131072},
	{385,131814},
	{386,132561},
	{387,133314},
	{388,134074},
	{389,134839},
	{390,135611},
	{391,136389},
	{392,137174},
	{393,137965},
	{394,138763},
	{395,139568},
	{396,140379},
	{397,141198},
	{398,142024},
	{399,142857},
	{400,143697},
	{401,144545},
	{402,145401},
	{403,146264},
	{404,147136},
	{405,148015},
	{406,148903},
	{407,149799},
	{408,150704},
	{409,151618},
	{410,152540},
	{411,153471},
	{412,154412},
	{413,155362},
	{414,156322},
	{415,157292},
	{416,158272},
	{417,159262},
	{418,160262},
	{419,161274},
	{420,162296},
	{421,163329},
	{422,164374},
	{423,165430},
	{424,166499},
	{425,167579},
	{426,168672},
	{427,169778},
	{428,170897},
	{429,172029},
	{430,173175},
	{431,174336},
	{432,175510},
	{433,176699},
	{434,177904},
	{435,179124},
	{436,180360},
	{437,181612},
	{438,182881},
	{439,184168},
	{440,185472},
	{441,186794},
	{442,188135},
	{443,189496},
	{444,190876},
	{445,192277},
	{446,193699},
	{447,195142},
	{448,196608},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int imx362_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = imx362_again_lut;
	while (lut->gain <= imx362_attr.again.max) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == imx362_attr.again.max) && (isp_gain >= lut->gain)) {
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
unsigned int imx362_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = imx362_again_lut;
	while(lut->gain <= imx362_attr.again_short.max) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == imx362_attr.again_short.max) && (isp_gain >= lut->gain)) {
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

unsigned int imx362_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

#ifdef SENSOR_HCG
unsigned int imx362_alloc_hcg(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG */

#ifdef SENSOR_HCG_SHORT
unsigned int imx362_alloc_hcg_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_hcg)
{

	return isp_gain;
}
#endif /* SENSOR_HCG_SHORT */

struct tx_isp_mipi_bus imx362_mipi_4K_30fps = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 1480,
	.lans = 2,
	.settle_time_apative_en = 0,
	.image_twidth = 3840,
	.image_theight = 2160,
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
	.mipi_sc.data_type_en = 1,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus imx362_mipi_4K_30fps_4lane = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 720,
	.lans = 4,
	.settle_time_apative_en = 0,
	.image_twidth = 3840,
	.image_theight = 2160,
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
	.mipi_sc.data_type_en = 1,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus imx362_mipi_1080P_60fps={
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 798,
	.lans = 2,
	.settle_time_apative_en = 0,
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
	.mipi_sc.data_type_en = 1,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute imx362_attr = {
	.name = "imx362",
	.chip_id = 0x0362,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x10,
	.sensor_ctrl.alloc_again = imx362_alloc_again,
	.sensor_ctrl.alloc_dgain = imx362_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = imx362_alloc_again_short,
#endif /* SENSOR_WDR_2_FRAME */

	.dgain = {0},
	.dgain_short = {0},
};

static struct regval_list imx362_init_regs_3840_2160_30fps_mipi[] = {
	{0x0136,0x18},
	{0x0137,0x00},
	{0x31A3,0x00},
	{0x45BF,0x00},
	{0x5812,0x04},
	{0x5813,0x04},
	{0x58D0,0x08},
	{0x5F20,0x01},
	{0x5FF0,0x00},
	{0x5FF1,0xFE},
	{0x5FF2,0x00},
	{0x5FF3,0x52},
	{0x720A,0x24},
	{0x720B,0x89},
	{0x720C,0x85},
	{0x720D,0xA1},
	{0x720E,0x6E},
	{0x729C,0x59},
	{0x72E8,0x96},
	{0x72E9,0x59},
	{0x72EA,0x65},
	{0x72FB,0x2C},
	{0x737E,0x02},
	{0x737F,0x30},
	{0x7380,0x28},
	{0x7381,0x00},
	{0x7383,0x02},
	{0x7384,0x00},
	{0x7385,0x00},
	{0x74CC,0x00},
	{0x74CD,0x55},
	{0x74D2,0x00},
	{0x74D3,0x52},
	{0x74DA,0x00},
	{0x74DB,0xFE},
	{0x793D,0x00},
	{0x9333,0x03},
	{0x9334,0x04},
	{0x9335,0x05},
	{0x9346,0x96},
	{0x934A,0x8C},
	{0x9352,0xAA},
	{0xB0B6,0x05},
	{0xB0B7,0x05},
	{0xB0B9,0x05},
	{0xBC88,0x06},
	{0xBC89,0xD8},
	{0x30F2,0x01},
	{0x0101,0x00},
	{0x0112,0x0A},
	{0x0113,0x0A},
	{0x0114,0x01},
	{0x0220,0x00},
	{0x0221,0x11},
	{0x0340,0x08}, // VTS
	{0x0341,0xD4},
	{0x0342,0x15}, // HTS
	{0x0343,0xA8},
	{0x0381,0x01},
	{0x0383,0x01},
	{0x0385,0x01},
	{0x0387,0x01},
	{0x0900,0x00},
	{0x0901,0x11},
	{0x30F4,0x02},
	{0x30F5,0xBC},
	{0x30F6,0x01},
	{0x30F7,0xB8},
	{0x31A0,0x02},
	{0x31A5,0x00},
	{0x31A6,0x00},
	{0x560F,0xC8},
	{0x0344,0x00},
	{0x0345,0x60},
	{0x0346,0x01},
	{0x0347,0xb0},
	{0x0348,0x0F},
	{0x0349,0x5F},
	{0x034A,0x0A},
	{0x034B,0x1F},
	{0x034C,0x0F},
	{0x034D,0x00},
	{0x034E,0x08},
	{0x034F,0x70},
	{0x0408,0x00},
	{0x0409,0x00},
	{0x040A,0x00},
	{0x040B,0x00},
	{0x040C,0x0F},
	{0x040D,0x00},
	{0x040E,0x08},
	{0x040F,0x70},
	{0x0301,0x03},
	{0x0303,0x02},
	{0x0305,0x04},
	{0x0306,0x00},
	{0x0307,0x5E},
	{0x0309,0x0A},
	{0x030B,0x01},
	{0x030D,0x04},
	{0x030E,0x00},
	{0x030F,0xE6},
	{0x0310,0x01},
	{0x0202,0x08},	//COARSE_INTEG_TIME
	{0x0203,0x00},
	{0x0224,0x01},	//ST_COARSE_INTEG_TIME
	{0x0225,0xF4},
	{0x0204,0x00},	//ANA_GAIN_GLOBAL
	{0x0205,0x00},
	{0x0216,0x00},
	{0x0217,0x00},
	{0x020E,0x01},	//DIG_GAIN_GLOBAL
	{0x020F,0x00},
	{0x0226,0x00},
	{0x0227,0x00},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00}, /* END MARKER */
};


static struct regval_list imx362_init_regs_3840_2160_30fps_mipi_4lane[] = {
	{0x0136,0x18},
	{0x0137,0x00},
	{0x31A3,0x00},
	{0x45BF,0x00},
	{0x5812,0x04},
	{0x5813,0x04},
	{0x58D0,0x08},
	{0x5F20,0x01},
	{0x5FF0,0x00},
	{0x5FF1,0xFE},
	{0x5FF2,0x00},
	{0x5FF3,0x52},
	{0x720A,0x24},
	{0x720B,0x89},
	{0x720C,0x85},
	{0x720D,0xA1},
	{0x720E,0x6E},
	{0x729C,0x59},
	{0x72E8,0x96},
	{0x72E9,0x59},
	{0x72EA,0x65},
	{0x72FB,0x2C},
	{0x737E,0x02},
	{0x737F,0x30},
	{0x7380,0x28},
	{0x7381,0x00},
	{0x7383,0x02},
	{0x7384,0x00},
	{0x7385,0x00},
	{0x74CC,0x00},
	{0x74CD,0x55},
	{0x74D2,0x00},
	{0x74D3,0x52},
	{0x74DA,0x00},
	{0x74DB,0xFE},
	{0x793D,0x00},
	{0x9333,0x03},
	{0x9334,0x04},
	{0x9335,0x05},
	{0x9346,0x96},
	{0x934A,0x8C},
	{0x9352,0xAA},
	{0xB0B6,0x05},
	{0xB0B7,0x05},
	{0xB0B9,0x05},
	{0xBC88,0x06},
	{0xBC89,0xD8},
	{0x30F2,0x01},
	{0x0101,0x00},
	{0x0112,0x0A},
	{0x0113,0x0A},
	{0x0114,0x03},
	{0x0220,0x00},
	{0x0221,0x11},
	{0x0340,0x08}, //VTS
	{0x0341,0xD4},
	{0x0342,0x15}, //HTS
	{0x0343,0xA8},
	{0x0381,0x01},
	{0x0383,0x01},
	{0x0385,0x01},
	{0x0387,0x01},
	{0x0900,0x00},
	{0x0901,0x11},
	{0x30F4,0x02},
	{0x30F5,0xBC},
	{0x30F6,0x01},
	{0x30F7,0xB8},
	{0x31A0,0x02},
	{0x31A5,0x00},
	{0x31A6,0x00},
	{0x560F,0xC8},
	{0x0344,0x00},
	{0x0345,0x60},
	{0x0346,0x01},
	{0x0347,0xb0},
	{0x0348,0x0F},
	{0x0349,0x5F},
	{0x034A,0x0A},
	{0x034B,0x1F},
	{0x034C,0x0F},
	{0x034D,0x00},
	{0x034E,0x08},
	{0x034F,0x70},
	{0x0408,0x00},
	{0x0409,0x00},
	{0x040A,0x00},
	{0x040B,0x00},
	{0x040C,0x0F},
	{0x040D,0x00},
	{0x040E,0x08},
	{0x040F,0x70},
	{0x0301,0x03},
	{0x0303,0x02},
	{0x0305,0x04},
	{0x0306,0x00},
	{0x0307,0x5E},
	{0x0309,0x0A},
	{0x030B,0x01},
	{0x030D,0x04},
	{0x030E,0x00},
	{0x030F,0x78},
	{0x0310,0x01},
	{0x0202,0x08},	//COARSE_INTEG_TIME
	{0x0203,0x00},
	{0x0224,0x01},	//ST_COARSE_INTEG_TIME
	{0x0225,0xF4},
	{0x0204,0x00},	//ANA_GAIN_GLOBAL
	{0x0205,0x00},
	{0x0216,0x00},
	{0x0217,0x00},
	{0x020E,0x01},	//DIG_GAIN_GLOBAL
	{0x020F,0x00},
	{0x0226,0x00},
	{0x0227,0x00},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00}, /* END MARKER */
};


static struct regval_list imx362_init_regs_1920_1080_60fps_mipi[] = {
	{0x0136,0x18},
	{0x0137,0x00},
	{0x31A3,0x00},
	{0x45BF,0x00},
	{0x5812,0x04},
	{0x5813,0x04},
	{0x58D0,0x08},
	{0x5F20,0x01},
	{0x5FF0,0x00},
	{0x5FF1,0xFE},
	{0x5FF2,0x00},
	{0x5FF3,0x52},
	{0x720A,0x24},
	{0x720B,0x89},
	{0x720C,0x85},
	{0x720D,0xA1},
	{0x720E,0x6E},
	{0x729C,0x59},
	{0x72E8,0x96},
	{0x72E9,0x59},
	{0x72EA,0x65},
	{0x72FB,0x2C},
	{0x737E,0x02},
	{0x737F,0x30},
	{0x7380,0x28},
	{0x7381,0x00},
	{0x7383,0x02},
	{0x7384,0x00},
	{0x7385,0x00},
	{0x74CC,0x00},
	{0x74CD,0x55},
	{0x74D2,0x00},
	{0x74D3,0x52},
	{0x74DA,0x00},
	{0x74DB,0xFE},
	{0x793D,0x00},
	{0x9333,0x03},
	{0x9334,0x04},
	{0x9335,0x05},
	{0x9346,0x96},
	{0x934A,0x8C},
	{0x9352,0xAA},
	{0xB0B6,0x05},
	{0xB0B7,0x05},
	{0xB0B9,0x05},
	{0xBC88,0x06},
	{0xBC89,0xD8},
	{0x30F2,0x01},
	{0x0101,0x00},
	{0x0112,0x0A},
	{0x0113,0x0A},
	{0x0114,0x01},
	{0x0220,0x00},
	{0x0221,0x11},
	{0x0340,0x04}, // vts = 0x4cc = 1228
	{0x0341,0xCC},
	{0x0342,0x0A}, // hts = 0xaf0 = 2800
	{0x0343,0xF0},
	{0x0381,0x01},
	{0x0383,0x01},
	{0x0385,0x01},
	{0x0387,0x01},
	{0x0900,0x01},
	{0x0901,0x22},
	{0x30F4,0x01},
	{0x30F5,0xCC},
	{0x30F6,0x01},
	{0x30F7,0xEA},
	{0x31A0,0x02},
	{0x31A5,0x00},
	{0x31A6,0x00},
	{0x560F,0xC8},
	{0x0344,0x00},
	{0x0345,0x60},
	{0x0346,0x01},
	{0x0347,0xb0},
	{0x0348,0x0F},
	{0x0349,0x5F},
	{0x034A,0x0A},
	{0x034B,0x1F},
	{0x034C,0x07},
	{0x034D,0x80},
	{0x034E,0x04},
	{0x034F,0x38},
	{0x0408,0x00},
	{0x0409,0x00},
	{0x040A,0x00},
	{0x040B,0x00},
	{0x040C,0x07},
	{0x040D,0x80},
	{0x040E,0x04},
	{0x040F,0x38},
	{0x0301,0x05},
	{0x0303,0x02},
	{0x0305,0x04},
	{0x0306,0x00},
	{0x0307,0x56},
	{0x0309,0x0A},
	{0x030B,0x02},
	{0x030D,0x04},
	{0x030E,0x01},
	{0x030F,0x0A},
	{0x0310,0x01},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00}, /* END MARKER */
};

/*
* the order of the imx362_win_sizes is [full_resolution, preview_resolution].
*/
static struct tx_isp_sensor_win_setting imx362_win_sizes[] = {
	{
		.width          = 3840,
		.height         = 2160,
		.fps            = 30 << 16 | 1,
		// .mbus_code      = TISP_VI_FMT_SGRBG10_1X10,
		.mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = imx362_init_regs_3840_2160_30fps_mipi,
	},
	{
		.width          = 1920,
		.height         = 1080,
		.fps            = 60 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = imx362_init_regs_1920_1080_60fps_mipi,
	},
		{
		.width          = 3840,
		.height         = 2160,
		.fps            = 30 << 16 | 1,
		.mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace     = TISP_COLORSPACE_SRGB,
		.regs           = imx362_init_regs_3840_2160_30fps_mipi_4lane,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &imx362_win_sizes[0];

/*
* the part of driver was fixed.
*/

static struct regval_list imx362_stream_on_mipi[] = {
	// {0x3000, 0x00},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

static struct regval_list imx362_stream_off_mipi[] = {
	// {0x3000, 0x01},
	{ SENSOR_REG_END, 0x00 }, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int imx362_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int imx362_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int imx362_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = imx362_read(sd, vals->reg_num, &val);
			/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int imx362_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = imx362_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int imx362_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int imx362_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int imx362_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = imx362_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int imx362_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = imx362_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int imx362_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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

static int imx362_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int imx362_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &imx362_win_sizes[0];
		imx362_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		imx362_attr.again.value = 0;
		imx362_attr.again.max = 196608;
		imx362_attr.again.min = 0;
		imx362_attr.again.apply_delay = 2;

		imx362_attr.integration_time.value = 0x424;
		imx362_attr.integration_time.max = 2260 - 16;
		imx362_attr.integration_time.min = 1;
		imx362_attr.integration_time.apply_delay = 2;

		imx362_attr.total_width = 5544;
		imx362_attr.total_height = 2260;

		imx362_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		imx362_attr.hcg.base_gain = ;
		imx362_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

		memcpy((void *)(&(imx362_attr.mipi)), (void *)(&imx362_mipi_4K_30fps), sizeof(imx362_attr.mipi));
		break;
	case 1:
		wsize = &imx362_win_sizes[1];
		imx362_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		imx362_attr.again.value = 0;
		imx362_attr.again.max = 196608;
		imx362_attr.again.min = 0;
		imx362_attr.again.apply_delay = 2;

		imx362_attr.integration_time.value = 0x424;
		imx362_attr.integration_time.max = 1228 - 32;
		imx362_attr.integration_time.min = 1;
		imx362_attr.integration_time.apply_delay = 2;

		imx362_attr.total_width = 2800;
		imx362_attr.total_height = 1228;

		imx362_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		imx362_attr.hcg.base_gain = ;
		imx362_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

		memcpy((void *)(&(imx362_attr.mipi)), (void *)(&imx362_mipi_1080P_60fps), sizeof(imx362_attr.mipi));
		break;
	case 2:
		wsize = &imx362_win_sizes[2];
		imx362_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;

		imx362_attr.again.value = 0;
		imx362_attr.again.max = 196608;
		imx362_attr.again.min = 0;
		imx362_attr.again.apply_delay = 2;

		imx362_attr.integration_time.value = 0x424;
		imx362_attr.integration_time.max = 2260 - 16;
		imx362_attr.integration_time.min = 1;
		imx362_attr.integration_time.apply_delay = 2;

		imx362_attr.total_width = 5544;
		imx362_attr.total_height = 2260;

		imx362_attr.expo_fs = 1;

#ifdef SENSOR_HCG
		imx362_attr.hcg.base_gain = ;
		imx362_attr.hcg.apply_delay = ;
#endif /* SENSOR_HCG */

		memcpy((void *)(&(imx362_attr.mipi)), (void *)(&imx362_mipi_4K_30fps_4lane), sizeof(imx362_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
		break;
	}

	return ret;
}

static int imx362_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	imx362_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		imx362_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		imx362_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		imx362_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		imx362_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		imx362_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	ret = imx362_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.fps.value = wsize->fps;
	sensor->video.fps.max = wsize->fps;
	sensor->video.fps.apply_delay = 1;
	imx362_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int imx362_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = imx362_read(sd, 0x0016, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = imx362_read(sd, 0x0017, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int imx362_g_chip_ident(struct tx_isp_subdev *sd,
								struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	imx362_attr_check(sd);
#ifndef ZRT_SENSOR_WITHOUT_INIT
	if (info->rst_gpio != -1) {
		ret = private_gpio_request(info->rst_gpio, "imx362_reset");
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
		ret = private_gpio_request(info->pwdn_gpio, "imx362_pwdn");
		if (!ret) {
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = imx362_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an imx362 chip.\n",
				client->addr, client->adapter->name);
		return ret;
	}
#endif /* ZRT_SENSOR_WITHOUT_INIT */

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", imx362_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "imx362", sizeof("imx362"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int imx362_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int imx362_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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
		ret = imx362_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int imx362_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = imx362_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int imx362_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	imx362_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int imx362_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
#ifndef ZRT_SENSOR_WITHOUT_INIT
			ret = imx362_write_array(sd, wsize->regs);
#endif /* ZRT_SENSOR_WITHOUT_INIT */
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = imx362_write_array(sd, imx362_stream_on_mipi);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("imx362 stream on\n");
		}

	} else {
		ret = imx362_write_array(sd, imx362_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("imx362 stream off\n");
	}

	return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int imx362_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret = imx362_write(sd, 0x0104, 0x01);
	ret = imx362_write(sd, 0x0350, 0x00);
	ret = imx362_write(sd, 0x0203, (unsigned char)(it & 0xff));
	ret += imx362_write(sd, 0x0202, (unsigned char)((it >> 8) & 0xff));

	ret += imx362_write(sd, 0x0205, (unsigned char)(again & 0xff));
	ret += imx362_write(sd, 0x0204, (unsigned char)(((again >> 8) & 0xff)));

	ret = imx362_write(sd, 0x0104, 0x00);

	return ret;
}
#else
static int imx362_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	unsigned short shs = 0;
	unsigned short vmax = 0;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	vmax = imx362_attr.total_height;
	shs = it;

	ret = imx362_write(sd, 0x0104, 0x01);
	ret = imx362_write(sd, 0x0350, 0x01);
	ret = imx362_write(sd, 0x0203, (unsigned char)(shs & 0xff));
	ret += imx362_write(sd, 0x0202, (unsigned char)((shs >> 8) & 0xff));

	ret = imx362_write(sd, 0x0104, 0x00);
	if (0 != ret)
	{
		ISP_ERROR("err: imx362_write err\n");
	}

	return ret;
}

static int imx362_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int again = (value & 0xffff0000) >> 16;

	ret = imx362_write(sd, 0x0104, 0x01);
	ret += imx362_write(sd, 0x0205, (unsigned char)(value & 0xff));
	ret += imx362_write(sd, 0x0204, (unsigned char)(((value >> 8) & 0xff)));
	ret = imx362_write(sd, 0x0104, 0x00);
	if (ret < 0)
	{
		ISP_ERROR("err: imx362_write err\n");
	}

	return ret;
}
#endif /* SENSOR_EXPO */

static int imx362_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx362_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx362_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
		ret = imx362_attr_set(sd, wsize);
	}

	return ret;
}

static int imx362_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = 5544 * 2260 * 30;
		max_fps = 30;
		break;
	case 1:
		sclk = 2800 * 1228 * 60;
		max_fps = 60;
		break;
	case 2:
		sclk = 5544 * 2260 * 30;
		max_fps = 30;
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
	ret += imx362_read(sd, 0x0342, &val);
	hts = val << 8;
	val = 0;
	ret += imx362_read(sd, 0x0343, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: imx362 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += imx362_write(sd, 0x0340, (unsigned char)((vts & 0xff00) >> 8));
	ret += imx362_write(sd, 0x0341, (unsigned char)(vts & 0xff));

	if (0 != ret) {
		ISP_ERROR("err: imx362_write err\n");
		return ret;
	}

	sensor->video.fps.max = fps;
	sensor->video.fps.value = fps;

#ifndef SENSOR_WDR_2_FRAME
	/* Linear mode */
	sensor->video.attr->total_height = vts;
	sensor->video.attr->integration_time.max = vts - 16;
#else
	/* WDR mode */
	switch (info->default_boot) {
	case 0:
		sensor->video.attr->total_height = vts;
		sensor->video.attr->integration_time.max = vts - 9;
		break;
	case 1:
		sensor->video.attr->total_height = vts;
		sensor->video.attr->integration_time.max = vts - 9 - 232;
		sensor->video.attr->integration_time_short.max = 232;
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

static int imx362_set_hvflip(struct tx_isp_subdev *sd, void *arg)
{
#ifdef SENSOR_MIR_FLIP
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_hvflip_par *par = (struct tx_isp_hvflip_par *)arg;
	int ret = ISP_SUCCESS;
	uint8_t val;

	par->drop_frame = 0;
	par->reset = 0;

	imx362_write(sd, 0x0100, 0x00);

	switch(par->hvflip) {
	case TX_ISP_SENSOR_HVFLIP_NOMAL:
		val = 0x00;
		sensor->video.mbus.code =TISP_VI_FMT_SGRBG10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_HFLIP:
		val = 0x01;
		sensor->video.mbus.code =TISP_VI_FMT_SRGGB10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_VFLIP:
		val = 0x02;
		sensor->video.mbus.code =TISP_VI_FMT_SBGGR10_1X10;
		break;
	case TX_ISP_SENSOR_HVFLIP_HVFLIP:
		val = 0x03;
		sensor->video.mbus.code =TISP_VI_FMT_SGBRG10_1X10;
		break;
	}
	ret = imx362_write(sd, 0x0101, val);

	sensor->video.mbus_change = 1;
	sensor->video.hvflip_mode = par->hvflip;
	imx362_attr_set(sd, wsize);

	imx362_write(sd, 0x0100, 0x01);

	return ret;
#else
	return -1;
#endif /* SENSOR_MIR_FLIP */
}

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int imx362_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	...;

	return ret;
}
#else
static int imx362_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	// ret += imx362_write(sd, 0x3092, (unsigned char)(value & 0xff));
	// ret += imx362_write(sd, 0x3093, (unsigned char)((value >> 8) & 0xff));

	return ret;
}

static int imx362_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	// unsigned short shs1 = 0;
	// int rhs1 = 473;

	// shs1 = rhs1 - (value << 1);
	// ret = imx362_write(sd, 0x3054, (unsigned char)(shs1 & 0xff));
	// ret += imx362_write(sd, 0x3055, (unsigned char)((shs1 >> 8) & 0xff));
	// ret += imx362_write(sd, 0x3056, (unsigned char)((shs1 >> 16) & 0x3));

	return ret;
}
#endif /* SENSOR_EXPO */

static int imx362_set_wdr_stop(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg;
	int ret = ISP_SUCCESS;

	ret = imx362_write_array(sd, imx362_stream_off_mipi);
	if (par->wdr_en == 1) {
		if (par->boot == -1) {
			info->default_boot = 1;
		}
		imx362_setting_select(sd, 1);
		imx362_attr_set(sd, wsize);
	} else if (par->wdr_en == 0) {
		if (par->boot == -1) {
			info->default_boot = 0;
		}
		imx362_setting_select(sd, 0);
		imx362_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int imx362_set_wdr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	/* struct tx_isp_wdr_par *par = (struct tx_isp_wdr_par *)arg; */
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = imx362_write_array(sd, wsize->regs);
	ret = imx362_write_array(sd, imx362_stream_on_mipi);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */

static int imx362_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = imx362_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = imx362_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = imx362_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = imx362_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = imx362_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = imx362_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = imx362_write_array(sd, imx362_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = imx362_write_array(sd, imx362_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = imx362_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_HVFLIP:
		if(arg)
			ret = imx362_set_hvflip(sd, (void *)sensor_val->value);
		break;
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = imx362_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = imx362_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = imx362_set_analog_gain_short(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = imx362_set_wdr(sd, (void *)sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = imx362_set_wdr_stop(sd, (void *)sensor_val->value);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
		break;
	}

	return ret;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops imx362_core_ops = {
	.g_chip_ident = imx362_g_chip_ident,
	.reset = imx362_reset,
	.init = imx362_init,
	.g_register = imx362_g_register,
	.s_register = imx362_s_register,
};

static struct tx_isp_subdev_video_ops imx362_video_ops = {
	.s_stream = imx362_s_stream,
};

static struct tx_isp_subdev_sensor_ops imx362_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = imx362_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops imx362_ops = {
	.core = &imx362_core_ops,
	.video = &imx362_video_ops,
	.sensor = &imx362_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
	.name = "imx362",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int imx362_probe(struct i2c_client *client,
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
	sensor->video.attr = &imx362_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &imx362_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->imx362\n");

	return 0;
}

static int imx362_remove(struct i2c_client *client)
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

static const struct i2c_device_id imx362_id[] = {
	{"imx362", 0},
	{}
};
#ifndef CONFIG_ZERATUL
MODULE_DEVICE_TABLE(i2c, imx362_id);
#endif  /* CONFIG_ZERATUL */

static struct i2c_driver imx362_driver = {
	.driver = {
#ifdef CONFIG_ZERATUL
		.owner  = NULL,
#else
		.owner  = THIS_MODULE,
#endif  /* CONFIG_ZERATUL */
		.name   = "imx362",
	},
	.probe          = imx362_probe,
	.remove         = imx362_remove,
	.id_table       = imx362_id,
};

#ifndef CONFIG_ZERATUL
static __init int init_imx362(void) {
	return private_i2c_add_driver(&imx362_driver);
}

static __exit void exit_imx362(void) {
	private_i2c_del_driver(&imx362_driver);
}

module_init(init_imx362);
module_exit(exit_imx362);
#endif  /* CONFIG_ZERATUL */

#ifdef CONFIG_ZERATUL
static int init_first_imx362(void) {
	return private_i2c_add_driver(&imx362_driver);
}

static void exit_first_imx362(void) {
	private_i2c_del_driver(&imx362_driver);
}

struct tx_isp_sensor_fast_attr sensor0 = {
	.name = "imx362",
	.i2c_addr = 0x10,
	.width = 3840,
	.height = 2160,
#ifdef SENSOR_WDR_2_FRAME
	.wdr = 1,
#else
	.wdr = 0,
#endif /* SENSOR_WDR_2_FRAME */
	.init_sensor = init_first_imx362,
	.exit_sensor = exit_first_imx362
};
#endif  /* CONFIG_ZERATUL */

MODULE_DESCRIPTION("A low-level driver for Sony imx362 sensor");
MODULE_LICENSE("GPL");
