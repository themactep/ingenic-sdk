/*
 * sc500ai.c
 *
 * Copyright (C) 2023 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps     interface              mode
 *   0          2880*1620       25        mipi_2lane           linear
 *   1          2880*1620       20        mipi_2lane           hdr
 */
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

#define SC500AI_CHIP_ID_H	(0xce)
#define SC500AI_CHIP_ID_L	(0x1f)
#define SC500AI_REG_END		0xffff
#define SC500AI_REG_DELAY	0xfffe
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MAX_FPS_DOL 20
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20230505a"
#define MCLK 24000000

static int reset_gpio = -1;
static int pwdn_gpio = -1;

static int wdr_bufsize = 5760000;//1000*2880*2

static int shvflip = 1;

static int data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;


struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
        unsigned int value;
        unsigned int gain;
};

struct again_lut sc500ai_again_lut[] = {
	{0x340, 0},
	{0x341, 1465},
	{0x342, 2818},
	{0x343, 4330},
	{0x344, 5731},
	{0x345, 7026},
	{0x346, 8471},
	{0x347, 9729},
	{0x348, 11135},
	{0x349, 12439},
	{0x34a, 13646},
	{0x34b, 14995},
	{0x34c, 16247},
	{0x34d, 17406},
	{0x34e, 18703},
	{0x34f, 19832},
	{0x350, 21097},
	{0x351, 22271},
	{0x352, 23360},
	{0x353, 24578},
	{0x354, 25710},
	{0x355, 26759},
	{0x356, 27935},
	{0x357, 28960},
	{0x358, 30108},
	{0x359, 31177},
	{0x35a, 32168},
	{0x35b, 33277},
	{0x35c, 34311},
	{0x35d, 35269},
	{0x35e, 36344},
	{0x35f, 37282},
	{0x360, 38335},
	{0x2340, 39315},
	{0x2341, 40766},
	{0x2342, 42195},
	{0x2343, 43660},
	{0x2344, 45046},
	{0x2345, 46413},
	{0x2346, 47760},
	{0x2347, 49142},
	{0x2348, 50451},
	{0x2349, 51741},
	{0x234a, 53068},
	{0x234b, 54323},
	{0x234c, 55563},
	{0x234d, 56785},
	{0x234e, 58044},
	{0x234f, 59186},
	{0x2350, 60413},
	{0x2351, 61623},
	{0x2352, 62723},
	{0x2353, 63905},
	{0x2354, 65025},
	{0x2355, 66133},
	{0x2356, 67227},
	{0x2357, 68354},
	{0x2358, 69468},
	{0x2359, 70481},
	{0x235a, 71570},
	{0x235b, 72604},
	{0x235c, 73626},
	{0x235d, 74638},
	{0x235e, 75680},
	{0x235f, 76671},
	{0x2360, 77651},
	{0x2361, 78660},
	{0x2362, 79580},
	{0x2363, 80570},
	{0x2364, 81510},
	{0x2365, 82441},
	{0x2366, 83364},
	{0x2367, 84315},
	{0x2368, 85256},
	{0x2369, 86114},
	{0x236a, 87038},
	{0x236b, 87916},
	{0x236c, 88787},
	{0x236d, 89649},
	{0x236e, 90540},
	{0x236f, 91386},
	{0x2370, 92225},
	{0x2371, 93092},
	{0x2372, 93882},
	{0x2373, 94733},
	{0x2374, 95543},
	{0x2375, 96346},
	{0x2376, 97176},
	{0x2377, 97966},
	{0x2378, 98781},
	{0x2379, 99525},
	{0x237a, 100326},
	{0x237b, 101090},
	{0x237c, 101848},
	{0x237d, 102632},
	{0x237e, 103377},
	{0x237f, 104117},
	{0x2740, 104851},
	{0x2741, 106302},
	{0x2742, 107790},
	{0x2743, 109196},
	{0x2744, 110611},
	{0x2745, 111977},
	{0x2746, 113323},
	{0x2747, 114678},
	{0x2748, 115987},
	{0x2749, 117277},
	{0x274a, 118604},
	{0x274b, 119859},
	{0x274c, 121124},
	{0x274d, 122347},
	{0x274e, 123554},
	{0x274f, 124772},
	{0x2750, 125949},
	{0x2751, 127135},
	{0x2752, 128307},
	{0x2753, 129441},
	{0x2754, 130585},
	{0x2755, 131691},
	{0x2756, 132785},
	{0x2757, 133890},
	{0x2758, 134982},
	{0x2759, 136039},
	{0x275a, 137106},
	{0x275b, 138140},
	{0x275c, 139184},
	{0x275d, 140195},
	{0x275e, 141196},
	{0x275f, 142228},
	{0x2760, 143206},
	{0x2761, 144177},
	{0x2762, 145156},
	{0x2763, 146106},
	{0x2764, 147066},
	{0x2765, 147996},
	{0x2766, 148918},
	{0x2767, 149869},
	{0x2768, 150772},
	{0x2769, 151669},
	{0x276a, 152574},
	{0x276b, 153452},
	{0x276c, 154341},
	{0x276d, 155203},
	{0x276e, 156076},
	{0x276f, 156940},
	{0x2770, 157779},
	{0x2771, 158610},
	{0x2772, 159452},
	{0x2773, 160269},
	{0x2774, 161096},
	{0x2775, 161916},
	{0x2776, 162712},
	{0x2777, 163518},
	{0x2778, 164300},
	{0x2779, 165077},
	{0x277a, 165862},
	{0x277b, 166626},
	{0x277c, 167416},
	{0x277d, 168168},
	{0x277e, 168913},
	{0x277f, 169668},
	{0x2f40, 170402},
	{0x2f41, 171867},
	{0x2f42, 173326},
	{0x2f43, 174732},
	{0x2f44, 176133},
	{0x2f45, 177513},
	{0x2f46, 178887},
	{0x2f47, 180214},
	{0x2f48, 181536},
	{0x2f49, 182853},
	{0x2f4a, 184140},
	{0x2f4b, 185395},
	{0x2f4c, 186648},
	{0x2f4d, 187896},
	{0x2f4e, 189116},
	{0x2f4f, 190308},
	{0x2f50, 191497},
	{0x2f51, 192683},
	{0x2f52, 193843},
	{0x2f53, 194977},
	{0x2f54, 196121},
	{0x2f55, 197239},
	{0x2f56, 198344},
	{0x2f57, 199426},
	{0x2f58, 200518},
	{0x2f59, 201586},
	{0x2f5a, 202642},
	{0x2f5b, 203676},
	{0x2f5c, 204720},
	{0x2f5d, 205742},
	{0x2f5e, 206752},
	{0x2f5f, 207753},
	{0x2f60, 208742},
	{0x2f61, 209722},
	{0x2f62, 210692},
	{0x2f63, 211651},
	{0x2f64, 212602},
	{0x2f65, 213542},
	{0x2f66, 214473},
	{0x2f67, 215395},
	{0x2f68, 216308},
	{0x2f69, 217214},
	{0x2f6a, 218119},
	{0x2f6b, 218997},
	{0x2f6c, 219877},
	{0x2f6d, 220748},
	{0x2f6e, 221620},
	{0x2f6f, 222467},
	{0x2f70, 223315},
	{0x2f71, 224155},
	{0x2f72, 224997},
	{0x2f73, 225814},
	{0x2f74, 226632},
	{0x2f75, 227452},
	{0x2f76, 228256},
	{0x2f77, 229046},
	{0x2f78, 229836},
	{0x2f79, 230629},
	{0x2f7a, 231407},
	{0x2f7b, 232170},
	{0x2f7c, 232936},
	{0x2f7d, 233704},
	{0x2f7e, 234455},
	{0x2f7f, 235196},
	{0x3f40, 235945},
	{0x3f41, 237410},
	{0x3f42, 238854},
	{0x3f43, 240276},
	{0x3f44, 241675},
	{0x3f45, 243055},
	{0x3f46, 244416},
	{0x3f47, 245757},
	{0x3f48, 247078},
	{0x3f49, 248389},
	{0x3f4a, 249669},
	{0x3f4b, 250944},
	{0x3f4c, 252190},
	{0x3f4d, 253432},
	{0x3f4e, 254645},
	{0x3f4f, 255856},
	{0x3f50, 257038},
	{0x3f51, 258219},
	{0x3f52, 259373},
	{0x3f53, 260525},
	{0x3f54, 261657},
	{0x3f55, 262775},
	{0x3f56, 263880},
	{0x3f57, 264973},
	{0x3f58, 266054},
	{0x3f59, 267122},
	{0x3f5a, 268178},
	{0x3f5b, 269223},
	{0x3f5c, 270256},
	{0x3f5d, 271278},
	{0x3f5e, 272288},
	{0x3f5f, 273294},
	{0x3f60, 274278},
	{0x3f61, 275264},
	{0x3f62, 276228},
	{0x3f63, 277193},
	{0x3f64, 278138},
	{0x3f65, 279082},
	{0x3f66, 280009},
	{0x3f67, 280937},
	{0x3f68, 281844},
	{0x3f69, 282755},
	{0x3f6a, 283650},
	{0x3f6b, 284537},
	{0x3f6c, 285417},
	{0x3f6d, 286288},
	{0x3f6e, 287152},
	{0x3f6f, 288007},
	{0x3f70, 288855},
	{0x3f71, 289695},
	{0x3f72, 290528},
	{0x3f73, 291358},
	{0x3f74, 292172},
	{0x3f75, 292988},
	{0x3f76, 293789},
	{0x3f77, 294590},
	{0x3f78, 295376},
	{0x3f79, 296165},
	{0x3f7a, 296938},
	{0x3f7b, 297714},
	{0x3f7c, 298477},
	{0x3f7d, 299240},
	{0x3f7e, 299991},
	{0x3f7f, 300740},
};

