// SPDX-License-Identifier: GPL-2.0+
/*
 * gc4023.c
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
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

#define SENSOR_NAME "gc4023"
#define SENSOR_CHIP_ID_H (0x40)
#define SENSOR_CHIP_ID_L (0x23)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0x0000
#define SENSOR_SUPPORT_30FPS_SCLK (0x7e9 * 0x4b0 * 2 * 25)
#define SENSOR_SUPPORT_20FPS_SCLK 108*1000*1000

#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20230719"

/* CONFIG_SENSOR_SUSPEND:支持Sensor suspend功能
 * SENSOR_POWER_OFF :选择Sensor断电
 * SENSOR_STANDBY   :选择Sensor standby方式,始终维持电源
 * */
#define CONFIG_SENSOR_SUSPEND   1
#if CONFIG_SENSOR_SUSPEND
#define SENSOR_POWER_OFF
/*#define SENSOR_STANDBY*/
#endif

static int reset_gpio = GPIO_PC(27);
static int pwdn_gpio = -1;
/* 定义SENSOR_WITHOUT_INIT时表示boot阶段已进行sensor初始化，下sensor初始化配置，可节省初始化sensor时间。*/
/* 不定义SENSOR_WITHOUT_INIT时，debug使用，在驱动里重新初始化sensor，重新下初始化配置*/
#define SENSOR_WITHOUT_INIT
static int shvflip = 1;
//module_param(shvflip, int, S_IRUGO);
//MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

static unsigned char ht_gain = 24;
static unsigned char gain_flag = 0;
static unsigned char ag_last = 0;

struct again_lut {
	unsigned int index;
	unsigned char reg614;
	unsigned char reg615;
	unsigned char reg218;
	unsigned char reg1467;
	unsigned char reg1468;
	unsigned char regb8;
	unsigned char regb9;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x01, 0x00, 0},
	{0x01, 0x80, 0x02, 0x00, 0x19, 0x19, 0x01, 0x0B, 16208},
	{0x02, 0x01, 0x00, 0x00, 0x19, 0x19, 0x01, 0x19, 32217},
	{0x03, 0x81, 0x02, 0x00, 0x1a, 0x1a, 0x01, 0x2A, 47690},
	{0x04, 0x02, 0x00, 0x00, 0x1b, 0x1b, 0x02, 0x00, 65536},
	{0x05, 0x82, 0x02, 0x00, 0x1c, 0x1c, 0x02, 0x17, 81784},
	{0x06, 0x03, 0x00, 0x00, 0x1d, 0x1d, 0x02, 0x33, 97213},
	{0x07, 0x83, 0x02, 0x00, 0x1e, 0x1e, 0x03, 0x14, 113226},
	{0x08, 0x04, 0x00, 0x00, 0x20, 0x20, 0x04, 0x00, 131072},
	{0x09, 0x80, 0x02, 0x20, 0x22, 0x22, 0x04, 0x2F, 147001},
	{0x0A, 0x01, 0x00, 0x20, 0x23, 0x23, 0x05, 0x26, 162766},
	{0x0B, 0x81, 0x02, 0x20, 0x25, 0x25, 0x06, 0x28, 178990},
	{0x0C, 0x02, 0x00, 0x20, 0x25, 0x25, 0x08, 0x00, 196608},
	{0x0D, 0x82, 0x02, 0x20, 0x26, 0x26, 0x09, 0x1E, 212696},
	{0x0E, 0x03, 0x00, 0x20, 0x26, 0x26, 0x0B, 0x0C, 228446},
	{0x0F, 0x83, 0x02, 0x20, 0x26, 0x26, 0x0D, 0x11, 244419},
	{0x10, 0x04, 0x00, 0x20, 0x27, 0x27, 0x10, 0x00, 262144},
	{0x11, 0x84, 0x02, 0x20, 0x27, 0x27, 0x12, 0x3D, 278158},
	{0x12, 0x05, 0x00, 0x20, 0x28, 0x28, 0x16, 0x19, 293982},
	{0x13, 0x85, 0x02, 0x20, 0x28, 0x28, 0x1A, 0x22, 310012},
	{0x14, 0xb5, 0x04, 0x20, 0x29, 0x29, 0x20, 0x00, 327680},
	{0x15, 0x85, 0x05, 0x20, 0x2a, 0x2a, 0x25, 0x3A, 343731},
	{0x16, 0x05, 0x08, 0x20, 0x2b, 0x2b, 0x2C, 0x33, 359484},
	{0x17, 0x45, 0x09, 0x20, 0x2d, 0x2d, 0x35, 0x05, 375550},
	{0x18, 0x55, 0x0a, 0x20, 0x2e, 0x2e, 0x40, 0x00, 393216},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it) {
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;

	*sensor_it = expo;

	return isp_it;
}

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = 0;
			return lut[0].gain;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 702,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 2560,
	.image_theight = 1440,
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


struct tx_isp_mipi_bus sensor_mipi_20fps_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 464,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 2560,
	.image_theight = 1440,
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

struct tx_isp_mipi_bus sensor_mipi_15fps_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 352,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 2560,
	.image_theight = 1440,
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

struct tx_isp_mipi_bus sensor_mipi_120fps_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 351,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 1280,
	.image_theight = 360,
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

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = 0x4023,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.cbus_device = 0x29,
	.max_again = 393216,
	.max_dgain = 0,
	.expo_fs = 1,
	.min_integration_time = 2,
	.min_integration_time_short = 2,
	.min_integration_time_native = 2,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	//      void priv; /* point to struct tx_isp_sensor_board_info */
};


