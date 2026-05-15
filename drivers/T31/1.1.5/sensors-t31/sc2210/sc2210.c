/*
 * sc2210.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#define SC2210_CHIP_ID_H	(0x22)
#define SC2210_CHIP_ID_L	(0x10)
#define SC2210_REG_END		0xffff
#define SC2210_REG_DELAY	0xfffe
#define SC2210_SUPPORT_30FPS_SCLK (37125000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20201124a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

//static unsigned short int dpc_flag = 1;
//static unsigned int gain_val = 0x37e;
/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sc2210_again_lut[] = {
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
	{0x361, 39339},
	{0x362, 40270},
	{0x363, 41253},
	{0x364, 42226},
	{0x365, 43129},
	{0x366, 44082},
	{0x367, 44968},
	{0x368, 45904},
	{0x369, 46830},
	{0x36a, 47690},
	{0x36b, 48599},
	{0x36c, 49500},
	{0x36d, 50336},
	{0x36e, 51220},
	{0x36f, 52042},
	{0x370, 52910},
	{0x371, 53771},
	{0x372, 54571},
	{0x373, 55416},
	{0x374, 56254},
	{0x375, 57033},
	{0x376, 57857},
	{0x377, 58623},
	{0x378, 59433},
	{0x379, 60237},
	{0x37a, 60984},
	{0x37b, 61774},
	{0x37c, 62558},
	{0x37d, 63287},
	{0x37e, 64059},
	{0x37f, 64776},
	{0x740, 65536},
	{0x741, 66990},
	{0x742, 68468},
	{0x743, 69878},
	{0x744, 71267},
	{0x745, 72637},
	{0x746, 74030},
	{0x747, 75360},
	{0x748, 76672},
	{0x749, 77965},
	{0x74a, 79283},
	{0x74b, 80541},
	{0x74c, 81784},
	{0x74d, 83010},
	{0x74e, 84259},
	{0x74f, 85454},
	{0x750, 86633},
	{0x751, 87799},
	{0x752, 88986},
	{0x753, 90123},
	{0x754, 91246},
	{0x755, 92356},
	{0x756, 93489},
	{0x757, 94573},
	{0x758, 95645},
	{0x759, 96705},
	{0x75a, 97786},
	{0x75b, 98823},
	{0x75c, 99848},
	{0x75d, 100862},
	{0x75e, 101897},
	{0x75f, 102890},
	{0x760, 103872},
	{0x761, 104844},
	{0x762, 105837},
	{0x763, 106789},
	{0x764, 107731},
	{0x765, 108665},
	{0x766, 109618},
	{0x767, 110533},
	{0x768, 111440},
	{0x769, 112337},
	{0x76a, 113255},
	{0x76b, 114135},
	{0x76c, 115008},
	{0x2340, 115872},
	{0x2341, 117332},
	{0x2342, 118770},
	{0x2343, 120213},
	{0x2344, 121608},
	{0x2345, 122982},
	{0x2346, 124337},
	{0x2347, 125698},
	{0x2348, 127015},
	{0x2349, 128313},
	{0x234a, 129595},
	{0x234b, 130859},
	{0x234c, 132129},
	{0x234d, 133360},
	{0x234e, 134575},
	{0x234f, 135775},
	{0x2350, 136981},
	{0x2351, 138151},
	{0x2352, 139306},
	{0x2353, 140448},
	{0x2354, 141575},
	{0x2355, 142711},
	{0x2356, 143812},
	{0x2357, 144900},
	{0x2358, 145977},
	{0x2359, 147041},
	{0x235a, 148113},
	{0x235b, 149153},
	{0x235c, 150182},
	{0x235d, 151200},
	{0x235e, 152226},
	{0x235f, 153222},
	{0x2360, 154208},
	{0x2361, 155184},
	{0x2362, 156150},
	{0x2363, 157124},
	{0x2364, 158070},
	{0x2365, 159007},
	{0x2366, 159935},
	{0x2367, 160871},
	{0x2368, 161781},
	{0x2369, 162682},
	{0x236a, 163574},
	{0x236b, 164458},
	{0x236c, 165351},
	{0x236d, 166219},
	{0x236e, 167078},
	{0x236f, 167931},
	{0x2370, 168791},
	{0x2371, 169628},
	{0x2372, 170458},
	{0x2373, 171280},
	{0x2374, 172095},
	{0x2375, 172919},
	{0x2376, 173720},
	{0x2377, 174515},
	{0x2378, 175303},
	{0x2379, 176084},
	{0x237a, 176874},
	{0x237b, 177642},
	{0x237c, 178405},
	{0x237d, 179161},
	{0x237e, 179925},
	{0x237f, 180670},
	{0x2740, 181408},
	{0x2741, 182868},
	{0x2742, 184319},
	{0x2743, 185735},
	{0x2744, 187144},
	{0x2745, 188518},
	{0x2746, 189886},
	{0x2747, 191221},
	{0x2748, 192551},
	{0x2749, 193849},
	{0x274a, 195131},
	{0x274b, 196406},
	{0x274c, 197654},
	{0x274d, 198896},
	{0x274e, 200111},
	{0x274f, 201322},
	{0x2750, 202506},
	{0x2751, 203676},
	{0x2752, 204842},
	{0x2753, 205984},
	{0x2754, 207122},
	{0x2755, 208236},
	{0x2756, 209348},
	{0x2757, 210436},
	{0x2758, 211523},
	{0x2759, 212587},
	{0x275a, 213639},
	{0x275b, 214689},
	{0x275c, 215718},
	{0x275d, 216746},
	{0x275e, 217753},
	{0x275f, 218758},
	{0x2760, 219744},
	{0x2761, 220720},
	{0x2762, 221695},
	{0x2763, 222651},
	{0x2764, 223606},
	{0x2765, 224543},
	{0x2766, 225480},
	{0x2767, 226398},
	{0x2768, 227317},
	{0x2769, 228218},
	{0x276a, 229110},
	{0x276b, 230003},
	{0x276c, 230879},
	{0x276d, 231755},
	{0x276e, 232614},
	{0x276f, 233475},
	{0x2770, 234319},
	{0x2771, 235156},
	{0x2772, 235994},
	{0x2773, 236816},
	{0x2774, 237639},
	{0x2775, 238447},
	{0x2776, 239256},
	{0x2777, 240051},
	{0x2778, 240846},
	{0x2779, 241627},
	{0x277a, 242402},
	{0x277b, 243178},
	{0x277c, 243941},
	{0x277d, 244704},
	{0x277e, 245454},
	{0x277f, 246206},
	{0x2f40, 246944},
	{0x2f41, 248411},
	{0x2f42, 249855},
	{0x2f43, 251278},
	{0x2f44, 252680},
	{0x2f45, 254054},
	{0x2f46, 255415},
	{0x2f47, 256757},
	{0x2f48, 258080},
	{0x2f49, 259385},
	{0x2f4a, 260673},
	{0x2f4b, 261942},
	{0x2f4c, 263195},
	{0x2f4d, 264426},
	{0x2f4e, 265647},
	{0x2f4f, 266852},
	{0x2f50, 268042},
	{0x2f51, 269217},
	{0x2f52, 270378},
	{0x2f53, 271525},
	{0x2f54, 272658},
	{0x2f55, 273772},
	{0x2f56, 274879},
	{0x2f57, 275972},
	{0x2f58, 277054},
	{0x2f59, 278123},
	{0x2f5a, 279180},
	{0x2f5b, 280225},
	{0x2f5c, 281259},
	{0x2f5d, 282277},
	{0x2f5e, 283289},
	{0x2f5f, 284290},
	{0x2f60, 285280},
	{0x2f61, 286261},
	{0x2f62, 287231},
	{0x2f63, 288192},
	{0x2f64, 289142},
	{0x2f65, 290079},
	{0x2f66, 291011},
	{0x2f67, 291934},
	{0x2f68, 292848},
	{0x2f69, 293754},
	{0x2f6a, 294650},
	{0x2f6b, 295539},
	{0x2f6c, 296419},
	{0x2f6d, 297287},
	{0x2f6e, 298150},
	{0x2f6f, 299007},
	{0x2f70, 299855},
	{0x2f71, 300696},
	{0x2f72, 301530},
	{0x2f73, 302356},
	{0x2f74, 303175},
	{0x2f75, 303983},
	{0x2f76, 304788},
	{0x2f77, 305587},
	{0x2f78, 306378},
	{0x2f79, 307163},
	{0x2f7a, 307942},
	{0x2f7b, 308714},
	{0x2f7c, 309480},
	{0x2f7d, 310237},
	{0x2f7e, 310990},
	{0x2f7f, 311738},
	{0x3f40, 312480},
	{0x3f41, 313947},
	{0x3f42, 315391},
	{0x3f43, 316811},
	{0x3f44, 318212},
	{0x3f45, 319593},
	{0x3f46, 320955},
	{0x3f47, 322293},
	{0x3f48, 323617},
	{0x3f49, 324921},
	{0x3f4a, 326209},
	{0x3f4b, 327475},
	{0x3f4c, 328728},
	{0x3f4d, 329965},
	{0x3f4e, 331186},
	{0x3f4f, 332388},
	{0x3f50, 333578},
	{0x3f51, 334753},
	{0x3f52, 335914},
	{0x3f53, 337058},
	{0x3f54, 338191},
	{0x3f55, 339311},
	{0x3f56, 340417},
	{0x3f57, 341508},
	{0x3f58, 342590},
	{0x3f59, 343659},
	{0x3f5a, 344716},
	{0x3f5b, 345759},
	{0x3f5c, 346792},
	{0x3f5d, 347815},
	{0x3f5e, 348827},
	{0x3f5f, 349826},
	{0x3f60, 350816},
	{0x3f61, 351797},
	{0x3f62, 352767},
	{0x3f63, 353725},
	{0x3f64, 354676},
	{0x3f65, 355617},
	{0x3f66, 356549},
	{0x3f67, 357470},
	{0x3f68, 358384},
	{0x3f69, 359290},
	{0x3f6a, 360186},
	{0x3f6b, 361073},
	{0x3f6c, 361953},
	{0x3f6d, 362825},
	{0x3f6e, 363689},
	{0x3f6f, 364543},
	{0x3f70, 365391},
	{0x3f71, 366232},
	{0x3f72, 367066},
	{0x3f73, 367890},
	{0x3f74, 368709},
	{0x3f75, 369521},
	{0x3f76, 370326},
	{0x3f77, 371123},
	{0x3f78, 371914},
	{0x3f79, 372699},
	{0x3f7a, 373478},
	{0x3f7b, 374248},
	{0x3f7c, 375015},
	{0x3f7d, 375774},
	{0x3f7e, 376528},
	{0x3f7f, 377274},

};

struct tx_isp_sensor_attribute sc2210_attr;

unsigned int sc2210_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc2210_again_lut;
	while(lut->gain <= sc2210_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == sc2210_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc2210_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sc2210_attr={
	.name = "sc2210",
	.chip_id = 0x2210,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 371,
		.lans = 2,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 1920,
		.image_theight = 1080,
		.mipi_sc.mipi_crop_start0x = 0,
		.mipi_sc.mipi_crop_start0y = 0,
		.mipi_sc.mipi_crop_start1x = 0,
		.mipi_sc.mipi_crop_start1y = 0,
		.mipi_sc.mipi_crop_start2x = 0,
		.mipi_sc.mipi_crop_start2y = 0,
		.mipi_sc.mipi_crop_start3x = 0,
		.mipi_sc.mipi_crop_start3y = 0,
		.mipi_sc.data_type_en = 0,
		.mipi_sc.data_type_value = RAW10,
		.mipi_sc.del_start = 0,
		.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
		.mipi_sc.sensor_fid_mode = 0,
		.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 377274,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 0x465 -4,
	.integration_time_limit = 0x465 -4,
	.total_width = 0x44c,
	.total_height = 0x465,
	.max_integration_time = 0x465 -4,
	.one_line_expr_in_us = 20,
	.integration_time_apply_delay = 6,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc2210_alloc_again,
	.sensor_ctrl.alloc_dgain = sc2210_alloc_dgain,
};

static struct regval_list sc2210_init_regs_1920_1080_30fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x36f9,0x80},
	{0x3001,0x07},
	{0x3002,0xc0},
	{0x300a,0x2c},
	{0x300f,0x00},
	{0x3018,0x33},
	{0x3019,0x0c},
	{0x301f,0x47},
	{0x3031,0x0a},
	{0x3033,0x20},
	{0x3038,0x22},
	{0x3106,0x81},
	{0x3201,0x04},
	{0x3203,0x04},
	{0x3204,0x07},
	{0x3205,0x8b},
	{0x3206,0x04},
	{0x3207,0x43},
	{0x320c,0x04},
	{0x320d,0x4c},
	{0x320e,0x04}, /*vts*/
	{0x320f,0x65},
	{0x3211,0x04},
	{0x3213,0x04},
	{0x3231,0x02},
	{0x3253,0x04},
	{0x3301,0x0a},
	{0x3302,0x10},
	{0x3304,0x48},
	{0x3305,0x00},
	{0x3306,0x68},
	{0x3308,0x20},
	{0x3309,0x98},
	{0x330a,0x00},
	{0x330b,0xe8},
	{0x330e,0x68},
	{0x3314,0x92},
	{0x3000,0xc0},
	{0x331e,0x41},
	{0x331f,0x91},
	{0x334c,0x10},
	{0x335d,0x60},
	{0x335e,0x02},
	{0x335f,0x06},
	{0x3364,0x16},
	{0x3366,0x92},
	{0x3367,0x10},
	{0x3368,0x04},
	{0x3369,0x00},
	{0x336a,0x00},
	{0x336b,0x00},
	{0x336d,0x03},
	{0x337c,0x08},
	{0x337d,0x0e},
	{0x337f,0x33},
	{0x3390,0x10},
	{0x3391,0x30},
	{0x3392,0x40},
	{0x3393,0x0a},
	{0x3394,0x0a},
	{0x3395,0x0a},
	{0x3396,0x08},
	{0x3397,0x30},
	{0x3398,0x3f},
	{0x3399,0x50},
	{0x339a,0x50},
	{0x339b,0x50},
	{0x339c,0x50},
	{0x33a2,0x0a},
	{0x33b9,0x0e},
	{0x33e1,0x08},
	{0x33e2,0x18},
	{0x33e3,0x18},
	{0x33e4,0x18},
	{0x33e5,0x10},
	{0x33e6,0x06},
	{0x33e7,0x02},
	{0x33e8,0x18},
	{0x33e9,0x10},
	{0x33ea,0x0c},
	{0x33eb,0x10},
	{0x33ec,0x04},
	{0x33ed,0x02},
	{0x33ee,0xa0},
	{0x33ef,0x08},
	{0x33f4,0x18},
	{0x33f5,0x10},
	{0x33f6,0x0c},
	{0x33f7,0x10},
	{0x33f8,0x06},
	{0x33f9,0x02},
	{0x33fa,0x18},
	{0x33fb,0x10},
	{0x33fc,0x0c},
	{0x33fd,0x10},
	{0x33fe,0x04},
	{0x33ff,0x02},
	{0x360f,0x01},
	{0x3622,0xf7},
	{0x3625,0x0a},
	{0x3627,0x02},
	{0x3630,0xa2},
	{0x3631,0x00},
	{0x3632,0xd8},
	{0x3633,0x33},
	{0x3635,0x20},
	{0x3638,0x24},
	{0x363a,0x80},
	{0x363b,0x02},
	{0x363e,0x22},
	{0x3670,0x40},
	{0x3671,0xf7},
	{0x3672,0xf7},
	{0x3673,0x07},
	{0x367a,0x40},
	{0x367b,0x7f},
	{0x36b5,0x40},
	{0x36b6,0x7f},
	{0x36c0,0x80},
	{0x36c1,0x9f},
	{0x36c2,0x9f},
	{0x36cc,0x22},
	{0x36cd,0x23},
	{0x36ce,0x30},
	{0x36d0,0x20},
	{0x36d1,0x40},
	{0x36d2,0x7f},
	{0x36ea,0x75},
	{0x36eb,0x0d},
	{0x36ec,0x13},
	{0x36ed,0x24},
	{0x36fa,0x5f},
	{0x36fb,0x1b},
	{0x36fc,0x10},
	{0x36fd,0x07},
	{0x3905,0xd8},
	{0x3907,0x01},
	{0x3908,0x11},
	{0x391b,0x83},
	{0x391d,0x2c},
	{0x391f,0x00},
	{0x3933,0x28},
	{0x3934,0xa6},
	{0x3940,0x70},
	{0x3942,0x08},
	{0x3943,0xbc},
	{0x3958,0x02},
	{0x3959,0x04},
	{0x3980,0x61},
	{0x3987,0x0b},
	{0x3990,0x00},
	{0x3991,0x00},
	{0x3992,0x00},
	{0x3993,0x00},
	{0x3994,0x00},
	{0x3995,0x00},
	{0x3996,0x00},
	{0x3997,0x00},
	{0x3998,0x00},
	{0x3999,0x00},
	{0x399a,0x00},
	{0x399b,0x00},
	{0x399c,0x00},
	{0x399d,0x00},
	{0x399e,0x00},
	{0x399f,0x00},
	{0x39a0,0x00},
	{0x39a1,0x00},
	{0x39a2,0x03},
	{0x39a3,0x30},
	{0x39a4,0x03},
	{0x39a5,0x60},
	{0x39a6,0x03},
	{0x39a7,0xa0},
	{0x39a8,0x03},
	{0x39a9,0xb0},
	{0x39aa,0x00},
	{0x39ab,0x00},
	{0x39ac,0x00},
	{0x39ad,0x20},
	{0x39ae,0x00},
	{0x39af,0x40},
	{0x39b0,0x00},
	{0x39b1,0x60},
	{0x39b2,0x00},
	{0x39b3,0x00},
	{0x39b4,0x08},
	{0x39b5,0x14},
	{0x39b6,0x20},
	{0x39b7,0x38},
	{0x39b8,0x38},
	{0x39b9,0x20},
	{0x39ba,0x14},
	{0x39bb,0x08},
	{0x39bc,0x08},
	{0x39bd,0x10},
	{0x39be,0x20},
	{0x39bf,0x30},
	{0x39c0,0x30},
	{0x39c1,0x20},
	{0x39c2,0x10},
	{0x39c3,0x08},
	{0x39c4,0x00},
	{0x39c5,0x80},
	{0x39c6,0x00},
	{0x39c7,0x80},
	{0x39c8,0x00},
	{0x39c9,0x00},
	{0x39ca,0x80},
	{0x39cb,0x00},
	{0x39cc,0x00},
	{0x39cd,0x00},
	{0x39ce,0x00},
	{0x39cf,0x00},
	{0x39d0,0x00},
	{0x39d1,0x00},
	{0x39e2,0x05},
	{0x39e3,0xeb},
	{0x39e4,0x07},
	{0x39e5,0xb6},
	{0x39e6,0x00},
	{0x39e7,0x3a},
	{0x39e8,0x3f},
	{0x39e9,0xb7},
	{0x39ea,0x02},
	{0x39eb,0x4f},
	{0x39ec,0x08},
	{0x39ed,0x00},
	{0x3e01,0x46},
	{0x3e02,0x10},
	{0x3e09,0x40},
	{0x3e14,0x31},
	{0x3e1b,0x3a},
	{0x3e26,0x40},
	{0x4401,0x1a},
	{0x4407,0xc0},
	{0x4418,0x34},
	{0x4500,0x18},
	{0x4501,0xb4},
	{0x4509,0x20},
	{0x4603,0x00},
	{0x4800,0x24},
	{0x4837,0x2b},
	{0x5000,0x0e},
	{0x550f,0x20},
	{0x36e9,0x51},
	{0x36f9,0x53},
	{0x0100,0x01},

	{SC2210_REG_END, 0x00},	/* END MARKER */
};

