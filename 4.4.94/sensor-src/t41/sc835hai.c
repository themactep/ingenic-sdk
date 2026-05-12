/*
 * sc835hai.c
 *
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps	interface	mode
 *   0          3840*2160       25	mipi_2lane	linear
 *   1          1920*1080       25	mipi_2lane	linear
 *   2          1920*2160       25	mipi_2lane	linear
 *   3          3840*2160       30	mipi_2lane	linear
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

#define SC835HAI_CHIP_ID_H	(0xc1)
#define SC835HAI_CHIP_ID_L	(0x70)
#define SC835HAI_REG_END	0xffff
#define SC835HAI_REG_DELAY	0xfffe
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20250712a"

#define MCLK 24000000

static int reset_gpio = -1;
static int pwdn_gpio = -1;
static int shvflip = 1;

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

struct again_lut sc835hai_again_lut[] = {
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
	{0x8020, 38336},
	{0x8021, 41253},
	{0x8022, 44083},
	{0x8023, 46830},
	{0x8024, 49500},
	{0x8025, 52042},
	{0x8026, 54571},
	{0x8027, 57034},
	{0x8028, 59434},
	{0x8029, 61775},
	{0x802a, 64059},
	{0x802b, 66289},
	{0x802c, 68468},
	{0x802d, 70553},
	{0x802e, 72637},
	{0x802f, 74676},
	{0x8030, 76672},
	{0x8031, 78627},
	{0x8032, 80542},
	{0x8033, 82419},
	{0x8034, 84260},
	{0x8035, 86027},
	{0x8036, 87799},
	{0x8037, 89539},
	{0x8038, 91247},
	{0x8039, 92925},
	{0x803a, 94573},
	{0x803b, 96194},
	{0x803c, 97787},
	{0x803d, 99320},
	{0x803e, 100862},
	{0x803f, 102379},
	{0x8120, 103872},
	{0x8121, 106789},
	{0x8122, 109619},
	{0x8123, 112338},
	{0x8124, 115008},
	{0x8125, 117606},
	{0x8126, 120134},
	{0x8127, 122570},
	{0x8128, 124970},
	{0x8129, 127311},
	{0x812a, 129595},
	{0x812b, 131802},
	{0x812c, 133981},
	{0x812d, 136112},
	{0x812e, 138195},
	{0x812f, 140212},
	{0x8130, 142208},
	{0x8131, 144163},
	{0x8132, 146078},
	{0x8133, 147935},
	{0x8134, 149776},
	{0x8135, 151582},
	{0x8136, 153354},
	{0x8137, 155075},
	{0x8138, 156783},
	{0x8139, 158461},
	{0x813a, 160109},
	{0x813b, 161713},
	{0x813c, 163306},
	{0x813d, 164873},
	{0x813e, 166414},
	{0x813f, 167915},
	{0x8320, 169408},
	{0x8321, 172325},
	{0x8322, 175140},
	{0x8323, 177888},
	{0x8324, 180544},
	{0x8325, 183142},
	{0x8326, 185656},
	{0x8327, 188119},
	{0x8328, 190506},
	{0x8329, 192847},
	{0x832a, 195119},
	{0x832b, 197350},
	{0x832c, 199517},
	{0x832d, 201648},
	{0x832e, 203720},
	{0x832f, 205759},
	{0x8330, 207744},
	{0x8331, 209699},
	{0x8332, 211604},
	{0x8333, 213481},
	{0x8334, 215312},
	{0x8335, 217118},
	{0x8336, 218880},
	{0x8337, 220620},
	{0x8338, 222319},
	{0x8339, 223997},
	{0x833a, 225637},
	{0x833b, 227257},
	{0x833c, 228842},
	{0x833d, 230409},
	{0x833e, 231942},
	{0x833f, 233459},
	{0x8720, 234944},
	{0x8721, 237854},
	{0x8722, 240676},
	{0x8723, 243417},
	{0x8724, 246080},
	{0x8725, 248671},
	{0x8726, 251192},
	{0x8727, 253648},
	{0x8728, 256042},
	{0x8729, 258377},
	{0x872a, 260655},
	{0x872b, 262880},
	{0x872c, 265053},
	{0x872d, 267178},
	{0x872e, 269256},
	{0x872f, 271290},
	{0x8730, 273280},
	{0x8731, 275230},
	{0x8732, 277140},
	{0x8733, 279012},
	{0x8734, 280848},
	{0x8735, 282649},
	{0x8736, 284416},
	{0x8737, 286151},
	{0x8738, 287855},
	{0x8739, 289528},
	{0x873a, 291173},
	{0x873b, 292789},
	{0x873c, 294378},
	{0x873d, 295941},
	{0x873e, 297478},
	{0x873f, 298991},
	{0x8f20, 300480},
	{0x8f21, 303390},
	{0x8f22, 306212},
	{0x8f23, 308953},
	{0x8f24, 311616},
	{0x8f25, 314207},
	{0x8f26, 316728},
	{0x8f27, 319184},
	{0x8f28, 321578},
	{0x8f29, 323913},
	{0x8f2a, 326191},
	{0x8f2b, 328416},
	{0x8f2c, 330589},
	{0x8f2d, 332714},
	{0x8f2e, 334792},
	{0x8f2f, 336826},
	{0x8f30, 338816},
	{0x8f31, 340766},
	{0x8f32, 342676},
	{0x8f33, 344548},
	{0x8f34, 346384},
	{0x8f35, 348185},
	{0x8f36, 349952},
	{0x8f37, 351687},
	{0x8f38, 353391},
	{0x8f39, 355064},
	{0x8f3a, 356709},
	{0x8f3b, 358325},
	{0x8f3c, 359914},
	{0x8f3d, 361477},
	{0x8f3e, 363014},
	{0x8f3f, 364527},
};

struct tx_isp_sensor_attribute sc835hai_attr;

unsigned int sc835hai_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;
	*sensor_it = expo;

	return isp_it;
}
unsigned int sc835hai_alloc_integration_time_short(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;
	*sensor_it = expo;

	return isp_it;
}

unsigned int sc835hai_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc835hai_again_lut;
	while(lut->gain <= sc835hai_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc835hai_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}
unsigned int sc835hai_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc835hai_again_lut;

	while(lut->gain <= sc835hai_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else{
			if((lut->gain == sc835hai_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc835hai_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sc835hai_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1296,
	.lans = 2,
	.index = 0,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
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
};

struct tx_isp_sensor_attribute sc835hai_attr={
	.name = "sc835hai",
	.chip_id = 0xc170,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL,
	.cbus_device = 0x30,
	.max_again = 364527,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc835hai_alloc_again,
	.sensor_ctrl.alloc_again_short = sc835hai_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sc835hai_alloc_dgain,
	.sensor_ctrl.alloc_integration_time_short = sc835hai_alloc_integration_time_short,
};

static struct regval_list sc835hai_init_regs_3840_2160_25fps_mipi[] = {
	{0x0103,0x01},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x3018,0x3a},
	{0x3019,0x0c},
	{0x301f,0x07},
	{0x30b8,0x44},
	{0x320c,0x08},//hts = 0x834 = 2100
	{0x320d,0x34},//
	{0x320e,0x09},//vts = 0x960 = 2400
	{0x320f,0x60},//
	{0x3221,0x60},
	{0x3301,0x0a},
	{0x3302,0x10},
	{0x3303,0x10},
	{0x3304,0x68},
	{0x3305,0x00},
	{0x3306,0x70},
	{0x3307,0x08},
	{0x3308,0x14},
	{0x3309,0x98},
	{0x330a,0x00},
	{0x330b,0xf8},
	{0x330c,0x10},
	{0x330d,0x08},
	{0x330e,0x3c},
	{0x331e,0x41},
	{0x331f,0x71},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x334c,0x10},
	{0x335d,0x60},
	{0x3364,0x5e},
	{0x3367,0x04},
	{0x338f,0x80},
	{0x3390,0x01},
	{0x3391,0x03},
	{0x3392,0x07},
	{0x3393,0x28},
	{0x3394,0x4c},
	{0x3395,0x4c},
	{0x3396,0x01},
	{0x3397,0x03},
	{0x3398,0x07},
	{0x3399,0x0e},
	{0x339a,0x12},
	{0x339b,0x4c},
	{0x339c,0x4c},
	{0x33ad,0x24},
	{0x33ae,0x58},
	{0x33af,0x88},
	{0x33b2,0x50},
	{0x33b3,0x20},
	{0x33f8,0x00},
	{0x33f9,0x88},
	{0x33fa,0x00},
	{0x33fb,0xa0},
	{0x33fc,0x43},
	{0x33fd,0x47},
	{0x349f,0x03},
	{0x34a6,0x43},
	{0x34a7,0x47},
	{0x34a8,0x20},
	{0x34a9,0x20},
	{0x34aa,0x01},
	{0x34ab,0x10},
	{0x34ac,0x01},
	{0x34ad,0x28},
	{0x34f8,0x43},
	{0x34f9,0x08},
	{0x3632,0x64},
	{0x363b,0x18},
	{0x363c,0x0e},
	{0x363d,0x8e},
	{0x363e,0x6c},
	{0x3654,0x00},
	{0x3674,0x94},
	{0x3675,0x84},
	{0x3676,0x68},
	{0x367c,0x41},
	{0x367d,0x43},
	{0x3690,0x35},
	{0x3691,0x35},
	{0x3692,0x45},
	{0x3693,0x40},
	{0x3694,0x41},
	{0x3696,0x81},
	{0x3697,0x80},
	{0x3698,0x80},
	{0x3699,0x83},
	{0x369a,0x81},
	{0x369b,0xff},
	{0x369c,0xff},
	{0x369d,0xff},
	{0x36a2,0x40},
	{0x36a3,0x41},
	{0x36a4,0x43},
	{0x36a5,0x47},
	{0x36a6,0x4f},
	{0x36a7,0x4f},
	{0x36a8,0x4f},
	{0x36d0,0x15},
	{0x36ea,0x09},
	{0x36eb,0x04},
	{0x36ec,0x43},
	{0x36ed,0x3a},
	{0x370f,0x01},
	{0x3724,0xe5},
	{0x3725,0xa8},
	{0x3727,0x14},
	{0x37b0,0x17},
	{0x37b1,0x9b},
	{0x37b2,0xfb},
	{0x37b3,0x41},
	{0x37b4,0x43},
	{0x37fa,0x0e},
	{0x37fb,0x31},
	{0x37fc,0x00},
	{0x37fd,0x16},
	{0x3905,0x0f},
	{0x391f,0x41},
	{0x3933,0x80},
	{0x3934,0xd3},
	{0x3937,0x70},
	{0x3939,0x0f},
	{0x393a,0xf8},
	{0x3e00,0x01},
	{0x3e01,0x2b},
	{0x3e02,0x30},
	{0x3e16,0x00},
	{0x3e17,0xbc},
	{0x3e18,0x00},
	{0x3e19,0xbc},
	{0x4424,0x02},
	{0x4509,0x1a},
	{0x450d,0x0b},
	{0x4800,0x24},
	{0x4837,0x0c},
	{0x575c,0x10},
	{0x575d,0x08},
	{0x5780,0x76},
	{0x5784,0x10},
	{0x5785,0x08},
	{0x5787,0x0a},
	{0x5788,0x0a},
	{0x5789,0x08},
	{0x578a,0x0a},
	{0x578b,0x0a},
	{0x578c,0x08},
	{0x578d,0x41},
	{0x5790,0x08},
	{0x5791,0x04},
	{0x5792,0x04},
	{0x5793,0x08},
	{0x5794,0x04},
	{0x5795,0x04},
	{0x57a8,0xd2},
	{0x57aa,0x2a},
	{0x57ab,0x7f},
	{0x57ac,0x00},
	{0x57ad,0x00},
	{0x5000,0x0e},
	{0x36e9,0x47},
	{0x37f9,0x57},
	/* {0x0100,0x01}, */
	{SC835HAI_REG_END, 0x00},
};

