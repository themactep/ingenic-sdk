/*
 * gc5035.c
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

#define GC5035_CHIP_ID_H	(0x50)
#define GC5035_CHIP_ID_L	(0x35)
#define GC5035_REG_END		0xffff
#define GC5035_REG_DELAY	0x0000
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define GC5035_SUPPORT_200RES_30FPS_SCLK (0x2da * 2 * 0x7d8 * 30)
#define GC5035_SUPPORT_500RES_15FPS_SCLK (0x2da * 2 * 0x7cc * 15)
#define GC5035_SUPPORT_500RES_25FPS_SCLK (0x2da * 2 * 0x960 * 25)
#define SENSOR_VERSION	"H20210802"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_resolution = TX_SENSOR_RES_200;
module_param(sensor_resolution, int, S_IRUGO);
MODULE_PARM_DESC(sensor_resolution, "Sensor Resolution");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_30;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	unsigned int reg;
	unsigned int gain;
};

struct again_lut gc5035_again_lut[] = {
		{0x0, 0},
		{0x4, 2519},
		{0xc, 4973},
		{0x14, 7364},
		{0x18, 9697},
		{0x20, 11973},
		{0x28, 14196},
		{0x100, 16368},
		{0x104, 18491},
		{0x10c, 20567},
		{0x110, 22599},
		{0x118, 24588},
		{0x11c, 26536},
		{0x124, 28444},
		{0x128, 30315},
		{0x200, 32150},
		{0x204, 33950},
		{0x208, 35716},
		{0x20c, 37450},
		{0x214, 39214},
		{0x218, 40886},
		{0x21c, 42528},
		{0x220, 44142},
		{0x228, 45729},
		{0x22c, 47290},
		{0x300, 48826},
		{0x304, 50337},
		{0x308, 51824},
		{0x30c, 53288},
		{0x310, 54730},
		{0x314, 56150},
		{0x318, 57550},
		{0x31c, 58928},
		{0x320, 60287},
		{0x324, 61627},
		{0x328, 62948},
		{0x800, 64251},
		{0x804, 65536},
		{0x808, 67130},
		{0x80c, 68743},
		{0x810, 70284},
		{0x814, 71845},
		{0x818, 73336},
		{0x81c, 74848},
		{0x824, 76293},
		{0x828, 77758},
		{0x82c, 79160},
		{0x900, 80582},
		{0x904, 81943},
		{0x904, 83324},
		{0x908, 84647},
		{0x90c, 85989},
		{0x910, 87275},
		{0x914, 88580},
		{0x918, 89832},
		{0x91c, 91103},
		{0x920, 92321},
		{0x924, 93560},
		{0x928, 94747},
		{0x92c, 95954},
		{0x930, 97112},
		{0xa00, 98290},
		{0xa04, 99420},
		{0xa08, 100569},
		{0xa08, 101672},
		{0xa0c, 102794},
		{0xa10, 103872},
		{0xa14, 105218},
		{0xa18, 106575},
		{0xa1c, 107883},
		{0xa20, 109203},
		{0xa24, 110475},
		{0xa28, 111759},
		{0xb00, 112998},
		{0xb00, 114249},
		{0xb04, 115455},
		{0xb08, 116674},
		{0xb0c, 117851},
		{0xb10, 119039},
		{0xb14, 120187},
		{0xb14, 121346},
		{0xb18, 122466},
		{0xb1c, 123599},
		{0xb20, 124692},
		{0xb24, 125798},
		{0xb28, 126867},
		{0xb28, 127948},
		{0xc00, 128993},
		{0xc00, 130050},
		{0xc04, 131072},
		{0xc08, 132386},
		{0xc0c, 133660},
		{0xc10, 134939},
		{0xc14, 136179},
		{0xc18, 137425},
		{0xc1c, 138633},
		{0xc20, 139847},
		{0xc20, 141024},
		{0xc24, 142208},
		{0xc28, 143378},
		{0xc2c, 144512},
		{0xc30, 145653},
		{0xd00, 146761},
		{0xd04, 147876},
		{0xd04, 148958},
		{0xd08, 150047},
		{0xd0c, 151105},
		{0xd10, 152170},
		{0xd14, 153503},
		{0xd18, 154836},
		{0xd18, 156132},
		{0xd1c, 157429},
		{0xd20, 158691},
		{0xd24, 159953},
		{0xd28, 161181},
		{0xd2c, 162394},
		{0xe00, 163608},
		{0xe04, 164790},
		{0xe08, 165974},
		{0xe08, 167127},
		{0xe0c, 168283},
		{0xe10, 169408},
		{0xe14, 170831},
		{0xe18, 172233},
		{0xe1c, 173615},
		{0xe20, 174977},
		{0xe24, 176319},
		{0xe28, 177628},
		{0xe2c, 178934},
		{0xf00, 180222},
		{0xf04, 181492},
		{0xf08, 182746},
		{0xf0c, 183983},
		{0xf10, 185470},
		{0xf14, 186935},
		{0xf18, 188377},
		{0xf1c, 189797},
		{0xf20, 191209},
		{0xf24, 192588},
		{0xf28, 193947},
		{0x1000, 195287},
		{0x1004, 196608},
		{0x1008, 198074},
		{0x100c, 199517},
		{0x1010, 200939},
		{0x1014, 202340},
		{0x1018, 203720},
		{0x101c, 205081},
		{0x1020, 206422},
		{0x1024, 207744},
		{0x1028, 209235},
		{0x1100, 210702},
		{0x1104, 212147},
		{0x1108, 213560},
		{0x110c, 214962},
		{0x1110, 216344},
		{0x1114, 217706},
		{0x1118, 219048},
		{0x111c, 220372},
		{0x1120, 221677},
		{0x1124, 222956},
		{0x1128, 224227},
		{0x112c, 225480},
		{0x1200, 226717},
		{0x1200, 227939},
		{0x1204, 229144},
		{0x1208, 230335},
		{0x120c, 231502},
		{0x1210, 232663},
		{0x1210, 233811},
		{0x1214, 234944},
		{0x1218, 236251},
		{0x121c, 237532},
		{0x1220, 238804},
		{0x1224, 240059},
		{0x1228, 241290},
		{0x1300, 242512},
		{0x1304, 243719},
		{0x1304, 244903},
		{0x1308, 246080},
		{0x130c, 247243},
		{0x1310, 248384},
		{0x1314, 249519},
		{0x1314, 250640},
		{0x1318, 251741},
		{0x131c, 252837},
		{0x1320, 253919},
		{0x1324, 254983},
		{0x1328, 256042},
		{0x1328, 257089},
		{0x132c, 258118},
		{0x1330, 259142},
		{0x1400, 260155},
		{0x1400, 261152},
		{0x1404, 262144},
		{0x1408, 263126},
		{0x1408, 264092},
		{0x140c, 265053},
		{0x1410, 266006},
		{0x1414, 266943},
};

struct tx_isp_sensor_attribute gc5035_attr;

unsigned int gc5035_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = gc5035_again_lut;
	while(lut->gain <= gc5035_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].reg;
			return lut[0].gain;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->reg;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == gc5035_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->reg;
				return lut->gain;
			}
		}

		lut++;
	}

	return 0;
}

unsigned int gc5035_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute gc5035_attr={
	.name = "gc5035",
	.chip_id = 0x5035,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x37,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 438,
		.lans = 2,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
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
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.mipi_sc.data_type_en = 0,
		.mipi_sc.data_type_value = RAW10,
		.mipi_sc.del_start = 0,
		.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
		.mipi_sc.sensor_fid_mode = 0,
		.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 266943,
	.expo_fs = 1,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 2000 - 4,
	.integration_time_limit = 2000 - 4,
	.total_width = 0x2da * 2,
	.total_height = 0x7d0,
	.max_integration_time = 2000 - 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = gc5035_alloc_again,
	.sensor_ctrl.alloc_dgain = gc5035_alloc_dgain,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};

/*
 * version 6.8
 * mclk 27Mhz
 * mipiclk 648Mhz
 * framelength 1500
 * linelength 4800
 * pclk 216Mhz
 * rowtime 22.2222us
 * pattern grbg
 */
