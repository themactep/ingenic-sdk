/*
 * sc535IoT.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * resolution      fps       interface          mode
 * 1936*1936       30        mipi_2lane        linear
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

#define SC535IoT_CHIP_ID_H	(0xce)
#define SC535IoT_CHIP_ID_L	(0x78)
#define SC535IoT_REG_END		0xffff
#define SC535IoT_REG_DELAY	0xfffe
#define SC535IoT_SUPPORT_30FPS_SCLK (90000000)/* 1500   ×   2000   ×   30 */
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20240805a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

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

struct again_lut sc535IoT_again_lut[] = {
	{0x0020, 0},
	{0x0021, 2886},
	{0x0022, 5776},
	{0x0023, 8494},
	{0x0024, 11136},
	{0x0025, 13706},
	{0x0026, 16288},
	{0x0027, 18724},
	{0x0028, 21098},
	{0x0029, 23414},
	{0x002a, 25747},
	{0x002b, 27953},
	{0x002c, 30109},
	{0x002d, 32217},
	{0x002e, 34345},
	{0x002f, 36362},
	{0x0030, 38336},
	{0x0031, 40270},
	{0x0032, 42226},
	{0x0033, 44083},
	{0x0034, 45904},
	{0x0035, 47691},
	{0x0036, 49500},
	{0x0037, 51221},
	{0x0038, 52911},
	{0x0039, 54571},
	{0x003a, 56255},
	{0x003b, 57858},
	{0x003c, 59434},
	{0x003d, 60984},
	{0x003e, 62559},
	{0x003f, 64059},
	{0x0120, 65536},
	{0x0121, 68468},
	{0x0122, 71268},
	{0x0123, 74030},
	{0x0124, 76672},
	{0x0125, 79283},
	{0x0126, 81784},
	{0x0127, 84260},
	{0x0128, 86634},
	{0x8020, 87762},
	{0x8021, 90669},
	{0x8022, 93489},
	{0x8023, 96228},
	{0x8024, 98890},
	{0x8025, 101478},
	{0x8026, 103998},
	{0x8027, 106452},
	{0x8028, 108875},
	{0x8029, 111207},
	{0x802a, 113483},
	{0x802b, 115706},
	{0x802c, 117878},
	{0x802d, 120001},
	{0x802e, 122077},
	{0x802f, 124109},
	{0x8030, 126098},
	{0x8031, 128046},
	{0x8032, 129954},
	{0x8033, 131825},
	{0x8034, 133660},
	{0x8035, 135460},
	{0x8036, 137226},
	{0x8037, 138959},
	{0x8038, 140683},
	{0x8039, 142355},
	{0x803a, 143998},
	{0x803b, 145613},
	{0x803c, 147201},
	{0x803d, 148762},
	{0x803e, 150298},
	{0x803f, 151810},
	{0x8120, 153298},
	{0x8121, 156205},
	{0x8122, 159025},
	{0x8123, 161764},
	{0x8124, 164442},
	{0x8125, 167030},
	{0x8126, 169550},
	{0x8127, 172004},
	{0x8128, 174396},
	{0x8129, 176728},
	{0x812a, 179005},
	{0x812b, 181228},
	{0x812c, 183414},
	{0x812d, 185537},
	{0x812e, 187613},
	{0x812f, 189645},
	{0x8130, 191634},
	{0x8131, 193582},
	{0x8132, 195490},
	{0x8133, 197361},
	{0x8134, 199207},
	{0x8135, 201007},
	{0x8136, 202773},
	{0x8137, 204506},
	{0x8138, 206209},
	{0x8139, 207881},
	{0x813a, 209524},
	{0x813b, 211139},
	{0x813c, 212737},
	{0x813d, 214298},
	{0x813e, 215834},
	{0x813f, 217346},
	{0x8320, 218834},
	{0x8321, 221741},
	{0x8322, 224570},
	{0x8323, 227309},
	{0x8324, 229970},
	{0x8325, 232558},
	{0x8326, 235086},
	{0x8327, 237540},
	{0x8328, 239932},
	{0x8329, 242264},
	{0x832a, 244548},
	{0x832b, 246771},
	{0x832c, 248943},
	{0x832d, 251066},
	{0x832e, 253149},
	{0x832f, 255181},
	{0x8330, 257170},
	{0x8331, 259118},
	{0x8332, 261032},
	{0x8333, 262903},
	{0x8334, 264738},
	{0x8335, 266537},
	{0x8336, 268309},
	{0x8337, 270042},
	{0x8338, 271744},
	{0x8339, 273417},
	{0x833a, 275065},
	{0x833b, 276680},
	{0x833c, 278268},
	{0x833d, 279829},
	{0x833e, 281370},
	{0x833f, 282882},
	{0x8720, 284370},
	{0x8721, 287281},
	{0x8722, 290102},
	{0x8723, 292845},
	{0x8724, 295506},
	{0x8725, 298098},
	{0x8726, 300618},
	{0x8727, 303076},
	{0x8728, 305468},
	{0x8729, 307804},
	{0x872a, 310081},
	{0x872b, 312307},
	{0x872c, 314479},
	{0x872d, 316605},
	{0x872e, 318682},
	{0x872f, 320717},
	{0x8730, 322706},
	{0x8731, 324657},
	{0x8732, 326565},
	{0x8733, 328439},
	{0x8734, 330274},
	{0x8735, 332076},
	{0x8736, 333842},
	{0x8737, 335578},
	{0x8738, 337280},
	{0x8739, 338955},
	{0x873a, 340598},
	{0x873b, 342216},
	{0x873c, 343804},
	{0x873d, 345368},
	{0x873e, 346904},
	{0x873f, 348418},
	{0x8f20, 349906},
	{0x8f21, 352815},
	{0x8f22, 355638},
	{0x8f23, 358378},
	{0x8f24, 361042},
	{0x8f25, 363632},
	{0x8f26, 366154},
	{0x8f27, 368610},
	{0x8f28, 371004},
	{0x8f29, 373338},
	{0x8f2a, 375617},
	{0x8f2b, 377841},
	{0x8f2c, 380015},
	{0x8f2d, 382140},
	{0x8f2e, 384218},
	{0x8f2f, 386251},
	{0x8f30, 388242},
	{0x8f31, 390191},
	{0x8f32, 392101},
	{0x8f33, 393974},
	{0x8f34, 395810},
	{0x8f35, 397611},
	{0x8f36, 399378},
	{0x8f37, 401113},
	{0x8f38, 402816},
	{0x8f39, 404490},
	{0x8f3a, 406134},
	{0x8f3b, 407751},
	{0x8f3c, 409340},
	{0x8f3d, 410902},
	{0x8f3e, 412440},
	{0x8f3f, 413953},
};