static struct regval_list sc835hai_init_regs_1920_1080_25fps_mipi[] = {
	//Cleaned_0x3c_SC835HAI_MIPI_24Minput_2lane_10bit_360Mbps_1920x1080_25fps_hsum_vbin
	//VTS=1350.000000,HTS=2133.333333,SCLK=70.875000,PCLK=72.000000,MipiCLK=360.000000,Tline=29.629630us,TExp_step=0.5*Tline=14.814815us,TExp_offset=4.684303us
	{0x0103,0x01},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x3018,0x3a},
	{0x3019,0x0c},
	{0x301f,0x00},
	{0x30b8,0x44},
	{0x3208,0x07},
	{0x3209,0x80},
	{0x320a,0x04},
	{0x320b,0x38},
	{0x320c,0x08},//hts = 0x834 = 2100
	{0x320d,0x34},//
	{0x320e,0x05},//vts = 0x546 = 1350
	{0x320f,0x46},//
	{0x3211,0x02},
	{0x3213,0x02},
	{0x3215,0x31},
	{0x3220,0x01},
	{0x3301,0x0a},
	{0x3302,0x10},
	{0x3303,0x10},
	{0x3304,0x68},
	{0x3305,0x00},
	{0x3306,0x70},
	{0x3307,0x08},
	{0x3308,0x14},
	{0x3309,0x98},
	{0x330a,0x00},
	{0x330b,0xf8},
	{0x330c,0x10},
	{0x330d,0x08},
	{0x330e,0x3c},
	{0x331e,0x41},
	{0x331f,0x71},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x334c,0x10},
	{0x335d,0x60},
	{0x3364,0x5e},
	{0x3367,0x04},
	{0x338f,0x80},
	{0x3390,0x01},
	{0x3391,0x03},
	{0x3392,0x07},
	{0x3393,0x28},
	{0x3394,0x4c},
	{0x3395,0x4c},
	{0x3396,0x01},
	{0x3397,0x03},
	{0x3398,0x07},
	{0x3399,0x0e},
	{0x339a,0x12},
	{0x339b,0x4c},
	{0x339c,0x4c},
	{0x33ad,0x24},
	{0x33ae,0x58},
	{0x33af,0x88},
	{0x33b2,0x50},
	{0x33b3,0x20},
	{0x33f8,0x00},
	{0x33f9,0x88},
	{0x33fa,0x00},
	{0x33fb,0xa0},
	{0x33fc,0x43},
	{0x33fd,0x47},
	{0x349f,0x03},
	{0x34a6,0x43},
	{0x34a7,0x47},
	{0x34a8,0x20},
	{0x34a9,0x20},
	{0x34aa,0x01},
	{0x34ab,0x10},
	{0x34ac,0x01},
	{0x34ad,0x28},
	{0x34f8,0x43},
	{0x34f9,0x08},
	{0x3632,0x64},
	{0x363b,0x16},
	{0x363c,0x0e},
	{0x363d,0x8e},
	{0x363e,0x6c},
	{0x3654,0x00},
	{0x3674,0x94},
	{0x3675,0x84},
	{0x3676,0x68},
	{0x367c,0x41},
	{0x367d,0x43},
	{0x3690,0x35},
	{0x3691,0x35},
	{0x3692,0x45},
	{0x3693,0x40},
	{0x3694,0x41},
	{0x3696,0x81},
	{0x3697,0x80},
	{0x3698,0x80},
	{0x3699,0x83},
	{0x369a,0x81},
	{0x369b,0xff},
	{0x369c,0xff},
	{0x369d,0xff},
	{0x36a2,0x40},
	{0x36a3,0x41},
	{0x36a4,0x43},
	{0x36a5,0x47},
	{0x36a6,0x4f},
	{0x36a7,0x4f},
	{0x36a8,0x4f},
	{0x36d0,0x15},
	{0x36ea,0x0a},
	{0x36eb,0x04},
	{0x36ec,0x53},
	{0x36ed,0x3a},
	{0x370f,0x01},
	{0x3724,0xe5},
	{0x3725,0xa8},
	{0x3727,0x14},
	{0x37b0,0x17},
	{0x37b1,0x9b},
	{0x37b2,0xfb},
	{0x37b3,0x41},
	{0x37b4,0x43},
	{0x37fa,0xc7},
	{0x37fb,0x33},
	{0x37fc,0x01},
	{0x37fd,0x36},
	{0x3905,0x0f},
	{0x391f,0x41},
	{0x3933,0x80},
	{0x3934,0xd3},
	{0x3937,0x70},
	{0x3939,0x0f},
	{0x393a,0xf8},
	{0x3e00,0x00},
	{0x3e01,0xa7},
	{0x3e02,0xf0},
	{0x3e16,0x00},
	{0x3e17,0xbc},
	{0x3e18,0x00},
	{0x3e19,0xbc},
	{0x4424,0x02},
	{0x4509,0x1a},
	{0x450d,0x0b},
	{0x4800,0x24},
	{0x4837,0x2c},
	{0x5000,0x4e},
	{0x575c,0x10},
	{0x575d,0x08},
	{0x5780,0x76},
	{0x5784,0x10},
	{0x5785,0x08},
	{0x5787,0x0a},
	{0x5788,0x0a},
	{0x5789,0x08},
	{0x578a,0x0a},
	{0x578b,0x0a},
	{0x578c,0x08},
	{0x578d,0x40},
	{0x5790,0x08},
	{0x5791,0x04},
	{0x5792,0x04},
	{0x5793,0x08},
	{0x5794,0x04},
	{0x5795,0x04},
	{0x57a8,0xd2},
	{0x57aa,0x2a},
	{0x57ab,0x7f},
	{0x57ac,0x00},
	{0x57ad,0x00},
	{0x5900,0xf2},
	{0x5901,0x04},
	{0x36e9,0x24},
	{0x37f9,0x53},
	/* {0x0100,0x01}, */
	{SC835HAI_REG_END, 0x00},
};