static struct regval_list gc5035_init_regs_1920_1080_30fps_mipi[] = {
	{0xfc, 0x01},
	{0xf4, 0x40},
	{0xf5, 0xe9},
	{0xf6, 0x14},
	{0xf8, 0x49},
	{0xf9, 0x82},
	{0xfa, 0x00},
	{0xfc, 0x81},
	{0xfe, 0x00},
	{0x36, 0x01},
	{0xd3, 0x87},
	{0x36, 0x00},
	{0x33, 0x00},
	{0xfe, 0x03},
	{0x01, 0xe7},
	{0xf7, 0x01},
	{0xfc, 0x8f},
	{0xfc, 0x8f},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xee, 0x30},
	{0x87, 0x18},
	{0xfe, 0x01},
	{0x8c, 0x90},
	{0xfe, 0x00},
	{0x03, 0x0b},
	{0x04, 0xb8},
	{0x41, 0x07},
	{0x42, 0xd0},
	{0x05, 0x02},
	{0x06, 0xda},
	{0x9d, 0x0c},
	{0x09, 0x00},
	{0x0a, 0x04},
	{0x0b, 0x00},
	{0x0c, 0x03},
	{0x0d, 0x07},
	{0x0e, 0xa8},
	{0x0f, 0x0a},
	{0x10, 0x30},
	{0x11, 0x02},
	{0x17, 0x80},
	{0x19, 0x05},
	{0xfe, 0x02},
	{0x30, 0x03},
	{0x31, 0x03},
	{0xfe, 0x00},
	{0xd9, 0xc0},
	{0x1b, 0x20},
	{0x21, 0x48},
	{0x28, 0x22},
	{0x29, 0x58},
	{0x44, 0x20},
	{0x4b, 0x10},
	{0x4e, 0x1a},
	{0x50, 0x11},
	{0x52, 0x33},
	{0x53, 0x44},
	{0x55, 0x10},
	{0x5b, 0x11},
	{0xc5, 0x02},
	{0x8c, 0x1a},
	{0xfe, 0x02},
	{0x33, 0x05},
	{0x32, 0x38},
	{0xfe, 0x00},
	{0x91, 0x80},
	{0x92, 0x28},
	{0x93, 0x20},
	{0x95, 0xa0},
	{0x96, 0xe0},
	{0xd5, 0xfc},
	{0x97, 0x28},
	{0x16, 0x0c},
	{0x1a, 0x1a},
	{0x1f, 0x11},
	{0x20, 0x10},
	{0x46, 0x83},
	{0x4a, 0x04},
	{0x54, 0x02},
	{0x62, 0x00},
	{0x72, 0x8f},
	{0x73, 0x89},
	{0x7a, 0x05},
	{0x7d, 0xcc},
	{0x90, 0x00},
	{0xce, 0x18},
	{0xd0, 0xb2},
	{0xd2, 0x40},
	{0xe6, 0xe0},
	{0xfe, 0x02},
	{0x12, 0x01},
	{0x13, 0x01},
	{0x14, 0x01},
	{0x15, 0x02},
	{0x22, 0x7c},
	{0xfe, 0x00},
	{0xfc, 0x88},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfc, 0x88},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xb0, 0x6e},
	{0xb1, 0x01},
	{0xb2, 0x00},
	{0xb3, 0x00},
	{0xb4, 0x00},
	{0xb6, 0x00},
	{0xfe, 0x01},
	{0x53, 0x00},
	{0x89, 0x03},
	{0x60, 0x40},
	{0xfe, 0x01},
	{0x42, 0x21},
	{0x49, 0x03},
	{0x4a, 0xff},
	{0x4b, 0xc0},
	{0x55, 0x00},
	{0xfe, 0x01},
	{0x41, 0x28},
	{0x4c, 0x00},
	{0x4d, 0x00},
	{0x4e, 0x3c},
	{0x44, 0x08},
	{0x48, 0x02},
	{0xfe, 0x01},
	{0x91, 0x01},
	{0x92, 0xb8},
	{0x93, 0x01},
	{0x94, 0x57},
	{0x95, 0x04},
	{0x96, 0x38},
	{0x97, 0x07},
	{0x98, 0x80},
	{0x99, 0x00},
	{0xfe, 0x03},
	{0x02, 0x57},
	{0x03, 0xb7},
	{0x15, 0x14},
	{0x18, 0x0f},
	{0x21, 0x22},
	{0x22, 0x06},
	{0x23, 0x48},
	{0x24, 0x12},
	{0x25, 0x28},
	{0x26, 0x08},
	{0x29, 0x06},
	{0x2a, 0x58},
	{0x2b, 0x08},
	{0xfe, 0x01},
	{0x8c, 0x10},
	{0xfe, 0x00},
	{0x3e, 0x91},

	{GC5035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc5035_init_regs_2560_1920_15fps_mipi[] = {
	{0xfc, 0x01},
	{0xf4, 0x40},
	{0xf5, 0xe4},
	{0xf6, 0x14},
	{0xf8, 0x49},
	{0xf9, 0x82},
	{0xfa, 0x01},
	{0xfc, 0x81},
	{0xfe, 0x00},
	{0x36, 0x01},
	{0xd3, 0x87},
	{0x36, 0x00},
	{0x33, 0x00},
	{0xfe, 0x03},
	{0x01, 0x87},
	{0xf7, 0x11},
	{0xfc, 0x8f},
	{0xfc, 0x8f},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xee, 0x30},
	{0x87, 0x18},
	{0xfe, 0x01},
	{0x8c, 0x90},
	{0xfe, 0x00},
	{0x03, 0x07},
	{0x04, 0x08},
	{0x41, 0x07},
	{0x42, 0xcc},
	{0x05, 0x02},
	{0x06, 0xda},
	{0x9d, 0x0c},
	{0x09, 0x00},
	{0x0a, 0x04},
	{0x0b, 0x00},
	{0x0c, 0x03},
	{0x0d, 0x07},
	{0x0e, 0xa8},
	{0x0f, 0x0a},
	{0x10, 0x30},
	{0x11, 0x02},
	{0x17, 0x80},
	{0x19, 0x05},
	{0xfe, 0x02},
	{0x30, 0x03},
	{0x31, 0x03},
	{0xfe, 0x00},
	{0xd9, 0xc0},
	{0x1b, 0x20},
	{0x21, 0x60},
	{0x28, 0x22},
	{0x29, 0x30},
	{0x44, 0x18},
	{0x4b, 0x10},
	{0x4e, 0x20},
	{0x50, 0x11},
	{0x52, 0x33},
	{0x53, 0x44},
	{0x55, 0x10},
	{0x5b, 0x11},
	{0xc5, 0x02},
	{0x8c, 0x20},
	{0xfe, 0x02},
	{0x33, 0x05},
	{0x32, 0x38},
	{0xfe, 0x00},
	{0x91, 0x15},
	{0x92, 0x3a},
	{0x93, 0x20},
	{0x95, 0x45},
	{0x96, 0x35},
	{0xd5, 0xf0},
	{0x97, 0x20},
	{0x16, 0x0c},
	{0x1a, 0x1a},
	{0x1f, 0x11},
	{0x20, 0x10},
	{0x46, 0x83},
	{0x4a, 0x04},
	{0x54, 0x02},
	{0x62, 0x00},
	{0x72, 0x8f},
	{0x73, 0x89},
	{0x7a, 0x05},
	{0x7d, 0xcc},
	{0x90, 0x00},
	{0xce, 0x18},
	{0xd0, 0xb2},
	{0xd2, 0x40},
	{0xe6, 0xe0},
	{0xfe, 0x02},
	{0x12, 0x01},
	{0x13, 0x01},
	{0x14, 0x01},
	{0x15, 0x0b},
	{0x22, 0x7c},
	{0xfe, 0x00},
	{0xfc, 0x88},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfc, 0x88},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xb0, 0x6e},
	{0xb1, 0x01},
	{0xb2, 0x00},
	{0xb3, 0x00},
	{0xb4, 0x00},
	{0xb6, 0x00},
	{0xfe, 0x01},
	{0x53, 0x00},
	{0x89, 0x03},
	{0x60, 0x40},
	{0xfe, 0x01},
	{0x42, 0x21},
	{0x49, 0x03},
	{0x4a, 0xff},
	{0x4b, 0xc0},
	{0x55, 0x82},
	{0xfe, 0x01},
	{0x41, 0x28},
	{0x4c, 0x00},
	{0x4d, 0x00},
	{0x4e, 0x3c},
	{0x44, 0x02},
	{0x48, 0x02},
	{0xfe, 0x01},
	{0x91, 0x00},
	{0x92, 0x08},
	{0x93, 0x00},
	{0x94, 0x07},
	{0x95, 0x07},
	{0x96, 0x80},
	{0x97, 0x0a},
	{0x98, 0x00},
	{0x99, 0x00},
	{0xfe, 0x03},
	{0x02, 0x58},
	{0x03, 0xb7},
	{0x15, 0x14},
	{0x18, 0x0f},
	{0x21, 0x22},
	{0x22, 0x03},
	{0x23, 0x48},
	{0x24, 0x12},
	{0x25, 0x28},
	{0x26, 0x06},
	{0x29, 0x03},
	{0x2a, 0x58},
	{0x2b, 0x06},
	{0xfe, 0x01},
	{0x8c, 0x10},
	{0xfe, 0x00},
	{0x3e, 0x91},

	{GC5035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc5035_init_regs_2592_1944_25fps_mipi[] = {
	{0xfc, 0x01},
	{0xf4, 0x40},
	{0xf5, 0xe9},
	{0xf6, 0x14},
	{0xf8, 0x49},
	{0xf9, 0x82},
	{0xfa, 0x00},
	{0xfc, 0x81},
	{0xfe, 0x00},
	{0x36, 0x01},
	{0xd3, 0x87},
	{0x36, 0x00},
	{0x33, 0x00},
	{0xfe, 0x03},
	{0x01, 0xe7},
	{0xf7, 0x01},
	{0xfc, 0x8f},
	{0xfc, 0x8f},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xee, 0x30},
	{0x87, 0x18},
	{0xfe, 0x01},
	{0x8c, 0x90},
	{0xfe, 0x00},
	{0x03, 0x0b},
	{0x04, 0xb8},
	{0x41, 0x09},
	{0x42, 0x60},
	{0x05, 0x02},
	{0x06, 0xda},
	{0x9d, 0x0c},
	{0x09, 0x00},
	{0x0a, 0x04},
	{0x0b, 0x00},
	{0x0c, 0x03},
	{0x0d, 0x07},
	{0x0e, 0xa8},
	{0x0f, 0x0a},
	{0x10, 0x30},
	{0x11, 0x02},
	{0x17, 0x80},
	{0x19, 0x05},
	{0xfe, 0x02},
	{0x30, 0x03},
	{0x31, 0x03},
	{0xfe, 0x00},
	{0xd9, 0xc0},
	{0x1b, 0x20},
	{0x21, 0x48},
	{0x28, 0x22},
	{0x29, 0x58},
	{0x44, 0x20},
	{0x4b, 0x10},
	{0x4e, 0x1a},
	{0x50, 0x11},
	{0x52, 0x33},
	{0x53, 0x44},
	{0x55, 0x10},
	{0x5b, 0x11},
	{0xc5, 0x02},
	{0x8c, 0x1a},
	{0xfe, 0x02},
	{0x33, 0x05},
	{0x32, 0x38},
	{0xfe, 0x00},
	{0x91, 0x80},
	{0x92, 0x28},
	{0x93, 0x20},
	{0x95, 0xa0},
	{0x96, 0xe0},
	{0xd5, 0xfc},
	{0x97, 0x28},
	{0x16, 0x0c},
	{0x1a, 0x1a},
	{0x1f, 0x11},
	{0x20, 0x10},
	{0x46, 0x83},
	{0x4a, 0x04},
	{0x54, 0x02},
	{0x62, 0x00},
	{0x72, 0x8f},
	{0x73, 0x89},
	{0x7a, 0x05},
	{0x7d, 0xcc},
	{0x90, 0x00},
	{0xce, 0x18},
	{0xd0, 0xb2},
	{0xd2, 0x40},
	{0xe6, 0xe0},
	{0xfe, 0x02},
	{0x12, 0x01},
	{0x13, 0x01},
	{0x14, 0x01},
	{0x15, 0x02},
	{0x22, 0x7c},
	{0xfe, 0x00},
	{0xfc, 0x88},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfc, 0x88},
	{0xfe, 0x10},
	{0xfe, 0x00},
	{0xfc, 0x8e},
	{0xfe, 0x00},
	{0xb0, 0x6e},
	{0xb1, 0x01},
	{0xb2, 0x00},
	{0xb3, 0x00},
	{0xb4, 0x00},
	{0xb6, 0x00},
	{0xfe, 0x01},
	{0x53, 0x00},
	{0x89, 0x03},
	{0x60, 0x40},
	{0xfe, 0x01},
	{0x42, 0x21},
	{0x49, 0x03},
	{0x4a, 0xff},
	{0x4b, 0xc0},
	{0x55, 0x00},
	{0xfe, 0x01},
	{0x41, 0x28},
	{0x4c, 0x00},
	{0x4d, 0x00},
	{0x4e, 0x3c},
	{0x44, 0x08},
	{0x48, 0x02},
	{0xfe, 0x01},
	{0x91, 0x00},
	{0x92, 0x08},
	{0x93, 0x00},
	{0x94, 0x07},
	{0x95, 0x07},
	{0x96, 0x98},
	{0x97, 0x0a},
	{0x98, 0x20},
	{0x99, 0x00},
	{0xfe, 0x03},
	{0x02, 0x57},
	{0x03, 0xb7},
	{0x15, 0x14},
	{0x18, 0x0f},
	{0x21, 0x22},
	{0x22, 0x06},
	{0x23, 0x48},
	{0x24, 0x12},
	{0x25, 0x28},
	{0x26, 0x08},
	{0x29, 0x06},
	{0x2a, 0x58},
	{0x2b, 0x08},
	{0xfe, 0x01},
	{0x8c, 0x10},
	{0xfe, 0x00},
	{0x3e, 0x91},

	{GC5035_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the gc5035_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting gc5035_win_sizes[] = {
	/* [0] 1920*1080 @30fps */
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= gc5035_init_regs_1920_1080_30fps_mipi,
	},
	/* [1] 2560*1920 @15fps */
	{
		.width		= 2560,
		.height		= 1920,
		.fps		= 15 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= gc5035_init_regs_2560_1920_15fps_mipi,
	},
	/* [1] 2592*1944 @25fps */
	{
		.width		= 2592,
		.height		= 1944,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= gc5035_init_regs_2592_1944_25fps_mipi,
	},
};