static struct tx_isp_sensor_win_setting sc2210_win_sizes[] = {
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc2210_init_regs_1920_1080_30fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sc2210_win_sizes[0];

static struct regval_list sc2210_stream_on_mipi[] = {
	{0x0100, 0x01},
	{SC2210_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc2210_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SC2210_REG_END, 0x00},	/* END MARKER */
};

int sc2210_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
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

int sc2210_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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

static int sc2210_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC2210_REG_END) {
		if (vals->reg_num == SC2210_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2210_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc2210_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC2210_REG_END) {
		if (vals->reg_num == SC2210_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2210_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc2210_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sc2210_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sc2210_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC2210_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc2210_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC2210_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc2210_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff) * 1;
	int again = (value & 0xffff0000) >> 16;
	ret += sc2210_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc2210_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc2210_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
	ret = sc2210_write(sd, 0x3e09, (unsigned char)(again & 0xff));
	ret += sc2210_write(sd, 0x3e08, (unsigned char)(((again >> 8) & 0xff)));

	if (ret < 0)
		return ret;
//	gain_val = again;

	return 0;
}
#if 0
static int sc2210_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
	ret += sc2210_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc2210_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc2210_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));

	if (ret < 0)
		return ret;

	return 0;
}

static int sc2210_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sc2210_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sc2210_write(sd, 0x3e08, (unsigned char)(((value >> 8) & 0xff)));
	if (ret < 0)
		return ret;