static struct regval_list sc835hai_init_regs_1920_2160_25fps_mipi[] = {
	//Cleaned_0x46_SC835HAI_MIPI_24Minput_2lane_10bit_720Mbps_1920x2160_25fps_hsum
	//VTS=2700.000000,HTS=2133.333333,SCLK=141.750000,PCLK=144.000000,MipiCLK=720.000000,Tline=14.814815us,TExp_step=0.5*Tline=7.407407us,TExp_offset=2.342152us
	{0x0103,0x01},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x3018,0x3a},
	{0x3019,0x0c},
	{0x301f,0x46},
	{0x30b8,0x44},
	{0x3208,0x07},
	{0x3209,0x80},
	{0x320c,0x08},//hts = 0x834 = 2100
	{0x320d,0x34},//
	{0x320e,0x0a},//vts = 0xa8c = 2700
	{0x320f,0x8c},//
	{0x3211,0x02},
	{0x3301,0x0a},
	{0x3302,0x10},
	{0x3303,0x10},
	{0x3304,0x68},
	{0x3305,0x00},
	{0x3306,0x70},
	{0x3307,0x08},
	{0x3308,0x14},
	{0x3309,0x98},
	{0x330a,0x00},
	{0x330b,0xf8},
	{0x330c,0x10},
	{0x330d,0x08},
	{0x330e,0x3c},
	{0x331e,0x41},
	{0x331f,0x71},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x334c,0x10},
	{0x335d,0x60},
	{0x3364,0x5e},
	{0x3367,0x04},
	{0x338f,0x80},
	{0x3390,0x01},
	{0x3391,0x03},
	{0x3392,0x07},
	{0x3393,0x28},
	{0x3394,0x4c},
	{0x3395,0x4c},
	{0x3396,0x01},
	{0x3397,0x03},
	{0x3398,0x07},
	{0x3399,0x0e},
	{0x339a,0x12},
	{0x339b,0x4c},
	{0x339c,0x4c},
	{0x33ad,0x24},
	{0x33ae,0x58},
	{0x33af,0x88},
	{0x33b2,0x50},
	{0x33b3,0x20},
	{0x33f8,0x00},
	{0x33f9,0x88},
	{0x33fa,0x00},
	{0x33fb,0xa0},
	{0x33fc,0x43},
	{0x33fd,0x47},
	{0x349f,0x03},
	{0x34a6,0x43},
	{0x34a7,0x47},
	{0x34a8,0x20},
	{0x34a9,0x20},
	{0x34aa,0x01},
	{0x34ab,0x10},
	{0x34ac,0x01},
	{0x34ad,0x28},
	{0x34f8,0x43},
	{0x34f9,0x08},
	{0x3632,0x64},
	{0x363b,0x16},
	{0x363c,0x0e},
	{0x363d,0x8e},
	{0x363e,0x6c},
	{0x3654,0x00},
	{0x3674,0x94},
	{0x3675,0x84},
	{0x3676,0x68},
	{0x367c,0x41},
	{0x367d,0x43},
	{0x3690,0x35},
	{0x3691,0x35},
	{0x3692,0x45},
	{0x3693,0x40},
	{0x3694,0x41},
	{0x3696,0x81},
	{0x3697,0x80},
	{0x3698,0x80},
	{0x3699,0x83},
	{0x369a,0x81},
	{0x369b,0xff},
	{0x369c,0xff},
	{0x369d,0xff},
	{0x36a2,0x40},
	{0x36a3,0x41},
	{0x36a4,0x43},
	{0x36a5,0x47},
	{0x36a6,0x4f},
	{0x36a7,0x4f},
	{0x36a8,0x4f},
	{0x36d0,0x15},
	{0x36ea,0x0a},
	{0x36eb,0x04},
	{0x36ec,0x43},
	{0x36ed,0x3a},
	{0x370f,0x01},
	{0x3724,0xe5},
	{0x3725,0xa8},
	{0x3727,0x14},
	{0x37b0,0x17},
	{0x37b1,0x9b},
	{0x37b2,0xfb},
	{0x37b3,0x41},
	{0x37b4,0x43},
	{0x37fa,0xc7},
	{0x37fb,0x31},
	{0x37fc,0x00},
	{0x37fd,0x36},
	{0x3905,0x0f},
	{0x391f,0x41},
	{0x3933,0x80},
	{0x3934,0xd3},
	{0x3937,0x70},
	{0x3939,0x0f},
	{0x393a,0xf8},
	{0x3e00,0x01},
	{0x3e01,0x50},
	{0x3e02,0xb0},
	{0x3e16,0x00},
	{0x3e17,0xbc},
	{0x3e18,0x00},
	{0x3e19,0xbc},
	{0x4424,0x02},
	{0x4509,0x1a},
	{0x450d,0x0b},
	{0x4800,0x24},
	{0x4837,0x16},
	{0x5000,0x4e},
	{0x575c,0x10},
	{0x575d,0x08},
	{0x5780,0x76},
	{0x5784,0x10},
	{0x5785,0x08},
	{0x5787,0x0a},
	{0x5788,0x0a},
	{0x5789,0x08},
	{0x578a,0x0a},
	{0x578b,0x0a},
	{0x578c,0x08},
	{0x578d,0x40},
	{0x5790,0x08},
	{0x5791,0x04},
	{0x5792,0x04},
	{0x5793,0x08},
	{0x5794,0x04},
	{0x5795,0x04},
	{0x57a8,0xd2},
	{0x57aa,0x2a},
	{0x57ab,0x7f},
	{0x57ac,0x00},
	{0x57ad,0x00},
	{0x5900,0xf2},
	{0x5901,0x04},
	{0x36e9,0x24},
	{0x37f9,0x53},
	/* {0x0100,0x01}, */
	{SC835HAI_REG_END, 0x00},
};