struct tx_isp_sensor_attribute sc500ai_attr;

unsigned int sc500ai_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;
	*sensor_it = expo;

	return isp_it;
}
unsigned int sc500ai_alloc_integration_time_short(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;
	*sensor_it = expo;

	return isp_it;
}

unsigned int sc500ai_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
        struct again_lut *lut = sc500ai_again_lut;

        while(lut->gain <= sc500ai_attr.max_again) {
                if(isp_gain == 0) {
                        *sensor_again = lut->value;
                        return 0;
                } else if(isp_gain < lut->gain) {
                        *sensor_again = (lut - 1)->value;
                        return (lut - 1)->gain;
                } else {
                        if((lut->gain == sc500ai_attr.max_again) && (isp_gain >= lut->gain)) {
                                *sensor_again = lut->value;
                                return lut->gain;
                        }
                }

                lut++;
        }

        return isp_gain;
}
unsigned int sc500ai_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
        struct again_lut *lut = sc500ai_again_lut;

        while(lut->gain <= sc500ai_attr.max_again) {
                if(isp_gain == 0) {
                        *sensor_again = lut->value;
                        return 0;
                } else if(isp_gain < lut->gain) {
                        *sensor_again = (lut - 1)->value;
                        return (lut - 1)->gain;
                } else{
                        if((lut->gain == sc500ai_attr.max_again) && (isp_gain >= lut->gain)) {
                                *sensor_again = lut->value;
                                return lut->gain;
                        }
                }

                lut++;
        }

        return isp_gain;
}

