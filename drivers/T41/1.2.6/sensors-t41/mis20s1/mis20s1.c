/*
* mis20s1.c
*
* Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* Settings:
* sboot        resolution      fps     interface	lane       mode
*   0          1920*1080       30        mipi        2  	  linear
*   1          1920*1080       30        mipi        2  	   hdr
*   2          1920*1080       60        mipi        2  	  linear
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

#define TVERSION "H20250116a"
#define SENSOR_VERSION  "H20250430a"

#define SENSOR_AGAIN_TABLE    /**< 选择Sensor AGain匹配方式(AGain表/非AGain表) */
#define SENSOR_WDR_2_FRAME    /**< WDR两帧融合 */
#define SENSOR_EXPO
#define SENSOR_MIR_FLIP         /**< 镜像翻转功能开关 */
#define MIS20S1_CHIP_ID_H	(0x20)
#define MIS20S1_CHIP_ID_L	(0xe1)
#define MIS20S1_CHIP_ID     0x20e1


#define SENSOR_MCLK 27000000
// define 30fps linear setting
#define MIS20S1_SUPPORT_RES_PCLK (74250000)
#define MIS20S1_SUPPORT_RES_BITCLCK 371
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_OUTPUT_INIT_FPS 30
#define SENSOR_INIT_30FPS_VTS  0x465
#define SENSOR_INIT_30FPS_HTS  0x898
#define SENSOR_WIDTH 1920
#define SENSOR_HEIGHT 1080
//end define

#define MIS20S1_60FPS_SUPPORT_RES_PCLK (165000000)
#define MIS20S1_60FPPS_SUPPORT_RES_BITCLCK 990
#define SENSOR_60FPS_OUTPUT_MIN_FPS 5
#define SENSOR_60FPS_OUTPUT_INIT_FPS 60
#define SENSOR_INIT_60FPS_VTS  0x4e2
#define SENSOR_INIT_60FPS_HTS  0x898

#ifdef SENSOR_WDR_2_FRAME
#define SENSOR_WDR_MCLK 27000000
#define MIS20S1_WDR_SUPPORT_RES_PCLK (165000000)
#define MIS20S1_WDR_SUPPORT_RES_BITCLCK 990
#define SENSOR_WDR_OUTPUT_MIN_FPS 5
#define SENSOR_WDR_OUTPUT_INIT_FPS 30
#define SENSOR_WDR_INIT_30FPS_VTS  0x4e2
#define SENSOR_WDR_INIT_30FPS_HTS  0x898
#define SENSOR_WDR_WIDTH 1920
#define SENSOR_WDR_HEIGHT 1080
static int wdr_line = 1000;
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

struct tx_isp_sensor_attribute mis20s1_attr;