static struct regval_list sc835hai_init_regs_3840_2160_30fps_mipi[] = {
	//Cleaned_0x4f_SC835HAI_MIPI_24Minput_2lane_10bit_1350Mbps_3840x2160_30fps
	//VTS=2250.000000,HTS=4000.000000,SCLK=141.750000,PCLK=270.000000,MipiCLK=1350.000000,Tline=14.814815us,TExp_step=0.5*Tline=7.407407us,TExp_offset=2.313933us
	{0x0103,0x01},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x3018,0x3a},
	{0x3019,0x0c},
	{0x301f,0x4f},
	{0x30b8,0x44},
	{0x320c,0x08},//hts=0x834=2100
	{0x320d,0x34},//
	{0x320e,0x08},//vts=0x8ca=2250
	{0x320f,0xca},//
	{0x3301,0x0e},
	{0x3302,0x18},
	{0x3303,0x10},
	{0x3304,0x58},
	{0x3305,0x00},
	{0x3306,0x70},
	{0x3307,0x04},
	{0x3308,0x14},
	{0x3309,0x98},
	{0x330a,0x00},
	{0x330b,0xf8},
	{0x330c,0x10},
	{0x330d,0x08},
	{0x330e,0x4a},
	{0x331e,0x31},
	{0x331f,0x71},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x334c,0x10},
	{0x335d,0x60},
	{0x3364,0x5e},
	{0x3367,0x04},
	{0x338f,0x80},
	{0x3390,0x01},
	{0x3391,0x03},
	{0x3392,0x07},
	{0x3393,0x28},
	{0x3394,0x4c},
	{0x3395,0x4c},
	{0x3396,0x01},
	{0x3397,0x03},
	{0x3398,0x07},
	{0x3399,0x1a},
	{0x339a,0x1a},
	{0x339b,0x2c},
	{0x339c,0x2c},
	{0x33ad,0x24},
	{0x33ae,0x40},
	{0x33af,0x80},
	{0x33b2,0x50},
	{0x33b3,0x20},
	{0x33f8,0x00},
	{0x33f9,0x88},
	{0x33fa,0x00},
	{0x33fb,0xa0},
	{0x33fc,0x43},
	{0x33fd,0x47},
	{0x349f,0x03},
	{0x34a6,0x43},
	{0x34a7,0x47},
	{0x34a8,0x20},
	{0x34a9,0x20},
	{0x34aa,0x01},
	{0x34ab,0x10},
	{0x34ac,0x01},
	{0x34ad,0x28},
	{0x34f8,0x43},
	{0x34f9,0x0c},
	{0x3632,0x64},
	{0x363b,0x16},
	{0x363c,0x0e},
	{0x363d,0x8e},
	{0x363e,0x6c},
	{0x3654,0x00},
	{0x3674,0x94},
	{0x3675,0x94},
	{0x3676,0x68},
	{0x367c,0x41},
	{0x367d,0x43},
	{0x3690,0x35},
	{0x3691,0x35},
	{0x3692,0x55},
	{0x3693,0x40},
	{0x3694,0x41},
	{0x3696,0x81},
	{0x3697,0x80},
	{0x3698,0x80},
	{0x3699,0x83},
	{0x369a,0x81},
	{0x369b,0xff},
	{0x369c,0xff},
	{0x369d,0xff},
	{0x36a2,0x40},
	{0x36a3,0x41},
	{0x36a4,0x43},
	{0x36a5,0x47},
	{0x36a6,0x4f},
	{0x36a7,0x4f},
	{0x36a8,0x4f},
	{0x36d0,0x15},
	{0x36ea,0x0f},
	{0x36eb,0x04},
	{0x36ec,0x43},
	{0x36ed,0x2a},
	{0x370f,0x01},
	{0x3724,0xe5},
	{0x3725,0xa8},
	{0x3727,0x14},
	{0x37b0,0x17},
	{0x37b1,0x9b},
	{0x37b2,0xfb},
	{0x37b3,0x41},
	{0x37b4,0x43},
	{0x37fa,0xc7},
	{0x37fb,0x31},
	{0x37fc,0x00},
	{0x37fd,0x36},
	{0x3905,0x0f},
	{0x391f,0x41},
	{0x3933,0x80},
	{0x3934,0xd4},
	{0x3935,0x00},
	{0x3936,0x40},
	{0x3937,0x69},
	{0x3938,0x70},
	{0x3939,0xff},
	{0x393a,0xf6},
	{0x393b,0xff},
	{0x393c,0xd5},
	{0x3e00,0x01},
	{0x3e01,0x18},
	{0x3e02,0x70},
	{0x3e16,0x00},
	{0x3e17,0xbc},
	{0x3e18,0x00},
	{0x3e19,0xbc},
	{0x4424,0x02},
	{0x4509,0x1a},
	{0x450d,0x0b},
	{0x4800,0x24},
	{0x4837,0x0b},
	{0x5000,0x0e},
	{0x575c,0x10},
	{0x575d,0x08},
	{0x5780,0x76},
	{0x5784,0x10},
	{0x5785,0x08},
	{0x5787,0x0a},
	{0x5788,0x0a},
	{0x5789,0x08},
	{0x578a,0x0a},
	{0x578b,0x0a},
	{0x578c,0x08},
	{0x578d,0x40},
	{0x5790,0x08},
	{0x5791,0x04},
	{0x5792,0x04},
	{0x5793,0x08},
	{0x5794,0x04},
	{0x5795,0x04},
	{0x57a8,0xd2},
	{0x57aa,0x2a},
	{0x57ab,0x7f},
	{0x57ac,0x00},
	{0x57ad,0x00},
	{0x36e9,0x53},
	{0x37f9,0x53},
	/* {0x0100,0x01}, */
	{SC835HAI_REG_END, 0x00},
};
/*
 * the order of the jxf23_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc835hai_win_sizes[] = {
	{
		.width		= 3840,
		.height		= 2160,
		.fps		= 25 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= sc835hai_init_regs_3840_2160_25fps_mipi,
	},
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= sc835hai_init_regs_1920_1080_25fps_mipi,
	},
	{
		.width		= 1920,
		.height		= 2160,
		.fps		= 25 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= sc835hai_init_regs_1920_2160_25fps_mipi,
	},
	{
		.width		= 3840,
		.height		= 2160,
		.fps		= 30 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= sc835hai_init_regs_3840_2160_30fps_mipi,
	},
};

struct tx_isp_sensor_win_setting *wsize = &sc835hai_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sc835hai_stream_on[] = {
	{SC835HAI_REG_DELAY, 0x10},
	{0x0100, 0x01},
	{SC835HAI_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc835hai_stream_off[] = {
	{SC835HAI_REG_DELAY, 0x10},
	{0x0100, 0x00},
	{SC835HAI_REG_END, 0x00},	/* END MARKER */
};

