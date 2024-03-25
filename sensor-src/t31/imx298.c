// SPDX-License-Identifier: GPL-2.0+
/*
 * imx298.c
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
#include <sensor-info.h>

#define SENSOR_NAME "imx298"
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x1a
#define SENSOR_MAX_WIDTH 2328
#define SENSOR_MAX_HEIGHT 1444
#define SENSOR_CHIP_ID 0x0000
#define SENSOR_CHIP_ID_H (0x00)
#define SENSOR_CHIP_ID_L (0x00)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_PCLK (1802 * 5536 * 30)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 10
#define SENSOR_VERSION "H20211103a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwr_gpio = GPIO_PB(03);
module_param(pwr_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwr_gpio, "PWR GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
module_param(data_type, int, S_IRUGO);
MODULE_PARM_DESC(data_type, "Sensor Date Type");

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static struct sensor_info sensor_info = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.version = SENSOR_VERSION,
	.min_fps = SENSOR_OUTPUT_MIN_FPS,
	.max_fps = SENSOR_OUTPUT_MAX_FPS,
	.chip_i2c_addr = SENSOR_I2C_ADDRESS,
	.width = SENSOR_MAX_WIDTH,
	.height = SENSOR_MAX_HEIGHT,
};

struct again_lut
{
	unsigned int value;
	unsigned int gain;
};

struct regval_list
{
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut sensor_again_lut[] = {
	{0x0002, 0},
	{0x0007, 940},
	{0x000d, 1872},
	{0x0011, 2794},
	{0x0016, 3708},
	{0x001a, 4613},
	{0x001f, 5509},
	{0x0023, 6397},
	{0x0028, 7276},
	{0x002c, 8147},
	{0x0030, 9011},
	{0x0034, 9867},
	{0x0038, 10715},
	{0x003c, 11555},
	{0x0040, 12388},
	{0x0044, 13214},
	{0x0048, 14032},
	{0x004c, 14844},
	{0x004f, 15649},
	{0x0053, 16447},
	{0x0057, 17238},
	{0x005a, 18022},
	{0x005e, 18801},
	{0x0061, 19572},
	{0x0064, 20338},
	{0x0068, 21097},
	{0x006b, 21851},
	{0x006e, 22598},
	{0x0071, 23340},
	{0x0074, 24076},
	{0x0077, 24806},
	{0x007a, 25530},
	{0x007d, 26249},
	{0x0080, 26963},
	{0x0083, 27671},
	{0x0086, 28374},
	{0x0088, 29072},
	{0x008b, 29764},
	{0x008e, 30452},
	{0x0090, 31135},
	{0x0093, 31812},
	{0x0096, 32485},
	{0x0098, 33154},
	{0x009b, 33817},
	{0x009d, 34476},
	{0x00a0, 35130},
	{0x00a2, 35780},
	{0x00a4, 36425},
	{0x00a7, 37066},
	{0x00a9, 37703},
	{0x00ab, 38336},
	{0x00ae, 38964},
	{0x00b2, 39588},
	{0x00b4, 39588},
	{0x00b6, 40208},
	{0x00b8, 40824},
	{0x00ba, 41436},
	{0x00bc, 42044},
	{0x00be, 42648},
	{0x00c0, 43248},
	{0x00c2, 43845},
	{0x00c4, 44438},
	{0x00c6, 45027},
	{0x00c8, 45612},
	{0x00ca, 46194},
	{0x00cc, 46772},
	{0x00ce, 47347},
	{0x00d0, 47918},
	{0x00d2, 48486},
	{0x00d3, 49051},
	{0x00d5, 49612},
	{0x00d6, 50170},
	{0x00d8, 50724},
	{0x00da, 51275},
	{0x00dc, 51824},
	{0x00dd, 52368},
	{0x00df, 52910},
	{0x00e1, 53449},
	{0x00e3, 53985},
	{0x00e4, 54517},
	{0x00e6, 55047},
	{0x00e8, 55574},
	{0x00ea, 56098},
	{0x00eb, 56619},
	{0x00ed, 57137},
	{0x00ee, 57652},
	{0x00ef, 58164},
	{0x00f1, 58674},
	{0x00f3, 59181},
	{0x00e0, 59685},
	{0x00f6, 60187},
	{0x00f7, 60686},
	{0x00f8, 61182},
	{0x00fa, 61676},
	{0x00fb, 62167},
	{0x00fc, 62656},
	{0x00fe, 63142},
	{0x00ff, 63625},
	{0x0100, 64107},
	{0x0101, 64585},
	{0x0103, 65062},
	{0x0104, 65536},
	{0x0105, 66007},
	{0x0106, 66476},
	{0x0108, 66943},
	{0x0109, 67408},
	{0x010a, 67870},
	{0x010b, 68330},
	{0x010c, 68788},
	{0x010d, 69244},
	{0x010f, 69697},
	{0x0110, 70149},
	{0x0111, 70598},
	{0x0112, 71045},
	{0x0113, 71490},
	{0x0114, 71933},
	{0x0115, 72373},
	{0x0116, 72812},
	{0x0117, 73249},
	{0x0118, 73683},
	{0x0119, 74116},
	{0x011a, 74547},
	{0x011b, 74976},
	{0x011c, 75403},
	{0x011d, 75828},
	{0x011e, 76251},
	{0x011f, 76672},
	{0x0120, 77091},
	{0x0121, 77508},
	{0x0122, 77924},
	{0x0123, 78338},
	{0x0124, 78750},
	{0x0125, 79160},
	{0x0126, 79568},
	{0x0127, 79975},
	{0x0128, 80380},
	{0x0129, 80783},
	{0x012a, 81185},
	{0x012b, 81584},
	{0x012c, 81983},
	{0x012d, 82379},
	{0x012e, 82774},
	{0x012f, 83558},
	{0x0130, 83948},
	{0x0131, 84337},
	{0x0132, 84723},
	{0x0133, 85108},
	{0x0134, 85492},
	{0x0135, 86254},
	{0x0136, 86633},
	{0x0137, 87011},
	{0x0138, 87387},
	{0x0139, 87761},
	{0x013a, 88506},
	{0x013b, 88876},
	{0x013c, 89244},
	{0x013d, 89977},
	{0x013e, 90342},
	{0x013f, 90705},
	{0x0140, 91426},
	{0x0141, 91785},
	{0x0142, 92143},
	{0x0143, 92854},
	{0x0144, 93207},
	{0x0145, 93559},
	{0x0146, 94259},
	{0x0147, 94608},
	{0x0148, 95300},
	{0x0149, 95645},
	{0x014a, 96330},
	{0x014b, 96671},
	{0x014c, 97348},
	{0x014d, 97686},
	{0x014e, 98356},
	{0x014f, 98690},
	{0x0150, 99353},
	{0x0151, 100012},
	{0x0152, 100340},
	{0x0153, 100992},
	{0x0154, 101639},
	{0x0155, 101961},
	{0x0156, 102602},
	{0x0157, 103239},
	{0x0158, 103556},
	{0x0159, 104186},
	{0x015a, 104812},
	{0x015b, 105434},
	{0x015c, 106052},
	{0x015d, 106360},
	{0x015e, 106972},
	{0x015f, 107580},
	{0x0160, 108184},
	{0x0161, 108784},
	{0x0162, 109381},
	{0x0163, 109974},
	{0x0164, 110563},
	{0x0165, 111148},
	{0x0166, 111730},
	{0x0167, 112308},
	{0x0168, 112883},
	{0x0169, 113454},
	{0x016a, 114305},
	{0x016b, 114868},
	{0x016c, 115427},
	{0x016d, 115983},
	{0x016e, 116811},
	{0x016f, 117360},
	{0x0170, 117904},
	{0x0171, 118716},
	{0x0172, 119253},
	{0x0173, 120053},
	{0x0174, 120583},
	{0x0175, 121372},
	{0x0176, 121894},
	{0x0177, 122673},
	{0x0178, 123188},
	{0x0179, 125973},
	{0x017a, 126718},
	{0x017b, 127458},
	{0x017c, 128192},
	{0x017d, 128920},
	{0x017e, 129643},
	{0x017f, 130360},
	{0x0180, 131072},
	{0x0181, 131778},
	{0x0182, 132479},
	{0x0183, 133406},
	{0x0184, 134095},
	{0x0185, 134780},
	{0x0186, 135685},
	{0x0187, 136357},
	{0x0188, 137247},
	{0x0189, 137909},
	{0x018a, 138785},
	{0x018b, 139652},
	{0x018c, 140298},
	{0x018d, 141151},
	{0x018e, 141997},
	{0x018f, 142836},
	{0x0190, 143667},
	{0x0191, 144491},
	{0x0192, 145308},
	{0x0193, 146319},
	{0x0194, 147120},
	{0x0195, 148113},
	{0x0196, 148899},
	{0x0197, 149873},
	{0x0198, 150644},
	{0x0199, 151600},
	{0x019a, 152547},
	{0x019b, 153484},
	{0x019c, 154412},
	{0x019d, 155331},
	{0x019e, 156241},
	{0x019f, 157321},
	{0x01a0, 158212},
	{0x01a1, 159271},
	{0x01a2, 160317},
	{0x01a3, 161353},
	{0x01a4, 162377},
	{0x01a5, 163390},
	{0x01a6, 164392},
	{0x01a7, 165384},
	{0x01a8, 166528},
	{0x01a9, 167658},
	{0x01aa, 168616},
	{0x01ab, 169722},
	{0x01ac, 170970},
	{0x01ad, 172049},
	{0x01ae, 173116},
	{0x01af, 174320},
	{0x01b0, 175510},
	{0x01b1, 176684},
	{0x01b2, 177844},
	{0x01b3, 179133},
	{0x01b4, 180404},
	{0x01b5, 181658},
	{0x01b6, 182896},
	{0x01b7, 184117},
	{0x01b8, 185457},
	{0x01b9, 186777},
	{0x01ba, 188079},
	{0x01bb, 189492},
	{0x01bc, 190883},
	{0x01bd, 192254},
	{0x01be, 193728},
	{0x01bf, 195179},
	{0x01c0, 196608},
};
struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again)
	{
		if (isp_gain == 0)
		{
			*sensor_again = lut[0].value;
			return 0;
		}
		else if (isp_gain < lut->gain)
		{
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else
		{
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain))
			{
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
	*sensor_dgain = 0;

	return 0;
}

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = 0x0000,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_SONY_MODE,
		.clk = 749,
		.lans = 2,
		.settle_time_apative_en = 0,
		.image_twidth = 2328,
		.image_theight = 1444,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.mipi_crop_start0x = 8,
		.mipi_sc.mipi_crop_start0y = 4,
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
	},
	.max_again = 196608,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 1802 - 10,
	.integration_time_limit = 1802 - 10,
	.total_width = 5536,
	.total_height = 1802,
	.max_integration_time = 1802 - 10,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_2320_1440_30fps_mipi[] = {
	{0x0136, 0x18},
	{0x0137, 0x00},
	{0x30F4, 0x01},
	{0x30F5, 0x7A},
	{0x30F6, 0x00},
	{0x30F7, 0xEC},
	{0x30FC, 0x01},
	{0x3101, 0x01},
	{0x5B2F, 0x08},
	{0x5D32, 0x05},
	{0x5D7C, 0x00},
	{0x5D7D, 0x00},
	{0x5DB9, 0x01},
	{0x5E43, 0x00},
	{0x6300, 0x00},
	{0x6301, 0xEA},
	{0x6302, 0x00},
	{0x6303, 0xB4},
	{0x6564, 0x00},
	{0x6565, 0xB6},
	{0x6566, 0x00},
	{0x6567, 0xE6},
	{0x6714, 0x01},
	{0x6910, 0x04},
	{0x6916, 0x01},
	{0x6918, 0x04},
	{0x691E, 0x01},
	{0x6931, 0x01},
	{0x6937, 0x02},
	{0x693B, 0x02},
	{0x6D00, 0x4A},
	{0x6D01, 0x41},
	{0x6D02, 0x23},
	{0x6D05, 0x4C},
	{0x6D06, 0x10},
	{0x6D08, 0x30},
	{0x6D09, 0x38},
	{0x6D0A, 0x2C},
	{0x6D0B, 0x2D},
	{0x6D0C, 0x34},
	{0x6D0D, 0x42},
	{0x6D19, 0x1C},
	{0x6D1A, 0x71},
	{0x6D1B, 0xC6},
	{0x6D1C, 0x94},
	{0x6D24, 0xE4},
	{0x6D30, 0x0A},
	{0x6D31, 0x01},
	{0x6D33, 0x0B},
	{0x6D34, 0x05},
	{0x6D35, 0x00},
	{0x83C2, 0x03},
	{0x83c3, 0x08},
	{0x83C4, 0x48},
	{0x83C7, 0x08},
	{0x83CB, 0x00},
	{0xB101, 0xFF},
	{0xB103, 0xFF},
	{0xB105, 0xFF},
	{0xB107, 0xFF},
	{0xB109, 0xFF},
	{0xB10B, 0xFF},
	{0xB10D, 0xFF},
	{0xB10F, 0xFF},
	{0xB111, 0xFF},
	{0xB163, 0x3C},
	{0xC2A0, 0x08},
	{0xC2A3, 0x03},
	{0xC2A5, 0x08},
	{0xC2A6, 0x48},
	{0xC2A9, 0x00},
	{0xF800, 0x5E},
	{0xF801, 0x5E},
	{0xF802, 0xCD},
	{0xF803, 0x20},
	{0xF804, 0x55},
	{0xF805, 0xD4},
	{0xF806, 0x1F},
	{0xF808, 0xF8},
	{0xF809, 0x3A},
	{0xF80A, 0xF1},
	{0xF80B, 0x7E},
	{0xF80C, 0x55},
	{0xF80D, 0x38},
	{0xF80E, 0xE3},
	{0xF810, 0x74},
	{0xF811, 0x41},
	{0xF812, 0xBF},
	{0xF844, 0x40},
	{0xF845, 0xBA},
	{0xF846, 0x70},
	{0xF847, 0x47},
	{0xF848, 0xC0},
	{0xF849, 0xBA},
	{0xF84A, 0x70},
	{0xF84B, 0x47},
	{0xF84C, 0x82},
	{0xF84D, 0xF6},
	{0xF84E, 0x32},
	{0xF84F, 0xFD},
	{0xF851, 0xF0},
	{0xF852, 0x02},
	{0xF853, 0xF8},
	{0xF854, 0x81},
	{0xF855, 0xF6},
	{0xF856, 0xC0},
	{0xF857, 0xFF},
	{0xF858, 0x10},
	{0xF859, 0xB5},
	{0xF85A, 0x0D},
	{0xF85B, 0x48},
	{0xF85C, 0x40},
	{0xF85D, 0x7A},
	{0xF85E, 0x01},
	{0xF85F, 0x28},
	{0xF860, 0x15},
	{0xF861, 0xD1},
	{0xF862, 0x0C},
	{0xF863, 0x49},
	{0xF864, 0x0C},
	{0xF865, 0x46},
	{0xF866, 0x40},
	{0xF867, 0x3C},
	{0xF868, 0x48},
	{0xF869, 0x8A},
	{0xF86A, 0x62},
	{0xF86B, 0x8A},
	{0xF86C, 0x80},
	{0xF86D, 0x1A},
	{0xF86E, 0x8A},
	{0xF86F, 0x89},
	{0xF871, 0xB2},
	{0xF872, 0x10},
	{0xF873, 0x18},
	{0xF874, 0x0A},
	{0xF875, 0x46},
	{0xF876, 0x20},
	{0xF877, 0x32},
	{0xF878, 0x12},
	{0xF879, 0x88},
	{0xF87A, 0x90},
	{0xF87B, 0x42},
	{0xF87D, 0xDA},
	{0xF87E, 0x10},
	{0xF87F, 0x46},
	{0xF880, 0x80},
	{0xF881, 0xB2},
	{0xF882, 0x88},
	{0xF883, 0x81},
	{0xF884, 0x84},
	{0xF885, 0xF6},
	{0xF886, 0xD2},
	{0xF887, 0xF9},
	{0xF888, 0xE0},
	{0xF889, 0x67},
	{0xF88A, 0x85},
	{0xF88B, 0xF6},
	{0xF88C, 0xA1},
	{0xF88D, 0xFC},
	{0xF88E, 0x10},
	{0xF88F, 0xBD},
	{0xF891, 0x18},
	{0xF892, 0x21},
	{0xF893, 0x24},
	{0xF895, 0x18},
	{0xF896, 0x19},
	{0xF897, 0xB4},
	{0x4E29, 0x01},
	{0x0100, 0x00},
	{0x0114, 0x01},
	{0x0220, 0x00},
	{0x0221, 0x11},
	{0x0222, 0x10},
	{0x0340, 0x07},
	{0x0341, 0x0A},
	{0x0342, 0x15},
	{0x0343, 0xA0},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x12},
	{0x0349, 0x2F},
	{0x034A, 0x0D},
	{0x034B, 0xA7},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0900, 0x01},
	{0x0901, 0x22},
	{0x0902, 0x02},
	{0x3010, 0x65},
	{0x3011, 0x01},
	{0x30C0, 0x11},
	{0x300D, 0x00},
	{0x30FD, 0x00},
	{0x8493, 0x00},
	{0x8863, 0x00},
	{0x90D7, 0x19},
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x034C, 0x09},
	{0x034D, 0x18},
	{0x034E, 0x05}, // 0x06
	{0x034F, 0xA4}, // 0xD4
	{0x0401, 0x00},
	{0x0404, 0x00},
	{0x0405, 0x10},
	{0x0408, 0x00},
	{0x0409, 0x00},
	{0x040A, 0x00},
	{0x040B, 0x00},
	{0x040C, 0x09},
	{0x040D, 0x18},
	{0x040E, 0x05}, // 0x06
	{0x040F, 0xA4}, // 0xD4
	{0x0301, 0x05},
	{0x0303, 0x02},
	{0x0305, 0x04},
	{0x0306, 0x00},
	{0x0307, 0x6F},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030D, 0x0F},
	{0x030E, 0x03},
	{0x030F, 0x41},
	{0x0310, 0x00},
	{0x0820, 0x17},
	{0x0821, 0x6A},
	{0x0822, 0x00},
	{0x0823, 0x00},
	{0x0202, 0x07},
	{0x0203, 0x00},
	{0x0224, 0x01},
	{0x0225, 0xF4},
	{0x0204, 0x00},
	{0x0205, 0x00},
	{0x0216, 0x00},
	{0x0217, 0x00},
	{0x020E, 0x01},
	{0x020F, 0x00},
	{0x0210, 0x01},
	{0x0211, 0x00},
	{0x0212, 0x01},
	{0x0213, 0x00},
	{0x0214, 0x01},
	{0x0215, 0x00},
	{0x3058, 0x00},
	{0x3103, 0x01},
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1920*1080 */
	{
		.width = 2320,
		.height = 1440,
		.fps = 30 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2320_1440_30fps_mipi,
	}};

