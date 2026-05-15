/*
* s5k3p3.c
*
* Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* Settings:
* sboot        resolution      fps       interface              mode
*   0          2320x1744       30        MIPI 2lane            linear
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

#define TVERSION "V20230103a"
#define SENSOR_VERSION  "H20240926a"

/* #define SENSOR_TEST */

/* 选择Sensor寄存器地址位宽(8bit/16bit) */
// #define SENSOR_I2C_REG_8BIT

/* 选择Sensor AGain匹配方式(AGain表/非AGain表) */
#define SENSOR_AGAIN_TABLE

#define SENSOR_EXPO

/* 镜像翻转功能开关 */
#define SENSOR_MIR_FLIP

static int rst_gpio = GPIO_PA(18);
module_param(rst_gpio, int, S_IRUGO);
MODULE_PARM_DESC(rst_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int default_boot = 0;
module_param(default_boot, int, S_IRUGO);
MODULE_PARM_DESC(default_boot, "Sensor default boot");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

#define SENSOR_CHIP_ID_H    (0x31)
#define SENSOR_CHIP_ID_L    (0x03)
#define s5k3p3_REG_END	    0xffff
#define s5k3p3_REG_DELAY	0xfffe
#define SENSOR_OUTPUT_MIN_FPS   5
#define SENSOR_MCLK 24000000

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
	uint16_t value;
};

struct tx_isp_sensor_attribute s5k3p3_attr;