int sc835hai_read(struct tx_isp_subdev *sd,  uint16_t reg,
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

int sc835hai_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc835hai_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC835HAI_REG_END) {
		if (vals->reg_num == SC835HAI_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc835hai_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}
	return 0;
}
#endif

static int sc835hai_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC835HAI_REG_END) {
		if (vals->reg_num == SC835HAI_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc835hai_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc835hai_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

#if 1
static int sc835hai_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sc835hai_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC835HAI_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc835hai_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;

	if (v != SC835HAI_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}
#endif

static int sc835hai_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = -1;
	int it = (value & 0xffff) * 2;
	int again = (value & 0xffff0000) >> 16;

	ret += sc835hai_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc835hai_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc835hai_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));

	ret += sc835hai_write(sd, 0x3e09, (unsigned char)(again & 0xff));
	ret += sc835hai_write(sd, 0x3e08, (unsigned char)(((again >> 8) & 0xff)));

	return 0;
}

#if 0
static int sc835hai_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = 0;

	ret = sc835hai_write(sd,  0x3e00, (unsigned char)((it >> 12) & 0x0f));
	ret += sc835hai_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc835hai_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
	if (ret < 0) {
		ISP_ERROR("sc835hai_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int sc835hai_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;


	ret += sc835hai_write(sd, 0x3e09, (unsigned char)((value >> 8) & 0xff));
	ret += sc835hai_write(sd, 0x3e07, (unsigned char)(value & 0xff));

	if (ret < 0) {
		ISP_ERROR("sc835hai_write error  %d" ,__LINE__ );
		return ret;
	}

	return 0;
}
#endif

static int sc835hai_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc835hai_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc835hai_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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

static int sc835hai_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT){
			ret = sc835hai_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sc835hai_write_array(sd, sc835hai_stream_on);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("sc835hai stream on\n");
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
	} else {
		ret = sc835hai_write_array(sd, sc835hai_stream_off);
		pr_debug("sc835hai stream off\n");
	}

	return ret;
}