/*
* The part of driver maybe modify about different sensor and different board.
*/
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut mis20s1_again_lut[] = {
	{0x0000, 0},
	{0x0022, 3708},
	{0x0044, 6397},
	{0x0065, 9867},
	{0x0084, 13214},
	{0x00a2, 16447},
	{0x00c0, 19573},
	{0x00dc, 22599},
	{0x00f7, 26250},
	{0x0112, 29765},
	{0x012b, 32486},
	{0x0144, 35781},
	{0x015c, 39588},
	{0x0173, 42649},
	{0x0189, 45613},
	{0x019f, 49051},
	{0x01b3, 52369},
	{0x01c7, 55574},
	{0x01db, 59182},
	{0x01ed, 62168},
	{0x01ff, 65536},
	{0x0211, 68789},
	{0x0222, 71933},
	{0x0232, 75403},
	{0x0242, 78750},
	{0x0251, 81983},
	{0x0260, 85109},
	{0x026e, 88506},
	{0x027b, 91786},
	{0x0289, 94955},
	{0x0295, 98357},
	{0x02a2, 101640},
	{0x02ae, 104813},
	{0x02b9, 108185},
	{0x02c4, 111440},
	{0x02cf, 114587},
	{0x02d9, 117905},
	{0x02e3, 121373},
	{0x02ed, 124464},
	{0x02f6, 127704},
	{0x0300, 131072},
	{0x0308, 134325},
	{0x0311, 137690},
	{0x0319, 140939},
	{0x0321, 144081},
	{0x0328, 147519},
	{0x0330, 150645},
	{0x0337, 154042},
	{0x033d, 157322},
	{0x0344, 160491},
	{0x034a, 163893},
	{0x0351, 167176},
	{0x0357, 170349},
	{0x035c, 173721},
	{0x0362, 176976},
	{0x0367, 180264},
	{0x036c, 183441},
	{0x0371, 186778},
	{0x0376, 190000},
	{0x037b, 193362},
	{0x0380, 196608},
	{0x0384, 199861},
	{0x1000, 203115},
	{0x1023, 206475},
	{0x1045, 209719},
	{0x1065, 212956},
	{0x1084, 216277},
	{0x10a3, 219578},
	{0x10c0, 222858},
	{0x10dc, 226114},
	{0x10f8, 229345},
	{0x1112, 232631},
	{0x112c, 235963},
	{0x1144, 239181},
	{0x115c, 242512},
	{0x1173, 245729},
	{0x1189, 249045},
	{0x119f, 252314},
	{0x11b4, 255600},
	{0x11c8, 258837},
	{0x11db, 262144},
	{0x11ee, 265397},
	{0x1200, 268707},
	{0x1211, 271958},
	{0x1222, 275255},
	{0x1232, 278541},
	{0x1242, 281813},
	{0x1251, 285068},
	{0x1260, 288349},
	{0x126e, 291650},
	{0x127c, 294923},
	{0x1289, 298207},
	{0x1296, 301460},
	{0x12a2, 304755},
	{0x12ae, 308012},
	{0x12b9, 311301},
	{0x12c4, 314581},
	{0x12cf, 317850},
	{0x12da, 321136},
	{0x12e4, 324403},
	{0x12ed, 327680},
	{0x12f7, 330961},
	{0x1300, 334243},
	{0x1308, 337520},
	{0x1311, 340791},
	{0x1319, 344052},
	{0x1321, 347349},
	{0x1328, 350627},
	{0x1330, 353885},
	{0x1337, 357164},
	{0x133e, 360438},
	{0x1344, 363723},
	{0x134b, 366996},
	{0x1351, 370272},
	{0x1357, 373548},
	{0x135c, 376837},
	{0x1362, 380117},
	{0x1367, 383386},
	{0x136d, 386656},
	{0x1372, 389939},
	{0x1376, 393216},
	{0x137b, 396497},
	{0x1380, 399765},
	{0x1384, 403043},
	{0x1388, 406327},
	{0x138c, 409601},
	{0x1390, 412873},
	{0x1394, 416151},
	{0x1398, 419432},
	{0x139b, 422711},
	{0x139f, 425984},
	{0x13a2, 429259},
	{0x13a5, 432542},
	{0x13a8, 435817},
	{0x13ab, 439093},
	{0x13ae, 442364},
	{0x13b1, 445644},
	{0x13b3, 448922},
	{0x13b6, 452200},
	{0x13b9, 455475},
	{0x13bb, 458752},
	{0x13bd, 462026},
	{0x13c0, 465308},
	{0x13c2, 468579},
	{0x13c4, 471857},
	{0x13c6, 475137},
	{0x13c8, 478415},
	{0x13ca, 481687},
	{0x13cc, 484968},
	{0x13cd, 488242},
	{0x13cf, 491520},
	{0x13d1, 494795},
	{0x13d2, 498073},
	{0x13d4, 501349},
	{0x13d5, 504629},
	{0x13d7, 507904},
	{0x13d8, 511180},
	{0x13d9, 514458},
	{0x13db, 517736},
	{0x13dc, 521011},
	{0x13dd, 524288},
	{0x13de, 527566},
	{0x13e0, 530840},
	{0x13e1, 534118},
	{0x13e2, 537396},
	{0x13e3, 540673},
	{0x13e4, 543948},
	{0x13e5, 547226},
	{0x13e6, 550501},
	{0x13e6, 553780},
	{0x13e7, 557056},
	{0x13e8, 560334},
	{0x13e9, 563609},
	{0x13ea, 566887},
	{0x13ea, 570162},
	{0x13eb, 573440},
	{0x13ec, 576716},
	{0x13ec, 579994},
	{0x13ed, 583270},
	{0x13ee, 586547},
	{0x13ee, 589824},
	{0x13ef, 593102},
	{0x13f0, 596378},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int mis20s1_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = mis20s1_again_lut;
	while (lut->gain <= mis20s1_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == mis20s1_attr.max_again) && (isp_gain >= lut->gain)) {
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
unsigned int mis20s1_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
	/* Analog gain table */
	struct again_lut *lut = mis20s1_again_lut;
	while(lut->gain <= mis20s1_attr.max_again_short) {
		if(isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == mis20s1_attr.max_again_short) && (isp_gain >= lut->gain)) {
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
unsigned int mis20s1_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus mis20s1_mipi_linear={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = MIS20S1_SUPPORT_RES_BITCLCK,
	.lans = 2,
	.image_twidth = SENSOR_WIDTH,
	.image_theight = SENSOR_HEIGHT,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
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
	.mipi_sc.data_type_value = RAW12,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus mis20s1_mipi_dol={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = MIS20S1_WDR_SUPPORT_RES_BITCLCK,
	.lans = 2,
	.image_twidth = SENSOR_WDR_WIDTH,
	.image_theight = SENSOR_WDR_HEIGHT,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
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
	.mipi_sc.data_type_value = RAW12,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_mipi_bus mis20s1_mipi_linear_60fps={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = MIS20S1_60FPPS_SUPPORT_RES_BITCLCK,
	.lans = 2,
	.image_twidth = SENSOR_WIDTH,
	.image_theight = SENSOR_HEIGHT,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
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
	.mipi_sc.data_type_value = RAW12,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute mis20s1_attr={
	.name = "mis20s1",
	.chip_id = MIS20S1_CHIP_ID,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 596378,//327680,//393216,
	.max_dgain = 0,
	.min_integration_time = 0,
	.min_integration_time_native = 0,
	.max_integration_time_native = SENSOR_INIT_30FPS_VTS - 4,
	.expo_fs = 1,
	.integration_time_limit = SENSOR_INIT_30FPS_VTS - 4,
	.total_width = SENSOR_INIT_30FPS_HTS,
	.total_height = SENSOR_INIT_30FPS_VTS,
	.max_integration_time = SENSOR_INIT_30FPS_VTS - 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = mis20s1_alloc_again,
	.sensor_ctrl.alloc_dgain = mis20s1_alloc_dgain,
#ifdef SENSOR_WDR_2_FRAME
	.sensor_ctrl.alloc_again_short = mis20s1_alloc_again_short,
	.max_again_short = 596378,//327680,//393216,
	.max_dgain_short = 0,
#endif /* SENSOR_WDR_2_FRAME */
};
/*Sensir revision:mis20s1
//Input clock frequency:27M
//Image output size:1920x1080
//Frame timing and frame rate: 30Fps
//System clock frequency:74.25M
//Output interface and data rate:MIPI 2Lane 445.55Mbps
//HTS = 310e/310f =0x898
//VTS = 3110/3111 =0x465
//Tline = 29.6us
//NO1_PIS2308_1920x1080_VTS1125_HTS2200_VCO1782_PCLK74P25_BITCLK445p5_ACLK297_2LANE_raw12_20241209—AD11_MIPI RAW12-寄存器删减.ini
*/
static struct regval_list mis20s1_init_regs_1920_1080_30fps_mipi[] = {
	{0x3006, 0x01},
	{SENSOR_REG_DELAY,0x01},
	{0x3006, 0x02},
	{0x3018, 0x50},
	{0x3113, 0x04},
	{0x3115, 0x3b},
	{0x3117, 0x04},
	{0x3119, 0x83},
	{0x311a, 0x01},
	{0x311b, 0xd6},
	{0x311c, 0x00},
	{0x311d, 0x19},
	{0x311e, 0x00},
	{0x311f, 0x00},
	{0x3120, 0xf8},
	{0x3121, 0x01},
	{0x3122, 0x2c},
	{0x3123, 0x12},
	{0x3205, 0xb2},
	{0x3306, 0x2f},
	{0x3307, 0x04},
	{0x3308, 0xb8},
	{0x3309, 0xb1},
	{0x330a, 0x07},
	{0x330b, 0x06},
	{0x3314, 0x25},
	{0x3502, 0x0d},
	{0x3604, 0x05},
	{0x360c, 0x05},
	{0x3612, 0x02},
	{0x3613, 0x40},

	{0x4607, 0x14},
	{0x3615, 0xa0},///
	{0x361c, 0x05},
	{0x3620, 0x05},
	{0x3624, 0x02},
	{0x3625, 0x4f},
	{0x3628, 0x05},
	{0x362c, 0x05},
	{0x3630, 0x02},
	{0x3631, 0x4f},
	{0x363d, 0x30},
	{0x363f, 0x40},
	{0x3641, 0xf0},
	{0x365c, 0x02},
	{0x365d, 0x40},
	{0x365e, 0x03},
	{0x365f, 0x00},
	{0x3660, 0x05},
	{0x367c, 0x02},
	{0x367d, 0x3c},
	{0x367f, 0xd5},
	{0x3680, 0x05},
	{0x3681, 0xec},
	{0x3688, 0x02},
	{0x3689, 0x4f},
	{0x368b, 0xd5},
	{0x368c, 0x05},
	{0x368f, 0xc0},
	{0x3690, 0x02},
	{0x3691, 0x51},
	{0x3692, 0x03},
	{0x3693, 0x64},
	{0x3694, 0x05},
	{0x36b3, 0x00},
	{0x36b5, 0x02},
	{0x36b6, 0x40},
	{0x36b7, 0x02},
	{0x36b8, 0xf0},
	{0x36b9, 0xe4},
	{0x36c2, 0x02},
	{0x36c3, 0x4f},
	{0x36c4, 0x02},
	{0x36c5, 0x5e},
	{0x3702, 0x05},
	{0x3704, 0x05},
	{0x3712, 0x06},
	{0x371a, 0x00},
	{0x371c, 0x00},
	{0x371d, 0x02},
	{0x371e, 0x6d},
	{0x3720, 0xa8},
	{0x3723, 0x02},
	{0x3724, 0x7c},
	{0x3725, 0x02},
	{0x3726, 0x3e},
	{0x3728, 0x9a},
	{0x3729, 0x02},
	{0x372a, 0x3b},
	{0x372b, 0x02},
	{0x372c, 0x3f},
	{0x372f, 0x05},
	{0x3741, 0x02},
	{0x3742, 0x3e},
	{0x3744, 0x9a},
	{0x375d, 0x02},
	{0x375e, 0x4f},
	{0x3760, 0xa0},
	{0x3903, 0x1b},
	{0x3905, 0x0f},
	{0x390a, 0x1f},
	{0x390b, 0x1f},
	{0x3a05, 0xfd},
	{0x3a06, 0x52},
	{0x3a08, 0x37},  //0x3f->0x37 修改电压优化黑点
	{0x3a1b, 0x7f},
	{0x3a1c, 0x3f},
	{0x3a1d, 0x0f},
	{0x3a21, 0x0e},
	{0x3b05, 0x3f},
	{0x3b07, 0x01},
	{0x4102, 0x13},
	{0x410e, 0x10},
	{0x410f, 0x39},
	{0x4303, 0x13},
	{0x432E, 0x00},
	{0x432F, 0x80},
	{0x4330, 0x00},
	{0x4331, 0x80},
	{0x4332, 0x00},
	{0x4333, 0x80},
	{0x4334, 0x00},
	{0x4335, 0x80},
	{0x4336, 0x13},
	{0x4361, 0x00},
	{0x4362, 0x80},
	{0x4363, 0x00},
	{0x4364, 0x80},
	{0x4365, 0x00},
	{0x4366, 0x80},
	{0x4367, 0x00},
	{0x4368, 0x80},
	{0x4402, 0x3f},
	{0x4403, 0x12},
	{0x3c1a, 0x00},
	{0x3c03, 0x06},
	{0x3c1a, 0x01},
	{0x3031, 0x0c},
	{0x3008, 0x01},
	{0x3006, 0x00},

	{SENSOR_REG_DELAY,0x01},
	{0x300c, 0x01},	//拉高IMG_EN

	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list mis20s1_init_regs_1920_1080_dol2_mipi[] = {
	//NO7_PIS2308_1920x1080_VTS1250_HTS2200_VCO1980_PCLK165_BITCLK990_ACLK396_2LANE_raw12_dol2_20250114_AD10_RAW12
	//Sensir revision:Mis20S1
	//Input clock frequency:27M
	//Image output size:1920x1080
	//Frame timing and frame rate: DOL2 30Fps
	//System clock frequency:165M
	//Output interface and data rate:MIPI 2Lane raw12 990Mbps
	//HTS = 310e/310f =0x898
	//VTS = 3110/3111 =0x4e2
	//Tline = 26.6us

	{0x3006, 0x01},
	{SENSOR_REG_DELAY, 0x01},  //delay 1ms
	{0x3006, 0x02},
	{0x310f, 0xe2},
	{0x3113, 0x04},
	{0x3115, 0x3b},
	{0x3117, 0x04},
	{0x3119, 0x83},
	{0x3120, 0x1d},
	{0x3123, 0x12},
	{0x3205, 0xb2},
	{0x3304, 0xdc},
	{0x3305, 0x72},
	{0x3312, 0xc6},
	{0x3314, 0x53},
	{0x3316, 0x7c},
	{0x3502, 0x0d},
	{0x3603, 0x77},
	{0x3604, 0x04},
	{0x3605, 0x98},
	{0x360b, 0x6d},
	{0x360c, 0x04},
	{0x360d, 0x98},
	{0x3612, 0x02},
	{0x3613, 0x3e},
	{0x3615, 0x68},
	{0x361c, 0x04},
	{0x361d, 0x98},
	{0x3620, 0x04},
	{0x3621, 0x98},
	{0x3624, 0x02},
	{0x3625, 0x3e},
	{0x3628, 0x04},
	{0x3629, 0x98},
	{0x362b, 0x14},
	{0x362c, 0x04},
	{0x362d, 0x84},
	{0x362f, 0x0a},
	{0x3630, 0x02},
	{0x3631, 0x3e},
	{0x3633, 0x50},
	{0x3634, 0x01},
	{0x3635, 0x29},
	{0x3637, 0x50},
	{0x3638, 0x01},
	{0x3639, 0x20},
	{0x363a, 0x01},
	{0x363b, 0x47},
	{0x363d, 0x6f},
	{0x363f, 0xa1},
	{0x3641, 0xc9},
	{0x365b, 0xaa},
	{0x365c, 0x02},
	{0x365d, 0x2a},
	{0x365e, 0x03},
	{0x365f, 0x04},
	{0x3660, 0x04},
	{0x3661, 0x84},
	{0x367D, 0x28},
	{0x367b, 0x96},
	{0x367c, 0x02},
	{0x367d, 0x3e},
	{0x367f, 0xf0},
	{0x3680, 0x04},
	{0x3681, 0x80},
	{0x3683, 0x63},
	{0x3685, 0x8b},
	{0x3687, 0x96},
	{0x3688, 0x02},
	{0x3689, 0x3e},
	{0x368b, 0xf0},
	{0x368c, 0x04},
	{0x368d, 0x98},
	{0x368f, 0xea},
	{0x3690, 0x02},
	{0x3691, 0x40},
	{0x3692, 0x03},
	{0x3693, 0x44},
	{0x3694, 0x04},
	{0x3695, 0x9a},
	{0x36af, 0x28},
	{0x36b1, 0x96},
	{0x36b3, 0x46},
	{0x36b5, 0xaa},
	{0x36b6, 0x02},
	{0x36b7, 0x3e},
	{0x36b8, 0x03},
	{0x36b9, 0x04},
	{0x36bb, 0x0a},
	{0x36bd, 0x5b},
	{0x36bf, 0x5b},
	{0x36c1, 0x6f},
	{0x36c2, 0x02},
	{0x36c3, 0x3e},
	{0x36c4, 0x02},
	{0x36c5, 0x52},
	{0x36df, 0x3c},
	{0x36e1, 0xc6},
	{0x36e3, 0x3c},
	{0x36e5, 0xc6},
	{0x36e7, 0x28},
	{0x36e9, 0x3c},
	{0x3702, 0x04},
	{0x3703, 0x8e},
	{0x3704, 0x04},
	{0x3705, 0x98},
	{0x3707, 0x3c},
	{0x3709, 0xc6},
	{0x370b, 0x3c},
	{0x370d, 0xc6},
	{0x3711, 0x0b},
	{0x3713, 0xc0},
	{0x371d, 0x02},
	{0x371e, 0x66},
	{0x3720, 0xb5},
	{0x3722, 0x14},
	{0x3723, 0x02},
	{0x3724, 0x7a},
	{0x3725, 0x02},
	{0x3726, 0x28},
	{0x3728, 0xa1},
	{0x3729, 0x02},
	{0x372a, 0x25},
	{0x372b, 0x02},
	{0x372c, 0x29},
	{0x372e, 0xda},
	{0x372f, 0x04},
	{0x3730, 0x5d},
	{0x3734, 0xaa},
	{0x3738, 0xda},
	{0x373c, 0xda},
	{0x3741, 0x02},
	{0x3742, 0x28},
	{0x3743, 0x02},
	{0x3744, 0xa1},
	{0x375d, 0x02},
	{0x375e, 0x3e},
	{0x3760, 0xa0},
	{0x3903, 0x1b},
	{0x3905, 0x0f},
	{0x390a, 0x1f},
	{0x390b, 0x1f},
	{0x3a05, 0xbd},
	{0x3a1b, 0x7f},
	{0x3a1c, 0x3f},
	{0x3a1d, 0x0f},
	{0x3a21, 0x0e},
	{0x3b05, 0x3f},
	{0x4102, 0x13},
	{0x410e, 0x02},
	{0x410f, 0x3a},
	{0x412a, 0x02},
	{0x412b, 0x3a},
	{0x4304, 0x3a},
	{0x4309, 0x00},
	{0x4337, 0x3a},
	{0x433c, 0x00},
	{0x3a0d, 0x0f},
	{0x4303, 0x13},
	{0x432E, 0x00},
	{0x432F, 0x80},
	{0x4330, 0x00},
	{0x4331, 0x80},
	{0x4332, 0x00},
	{0x4333, 0x80},
	{0x4334, 0x00},
	{0x4335, 0x80},
	{0x4336, 0x13},
	{0x4361, 0x00},
	{0x4362, 0x80},
	{0x4363, 0x00},
	{0x4364, 0x80},
	{0x4365, 0x00},
	{0x4366, 0x80},
	{0x4367, 0x00},
	{0x4368, 0x80},
	{0x4402, 0x12},
	{0x4403, 0x12},
	{0x3906, 0x1f},
	{0x3907, 0x1f},
	{0x3908, 0x1f},
	{0x3909, 0x1f},
	{0x390A, 0x1f},
	{0x390B, 0x1f},
	{0x390c, 0x12},
	{0x390d, 0x12},
	{0x390e, 0x12},
	{0x390f, 0x12},
	{0x3910, 0x12},
	{0x3911, 0x12},
	{0x3a1b, 0x61},
	{0x3a1c, 0x00},
	{0x3a1d, 0x7f},
	{0x3a1e, 0xdf},
	//0x3a0e,0x06  //
	{0x311c, 0x01}, //RD1_ST_PNT, when wdr, expo1 < RD1_ST_PNT-2 , wg lpf
	{0x311d, 0x95},

	{0x3031, 0x0C},
	{0x3031, 0x0C},

	{0x3008, 0x01},
	{0x3031, 0x0c},
	{0x3c1a, 0x01},
	{0x3006, 0x00},
	{SENSOR_REG_DELAY, 0x01},  //delay 1ms
	{0x300c, 0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list mis20s1_init_regs_1920_1080_60fps_mipi[] = {
	{0x3006, 0x01},
	{SENSOR_REG_DELAY, 0x01},  //delay 1ms
	{0x3006, 0x02},
	{0x310f, 0xe2},
	{0x3113, 0x04},
	{0x3115, 0x3b},
	{0x3117, 0x04},
	{0x3119, 0x83},
	{0x3120, 0x1c},
	{0x3123, 0x12},
	{0x3205, 0xb2},
	{0x3304, 0xdc},
	{0x3305, 0x72},
	{0x3312, 0xc6},
	{0x3314, 0x53},
	{0x3316, 0x7c},
	{0x3502, 0x0d},
	{0x3603, 0x77},
	{0x3604, 0x04},
	{0x3605, 0x98},
	{0x360b, 0x6d},
	{0x360c, 0x04},
	{0x360d, 0x98},
	{0x3612, 0x02},
	{0x3613, 0x3e},
	{0x3615, 0x68},
	{0x361c, 0x04},
	{0x361d, 0x98},
	{0x3620, 0x04},
	{0x3621, 0x98},
	{0x3624, 0x02},
	{0x3625, 0x3e},
	{0x3628, 0x04},
	{0x3629, 0x98},
	{0x362b, 0x14},
	{0x362c, 0x04},
	{0x362d, 0x84},
	{0x362f, 0x0a},
	{0x3630, 0x02},
	{0x3631, 0x3e},
	{0x3633, 0x50},
	{0x3634, 0x01},
	{0x3635, 0x29},
	{0x3637, 0x50},
	{0x3638, 0x01},
	{0x3639, 0x20},
	{0x363a, 0x01},
	{0x363b, 0x47},
	{0x363d, 0x6f},
	{0x363f, 0xa1},
	{0x3641, 0xc9},
	{0x365b, 0xaa},
	{0x365c, 0x02},
	{0x365d, 0x2a},
	{0x365e, 0x03},
	{0x365f, 0x04},
	{0x3660, 0x04},
	{0x3661, 0x84},
	{0x367D, 0x28},
	{0x367b, 0x96},
	{0x367c, 0x02},
	{0x367d, 0x3e},
	{0x367f, 0xf0},
	{0x3680, 0x04},
	{0x3681, 0x80},
	{0x3683, 0x63},
	{0x3685, 0x8b},
	{0x3687, 0x96},
	{0x3688, 0x02},
	{0x3689, 0x3e},
	{0x368b, 0xf0},
	{0x368c, 0x04},
	{0x368d, 0x98},
	{0x368f, 0xea},
	{0x3690, 0x02},
	{0x3691, 0x40},
	{0x3692, 0x03},
	{0x3693, 0x44},
	{0x3694, 0x04},
	{0x3695, 0x9a},
	{0x36af, 0x28},
	{0x36b1, 0x96},
	{0x36b3, 0x46},
	{0x36b5, 0xaa},
	{0x36b6, 0x02},
	{0x36b7, 0x3e},
	{0x36b8, 0x03},
	{0x36b9, 0x04},
	{0x36bb, 0x0a},
	{0x36bd, 0x5b},
	{0x36bf, 0x5b},
	{0x36c1, 0x6f},
	{0x36c2, 0x02},
	{0x36c3, 0x3e},
	{0x36c4, 0x02},
	{0x36c5, 0x52},
	{0x36df, 0x3c},
	{0x36e1, 0xc6},
	{0x36e3, 0x3c},
	{0x36e5, 0xc6},
	{0x36e7, 0x28},
	{0x36e9, 0x3c},
	{0x3702, 0x04},
	{0x3703, 0x8e},
	{0x3704, 0x04},
	{0x3705, 0x98},
	{0x3707, 0x3c},
	{0x3709, 0xc6},
	{0x370b, 0x3c},
	{0x370d, 0xc6},
	{0x3711, 0x0b},
	{0x3713, 0xc0},
	{0x371d, 0x02},
	{0x371e, 0x66},
	{0x3720, 0xb5},
	{0x3722, 0x14},
	{0x3723, 0x02},
	{0x3724, 0x7a},
	{0x3725, 0x02},
	{0x3726, 0x28},
	{0x3728, 0xa1},
	{0x3729, 0x02},
	{0x372a, 0x25},
	{0x372b, 0x02},
	{0x372c, 0x29},
	{0x372e, 0xda},
	{0x372f, 0x04},
	{0x3730, 0x5d},
	{0x3734, 0xaa},
	{0x3738, 0xda},
	{0x373c, 0xda},
	{0x3741, 0x02},
	{0x3742, 0x28},
	{0x3743, 0x02},
	{0x3744, 0xa1},
	{0x375d, 0x02},
	{0x375e, 0x3e},
	{0x3760, 0xa0},
	{0x3903, 0x1b},
	{0x3905, 0x0f},
	{0x390a, 0x1f},
	{0x390b, 0x1f},
	{0x3a05, 0xbd},
	{0x3a1b, 0x7f},
	{0x3a1c, 0x3f},
	{0x3a1d, 0x0f},
	{0x3a21, 0x0e},
	{0x3b05, 0x3f},
	{0x4102, 0x13},
	{0x410e, 0x02},
	{0x410f, 0x3a},
	{0x412a, 0x02},
	{0x412b, 0x3a},
	{0x4304, 0x3a},
	{0x4309, 0x00},
	{0x4337, 0x3a},
	{0x433c, 0x00},
	{0x3a0d, 0x0f},
	{0x4303, 0x13},
	{0x432E, 0x00},
	{0x432F, 0x80},
	{0x4330, 0x00},
	{0x4331, 0x80},
	{0x4332, 0x00},
	{0x4333, 0x80},
	{0x4334, 0x00},
	{0x4335, 0x80},
	{0x4336, 0x13},
	{0x4361, 0x00},
	{0x4362, 0x80},
	{0x4363, 0x00},
	{0x4364, 0x80},
	{0x4365, 0x00},
	{0x4366, 0x80},
	{0x4367, 0x00},
	{0x4368, 0x80},
	{0x4402, 0x12},
	{0x4403, 0x12},
	{0x3906, 0x1f},
	{0x3907, 0x1f},
	{0x3908, 0x1f},
	{0x3909, 0x1f},
	{0x390A, 0x1f},
	{0x390B, 0x1f},
	{0x390c, 0x12},
	{0x390d, 0x12},
	{0x390e, 0x12},
	{0x390f, 0x12},
	{0x3910, 0x12},
	{0x3911, 0x12},
	{0x3a1b, 0x61},
	{0x3a1c, 0x00},
	{0x3a1d, 0x7f},
	{0x3a1e, 0xdf},
	//0x3a0e,0x06  //90C高温满井不饱和 20250114 hql    需评估影响是否使用逻辑控制
	{0x3031, 0x0C},

	{0x3008, 0x01},
	{0x3031, 0x0c},
	{0x3c1a, 0x01},
	{0x3006, 0x00},
	{SENSOR_REG_DELAY, 0x01},  //delay 1ms
	{0x300c, 0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

/*
* the order of the mis20s1_win_sizes is [full_resolution, preview_resolution].
*/
static struct tx_isp_sensor_win_setting mis20s1_win_sizes[] = {
	{
		.width		= SENSOR_WIDTH,
		.height		= SENSOR_HEIGHT,
		.fps		= SENSOR_OUTPUT_INIT_FPS<< 16 | 1,
		.mbus_code	= TISP_VI_FMT_SGRBG12_1X12,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= mis20s1_init_regs_1920_1080_30fps_mipi,
	},
	{
		.width		= SENSOR_WDR_WIDTH,
		.height		= SENSOR_WDR_HEIGHT,
		.fps		= SENSOR_WDR_OUTPUT_INIT_FPS<< 16 | 1,
		.mbus_code	= TISP_VI_FMT_SGRBG12_1X12,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= mis20s1_init_regs_1920_1080_dol2_mipi,
	},
	{
		.width		= SENSOR_WIDTH,
		.height		= SENSOR_HEIGHT,
		.fps		= SENSOR_60FPS_OUTPUT_INIT_FPS << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SGRBG12_1X12,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= mis20s1_init_regs_1920_1080_60fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &mis20s1_win_sizes[0];

/*
* the part of driver was fixed.
*/

static struct regval_list mis20s1_stream_on[] = {
	{0x300c, 0x01},
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list mis20s1_stream_off[] = {
	{0x300c, 0x00},
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int mis20s1_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int mis20s1_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int mis20s1_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
		int ret;
		unsigned char val;
		while (vals->reg_num != MIS20S1_REG_END) {
				if (vals->reg_num == MIS20S1_REG_DELAY) {
						private_msleep(vals->value);
				} else {
						ret = mis20s1_read(sd, vals->reg_num, &val);
						/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
						if (ret < 0)
								return ret;
				}
				vals++;
		}

		return 0;
}
#endif

static int mis20s1_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
		int ret;
		while (vals->reg_num != MIS20S1_REG_END) {
				if (vals->reg_num == MIS20S1_REG_DELAY) {
						private_msleep(vals->value);
				} else {
						ret = mis20s1_write(sd, vals->reg_num, vals->value);
						if (ret < 0)
								return ret;
				}
				vals++;
		}

		return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int mis20s1_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int mis20s1_write(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

/*
static int mis20s1_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != MIS20S1_REG_END) {
		if (vals->reg_num == MIS20S1_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = mis20s1_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%02x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}
	return 0;
}
#endif
*/
static int mis20s1_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = mis20s1_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

static int mis20s1_clk_set(struct tx_isp_subdev *sd, struct clk *sclka, int mclk)
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
					private_clk_set_rate(sclka, 1080000000);
					break;
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

static int mis20s1_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
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

static int mis20s1_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &mis20s1_win_sizes[0];
		memcpy((void *)(&(mis20s1_attr.mipi)), (void *)(&mis20s1_mipi_linear), sizeof(mis20s1_attr.mipi));
		mis20s1_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		mis20s1_attr.min_integration_time = 1;
		mis20s1_attr.max_integration_time = SENSOR_INIT_30FPS_VTS-4;
		mis20s1_attr.total_width = SENSOR_INIT_30FPS_HTS;
		mis20s1_attr.total_height = SENSOR_INIT_30FPS_VTS;
		mis20s1_attr.integration_time_apply_delay = 2;
		mis20s1_attr.again_apply_delay = 2;
		mis20s1_attr.dgain_apply_delay = 0;
		mis20s1_attr.integration_time_limit = mis20s1_attr.max_integration_time;
		mis20s1_attr.max_integration_time_native = mis20s1_attr.max_integration_time;
		mis20s1_attr.min_integration_time_native = mis20s1_attr.min_integration_time;
		mis20s1_attr.expo_fs = 1;
		break;
	case 1:
		wsize = &mis20s1_win_sizes[1];
		memcpy((void *)(&(mis20s1_attr.mipi)), (void *)(&mis20s1_mipi_dol), sizeof(mis20s1_attr.mipi));
		mis20s1_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		mis20s1_attr.min_integration_time = 1;
		mis20s1_attr.max_integration_time =((SENSOR_WDR_INIT_30FPS_VTS-4)/17) << 4;
		mis20s1_attr.total_width = SENSOR_WDR_INIT_30FPS_HTS;
		mis20s1_attr.total_height = SENSOR_WDR_INIT_30FPS_VTS;
		mis20s1_attr.integration_time_apply_delay = 2;
		mis20s1_attr.again_apply_delay = 2;
		mis20s1_attr.dgain_apply_delay = 0;
		mis20s1_attr.integration_time_limit = mis20s1_attr.max_integration_time;
		mis20s1_attr.max_integration_time_native = mis20s1_attr.max_integration_time;
		mis20s1_attr.min_integration_time_native = mis20s1_attr.min_integration_time;
		mis20s1_attr.expo_fs = 1;
#ifdef SENSOR_WDR_2_FRAME
		mis20s1_attr.wdr_cache = wdr_line * mis20s1_attr.total_width * 2;
		mis20s1_attr.min_integration_time_short = 1;
		mis20s1_attr.max_integration_time_short = (SENSOR_WDR_INIT_30FPS_VTS-4)/17;
#endif /* SENSOR_WDR_2_FRAME */
		break;
	case 2:
		wsize = &mis20s1_win_sizes[2];
		memcpy((void *)(&(mis20s1_attr.mipi)), (void *)(&mis20s1_mipi_linear_60fps), sizeof(mis20s1_attr.mipi));
		mis20s1_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		mis20s1_attr.min_integration_time = 1;
		mis20s1_attr.max_integration_time = SENSOR_INIT_60FPS_VTS-4;
		mis20s1_attr.total_width = SENSOR_INIT_60FPS_HTS;
		mis20s1_attr.total_height = SENSOR_INIT_60FPS_VTS;
		mis20s1_attr.integration_time_apply_delay = 2;
		mis20s1_attr.again_apply_delay = 2;
		mis20s1_attr.dgain_apply_delay = 0;
		mis20s1_attr.integration_time_limit = mis20s1_attr.max_integration_time;
		mis20s1_attr.max_integration_time_native = mis20s1_attr.max_integration_time;
		mis20s1_attr.min_integration_time_native = mis20s1_attr.min_integration_time;
		mis20s1_attr.expo_fs = 1;
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
	}

	return ret;
}

static int mis20s1_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	int ret = ISP_SUCCESS;

	mis20s1_setting_select(sd, info->default_boot);

	switch (info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
	case TISP_SENSOR_VI_MIPI_CSI1:
		mis20s1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		mis20s1_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_DVP:
		mis20s1_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this interface!!!\n");
	}

switch(info->mclk){
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

	ret = mis20s1_clk_set(sd, sclka, SENSOR_MCLK);
	if (ret) {
		ISP_ERROR("MCLK configuration failed!!!\n");
	}

	mis20s1_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int mis20s1_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = mis20s1_read(sd, 0x3000, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
			return ret;
	if (v != MIS20S1_CHIP_ID_H)
			return -ENODEV;
	*ident = v;

	ret = mis20s1_read(sd, 0x3001, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
			return ret;
	if (v != MIS20S1_CHIP_ID_L)
			return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int mis20s1_g_chip_ident(struct tx_isp_subdev *sd,
								struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	mis20s1_attr_check(sd);
	if(info->rst_gpio != -1){
		ret = private_gpio_request(info->rst_gpio,"mis20s1_reset");
		if(!ret){
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(info->rst_gpio, 0);
			private_msleep(30);
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(15);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",info->rst_gpio);
		}
	}
	if(info->pwdn_gpio != -1){
		ret = private_gpio_request(info->pwdn_gpio,"mis20s1_pwdn");
		if(!ret){
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",info->pwdn_gpio);
		}
	}
	ret = mis20s1_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an mis20s1 chip.\n",
				client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor name is %s\n", mis20s1_attr.name);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", info->video_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
					info->default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");
	if(chip){
		memcpy(chip->name, "mis20s1", sizeof("mis20s1"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int mis20s1_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int mis20s1_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	ret = mis20s1_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	sensor->video.state = TX_ISP_MODULE_INIT;
	private_msleep(280);
	*((u32 *)0xb3380000) = 0x5;

	if (!init->enable) {
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		return ISP_SUCCESS;
	} else {
		ret = mis20s1_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int mis20s1_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = ISP_SUCCESS;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = mis20s1_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int mis20s1_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	mis20s1_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}

static int mis20s1_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
			//ret = mis20s1_write_array(sd, wsize->regs);
			//if (ret)
			//        return ret;
			//sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = mis20s1_write_array(sd, mis20s1_stream_on);
			*((u32 *)0xb3380000) = 0x5;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("mis20s1 stream on\n");
		}
	} else {
		ret = mis20s1_write_array(sd, mis20s1_stream_off);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("mis20s1 stream off\n");
	}

	return ret;
}


#ifdef SENSOR_EXPO
static int mis20s1_temp_status(struct tx_isp_subdev *sd, int *again) {
	unsigned char temEn = 0;
	unsigned int temperature = 0;
	unsigned char tmper = 0;
	int ret = ISP_SUCCESS;
	ret += mis20s1_read(sd, 0x3502, &temEn);
	if ((temEn & 0x01) == 1) {
		ret += mis20s1_read(sd, 0x3504, &tmper);
		temperature = tmper;
		ret += mis20s1_read(sd, 0x3505, &tmper);
		temperature = (temperature << 8) | tmper;

		if (temperature > 0xb05) {
			ret += mis20s1_write(sd, 0x410f, 0x3b);
			if (temperature > 0xbe3) {
				if (*again >= 0x3c0)
					*again = 0x3c0;
			}
		} else {
			ret += mis20s1_write(sd, 0x410f, 0x3a);
		}

		if (temperature > 0xa38) {//65 start DPC en
			if (*again < 0x380) {
				ret += mis20s1_write(sd, 0x4402, 0x00);
				ret += mis20s1_write(sd, 0x3008, 0x01);
			}
			else {
				ret += mis20s1_write(sd, 0x4402, 0x3f);
				ret += mis20s1_write(sd, 0x3008, 0x01);
			}
		} else {
			ret += mis20s1_write(sd, 0x4402, 0x00);
			ret += mis20s1_write(sd, 0x3008, 0x01);
		}
	}
	return 0;
}

static int mis20s1_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	unsigned int exp = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;


	if ((again >> 12) == 0x1) {
		#ifdef SENSOR_WDR_2_FRAME
		ret += mis20s1_write(sd, 0x310c, 0x01 | 0x02);
		#else
		ret += mis20s1_write(sd, 0x310c, 0x01);
		#endif
	}
	else  {
		ret += mis20s1_write(sd, 0x310c, 0x00);
	}

	again = again & 0x0fff;
	mis20s1_temp_status(sd, &again);

	ret += mis20s1_write(sd, 0x3100, (unsigned char)((exp >> 8) & 0xff));
	ret += mis20s1_write(sd, 0x3101, (unsigned char)(exp & 0xff));
	ret += mis20s1_write(sd, 0x3106, (unsigned char)((again >> 8) & 0x03));
	ret += mis20s1_write(sd, 0x3107, (unsigned char)(again & 0xff));
	ret += mis20s1_write(sd, 0x3108, (unsigned char)((again >> 8) & 0x03));
	ret += mis20s1_write(sd, 0x3109, (unsigned char)(again & 0xff));
	ret += mis20s1_write(sd, 0x3008, 0x01);

	return ret;

}
#else
static int mis20s1_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	//unsigned int exp = value & 0xffff;

	ret += mis20s1_write(sd, 0x3100, (unsigned char)((value >> 8) & 0xff));
	ret += mis20s1_write(sd, 0x3101, (unsigned char)(value & 0xff));
	ret += mis20s1_write(sd, 0x3008, 0x01);

		return ret;
}

static int mis20s1_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	//unsigned int temperature = 0;
	//unsigned int temp = 0;

	ret += mis20s1_write(sd, 0x3106, (unsigned char)((value >> 8) & 0x03));
	ret += mis20s1_write(sd, 0x3107, (unsigned char)(value & 0xff));
	ret += mis20s1_write(sd, 0x3008, 0x01);

	//ret += mis20s1_read(sd, 0x3504, &tmp);
	//temperature = tmp;
	//ret += mis20s1_read(sd, 0x3505, &tmp);
	//temperature = (temperature << 8) | tmp;

	return ret;
}
#endif /* SENSOR_EXPO */

static int mis20s1_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int mis20s1_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int mis20s1_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
			ret = mis20s1_attr_set(sd, wsize);
	}

	return ret;
}

static int mis20s1_set_fps(struct tx_isp_subdev *sd, int fps)
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
		sclk = SENSOR_INIT_30FPS_HTS * SENSOR_INIT_30FPS_VTS * SENSOR_OUTPUT_INIT_FPS;  /**< HTS * VTS * FPS */
		max_fps = SENSOR_OUTPUT_INIT_FPS;
		break;
	case 1:
		sclk = SENSOR_WDR_INIT_30FPS_HTS * SENSOR_WDR_INIT_30FPS_VTS * SENSOR_WDR_OUTPUT_INIT_FPS;  /**< HTS * VTS * FPS */
		max_fps = SENSOR_WDR_OUTPUT_INIT_FPS;
		break;
	case 2:
		sclk = MIS20S1_60FPS_SUPPORT_RES_PCLK;  /**< HTS * VTS * FPS */
		max_fps = SENSOR_60FPS_OUTPUT_INIT_FPS;
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
	ret += mis20s1_read(sd, 0x3110, &val);
	hts = val << 8;
	val = 0;
	ret += mis20s1_read(sd, 0x3111, &val);
	hts = (hts | val);
	if (0 != ret) {
		ISP_ERROR("err: mis20s1 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);


	ret += mis20s1_write(sd, 0x310f, (unsigned char) (vts & 0xff));
	ret += mis20s1_write(sd, 0x310e, (unsigned char) (vts >> 8));
	ret += mis20s1_write(sd, 0x3008, 0x01);

	//  \*((u32 *)0xb3380000) = 0x5;

	if (0 != ret) {
		ISP_ERROR("err: mis20s1_write err\n");
		return ret;
	}

	sensor->video.fps = fps;

	if(info->default_boot == 1){
		sensor->video.attr->total_height = vts;
		sensor->video.attr->max_integration_time = (((vts - 5) /17) << 4);
		sensor->video.attr->integration_time_limit = (((vts - 5) /17) << 4);
		sensor->video.attr->max_integration_time_native = (((vts - 5) /17) << 4);
		sensor->video.attr->max_integration_time_short = ((vts - 5) /17);
	} else {
		sensor->video.attr->total_height = vts;
		sensor->video.attr->max_integration_time = vts - 4;
		sensor->video.attr->integration_time_limit = vts - 4;
		sensor->video.attr->max_integration_time_native = vts - 4;
	}

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	if (ret) {
		ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
	}


	return ret;
}

#ifdef SENSOR_MIR_FLIP
static int mis20s1_set_vflip(struct tx_isp_subdev *sd, int enable)
{

	int ret = ISP_SUCCESS;

	switch(enable) {
	case 0:
		ret += mis20s1_write(sd, 0x300c, 0x00);
		msleep(50);
		ret += mis20s1_write(sd, 0x3007, 0x00);
		ret += mis20s1_write(sd, 0x300c, 0x01);
		break;
	case 1:
		ret += mis20s1_write(sd, 0x300c, 0x00);
		msleep(50);
		ret += mis20s1_write(sd, 0x3007, 0x01);
		ret += mis20s1_write(sd, 0x300c, 0x01);
		break;
	case 2:
		ret += mis20s1_write(sd, 0x300c, 0x00);
		msleep(50);
		ret += mis20s1_write(sd, 0x3007, 0x02);
		ret += mis20s1_write(sd, 0x300c, 0x01);
		break;
	case 3:
		ret += mis20s1_write(sd, 0x300c, 0x00);
		msleep(50);
		ret += mis20s1_write(sd, 0x3007, 0x03);
		ret += mis20s1_write(sd, 0x300c, 0x01);
		break;
	default:
		ret += mis20s1_write(sd, 0x300c, 0x00);
		msleep(50);
		ret += mis20s1_write(sd, 0x3007, 0x00);
		ret += mis20s1_write(sd, 0x300c, 0x01);
		break;
	}

	*((u32 *)0xb3380000) = 0x5;
	return ret;
}
#endif /* SENSOR_MIR_FLIP */

#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
static int mis20s1_set_expo_short(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int short_exp = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	again = again & 0x0fff;
	mis20s1_temp_status(sd, &again);

	ret += mis20s1_write(sd, 0x3102, (unsigned char)((short_exp >> 8) & 0xff));
	ret += mis20s1_write(sd, 0x3103, (unsigned char)(short_exp & 0xff));

	//ret += mis20s1_write(sd, 0x3108, (unsigned char)((again >> 8) & 0x03));
	//ret += mis20s1_write(sd, 0x3109, (unsigned char)(again & 0xff));
	ret += mis20s1_write(sd, 0x3008, 0x01);

	return ret;
}
#else
static int mis20s1_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
		int ret = ISP_SUCCESS;


		return ret;
}

static int mis20s1_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
		int ret = ISP_SUCCESS;


		return ret;
}
#endif /* SENSOR_EXPO */

static int mis20s1_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
	//struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = ISP_SUCCESS;

	ret = mis20s1_write_array(sd, mis20s1_stream_off);
	if (wdr_en == 1) {
		mis20s1_setting_select(sd, 1);
		mis20s1_attr_set(sd, wsize);
	} else if (wdr_en == 0) {
		mis20s1_setting_select(sd, 2);
		mis20s1_attr_set(sd, wsize);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int mis20s1_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = ISP_SUCCESS;

	private_gpio_direction_output(info->rst_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(info->rst_gpio, 1);
	private_msleep(1);

	ret = mis20s1_write_array(sd, wsize->regs);
	ret = mis20s1_write_array(sd, mis20s1_stream_on);

	return 0;
}
#endif /* SENSOR_WDR_2_FRAME */
static int mis20s1_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
#ifdef SENSOR_WDR_2_FRAME
	struct tx_isp_initarg *init = arg;
#endif /* SENSOR_WDR_2_FRAME */

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}

	switch (cmd) {
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = mis20s1_set_expo(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = mis20s1_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
				ret = mis20s1_set_analog_gain(sd, sensor_val->value);
		break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = mis20s1_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = mis20s1_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = mis20s1_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = mis20s1_write_array(sd, mis20s1_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = mis20s1_write_array(sd, mis20s1_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = mis20s1_set_fps(sd, sensor_val->value);
		break;

#ifdef SENSOR_MIR_FLIP
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = mis20s1_set_vflip(sd, sensor_val->value);
		break;
#endif /* SENSOR_MIR_FLIP */
#ifdef SENSOR_WDR_2_FRAME
#ifdef SENSOR_EXPO
	case TX_ISP_EVENT_SENSOR_EXPO_SHORT:
		if (arg)
			ret = mis20s1_set_expo_short(sd, sensor_val->value);
		break;
#else
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
			if(arg)
					ret = mis20s1_set_integration_time_short(sd, sensor_val->value);
			break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
			if(arg)
					ret = mis20s1_set_analog_gain_short(sd, sensor_val->value);
			break;
#endif /* SENSOR_EXPO */
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = mis20s1_set_wdr(sd, init->enable);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = mis20s1_set_wdr_stop(sd, init->enable);
		break;
#endif /* SENSOR_WDR_2_FRAME */
	default:
			break;
	}

	return 0;
}

static struct tx_isp_subdev_core_ops mis20s1_core_ops = {
	.g_chip_ident = mis20s1_g_chip_ident,
	.reset = mis20s1_reset,
	.init = mis20s1_init,
	.g_register = mis20s1_g_register,
	.s_register = mis20s1_s_register,
};

static struct tx_isp_subdev_video_ops mis20s1_video_ops = {
	.s_stream = mis20s1_s_stream,
};

static struct tx_isp_subdev_sensor_ops	mis20s1_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl	= mis20s1_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops mis20s1_ops = {
	.core = &mis20s1_core_ops,
	.video = &mis20s1_video_ops,
	.sensor = &mis20s1_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
static struct platform_device sensor_platform_device = {
	.name = "mis20s1",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int mis20s1_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));


	sd = &sensor->sd;
	video = &sensor->video;

	sensor->video.attr = &mis20s1_attr;
	sensor->dev = &client->dev;
	tx_isp_subdev_init(&sensor_platform_device, sd, &mis20s1_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->mis20s1\n");
	return 0;

}

static int mis20s1_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;

	if(info->rst_gpio != -1)
		private_gpio_free(info->rst_gpio);
	if(info->pwdn_gpio != -1)
		private_gpio_free(info->pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);

	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id mis20s1_id[] = {
	{ "mis20s1", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mis20s1_id);

static struct i2c_driver mis20s1_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "mis20s1",
	},
	.probe		= mis20s1_probe,
	.remove		= mis20s1_remove,
	.id_table	= mis20s1_id,
};

static __init int init_mis20s1(void)
{
	return private_i2c_add_driver(&mis20s1_driver);
}

static __exit void exit_mis20s1(void)
{
	private_i2c_del_driver(&mis20s1_driver);
}

module_init(init_mis20s1);
module_exit(exit_mis20s1);

MODULE_DESCRIPTION("A low-level driver for ImageDesign mis20s1 sensor");
MODULE_LICENSE("GPL");