static struct regval_list sensor_init_regs_2560_1440_25fps_mipi[] = {
	/*version 1.2*/
	/*mclk 27Mhz*/
	/*mipi 702Mbps/lane*/
	/*vts = 1800*/
	/*window 2560 1440*/
	/*row time=26.666us*/
	/*bayer order  rggb*/
	//row time=26.66us
	//bayer order  rggb
	/*SYSTEM*/
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x0a38, 0x00},
	{0x0a38, 0x01},
	{0x0a20, 0x17},
	{0x061c, 0x50},
	{0x061d, 0x21},
	{0x061e, 0x50},
	{0x061f, 0x06},
	{0x0a21, 0x10},
	{0x0a34, 0x40},
	{0x0a35, 0x01},
	{0x0a36, 0x42},
	{0x0a37, 0x06},
	{0x0314, 0x50},
	{0x0315, 0x00},
	{0x031c, 0xce},
	{0x0219, 0x47},
	{0x0342, 0x04},
	{0x0343, 0xb0},
	{0x0259, 0x05},
	{0x025a, 0xa0},
	{0x0340, 0x07},
	{0x0341, 0x53},
	{0x0347, 0x02},
	{0x0348, 0x0a},
	{0x0349, 0x08},
	{0x034a, 0x05},
	{0x034b, 0xa8},
	{0x0094, 0x0a},
	{0x0095, 0x00},
	{0x0096, 0x05},
	{0x0097, 0xa0},
	{0x0099, 0x04},
	{0x009b, 0x04},
	{0x0709, 0x40},
	{0x0719, 0x40},
	{0x060c, 0x01},
	{0x060e, 0x08},
	{0x060f, 0x05},
	{0x070c, 0x01},
	{0x070e, 0x08},
	{0x070f, 0x05},
	{0x0909, 0x03},
	{0x0902, 0x04},
	{0x0904, 0x0b},
	{0x0907, 0x54},
	{0x0908, 0x06},
	{0x0903, 0x9d},
	{0x072a, 0x18},
	{0x0724, 0x0a},
	{0x0727, 0x0a},
	{0x072a, 0x1c},
	{0x072b, 0x0a},
	{0x1466, 0x10},
	{0x1468, 0x18},
	{0x1467, 0x18},
	{0x1469, 0x80},
	{0x146a, 0xe8},
	{0x0707, 0x07},
	{0x0737, 0x0f},
	{0x0704, 0x01},
	{0x0706, 0x02},
	{0x0716, 0x02},
	{0x0708, 0xc8},
	{0x0718, 0xc8},
	{0x061a, 0x00},
	{0x1430, 0x80},
	{0x1407, 0x10},
	{0x1408, 0x16},
	{0x1409, 0x03},
	{0x146d, 0x0e},
	{0x146e, 0x42},
	{0x146f, 0x43},
	{0x1470, 0x3c},
	{0x1471, 0x3d},
	{0x1472, 0x3a},
	{0x1473, 0x3a},
	{0x1474, 0x40},
	{0x1475, 0x46},
	{0x1420, 0x14},
	{0x1464, 0x15},
	{0x146c, 0x40},
	{0x146d, 0x40},
	{0x1423, 0x08},
	{0x1428, 0x10},
	{0x1462, 0x18},
	{0x02ce, 0x04},
	{0x143a, 0x0f},
	{0x142b, 0x88},
	{0x0245, 0xc9},
	{0x023a, 0x08},
	{0x02cd, 0x92},
	{0x0612, 0x02},
	{0x0613, 0xc7},
	{0x0243, 0x03},
	{0x021b, 0x09},
	{0x0089, 0x03},
	{0x0040, 0xa3},
	{0x0075, 0x64},
	{0x0004, 0x0f},
	{0x0002, 0xab},
	{0x0053, 0x0a},
	{0x0205, 0x0c},
	{0x0202, 0x06},
	{0x0203, 0x27},
	{0x0614, 0x00},
	{0x0615, 0x00},
	{0x0181, 0x0c},
	{0x0182, 0x05},
	{0x0185, 0x01},
	{0x0180, 0x46},
	{0x0100, 0x08},
	{0x0106, 0x38},
	{0x010d, 0x80},
	{0x010e, 0x0c},
	{0x0113, 0x02},
	{0x0114, 0x01},
	{0x0115, 0x10},
	{0x0052, 0x02},
	{0x0076, 0x01},
	{0x021a, 0x10},
	{0x0434, 0x75},
	{0x0435, 0x75},
	{0x0436, 0x75},
	{0x0437, 0x75},
	{0x0430, 0x0a},
	{0x0431, 0x0a},
	{0x0432, 0x0a},
	{0x0433, 0x0a},
	{0x0458, 0x00},
	{0x0459, 0x00},
	{0x045a, 0x00},
	{0x045b, 0x00},
	{0x0a67, 0x80},
	{0x0a54, 0x0e},
	{0x0a65, 0x10},
	{0x0a98, 0x10},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0029, 0x08},
	{0x002b, 0xa8},
	{0x0a83, 0xe0},
	{0x0a72, 0x02},
	{0x0a73, 0x60},
	{0x0a75, 0x41},
	{0x0a70, 0x03},
	{0x0a5a, 0x80},
	{SENSOR_REG_DELAY, 0x14}, //delay 20ms
	{0x05be, 0x01},
	{0x0a70, 0x00},
	{0x0080, 0x02},
	{0x0a67, 0x00},
	{0x0100, 0x09},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_2560_1440_25fps_24Mmipi[] = {

	/*version 1.2*/
	/*mclk 24Mhz*/
	/*mipi 704Mbps/lane*/
	/*vts = 1500*/
	/*window 2560 1440*/
	/*row time=22.22us*/
	/*bayer order  rggb*/
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x0a38, 0x00},
	{0x0a38, 0x01},
	{0x0a20, 0x17},
	{0x061c, 0x50},
	{0x061d, 0x21},
	{0x061e, 0x6c},
	{0x061f, 0x06},
	{0x0a21, 0x10},
	{0x0a34, 0x40},
	{0x0a35, 0x01},
	{0x0a36, 0x58},
	{0x0a37, 0x06},
	{0x0314, 0x50},
	{0x0315, 0x00},
	{0x031c, 0xce},
	{0x0219, 0x47},
	{0x0342, 0x04},
	{0x0343, 0xb0},
	{0x0259, 0x05},
	{0x025a, 0xa0},
	{0x0340, 0x07},
	{0x0341, 0xe9},
	{0x0347, 0x02},
	{0x0348, 0x0a},
	{0x0349, 0x08},
	{0x034a, 0x05},
	{0x034b, 0xa8},

	{0x0094, 0x0a},
	{0x0095, 0x00},
	{0x0096, 0x05},
	{0x0097, 0xa0},

	{0x0099, 0x04},
	{0x009b, 0x04},
	{0x0709, 0x40},
	{0x0719, 0x40},
	{0x060c, 0x01},
	{0x060e, 0x08},
	{0x060f, 0x05},
	{0x070c, 0x01},
	{0x070e, 0x08},
	{0x070f, 0x05},
	{0x0909, 0x03},
	{0x0902, 0x04},
	{0x0904, 0x0b},
	{0x0907, 0x54},
	{0x0908, 0x06},
	{0x0903, 0x9d},
	{0x072a, 0x18},
	{0x0724, 0x0a},
	{0x0727, 0x0a},
	{0x072a, 0x1c},
	{0x072b, 0x0a},
	{0x1466, 0x10},
	{0x1468, 0x18},
	{0x1467, 0x18},
	{0x1469, 0x80},
	{0x146a, 0xe8},
	{0x0707, 0x07},
	{0x0737, 0x0f},
	{0x0704, 0x01},
	{0x0706, 0x02},
	{0x0716, 0x02},
	{0x0708, 0xc8},
	{0x0718, 0xc8},
	{0x061a, 0x00},
	{0x1430, 0x80},
	{0x1407, 0x10},
	{0x1408, 0x16},
	{0x1409, 0x03},
	{0x146d, 0x0e},
	{0x146e, 0x42},
	{0x146f, 0x43},
	{0x1470, 0x3c},
	{0x1471, 0x3d},
	{0x1472, 0x3a},
	{0x1473, 0x3a},
	{0x1474, 0x40},
	{0x1475, 0x46},
	{0x1420, 0x14},
	{0x1464, 0x15},
	{0x146c, 0x40},
	{0x146d, 0x40},
	{0x1423, 0x08},
	{0x1428, 0x10},
	{0x1462, 0x18},
	{0x02ce, 0x04},
	{0x143a, 0x0f},
	{0x142b, 0x88},
	{0x0245, 0xc9},
	{0x023a, 0x08},
	{0x02cd, 0x92},
	{0x0612, 0x02},
	{0x0613, 0xc7},
	{0x0243, 0x03},
	{0x021b, 0x09},
	{0x0089, 0x03},
	{0x0040, 0xa3},
	{0x0075, 0x64},
	{0x0004, 0x0f},
	{0x0002, 0xab},
	{0x0053, 0x0a},
	{0x0205, 0x0c},
	{0x0202, 0x06},
	{0x0203, 0x27},
	{0x0614, 0x00},
	{0x0615, 0x00},
	{0x0181, 0x0c},
	{0x0182, 0x05},
	{0x0185, 0x01},
	{0x0180, 0x46},
	{0x0100, 0x08},
	{0x0106, 0x38},
	{0x010d, 0x80},
	{0x010e, 0x0c},
	{0x0113, 0x02},
	{0x0114, 0x01},
	{0x0115, 0x10},
	{0x0052, 0x02},
	{0x0076, 0x01},
	{0x021a, 0x10},
	{0x0434, 0x75},
	{0x0435, 0x75},
	{0x0436, 0x75},
	{0x0437, 0x75},
	{0x0430, 0x0a},
	{0x0431, 0x0a},
	{0x0432, 0x0a},
	{0x0433, 0x0a},
	{0x0458, 0x00},
	{0x0459, 0x00},
	{0x045a, 0x00},
	{0x045b, 0x00},
	{0x0a67, 0x80},
	{0x0a54, 0x0e},
	{0x0a65, 0x10},
	{0x0a98, 0x10},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0029, 0x08},
	{0x002b, 0xa8},
	{0x0a83, 0xe0},
	{0x0a72, 0x02},
	{0x0a73, 0x60},
	{0x0a75, 0x41},
	{0x0a70, 0x03},
	{0x0a5a, 0x80},
	{SENSOR_REG_DELAY, 0x14}, //delay 20ms
	{0x05be, 0x01},
	{0x0a70, 0x00},
	{0x0080, 0x02},
	{0x0a67, 0x00},
	{0x0100, 0x09},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_2560_1440_20fps_mipi[] = {
	/*version 1.2*/
	/*mclk 27Mhz*/
	/*mipi 702Mbps/lane*/
	/*vts = 2250*/
	/*window 2560 1440*/
	/*row time=22.22us*/
	/*bayer order  rggb*/
	//row time=22.22us
	//bayer order  rggb
	/*SYSTEM*/
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x0a38, 0x00},
	{0x0a38, 0x01},
	{0x0a20, 0x17},
	{0x061c, 0x50},
	{0x061d, 0x21},
	{0x061e, 0x60},
	{0x061f, 0x06},
	{0x0a21, 0x10},
	{0x0a34, 0x40},
	{0x0a35, 0x01},
	{0x0a36, 0x4e},
	{0x0a37, 0x06},
	{0x0314, 0x50},
	{0x0315, 0x00},
	{0x031c, 0xce},
	{0x0219, 0x47},
	{0x0342, 0x04},
	{0x0343, 0xb0},
	{0x0259, 0x05},
	{0x025a, 0xa0},
	{0x0340, 0x08},//vts -> 0x5dc = 1500
	{0x0341, 0xca},//
	{0x0347, 0x02},
	{0x0348, 0x0a},
	{0x0349, 0x08},
	{0x034a, 0x05},
	{0x034b, 0xa8},
	{0x0094, 0x0a},
	{0x0095, 0x00},
	{0x0096, 0x05},
	{0x0097, 0xa0},
	{0x0099, 0x04},
	{0x009b, 0x04},
	{0x0709, 0x40},
	{0x0719, 0x40},
	{0x060c, 0x01},
	{0x060e, 0x08},
	{0x060f, 0x05},
	{0x070c, 0x01},
	{0x070e, 0x08},
	{0x070f, 0x05},
	{0x0909, 0x03},
	{0x0902, 0x04},
	{0x0904, 0x0b},
	{0x0907, 0x54},
	{0x0908, 0x06},
	{0x0903, 0x9d},
	{0x072a, 0x18},
	{0x0724, 0x0a},
	{0x0727, 0x0a},
	{0x072a, 0x1c},
	{0x072b, 0x0a},
	{0x1466, 0x10},
	{0x1468, 0x18},
	{0x1467, 0x18},
	{0x1469, 0x80},
	{0x146a, 0xe8},
	{0x0707, 0x07},
	{0x0737, 0x0f},
	{0x0704, 0x01},
	{0x0706, 0x02},
	{0x0716, 0x02},
	{0x0708, 0xc8},
	{0x0718, 0xc8},
	{0x061a, 0x00},
	{0x1430, 0x80},
	{0x1407, 0x10},
	{0x1408, 0x16},
	{0x1409, 0x03},
	{0x146d, 0x0e},
	{0x146e, 0x42},
	{0x146f, 0x43},
	{0x1470, 0x3c},
	{0x1471, 0x3d},
	{0x1472, 0x3a},
	{0x1473, 0x3a},
	{0x1474, 0x40},
	{0x1475, 0x46},
	{0x1420, 0x14},
	{0x1464, 0x15},
	{0x146c, 0x40},
	{0x146d, 0x40},
	{0x1423, 0x08},
	{0x1428, 0x10},
	{0x1462, 0x18},
	{0x02ce, 0x04},
	{0x143a, 0x0f},
	{0x142b, 0x88},
	{0x0245, 0xc9},
	{0x023a, 0x08},
	{0x02cd, 0x92},
	{0x0612, 0x02},
	{0x0613, 0xc7},
	{0x0243, 0x03},
	{0x021b, 0x09},
	{0x0089, 0x03},
	{0x0040, 0xa3},
	{0x0075, 0x64},
	{0x0004, 0x0f},
	{0x0002, 0xab},
	{0x0053, 0x0a},
	{0x0205, 0x0c},
	{0x0202, 0x06},
	{0x0203, 0x27},
	{0x0614, 0x00},
	{0x0615, 0x00},
	{0x0181, 0x0c},
	{0x0182, 0x05},
	{0x0185, 0x01},
	{0x0180, 0x46},
	{0x0100, 0x08},
	{0x0106, 0x38},
	{0x010d, 0x80},
	{0x010e, 0x0c},
	{0x0113, 0x02},
	{0x0114, 0x01},
	{0x0115, 0x10},
	{0x0052, 0x02},
	{0x0076, 0x01},
	{0x021a, 0x10},
	{0x0434, 0x75},
	{0x0435, 0x75},
	{0x0436, 0x75},
	{0x0437, 0x75},
	{0x0430, 0x0a},
	{0x0431, 0x0a},
	{0x0432, 0x0a},
	{0x0433, 0x0a},
	{0x0458, 0x00},
	{0x0459, 0x00},
	{0x045a, 0x00},
	{0x045b, 0x00},
	{0x0a67, 0x80},
	{0x0a54, 0x0e},
	{0x0a65, 0x10},
	{0x0a98, 0x10},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0029, 0x08},
	{0x002b, 0xa8},
	{0x0a83, 0xe0},
	{0x0a72, 0x02},
	{0x0a73, 0x60},
	{0x0a75, 0x41},
	{0x0a70, 0x03},
	{0x0a5a, 0x80},
	{SENSOR_REG_DELAY, 0x14}, //delay 20ms
	{0x05be, 0x01},
	{0x0a70, 0x00},
	{0x0080, 0x02},
	{0x0a67, 0x00},
	{0x0100, 0x09},

	{SENSOR_REG_END, 0x00},
};