//	gain_val = value;

	return 0;
}

static int sc2210_set_logic(struct tx_isp_subdev *sd, int value)
{
	return 0;
	unsigned char reg0;
	unsigned int ret = 0;

	/* analog gain setting logic */
	ret = sc2210_read(sd, 0x3040, &reg0);
	if (0x40 == reg0) {
		if (gain_val < 0x740) {
			ret += sc2210_write(sd, 0x363c, 0x0e);
		} else if (gain_val >= 0x740) {
			ret += sc2210_write(sd, 0x363c, 0x07);
		}
	} else if (0x41 == reg0) {
		if (gain_val < 0x740) {
			ret += sc2210_write(sd, 0x363c, 0x0f);
		} else if (gain_val >= 0x740) {
			ret += sc2210_write(sd, 0x363c, 0x07);
		}
	} else {
		ret += sc2210_write(sd, 0x363c, 0x07);
	}
	if (gain_val >= 0xf60) { //6x
		ret += sc2210_write(sd, 0x5799, 0x7);
		dpc_flag = 2;
	}
	else if (gain_val <= 0xf40) {//4x
		ret += sc2210_write(sd, 0x5799, 0x00);
	}
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int sc2210_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc2210_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc2210_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;

	ret = sc2210_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc2210_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2210_write_array(sd, sc2210_stream_on_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc2210 stream on\n");
	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2210_write_array(sd, sc2210_stream_off_mipi);
		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc2210 stream off\n");
	}

	return ret;
}

