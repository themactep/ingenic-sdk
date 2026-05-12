/*
* gc08a8.c
*
* Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* Settings:
* sboot        resolution       fps     interface              mode
*   0          3264*2448        30     mipi_2lane             linear
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

#define GC08A8_CHIP_ID_H (0x08)
#define GC08A8_CHIP_ID_L (0xa8)
#define GC08A8_REG_END 0xffff
#define GC08A8_REG_DELAY 0x0000
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20250304a"

static int reset_gpio = -1;//GPIO_PA(18);
static int pwdn_gpio = -1;
static int shvflip = 1;


struct regval_list
{
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

struct again_lut gc08a8_again_lut[] = {
	{0x400,0},
	{0x440,5732},
	{0x480,11136},
	{0x4C0,16248},
	{0x500,21098},
	{0x540,25711},
	{0x580,30109},
	{0x5C0,34312},
	{0x600,38336},
	{0x640,42196},
	{0x680,45904},
	{0x6C0,49472},
	{0x700,52911},
	{0x740,56229},
	{0x780,59434},
	{0x7C0,62534},
	{0x800,65536},
	{0x840,68445},
	{0x880,71268},
	{0x8C0,74009},
	{0x900,76672},
	{0x940,79263},
	{0x980,81784},
	{0x9C0,84240},
	{0xA00,86634},
	{0xA40,88969},
	{0xA80,91247},
	{0xAC0,93472},
	{0xB00,95645},
	{0xB40,97770},
	{0xB80,99848},
	{0xBC0,101882},
	{0xC00,103872},
	{0xC40,105822},
	{0xC80,107732},
	{0xCC0,109604},
	{0xD00,111440},
	{0xD40,113241},
	{0xD80,115008},
	{0xDC0,116743},
	{0xE00,118447},
	{0xE40,120120},
	{0xE80,121765},
	{0xEC0,123381},
	{0xF00,124970},
	{0xF40,126533},
	{0xF80,128070},
	{0xFC0,129583},
	{0x1000,131072},
	{0x1040,132538},
	{0x1080,133981},
	{0x10C0,135403},
	{0x1100,136804},
	{0x1140,138184},
	{0x1180,139545},
	{0x11C0,140886},
	{0x1200,142208},
	{0x1240,143512},
	{0x1280,144799},
	{0x12C0,146068},
	{0x1300,147320},
	{0x1340,148556},
	{0x1380,149776},
	{0x13C0,150981},
	{0x1400,152170},
	{0x1440,153344},
	{0x1480,154505},
	{0x14C0,155651},
	{0x1500,156783},
	{0x1540,157902},
	{0x1580,159008},
	{0x15C0,160101},
	{0x1600,161181},
	{0x1640,162250},
	{0x1680,163306},
	{0x16C0,164351},
	{0x1700,165384},
	{0x1740,166406},
	{0x1780,167418},
	{0x17C0,168418},
	{0x1800,169408},
	{0x1840,170388},
	{0x1880,171358},
	{0x18C0,172318},
	{0x1900,173268},
	{0x1940,174209},
	{0x1980,175140},
	{0x19C0,176062},
	{0x1A00,176976},
	{0x1A40,177881},
	{0x1A80,178777},
	{0x1AC0,179665},
	{0x1B00,180544},
	{0x1B40,181416},
	{0x1B80,182279},
	{0x1BC0,183135},
	{0x1C00,183983},
	{0x1C40,184823},
	{0x1C80,185656},
	{0x1CC0,186482},
	{0x1D00,187301},
	{0x1D40,188112},
	{0x1D80,188917},
	{0x1DC0,189715},
	{0x1E00,190506},
	{0x1E40,191291},
	{0x1E80,192069},
	{0x1EC0,192841},
	{0x1F00,193606},
	{0x1F40,194366},
	{0x1F80,195119},
	{0x1FC0,195866},
	{0x2000,196608},
	{0x2040,197344},
	{0x2080,198074},
	{0x20C0,198798},
	{0x2100,199517},
	{0x2140,200231},
	{0x2180,200939},
	{0x21C0,201642},
	{0x2200,202340},
	{0x2240,203033},
	{0x2280,203720},
	{0x22C0,204403},
	{0x2300,205081},
	{0x2340,205754},
	{0x2380,206422},
	{0x23C0,207085},
	{0x2400,207744},
	{0x2440,208399},
	{0x2480,209048},
	{0x24C0,209694},
	{0x2500,210335},
	{0x2540,210971},
	{0x2580,211604},
	{0x25C0,212232},
	{0x2600,212856},
	{0x2640,213476},
	{0x2680,214092},
	{0x26C0,214704},
	{0x2700,215312},
	{0x2740,215916},
	{0x2780,216517},
	{0x27C0,217113},
	{0x2800,217706},
	{0x2840,218295},
	{0x2880,218880},
	{0x28C0,219462},
	{0x2900,220041},
	{0x2940,220615},
	{0x2980,221187},
	{0x29C0,221754},
	{0x2A00,222319},
	{0x2A40,222880},
	{0x2A80,223438},
	{0x2AC0,223992},
	{0x2B00,224544},
	{0x2B40,225092},
	{0x2B80,225637},
	{0x2BC0,226179},
	{0x2C00,226717},
	{0x2C40,227253},
	{0x2C80,227786},
	{0x2CC0,228315},
	{0x2D00,228842},
	{0x2D40,229366},
	{0x2D80,229887},
	{0x2DC0,230405},
	{0x2E00,230920},
	{0x2E40,231433},
	{0x2E80,231942},
	{0x2EC0,232449},
	{0x2F00,232954},
	{0x2F40,233455},
	{0x2F80,233954},
	{0x2FC0,234450},
	{0x3000,234944},
	{0x3040,235435},
	{0x3080,235924},
	{0x30C0,236410},
	{0x3100,236894},
	{0x3140,237375},
	{0x3180,237854},
	{0x31C0,238330},
	{0x3200,238804},
	{0x3240,239275},
	{0x3280,239745},
	{0x32C0,240211},
	{0x3300,240676},
	{0x3340,241138},
	{0x3380,241598},
	{0x33C0,242056},
	{0x3400,242512},
	{0x3440,242965},
	{0x3480,243417},
	{0x34C0,243866},
	{0x3500,244313},
	{0x3540,244758},
	{0x3580,245201},
	{0x35C0,245642},
	{0x3600,246080},
	{0x3640,246517},
	{0x3680,246952},
	{0x36C0,247384},
	{0x3700,247815},
	{0x3740,248244},
	{0x3780,248671},
	{0x37C0,249096},
	{0x3800,249519},
	{0x3840,249940},
	{0x3880,250359},
	{0x38C0,250777},
	{0x3900,251192},
	{0x3940,251606},
	{0x3980,252018},
	{0x39C0,252428},
	{0x3A00,252837},
	{0x3A40,253243},
	{0x3A80,253648},
	{0x3AC0,254051},
	{0x3B00,254453},
	{0x3B40,254853},
	{0x3B80,255251},
	{0x3BC0,255647},
	{0x3C00,256042},
	{0x3C40,256435},
	{0x3C80,256827},
	{0x3CC0,257217},
	{0x3D00,257605},
	{0x3D40,257992},
	{0x3D80,258377},
	{0x3DC0,258760},
	{0x3E00,259142},
	{0x3E40,259523},
	{0x3E80,259902},
	{0x3EC0,260279},
	{0x3F00,260655},
	{0x3F40,261029},
	{0x3F80,261402},
	{0x3FC0,261774},
	{0x4000,262144},
};

struct tx_isp_sensor_attribute gc08a8_attr;

unsigned int gc08a8_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;

	*sensor_it = expo;

	return isp_it;
}



unsigned int gc08a8_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = gc08a8_again_lut;
	while(lut->gain <= gc08a8_attr.max_again) {
		if(isp_gain == 0) {
				*sensor_again = lut->value;
				return 0;
		} else if(isp_gain < lut->gain) {
				*sensor_again = (lut - 1)->value;
				return (lut - 1)->gain;
		} else {
				if((lut->gain == gc08a8_attr.max_again) && (isp_gain >= lut->gain)) {
						*sensor_again = lut->value;
						return lut->gain;
				}
		}

		lut++;
}

return isp_gain;
}

unsigned int gc08a8_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus gc08a8_mipi = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1344,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10, // RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 3264,
	.image_theight = 2448,
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
	.mipi_sc.data_type_value = 0,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute gc08a8_attr = {
	.name = "gc08a8",
	.chip_id = 0x08a8,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.cbus_device = 0x31,
	.max_again = 262144,
	.max_dgain = 0,
	.expo_fs = 1,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = gc08a8_alloc_again,
	.sensor_ctrl.alloc_dgain = gc08a8_alloc_dgain,
};

static struct regval_list gc08a8_init_regs_3264_2448_30fps_mipi[] = {
	{0x031c,0x60},
	{0x0337,0x04},
	{0x0335,0x51},
	{0x0336,0x70},
	{0x0383,0xbb},
	{0x031a,0x00},
	{0x0321,0x10},
	{0x0327,0x03},
	{0x0325,0x40},
	{0x0326,0x23},
	{0x0314,0x11},
	{0x0315,0xd6},
	{0x0316,0x01},
	{0x0334,0xc0},
	{0x0324,0x42},
	{0x031c,0x00},
	{0x031c,0x9f},
	{0x039a,0x43},
	{0x0084,0x30},
	{0x02b3,0x08},
	{0x0057,0x0c},
	{0x05c3,0x50},
	{0x0311,0x90},
	{0x05a0,0x02},
	{0x0074,0x0a},
	{0x0059,0x11},
	{0x0070,0x05},
	{0x0101,0x00},
	{0x0344,0x00},
	{0x0345,0x06},
	{0x0346,0x00},
	{0x0347,0x04},
	{0x0348,0x0c},
	{0x0349,0xd0},
	{0x034a,0x09},
	{0x034b,0x9c},
	{0x0202,0x09},
	{0x0203,0x04},
	{0x0340,0x09},
	{0x0341,0xf4},
	{0x0342,0x07},
	{0x0343,0x1c},
	{0x0219,0x05},
	{0x0226,0x00},
	{0x0227,0x28},
	{0x0e0a,0x00},
	{0x0e0b,0x00},
	{0x0e24,0x04},
	{0x0e25,0x04},
	{0x0e26,0x00},
	{0x0e27,0x10},
	{0x0e01,0x74},
	{0x0e03,0x47},
	{0x0e04,0x33},
	{0x0e05,0x44},
	{0x0e06,0x44},
	{0x0e0c,0x1e},
	{0x0e17,0x3a},
	{0x0e18,0x3c},
	{0x0e19,0x40},
	{0x0e1a,0x42},
	{0x0e28,0x21},
	{0x0e2b,0x68},
	{0x0e2c,0x0d},
	{0x0e2d,0x08},
	{0x0e34,0xf4},
	{0x0e35,0x44},
	{0x0e36,0x07},
	{0x0e38,0x39},
	{0x0210,0x13},
	{0x0218,0x00},
	{0x0241,0x88},
	{0x0e32,0x00},
	{0x0e33,0x18},
	{0x0e42,0x03},
	{0x0e43,0x80},
	{0x0e44,0x04},
	{0x0e45,0x00},
	{0x0e4f,0x04},
	{0x057a,0x20},
	{0x0381,0x7c},
	{0x0382,0x9b},
	{0x0384,0xfb},
	{0x0389,0x38},
	{0x038a,0x03},
	{0x0390,0x6a},
	{0x0391,0x0f},
	{0x0392,0x60},
	{0x0393,0xc1},
	{0x0396,0x3f},
	{0x0398,0x22},
	{0x031c,0x80},
	{0x03fe,0x10},
	{0x03fe,0x00},
	{0x031c,0x9f},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x031c,0x80},
	{0x03fe,0x10},
	{0x03fe,0x00},
	{0x031c,0x9f},
	{0x0360,0x01},
	{0x0360,0x00},
	{0x0316,0x09},
	{0x0a67,0x80},
	{0x0313,0x00},
	{0x0a53,0x0e},
	{0x0a65,0x17},
	{0x0a68,0xa1},
	{0x0a58,0x00},
	{0x0ace,0x0c},
	{0x00a4,0x00},
	{0x00a5,0x01},
	{0x00a7,0x09},
	{0x00a8,0x9c},
	{0x00a9,0x0c},
	{0x00aa,0xd0},
	{0x0a8a,0x00},
	{0x0a8b,0xe0},
	{0x0a8c,0x13},
	{0x0a8d,0xe8},
	{0x0a90,0x0a},
	{0x0a91,0x10},
	{0x0a92,0xf8},
	{0x0a71,0xf2},
	{0x0a72,0x12},
	{0x0a73,0x64},
	{0x0a75,0x41},
	{0x0a70,0x07},
	{0x0313,0x80},
	{0x00a0,0x01},
	{0x0080,0xd2},
	{0x0081,0x3f},
	{0x0087,0x51},
	{0x0089,0x03},
	{0x009b,0x40},
	{0x0096,0x81},
	{0x0097,0x08},
	{0x05a0,0x82},
	{0x05ac,0x00},
	{0x05ad,0x01},
	{0x05ae,0x00},
	{0x0800,0x0a},
	{0x0801,0x14},
	{0x0802,0x28},
	{0x0803,0x34},
	{0x0804,0x0e},
	{0x0805,0x33},
	{0x0806,0x03},
	{0x0807,0x8a},
	{0x0808,0x3e},
	{0x0809,0x00},
	{0x080a,0x28},
	{0x080b,0x03},
	{0x080c,0x1d},
	{0x080d,0x03},
	{0x080e,0x16},
	{0x080f,0x03},
	{0x0810,0x10},
	{0x0811,0x03},
	{0x0812,0x00},
	{0x0813,0x00},
	{0x0814,0x01},
	{0x0815,0x00},
	{0x0816,0x01},
	{0x0817,0x00},
	{0x0818,0x00},
	{0x0819,0x0a},
	{0x081a,0x01},
	{0x081b,0x6c},
	{0x081c,0x00},
	{0x081d,0x0b},
	{0x081e,0x02},
	{0x081f,0x00},
	{0x0820,0x00},
	{0x0821,0x0c},
	{0x0822,0x02},
	{0x0823,0xd9},
	{0x0824,0x00},
	{0x0825,0x0d},
	{0x0826,0x03},
	{0x0827,0xf0},
	{0x0828,0x00},
	{0x0829,0x0e},
	{0x082a,0x05},
	{0x082b,0x94},
	{0x082c,0x09},
	{0x082d,0x6e},
	{0x082e,0x07},
	{0x082f,0xe6},
	{0x0830,0x10},
	{0x0831,0x0e},
	{0x0832,0x0b},
	{0x0833,0x2c},
	{0x0834,0x14},
	{0x0835,0xae},
	{0x0836,0x0f},
	{0x0837,0xc4},
	{0x0838,0x18},
	{0x0839,0x0e},
	{0x05ac,0x01},
	{0x059a,0x00},
	{0x059b,0x00},
	{0x059c,0x01},
	{0x0598,0x00},
	{0x0597,0x14},
	{0x05ab,0x09},
	{0x05a4,0x02},
	{0x05a3,0x05},
	{0x05a0,0xc2},
	{0x0207,0xc4},
	{0x0208,0x01},
	{0x0209,0x78},
	{0x0204,0x04},
	{0x0205,0x00},
	{0x0040,0x22},
	{0x0041,0x20},
	{0x0043,0x10},
	{0x0044,0x00},
	{0x0046,0x08},
	{0x0047,0xf0},
	{0x0048,0x0f},
	{0x004b,0x0f},
	{0x004c,0x00},
	{0x0050,0x5c},
	{0x0051,0x44},
	{0x005b,0x03},
	{0x00c0,0x00},
	{0x00c1,0x80},
	{0x00c2,0x31},
	{0x00c3,0x00},
	{0x0460,0x04},
	{0x0462,0x08},
	{0x0464,0x0e},
	{0x0466,0x0a},
	{0x0468,0x12},
	{0x046a,0x12},
	{0x046c,0x10},
	{0x046e,0x0c},
	{0x0461,0x03},
	{0x0463,0x03},
	{0x0465,0x03},
	{0x0467,0x03},
	{0x0469,0x04},
	{0x046b,0x04},
	{0x046d,0x04},
	{0x046f,0x04},
	{0x0470,0x04},
	{0x0472,0x10},
	{0x0474,0x26},
	{0x0476,0x38},
	{0x0478,0x20},
	{0x047a,0x30},
	{0x047c,0x38},
	{0x047e,0x60},
	{0x0471,0x05},
	{0x0473,0x05},
	{0x0475,0x05},
	{0x0477,0x05},
	{0x0479,0x04},
	{0x047b,0x04},
	{0x047d,0x04},
	{0x047f,0x04},
	{0x031c,0x60},
	{0x0337,0x04},
	{0x0335,0x51},
	{0x0336,0x70},
	{0x0383,0xbb},
	{0x031a,0x00},
	{0x0321,0x10},
	{0x0327,0x03},
	{0x0325,0x40},
	{0x0326,0x23},
	{0x0314,0x11},
	{0x0315,0xd6},
	{0x0316,0x01},
	{0x0334,0xc0},
	{0x0324,0x42},
	{0x031c,0x00},
	{0x031c,0x9f},
	{0x0344,0x00},
	{0x0345,0x06},
	{0x0346,0x00},
	{0x0347,0x04},
	{0x0348,0x0c},
	{0x0349,0xd0},
	{0x034a,0x09},
	{0x034b,0x9c},
	{0x0202,0x09},
	{0x0203,0x04},
	{0x0340,0x09},
	{0x0341,0xf4},
	{0x0342,0x07},
	{0x0343,0x1c},
	{0x0226,0x00},
	{0x0227,0x28},
	{0x0e38,0x39},
	{0x0210,0x13},
	{0x0218,0x00},
	{0x0241,0x88},
	{0x0392,0x60},
	{0x031c,0x80},
	{0x03fe,0x10},
	{0x03fe,0x00},
	{0x031c,0x9f},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x03fe,0x00},
	{0x031c,0x80},
	{0x03fe,0x10},
	{0x03fe,0x00},
	{0x031c,0x9f},
	{0x00a2,0x00},
	{0x00a3,0x00},
	{0x00ab,0x00},
	{0x00ac,0x00},
	{0x05a0,0x82},
	{0x05ac,0x00},
	{0x05ad,0x01},
	{0x05ae,0x00},
	{0x0800,0x0a},
	{0x0801,0x14},
	{0x0802,0x28},
	{0x0803,0x34},
	{0x0804,0x0e},
	{0x0805,0x33},
	{0x0806,0x03},
	{0x0807,0x8a},
	{0x0808,0x3e},
	{0x0809,0x00},
	{0x080a,0x28},
	{0x080b,0x03},
	{0x080c,0x1d},
	{0x080d,0x03},
	{0x080e,0x16},
	{0x080f,0x03},
	{0x0810,0x10},
	{0x0811,0x03},
	{0x0812,0x00},
	{0x0813,0x00},
	{0x0814,0x01},
	{0x0815,0x00},
	{0x0816,0x01},
	{0x0817,0x00},
	{0x0818,0x00},
	{0x0819,0x0a},
	{0x081a,0x01},
	{0x081b,0x6c},
	{0x081c,0x00},
	{0x081d,0x0b},
	{0x081e,0x02},
	{0x081f,0x00},
	{0x0820,0x00},
	{0x0821,0x0c},
	{0x0822,0x02},
	{0x0823,0xd9},
	{0x0824,0x00},
	{0x0825,0x0d},
	{0x0826,0x03},
	{0x0827,0xf0},
	{0x0828,0x00},
	{0x0829,0x0e},
	{0x082a,0x05},
	{0x082b,0x94},
	{0x082c,0x09},
	{0x082d,0x6e},
	{0x082e,0x07},
	{0x082f,0xe6},
	{0x0830,0x10},
	{0x0831,0x0e},
	{0x0832,0x0b},
	{0x0833,0x2c},
	{0x0834,0x14},
	{0x0835,0xae},
	{0x0836,0x0f},
	{0x0837,0xc4},
	{0x0838,0x18},
	{0x0839,0x0e},
	{0x05ac,0x01},
	{0x059a,0x00},
	{0x059b,0x00},
	{0x059c,0x01},
	{0x0598,0x00},
	{0x0597,0x14},
	{0x05ab,0x09},
	{0x05a4,0x02},
	{0x05a3,0x05},
	{0x05a0,0xc2},
	{0x0207,0xc4},
	{0x0204,0x04},
	{0x0205,0x00},
	{0x0050,0x5c},
	{0x0051,0x44},
	{0x009a,0x00},
	{0x0351,0x00},
	{0x0352,0x06},
	{0x0353,0x00},
	{0x0354,0x08},
	{0x034c,0x0c},
	{0x034d,0xc0},
	{0x034e,0x09},
	{0x034f,0x90},
	{0x0114,0x01},
	{0x0180,0x67},
	{0x0181,0x30},
	{0x0185,0x01},
	{0x0115,0x10},
	{0x011b,0x12},
	{0x011c,0x12},
	{0x0121,0x0b},
	{0x0122,0x0d},
	{0x0123,0x2f},
	{0x0124,0x01},
	{0x0125,0x12},
	{0x0126,0x0f},
	{0x0129,0x0c},
	{0x012a,0x13},
	{0x012b,0x0f},
	{0x0a73,0x60},
	{0x0a70,0x11},
	{0x0313,0x80},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0aff,0x00},
	{0x0a70,0x00},
	{0x00a4,0x80},
	{0x0316,0x01},
	{0x0a67,0x00},
	{0x0084,0x10},
	{0x0102,0x09},
	{0x0100,0x01},
	{GC08A8_REG_END, 0x00}, /* END MARKER */
};