static struct regval_list sensor_init_regs_2560_1440_15fps_mipi[] = {
	/*version 1.2*/
	/*mclk 27Mhz*/
	/*mipi 702Mbps/lane*/
	/*vts = 3000*/
	/*window 2560 1440*/
	/*row time=22.22us*/
	/*bayer order  rggb*/
	//row time=22.22us
	//bayer order  rggb
	/*SYSTEM*/
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x0a38, 0x00},
	{0x0a38, 0x01},
	{0x0a20, 0x17},
	{0x061c, 0x50},
	{0x061d, 0x21},
	{0x061e, 0x60},
	{0x061f, 0x06},
	{0x0a21, 0x10},
	{0x0a34, 0x40},
	{0x0a35, 0x01},
	{0x0a36, 0x4e},
	{0x0a37, 0x06},
	{0x0314, 0x50},
	{0x0315, 0x00},
	{0x031c, 0xce},
	{0x0219, 0x47},
	{0x0342, 0x04},
	{0x0343, 0xb0},
	{0x0259, 0x05},
	{0x025a, 0xa0},
	{0x0340, 0x0b},//vts -> 1500
	{0x0341, 0xb8},//
	{0x0347, 0x02},
	{0x0348, 0x0a},
	{0x0349, 0x08},
	{0x034a, 0x05},
	{0x034b, 0xa8},
	{0x0094, 0x0a},
	{0x0095, 0x00},
	{0x0096, 0x05},
	{0x0097, 0xa0},
	{0x0099, 0x04},
	{0x009b, 0x04},
	{0x0709, 0x40},
	{0x0719, 0x40},
	{0x060c, 0x01},
	{0x060e, 0x08},
	{0x060f, 0x05},
	{0x070c, 0x01},
	{0x070e, 0x08},
	{0x070f, 0x05},
	{0x0909, 0x03},
	{0x0902, 0x04},
	{0x0904, 0x0b},
	{0x0907, 0x54},
	{0x0908, 0x06},
	{0x0903, 0x9d},
	{0x072a, 0x18},
	{0x0724, 0x0a},
	{0x0727, 0x0a},
	{0x072a, 0x1c},
	{0x072b, 0x0a},
	{0x1466, 0x10},
	{0x1468, 0x18},
	{0x1467, 0x18},
	{0x1469, 0x80},
	{0x146a, 0xe8},
	{0x0707, 0x07},
	{0x0737, 0x0f},
	{0x0704, 0x01},
	{0x0706, 0x02},
	{0x0716, 0x02},
	{0x0708, 0xc8},
	{0x0718, 0xc8},
	{0x061a, 0x00},
	{0x1430, 0x80},
	{0x1407, 0x10},
	{0x1408, 0x16},
	{0x1409, 0x03},
	{0x146d, 0x0e},
	{0x146e, 0x42},
	{0x146f, 0x43},
	{0x1470, 0x3c},
	{0x1471, 0x3d},
	{0x1472, 0x3a},
	{0x1473, 0x3a},
	{0x1474, 0x40},
	{0x1475, 0x46},
	{0x1420, 0x14},
	{0x1464, 0x15},
	{0x146c, 0x40},
	{0x146d, 0x40},
	{0x1423, 0x08},
	{0x1428, 0x10},
	{0x1462, 0x18},
	{0x02ce, 0x04},
	{0x143a, 0x0f},
	{0x142b, 0x88},
	{0x0245, 0xc9},
	{0x023a, 0x08},
	{0x02cd, 0x92},
	{0x0612, 0x02},
	{0x0613, 0xc7},
	{0x0243, 0x03},
	{0x021b, 0x09},
	{0x0089, 0x03},
	{0x0040, 0xa3},
	{0x0075, 0x64},
	{0x0004, 0x0f},
	{0x0002, 0xab},
	{0x0053, 0x0a},
	{0x0205, 0x0c},
	{0x0202, 0x06},
	{0x0203, 0x27},
	{0x0614, 0x00},
	{0x0615, 0x00},
	{0x0181, 0x0c},
	{0x0182, 0x05},
	{0x0185, 0x01},
	{0x0180, 0x46},
	{0x0100, 0x08},
	{0x0106, 0x38},
	{0x010d, 0x80},
	{0x010e, 0x0c},
	{0x0113, 0x02},
	{0x0114, 0x01},
	{0x0115, 0x10},
	{0x0052, 0x02},
	{0x0076, 0x01},
	{0x021a, 0x10},
	{0x0434, 0x75},
	{0x0435, 0x75},
	{0x0436, 0x75},
	{0x0437, 0x75},
	{0x0430, 0x0a},
	{0x0431, 0x0a},
	{0x0432, 0x0a},
	{0x0433, 0x0a},
	{0x0458, 0x00},
	{0x0459, 0x00},
	{0x045a, 0x00},
	{0x045b, 0x00},
	{0x0a67, 0x80},
	{0x0a54, 0x0e},
	{0x0a65, 0x10},
	{0x0a98, 0x10},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0029, 0x08},
	{0x002b, 0xa8},
	{0x0a83, 0xe0},
	{0x0a72, 0x02},
	{0x0a73, 0x60},
	{0x0a75, 0x41},
	{0x0a70, 0x03},
	{0x0a5a, 0x80},
	{SENSOR_REG_DELAY, 0x14}, //delay 20ms
	{0x05be, 0x01},
	{0x0a70, 0x00},
	{0x0080, 0x02},
	{0x0a67, 0x00},
	{0x0100, 0x09},
	{SENSOR_REG_END, 0x00},
};