unsigned int sc500ai_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sc500ai_mipi_dol = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1080,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 2880,
	.image_theight = 1620,
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
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_mipi_bus sc500ai_mipi_linear={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 792,
	.lans = 2,
	.index = 0,
	.settle_time_apative_en = 0,
	.image_twidth = 2880,
	.image_theight = 1620,
	.mipi_sc.mipi_crop_start0x = 0,
	.mipi_sc.mipi_crop_start0y = 0,
	.mipi_sc.mipi_crop_start1x = 0,
	.mipi_sc.mipi_crop_start1y = 0,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute sc500ai_attr={
	.name = "sc500ai",
	.chip_id = 0xce1f,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL,
	.cbus_device = 0x30,
	.max_again = 300740,
	.max_again_short = 300740,
	.max_dgain = 0,
	.min_integration_time = 3,
	.min_integration_time_short = 1,
	.min_integration_time_native = 3,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc500ai_alloc_again,
	.sensor_ctrl.alloc_again_short = sc500ai_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sc500ai_alloc_dgain,
	.sensor_ctrl.alloc_integration_time_short = sc500ai_alloc_integration_time_short,
};

static struct regval_list sc500ai_init_regs_2880_1620_25fps_mipi[] = {
		{0x0103,0x01},
		{0x0100,0x00},
		{0x36e9,0x80},
		{0x36f9,0x80},
		{0x3018,0x32},
		{0x3019,0x0c},
		{0x301f,0xe3},
		{0x320c,0x06},//
		{0x320d,0x14},//0x614 = 1556
		{0x320e,0x07},//
		{0x320f,0xf4},//0x7f4 = 2036
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
		{0x3e01,0xfe},
		{0x3e02,0x00},
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
		{SC500AI_REG_DELAY, 0x10},
		{0x0100,0x01},
		{SC500AI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc500ai_init_regs_2880_1620_20fps_mipi_dol[] = {
		{0x0103,0x01},
		{0x0100,0x00},
		{0x36e9,0x80},
		{0x36f9,0x80},
		{0x3018,0x32},
		{0x3019,0x0c},
		{0x301f,0x0a},
		{0x3106,0x01},
		{0x320c,0x09},//hts 0x960 = 2400
		{0x320d,0x60},//
		{0x320e,0x0d},//vts 0xd30 = 3376
		{0x320f,0x30},//
		{0x3220,0x53},
		{0x3250,0xff},
		{0x3253,0x0a},
		{0x3301,0x0b},
		{0x3302,0x20},
		{0x3303,0x10},
		{0x3304,0x70},
		{0x3306,0x50},
		{0x3308,0x18},
		{0x3309,0x80},
		{0x330a,0x00},
		{0x330b,0xe8},
		{0x330d,0x30},
		{0x330e,0x30},
		{0x330f,0x02},
		{0x3310,0x02},
		{0x331c,0x08},
		{0x331e,0x61},
		{0x331f,0x71},
		{0x3320,0x11},
		{0x3333,0x10},
		{0x334c,0x10},
		{0x3356,0x11},
		{0x3364,0x17},
		{0x336d,0x03},
		{0x3390,0x08},
		{0x3391,0x18},
		{0x3392,0x38},
		{0x3393,0x0a},
		{0x3394,0x0a},
		{0x3395,0x12},
		{0x3396,0x08},
		{0x3397,0x18},
		{0x3398,0x38},
		{0x3399,0x0a},
		{0x339a,0x0a},
		{0x339b,0x0a},
		{0x339c,0x12},
		{0x33ac,0x10},
		{0x33ae,0x20},
		{0x33af,0x21},
		{0x360f,0x01},
		{0x3621,0xe8},
		{0x3622,0x06},
		{0x3630,0x82},
		{0x3633,0x33},
		{0x3634,0x64},
		{0x3637,0x50},
		{0x363a,0x1f},
		{0x363c,0x40},
		{0x3651,0x7d},
		{0x3670,0x0a},
		{0x3671,0x06},
		{0x3672,0x16},
		{0x3673,0x17},
		{0x3674,0x82},
		{0x3675,0x62},
		{0x3676,0x44},
		{0x367a,0x48},
		{0x367b,0x78},
		{0x367c,0x48},
		{0x367d,0x58},
		{0x3690,0x34},
		{0x3691,0x34},
		{0x3692,0x54},
		{0x369c,0x48},
		{0x369d,0x78},
		{0x36ea,0x37},
		{0x36eb,0x04},
		{0x36ec,0x0a},
		{0x36ed,0x24},
		{0x36fa,0x37},
		{0x36fb,0x04},
		{0x36fc,0x00},
		{0x36fd,0x26},
		{0x3904,0x04},
		{0x3908,0x41},
		{0x391f,0x10},
		{0x39c2,0x30},
		{0x3e00,0x01},
		{0x3000,0x01},
		{0x3e01,0x8c},
		{0x3e02,0x00},
		{0x3e04,0x18},
		{0x3e05,0xc0},
		{0x3e23,0x00},//RHS 0xcc = 204
		{0x3e24,0xcc},//
		{0x4500,0x88},
		{0x4509,0x20},
		{0x4800,0x24},
		{0x4837,0x0e},
		{0x4853,0xfd},
		{0x36e9,0x24},
		{0x36f9,0x21},
		{SC500AI_REG_DELAY, 0x10},
		{0x0100,0x01},
		{SC500AI_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the jxf23_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc500ai_win_sizes[] = {
	{
		.width		= 2880,
		.height		= 1620,
		.fps		= 25 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= sc500ai_init_regs_2880_1620_25fps_mipi,
	},
	{
		.width		= 2880,
		.height		= 1620,
		.fps		= 20 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs		= sc500ai_init_regs_2880_1620_20fps_mipi_dol,
	}
};

struct tx_isp_sensor_win_setting *wsize = &sc500ai_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sc500ai_stream_on[] = {
	{SC500AI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc500ai_stream_off[] = {
	{SC500AI_REG_END, 0x00},	/* END MARKER */
};

int sc500ai_read(struct tx_isp_subdev *sd,  uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg>>8)&0xff, reg&0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= buf,
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

int sc500ai_write(struct tx_isp_subdev *sd, uint16_t reg,
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

#if 0
static int sc500ai_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC500AI_REG_END) {
		if (vals->reg_num == SC500AI_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc500ai_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}
	return 0;
}
#endif

static int sc500ai_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC500AI_REG_END) {
		if (vals->reg_num == SC500AI_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc500ai_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc500ai_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

#if 1
static int sc500ai_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sc500ai_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC500AI_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc500ai_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;

	if (v != SC500AI_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}
#endif

static int sc500ai_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff );
	int again = (value & 0xffff0000) >> 16;
	if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) it = (it << 2) + 1;
	ret = sc500ai_write(sd,  0x3e00, (unsigned char)((it >> 12) & 0x0f));
	ret += sc500ai_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc500ai_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));

	ret += sc500ai_write(sd, 0x3e08, (unsigned char)((again >> 8) & 0xff));
	ret += sc500ai_write(sd, 0x3e09, (unsigned char)(again & 0xff));

	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int sc500ai_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	
	if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) value = (value << 2) + 1;
	ret = sc500ai_write(sd,  0x3e00, (unsigned char)((value >> 12) & 0x0f));
	ret += sc500ai_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc500ai_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));

	return 0;
}