/*
* the order of the jxf23_win_sizes is [full_resolution, preview_resolution].
*/
static struct tx_isp_sensor_win_setting gc08a8_win_sizes[] = {
		/* 3264*2448 [0] */
		{
			.width          = 3264,
			.height         = 2448,
			.fps            = 30 << 16 | 1,
			.mbus_code      = TISP_VI_FMT_SRGGB10_1X10,
			.colorspace     = TISP_COLORSPACE_SRGB,
			.regs           = gc08a8_init_regs_3264_2448_30fps_mipi,
	},
};

struct tx_isp_sensor_win_setting *wsize = &gc08a8_win_sizes[0];

static struct regval_list gc08a8_stream_on[] = {
	// {0x0100,  0x01},
	{GC08A8_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list gc08a8_stream_off[] = {
	// {0x0100,  0x00},
	{GC08A8_REG_END, 0x00}, /* END MARKER */
};

int gc08a8_read(struct tx_isp_subdev *sd, uint16_t reg,
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
		}};
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int gc08a8_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int gc08a8_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != GC08A8_REG_END) {
		if (vals->reg_num == GC08A8_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc08a8_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int gc08a8_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != GC08A8_REG_END)
	{
		if (vals->reg_num == GC08A8_REG_DELAY)
		{
			private_msleep(vals->value);
		}
		else
		{
			ret = gc08a8_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int gc08a8_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int gc08a8_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v = 0;
	int ret;

	ret = gc08a8_read(sd, 0x03f0, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != GC08A8_CHIP_ID_H)
		return -ENODEV;
	ret = gc08a8_read(sd, 0x03f1, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != GC08A8_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int gc08a8_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret += gc08a8_write(sd, 0x0202, (unsigned char)((it >> 8) & 0xff));
	ret += gc08a8_write(sd, 0x0203, (unsigned char)(it & 0xff));

	ret += gc08a8_write(sd, 0x0204, (unsigned char)((again >> 8) & 0xff));
	ret += gc08a8_write(sd, 0x0205, (unsigned char)(again & 0xff));

	return ret;
}


#if 0
static int gc08a8_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = gc08a8_write(sd, 0x0203, value & 0xff);
	ret += gc08a8_write(sd, 0x0202, value >> 8);
	if (ret < 0)
		ISP_ERROR("gc08a8_write error  %d\n" ,__LINE__ );

	return ret;
}

static int gc08a8_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	struct again_lut *val_lut = gc08a8_again_lut;

	ret += gc08a8_write(sd, 0x031d ,0x2d);
	ret += gc08a8_write(sd, 0x0614, val_lut[value].reg614);
	ret += gc08a8_write(sd, 0x0615, val_lut[value].reg615);
	ret += gc08a8_write(sd, 0x031d, 0x28);
	ret += gc08a8_write(sd, 0x0225, val_lut[value].reg225);
	ret += gc08a8_write(sd, 0x1467, val_lut[value].reg1467);
	ret += gc08a8_write(sd, 0x1468, val_lut[value].reg1468);
	ret += gc08a8_write(sd, 0x00b8, val_lut[value].regb8);
	ret += gc08a8_write(sd, 0x00b9, val_lut[value].regb9);
	if (ret < 0)
		ISP_ERROR("gc08a8_write error  %d\n" ,__LINE__ );

	return ret;
}
#endif

static int gc08a8_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc08a8_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_set_attr(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;

	return 0;
}

static int gc08a8_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable)
		return ISP_SUCCESS;

	sensor_set_attr(sd, wsize);
	sensor->video.state = TX_ISP_MODULE_DEINIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int gc08a8_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	if (init->enable)
	{
		if (sensor->video.state == TX_ISP_MODULE_DEINIT)
		{
			ret = gc08a8_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if (sensor->video.state == TX_ISP_MODULE_RUNNING)
		{
			ret = gc08a8_write_array(sd, gc08a8_stream_on);
			ISP_WARNING("gc08a8 stream on\n");
		}
	}
	else
	{
		ret = gc08a8_write_array(sd, gc08a8_stream_off);
		pr_debug("gc08a8 stream off\n");
	}

	return ret;
}