struct tx_isp_sensor_attribute sc535IoT_attr;

unsigned int sc535IoT_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc535IoT_again_lut;
	while(lut->gain <= sc535IoT_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == sc535IoT_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc535IoT_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sc535IoT_attr={
	.name = "sc535IoT",
	.chip_id = 0xce78,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 900,
		.lans = 2,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 1936,
		.image_theight = 1936,
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
	.max_again = 413953,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 2000 - 8,
	.integration_time_limit = 2000 - 8,
	.total_width = 1500,
	.total_height = 2000,
	.max_integration_time = 2000 - 8,
	.one_line_expr_in_us = 27,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc535IoT_alloc_again,
	.sensor_ctrl.alloc_dgain = sc535IoT_alloc_dgain,
};

static struct regval_list sc535IoT_init_regs_1936_1936_30fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x23b0,0x00},
	{0x23b1,0x08},
	{0x23b2,0x00},
	{0x23b3,0x18},
	{0x23b4,0x00},
	{0x23b5,0x38},
	{0x23b6,0x04},
	{0x23b7,0x08},
	{0x23b8,0x04},
	{0x23b9,0x18},
	{0x23ba,0x04},
	{0x23bb,0x38},
	{0x23c0,0x04},
	{0x23c1,0x00},
	{0x23c2,0x04},
	{0x23c3,0x18},
	{0x23c4,0x04},
	{0x23c5,0x78},
	{0x23c6,0x04},
	{0x23c7,0x08},
	{0x23c8,0x04},
	{0x23c9,0x78},
	{0x3018,0x3b},
	{0x3019,0x0c},
	{0x301e,0xf0},
	{0x301f,0x05},
	{0x302c,0x00},
	{0x30b8,0x44},
	{0x3200,0x00},
	{0x3201,0x30},
	{0x3202,0x00},
	{0x3203,0x00},
	{0x3204,0x0a},
	{0x3205,0x57},
	{0x3206,0x07},
	{0x3207,0x9f},
	{0x3208,0x0a},
	{0x3209,0x20},
	{0x320a,0x07},
	{0x320b,0x98},
	{0x320c,0x05},
	{0x320d,0xdc},
	{0x320e,0x07},
	{0x320f,0xd0},
	{0x3210,0x00},
	{0x3211,0x04},
	{0x3212,0x00},
	{0x3213,0x04},
	{0x3214,0x11},
	{0x3215,0x11},
	{0x3223,0xc0},
	{0x3250,0x40},
	{0x327f,0x3f},
	{0x32e0,0x00},
	{0x3301,0x12},
	{0x3302,0x20},
	{0x3304,0xc0},
	{0x3306,0xb0},
	{0x3309,0xf0},
	{0x330a,0x01},
	{0x330b,0x70},
	{0x330d,0x10},
	{0x3310,0x18},
	{0x331e,0xb1},
	{0x331f,0xe1},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x3364,0x56},
	{0x338f,0x80},
	{0x3393,0x1c},
	{0x3394,0x2c},
	{0x3395,0x3c},
	{0x3399,0x0c},
	{0x339a,0x10},
	{0x339b,0x18},
	{0x339c,0x80},
	{0x33ac,0x10},
	{0x33ad,0x2c},
	{0x33ae,0xb0},
	{0x33af,0xe0},
	{0x33b0,0x0f},
	{0x33b2,0x2c},
	{0x33b3,0x02},
	{0x349f,0x03},
	{0x34a8,0x02},/* 0x5dc = 1500 = hts */
	{0x34a9,0x08},
	{0x34aa,0x01},/* 0x7d0 = 2000 = vts */
	{0x34ab,0x70},
	{0x34ac,0x01},
	{0x34ad,0x70},
	{0x34f9,0x12},
	{0x3631,0x0f},
	{0x3632,0x8d},
	{0x3633,0x4d},
	{0x363b,0x58},
	{0x363c,0xd8},
	{0x363d,0x20},
	{0x3641,0x08},
	{0x3670,0x32},
	{0x3671,0x34},
	{0x3672,0x26},
	{0x3673,0x04},
	{0x3674,0x08},
	{0x3675,0x04},
	{0x3676,0x18},
	{0x367e,0x49},
	{0x367f,0x49},
	{0x3680,0x49},
	{0x3681,0x04},
	{0x3682,0x08},
	{0x3683,0x04},
	{0x3684,0x38},
	{0x3685,0xc1},
	{0x3686,0xc2},
	{0x3687,0xc1},
	{0x3688,0xc1},
	{0x3689,0xc1},
	{0x368a,0xc1},
	{0x368b,0xc4},
	{0x368c,0xc1},
	{0x368d,0x00},
	{0x368e,0x08},
	{0x368f,0x00},
	{0x3690,0x18},
	{0x3691,0x04},
	{0x3692,0x00},
	{0x3693,0x04},
	{0x3694,0x08},
	{0x3695,0x04},
	{0x3696,0x18},
	{0x3697,0x04},
	{0x3698,0x38},
	{0x3699,0x04},
	{0x369a,0x78},
	{0x36d0,0x0d},
	{0x36ea,0x0a},
	{0x36eb,0x0c},
	{0x36ec,0x43},
	{0x36ed,0xaa},
	{0x370f,0x13},
	{0x3721,0x6c},
	{0x3722,0x8b},
	{0x3724,0xd1},
	{0x3729,0x34},
	{0x37b0,0x17},
	{0x37b1,0x17},
	{0x37b2,0x13},
	{0x37b3,0x04},
	{0x37b4,0x08},
	{0x37b5,0x04},
	{0x37b6,0x38},
	{0x37b7,0x1d},
	{0x37b8,0x1f},
	{0x37b9,0x1f},
	{0x37ba,0x04},
	{0x37bb,0x04},
	{0x37bc,0x04},
	{0x37bd,0x04},
	{0x37be,0x08},
	{0x37bf,0x04},
	{0x37c0,0x38},
	{0x37c1,0x04},
	{0x37c2,0x08},
	{0x37c3,0x04},
	{0x37c4,0x38},
	{0x37fa,0x09},
	{0x37fb,0x22},
	{0x37fc,0x30},
	{0x37fd,0x26},
	{0x3900,0x05},
	{0x3901,0x00},
	{0x3902,0xc0},
	{0x3903,0x40},
	{0x3905,0x2d},
	{0x391a,0x72},
	{0x391b,0x39},
	{0x391c,0x22},
	{0x391d,0x00},
	{0x391f,0x41},
	{0x3926,0xe0},
	{0x3933,0x80},
	{0x3934,0x03},
	{0x3935,0x01},
	{0x3936,0xc0},
	{0x3937,0x6a},
	{0x3938,0x6b},
	{0x3939,0x0f},
	{0x393a,0xf6},
	{0x393d,0x05},
	{0x393e,0x50},
	{0x39dd,0x00},
	{0x39de,0x06},
	{0x39e7,0x04},
	{0x39e8,0x04},
	{0x39e9,0x80},
	{0x3e00,0x00},
	{0x3e01,0x7c},
	{0x3e02,0x80},
	{0x3e03,0x0b},
	{0x3e16,0x01},
	{0x3e17,0x44},
	{0x3e18,0x01},
	{0x3e19,0x44},
	{0x4509,0x18},
	{0x450d,0x07},
	{0x480f,0x03},
	{0x5000,0x06},
	{0x5780,0x76},
	{0x5784,0x10},
	{0x5785,0x08},
	{0x5787,0x16},
	{0x5788,0x16},
	{0x5789,0x15},
	{0x578a,0x16},
	{0x578b,0x16},
	{0x578c,0x15},
	{0x578d,0x41},
	{0x5790,0x11},
	{0x5791,0x0f},
	{0x5792,0x0f},
	{0x5793,0x11},
	{0x5794,0x0f},
	{0x5795,0x0f},
	{0x5799,0x46},
	{0x579a,0x77},
	{0x57a1,0x04},
	{0x57a8,0xd2},
	{0x57aa,0x2a},
	{0x57ab,0x7f},
	{0x57ac,0x00},
	{0x57ad,0x00},
	{0x3200,0x00},
	{0x3201,0x30},
	{0x3202,0x00},
	{0x3203,0x00},
	{0x3204,0x0a},
	{0x3205,0x57},
	{0x3206,0x07},
	{0x3207,0x9f},
	{0x3208,0x07},
	{0x3209,0x90},
	{0x320a,0x07},
	{0x320b,0x90},
	{0x3210,0x01},
	{0x3211,0x48},
	{0x3212,0x00},
	{0x3213,0x04},
	{0x36e9,0x53},
	{0x37f9,0x00},
	{0x0100,0x01},
	{SC535IoT_REG_END, 0x00},	/* END MARKER */
};