static struct regval_list sensor_init_regs_1280_360_120fps_24M_mipi[] = {
	//version 0.5
	//mclk 24Mhz
	//mipi 2 lane 351Mbps/lane
	//vts = 400
	//window 1280 360
	//row time=20.833us,120FPS
	//bayer order RGGB
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x0a38, 0x00},
	{0x0a38, 0x01},
	{0x0a20, 0x17},
	{0x061c, 0x50},
	{0x061d, 0x21},
	{0x061e, 0x6c},
	{0x061f, 0x06},
	{0x0a21, 0x10},
	{0x0a34, 0x40},
	{0x0a35, 0x01},
	{0x0a36, 0x58},
	{0x0a37, 0x06},
	{0x0a28, 0x01},
	{0x0314, 0x50},
	{0x0315, 0x00},
	{0x031c, 0xce},
	{0x0219, 0x47},
	{0x0342, 0x04},//hts -> 0x465 = 1125
	{0x0343, 0x65},//
	{0x0259, 0x01},
	{0x025a, 0x8a},
	{0x0340, 0x01},//vts -> 0x190 = 400
	{0x0341, 0x90},//
	{0x0346, 0x01},
	{0x0347, 0x68},
	{0x0348, 0x0a},
	{0x0349, 0x08},
	{0x034a, 0x02},
	{0x034b, 0xd8},
	{0x0094, 0x0a},
	{0x0095, 0x00},
	{0x0096, 0x02},
	{0x0097, 0xd0},
	{0x0099, 0x04},
	{0x009b, 0x04},
	{0x0218, 0x10},
	{0x0077, 0x08},
	{0x060c, 0x01},
	{0x060e, 0x08},
	{0x060f, 0x05},
	{0x070c, 0x01},
	{0x070e, 0x08},
	{0x070f, 0x05},
	{0x0909, 0x03},
	{0x0902, 0x04},
	{0x0904, 0x0b},
	{0x0907, 0x54},
	{0x0908, 0x06},
	{0x0903, 0x9d},
	{0x072a, 0x18},
	{0x0724, 0x0a},
	{0x0727, 0x0a},
	{0x072a, 0x1c},
	{0x072b, 0x0a},
	{0x1466, 0x10},
	{0x1468, 0x12},
	{0x1467, 0x1a},
	{0x1469, 0x80},
	{0x146a, 0xe8},
	{0x0707, 0x07},
	{0x0737, 0x0f},
	{0x0704, 0x01},
	{0x0706, 0x02},
	{0x0716, 0x02},
	{0x0708, 0xc8},
	{0x0718, 0xc8},
	{0x061a, 0x00},
	{0x1430, 0x80},
	{0x1407, 0x10},
	{0x1408, 0x16},
	{0x1409, 0x03},
	{0x146d, 0x0e},
	{0x146e, 0x42},
	{0x146f, 0x43},
	{0x1470, 0x3c},
	{0x1471, 0x3d},
	{0x1472, 0x3a},
	{0x1473, 0x3a},
	{0x1474, 0x40},
	{0x1475, 0x46},
	{0x1420, 0x14},
	{0x1464, 0x15},
	{0x146c, 0x40},
	{0x146d, 0x40},
	{0x1423, 0x08},
	{0x1428, 0x10},
	{0x1462, 0x18},
	{0x02ce, 0x04},
	{0x143a, 0x0f},
	{0x142b, 0x88},
	{0x0245, 0xc9},
	{0x023a, 0x08},
	{0x02cd, 0x99},
	{0x0612, 0x02},
	{0x0613, 0xc7},
	{0x0243, 0x03},
	{0x021b, 0x09},
	{0x0089, 0x03},
	{0x0040, 0xa3},
	{0x0075, 0x64},
	{0x0004, 0x0f},
	{0x0002, 0xab},
	{0x0053, 0x0a},
	{0x0205, 0x0c},
	{0x0202, 0x06},
	{0x0203, 0x27},
	{0x0614, 0x00},
	{0x0615, 0x00},
