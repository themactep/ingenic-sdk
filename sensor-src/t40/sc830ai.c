// SPDX-License-Identifier: GPL-2.0+
/*
 * sc830ai.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
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
#include <txx-funcs.h>

#define SENSOR_NAME "sc830ai"
#define SENSOR_CHIP_ID_H (0xc1)
#define SENSOR_CHIP_ID_L (0x43)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK_8M_FPS_30 (143892000)
#define SENSOR_SUPPORT_SCLK_8M_FPS_15 (71946000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_1
#define SENSOR_VERSION "H20220610a"

static int reset_gpio = GPIO_PC(27);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = GPIO_PC(25);
//static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static unsigned int expo_val = 0x031f0320;

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x4080, 0},
	{0x4082, 1500},
	{0x4084, 2886},
	{0x4086, 4342},
	{0x4088, 5776},
	{0x408a, 7101},
	{0x408c, 8494},
	{0x408e, 9781},
	{0x4090, 11136},
	{0x4092, 12471},
	{0x4094, 13706},
	{0x4096, 15005},
	{0x4098, 16287},
	{0x409a, 17474},
	{0x409c, 18723},
	{0x409e, 19879},
	{0x40a0, 21097},
	{0x40a2, 22300},
	{0x40a4, 23414},
	{0x40a6, 24587},
	{0x40a8, 25746},
	{0x40aa, 26820},
	{0x40ac, 27953},
	{0x40ae, 29002},
	{0x40b0, 30109},
	{0x40b2, 31203},
	{0x40b4, 32217},
	{0x40b6, 33287},
	{0x40b8, 34345},
	{0x40ba, 35326},
	{0x40bc, 36361},
	{0x40be, 37322},
	{0x40c0, 38336},
	{0x40c2, 39339},
	{0x40c4, 40270},
	{0x40c6, 41253},
	{0x40c8, 42226},
	{0x40ca, 43129},
	{0x40cc, 44082},
	{0x40ce, 44968},
	{0x40d0, 45904},
	{0x40d2, 46830},
	{0x40d4, 47690},
	{0x40d6, 48599},
	{0x40d8, 49500},
	{0x40da, 50336},
	{0x40dc, 51220},
	{0x40de, 52042},
	{0x40e0, 52910},
	{0x40e2, 53771},
	{0x40e4, 54571},
	{0x40e6, 55416},
	{0x40e8, 56254},
	{0x40ea, 57033},
	{0x40ec, 57857},
	{0x40ee, 58623},
	{0x40f0, 59433},
	{0x40f2, 60237},
	{0x40f4, 60984},
	{0x40f6, 61774},
	{0x40f8, 62558},
	{0x40fa, 63287},
	{0x40fc, 64059},
	{0x40fe, 64776},
	{0x4880, 65536},
	{0x4882, 67036},
	{0x4884, 68422},
	{0x4886, 69878},
	{0x4888, 71312},
	{0x488a, 72637},
	{0x488c, 74030},
	{0x488e, 75317},
	{0x4890, 76672},
	{0x4892, 78007},
	{0x4894, 79242},
	{0x4896, 80541},
	{0x4898, 81823},
	{0x489a, 83010},
	{0x489c, 84259},
	{0x489e, 85415},
	{0x48a0, 86633},
	{0x48a2, 87836},
	{0x48a4, 88950},
	{0x48a6, 90123},
	{0x48a8, 91282},
	{0x48aa, 92356},
	{0x48ac, 93489},
	{0x48ae, 94538},
	{0x48b0, 95645},
	{0x48b2, 96739},
	{0x48b4, 97753},
	{0x48b6, 98823},
	{0x48b8, 99881},
	{0x48ba, 100862},
	{0x48bc, 101897},
	{0x48be, 102858},
	{0x48c0, 103872},
	{0x48c2, 104875},
	{0x48c4, 105806},
	{0x48c6, 106789},
	{0x48c8, 107762},
	{0x48ca, 108665},
	{0x48cc, 109618},
	{0x48ce, 110504},
	{0x48d0, 111440},
	{0x48d2, 112366},
	{0x48d4, 113226},
	{0x48d6, 114135},
	{0x48d8, 115036},
	{0x48da, 115872},
	{0x48dc, 116756},
	{0x48de, 117578},
	{0x48e0, 118446},
	{0x48e2, 119307},
	{0x48e4, 120107},
	{0x48e6, 120952},
	{0x48e8, 121790},
	{0x48ea, 122569},
	{0x48ec, 123393},
	{0x48ee, 124159},
	{0x48f0, 124969},
	{0x48f2, 125773},
	{0x48f4, 126520},
	{0x48f6, 127310},
	{0x48f8, 128094},
	{0x48fa, 128823},
	{0x48fc, 129595},
	{0x48fe, 130312},
	{0x4980, 131072},
	{0x4982, 132572},
	{0x4984, 133958},
	{0x4986, 135414},
	{0x4988, 136848},
	{0x498a, 138173},
	{0x498c, 139566},
	{0x498e, 140853},
	{0x4990, 142208},
	{0x4992, 143543},
	{0x4994, 144778},
	{0x4996, 146077},
	{0x4998, 147359},
	{0x499a, 148546},
	{0x499c, 149795},
	{0x499e, 150951},
	{0x49a0, 152169},
	{0x49a2, 153372},
	{0x49a4, 154486},
	{0x49a6, 155659},
	{0x49a8, 156818},
	{0x49aa, 157892},
	{0x49ac, 159025},
	{0x49ae, 160074},
	{0x49b0, 161181},
	{0x49b2, 162275},
	{0x49b4, 163289},
	{0x49b6, 164359},
	{0x49b8, 165417},
	{0x49ba, 166398},
	{0x49bc, 167433},
	{0x49be, 168394},
	{0x49c0, 169408},
	{0x49c2, 170411},
	{0x49c4, 171342},
	{0x49c6, 172325},
	{0x49c8, 173298},
	{0x49ca, 174201},
	{0x49cc, 175154},
	{0x49ce, 176040},
	{0x49d0, 176976},
	{0x49d2, 177902},
	{0x49d4, 178762},
	{0x49d6, 179671},
	{0x49d8, 180572},
	{0x49da, 181408},
	{0x49dc, 182292},
	{0x49de, 183114},
	{0x49e0, 183982},
	{0x49e2, 184843},
	{0x49e4, 185643},
	{0x49e6, 186488},
	{0x49e8, 187326},
	{0x49ea, 188105},
	{0x49ec, 188929},
	{0x49ee, 189695},
	{0x49f0, 190505},
	{0x49f2, 191309},
	{0x49f4, 192056},
	{0x49f6, 192846},
	{0x49f8, 193630},
	{0x49fa, 194359},
	{0x49fc, 195131},
	{0x49fe, 195848},
	{0x4b80, 196608},
	{0x4b82, 198108},
	{0x4b84, 199494},
	{0x4b86, 200950},
	{0x4b88, 202384},
	{0x4b8a, 203709},
	{0x4b8c, 205102},
	{0x4b8e, 206389},
	{0x4b90, 207744},
	{0x4b92, 209079},
	{0x4b94, 210314},
	{0x4b96, 211613},
	{0x4b98, 212895},
	{0x4b9a, 214082},
	{0x4b9c, 215331},
	{0x4b9e, 216487},
	{0x4ba0, 217705},
	{0x4ba2, 218908},
	{0x4ba4, 220022},
	{0x4ba6, 221195},
	{0x4ba8, 222354},
	{0x4baa, 223428},
	{0x4bac, 224561},
	{0x4bae, 225610},
	{0x4bb0, 226717},
	{0x4bb2, 227811},
	{0x4bb4, 228825},
	{0x4bb6, 229895},
	{0x4bb8, 230953},
	{0x4bba, 231934},
	{0x4bbc, 232969},
	{0x4bbe, 233930},
	{0x4bc0, 234944},
	{0x4bc2, 235947},
	{0x4bc4, 236878},
	{0x4bc6, 237861},
	{0x4bc8, 238834},
	{0x4bca, 239737},
	{0x4bcc, 240690},
	{0x4bce, 241576},
	{0x4bd0, 242512},
	{0x4bd2, 243438},
	{0x4bd4, 244298},
	{0x4bd6, 245207},
	{0x4bd8, 246108},
	{0x4bda, 246944},
	{0x4bdc, 247828},
	{0x4bde, 248650},
	{0x4be0, 249518},
	{0x4be2, 250379},
	{0x4be4, 251179},
	{0x4be6, 252024},
	{0x4be8, 252862},
	{0x4bea, 253641},
	{0x4bec, 254465},
	{0x4bee, 255231},
	{0x4bf0, 256041},
	{0x4bf2, 256845},
	{0x4bf4, 257592},
	{0x4bf6, 258382},
	{0x4bf8, 259166},
	{0x4bfa, 259895},
	{0x4bfc, 260667},
	{0x4bfe, 261384},
	{0x4f80, 262144},
	{0x4f82, 263644},
	{0x4f84, 265030},
	{0x4f86, 266486},
	{0x4f88, 267920},
	{0x4f8a, 269245},
	{0x4f8c, 270638},
	{0x4f8e, 271925},
	{0x4f90, 273280},
	{0x4f92, 274615},
	{0x4f94, 275850},
	{0x4f96, 277149},
	{0x4f98, 278431},
	{0x4f9a, 279618},
	{0x4f9c, 280867},
	{0x4f9e, 282023},
	{0x4fa0, 283241},
	{0x4fa2, 284444},
	{0x4fa4, 285558},
	{0x4fa6, 286731},
	{0x4fa8, 287890},
	{0x4faa, 288964},
	{0x4fac, 290097},
	{0x4fae, 291146},
	{0x4fb0, 292253},
	{0x4fb2, 293347},
	{0x4fb4, 294361},
	{0x4fb6, 295431},
	{0x4fb8, 296489},
	{0x4fba, 297470},
	{0x4fbc, 298505},
	{0x4fbe, 299466},
	{0x4fc0, 300480},
	{0x4fc2, 301483},
	{0x4fc4, 302414},
	{0x4fc6, 303397},
	{0x4fc8, 304370},
	{0x4fca, 305273},
	{0x4fcc, 306226},
	{0x4fce, 307112},
	{0x4fd0, 308048},
	{0x4fd2, 308974},
	{0x4fd4, 309834},
	{0x4fd6, 310743},
	{0x4fd8, 311644},
	{0x4fda, 312480},
	{0x4fdc, 313364},
	{0x4fde, 314186},
	{0x4fe0, 315054},
	{0x4fe2, 315915},
	{0x4fe4, 316715},
	{0x4fe6, 317560},
	{0x4fe8, 318398},
	{0x4fea, 319177},
	{0x4fec, 320001},
	{0x4fee, 320767},
	{0x4ff0, 321577},
	{0x4ff2, 322381},
	{0x4ff4, 323128},
	{0x4ff6, 323918},
	{0x4ff8, 324702},
	{0x4ffa, 325431},
	{0x4ffc, 326203},
	{0x4ffe, 326920},
	{0x5f80, 327680},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0xc143,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 360, //720MHz for 8M@30FPS
		.lans = 4,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 3840,
		.image_theight = 2160,
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
	.max_again = 327680,
	.max_dgain = 0,
	.min_integration_time = 3,
	.min_integration_time_native = 3,
	.max_integration_time_native = 2275,
	.integration_time_limit = 2275,
	.total_width = 0x834 * 2,
	.total_height = 2284,
	.max_integration_time = 2275,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_3840_2160_30fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x02},
	{0x320c,0x08}, //hts = 0x834 = 2100
	{0x320d,0x34},
	{0x320e,0x08}, //vts = 0x8ec = 2284
	{0x320f,0xec},
	{0x3221,0x66}, //flip[6:5] mirror[2:1]
	{0x3281,0x80},
	{0x3301,0x0e},
	{0x3303,0x18},
	{0x3306,0x50},
	{0x3308,0x20},
	{0x330a,0x00},
	{0x330b,0xd8},
	{0x330c,0x20},
	{0x330e,0x40},
	{0x330f,0x08},
	{0x3314,0x16},
	{0x3317,0x07},
	{0x3319,0x0c},
	{0x3321,0x0c},
	{0x3324,0x09},
	{0x3325,0x09},
	{0x3327,0x16},
	{0x3328,0x10},
	{0x3329,0x1c},
	{0x332b,0x0d},
	{0x3333,0x10},
	{0x333e,0x0e},
	{0x3352,0x0c},
	{0x3353,0x0c},
	{0x335e,0x06},
	{0x335f,0x08},
	{0x3364,0x5e},
	{0x3366,0x01},
	{0x337c,0x02},
	{0x337d,0x0a},
	{0x3390,0x01},
	{0x3391,0x0b},
	{0x3392,0x1f},
	{0x3393,0x0e},
	{0x3394,0x30},
	{0x3395,0x30},
	{0x3396,0x01},
	{0x3397,0x0b},
	{0x3398,0x1f},
	{0x3399,0x0e},
	{0x339a,0x0e},
	{0x339b,0x30},
	{0x339c,0x30},
	{0x339f,0x0e},
	{0x33a2,0x04},
	{0x33ad,0x3c},
	{0x33af,0x68},
	{0x33b1,0x80},
	{0x33b2,0x58},
	{0x33b3,0x40},
	{0x33ba,0x0c},
	{0x33f9,0x80},
	{0x33fb,0xa0},
	{0x33fc,0x4b},
	{0x33fd,0x5f},
	{0x349f,0x03},
	{0x34a0,0x0e},
	{0x34a6,0x4b},
	{0x34a7,0x5f},
	{0x34a8,0x20},
	{0x34a9,0x10},
	{0x34aa,0x01},
	{0x34ab,0x10},
	{0x34ac,0x01},
	{0x34ad,0x28},
	{0x34f8,0x5f},
	{0x34f9,0x10},
	{0x3630,0xc8},
	{0x3632,0x46},
	{0x3633,0x33},
	{0x3637,0x24},
	{0x3638,0xc3},
	{0x363c,0x40},
	{0x363d,0x40},
	{0x363e,0x70},
	{0x3670,0x01},
	{0x3674,0xc6},
	{0x3675,0x8c},
	{0x3676,0x8c},
	{0x367c,0x4b},
	{0x367d,0x5f},
	{0x3698,0x82},
	{0x3699,0x8d},
	{0x369a,0x9c},
	{0x369b,0xba},
	{0x369e,0xba},
	{0x369f,0xba},
	{0x36a2,0x49},
	{0x36a3,0x4b},
	{0x36a4,0x4f},
	{0x36a5,0x5f},
	{0x36a6,0x5f},
	{0x36d0,0x01},
	{0x36ea,0x08},
	{0x36eb,0x04},
	{0x36ec,0x03},
	{0x36ed,0x22},
	{0x370f,0x01},
	{0x3721,0x9c},
	{0x3722,0x03},
	{0x3724,0x31},
	{0x37b0,0x03},
	{0x37b1,0x03},
	{0x37b2,0x83},
	{0x37b3,0x4b},
	{0x37b4,0x4f},
	{0x37fa,0x08},
	{0x37fb,0x30},
	{0x37fc,0x00},
	{0x37fd,0x04},
	{0x3903,0x40},
	{0x3905,0x4c},
	{0x391e,0x09},
	{0x3929,0x18},
	{0x3933,0x80},
	{0x3934,0x03},
	{0x3935,0x00},
	{0x3936,0x34},
	{0x3937,0x6a},
	{0x3938,0x69},
	{0x3e00,0x01},
	{0x3e01,0x1c},
	{0x3e02,0x60},
	{0x3e09,0x40},
	{0x440e,0x02},
	{0x4837,0x16},
	{0x5010,0x01},
	{0x57d9,0x00},
	{0x5ae0,0xfe},
	{0x5ae1,0x40},
	{0x5ae2,0x38},
	{0x5ae3,0x30},
	{0x5ae4,0x28},
	{0x5ae5,0x38},
	{0x5ae6,0x30},
	{0x5ae7,0x28},
	{0x5ae8,0x3f},
	{0x5ae9,0x34},
	{0x5aea,0x2c},
	{0x5aeb,0x3f},
	{0x5aec,0x34},
	{0x5aed,0x2c},
	{0x5aee,0xfe},
	{0x5aef,0x40},
	{0x5af4,0x38},
	{0x5af5,0x30},
	{0x5af6,0x28},
	{0x5af7,0x38},
	{0x5af8,0x30},
	{0x5af9,0x28},
	{0x5afa,0x3f},
	{0x5afb,0x34},
	{0x5afc,0x2c},
	{0x5afd,0x3f},
	{0x5afe,0x34},
	{0x5aff,0x2c},
	{0x5f00,0x05},
	{0x36e9,0x53},
	{0x37f9,0x27},
	//{0x0100,0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_3840_2160_15fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x09},
	{0x320c,0x08}, //hts 0x834=2100
	{0x320d,0x34},
	{0x320e,0x08}, //vts 0x8ec=2284
	{0x320f,0xec},
	{0x3221,0x66}, //flip[6:5] mirror[2:1]
	{0x3281,0x80},
	{0x3301,0x0e},
	{0x3303,0x18},
	{0x3306,0x50},
	{0x3308,0x20},
	{0x330a,0x00},
	{0x330b,0xd8},
	{0x330c,0x20},
	{0x330e,0x40},
	{0x330f,0x08},
	{0x3314,0x16},
	{0x3317,0x07},
	{0x3319,0x0c},
	{0x3321,0x0c},
	{0x3324,0x09},
	{0x3325,0x09},
	{0x3327,0x16},
	{0x3328,0x10},
	{0x3329,0x1c},
	{0x332b,0x0d},
	{0x3333,0x10},
	{0x333e,0x0e},
	{0x3352,0x0c},
	{0x3353,0x0c},
	{0x335e,0x06},
	{0x335f,0x08},
	{0x3364,0x5e},
	{0x3366,0x01},
	{0x337c,0x02},
	{0x337d,0x0a},
	{0x3390,0x01},
	{0x3391,0x0b},
	{0x3392,0x1f},
	{0x3393,0x0e},
	{0x3394,0x30},
	{0x3395,0x30},
	{0x3396,0x01},
	{0x3397,0x0b},
	{0x3398,0x1f},
	{0x3399,0x0e},
	{0x339a,0x0e},
	{0x339b,0x30},
	{0x339c,0x30},
	{0x339f,0x0e},
	{0x33a2,0x04},
	{0x33ad,0x3c},
	{0x33af,0x68},
	{0x33b1,0x80},
	{0x33b2,0x58},
	{0x33b3,0x40},
	{0x33ba,0x0c},
	{0x33f9,0x80},
	{0x33fb,0xa0},
	{0x33fc,0x4b},
	{0x33fd,0x5f},
	{0x349f,0x03},
	{0x34a0,0x0e},
	{0x34a6,0x4b},
	{0x34a7,0x5f},
	{0x34a8,0x20},
	{0x34a9,0x10},
	{0x34aa,0x01},
	{0x34ab,0x10},
	{0x34ac,0x01},
	{0x34ad,0x28},
	{0x34f8,0x5f},
	{0x34f9,0x10},
	{0x3630,0xc8},
	{0x3632,0x46},
	{0x3633,0x33},
	{0x3637,0x24},
	{0x3638,0xc3},
	{0x363c,0x40},
	{0x363d,0x40},
	{0x363e,0x70},
	{0x3670,0x01},
	{0x3674,0xc6},
	{0x3675,0x8c},
	{0x3676,0x8c},
	{0x367c,0x4b},
	{0x367d,0x5f},
	{0x3698,0x82},
	{0x3699,0x8d},
	{0x369a,0x9c},
	{0x369b,0xba},
	{0x369e,0xba},
	{0x369f,0xba},
	{0x36a2,0x49},
	{0x36a3,0x4b},
	{0x36a4,0x4f},
	{0x36a5,0x5f},
	{0x36a6,0x5f},
	{0x36d0,0x01},
	{0x36ea,0x08},
	{0x36eb,0x14},
	{0x36ec,0x03},
	{0x36ed,0x22},
	{0x370f,0x01},
	{0x3721,0x9c},
	{0x3722,0x03},
	{0x3724,0x31},
	{0x37b0,0x03},
	{0x37b1,0x03},
	{0x37b2,0x83},
	{0x37b3,0x4b},
	{0x37b4,0x4f},
	{0x37fa,0x08},
	{0x37fb,0x31},
	{0x37fc,0x10},
	{0x37fd,0x04},
	{0x3903,0x40},
	{0x3905,0x4c},
	{0x391e,0x09},
	{0x3929,0x18},
	{0x3933,0x80},
	{0x3934,0x03},
	{0x3935,0x00},
	{0x3936,0x34},
	{0x3937,0x6a},
	{0x3938,0x69},
	{0x3e00,0x01},
	{0x3e01,0x1c},
	{0x3e02,0x60},
	{0x3e09,0x40},
	{0x440e,0x02},
	{0x4837,0x2c},
	{0x5010,0x01},
	{0x57d9,0x00},
	{0x5ae0,0xfe},
	{0x5ae1,0x40},
	{0x5ae2,0x38},
	{0x5ae3,0x30},
	{0x5ae4,0x28},
	{0x5ae5,0x38},
	{0x5ae6,0x30},
	{0x5ae7,0x28},
	{0x5ae8,0x3f},
	{0x5ae9,0x34},
	{0x5aea,0x2c},
	{0x5aeb,0x3f},
	{0x5aec,0x34},
	{0x5aed,0x2c},
	{0x5aee,0xfe},
	{0x5aef,0x40},
	{0x5af4,0x38},
	{0x5af5,0x30},
	{0x5af6,0x28},
	{0x5af7,0x38},
	{0x5af8,0x30},
	{0x5af9,0x28},
	{0x5afa,0x3f},
	{0x5afb,0x34},
	{0x5afc,0x2c},
	{0x5afd,0x3f},
	{0x5afe,0x34},
	{0x5aff,0x2c},
	{0x5f00,0x05},
	{0x36e9,0x53},
	{0x37f9,0x27},
	//{0x0100,0x01},

	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 3840*2160 */
	{
		.width = 3840,
		.height = 2160,
		.fps = 15 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_3840_2160_15fps_mipi,
	},

	{
		.width = 3840,
		.height = 2160,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_3840_2160_30fps_mipi,
	}
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sensor_stream_on_mipi[] = {
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = buf,
		},
		[1] = {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = value,
		}
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg,
		 unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 3,
		.buf = buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 0