static int sc835hai_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned short vts = 0;
	unsigned short hts=0;
	unsigned int sensor_max_fps;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;
	unsigned char val;

	switch(sensor->info.default_boot){
	case 0:
		sclk = 2100 * 2400 * 25;
		sensor_max_fps = TX_SENSOR_MAX_FPS_25;
		break;
	case 1:
		sclk = 2100 * 1350 * 25;
		sensor_max_fps = TX_SENSOR_MAX_FPS_25;
		break;
	case 2:
		sclk = 2100 * 2700 * 25;
		sensor_max_fps = TX_SENSOR_MAX_FPS_25;
		break;
	case 3:
		sclk = 2100 * 2250 * 30;
		sensor_max_fps = TX_SENSOR_MAX_FPS_30;
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

	val = 0;
	ret += sc835hai_read(sd, 0x320c, &val);
	hts = val<<8;
	val = 0;
	ret += sc835hai_read(sd, 0x320d, &val);
	hts |= val;

	if (0 != ret) {
		ISP_ERROR("err: sc835hai read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	sc835hai_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	sc835hai_write(sd, 0x320e, (unsigned char)(vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: sc835hai_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 7;
	sensor->video.attr->integration_time_limit = vts - 7;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 7;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	if (ret) {
		ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
	}

	return ret;
}


static int sc835hai_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	val = sc835hai_read(sd, 0x3221, &val);
	switch(enable) {
	case 0:
		sc835hai_write(sd, 0x3221, val | 0x60);
		break;
	case 1:
		sc835hai_write(sd, 0x3221, val | 0x66);
		break;
	case 2:
		sc835hai_write(sd, 0x3221, val & 0x99);
		break;
	case 3:
		sc835hai_write(sd, 0x3221, val | 0x06);
		break;
	}

	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}


static int sc835hai_set_mode(struct tx_isp_subdev *sd, int value)
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
		wsize = &sc835hai_win_sizes[0];
		memcpy(&sc835hai_attr.mipi, &sc835hai_mipi, sizeof(sc835hai_mipi));
		sc835hai_attr.mipi.clk = 1216;
		sc835hai_attr.min_integration_time = 2;
		sc835hai_attr.min_integration_time_native = 2;
		sc835hai_attr.total_width = 2100;
		sc835hai_attr.total_height = 2400;
		sc835hai_attr.max_integration_time_native = 2400 - 7;
		sc835hai_attr.integration_time_limit = 2400 - 7;
		sc835hai_attr.max_integration_time = 2400 - 7;
		sc835hai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sc835hai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		break;
	case 1:
		wsize = &sc835hai_win_sizes[1];
		memcpy(&sc835hai_attr.mipi, &sc835hai_mipi, sizeof(sc835hai_mipi));
		sc835hai_attr.mipi.clk = 360;
		sc835hai_attr.mipi.image_twidth = 1920;
		sc835hai_attr.mipi.image_theight = 1080;
		sc835hai_attr.min_integration_time = 2;
		sc835hai_attr.min_integration_time_native = 2;
		sc835hai_attr.total_width = 2100;
		sc835hai_attr.total_height = 1350;
		sc835hai_attr.max_integration_time_native = 1350 - 7;
		sc835hai_attr.integration_time_limit = 1350 - 7;
		sc835hai_attr.max_integration_time = 1350 - 7;
		sc835hai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sc835hai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		break;
	case 2:
		wsize = &sc835hai_win_sizes[2];
		memcpy(&sc835hai_attr.mipi, &sc835hai_mipi, sizeof(sc835hai_mipi));
		sc835hai_attr.mipi.clk = 720;
		sc835hai_attr.mipi.image_twidth = 1920;
		sc835hai_attr.mipi.image_theight = 2160;
		sc835hai_attr.min_integration_time = 2;
		sc835hai_attr.min_integration_time_native = 2;
		sc835hai_attr.total_width = 2100;
		sc835hai_attr.total_height = 2700;
		sc835hai_attr.max_integration_time_native = 2700 - 7;
		sc835hai_attr.integration_time_limit = 2700 - 7;
		sc835hai_attr.max_integration_time = 2700 - 7;
		sc835hai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sc835hai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		break;
	case 3:
		wsize = &sc835hai_win_sizes[3];
		memcpy(&sc835hai_attr.mipi, &sc835hai_mipi, sizeof(sc835hai_mipi));
		sc835hai_attr.mipi.clk = 1350;
		sc835hai_attr.min_integration_time = 2;
		sc835hai_attr.min_integration_time_native = 2;
		sc835hai_attr.total_width = 2100;
		sc835hai_attr.total_height = 2250;
		sc835hai_attr.max_integration_time_native = 2250 - 7;
		sc835hai_attr.integration_time_limit = 2250 - 7;
		sc835hai_attr.max_integration_time = 2250 - 7;
		sc835hai_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sc835hai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		break;
	default:
		ISP_ERROR("Have no this setting!!!\n");
	}


	switch(info->video_interface){
	case TISP_SENSOR_VI_MIPI_CSI0:
		sc835hai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc835hai_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_DVP:
		sc835hai_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	//sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

	return 0;

err_get_mclk:
	return -1;
}