static int gc08a8_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int vts = 0;
	unsigned int hts = 0;
	unsigned int max_fps;
	unsigned char tmp;
	unsigned int newformat = 0; // the format is 24.8
	int ret = 0;
	switch (sensor->info.default_boot)
	{
	case 0:
		sclk = 0x71c * 2 * 0x9f4 * 30;  /**< HTS * VTS * FPS */
		max_fps = 30;
		break;
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	/* the format of fps is 16/16. for example 30 << 16 | 2, the value is 30/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8))
	{
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}
	ret += gc08a8_read(sd, 0x0342, &tmp);
	hts = tmp & 0x0f;
	ret += gc08a8_read(sd, 0x0343, &tmp);
	if (ret < 0)
		return -1;
	hts = ((hts << 8) | tmp) << 1;
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = gc08a8_write(sd, 0x0340, (unsigned char) ((vts >> 8) & 0xff));
	ret += gc08a8_write(sd, 0x0341, (unsigned char) (vts & 0xff));
	if (ret < 0)
		return -1;
	printk("vts=%x hts=%x fps%d\n",vts,hts,fps);

	sensor->video.attr->max_integration_time_native = vts - 16;
	sensor->video.attr->integration_time_limit = vts - 16;
	sensor->video.attr->max_integration_time = vts - 16;

	sensor->video.fps = fps;
	sensor->video.attr->total_height = vts;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return 0;
}

static int gc08a8_set_hvflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = ISP_SUCCESS;
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	unsigned char val = 0x0;


	/* 2'b01:mirror,2'b10:filp */
	ret = gc08a8_read(sd, 0x0101, &val);
	switch(enable) {
	case 0:
		val &= 0xFC;
		sensor->video.mbus.code = TISP_VI_FMT_SRGGB10_1X10;
		break;
	case 1:
		val = ((val & 0xFC) | 0x01);
		sensor->video.mbus.code = TISP_VI_FMT_SGRBG10_1X10;
		break;
	case 2:
		val = ((val & 0xFC) | 0x02);
		sensor->video.mbus.code = TISP_VI_FMT_SGBRG10_1X10;
		break;
	case 3:
		val |= 0x03;
		sensor->video.mbus.code = TISP_VI_FMT_SBGGR10_1X10;
		break;
	}
	ret += gc08a8_write(sd, 0x0101, val);

	sensor->video.mbus_change = 1;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int gc08a8_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize)
	{
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}
	return ret;
}