static struct tx_isp_sensor_win_setting sc535IoT_win_sizes[] = {
	{
		.width		= 1936,
		.height		= 1936,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc535IoT_init_regs_1936_1936_30fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sc535IoT_win_sizes[0];

static struct regval_list sc535IoT_stream_on_mipi[] = {
	{0x0100, 0x01},
	{SC535IoT_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc535IoT_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SC535IoT_REG_END, 0x00},	/* END MARKER */
};

int sc535IoT_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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

int sc535IoT_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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
        /* printk("[%s %d] 0x%04x = 0x%02x\n", __func__, __LINE__, reg, value); */

	return ret;
}

#if 0
static int sc535IoT_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC535IoT_REG_END) {
		if (vals->reg_num == SC535IoT_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc535IoT_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc535IoT_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC535IoT_REG_END) {
		if (vals->reg_num == SC535IoT_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc535IoT_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc535IoT_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sc535IoT_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret = 0;
	unsigned char v;

	ret += sc535IoT_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC535IoT_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret += sc535IoT_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC535IoT_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc535IoT_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	//integration time
	ret = sc535IoT_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc535IoT_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc535IoT_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));

	//sensor analog gain
	ret += sc535IoT_write(sd, 0x3e08, (unsigned char)(((again >> 8) & 0xff)));
	ret += sc535IoT_write(sd, 0x3e09, (unsigned char)(again & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int sc535IoT_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
	ret += sc535IoT_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc535IoT_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc535IoT_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));

	if (ret < 0)
		return ret;

	return 0;
}