static int sc500ai_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sc500ai_write(sd, 0x3e08, (unsigned char)((value >> 8) & 0xff));
	ret += sc500ai_write(sd, 0x3e09, (unsigned char)(value & 0xff));

	return 0;
}
#endif

static int sc500ai_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value = (value << 2) + 1;
	ret = sc500ai_write(sd,  0x3e04, (unsigned char)((value >> 4) & 0xff));
	ret = sc500ai_write(sd,  0x3e05, (unsigned char)(value & 0x0f) << 4);

	if (ret < 0)
		return ret;

	return 0;
}

static int sc500ai_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sc500ai_write(sd, 0x3e12, (unsigned char)((value >> 8) & 0xff));
	ret += sc500ai_write(sd, 0x3e13, (unsigned char)(value & 0xff));

	return 0;
}

static int sc500ai_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc500ai_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc500ai_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!init->enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	sensor->video.state = TX_ISP_MODULE_DEINIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc500ai_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT){
			ret = sc500ai_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sc500ai_write_array(sd, sc500ai_stream_on);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("sc500ai stream on\n");
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
	} else {
		ret = sc500ai_write_array(sd, sc500ai_stream_off);
		pr_debug("sc500ai stream off\n");
	}

	return ret;
}

static int sc500ai_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int aclk = 0;
	unsigned short vts = 0;
	unsigned short hts=0;
	unsigned int short_time = 0;
	unsigned int sensor_max_fps = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;
	unsigned char val = 0;

	switch(sensor->info.default_boot){
	case 0:
		sclk = 1556 * 2036 * 25;
		sensor_max_fps = TX_SENSOR_MAX_FPS_25;
		break;
	case 1:
		sclk = 2400 * 3376 * 20;
		aclk = 204 * 20;
		sensor_max_fps = TX_SENSOR_MAX_FPS_20;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	/* the format of fps is 16/16. for example 30 << 16 | 2, the value is 30/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (sensor_max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)){
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}

	ret += sc500ai_read(sd, 0x320c, &val);
	hts = val;
	val = 0;
	ret += sc500ai_read(sd, 0x320d, &val);
	hts = ((hts << 8)| val);

	if (0 != ret) {
		ISP_ERROR("err: sc500ai read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	sc500ai_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	sc500ai_write(sd, 0x320e, (unsigned char)(vts >> 8));
	
	if (sensor->info.default_boot == 1)
	{
		short_time = aclk * (fps & 0xffff)/((fps & 0xffff0000) >> 16);
		sc500ai_write(sd, 0x3e24, (unsigned char)(short_time & 0xff));
		sc500ai_write(sd, 0x3e23, (unsigned char)(short_time >> 8));
	}

	if (0 != ret) {
		ISP_ERROR("err: sc500ai_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = (sensor->info.default_boot == 0) ? (2*vts -10) : (vts/2 - short_time/2 - 5);
	sensor->video.attr->integration_time_limit = (sensor->info.default_boot == 0) ? (2*vts -10) : (vts/2 - short_time/2 - 5);
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = (sensor->info.default_boot == 0) ? (2*vts -10) : (vts/2 - short_time/2 - 5);
	if (sensor->info.default_boot == 1) sensor->video.attr->max_integration_time_short = short_time/2 - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	if (ret) {
		ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
	}

	return ret;
}


static int sc500ai_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	val = sc500ai_read(sd, 0x3221, &val);
	switch(enable) {
	case 0:
		val &= 0x99;
		break;
	case 1:
		val = ((val & 0x9F) | 0x06);
		break;
	case 2:
		val = ((val & 0xF9) | 0x60);
		break;
	case 3:
		val |= 0x66;
		break;
	}
	sc500ai_write(sd, 0x3221, val);
	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}


static int sc500ai_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = TISP_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}
	return ret;
}

struct clk *sclka;
static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = 0;

	switch(info->default_boot){
	case 0:
		wsize = &sc500ai_win_sizes[0];
		memcpy(&sc500ai_attr.mipi, &sc500ai_mipi_linear, sizeof(sc500ai_mipi_linear));
		sc500ai_attr.mipi.clk = 792,
		sc500ai_attr.min_integration_time = 3;
		sc500ai_attr.min_integration_time_native = 3,
		sc500ai_attr.total_width = 1556;
		sc500ai_attr.total_height = 2036;
		sc500ai_attr.max_integration_time_native = 4062;   /* 2036*2-10 */
		sc500ai_attr.integration_time_limit = 4062;
		sc500ai_attr.max_integration_time = 4062;
		sc500ai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sc500ai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		printk("=================> linear is ok");
		break;
	case 1:
		sc500ai_attr.wdr_cache = wdr_bufsize;
		wsize = &sc500ai_win_sizes[1];
		memcpy(&sc500ai_attr.mipi, &sc500ai_mipi_dol, sizeof(sc500ai_mipi_dol));
		sc500ai_attr.mipi.clk = 1080,
		sc500ai_attr.min_integration_time = 1;
		sc500ai_attr.min_integration_time_short = 1;
		sc500ai_attr.min_integration_time_native = 1,
		sc500ai_attr.total_width = 2400;
		sc500ai_attr.total_height = 3376;
		sc500ai_attr.max_integration_time_native = 1581; /* vts/2 -rhs/2-5 */
		sc500ai_attr.integration_time_limit = 1581;
		sc500ai_attr.max_integration_time = 1581;
		sc500ai_attr.max_integration_time_short = 98; /* rhs/2-4 */
		sc500ai_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		printk("=================> 25fps_hdr is ok");
		break;
	default:
		ISP_ERROR("Have no this setting!!!\n");
	}

	data_type = sc500ai_attr.data_type;

	switch(info->video_interface){
	case TISP_SENSOR_VI_MIPI_CSI0:
		sc500ai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc500ai_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		sc500ai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc500ai_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		sc500ai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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
	rate = private_clk_get_rate(sensor->mclk);

	if (((rate / 1000) % 24000) != 0) {
                ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
                sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
		if (IS_ERR(sclka)) {
			pr_err("get sclka failed\n");
		} else {
			rate = private_clk_get_rate(sclka);
			if (((rate / 1000) % 24000) != 0) {
				private_clk_set_rate(sclka, 1200000000);
			}
		}
	}

        private_clk_set_rate(sensor->mclk, MCLK);
        private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

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
    ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;

err_get_mclk:
	return -1;
}