/*
* The part of driver maybe modify about different sensor and different board.
*/
#ifdef SENSOR_AGAIN_TABLE
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut s5k3p3_again_lut[] = {
	{0x20,0},
	{0x21,2909},
	{0x22,5732},
	{0x23,8473},
	{0x24,11136},
	{0x25,13727},
	{0x26,16248},
	{0x27,18704},
	{0x28,21098},
	{0x29,23433},
	{0x2A,25711},
	{0x2B,27936},
	{0x2C,30109},
	{0x2D,32234},
	{0x2E,34312},
	{0x2F,36346},
	{0x30,38336},
	{0x31,40286},
	{0x32,42196},
	{0x33,44068},
	{0x34,45904},
	{0x35,47705},
	{0x36,49472},
	{0x37,51207},
	{0x38,52911},
	{0x39,54584},
	{0x3A,56229},
	{0x3B,57845},
	{0x3C,59434},
	{0x3D,60997},
	{0x3E,62534},
	{0x3F,64047},
	{0x40,65536},
	{0x41,67002},
	{0x42,68445},
	{0x43,69867},
	{0x44,71268},
	{0x45,72648},
	{0x46,74009},
	{0x47,75350},
	{0x48,76672},
	{0x49,77976},
	{0x4A,79263},
	{0x4B,80532},
	{0x4C,81784},
	{0x4D,83020},
	{0x4E,84240},
	{0x4F,85445},
	{0x50,86634},
	{0x51,87808},
	{0x52,88969},
	{0x53,90115},
	{0x54,91247},
	{0x55,92366},
	{0x56,93472},
	{0x57,94565},
	{0x58,95645},
	{0x59,96714},
	{0x5A,97770},
	{0x5B,98815},
	{0x5C,99848},
	{0x5D,100870},
	{0x5E,101882},
	{0x5F,102882},
	{0x60,103872},
	{0x61,104852},
	{0x62,105822},
	{0x63,106782},
	{0x64,107732},
	{0x65,108673},
	{0x66,109604},
	{0x67,110526},
	{0x68,111440},
	{0x69,112345},
	{0x6A,113241},
	{0x6B,114129},
	{0x6C,115008},
	{0x6D,115880},
	{0x6E,116743},
	{0x6F,117599},
	{0x70,118447},
	{0x71,119287},
	{0x72,120120},
	{0x73,120946},
	{0x74,121765},
	{0x75,122576},
	{0x76,123381},
	{0x77,124179},
	{0x78,124970},
	{0x79,125755},
	{0x7A,126533},
	{0x7B,127305},
	{0x7C,128070},
	{0x7D,128830},
	{0x7E,129583},
	{0x7F,130330},
	{0x80,131072},
	{0x81,131808},
	{0x82,132538},
	{0x83,133262},
	{0x84,133981},
	{0x85,134695},
	{0x86,135403},
	{0x87,136106},
	{0x88,136804},
	{0x89,137497},
	{0x8A,138184},
	{0x8B,138867},
	{0x8C,139545},
	{0x8D,140218},
	{0x8E,140886},
	{0x8F,141549},
	{0x90,142208},
	{0x91,142863},
	{0x92,143512},
	{0x93,144158},
	{0x94,144799},
	{0x95,145435},
	{0x96,146068},
	{0x97,146696},
	{0x98,147320},
	{0x99,147940},
	{0x9A,148556},
	{0x9B,149168},
	{0x9C,149776},
	{0x9D,150380},
	{0x9E,150981},
	{0x9F,151577},
	{0xA0,152170},
	{0xA1,152759},
	{0xA2,153344},
	{0xA3,153926},
	{0xA4,154505},
	{0xA5,155079},
	{0xA6,155651},
	{0xA7,156218},
	{0xA8,156783},
	{0xA9,157344},
	{0xAA,157902},
	{0xAB,158456},
	{0xAC,159008},
	{0xAD,159556},
	{0xAE,160101},
	{0xAF,160643},
	{0xB0,161181},
	{0xB1,161717},
	{0xB2,162250},
	{0xB3,162779},
	{0xB4,163306},
	{0xB5,163830},
	{0xB6,164351},
	{0xB7,164869},
	{0xB8,165384},
	{0xB9,165897},
	{0xBA,166406},
	{0xBB,166913},
	{0xBC,167418},
	{0xBD,167919},
	{0xBE,168418},
	{0xBF,168914},
	{0xC0,169408},
	{0xC1,169899},
	{0xC2,170388},
	{0xC3,170874},
	{0xC4,171358},
	{0xC5,171839},
	{0xC6,172318},
	{0xC7,172794},
	{0xC8,173268},
	{0xC9,173739},
	{0xCA,174209},
	{0xCB,174675},
	{0xCC,175140},
	{0xCD,175602},
	{0xCE,176062},
	{0xCF,176520},
	{0xD0,176976},
	{0xD1,177429},
	{0xD2,177881},
	{0xD3,178330},
	{0xD4,178777},
	{0xD5,179222},
	{0xD6,179665},
	{0xD7,180106},
	{0xD8,180544},
	{0xD9,180981},
	{0xDA,181416},
	{0xDB,181848},
	{0xDC,182279},
	{0xDD,182708},
	{0xDE,183135},
	{0xDF,183560},
	{0xE0,183983},
	{0xE1,184404},
	{0xE2,184823},
	{0xE3,185241},
	{0xE4,185656},
	{0xE5,186070},
	{0xE6,186482},
	{0xE7,186892},
	{0xE8,187301},
	{0xE9,187707},
	{0xEA,188112},
	{0xEB,188515},
	{0xEC,188917},
	{0xED,189317},
	{0xEE,189715},
	{0xEF,190111},
	{0xF0,190506},
	{0xF1,190899},
	{0xF2,191291},
	{0xF3,191681},
	{0xF4,192069},
	{0xF5,192456},
	{0xF6,192841},
	{0xF7,193224},
	{0xF8,193606},
	{0xF9,193987},
	{0xFA,194366},
	{0xFB,194743},
	{0xFC,195119},
	{0xFD,195493},
	{0xFE,195866},
	{0xFF,196238},
	{0x100,196608},
	{0x101,196977},
	{0x102,197344},
	{0x103,197710},
	{0x104,198074},
	{0x105,198437},
	{0x106,198798},
	{0x107,199159},
	{0x108,199517},
	{0x109,199875},
	{0x10A,200231},
	{0x10B,200586},
	{0x10C,200939},
	{0x10D,201291},
	{0x10E,201642},
	{0x10F,201992},
	{0x110,202340},
	{0x111,202687},
	{0x112,203033},
	{0x113,203377},
	{0x114,203720},
	{0x115,204062},
	{0x116,204403},
	{0x117,204742},
	{0x118,205081},
	{0x119,205418},
	{0x11A,205754},
	{0x11B,206088},
	{0x11C,206422},
	{0x11D,206754},
	{0x11E,207085},
	{0x11F,207415},
	{0x120,207744},
	{0x121,208072},
	{0x122,208399},
	{0x123,208724},
	{0x124,209048},
	{0x125,209372},
	{0x126,209694},
	{0x127,210015},
	{0x128,210335},
	{0x129,210654},
	{0x12A,210971},
	{0x12B,211288},
	{0x12C,211604},
	{0x12D,211918},
	{0x12E,212232},
	{0x12F,212545},
	{0x130,212856},
	{0x131,213167},
	{0x132,213476},
	{0x133,213785},
	{0x134,214092},
	{0x135,214399},
	{0x136,214704},
	{0x137,215009},
	{0x138,215312},
	{0x139,215615},
	{0x13A,215916},
	{0x13B,216217},
	{0x13C,216517},
	{0x13D,216815},
	{0x13E,217113},
	{0x13F,217410},
	{0x140,217706},
	{0x141,218001},
	{0x142,218295},
	{0x143,218588},
	{0x144,218880},
	{0x145,219172},
	{0x146,219462},
	{0x147,219752},
	{0x148,220041},
	{0x149,220328},
	{0x14A,220615},
	{0x14B,220901},
	{0x14C,221187},
	{0x14D,221471},
	{0x14E,221754},
	{0x14F,222037},
	{0x150,222319},
	{0x151,222600},
	{0x152,222880},
	{0x153,223159},
	{0x154,223438},
	{0x155,223716},
	{0x156,223992},
	{0x157,224268},
	{0x158,224544},
	{0x159,224818},
	{0x15A,225092},
	{0x15B,225365},
	{0x15C,225637},
	{0x15D,225908},
	{0x15E,226179},
	{0x15F,226448},
	{0x160,226717},
	{0x161,226986},
	{0x162,227253},
	{0x163,227520},
	{0x164,227786},
	{0x165,228051},
	{0x166,228315},
	{0x167,228579},
	{0x168,228842},
	{0x169,229104},
	{0x16A,229366},
	{0x16B,229627},
	{0x16C,229887},
	{0x16D,230146},
	{0x16E,230405},
	{0x16F,230663},
	{0x170,230920},
	{0x171,231177},
	{0x172,231433},
	{0x173,231688},
	{0x174,231942},
	{0x175,232196},
	{0x176,232449},
	{0x177,232702},
	{0x178,232954},
	{0x179,233205},
	{0x17A,233455},
	{0x17B,233705},
	{0x17C,233954},
	{0x17D,234203},
	{0x17E,234450},
	{0x17F,234698},
	{0x180,234944},
	{0x181,235190},
	{0x182,235435},
	{0x183,235680},
	{0x184,235924},
	{0x185,236167},
	{0x186,236410},
	{0x187,236652},
	{0x188,236894},
	{0x189,237135},
	{0x18A,237375},
	{0x18B,237614},
	{0x18C,237854},
	{0x18D,238092},
	{0x18E,238330},
	{0x18F,238567},
	{0x190,238804},
	{0x191,239040},
	{0x192,239275},
	{0x193,239510},
	{0x194,239745},
	{0x195,239978},
	{0x196,240211},
	{0x197,240444},
	{0x198,240676},
	{0x199,240908},
	{0x19A,241138},
	{0x19B,241369},
	{0x19C,241598},
	{0x19D,241828},
	{0x19E,242056},
	{0x19F,242284},
	{0x1A0,242512},
	{0x1A1,242739},
	{0x1A2,242965},
	{0x1A3,243191},
	{0x1A4,243417},
	{0x1A5,243642},
	{0x1A6,243866},
	{0x1A7,244090},
	{0x1A8,244313},
	{0x1A9,244536},
	{0x1AA,244758},
	{0x1AB,244980},
	{0x1AC,245201},
	{0x1AD,245421},
	{0x1AE,245642},
	{0x1AF,245861},
	{0x1B0,246080},
	{0x1B1,246299},
	{0x1B2,246517},
	{0x1B3,246735},
	{0x1B4,246952},
	{0x1B5,247168},
	{0x1B6,247384},
	{0x1B7,247600},
	{0x1B8,247815},
	{0x1B9,248030},
	{0x1BA,248244},
	{0x1BB,248458},
	{0x1BC,248671},
	{0x1BD,248884},
	{0x1BE,249096},
	{0x1BF,249308},
	{0x1C0,249519},
	{0x1C1,249730},
	{0x1C2,249940},
	{0x1C3,250150},
	{0x1C4,250359},
	{0x1C5,250568},
	{0x1C6,250777},
	{0x1C7,250985},
	{0x1C8,251192},
	{0x1C9,251399},
	{0x1CA,251606},
	{0x1CB,251812},
	{0x1CC,252018},
	{0x1CD,252223},
	{0x1CE,252428},
	{0x1CF,252633},
	{0x1D0,252837},
	{0x1D1,253040},
	{0x1D2,253243},
	{0x1D3,253446},
	{0x1D4,253648},
	{0x1D5,253850},
	{0x1D6,254051},
	{0x1D7,254252},
	{0x1D8,254453},
	{0x1D9,254653},
	{0x1DA,254853},
	{0x1DB,255052},
	{0x1DC,255251},
	{0x1DD,255449},
	{0x1DE,255647},
	{0x1DF,255845},
	{0x1E0,256042},
	{0x1E1,256239},
	{0x1E2,256435},
	{0x1E3,256631},
	{0x1E4,256827},
	{0x1E5,257022},
	{0x1E6,257217},
	{0x1E7,257411},
	{0x1E8,257605},
	{0x1E9,257798},
	{0x1EA,257992},
	{0x1EB,258184},
	{0x1EC,258377},
	{0x1ED,258569},
	{0x1EE,258760},
	{0x1EF,258951},
	{0x1F0,259142},
	{0x1F1,259333},
	{0x1F2,259523},
	{0x1F3,259712},
	{0x1F4,259902},
	{0x1F5,260091},
	{0x1F6,260279},
	{0x1F7,260467},
	{0x1F8,260655},
	{0x1F9,260842},
	{0x1FA,261029},
	{0x1FB,261216},
	{0x1FC,261402},
	{0x1FD,261588},
	{0x1FE,261774},
	{0x1FF,261959},
	{0x200,262144},
};
#endif /* SENSOR_AGAIN_TABLE */