static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sensor_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sensor_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sensor_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = -1;
	int it = (value & 0xffff) * 2;
	int again = (value & 0xffff0000) >> 16;

	ret += sensor_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
	ret += sensor_write(sd, 0x3e07, (unsigned char)(again & 0xff));
	ret += sensor_write(sd, 0x3e09, (unsigned char)(((again >> 8) & 0xff)));
#if 0
		ret += sensor_write(sd,0x3812,0x00);
		if (again < 0x720) {
			ret += sensor_write(sd,0x3301,0x1c);
			ret += sensor_write(sd,0x3630,0x30);
			ret += sensor_write(sd,0x3633,0x23);
			ret += sensor_write(sd,0x3622,0xf6);
			ret += sensor_write(sd,0x363a,0x83);
		} else if (again < 0xf20) {
			ret += sensor_write(sd,0x3301,0x26);
			ret += sensor_write(sd,0x3630,0x23);
			ret += sensor_write(sd,0x3633,0x33);
			ret += sensor_write(sd,0x3622,0xf6);
			ret += sensor_write(sd,0x363a,0x87);
		} else if (again < 0x1f20) {
			ret += sensor_write(sd,0x3301,0x2c);
			ret += sensor_write(sd,0x3630,0x24);
			ret += sensor_write(sd,0x3633,0x43);
			ret += sensor_write(sd,0x3622,0xf6);
			ret += sensor_write(sd,0x363a,0x9f);
		} else if (again < 0x1f3f) {
			ret += sensor_write(sd,0x3301,0x38);
			ret += sensor_write(sd,0x3630,0x28);
			ret += sensor_write(sd,0x3633,0x43);
			ret += sensor_write(sd,0x3622,0xf6);
			ret += sensor_write(sd,0x363a,0x9f);
		} else {
			ret += sensor_write(sd,0x3301,0x44);
			ret += sensor_write(sd,0x3630,0x19);
			ret += sensor_write(sd,0x3633,0x55);
			ret += sensor_write(sd,0x3622,0x16);
			ret += sensor_write(sd,0x363a,0x9f);
		}
		ret += sensor_write(sd,0x3812,0x30);
		if (ret < 0)
			return ret;