static int sc835hai_g_chip_ident(struct tx_isp_subdev *sd,
				 struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc835hai_reset");
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
		ret = private_gpio_request(pwdn_gpio,"sc835hai_pwdn");
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
	ret = sc835hai_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc835hai chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("sc835hai chip found @ 0x%02x (%s)\n sensor drv version %s", client->addr, client->adapter->name, SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc835hai", sizeof("sc835hai"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}


static int sc835hai_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}

	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = sc835hai_set_expo(sd, sensor_val->value);
		break;
		/* case TX_ISP_EVENT_SENSOR_INT_TIME: */
		/* 	if(arg) */
		/* 		ret = sc835hai_set_integration_time(sd, sensor_val->value); */
		/* 	break; */
		/* case TX_ISP_EVENT_SENSOR_AGAIN: */
		/* 	if(arg) */
		/* 		ret = sc835hai_set_analog_gain(sd, sensor_val->value); */
		/* 	break; */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc835hai_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc835hai_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc835hai_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = sc835hai_write_array(sd, sc835hai_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sc835hai_write_array(sd, sc835hai_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc835hai_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = sc835hai_set_vflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int sc835hai_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc835hai_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc835hai_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc835hai_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc835hai_core_ops = {
	.g_chip_ident = sc835hai_g_chip_ident,
	.reset = sc835hai_reset,
	.init = sc835hai_init,
	/*.ioctl = sc835hai_ops_ioctl,*/
	.g_register = sc835hai_g_register,
	.s_register = sc835hai_s_register,
};

static struct tx_isp_subdev_video_ops sc835hai_video_ops = {
	.s_stream = sc835hai_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc835hai_sensor_ops = {
	.ioctl	= sc835hai_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc835hai_ops = {
	.core = &sc835hai_core_ops,
	.video = &sc835hai_video_ops,
	.sensor = &sc835hai_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc835hai",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc835hai_probe(struct i2c_client *client, const struct i2c_device_id *id)
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
	sc835hai_attr.expo_fs = 1;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sc835hai_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc835hai_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc835hai\n");

	return 0;
}

static int sc835hai_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc835hai_id[] = {
	{ "sc835hai", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc835hai_id);

static struct i2c_driver sc835hai_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc835hai",
	},
	.probe		= sc835hai_probe,
	.remove		= sc835hai_remove,
	.id_table	= sc835hai_id,
};

static __init int init_sc835hai(void)
{
	return private_i2c_add_driver(&sc835hai_driver);
}

static __exit void exit_sc835hai(void)
{
	private_i2c_del_driver(&sc835hai_driver);
}

module_init(init_sc835hai);
module_exit(exit_sc835hai);

MODULE_DESCRIPTION("A low-level driver for sc835hai sensors");
MODULE_LICENSE("GPL");