struct tx_isp_sensor_win_setting *wsize = &gc5035_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list gc5035_stream_on[] = {
	{GC5035_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc5035_stream_off[] = {
	{GC5035_REG_END, 0x00},	/* END MARKER */
};

int gc5035_read(struct tx_isp_subdev *sd, unsigned char reg,
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

int gc5035_write(struct tx_isp_subdev *sd, unsigned char reg,
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
static int gc5035_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != GC5035_REG_END) {
		if (vals->reg_num == GC5035_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc5035_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}
	return 0;
}
#endif

static int gc5035_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != GC5035_REG_END) {
		if (vals->reg_num == GC5035_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc5035_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int gc5035_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int gc5035_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = gc5035_read(sd, 0xf0, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != GC5035_CHIP_ID_H)
		return -ENODEV;
	ret = gc5035_read(sd, 0xf1, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != GC5035_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int gc5035_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = gc5035_write(sd, 0x04, value & 0xff);
	ret += gc5035_write(sd, 0x03, (value&0x3f00)>>8);
	if (ret < 0) {
		ISP_ERROR("gc5035_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int gc5035_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = gc5035_write(sd, 0xfe, 0x00);
	ret = gc5035_write(sd, 0xb6, value >> 8 & 0xff);
	ret = gc5035_write(sd, 0xb1, 0x01);
	ret = gc5035_write(sd, 0xb2, value & 0xff);

	if (ret < 0) {
		ISP_ERROR("gc5035_write error  %d" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int gc5035_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc5035_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc5035_init(struct tx_isp_subdev *sd, int enable)
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
	ret = gc5035_write_array(sd, wsize->regs);

	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int gc5035_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = gc5035_write_array(sd, gc5035_stream_on);
		pr_debug("gc5035 stream on\n");
	} else {
		ret = gc5035_write_array(sd, gc5035_stream_off);
		pr_debug("gc5035 stream off\n");
	}

	return ret;
}

static int gc5035_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;

	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	if(sensor_resolution == TX_SENSOR_RES_200)
		sclk = GC5035_SUPPORT_200RES_30FPS_SCLK;
	else if(sensor_resolution == TX_SENSOR_RES_500 && sensor_max_fps == TX_SENSOR_MAX_FPS_15)
		sclk = GC5035_SUPPORT_500RES_15FPS_SCLK;
	else if(sensor_resolution == TX_SENSOR_RES_500 && sensor_max_fps == TX_SENSOR_MAX_FPS_25)
		sclk = GC5035_SUPPORT_500RES_25FPS_SCLK;

	ret = gc5035_write(sd, 0xfe, 0x0);
	ret += gc5035_read(sd, 0x05, &tmp);
	hts = tmp;
	ret += gc5035_read(sd, 0x06, &tmp);
	if(ret < 0)
		return -1;
	hts = ((hts << 8) + tmp) << 1;

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = gc5035_write(sd, 0x41, (unsigned char)((vts & 0x3f00) >> 8));
	ret += gc5035_write(sd, 0x42, (unsigned char)(vts & 0xff));
	if (0 != ret) {
		ISP_ERROR("err: gc5035_write err\n");
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

static int gc5035_set_mode(struct tx_isp_subdev *sd, int value)
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

static int gc5035_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"gc5035_reset");
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
		ret = private_gpio_request(pwdn_gpio,"gc5035_pwdn");
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
	ret = gc5035_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc5035 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("gc5035 chip found @ 0x%02x (%s)\n sensor drv version %s", client->addr, client->adapter->name, SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "gc5035", sizeof("gc5035"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int gc5035_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = gc5035_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = gc5035_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = gc5035_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = gc5035_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = gc5035_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = gc5035_write_array(sd, gc5035_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = gc5035_write_array(sd, gc5035_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = gc5035_set_fps(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int gc5035_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = gc5035_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int gc5035_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc5035_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops gc5035_core_ops = {
	.g_chip_ident = gc5035_g_chip_ident,
	.reset = gc5035_reset,
	.init = gc5035_init,
	/*.ioctl = gc5035_ops_ioctl,*/
	.g_register = gc5035_g_register,
	.s_register = gc5035_s_register,
};

static struct tx_isp_subdev_video_ops gc5035_video_ops = {
	.s_stream = gc5035_s_stream,
};

static struct tx_isp_subdev_sensor_ops	gc5035_sensor_ops = {
	.ioctl	= gc5035_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops gc5035_ops = {
	.core = &gc5035_core_ops,
	.video = &gc5035_video_ops,
	.sensor = &gc5035_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "gc5035",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc5035_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

	switch(sensor_resolution)
	{
	case TX_SENSOR_RES_200:
		wsize = &gc5035_win_sizes[0];
		gc5035_attr.max_integration_time_native = 0x7d0 - 4;
		gc5035_attr.integration_time_limit = 0x7d0 - 4;
		gc5035_attr.total_width = 0x2da * 2;
		gc5035_attr.total_height = 0x7d0;
		gc5035_attr.max_integration_time = 0x7d0 - 4;
		gc5035_attr.mipi.image_twidth = 1920;
		gc5035_attr.mipi.image_theight = 1080;
		break;
	case TX_SENSOR_RES_500:
		switch(sensor_max_fps)
		{
		case TX_SENSOR_MAX_FPS_15:
			wsize = &gc5035_win_sizes[1];
			gc5035_attr.max_integration_time_native = 0x7cc - 4;
			gc5035_attr.integration_time_limit = 0x7cc - 4;
			gc5035_attr.total_width = 0x2da * 2;
			gc5035_attr.total_height = 0x7cc;
			gc5035_attr.max_integration_time = 0x7cc - 4;
			gc5035_attr.mipi.image_twidth = 2560;
			gc5035_attr.mipi.image_theight = 1920;
			break;
		case TX_SENSOR_MAX_FPS_25:
			wsize = &gc5035_win_sizes[2];
			gc5035_attr.max_integration_time_native = 0x960 - 4;
			gc5035_attr.integration_time_limit = 0x960 - 4;
			gc5035_attr.total_width = 0x2da * 2;
			gc5035_attr.total_height = 0x960;
			gc5035_attr.max_integration_time = 0x960 - 4;
			gc5035_attr.mipi.image_twidth = 2592;
			gc5035_attr.mipi.image_theight = 1944;
			break;
		}
		break;
	default:
		ISP_ERROR("Can not support this resolution!!!\n");
		break;
	}

	/*
	  convert sensor-gain into isp-gain,
	*/

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &gc5035_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc5035_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->gc5035\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int gc5035_remove(struct i2c_client *client)
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

static const struct i2c_device_id gc5035_id[] = {
	{ "gc5035", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc5035_id);

static struct i2c_driver gc5035_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "gc5035",
	},
	.probe		= gc5035_probe,
	.remove		= gc5035_remove,
	.id_table	= gc5035_id,
};

static __init int init_gc5035(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init gc5035 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&gc5035_driver);
}

static __exit void exit_gc5035(void)
{
	private_i2c_del_driver(&gc5035_driver);
}

module_init(init_gc5035);
module_exit(exit_gc5035);

MODULE_DESCRIPTION("A low-level driver for gc5035 sensors");
MODULE_LICENSE("GPL");