#if 1
	{0x0218, 0x00},
	{0x00b8, 0x10},
	{0x00b9, 0x00},
#endif
	{0x0181, 0x0c},
	{0x0182, 0x05},
	{0x0185, 0x01},
	{0x0180, 0x46},
	{0x0100, 0x08},
	{0x0106, 0x38},
	{0x010d, 0x40},
	{0x010e, 0x06},
	{0x0113, 0x02},
	{0x0114, 0x01},
	{0x0115, 0x10},
	{0x0100, 0x09},
	{0x0052, 0x02},
	{0x0076, 0x01},
	{0x021a, 0x10},
	{0x0434, 0x75},
	{0x0435, 0x75},
	{0x0436, 0x75},
	{0x0437, 0x75},
	{0x0430, 0x0a},
	{0x0431, 0x0a},
	{0x0432, 0x0a},
	{0x0433, 0x0a},
	{0x0458, 0x00},
	{0x0459, 0x00},
	{0x045a, 0x00},
	{0x045b, 0x00},
	{0x0a67, 0x80},
	{0x0a54, 0x0e},
	{0x0a65, 0x10},
	{0x0a98, 0x10},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0029, 0x08},
	{0x002b, 0xa8},
	{0x0a83, 0xe0},
	{0x0a72, 0x02},
	{0x0a73, 0x60},
	{0x0a75, 0x41},
	{0x0a70, 0x03},
	{0x0a5a, 0x80},
	{SENSOR_REG_DELAY, 0x14}, //delay 20ms
	{0x05be, 0x01},
	{0x0a70, 0x00},
	{0x0080, 0x02},
	{0x0a67, 0x00},
	{SENSOR_REG_END, 0x00},
};


