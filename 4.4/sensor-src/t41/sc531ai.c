// SPDX-License-Identifier: GPL-2.0+
/*
 * sc531ai.c
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps     interface              mode
 *   0          2880*1620       30      mipi_2lane            linear
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
#include <sensor-info.h>

#define SENSOR_NAME "sc531ai"
#define SENSOR_CHIP_ID_H (0x9e)
#define SENSOR_CHIP_ID_L (0x39)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_PCLK_FPS_30 (158400000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20230914a-roy"

static int reset_gpio = GPIO_PA(18);
static int pwdn_gpio = -1;

static int shvflip = 1;
struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int reg_value;
	unsigned int gain;
};

static struct again_lut sensor_again_lut[] = {
	{0, 0x000080, 0},
	{1, 0x000084, 2886},
	{2, 0x000088, 5776},
	{3, 0x00008c, 8494},
	{4, 0x000090, 11136},
	{5, 0x000094, 13706},
	{6, 0x000098, 16287},
	{7, 0x00009c, 18723},
	{8, 0x0000a0, 21097},
	{9, 0x0000a4, 23414},
	{10, 0x0000a8, 25746},
	{11, 0x0000ac, 27953},
	{12, 0x0000b0, 30109},
	{13, 0x0000b4, 32217},
	{14, 0x0000b8, 34345},
	{15, 0x0000bc, 36361},
	{16, 0x0000c0, 38336},
	{17, 0x0000c4, 40270},
	{18, 0x0000c8, 42226},
	{19, 0x0000cc, 44082},
	{20, 0x0000d0, 45904},
	{21, 0x0000d4, 47690},
	{22, 0x0000d8, 49500},
	{23, 0x0000dc, 51220},
	{24, 0x0000e0, 52910},
	{25, 0x0000e4, 54571},
	{26, 0x0000e8, 56254},
	{27, 0x0000ec, 57857},
	{28, 0x0000f0, 59433},
	{29, 0x0000f4, 60984},
	{30, 0x0000f8, 62558},
	{31, 0x0000fc, 64059},
	{32, 0x010080, 65536},
	{33, 0x010084, 68422},
	{34, 0x010088, 71312},
	{35, 0x01008c, 74030},
	{36, 0x010090, 76672},
	{37, 0x010094, 79242},
	{38, 0x010098, 81823},
	{39, 0x01009c, 84259},
	{40, 0x0100a0, 86633},
	{41, 0x0100a4, 88950},
	{42, 0x0100a8, 91282},
	{43, 0x0100ac, 93489},
	{44, 0x0100b0, 95645},
	{45, 0x0100b4, 97753},
	{46, 0x0100b8, 99881},
	{47, 0x0100bc, 101897},
	{48, 0x0100c0, 103872},
	{49, 0x0100c4, 105806},
	{50, 0x0100c8, 107762},
	{51, 0x0100cc, 109618},
	{52, 0x0100d0, 111440},
	{53, 0x0100d4, 113226},
	{54, 0x0100d8, 115036},
	{55, 0x0100dc, 116756},
	{56, 0x0100e0, 118446},
	{57, 0x0100e4, 120107},
	{58, 0x0100e8, 121790},
	{59, 0x0100ec, 123393},
	{60, 0x0100f0, 124969},
	{61, 0x0100f4, 126520},
	{62, 0x0100f8, 128094},
	{63, 0x0100fc, 129595},
	{64, 0x480080, 147915},
	{65, 0x480084, 150801},
	{66, 0x480088, 153691},
	{67, 0x48008c, 156409},
	{68, 0x480090, 159051},
	{69, 0x480094, 161621},
	{70, 0x480098, 164203},
	{71, 0x48009c, 166638},
	{72, 0x4800a0, 169013},
	{73, 0x4800a4, 171329},
	{74, 0x4800a8, 173662},
	{75, 0x4800ac, 175868},
	{76, 0x4800b0, 178024},
	{77, 0x4800b4, 180132},
	{78, 0x4800b8, 182260},
	{79, 0x4800bc, 184277},
	{80, 0x4800c0, 186251},
	{81, 0x4800c4, 188185},
	{82, 0x4800c8, 190141},
	{83, 0x4800cc, 191998},
	{84, 0x4800d0, 193819},
	{85, 0x4800d4, 195606},
	{86, 0x4800d8, 197415},
	{87, 0x4800dc, 199136},
	{88, 0x4800e0, 200826},
	{89, 0x4800e4, 202486},
	{90, 0x4800e8, 204170},
	{91, 0x4800ec, 205773},
	{92, 0x4800f0, 207349},
	{93, 0x4800f4, 208899},
	{94, 0x4800f8, 210474},
	{95, 0x4800fc, 211974},
	{96, 0x490080, 213451},
	{97, 0x490084, 216337},
	{98, 0x490088, 219227},
	{99, 0x49008c, 221945},
	{100, 0x490090, 224587},
	{101, 0x490094, 227157},
	{102, 0x490098, 229739},
	{103, 0x49009c, 232174},
	{104, 0x4900a0, 234549},
	{105, 0x4900a4, 236865},
	{106, 0x4900a8, 239198},
	{107, 0x4900ac, 241404},
	{108, 0x4900b0, 243560},
	{109, 0x4900b4, 245668},
	{110, 0x4900b8, 247796},
	{111, 0x4900bc, 249813},
	{112, 0x4900c0, 251787},
	{113, 0x4900c4, 253721},
	{114, 0x4900c8, 255677},
	{115, 0x4900cc, 257534},
	{116, 0x4900d0, 259355},
	{117, 0x4900d4, 261142},
	{118, 0x4900d8, 262951},
	{119, 0x4900dc, 264672},
	{120, 0x4900e0, 266362},
	{121, 0x4900e4, 268022},
	{122, 0x4900e8, 269706},
	{123, 0x4900ec, 271309},
	{124, 0x4900f0, 272885},
	{125, 0x4900f4, 274435},
	{126, 0x4900f8, 276010},
	{127, 0x4900fc, 277510},
	{128, 0x4b0080, 278987},
	{129, 0x4b0084, 281873},
	{130, 0x4b0088, 284763},
	{131, 0x4b008c, 287481},
	{132, 0x4b0090, 290123},
	{133, 0x4b0094, 292693},
	{134, 0x4b0098, 295275},
	{135, 0x4b009c, 297710},
	{136, 0x4b00a0, 300085},
	{137, 0x4b00a4, 302401},
	{138, 0x4b00a8, 304734},
	{139, 0x4b00ac, 306940},
	{140, 0x4b00b0, 309096},
	{141, 0x4b00b4, 311204},
	{142, 0x4b00b8, 313332},
	{143, 0x4b00bc, 315349},
	{144, 0x4b00c0, 317323},
	{145, 0x4b00c4, 319257},
	{146, 0x4b00c8, 321213},
	{147, 0x4b00cc, 323070},
	{148, 0x4b00d0, 324891},
	{149, 0x4b00d4, 326678},
	{150, 0x4b00d8, 328487},
	{151, 0x4b00dc, 330208},
	{152, 0x4b00e0, 331898},
	{153, 0x4b00e4, 333558},
	{154, 0x4b00e8, 335242},
	{155, 0x4b00ec, 336845},
	{156, 0x4b00f0, 338421},
	{157, 0x4b00f4, 339971},
	{158, 0x4b00f8, 341546},
	{159, 0x4b00fc, 343046},
	{160, 0x4f0080, 344523},
	{161, 0x4f0084, 347409},
	{162, 0x4f0088, 350299},
	{163, 0x4f008c, 353017},
	{164, 0x4f0090, 355659},
	{165, 0x4f0094, 358229},
	{166, 0x4f0098, 360811},
	{167, 0x4f009c, 363246},
	{168, 0x4f00a0, 365621},
	{169, 0x4f00a4, 367937},
	{170, 0x4f00a8, 370270},
	{171, 0x4f00ac, 372476},
	{172, 0x4f00b0, 374632},
	{173, 0x4f00b4, 376740},
	{174, 0x4f00b8, 378868},
	{175, 0x4f00bc, 380885},
	{176, 0x4f00c0, 382859},
	{177, 0x4f00c4, 384793},
	{178, 0x4f00c8, 386749},
	{179, 0x4f00cc, 388606},
	{180, 0x4f00d0, 390427},
	{181, 0x4f00d4, 392214},
	{182, 0x4f00d8, 394023},
	{183, 0x4f00dc, 395744},
	{184, 0x4f00e0, 397434},
	{185, 0x4f00e4, 399094},
	{186, 0x4f00e8, 400778},
	{187, 0x4f00ec, 402381},
	{188, 0x4f00f0, 403957},
	{189, 0x4f00f4, 405507},
	{190, 0x4f00f8, 407082},
	{191, 0x4f00fc, 408582},
	{192, 0x5f0080, 410059},
	{193, 0x5f0084, 412945},
	{194, 0x5f0088, 415835},
	{195, 0x5f008c, 418553},
	{196, 0x5f0090, 421195},
	{197, 0x5f0094, 423765},
	{198, 0x5f0098, 426347},
	{199, 0x5f009c, 428782},
	{200, 0x5f00a0, 431157},
	{201, 0x5f00a4, 433473},
	{202, 0x5f00a8, 435806},
	{203, 0x5f00ac, 438012},
	{204, 0x5f00b0, 440168},
	{205, 0x5f00b4, 442276},
	{206, 0x5f00b8, 444404},
	{207, 0x5f00bc, 446421},
	{208, 0x5f00c0, 448395},
	{209, 0x5f00c4, 450329},
	{210, 0x5f00c8, 452285},
	{211, 0x5f00cc, 454142},
	{212, 0x5f00d0, 455963},
	{213, 0x5f00d4, 457750},
	{214, 0x5f00d8, 459559},
	{215, 0x5f00dc, 461280},
	{216, 0x5f00e0, 462970},
	{217, 0x5f00e4, 464630},
	{218, 0x5f00e8, 466314},
	{219, 0x5f00ec, 467917},
	{220, 0x5f00f0, 469493},
	{221, 0x5f00f4, 471043},
	{222, 0x5f00f8, 472618},
	{223, 0x5f00fc, 474118},
	{224, 0x5f0180, 475595},
	{225, 0x5f0184, 478527},
	{226, 0x5f0188, 481327},
	{227, 0x5f018c, 484089},
	{228, 0x5f0190, 486731},
	{229, 0x5f0194, 489342},
	{230, 0x5f0198, 491843},
	{231, 0x5f019c, 494318},
	{232, 0x5f01a0, 496693},
	{233, 0x5f01a4, 499046},
	{234, 0x5f01a8, 501306},
	{235, 0x5f01ac, 503548},
	{236, 0x5f01b0, 505704},
	{237, 0x5f01b4, 507846},
	{238, 0x5f01b8, 509907},
	{239, 0x5f01bc, 511957},
	{240, 0x5f01c0, 513931},
	{241, 0x5f01c4, 515896},
	{242, 0x5f01c8, 517791},
	{243, 0x5f01cc, 519678},
	{244, 0x5f01d0, 521499},
	{245, 0x5f01d4, 523314},
	{246, 0x5f01d8, 525067},
	{247, 0x5f01dc, 526816},
	{248, 0x5f01e0, 528506},
	{249, 0x5f01e4, 530192},
	{250, 0x5f01e8, 531824},
	{251, 0x5f01ec, 533453},
	{252, 0x5f01f0, 535029},
	{253, 0x5f01f4, 536604},
	{254, 0x5f01f8, 538129},
	{255, 0x5f01fc, 539654},
};

static struct tx_isp_sensor_attribute sensor_attr;

#if 0
unsigned int sensor_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;
	*sensor_it = expo;

	return isp_it;
}
#endif

static unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

static unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return 0;
}

static struct tx_isp_mipi_bus sensor_mipi = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 792,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
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
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

static struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = 0x9e39,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 539654,
	.max_dgain = 0,
	.min_integration_time = 3,
	.min_integration_time_native = 3,
	.max_integration_time_native = 0x339 - 5,
	.integration_time_limit = 0x339 - 5,
	.total_width = 0x640 * 2,
	.total_height = 0x339,
	.max_integration_time = 0x339 - 5,
	//.one_line_expr_in_us = 20,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_2880_1620_30fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x3018, 0x32},
	{0x3019, 0x0c},
	{0x301f, 0x0d},
	{0x3250, 0x40},
	{0x3251, 0x98},
	{0x3253, 0x0c},
	{0x325f, 0x20},
	{0x3301, 0x08},
	{0x3304, 0x50},
	{0x3306, 0x88},
	{0x3308, 0x14},
	{0x3309, 0x70},
	{0x330a, 0x00},
	{0x330b, 0xf8},
	{0x330d, 0x10},
	{0x331e, 0x41},
	{0x331f, 0x61},
	{0x3333, 0x10},
	{0x335d, 0x60},
	{0x335e, 0x06},
	{0x335f, 0x08},
	{0x3364, 0x56},
	{0x3366, 0x01},
	{0x337c, 0x02},
	{0x337d, 0x0a},
	{0x3390, 0x01},
	{0x3391, 0x03},
	{0x3392, 0x07},
	{0x3393, 0x08},
	{0x3394, 0x08},
	{0x3395, 0x08},
	{0x3396, 0x40},
	{0x3397, 0x48},
	{0x3398, 0x4b},
	{0x3399, 0x08},
	{0x339a, 0x08},
	{0x339b, 0x08},
	{0x339c, 0x1d},
	{0x33a2, 0x04},
	{0x33ae, 0x30},
	{0x33af, 0x50},
	{0x33b1, 0x80},
	{0x33b2, 0x48},
	{0x33b3, 0x30},
	{0x349f, 0x02},
	{0x34a6, 0x48},
	{0x34a7, 0x4b},
	{0x34a8, 0x30},
	{0x34a9, 0x18},
	{0x34f8, 0x5f},
	{0x34f9, 0x08},
	{0x3632, 0x48},
	{0x3633, 0x32},
	{0x3637, 0x29},
	{0x3638, 0xc1},
	{0x363b, 0x20},
	{0x363d, 0x02},
	{0x3670, 0x09},
	{0x3674, 0x8b},
	{0x3675, 0xc6},
	{0x3676, 0x8b},
	{0x367c, 0x40},
	{0x367d, 0x48},
	{0x3690, 0x32},
	{0x3691, 0x43},
	{0x3692, 0x33},
	{0x3693, 0x40},
	{0x3694, 0x4b},
	{0x3698, 0x85},
	{0x3699, 0x8f},
	{0x369a, 0xa0},
	{0x369b, 0xc3},
	{0x36a2, 0x49},
	{0x36a3, 0x4b},
	{0x36a4, 0x4f},
	{0x36d0, 0x01},
	{0x36ea, 0x0b},
	{0x36eb, 0x04},
	{0x36ec, 0x03},
	{0x36ed, 0x34},
	{0x370f, 0x01},
	{0x3722, 0x00},
	{0x3728, 0x10},
	{0x37b0, 0x03},
	{0x37b1, 0x03},
	{0x37b2, 0x83},
	{0x37b3, 0x48},
	{0x37b4, 0x49},
	{0x37fa, 0x0b},
	{0x37fb, 0x24},
	{0x37fc, 0x01},
	{0x37fd, 0x34},
	{0x3901, 0x00},
	{0x3902, 0xc5},
	{0x3904, 0x08},
	{0x3905, 0x8c},
	{0x3909, 0x00},
	{0x391d, 0x04},
	{0x391f, 0x44},
	{0x3926, 0x21},
	{0x3929, 0x18},
	{0x3933, 0x82},
	{0x3934, 0x0a},
	{0x3937, 0x5f},
	{0x3939, 0x00},
	{0x393a, 0x00},
	{0x39dc, 0x02},
	{0x3e01, 0xcd},
	{0x3e02, 0xa0},
	{0x440e, 0x02},
	{0x4509, 0x20},
	{0x4837, 0x14},
	{0x5010, 0x10},
	{0x5780, 0x66},
	{0x578d, 0x40},
	{0x5799, 0x06},
	{0x57ad, 0x00},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x30},
	{0x5ae3, 0x2a},
	{0x5ae4, 0x24},
	{0x5ae5, 0x30},
	{0x5ae6, 0x2a},
	{0x5ae7, 0x24},
	{0x5ae8, 0x3c},
	{0x5ae9, 0x30},
	{0x5aea, 0x28},
	{0x5aeb, 0x3c},
	{0x5aec, 0x30},
	{0x5aed, 0x28},
	{0x5aee, 0xfe},
	{0x5aef, 0x40},
	{0x5af4, 0x30},
	{0x5af5, 0x2a},
	{0x5af6, 0x24},
	{0x5af7, 0x30},
	{0x5af8, 0x2a},
	{0x5af9, 0x24},
	{0x5afa, 0x3c},
	{0x5afb, 0x30},
	{0x5afc, 0x28},
	{0x5afd, 0x3c},
	{0x5afe, 0x30},
	{0x5aff, 0x28},
	{0x36e9, 0x54},
	{0x37f9, 0x54},
	{SENSOR_REG_DELAY, 0x10},
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list sensor_init_regs_1440_810_60fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x3018, 0x32},
	{0x3019, 0x0c},
	{0x301f, 0x6f},
	{0x3208, 0x05},
	{0x3209, 0xa0},
	{0x320a, 0x03},
	{0x320b, 0x2a},
	{0x320e, 0x03},
	{0x320f, 0x39},
	{0x3211, 0x02},
	{0x3213, 0x02},
	{0x3215, 0x31},
	{0x3220, 0x01},
	{0x3250, 0x40},
	{0x3251, 0x98},
	{0x3253, 0x0c},
	{0x325f, 0x20},
	{0x3301, 0x08},
	{0x3304, 0x50},
	{0x3306, 0x88},
	{0x3308, 0x14},
	{0x3309, 0x70},
	{0x330a, 0x00},
	{0x330b, 0xf8},
	{0x330d, 0x10},
	{0x330e, 0x42},
	{0x331e, 0x41},
	{0x331f, 0x61},
	{0x3333, 0x10},
	{0x335d, 0x60},
	{0x335e, 0x06},
	{0x335f, 0x08},
	{0x3364, 0x56},
	{0x3366, 0x01},
	{0x337c, 0x02},
	{0x337d, 0x0a},
	{0x3390, 0x01},
	{0x3391, 0x03},
	{0x3392, 0x07},
	{0x3393, 0x08},
	{0x3394, 0x08},
	{0x3395, 0x08},
	{0x3396, 0x40},
	{0x3397, 0x48},
	{0x3398, 0x4b},
	{0x3399, 0x08},
	{0x339a, 0x08},
	{0x339b, 0x08},
	{0x339c, 0x1d},
	{0x33a2, 0x04},
	{0x33ae, 0x30},
	{0x33af, 0x50},
	{0x33b1, 0x80},
	{0x33b2, 0x48},
	{0x33b3, 0x30},
	{0x349f, 0x02},
	{0x34a6, 0x48},
	{0x34a7, 0x4b},
	{0x34a8, 0x30},
	{0x34a9, 0x18},
	{0x34f8, 0x5f},
	{0x34f9, 0x08},
	{0x3632, 0x48},
	{0x3633, 0x32},
	{0x3637, 0x27},
	{0x3638, 0xc1},
	{0x363b, 0x20},
	{0x363d, 0x02},
	{0x3670, 0x09},
	{0x3674, 0x8b},
	{0x3675, 0xc6},
	{0x3676, 0x8b},
	{0x367c, 0x40},
	{0x367d, 0x48},
	{0x3690, 0x32},
	{0x3691, 0x43},
	{0x3692, 0x33},
	{0x3693, 0x40},
	{0x3694, 0x4b},
	{0x3698, 0x85},
	{0x3699, 0x8f},
	{0x369a, 0xa0},
	{0x369b, 0xc3},
	{0x36a2, 0x49},
	{0x36a3, 0x4b},
	{0x36a4, 0x4f},
	{0x36d0, 0x01},
	{0x36ea, 0x0b},
	{0x36eb, 0x04},
	{0x36ec, 0x03},
	{0x36ed, 0x04},
	{0x370f, 0x01},
	{0x3722, 0x00},
	{0x3728, 0x10},
	{0x37b0, 0x03},
	{0x37b1, 0x03},
	{0x37b2, 0x83},
	{0x37b3, 0x48},
	{0x37b4, 0x49},
	{0x37fa, 0xcb},
	{0x37fb, 0x25},
	{0x37fc, 0x01},
	{0x37fd, 0x04},
	{0x3901, 0x00},
	{0x3902, 0xc5},
	{0x3904, 0x08},
	{0x3905, 0x8c},
	{0x3909, 0x00},
	{0x391d, 0x04},
	{0x391f, 0x44},
	{0x3926, 0x21},
	{0x3929, 0x18},
	{0x3933, 0x82},
	{0x3934, 0x0a},
	{0x3937, 0x5f},
	{0x3939, 0x00},
	{0x393a, 0x00},
	{0x39dc, 0x02},
	{0x3e01, 0x66},
	{0x3e02, 0x80},
	{0x440e, 0x02},
	{0x4509, 0x20},
	{0x4837, 0x28},
	{0x5000, 0x46},
	{0x5010, 0x10},
	{0x5780, 0x66},
	{0x578d, 0x40},
	{0x5799, 0x06},
	{0x57ad, 0x00},
	{0x5900, 0x01},
	{0x5901, 0x04},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x30},
	{0x5ae3, 0x2a},
	{0x5ae4, 0x24},
	{0x5ae5, 0x30},
	{0x5ae6, 0x2a},
	{0x5ae7, 0x24},
	{0x5ae8, 0x3c},
	{0x5ae9, 0x30},
	{0x5aea, 0x28},
	{0x5aeb, 0x3c},
	{0x5aec, 0x30},
	{0x5aed, 0x28},
	{0x5aee, 0xfe},
	{0x5aef, 0x40},
	{0x5af4, 0x30},
	{0x5af5, 0x2a},
	{0x5af6, 0x24},
	{0x5af7, 0x30},
	{0x5af8, 0x2a},
	{0x5af9, 0x24},
	{0x5afa, 0x3c},
	{0x5afb, 0x30},
	{0x5afc, 0x28},
	{0x5afd, 0x3c},
	{0x5afe, 0x30},
	{0x5aff, 0x28},
	{0x36e9, 0x57},
	{0x37f9, 0x33},
	{SENSOR_REG_DELAY, 0x10},
	//{0x0100,0x01},
	{SENSOR_REG_END, 0x00},/* END MARKER */

};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 2880,
		.height = 1620,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2880_1620_30fps_mipi,
	},
	{
		.width = 1440,
		.height = 810,
		.fps = 60 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1440_810_60fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

static struct regval_list sensor_stream_on[] = {
	{SENSOR_REG_DELAY, 0x10},
	//{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	//{0x0100, 0x00},
	{SENSOR_REG_END, 0x00},
};

static int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,
		       unsigned char *value) {
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

static int sensor_write(struct tx_isp_subdev *sd, uint16_t reg,
			unsigned char value) {
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

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
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

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident) {
	int ret;
	unsigned char v;

	ret = sensor_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_expo(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	int it = (value & 0xffff) * 2;
	int again = sensor_again_lut[(value & 0xffff0000) >> 16].reg_value;

	ret = sensor_write(sd, 0x3e00, (unsigned char) ((it >> 12) & 0xf));
	ret += sensor_write(sd, 0x3e01, (unsigned char) ((it >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char) ((it & 0x0f) << 4));

	ret += sensor_write(sd, 0x3e09, (unsigned char) (again >> 16 & 0xff));
	ret += sensor_write(sd, 0x3e06, (unsigned char) ((again >> 8 & 0xff)));
	ret += sensor_write(sd, 0x3e07, (unsigned char) ((again & 0xff)));

	if (ret != 0) {
		ISP_ERROR("err: sc530ai write err %d\n", __LINE__);
		return ret;
	}

	return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff) * 2;
    ret = sensor_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
    if (ret < 0)
	return ret;

    return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
    int ret = 0;
	int again = sensor_again_lut[(value & 0xffff0000) >> 16].reg_value;
	ret += sensor_write(sd, 0x3e09, (unsigned char)(again >> 16 & 0xff));
	ret += sensor_write(sd, 0x3e06, (unsigned char)((again >> 8 & 0xff)));
	ret += sensor_write(sd, 0x3e07, (unsigned char)((again & 0xff)));
    if (ret < 0)
	return ret;

    return 0;
}
#endif

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_set_attr(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	sensor->video.max_fps = SENSOR_OUTPUT_MAX_FPS << 16 | 1;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable) {
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		return ISP_SUCCESS;
	} else {
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = TISP_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		ret = sensor_write_array(sd, wsize->regs);
		if (ret)
			return ret;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
		sensor->priv = wsize;
	}
	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
			ret = sensor_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sensor_write_array(sd, sensor_stream_on);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("%s stream on\n", SENSOR_NAME);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("%s stream on\n", SENSOR_NAME);
	} else {
		ret = sensor_write_array(sd, sensor_stream_off);
		sensor->video.state = TX_ISP_MODULE_INIT;
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;
//316800000
	return 0;
	switch (info->default_boot) {
		case 0:
			sclk = SENSOR_SUPPORT_PCLK_FPS_30;
			break;
		case 1:
			sclk = 316800000;
			break;
		default:
			ISP_ERROR("Have no this setting!!!\n");
	}
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}

	ret = sensor_read(sd, 0x320c, &val);
	hts = val;
	ret += sensor_read(sd, 0x320d, &val);
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return -1;
	}
	hts = ((hts << 8) + val) << 1;

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += sensor_write(sd, 0x320f, (unsigned char) (vts & 0xff));
	ret += sensor_write(sd, 0x320e, (unsigned char) (vts >> 8));
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 5;
	sensor->video.attr->integration_time_limit = vts - 5;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 5;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value) {
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

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = -1;
	unsigned char val = 0x0;

	ret += sensor_read(sd, 0x3221, &val);

	/* 2'b00 Normal; 2'b01 mirror; 2'b10 flip; 2'b11 mirror & flip */
	switch (enable) {
		case 0:
			val = 0;
			break;
		case 1:
			val = 0x06;
			break;
		case 2:
			val = 0x60;
			break;
		case 3:
			val = 0x66;
			break;
	}

	ret += sensor_write(sd, 0x3221, val);

	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

struct clk *sclka;

static int sensor_attr_check(struct tx_isp_subdev *sd) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = 0;

	switch (info->default_boot) {
		case 0:
			wsize = &sensor_win_sizes[0];
			memcpy(&(sensor_attr.mipi), &sensor_mipi, sizeof(sensor_mipi));
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.max_integration_time_native = 0x339 - 5;
			sensor_attr.integration_time_limit = 0x339 - 5;
			sensor_attr.total_width = 0x640 * 2;
			sensor_attr.total_height = 0x339;
			sensor_attr.total_width = 2880;//4200
			sensor_attr.total_height = 1620;//2250
			sensor_attr.again = 0;
			sensor_attr.integration_time = 0xcda;
			sensor_attr.max_integration_time = 0x339 - 5;
			sensor_attr.min_integration_time = 3;
			printk("=================> linear is ok");
			break;
		case 1:
			wsize = &sensor_win_sizes[1];
			memcpy(&(sensor_attr.mipi), &sensor_mipi, sizeof(sensor_mipi));
			sensor_attr.mipi.clk = 396;
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.max_integration_time_native = 0x339 - 5;
			sensor_attr.integration_time_limit = 0x339 - 5;
			sensor_attr.total_width = 0x640 * 2;
			sensor_attr.total_height = 0x339;
			sensor_attr.mipi.image_twidth = 1440,
			sensor_attr.mipi.image_theight = 810,
			sensor_attr.again = 0;
			//sensor_attr.integration_time = 0xcda;
			sensor_attr.max_integration_time = 0x339 - 5;
			sensor_attr.min_integration_time = 3;
			printk("=================> 60fps linear is ok");
			break;
		default:
			ISP_ERROR("Have no this setting!!!\n");
	}

	switch (info->video_interface) {
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
	rate = private_clk_get_rate(sensor->mclk);

	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_prepare_enable(sensor->mclk);
	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio, "sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", pwdn_gpio);
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
	ISP_WARNING("sensor driver version %s\n", SENSOR_VERSION);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg) {
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd) {
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
			ret = sensor_write_array(sd, sensor_stream_off);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			ret = sensor_write_array(sd, sensor_stream_on);
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if (arg)
				ret = sensor_set_fps(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_VFLIP:
			if (arg)
				ret = sensor_set_vflip(sd, sensor_val->value);
			break;
		default:
			break;
	}

	return ret;
}

static int sensor_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg) {
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

static int sensor_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg) {
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
static u64 tx_isp_module_dma_mask = ~(u64) 0;
static struct platform_device sensor_platform_device = {
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
			const struct i2c_device_id *id) {
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
	sensor->dev = &client->dev;
	sensor_attr.expo_fs = 1;
	sensor->video.shvflip = 1;
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

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

	return 0;
}

static int sensor_remove(struct i2c_client *client) {
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
	{SENSOR_NAME, 0},
	{}
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

static __init int init_sensor(void) {
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