unsigned int s5k3p3_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
#ifndef SENSOR_TEST
#ifdef SENSOR_AGAIN_TABLE
		/* Analog gain table */
	struct again_lut *lut = s5k3p3_again_lut;
	while(lut->gain <= s5k3p3_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == s5k3p3_attr.max_again) && (isp_gain >= lut->gain)) {
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

unsigned int s5k3p3_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus s5k3p3_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 720,
	.lans = 2,
	.image_twidth = 2320,
	.image_theight = 1744,
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
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute s5k3p3_attr = {
	.name = "s5k3p3",
	.chip_id = 0x3103,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x10,
	.sensor_ctrl.alloc_again = s5k3p3_alloc_again,
	.sensor_ctrl.alloc_dgain = s5k3p3_alloc_dgain,
};


static struct regval_list s5k3p3_init_regs_2320_1744_30fps_mipi[] = {
	{0x6028,0x4000},
	{0x602A,0x6010},
	{0x6F12,0x0001},
	{0x6028,0x4000},
	{0x602A,0x0114},
	{0x6F12,0x0100},//2lane
	{0x6028,0x4000},
	{0x602A,0x6214},
	{0x6F12,0x7971},
	{0x602A,0x6218},
	{0x6F12,0x0100},
	{0x602A,0xF408},
	{0x6F12,0x0048},
	{0x602A,0xF40C},
	{0x6F12,0x0000},
	{0x602A,0xF4AA},
	{0x6F12,0x0060},
	{0x602A,0xF442},
	{s5k3p3_REG_DELAY,0x05},
	{0x6F12,0x0800},
	{0x602A,0xF43E},
	{0x6F12,0x0400},
	{0x6F12,0x0000},
	{0x602A,0xF4A4},
	{0x6F12,0x0010},
	{0x602A,0xF4AC},
	{0x6F12,0x0056},
	{0x602A,0xF480},
	{0x6F12,0x0008},
	{0x602A,0xF492},
	{0x6F12,0x0016},
	{0x602A,0x3E58},
	{0x6F12,0x0056},
	{0x602A,0x39EE},
	{s5k3p3_REG_DELAY,0x02},
	{0x6F12,0x0206},
	{s5k3p3_REG_DELAY,0x02},
	{0x602A,0x39E8},
	{s5k3p3_REG_DELAY,0x02},
	{0x6F12,0x0205},
	{s5k3p3_REG_DELAY,0x02},
	{0x6028,0x2000},
	{0x602A,0x14B0},
	{0x6F12,0xF412},
	{0x6028,0x4000},
	{0x602A,0x3A36},
	{0x6F12,0x33F0},
	{0x602A,0x32B2},
	{0x6F12,0x0132},
	{0x602A,0x3A38},
	{0x6F12,0x006C},
	{0x602A,0x3552},
	{0x6F12,0x00D0},
	{0x602A,0x3195},
	{s5k3p3_REG_DELAY,0x02},
	{0x6F12,0x0101},
	{s5k3p3_REG_DELAY,0x02},
	{0x6028,0x2000},
	{0x602A,0x13EC},
	{0x6F12,0x8011},
	{0x6F12,0x8011},
	{0x6028,0x4000},
	{0x602A,0x3002},
	{0x6F12,0x0001},
	{s5k3p3_REG_DELAY,0x02},
	{0x602A,0x0136},
	{0x6F12,0x1800},
	{0x602A,0x0304},
	{0x6F12,0x0006},
	{0x6F12,0x008C},
	{0x602A,0x030C},
	{0x6F12,0x0004},
	{0x6F12,0x0078},
	{0x602A,0x3008},
	{0x6F12,0x0001},
	{0x602A,0x0302},
	{0x6F12,0x0001},
	{0x602A,0x0300},
	{0x6F12,0x0008},
	{0x602A,0x030A},
	{0x6F12,0x0001},
	{0x602A,0x0308},
	{0x6F12,0x0008},
	{0x602A,0x0202},
	{0x6F12,0x0100},
	{0x602A,0x0200},
	{0x6F12,0x0200},
	{0x602A,0x021E},
	{0x6F12,0x0100},
	{0x602A,0x021C},
	{0x6F12,0x0200},
	{0x602A,0x0344},
	{0x6F12,0x0000},
	{0x602A,0x0348},
	{0x6F12,0x090F},
	{0x602A,0x0346},
	{0x6F12,0x0000},
	{0x602A,0x034A},
	{0x6F12,0x06D3},
	{0x6F12,0x0910},
	{0x6F12,0x06D0},
	{0x602A,0x0342},
	{0x6F12,0x1400},
	{0x602A,0x0340},
	{0x6F12,0x071E},
	{0x602A,0x39BA},
	{0x6F12,0x0001},
	{0x602A,0x3005},
	{0x6F12,0x0801},
	{0x602A,0x39AB},
	{0x6F12,0x0001},
	{s5k3p3_REG_DELAY,0x02},
	{0x6028,0x2000},
	{0x602A,0x026C},
	{0x6F12,0x41F0},
	{0x6F12,0x0000},
	{0x6028,0x4000},
	{0x602A,0x37D4},
	{0x6F12,0x002D},
	{0x602A,0x37DA},
	{0x6F12,0x005D},
	{0x602A,0x37E0},
	{0x6F12,0x008D},
	{0x602A,0x37E6},
	{0x6F12,0x00BD},
	{0x602A,0x37EC},
	{0x6F12,0x00ED},
	{0x602A,0x37F2},
	{0x6F12,0x011D},
	{0x602A,0x37F8},
	{0x6F12,0x014D},
	{0x602A,0x37FE},
	{0x6F12,0x017D},
	{0x602A,0x3804},
	{0x6F12,0x01AD},
	{0x602A,0x380A},
	{0x6F12,0x01DD},
	{0x602A,0x3810},
	{0x6F12,0x020D},
	{0x602A,0x32A6},
	{0x6F12,0x0006},
	{0x602A,0x32BE},
	{0x6F12,0x0006},
	{0x602A,0x3210},
	{0x6F12,0x0006},
	{0x602A,0x3072},
	{0x6F12,0x03C0},
	{0x602A,0x6214},
	{0x6F12,0x7970},
	{0x602A,0x0100},
	{0x6F12,0x0100},
	{s5k3p3_REG_DELAY,0xC8},
	{0x0202,0x0400},
	{0x0204,0x0080},
	{s5k3p3_REG_END, 0x00},/* END MARKER */	/* END MARKER *//* END MARKER */
};

/*
* the order of the s5k3p3_win_sizes is [full_resolution, preview_resolution].
*/
static struct tx_isp_sensor_win_setting s5k3p3_win_sizes[] = {
	/* 1920*1080 [0] */
	{
		.width          = 2320,
		.height         = 1744,
		.fps            = 30 << 16 | 1,
		.mbus_code		= V4L2_MBUS_FMT_SGRBG10_1X10,
		.colorspace		= V4L2_COLORSPACE_SRGB,
		.regs           = s5k3p3_init_regs_2320_1744_30fps_mipi,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &s5k3p3_win_sizes[0];

/*
* the part of driver was fixed.
*/

static struct regval_list s5k3p3_stream_on_mipi[] = {
		{SENSOR_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list s5k3p3_stream_off_mipi[] = {
		{SENSOR_REG_END, 0x00}, /* END MARKER */
};

#ifdef SENSOR_I2C_REG_8BIT
int s5k3p3_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int s5k3p3_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int s5k3p3_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
		int ret;
		unsigned char val;
		while (vals->reg_num != SENSOR_REG_END) {
				if (vals->reg_num == SENSOR_REG_DELAY) {
						private_msleep(vals->value);
				} else {
						ret = s5k3p3_read(sd, vals->reg_num, &val);
						/* printk("{0x%x, 0x%x}\n", vals->reg_num, val); */
						if (ret < 0)
								return ret;
				}
				vals++;
		}

		return 0;
}
#endif

static int s5k3p3_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
		int ret;
		while (vals->reg_num != SENSOR_REG_END) {
				if (vals->reg_num == SENSOR_REG_DELAY) {
						private_msleep(vals->value);
				} else {
						ret = s5k3p3_write(sd, vals->reg_num, vals->value);
						if (ret < 0)
								return ret;
				}
				vals++;
		}

		return 0;
}
#endif  /* SENSOR_I2C_REG_8BIT */

#ifdef SENSOR_I2C_REG_16BIT

int s5k3p3_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int s5k3p3_write(struct tx_isp_subdev *sd, uint16_t reg,
				uint16_t value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf1[3] = {(reg >> 8) & 0xff, reg & 0xff, (value >> 8) & 0xff };
	struct i2c_msg msg1 = {
		.addr   = client->addr,
		.flags  = 0,
		.len    = 3,
		.buf    = buf1,

	};
	uint8_t buf2[3] = {(reg >> 8) & 0xff, (reg & 0xff)+1,value & 0xff};
	struct i2c_msg msg2 = {
		.addr   = client->addr,
		.flags  = 0,
		.len    = 3,
		.buf    = buf2,

	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg1, 1);
	ret += private_i2c_transfer(client->adapter, &msg2, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 0
static int s5k3p3_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
		int ret;
		unsigned char val;
		while (vals->reg_num != SENSOR_REG_END) {
				if (vals->reg_num == SENSOR_REG_DELAY) {
						private_msleep(vals->value);
				} else {
						ret = s5k3p3_read(sd, vals->reg_num, &val);
						if (ret < 0)
								return ret;
				}
				vals++;
		}
		return 0;
}
#endif

static int s5k3p3_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = s5k3p3_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif  /* SENSOR_I2C_REG_16BIT */

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

static int s5k3p3_attr_set(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;

	sensor->video.fps = wsize->fps;

#ifdef SENSOR_MIR_FLIP
	sensor->video.shvflip = 1;
#endif

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int s5k3p3_setting_select(struct tx_isp_subdev *sd, int deboot)
{
	int ret = ISP_SUCCESS;

	switch (deboot) {
	case 0:
		wsize = &s5k3p3_win_sizes[0];
		s5k3p3_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		s5k3p3_attr.max_dgain = 0;
		s5k3p3_attr.max_again = 262144;
		s5k3p3_attr.min_integration_time = 2;
		s5k3p3_attr.max_integration_time = 1822 - 8;
		s5k3p3_attr.total_width = 5120;
		s5k3p3_attr.total_height = 1822;
		s5k3p3_attr.integration_time_apply_delay = 2;
		s5k3p3_attr.again_apply_delay = 2;
		s5k3p3_attr.dgain_apply_delay = 0;
		s5k3p3_attr.integration_time_limit = s5k3p3_attr.max_integration_time;
		s5k3p3_attr.max_integration_time_native = s5k3p3_attr.max_integration_time;
		s5k3p3_attr.min_integration_time_native = s5k3p3_attr.min_integration_time;
		s5k3p3_attr.expo_fs = 1;
		memcpy((void *)(&(s5k3p3_attr.mipi)), (void *)(&s5k3p3_mipi_linear), sizeof(s5k3p3_attr.mipi));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
	}

	return ret;
}

static int s5k3p3_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	s5k3p3_setting_select(sd, default_boot);

	switch (data_interface) {
	case TX_SENSOR_DATA_INTERFACE_MIPI:
	case TX_SENSOR_DATA_INTERFACE_DVP:
		s5k3p3_attr.dbus_type = data_interface;
		break;
	default:
		ISP_ERROR("Have no this interface!!!\n");
	}

#ifdef CONFIG_KERNEL_4_4_94
	sensor->mclk = clk_get(NULL, "div_cim");
#else
	sensor->mclk = clk_get(NULL, "cgu_cim");
#endif
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	sensor_mclk_config(sensor, SENSOR_MCLK);
		if (ret) {
			ISP_ERROR("MCLK configuration failed!!!\n");
		}

	s5k3p3_attr_set(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int s5k3p3_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = s5k3p3_write(sd, 0x6028, 0x4000);


	ret += s5k3p3_read(sd, 0x0000, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = s5k3p3_read(sd, 0x0001, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 24) | v;

	return 0;
}

static int s5k3p3_g_chip_ident(struct tx_isp_subdev *sd,
								struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if (rst_gpio != -1) {
			ret = private_gpio_request(rst_gpio, "s5k3p3_reset");
			if (!ret) {
				private_gpio_direction_output(rst_gpio, 1);
				private_msleep(20);
				private_gpio_direction_output(rst_gpio, 0);
				private_msleep(20);
				private_gpio_direction_output(rst_gpio, 1);
				private_msleep(10);
			} else {
				ISP_ERROR("gpio requrest fail %d\n", rst_gpio);
			}
	}
	if (pwdn_gpio != -1) {
			ret = private_gpio_request(pwdn_gpio, "s5k3p3_pwdn");
			if (!ret) {
				private_gpio_direction_output(pwdn_gpio, 1);
				private_msleep(10);
				private_gpio_direction_output(pwdn_gpio, 0);
				private_msleep(10);
			} else {
				ISP_ERROR("gpio requrest fail %d\n", pwdn_gpio);
			}
	}
	ret = s5k3p3_detect(sd, &ident);
	if (ret) {
			ISP_ERROR("chip found @ 0x%x (%s) is not an s5k3p3 chip.\n",
				client->addr, client->adapter->name);
			return ret;
	}

	ISP_WARNING("===================================================\n");
	ISP_WARNING("Template version is %s\n", TVERSION);
	ISP_WARNING("Sensor driver version is %s\n", SENSOR_VERSION);
	ISP_WARNING("Sensor chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("Sensor video interface is %d\n", data_interface);
	ISP_WARNING("Sensor default boot is [%d-->%dx%d@(%d/%d)fps]\n",
				default_boot, wsize->width, wsize->height, wsize->fps >> 16, wsize->fps&0xffff);
	ISP_WARNING("===================================================\n");

	if (chip) {
		memcpy(chip->name, "s5k3p3", sizeof("s5k3p3"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int s5k3p3_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int s5k3p3_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (!enable) {
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		return ISP_SUCCESS;
	} else {
		ret = s5k3p3_attr_set(sd, wsize);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int s5k3p3_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = s5k3p3_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int s5k3p3_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	s5k3p3_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static int s5k3p3_s_stream(struct tx_isp_subdev *sd, int enable)
{
		struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
		int ret = ISP_SUCCESS;

		if (enable) {
				if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
						ret = s5k3p3_write_array(sd, wsize->regs);
						if (ret)
								return ret;
						sensor->video.state = TX_ISP_MODULE_INIT;
				}
				if (sensor->video.state == TX_ISP_MODULE_INIT) {
						ret = s5k3p3_write_array(sd, s5k3p3_stream_on_mipi);
						sensor->video.state = TX_ISP_MODULE_RUNNING;
						pr_debug("s5k3p3 stream on\n");
				}

		} else {
				ret = s5k3p3_write_array(sd, s5k3p3_stream_off_mipi);
				sensor->video.state = TX_ISP_MODULE_INIT;
				pr_debug("s5k3p3 stream off\n");
		}

		return ret;
}

#ifndef SENSOR_TEST
#ifdef SENSOR_EXPO
static int s5k3p3_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;
	ret = s5k3p3_write(sd, 0x6028, 0x4000);

	//it
	ret = s5k3p3_write(sd, 0x602a, 0x0202);
	ret = s5k3p3_write(sd, 0x6f12, it);

	//again
	ret = s5k3p3_write(sd, 0x602a, 0x0204);
	ret = s5k3p3_write(sd, 0x6f12, again);

	return ret;
}
#else
static int s5k3p3_set_integration_time(struct tx_isp_subdev *sd, int value)
{
		int ret = ISP_SUCCESS;

//        value &= 0xffff;
//        s5k3p3_write(sd, 0xfd, 0x01);
//        s5k3p3_write(sd, 0x0e, value >> 8);
//        s5k3p3_write(sd, 0x0f, value & 0xff);
//        s5k3p3_write(sd, 0xfe, 0x02);

		return ret;
}

static int s5k3p3_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
		int ret = ISP_SUCCESS;

//        s5k3p3_write(sd, 0xfd, 0x01);
//        s5k3p3_write(sd, 0x24, value & 0xff);
//        s5k3p3_write(sd, 0xfe, 0x02);

		return ret;
}
#endif /* SENSOR_EXPO */

static int s5k3p3_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int s5k3p3_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int s5k3p3_set_mode(struct tx_isp_subdev *sd, int value)
{
	int ret = ISP_SUCCESS;

	if (wsize) {
			ret = s5k3p3_attr_set(sd, wsize);
	}

	return ret;
}

static int s5k3p3_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0;
	int ret = ISP_SUCCESS;

	switch (default_boot) {
	case 0:
		sclk = 5120 * 1822 * 30;  /**< HTS * VTS * FPS */
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

	s5k3p3_write(sd, 0x6028, 0x4000);
	val = 0;
	ret += s5k3p3_read(sd, 0x0342, &val);
	hts = val << 8;
	val = 0;
	ret += s5k3p3_read(sd, 0x0343, &val);
	hts = (hts | val);
	if (0 != ret) {
		ISP_ERROR("err: s5k3p3 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	s5k3p3_write(sd, 0x602A, 0x0340);
	s5k3p3_write(sd, 0x6F12, vts);

	if (0 != ret) {
		ISP_ERROR("err: s5k3p3_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 9;
	sensor->video.attr->integration_time_limit = vts - 9;
	sensor->video.attr->max_integration_time_native = vts - 9;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	if (ret) {
			ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
	}

	return ret;
}

#ifdef SENSOR_MIR_FLIP
static int s5k3p3_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;
	uint16_t val = 0;


	/* 2'b01:filp,2'b10:mirror */
	switch(enable) {
	case 0:
		val = 0x0000;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SGRBG10_1X10;
		break;
	case 1:/*mirror*/
		val = 0x0100;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SRGGB10_1X10;
		break;
	case 2:/*filp*/
		val = 0x0200;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SBGGR10_1X10;
		break;
	case 3:
		val = 0x0300;
		sensor->video.mbus.code = V4L2_MBUS_FMT_SGBRG10_1X10;
		break;
	}

	s5k3p3_write(sd, 0x6028, 0x4000);
	s5k3p3_write(sd, 0x602A, 0x0101);
	s5k3p3_write(sd, 0x6F12, val);

	sensor->video.mbus_change = 1;
	tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}
#endif /* SENSOR_MIR_FLIP */

static int s5k3p3_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
		long ret = 0;

		if (IS_ERR_OR_NULL(sd)) {
				ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
				return -EINVAL;
		}

		switch (cmd) {
#ifdef SENSOR_EXPO
		case TX_ISP_EVENT_SENSOR_EXPO:
			if (arg)
				ret = s5k3p3_set_expo(sd, *(int*)arg);
			break;
#else
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			if (arg)
					ret = s5k3p3_set_integration_time(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			if (arg)
					ret = s5k3p3_set_analog_gain(sd, *(int*)arg);
			break;
#endif /* SENSOR_EXPO */
		case TX_ISP_EVENT_SENSOR_DGAIN:
			if (arg)
				ret = s5k3p3_set_digital_gain(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
			if (arg)
				ret = s5k3p3_get_black_pedestal(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if (arg)
				ret = s5k3p3_set_mode(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
			if (arg)
				ret = s5k3p3_write_array(sd, s5k3p3_stream_off_mipi);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			if (arg)
				ret = s5k3p3_write_array(sd, s5k3p3_stream_on_mipi);
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if (arg)
				ret = s5k3p3_set_fps(sd, *(int*)arg);
			break;
#ifdef SENSOR_MIR_FLIP
		case TX_ISP_EVENT_SENSOR_VFLIP:
			if(arg)
				ret = s5k3p3_set_vflip(sd, *(int*)arg);
			break;
#endif /* SENSOR_MIR_FLIP */
		default:
			break;
		}

		return 0;
}
#endif /* SENSOR_TEST */

static struct tx_isp_subdev_core_ops s5k3p3_core_ops = {
	.g_chip_ident = s5k3p3_g_chip_ident,
	.reset = s5k3p3_reset,
	.init = s5k3p3_init,
	.g_register = s5k3p3_g_register,
	.s_register = s5k3p3_s_register,
};

static struct tx_isp_subdev_video_ops s5k3p3_video_ops = {
	.s_stream = s5k3p3_s_stream,
};

static struct tx_isp_subdev_sensor_ops s5k3p3_sensor_ops = {
#ifndef SENSOR_TEST
	.ioctl  = s5k3p3_sensor_ops_ioctl,
#endif /* SENSOR_TEST */
};

static struct tx_isp_subdev_ops s5k3p3_ops = {
	.core = &s5k3p3_core_ops,
	.video = &s5k3p3_video_ops,
	.sensor = &s5k3p3_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
struct platform_device sensor_platform_device = {
	.name = "s5k3p3",
	.id = -1,
	.dev = {
			.dma_mask = &tx_isp_module_dma_mask,
			.coherent_dma_mask = 0xffffffff,
			.platform_data = NULL,
	},
	.num_resources = 0,
};

static int s5k3p3_probe(struct i2c_client *client,
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

	s5k3p3_attr_check(sd);

	sensor->video.attr = &s5k3p3_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &s5k3p3_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->s5k3p3\n");

	return 0;
}

static int s5k3p3_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(rst_gpio != -1)
		private_gpio_free(rst_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	tx_isp_subdev_deinit(sd);

	kfree(sensor);

	return 0;
}

static const struct i2c_device_id s5k3p3_id[] = {
		{"s5k3p3", 0},
		{}
};
MODULE_DEVICE_TABLE(i2c, s5k3p3_id
				);

static struct i2c_driver s5k3p3_driver = {
	.driver = {
		.owner  = THIS_MODULE,
		.name   = "s5k3p3",
	},
	.probe          = s5k3p3_probe,
	.remove         = s5k3p3_remove,
	.id_table       = s5k3p3_id,
};

static __init int init_s5k3p3(void) {
	return private_i2c_add_driver(&s5k3p3_driver);
}

static __exit void exit_s5k3p3(void) {
	private_i2c_del_driver(&s5k3p3_driver);
}

module_init(init_s5k3p3);
module_exit(exit_s5k3p3);

MODULE_DESCRIPTION("A low-level driver for OV s5k3p3 sensor");
MODULE_LICENSE("GPL");