static struct regval_list sensor_stream_on[] = {
	{0x3000, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	{0x3000, 0x01},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,
				unsigned char *value)
{
	int ret;
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
/*
static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END)
	{
		if (vals->reg_num == SENSOR_REG_DELAY)
		{
			msleep(vals->value);
		}
		else
		{
			ret = sensor_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
			ISP_WARNING("{0x%0x, 0x%02x}\n", vals->reg_num, val);
		}
		vals++;
	}

	return 0;
}
*/
static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END)
	{
		if (vals->reg_num == SENSOR_REG_DELAY)
		{
			msleep(vals->value);
		}
		else
		{
			ret = sensor_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sensor_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sensor_read(sd, 0x3008, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x301e, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}
/**
static int sensor_set_integration_time_short(struct tx_isp_subdev *sd, int int_time)
{
	return 0;
}
*/
static int sensor_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;
	ret += sensor_write(sd, 0x0202, (unsigned char)((it >> 8) & 0xff));
	ret += sensor_write(sd, 0x0203, (unsigned char)(it & 0xff));

	ret = sensor_write(sd, 0x0205, (unsigned char)(again & 0xff));
	ret += sensor_write(sd, 0x0204, (unsigned char)(((again >> 8) & 0xff)));
	return 0;
}
/*
static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret = sensor_write(sd, 0x3014, (unsigned char)(value & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	return 0;
}
*/
static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
	int ret = 0;
	if (!enable)
		return ISP_SUCCESS;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = sensor_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable)
	{
		ret = sensor_write_array(sd, sensor_stream_on);
		pr_debug("%s stream on\n", SENSOR_NAME);
	}
	else
	{
		ret = sensor_write_array(sd, sensor_stream_off);
		pr_debug("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;

	unsigned int newformat = 0; // the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8))
	{
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}
	sclk = 5536 * 1802 * 30;

	ret = sensor_read(sd, 0x0342, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0x0343, &tmp);
	hts = ((hts << 8) + tmp);

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += sensor_write(sd, 0x0341, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x0340, (unsigned char)(vts >> 8));

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 10;
	sensor->video.attr->integration_time_limit = vts - 10;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 10;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	if (ret < 0)
		return -1;

	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;

	if (value == TX_ISP_SENSOR_FULL_RES_MAX_FPS)
	{
		wsize = &sensor_win_sizes[0];
	}
	else if (value == TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS)
	{
		wsize = &sensor_win_sizes[0];
	}

	if (wsize)
	{
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

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
							   struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if (reset_gpio != -1)
	{
		ret = private_gpio_request(reset_gpio, "sensor_reset");
		if (!ret)
		{
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(50);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(50);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(50);
		}
		else
		{
			ISP_ERROR("gpio requrest fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1)
	{
		ret = private_gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret)
		{
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(50);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(50);
		}
		else
		{
			ISP_ERROR("gpio requrest fail %d\n", pwdn_gpio);
		}
	}


	if (pwr_gpio != -1) {
		ret = private_gpio_request(pwr_gpio,"ov4688_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwr_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwr_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",pwr_gpio);
		}
	}

	ret = sensor_detect(sd, &ident);
	if (ret)
	{
		ISP_ERROR("chip found @ 0x%x (%s) is not an %s chip.\n",
			  client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}
	ISP_WARNING("%s chip found @ 0x%02x (%s)\n",
		    SENSOR_NAME, client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n", SENSOR_VERSION);
	if (chip)
	{
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = -1;
	unsigned char val = 0x0;

	ret = sensor_read(sd, 0x3007, &val);
	if (enable & 0x2)
		val |= 0x01;
	else
		val &= 0xfe;
	ret += sensor_write(sd, 0x3007, val);
	if (0 != ret)
		return ret;

	return 0;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if (IS_ERR_OR_NULL(sd))
	{
		return -EINVAL;
	}
	switch (cmd)
	{
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = sensor_set_expo(sd, *(int *)arg);
		break;/*
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sensor_set_integration_time(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if (arg)
			ret = sensor_set_integration_time_short(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sensor_set_analog_gain(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if (arg)
			ret = sensor_set_analog_gain_short(sd, *(int *)arg);
		break;*/
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sensor_set_digital_gain(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sensor_get_black_pedestal(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sensor_set_mode(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (arg)
			ret = sensor_write_array(sd, sensor_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = sensor_write_array(sd, sensor_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, *(int *)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if (arg)
			ret = sensor_set_vflip(sd, *(int *)arg);
		break;
	default:
		break;
	}

	return 0;
}

static int sensor_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sensor_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sensor_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len))
	{
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
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
	int ret;
	unsigned long rate = 0;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
	{
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk))
	{
		goto err_get_mclk;
	}
	rate = clk_get_rate(clk_get_parent(sensor->mclk));
	if (((rate / 1000) % 37125) != 0)
	{
		struct clk *vpll;
		vpll = clk_get(NULL, "vpll");
		if (IS_ERR(vpll))
		{
			pr_err("get vpll failed\n");
		}
		else
		{
			rate = clk_get_rate(vpll);
			if (((rate / 1000) % 37125) != 0)
			{
				clk_set_rate(vpll, 891000000);
			}
			ret = clk_set_parent(sensor->mclk, vpll);
			if (ret < 0)
				pr_err("set mclk parent as epll err\n");
		}
	}

	private_clk_set_rate(sensor->mclk, 27000000);
	private_clk_enable(sensor->mclk);

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sensor_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int sensor_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
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
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
	},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

static __init int init_sensor(void)
{
	int ret = 0;
	sensor_common_init(&sensor_info);

	ret = private_driver_get_interface();
	if (ret)
	{
		return -1;
	}

	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