static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	unsigned long rate;
	int ret = 0;

	switch (info->default_boot)
	{
	case 0:
		wsize = &gc08a8_win_sizes[0];
		memcpy(&gc08a8_attr.mipi, &gc08a8_mipi, sizeof(gc08a8_mipi));
		gc08a8_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		gc08a8_attr.one_line_expr_in_us = 19;
		gc08a8_attr.total_width = 0x071c * 2;
		gc08a8_attr.total_height = 0x09f4;
		gc08a8_attr.max_integration_time_native = 0x09f4 - 16;
		gc08a8_attr.integration_time_limit = 0x09f4 - 16;
		gc08a8_attr.max_integration_time = 0x09f4 - 16;
		gc08a8_attr.again = 0;
		gc08a8_attr.integration_time = 0x10;
		gc08a8_attr.max_again = 262144;
		break;
	default:
		ISP_ERROR("Have no this setting!!!\n");
	}

	switch (info->mclk)
	{
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

	if (IS_ERR(sensor->mclk))
	{
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	rate = private_clk_get_rate(sensor->mclk);
	if (((rate / 1000) % 27000) != 0)
	{
		ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
		sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
		if (IS_ERR(sclka))
		{
			pr_err("get sclka failed\n");
		}
		else
		{
			rate = private_clk_get_rate(sclka);
			if (((rate / 1000) % 27000) != 0)
			{
				private_clk_set_rate(sclka, 891000000);
			}
		}
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_prepare_enable(sensor->mclk);
	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;
	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;
	return 0;

err_get_mclk:
	return -1;
}

static int gc08a8_g_chip_ident(struct tx_isp_subdev *sd,
							struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if (reset_gpio != -1)
	{
		ret = private_gpio_request(reset_gpio, "gc08a8_reset");
		if (!ret)
		{
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}
		else
		{
			ISP_ERROR("gpio requrest fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1)
	{
		ret = private_gpio_request(pwdn_gpio, "gc08a8_pwdn");
		if (!ret)
		{
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		}
		else
		{
			ISP_ERROR("gpio requrest fail %d\n", pwdn_gpio);
		}
	}
	ret = gc08a8_detect(sd, &ident);
	if (ret)
	{
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc08a8 chip.\n",
				client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("gc08a8 chip found @ 0x%02x (%s)\n sensor drv version %s", client->addr, client->adapter->name, SENSOR_VERSION);
	if (chip)
	{
		memcpy(chip->name, "gc08a8", sizeof("gc08a8"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int gc08a8_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	// struct tx_isp_initarg *init = arg;
	// return 0;
	if (IS_ERR_OR_NULL(sd))
	{
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd)
	{
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = gc08a8_set_expo(sd, sensor_val->value);
		break;
		/*
			case TX_ISP_EVENT_SENSOR_INT_TIME:
				if(arg)
					ret = gc08a8_set_integration_time(sd, sensor_val->value);
				break;
			case TX_ISP_EVENT_SENSOR_AGAIN:
				if(arg)
					ret = gc08a8_set_analog_gain(sd, sensor_val->value);
				break;
		*/
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = gc08a8_set_digital_gain(sd, sensor_val->value);
		break;

	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = gc08a8_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = gc08a8_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = gc08a8_write_array(sd, gc08a8_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = gc08a8_write_array(sd, gc08a8_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = gc08a8_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if (arg)
			ret = gc08a8_set_hvflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int gc08a8_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len))
	{
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = gc08a8_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int gc08a8_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len))
	{
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc08a8_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops gc08a8_core_ops = {
	.g_chip_ident = gc08a8_g_chip_ident,
	.reset = gc08a8_reset,
	.init = gc08a8_init,
	/*.ioctl = gc08a8_ops_ioctl,*/
	.g_register = gc08a8_g_register,
	.s_register = gc08a8_s_register,
};

static struct tx_isp_subdev_video_ops gc08a8_video_ops = {
	.s_stream = gc08a8_s_stream,
};

static struct tx_isp_subdev_sensor_ops gc08a8_sensor_ops = {
	.ioctl = gc08a8_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops gc08a8_ops = {
	.core = &gc08a8_core_ops,
	.video = &gc08a8_video_ops,
	.sensor = &gc08a8_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "gc08a8",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc08a8_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
	{
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));
	sensor->dev = &client->dev;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	gc08a8_attr.expo_fs = 1;
	sensor->video.attr = &gc08a8_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc08a8_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->gc08a8\n");

	return 0;
}

static int gc08a8_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id gc08a8_id[] = {
	{"gc08a8", 0},
	{}};
MODULE_DEVICE_TABLE(i2c, gc08a8_id);

static struct i2c_driver gc08a8_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "gc08a8",
	},
	.probe = gc08a8_probe,
	.remove = gc08a8_remove,
	.id_table = gc08a8_id,
};

static __init int init_gc08a8(void)
{
	return private_i2c_add_driver(&gc08a8_driver);
}

static __exit void exit_gc08a8(void)
{
	private_i2c_del_driver(&gc08a8_driver);
}

module_init(init_gc08a8);
module_exit(exit_gc08a8);

MODULE_DESCRIPTION("A low-level driver for gc08a8 sensor");
MODULE_LICENSE("GPL");