/*
 * the order of the jxf23_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* [0] 2560*1440 @max 30fps*/
	{
		.width = 2560,
		.height = 1440,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2560_1440_25fps_mipi,
	},
	{
		.width = 2560,
		.height = 1440,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2560_1440_25fps_24Mmipi,
	},
	{
		.width = 2560,
		.height = 1440,
		.fps = 20 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2560_1440_20fps_mipi,
	},
	{
		.width = 2560,
		.height = 1440,
		.fps = 15 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2560_1440_15fps_mipi,
	},
	{
		.width = 1280,
		.height = 360,
		.fps = 120 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1280_360_120fps_24M_mipi,
	},
};

struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sensor_stream_on[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg,
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

#if 1

static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sensor_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		printk("=======>vals->reg_num:0x%04x, vals->value:0x%02x\n", vals->reg_num, val);
		vals++;
	}

    return 0;
}
#endif

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
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
	unsigned char v;
	int ret;
	ret = sensor_read(sd, 0x03f0, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	ret = sensor_read(sd, 0x03f1, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

#if 1

static int sensor_set_expo(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	int expo = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;
	struct again_lut *val_lut = sensor_again_lut;

	/* printk("it is %d, again is %d\n",expo,again); */
	/*expo*/
	ret = sensor_write(sd, 0x0203, expo & 0xff);
	ret += sensor_write(sd, 0x0202, expo >> 8);
	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d\n", __LINE__);
		return ret;
	}

	/*gain*/
	if (again > ht_gain) again = ht_gain;

	ret = sensor_write(sd, 0x0614, val_lut[again].reg614);
	ret = sensor_write(sd, 0x0615, val_lut[again].reg615);

	ret = sensor_write(sd, 0x0218, val_lut[again].reg218);
	ret = sensor_write(sd, 0x1467, val_lut[again].reg1467);
	ret = sensor_write(sd, 0x1468, val_lut[again].reg1468);
	ret = sensor_write(sd, 0x00b8, val_lut[again].regb8);
	ret = sensor_write(sd, 0x00b9, val_lut[again].regb9);
	ag_last = again;

	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d\n", __LINE__);
		return ret;
	}

    return 0;
}

#endif

static int resume_expo = 0;
static unsigned int resume_vts = 0;

static int sensor_resume(struct tx_isp_subdev *sd) {
	int ret = 0;
	int expo = (resume_expo & 0xffff);
	int again = (resume_expo & 0xffff0000) >> 16;
	struct again_lut *val_lut = sensor_again_lut;

	/* printk("it is %d, again is %d\n",expo,again); */
	/*expo*/
	ret = sensor_write(sd, 0x0203, expo & 0xff);
	ret += sensor_write(sd, 0x0202, expo >> 8);
	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d\n", __LINE__);
		return ret;
	}

	/*gain*/

	ret = sensor_write(sd, 0x0614, val_lut[again].reg614);
	ret = sensor_write(sd, 0x0615, val_lut[again].reg615);

	ret = sensor_write(sd, 0x0218, val_lut[again].reg218);
	ret = sensor_write(sd, 0x1467, val_lut[again].reg1467);
	ret = sensor_write(sd, 0x1468, val_lut[again].reg1468);
	ret = sensor_write(sd, 0x00b8, val_lut[again].regb8);
	ret = sensor_write(sd, 0x00b9, val_lut[again].regb9);

	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d", __LINE__);
		return ret;
	}
	//set fps
	if (resume_vts) {
		ret = sensor_write(sd, 0x0340, (unsigned char) ((resume_vts & 0x3f00) >> 8));
		ret += sensor_write(sd, 0x0341, (unsigned char) (resume_vts & 0xff));
		if (ret < 0)
			return -1;
	}

	return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
    int ret = 0;

    ret = sensor_write(sd, 0x0203, value & 0xff);
    ret += sensor_write(sd, 0x0202, value >> 8);
    if (ret < 0) {
	ISP_ERROR("sensor_write error  %d\n",__LINE__ );
	return ret;
    }

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
    int ret = 0;
    struct again_lut *val_lut = sensor_again_lut;


    ret = sensor_write(sd, 0x0614, val_lut[value].reg614);
    ret = sensor_write(sd, 0x0615, val_lut[value].reg615);

    ret = sensor_write(sd, 0x0218, val_lut[value].reg218);
    ret = sensor_write(sd, 0x1467, val_lut[value].reg1467);
    ret = sensor_write(sd, 0x1468, val_lut[value].reg1468);
    ret = sensor_write(sd, 0x00b8, val_lut[value].regb8);
    ret = sensor_write(sd, 0x00b9, val_lut[value].regb9);


    if (ret < 0) {
	ISP_ERROR("sensor_write error  %d" ,__LINE__ );
	return ret;
    }

	return 0;
}

#endif

static int sensor_set_logic(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	unsigned char reg_410 = 0;
	struct again_lut *val_lut = sensor_again_lut;

	ret = sensor_read(sd, 0x0410, &reg_410);

	if (ret < 0)
		return ret;

	if (reg_410 > 0x1a) gain_flag = 1; //0x1d

	if (gain_flag == 1) {
		if (reg_410 > 0x16)//0x14
		{
			ht_gain = ht_gain - 1;
			if (ht_gain < 21) ht_gain = 21;
		}

		if (reg_410 < 0x10) {
			ht_gain = ht_gain + 1;
			if (ht_gain > 24) {
				ht_gain = 24;
				gain_flag = 0;
			}
		}
	}

	if (ag_last < 8) {
		ht_gain = 24;
		gain_flag = 0;
	}

	if (ag_last > ht_gain) {
		ag_last = ht_gain;
		ret = sensor_write(sd, 0x0614, val_lut[ag_last].reg614);
		ret += sensor_write(sd, 0x0615, val_lut[ag_last].reg615);
		ret += sensor_write(sd, 0x0218, val_lut[ag_last].reg218);
		ret += sensor_write(sd, 0x1467, val_lut[ag_last].reg1467);
		ret += sensor_write(sd, 0x1468, val_lut[ag_last].reg1468);
		ret += sensor_write(sd, 0x00b8, val_lut[ag_last].regb8);
		ret += sensor_write(sd, 0x00b9, val_lut[ag_last].regb9);
	}

	//  pr_debug("gc4653_set_logic:gain_flag=%d,ag_last=%d\n",gain_flag,ag_last);

	//  pr_debug("gc4653_set_logic:reg_410=%d,gain_flag=%d,ag_last=%d,ht_gain=%d\n",reg_410,gain_flag,ag_last,ht_gain);
	return 0;
}

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

	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable)
		return ISP_SUCCESS;
	/* sensor_read_array(sd, wsize->regs); */
	sensor_set_attr(sd, wsize);
	sensor->video.state = TX_ISP_MODULE_INIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