static int sc535IoT_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sc535IoT_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sc535IoT_write(sd, 0x3e08, (unsigned char)(((value >> 8) & 0xff)));
	if (ret < 0)
		return ret;
	gain_val = value;

	return 0;
}
#endif

static int sc535IoT_set_logic(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc535IoT_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc535IoT_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc535IoT_init(struct tx_isp_subdev *sd, int enable)
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

	ret = sc535IoT_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc535IoT_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc535IoT_write_array(sd, sc535IoT_stream_on_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc535IoT stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc535IoT_write_array(sd, sc535IoT_stream_off_mipi);
		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc535IoT stream off\n");
	}

	return ret;
}

static int sc535IoT_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	sclk = SC535IoT_SUPPORT_30FPS_SCLK;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}

	ret = sc535IoT_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sc535IoT_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("err: sc535IoT read err\n");
		return ret;
	}
	hts = ((hts << 8) + tmp);
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += sc535IoT_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc535IoT_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: sc535IoT_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 8;
	sensor->video.attr->integration_time_limit = vts - 8;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 8;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sc535IoT_set_mode(struct tx_isp_subdev *sd, int value)
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

static int sc535IoT_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc535IoT_reset");
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
		ret = private_gpio_request(pwdn_gpio,"sc535IoT_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc535IoT_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc535IoT chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("sc535IoT chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc535IoT", sizeof("sc535IoT"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc535IoT_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	val = sc535IoT_read(sd, 0x3221, &val);
	switch(enable) {
	case 0:
		val = 0x00;
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
	default:
		break;
	}
	sc535IoT_write(sd, 0x3221, val);

	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int sc535IoT_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}

	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
	     	ret = sc535IoT_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
	//	if(arg)
	//		ret = sc535IoT_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
	//	if(arg)
	//		ret = sc535IoT_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc535IoT_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc535IoT_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc535IoT_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc535IoT_write_array(sd, sc535IoT_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc535IoT_write_array(sd, sc535IoT_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc535IoT_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = sc535IoT_set_vflip(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if(arg)
			ret = sc535IoT_set_logic(sd, *(int*)arg);
	default:
		break;
	}

	return ret;
}