#endif
	expo_val = value;

	return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
	ret = sensor_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0x0f));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sensor_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3e08, (unsigned char)((value & 0xff00) >> 8));
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	sensor->video.state = TX_ISP_MODULE_INIT;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sensor_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if (sensor->video.state == TX_ISP_MODULE_RUNNING) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
			ISP_WARNING("%s stream on\n", SENSOR_NAME);
		}
	}
	else {
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int max_fps;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch(sensor->info.default_boot) {
	case 0:
		sclk = SENSOR_SUPPORT_SCLK_8M_FPS_15;
		max_fps = TX_SENSOR_MAX_FPS_15;
		break;
	case 1:
		sclk = SENSOR_SUPPORT_SCLK_8M_FPS_30;
		max_fps = TX_SENSOR_MAX_FPS_30;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}

	ret += sensor_read(sd, 0x320c, &val);
	hts = val << 8;
	ret += sensor_read(sd, 0x320d, &val);
	hts = (hts | val);
	//hts = (hts | val) << 1;
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	//printk("%s() hts: 0x%04x, vts:0x%04x.\n", __func__, hts, vts);

	ret = sensor_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 9;
	sensor->video.attr->integration_time_limit = vts - 9;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 9;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
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

static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	struct clk *sclka;

        switch(info->default_boot) {
        case 0:
                wsize = &sensor_win_sizes[0];
                sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
                sensor_attr.mipi.clk = 360;
	        sensor_attr.again = 0;
                sensor_attr.integration_time = 0x11c6;
                break;
        case 1:
                wsize = &sensor_win_sizes[1];
                sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
                sensor_attr.mipi.clk = 720;
	        sensor_attr.again = 0;
                sensor_attr.integration_time = 0x11c6;
                break;
        default:
                ISP_ERROR("Have no this MCLK Source!!!\n");
        }

	switch(info->video_interface) {
        case TISP_SENSOR_VI_MIPI_CSI0:
            sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
            sensor_attr.mipi.index = 0;
            break;
        case TISP_SENSOR_VI_MIPI_CSI1:
            sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
            sensor_attr.mipi.index = 1;
            break;
        case TISP_SENSOR_VI_DVP:
            sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
            break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	switch(info->mclk) {
	case TISP_SENSOR_MCLK0:
		sclka = private_devm_clk_get(&client->dev, "mux_cim0");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim0");
		set_sensor_mclk_function(0);
		break;
	case TISP_SENSOR_MCLK1:
		sclka = private_devm_clk_get(&client->dev, "mux_cim1");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim1");
		set_sensor_mclk_function(1);
		break;
	case TISP_SENSOR_MCLK2:
		sclka = private_devm_clk_get(&client->dev, "mux_cim2");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim2");
		set_sensor_mclk_function(2);
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	rate = private_clk_get_rate(sensor->mclk);
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_prepare_enable(sensor->mclk);


	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	return 0;

err_get_mclk:
	return -1;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sensor_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an %s chip.\n",
			  client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}
	ISP_WARNING("%s chip found @ 0x%02x (%s)\n",
		    SENSOR_NAME, client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = sensor_set_expo(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		//if (arg)
		//	ret = sensor_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		//if (arg)
		//	ret = sensor_set_analog_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sensor_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sensor_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sensor_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sensor_write_array(sd, sensor_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int sensor_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = sensor_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sensor_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sensor_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sensor_core_ops = {
	.g_chip_ident = sensor_g_chip_ident,
	.reset = sensor_reset,
	.init = sensor_init,
	.g_register = sensor_g_register,
	.s_register = sensor_s_register,
};

static struct tx_isp_subdev_video_ops sensor_video_ops = {
	.s_stream = sensor_s_stream,
};

static struct tx_isp_subdev_sensor_ops sensor_sensor_ops = {
	.ioctl = sensor_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.sensor = &sensor_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = SENSOR_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};


static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));

    	sd = &sensor->sd;
	video = &sensor->video;
	sensor->dev = &client->dev;
	sensor_attr.expo_fs = 1;
	sensor->video.attr = &sensor_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	private_devm_clk_put(&client->dev, sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver = {
		.owner = NULL,
		.name = SENSOR_NAME,
	},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

static __init int init_sensor(void)
{
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