#ifndef SENSOR_WITHOUT_INIT
			//When SENSOR_WITHOUT_INIT is enabled, the sensor will not be initialized by default.
			ret = sensor_write_array(sd, wsize->regs);
			if (ret)
			    return ret;
#endif
#ifdef SENSOR_POWER_OFF
			// If enable=2, it represents a PM operation.
			// It is necessary to restore the relevant registers of the sensor to the state before sleep.
			if (2 == init->enable) {
				ret = sensor_write_array(sd, wsize->regs);
				if (ret < 0)
					return ret;
				ret = sensor_resume(sd);
				if (ret < 0)
					return ret;
			}
#endif

			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if (sensor->video.state == TX_ISP_MODULE_RUNNING) {
			ret = sensor_write_array(sd, sensor_stream_on);
			ISP_WARNING("%s stream on\n", SENSOR_NAME);
		}
	} else {
		ret = sensor_write_array(sd, sensor_stream_off);
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
#ifdef SENSOR_POWER_OFF
		//Prepare for the next sleep.
		sensor->video.state = TX_ISP_MODULE_INIT;
#endif
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int wpclk = 0;
	unsigned int vts = 0;
	unsigned int hts = 0;
	unsigned char tmp;
	unsigned int sensor_max_fps;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	/* return 0; */
	switch (sensor->info.default_boot) {
		case 0:
		case 1:
			wpclk = SENSOR_SUPPORT_30FPS_SCLK;
			sensor_max_fps = TX_SENSOR_MAX_FPS_30;
			break;
		case 2:
			wpclk = SENSOR_SUPPORT_20FPS_SCLK;
			sensor_max_fps = TX_SENSOR_MAX_FPS_20;
			break;
		case 3:
			wpclk = SENSOR_SUPPORT_30FPS_SCLK;
			sensor_max_fps = TX_SENSOR_MAX_FPS_15;
			break;
		case 4:
			wpclk = 1125 * 400 * 120 * 2;
			sensor_max_fps = TX_SENSOR_MAX_FPS_120;
			break;
		default:
			ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	/* the format of fps is 16/16. for example 30 << 16 | 2, the value is 30/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (sensor_max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}

	ret += sensor_read(sd, 0x0342, &tmp);
	hts = tmp & 0x0f;
	ret += sensor_read(sd, 0x0343, &tmp);
	if (ret < 0)
		return -1;
	hts = ((hts << 8) + tmp) << 1;

	vts = wpclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = sensor_write(sd, 0x0340, (unsigned char) ((vts & 0x3f00) >> 8));
	ret += sensor_write(sd, 0x0341, (unsigned char) (vts & 0xff));
	if (ret < 0)
		return -1;

	resume_vts = vts;
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 8;
	sensor->video.attr->integration_time_limit = vts - 8;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 8;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = -1;
	unsigned char val = 0x0;
	unsigned char otp_val = 0x0;
	ret += sensor_read(sd, 0x022c, &val);

	val = (val & (~0x3));
	/* 2'b01 mirror; 2'b10 flip; 2'b11 mirror &flip */
	switch (enable) {
		case 0:
			sensor_write(sd, 0x022c, val);
			break;
		case 1:
			sensor_write(sd, 0x022c, (val | 0x1));
			break;
		case 2:
			sensor_write(sd, 0x022c, (val | 0x2));
			break;
		case 3:
			sensor_write(sd, 0x022c, (val | 0x3));
			break;
	}
	otp_val = 0x60 | val;
	ret += sensor_write(sd, 0x0a67, 0x80);
	ret += sensor_write(sd, 0x0a54, 0x0e);
	ret += sensor_write(sd, 0x0a65, 0x10);
	ret += sensor_write(sd, 0x0a98, 0x10);
	ret += sensor_write(sd, 0x05be, 0x00);
	ret += sensor_write(sd, 0x05a9, 0x01);
	ret += sensor_write(sd, 0x0029, 0x08);
	ret += sensor_write(sd, 0x002b, 0xa8);
	ret += sensor_write(sd, 0x0a83, 0xe0);
	ret += sensor_write(sd, 0x0a72, 0x02);
	ret += sensor_write(sd, 0x0a73, otp_val);
	ret += sensor_write(sd, 0x0a75, 0x41);
	ret += sensor_write(sd, 0x0a70, 0x03);
	ret += sensor_write(sd, 0x0a5a, 0x80);
	private_msleep(20);
	ret += sensor_write(sd, 0x05be, 0x01);
	ret += sensor_write(sd, 0x0a70, 0x00);
	ret += sensor_write(sd, 0x0080, 0x02);
	ret += sensor_write(sd, 0x0a67, 0x00);

	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_attr_check(struct tx_isp_subdev *sd) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	unsigned long rate;
	int ret = 0;

	switch (info->default_boot) {
		case 0:
			wsize = &sensor_win_sizes[0];
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			memcpy(&sensor_attr.mipi, &sensor_mipi_linear, sizeof(sensor_mipi_linear));
			sensor_attr.one_line_expr_in_us = 22;
			sensor_attr.total_width = 3840;
			sensor_attr.total_height = 1500;
			sensor_attr.max_integration_time_native = 1500 - 8;
			sensor_attr.integration_time_limit = 1500 - 8;
			sensor_attr.max_integration_time = 1500 - 8;
			sensor_attr.again = 0;
			sensor_attr.integration_time = 0x627;
			break;
		case 1:
			wsize = &sensor_win_sizes[1];
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			memcpy(&sensor_attr.mipi, &sensor_mipi_linear, sizeof(sensor_mipi_linear));
			sensor_attr.one_line_expr_in_us = 22;
			sensor_attr.total_width = 3840;
			sensor_attr.total_height = 1500;
			sensor_attr.max_integration_time_native = 1500 - 8;
			sensor_attr.integration_time_limit = 1500 - 8;
			sensor_attr.max_integration_time = 1500 - 8;
			sensor_attr.again = 0;
			sensor_attr.integration_time = 0x627;
			break;
		case 2:
			wsize = &sensor_win_sizes[2];
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			memcpy(&sensor_attr.mipi, &sensor_mipi_20fps_linear, sizeof(sensor_mipi_20fps_linear));
			sensor_attr.one_line_expr_in_us = 33;
			sensor_attr.total_width = 2400;
			sensor_attr.total_height = 1500;
			sensor_attr.max_integration_time_native = 1500 - 8;
			sensor_attr.integration_time_limit = 1500 - 8;
			sensor_attr.max_integration_time = 1500 - 8;
			sensor_attr.again = 0;
			sensor_attr.integration_time = 0x627;
			break;
		case 3:
			wsize = &sensor_win_sizes[3];
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			memcpy(&sensor_attr.mipi, &sensor_mipi_15fps_linear, sizeof(sensor_mipi_15fps_linear));
			sensor_attr.one_line_expr_in_us = 44;
			sensor_attr.total_width = 2400;
			sensor_attr.total_height = 1500;
			sensor_attr.max_integration_time_native = 1500 - 8;
			sensor_attr.integration_time_limit = 1500 - 8;
			sensor_attr.max_integration_time = 1500 - 8;
			sensor_attr.again = 0;
			sensor_attr.integration_time = 0x627;
			break;
		case 4:
			wsize = &sensor_win_sizes[4];
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			memcpy(&sensor_attr.mipi, &sensor_mipi_120fps_linear, sizeof(sensor_mipi_120fps_linear));
			sensor_attr.one_line_expr_in_us = 44;
			sensor_attr.total_width = 2250;
			sensor_attr.total_height = 400;
			sensor_attr.max_integration_time_native = 400 - 8;
			sensor_attr.integration_time_limit = 400 - 8;
			sensor_attr.max_integration_time = 400 - 8;
			sensor_attr.again = 0;
			sensor_attr.integration_time = 0x627;
			break;
		default:
			ISP_ERROR("Have no this Setting Source!!!\n");
	}

	switch (info->video_interface) {
		case TISP_SENSOR_VI_MIPI_CSI0:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.mipi.index = 0;
			break;
		case TISP_SENSOR_VI_DVP:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
			break;
		default:
			ISP_ERROR("Have no this Interface Source!!!\n");
	}
#ifndef SENSOR_WITHOUT_INIT
	//struct clk *tclk;
	//struct clk *sclka;
	//unsigned long rate;
	switch(info->mclk) {
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

	    rate = private_clk_get_rate(sensor->mclk);
	    printk("\n====>[private_clk_get_rate(sensor->mclk)=%ld]\n", rate);
	    switch(info->default_boot) {
	    case 0:
		    case 2:
		    case 3:
		    if (((rate / 1000) % 27000) != 0) {
			    ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
			    printk("\n====>[clk_set_parent(sclka, clk_get(NULL, SEN_TCLK:%s))=%d]\n", SEN_TCLK, ret);
			    sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
			    if (IS_ERR(sclka)) {
				    pr_err("get sclka failed\n");
			    } else {
				    rate = private_clk_get_rate(sclka);
				    printk("\n====>[private_clk_get_rate(sclka)=%ld]\n", rate);
				    if (((rate / 1000) % 27000) != 0) {
					    private_clk_set_rate(sclka, 1080000000);
				    }
			    }
		    }
		    private_clk_set_rate(sensor->mclk, 27000000);
		    private_clk_prepare_enable(sensor->mclk);
		    break;

		    case 1:
		    case 4:
		    private_clk_set_rate(sensor->mclk, 24000000);
		    private_clk_prepare_enable(sensor->mclk);
		    break;
	    }
#endif

	ISP_WARNING("\n====>[default_boot=%d] [resolution=%dx%d] [video_interface=%d] [MCLK=%d] \n", info->default_boot,
		    wsize->width, wsize->height, info->video_interface, info->mclk);
	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;
	sensor->video.fps = wsize->fps;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

	return 0;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
#ifndef SENSOR_WITHOUT_INIT
	if (reset_gpio != -1) {
	    ret = private_gpio_request(reset_gpio,"sensor_reset");
	    if (!ret) {
		private_gpio_direction_output(reset_gpio, 1);
		private_msleep(10);
		private_gpio_direction_output(reset_gpio, 0);
		private_msleep(20);
		private_gpio_direction_output(reset_gpio, 1);
		private_msleep(10);
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
		private_gpio_direction_output(pwdn_gpio, 1);
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
#else
	ident = 0x4653;
#endif
	ISP_WARNING("%s chip found @ 0x%02x (%s)\n",
		    SENSOR_NAME, client->addr, client->adapter->name);
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
#if 1
		case TX_ISP_EVENT_SENSOR_EXPO:
			if (arg)
				ret = sensor_set_expo(sd, sensor_val->value);
			break;
#else
			case TX_ISP_EVENT_SENSOR_INT_TIME:
				      if (arg)
					      ret = sensor_set_integration_time(sd, sensor_val->value);
				break;
			case TX_ISP_EVENT_SENSOR_AGAIN:
				      if (arg)
					      ret = sensor_set_analog_gain(sd, sensor_val->value);
				break;
#endif
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
		case TX_ISP_EVENT_SENSOR_LOGIC:
			if (arg)
				ret = sensor_set_logic(sd, *(int *) arg);
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

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id) {
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *) kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));

	sensor->dev = &client->dev;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor_attr.expo_fs = 1;
	sensor->video.attr = &sensor_attr;
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
//MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver = {
		.owner = NULL,
		.name = SENSOR_NAME,
	},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

char *get_sensor_name(void) {
	return SENSOR_NAME;
}

int get_sensor_i2c_addr(void) {
	return 0x29;// Caller
}

int get_sensor_width(void) {
	return sensor_win_sizes->width;
}

int get_sensor_height(void) {
	return sensor_win_sizes->height;
}

int get_sensor_wdr_mode(void) {
	return 0;
}

int init_sensor(void) {
	return private_i2c_add_driver(&sensor_driver);
}

void exit_sensor(void) {
	i2c_del_driver(&sensor_driver);
}