static int sc500ai_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc500ai_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"sc500ai_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc500ai_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc500ai chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("sc500ai chip found @ 0x%02x (%s)\n sensor drv version %s", client->addr, client->adapter->name, SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc500ai", sizeof("sc500ai"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

#if 1
static int sc500ai_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = 0;

	//ret = sc500ai_write(sd, 0x0103, 0x1);

	if(wdr_en == 1){
		info->default_boot=1;
		memcpy(&sc500ai_attr.mipi, &sc500ai_mipi_dol, sizeof(sc500ai_mipi_dol));
		sc500ai_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		sc500ai_attr.wdr_cache = wdr_bufsize;
		wsize = &sc500ai_win_sizes[1];
		sc500ai_attr.mipi.clk = 1080,
		sc500ai_attr.min_integration_time = 1;
		sc500ai_attr.min_integration_time_short = 1;
		sc500ai_attr.min_integration_time_native = 1,
		sc500ai_attr.total_width = 2400;
		sc500ai_attr.total_height = 3376;
		sc500ai_attr.max_integration_time_native = 1581;
		sc500ai_attr.integration_time_limit = 1581;
		sc500ai_attr.max_integration_time = 1581;
		sc500ai_attr.max_integration_time_short = 98;
		printk("\n-------------------------switch wdr@25fps ok ----------------------\n");
	}else if (wdr_en == 0){
		info->default_boot = 0;
		memcpy(&sc500ai_attr.mipi, &sc500ai_mipi_linear, sizeof(sc500ai_mipi_linear));
		sc500ai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		wsize = &sc500ai_win_sizes[0];
		sc500ai_attr.mipi.clk = 792,
		sc500ai_attr.min_integration_time = 3;
		sc500ai_attr.min_integration_time_native = 3,
		sc500ai_attr.total_width = 1556;
		sc500ai_attr.total_height = 2036;
		sc500ai_attr.max_integration_time_native = 4062;
		sc500ai_attr.integration_time_limit = 4062;
		sc500ai_attr.max_integration_time = 4062;
		printk("\n-------------------------switch linear ok ----------------------\n");
	}else{
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	data_type = sc500ai_attr.data_type;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int sc500ai_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
	int ret = 0;
//	printk("\n==========> set_wdr\n");
	private_gpio_direction_output(reset_gpio, 1);
	private_msleep(1);
	private_gpio_direction_output(reset_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(reset_gpio, 1);
	private_msleep(1);

	ret = sc500ai_write_array(sd, wsize->regs);
	ret = sc500ai_write_array(sd, sc500ai_stream_on);

	return 0;
}
#endif

static int sc500ai_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	struct tx_isp_initarg *init = arg;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = sc500ai_set_expo(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
	//	if(arg)
	//		ret = sc500ai_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
	//	if(arg)
	//		ret = sc500ai_set_analog_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc500ai_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc500ai_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc500ai_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = sc500ai_write_array(sd, sc500ai_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sc500ai_write_array(sd, sc500ai_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc500ai_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = sc500ai_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if(arg)
			ret = sc500ai_set_analog_gain_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = sc500ai_set_wdr(sd, init->enable);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = sc500ai_set_wdr_stop(sd, init->enable);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = sc500ai_set_vflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int sc500ai_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = sc500ai_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc500ai_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc500ai_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc500ai_core_ops = {
	.g_chip_ident = sc500ai_g_chip_ident,
	.reset = sc500ai_reset,
	.init = sc500ai_init,
	/*.ioctl = sc500ai_ops_ioctl,*/
	.g_register = sc500ai_g_register,
	.s_register = sc500ai_s_register,
};

static struct tx_isp_subdev_video_ops sc500ai_video_ops = {
	.s_stream = sc500ai_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc500ai_sensor_ops = {
	.ioctl	= sc500ai_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc500ai_ops = {
	.core = &sc500ai_core_ops,
	.video = &sc500ai_video_ops,
	.sensor = &sc500ai_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc500ai",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc500ai_probe(struct i2c_client *client, const struct i2c_device_id *id)
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
	sensor->dev = &client->dev;
	sd = &sensor->sd;
	video = &sensor->video;
	sc500ai_attr.expo_fs = 1;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sc500ai_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc500ai_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc500ai\n");

	return 0;
}

static int sc500ai_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id sc500ai_id[] = {
	{ "sc500ai", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc500ai_id);

static struct i2c_driver sc500ai_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc500ai",
	},
	.probe		= sc500ai_probe,
	.remove		= sc500ai_remove,
	.id_table	= sc500ai_id,
};

static __init int init_sc500ai(void)
{
	return private_i2c_add_driver(&sc500ai_driver);
}

static __exit void exit_sc500ai(void)
{
	private_i2c_del_driver(&sc500ai_driver);
}

module_init(init_sc500ai);
module_exit(exit_sc500ai);

MODULE_DESCRIPTION("A low-level driver for sc500ai sensors");
MODULE_LICENSE("GPL");