static int sc2210_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned short hts = 0;
	unsigned short vts = 0;
	unsigned char tmp = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	sclk = SC2210_SUPPORT_30FPS_SCLK;
	ret = sc2210_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sc2210_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("err: sc2210 read err\n");
		return ret;
	}
	hts = ((hts << 8) + tmp);
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = sc2210_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc2210_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: sc2210_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sc2210_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int sc2210_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc2210_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"sc2210_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc2210_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc2210 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("sc2210 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc2210", sizeof("sc2210"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

#if 1
static int sc2210_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = -1;
	unsigned char val = 0x0;

	ret += sc2210_read(sd, 0x3221, &val);

	if(enable & 0x2)
		val |= 0x60;
	else
		val &= 0x9f;

	ret += sc2210_write(sd, 0x3221, val);

	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}
#endif

static int sc2210_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
	     	ret = sc2210_set_expo(sd, *(int*)arg);
		break;
/*
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = sc2210_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = sc2210_set_analog_gain(sd, *(int*)arg);
		break;
*/
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc2210_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc2210_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc2210_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2210_write_array(sd, sc2210_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2210_write_array(sd, sc2210_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc2210_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = sc2210_set_vflip(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_LOGIC:
//		if(arg)
//			ret = sc2210_set_logic(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int sc2210_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc2210_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc2210_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	sc2210_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc2210_core_ops = {
	.g_chip_ident = sc2210_g_chip_ident,
	.reset = sc2210_reset,
	.init = sc2210_init,
	/*.ioctl = sc2210_ops_ioctl,*/
	.g_register = sc2210_g_register,
	.s_register = sc2210_s_register,
};

static struct tx_isp_subdev_video_ops sc2210_video_ops = {
	.s_stream = sc2210_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc2210_sensor_ops = {
	.ioctl	= sc2210_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc2210_ops = {
	.core = &sc2210_core_ops,
	.video = &sc2210_video_ops,
	.sensor = &sc2210_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc2210",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc2210_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	/*
	  convert sensor-gain into isp-gain,
	*/
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sc2210_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc2210_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	ISP_WARNING("\n probe ok ------->sc2210\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int sc2210_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id sc2210_id[] = {
	{ "sc2210", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc2210_id);

static struct i2c_driver sc2210_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc2210",
	},
	.probe		= sc2210_probe,
	.remove		= sc2210_remove,
	.id_table	= sc2210_id,
};

static __init int init_sc2210(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init sc2210 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&sc2210_driver);
}

static __exit void exit_sc2210(void)
{
	private_i2c_del_driver(&sc2210_driver);
}

module_init(init_sc2210);
module_exit(exit_sc2210);

MODULE_DESCRIPTION("A low-level driver for SmartSens sc2210 sensors");
MODULE_LICENSE("GPL");