static int sc535IoT_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc535IoT_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc535IoT_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	sc535IoT_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc535IoT_core_ops = {
	.g_chip_ident = sc535IoT_g_chip_ident,
	.reset = sc535IoT_reset,
	.init = sc535IoT_init,
	/*.ioctl = sc535IoT_ops_ioctl,*/
	.g_register = sc535IoT_g_register,
	.s_register = sc535IoT_s_register,
};

static struct tx_isp_subdev_video_ops sc535IoT_video_ops = {
	.s_stream = sc535IoT_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc535IoT_sensor_ops = {
	.ioctl	= sc535IoT_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc535IoT_ops = {
	.core = &sc535IoT_core_ops,
	.video = &sc535IoT_video_ops,
	.sensor = &sc535IoT_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc535IoT",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sensor_mclk_config(struct tx_isp_sensor *sensor, unsigned long want_rate)
{
        unsigned long rate = 0;
        struct clk *pll = NULL;
        char *plls[] = {"mpll", "sclka"};
        int psize = sizeof(plls) / sizeof(char *);
        char *ppll = plls[psize - 1];
        int ret = 0, i = 0;

        pll = clk_get_parent(sensor->mclk);
        rate = clk_get_rate(pll);
        if (rate % want_rate) {
                for (i = 0; i < psize; i++) {
                        pll = clk_get(NULL, plls[i]);
                        rate = clk_get_rate(pll);
                        if (!(rate % want_rate)) {
                                ret = clk_set_parent(sensor->mclk, pll);
                                if (ret) {
                                        ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
                                                    __func__, __LINE__, plls[i]);
                                        continue;
                                } else {
                                        break;
                                }
                        }
                }
                if (i == psize) {
                        if (!ret) {
                                pll = clk_get(NULL, ppll);
                                rate = clk_get_rate(pll);
                                if(want_rate == 37125000){
                                        if((rate >= 1188000000)) {
                                                rate = 1188000000;
                                        } else if (rate >= 891000000) {
                                                rate = 891000000;
                                        } else {
                                                ISP_ERROR("[%s %d] The %s clock setting failed !!!\n",
                                                          __func__, __LINE__, ppll);
                                                ret = -1;
                                                goto error;
                                        }
                                } else if (want_rate == 24000000 || want_rate == 27000000) {
                                        rate -= rate % want_rate;
                                } else {
                                        ret = -1;
                                        goto error;
                                }
                                ret = private_clk_set_rate(pll, rate);
                                if (ret) {
                                        ISP_WARNING("[%s %d] Failed to set %s !!!\n",
                                                    __func__, __LINE__, ppll);
                                        goto error;
                                } else {
                                        ISP_WARNING("[%s %d] !!!!!!!!!!! The %s frequency has been changed to %ld !!!\n",
                                                    __func__, __LINE__, ppll, rate);
                                }
                                ret = clk_set_parent(sensor->mclk, pll);
                                if (ret) {
                                        ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
                                                    __func__, __LINE__, ppll);
                                        goto error;
                                }
                        } else {
                                goto error;
                        }
                }
        }
        private_clk_set_rate(sensor->mclk, want_rate);
        private_clk_enable(sensor->mclk);

        rate = clk_get_rate(sensor->mclk);
        if (rate % want_rate) {
                ret = -1;
                goto error;
        }

        return ret;

error:
        ISP_ERROR("[%s %d] Unable to allocate the required MCLK %ld !!!\n",
                  __func__, __LINE__, want_rate);
        return ret;
}


static int sc535IoT_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

#ifdef CONFIG_KERNEL_4_4_94
		sensor->mclk = clk_get(NULL, "div_cim");
#else
		sensor->mclk = clk_get(NULL, "cgu_cim");
#endif
        if (IS_ERR(sensor->mclk)) {
                ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
                goto err_get_mclk;
        }
    sensor_mclk_config(sensor, 24000000);

	/*
	  convert sensor-gain into isp-gain,
	*/
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sc535IoT_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc535IoT_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc535IoT\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int sc535IoT_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc535IoT_id[] = {
	{ "sc535IoT", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc535IoT_id);

static struct i2c_driver sc535IoT_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc535IoT",
	},
	.probe		= sc535IoT_probe,
	.remove		= sc535IoT_remove,
	.id_table	= sc535IoT_id,
};

static __init int init_sc535IoT(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init sc535IoT dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&sc535IoT_driver);
}

static __exit void exit_sc535IoT(void)
{
	private_i2c_del_driver(&sc535IoT_driver);
}

module_init(init_sc535IoT);
module_exit(exit_sc535IoT);

MODULE_DESCRIPTION("A low-level driver for SmartSens sc535IoT sensors");
MODULE_LICENSE("GPL");
