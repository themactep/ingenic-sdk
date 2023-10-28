/*
 * gc4653.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG
#define __WDR__

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

#define GC4653_CHIP_ID_H			(0x46)
#define GC4653_CHIP_ID_L			(0x53)
#define GC4653_REG_END				0xffff
#define GC4653_REG_DELAY			0x0000
#define GC4653_SUPPORT_386RES_30FPS_SCLK	(144 * 1000 * 1000)
#define GC4653_SUPPORT_207RES_60FPS_SCLK	(0x4b0 * 0x4e2 * 60 * 2)
#define GC4653_SUPPORT_92RES_60FPS_SCLK		(0x5dd * 0x640 * 60 * 2)
#define SENSOR_OUTPUT_MIN_FPS			5
#define SENSOR_VERSION				"H20211211a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_30;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
module_param(data_type, int, S_IRUGO);
MODULE_PARM_DESC(data_type, "Sensor Date Type");

static int wdr_bufsize = 10077696 * 2;//cache lines corrponding on VPB1
module_param(wdr_bufsize, int, S_IRUGO);
MODULE_PARM_DESC(wdr_bufsize, "Wdr Buf Size");

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static int sensor_resolution = TX_SENSOR_RES_400;
module_param(sensor_resolution, int, S_IRUGO);
MODULE_PARM_DESC(sensor_resolution, "Sensor resolution");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	unsigned int index;
	unsigned char reg2b3;
	unsigned char reg2b4;
	unsigned char reg2b8;
	unsigned char reg2b9;
	unsigned char reg515;
	unsigned char reg519;
	unsigned char reg2d9;
	unsigned int gain;
};

struct again_lut gc4653_again_lut[] = {
	{0x00, 0x00, 0x00, 0x01, 0x00, 0x30, 0x1E, 0x5C, 0},
	{0x01, 0x20, 0x00, 0x01, 0x0B, 0x30, 0x1E, 0x5C, 14995},
	{0x02, 0x01, 0x00, 0x01, 0x19, 0x30, 0x1D, 0x5B, 31177},
	{0x03, 0x21, 0x00, 0x01, 0x2A, 0x30, 0x1E, 0x5C, 47704},
	{0x04, 0x02, 0x00, 0x02, 0x00, 0x30, 0x1E, 0x5C, 65535},
	{0x05, 0x22, 0x00, 0x02, 0x17, 0x30, 0x1D, 0x5B, 81158},
	{0x06, 0x03, 0x00, 0x02, 0x33, 0x20, 0x16, 0x54, 97241},
	{0x07, 0x23, 0x00, 0x03, 0x14, 0x20, 0x17, 0x55, 113239},
	{0x08, 0x04, 0x00, 0x04, 0x00, 0x20, 0x17, 0x55, 131070},
	{0x09, 0x24, 0x00, 0x04, 0x2F, 0x20, 0x19, 0x57, 147006},
	{0x0A, 0x05, 0x00, 0x05, 0x26, 0x20, 0x19, 0x57, 162776},
	{0x0B, 0x25, 0x00, 0x06, 0x28, 0x20, 0x1B, 0x59, 178774},
	{0x0C, 0x0C, 0x00, 0x08, 0x00, 0x20, 0x1D, 0x5B, 196605},
	{0x0D, 0x2C, 0x00, 0x09, 0x1E, 0x20, 0x1F, 0x5D, 212541},
	{0x0E, 0x0D, 0x00, 0x0B, 0x0C, 0x20, 0x21, 0x5F, 228311},
	{0x0F, 0x2D, 0x00, 0x0D, 0x11, 0x20, 0x24, 0x62, 244420},
	{0x10, 0x1C, 0x00, 0x10, 0x00, 0x20, 0x26, 0x64, 262140},
	{0x11, 0x3C, 0x00, 0x12, 0x3D, 0x18, 0x2A, 0x68, 278154},
	{0x12, 0x5C, 0x00, 0x16, 0x19, 0x18, 0x2C, 0x6A, 293912},
	{0x13, 0x7C, 0x00, 0x1A, 0x22, 0x18, 0x2E, 0x6C, 309955},
	{0x14, 0x9C, 0x00, 0x20, 0x00, 0x18, 0x32, 0x70, 327675},
	{0x15, 0xBC, 0x00, 0x25, 0x3A, 0x18, 0x35, 0x73, 343689},
	{0x16, 0xDC, 0x00, 0x2C, 0x33, 0x10, 0x36, 0x74, 359480},
	{0x17, 0xFC, 0x00, 0x35, 0x05, 0x10, 0x38, 0x76, 375518},
	{0x18, 0x1C, 0x01, 0x40, 0x00, 0x10, 0x3C, 0x7A, 393210},
	{0x19, 0x3C, 0x01, 0x4B, 0x35, 0x10, 0x42, 0x80, 409243},
};

struct tx_isp_sensor_attribute gc4653_attr;

unsigned int gc4653_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;
#if 0
	if(data_type == TX_SENSOR_DATA_TYPE_WDR_DOL){
		if (expo % 2 == 0)
			expo = expo - 1;
		if(expo < gc4653_attr.min_integration_time)
			expo = 3;
	}
	isp_it = expo << shift;
#endif
	*sensor_it = expo;

	return isp_it;
}
unsigned int gc4653_alloc_integration_time_short(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;
#if 0
	if (expo % 2 == 0)
		expo = expo - 1;
	if(expo < gc4653_attr.min_integration_time_short)
		expo = 3;
	isp_it = expo << shift;
	expo = (expo - 1) / 2;
	if(expo < 0)
		expo = 0;
#endif
	*sensor_it = expo;

	return isp_it;
}

unsigned int gc4653_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = gc4653_again_lut;
	while(lut->gain <= gc4653_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return lut[0].gain;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == gc4653_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
				return lut->gain;
			}
		}

		lut++;
	}

	return 0;
}

unsigned int gc4653_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = gc4653_again_lut;
	while(lut->gain <= gc4653_attr.max_again_short) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->gain;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == gc4653_attr.max_again_short) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int gc4653_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus gc4653_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 648,
	.lans = 2,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
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
	.mipi_sc.data_type_value = 0,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus gc4653_mipi_dol = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 648,
	.lans = 2,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
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
	.mipi_sc.data_type_value = 0,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_sensor_attribute gc4653_attr = {
	.name = "gc4653",
	.chip_id = 0x4653,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x29,
	.max_again = 409243,
	.max_dgain = 0,
	.expo_fs = 1,
	.min_integration_time = 4,
	.min_integration_time_short = 4,
	.min_integration_time_native = 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = gc4653_alloc_again,
	.sensor_ctrl.alloc_again_short = gc4653_alloc_again_short,
	.sensor_ctrl.alloc_dgain = gc4653_alloc_dgain,
	.sensor_ctrl.alloc_integration_time_short = gc4653_alloc_integration_time_short,
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
static struct regval_list gc4653_init_regs_2560_1440_25fps_mipi[] = {
	/* SYS */
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x0317, 0x00},
	{0x0320, 0x77},
	{0x0324, 0xc8},
	{0x0325, 0x06},
	{0x0326, 0x60},
	{0x0327, 0x03},
	{0x0334, 0x40},
	{0x0336, 0x60},
	{0x0337, 0x82},
	{0x0315, 0x25},
	{0x031c, 0xc6},
	{0x0287, 0x18},
	{0x0084, 0x00},
	{0x0087, 0x50},
	{0x029d, 0x08},
	{0x0290, 0x00},
	{0x0340, 0x06},//vts
	{0x0341, 0x40},
	{0x0345, 0x06},
	{0x034b, 0xb0},
	{0x0352, 0x08},
	{0x0354, 0x08},
	{0x02d1, 0xe0},
	{0x0223, 0xf2},
	{0x0238, 0xa4},
	{0x02ce, 0x7f},
	{0x0232, 0xc4},
	{0x02d3, 0x05},
	{0x0243, 0x06},
	{0x02ee, 0x30},
	{0x026f, 0x70},
	{0x0257, 0x09},
	{0x0211, 0x02},
	{0x0219, 0x09},
	{0x023f, 0x2d},
	{0x0518, 0x00},
	{0x0519, 0x01},
	{0x0515, 0x08},
	{0x02d9, 0x3f},
	{0x02da, 0x02},
	{0x02db, 0xe8},
	{0x02e6, 0x20},
	{0x021b, 0x10},
	{0x0252, 0x22},
	{0x024e, 0x22},
	{0x02c4, 0x01},
	{0x021d, 0x17},
	{0x024a, 0x01},
	{0x02ca, 0x02},
	{0x0262, 0x10},
	{0x029a, 0x20},
	{0x021c, 0x0e},
	{0x0298, 0x03},
	{0x029c, 0x00},
	{0x027e, 0x14},
	{0x02c2, 0x10},
	{0x0540, 0x20},
	{0x0546, 0x01},
	{0x0548, 0x01},
	{0x0544, 0x01},
	{0x0242, 0x1b},
	{0x02c0, 0x1b},
	{0x02c3, 0x40},/*0x20 -> 0x40*/
	{0x02e4, 0x10},
	{0x022e, 0x00},
	{0x027b, 0x3f},
	{0x0269, 0x0f},
	{0x02d2, 0x40},
	{0x027c, 0x08},
	{0x023a, 0x2e},
	{0x0245, 0xce},
	{0x0530, 0x20},
	{0x0531, 0x02},
	{0x0228, 0x50},
	{0x02ab, 0x00},
	{0x0250, 0x00},
	{0x0221, 0x50},
	{0x02ac, 0x00},
	{0x02a5, 0x02},
	{0x0260, 0x0b},
	{0x0216, 0x04},
	{0x0299, 0x1C},
	{0x02bb, 0x0d},
	{0x02a3, 0x02},
	{0x02a4, 0x02},
	{0x021e, 0x02},
	{0x024f, 0x08},
	{0x028c, 0x08},
	{0x0532, 0x3f},
	{0x0533, 0x02},
	{0x0277, 0xc0},
	{0x0276, 0xc0},
	{0x0239, 0xc0},
	{0x0202, 0x05},
	{0x0203, 0xd0},
	{0x0205, 0xc0},
	{0x02b0, 0x68},
	{0x0002, 0xa9},
	{0x0004, 0x01},
	{0x021a, 0x98},
	{0x0266, 0xa0},
	{0x0020, 0x01},
	{0x0021, 0x03},
	{0x0022, 0x00},
	{0x0023, 0x04},
	{0x0342, 0x05},//hts
	{0x0343, 0xdc},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x0106, 0x78},
	{0x0108, 0x0c},
	{0x0114, 0x01},
	{0x0115, 0x12},
	{0x0180, 0x46},
	{0x0181, 0x30},
	{0x0182, 0x05},
	{0x0185, 0x01},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x0100, 0x09},

	{0x000f, 0x00},

	/* OTP */
	{0x0080,0x02},
	{0x0097,0x0a},
	{0x0098,0x10},
	{0x0099,0x05},
	{0x009a,0xb0},
	{0x0317,0x08},
	{0x0a67,0x80},
	{0x0a70,0x03},
	{0x0a82,0x00},
	{0x0a83,0x10},
	{0x0a80,0x2b},
	{0x05be,0x00},
	{0x05a9,0x01},
	{0x0313,0x80},
	{0x05be,0x01},
	{0x0317,0x00},
	{0x0a67,0x00},

	{GC4653_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc4653_init_regs_2560_1440_15fps_mipi_dol[] = {
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x0317, 0x00},
	{0x0320, 0x77},
	{0x0324, 0xc8},
	{0x0325, 0x06},
	{0x0326, 0x60},
	{0x0327, 0x03},
	{0x0334, 0x40},
	{0x0336, 0x70},
	{0x0337, 0x82},
	{0x0315, 0x25},
	{0x031c, 0xc6},
	{0x0287, 0x18},
	{0x0084, 0x00},
	{0x0087, 0x50},
	{0x029d, 0x08},
	{0x0290, 0x00},
	{0x0340, 0x07},
	{0x0341, 0x08},
	{0x0345, 0x06},
	{0x034b, 0xb0},
	{0x0352, 0x08},
	{0x0354, 0x08},
	{0x02d1, 0xe0},
	{0x0223, 0xf2},
	{0x0238, 0xa4},
	{0x02ce, 0x7f},
	{0x0232, 0xc4},
	{0x02d3, 0x05},
	{0x0243, 0x06},
	{0x02ee, 0x30},
	{0x026f, 0x70},
	{0x0257, 0x09},
	{0x0211, 0x02},
	{0x0219, 0x09},
	{0x023f, 0x2d},
	{0x0518, 0x00},
	{0x0519, 0x01},
	{0x0515, 0x08},
	{0x02d9, 0x3f},
	{0x02da, 0x02},
	{0x02db, 0xe8},
	{0x02e6, 0x20},
	{0x021b, 0x10},
	{0x0252, 0x22},
	{0x024e, 0x22},
	{0x02c4, 0x01},
	{0x021d, 0x17},
	{0x024a, 0x01},
	{0x02ca, 0x02},
	{0x0262, 0x10},
	{0x029a, 0x20},
	{0x021c, 0x0e},
	{0x0298, 0x03},
	{0x029c, 0x00},
	{0x027e, 0x14},
	{0x02c2, 0x10},
	{0x0540, 0x20},
	{0x0546, 0x01},
	{0x0548, 0x01},
	{0x0544, 0x01},
	{0x0242, 0x1b},
	{0x02c0, 0x1b},
	{0x02c3, 0x20},
	{0x02e4, 0x10},
	{0x022e, 0x00},
	{0x027b, 0x3f},
	{0x0269, 0x0f},
	{0x02d2, 0x40},
	{0x027c, 0x08},
	{0x023a, 0x2e},
	{0x0245, 0xce},
	{0x0530, 0x20},
	{0x0531, 0x02},
	{0x0228, 0x50},
	{0x02ab, 0x00},
	{0x0250, 0x00},
	{0x0221, 0x50},
	{0x02ac, 0x00},
	{0x02a5, 0x02},
	{0x0260, 0x0b},
	{0x0216, 0x04},
	{0x0299, 0x1C},
	{0x02bb, 0x0d},
	{0x02a3, 0x02},
	{0x02a4, 0x02},
	{0x021e, 0x02},
	{0x024f, 0x08},
	{0x028c, 0x08},
	{0x0532, 0x3f},
	{0x0533, 0x02},
	{0x0277, 0xc0},
	{0x0276, 0xc0},
	{0x0239, 0xc0},
	{0x0202, 0x05},
	{0x0203, 0xd0},
	{0x0205, 0xc0},
	{0x02b0, 0x68},

	{0x0002, 0xa9},
	{0x0004, 0x01},
	{0x021a, 0x98},
	{0x0266, 0xa0},
	{0x0020, 0x01},
	{0x0021, 0x03},
	{0x0022, 0x00},
	{0x0023, 0x04},
	{0x0342, 0x05},
	{0x0343, 0x35},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x0218, 0x12},
	{0x0106, 0x78},
	{0x0107, 0x89},
	{0x0108, 0x0c},
	{0x0114, 0x01},
	{0x0115, 0x12},
	{0x0180, 0x46},
	{0x0181, 0x30},
	{0x0182, 0x05},
	{0x0185, 0x01},
	{0x03fe, 0x00},
	{0x0100, 0x09},
	{0x000f, 0x00},

	/* OTP */
	{0x0080, 0x02},
	{0x0097, 0x0a},
	{0x0098, 0x10},
	{0x0099, 0x05},
	{0x009a, 0xb0},
	{0x0317, 0x08},
	{0x0a67, 0x80},
	{0x0a70, 0x03},
	{0x0a82, 0x00},
	{0x0a83, 0x10},
	{0x0a80, 0x2b},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0313, 0x80},
	{0x05be, 0x01},
	{0x0317, 0x00},
	{0x0a67, 0x00},

	{GC4653_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc4653_init_regs_1280_720_60fps_mipi[] = {
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x0317, 0x00},
	{0x0320, 0x77},
	{0x0324, 0xc8},
	{0x0325, 0x06},
	{0x0326, 0x60},
	{0x0327, 0x03},
	{0x0334, 0x40},
	{0x0336, 0x60},
	{0x0337, 0x82},
	{0x0335, 0x55},
	{0x0315, 0x25},
	{0x031c, 0xc6},
	{0x0287, 0x18},
	{0x0084, 0x00},
	{0x0087, 0x50},
	{0x029d, 0x08},
	{0x0290, 0x00},
	{0x0217, 0x40},
	{0x0234, 0x20},
	{0x0340, 0x05},
	{0x0341, 0xdd},
	{0x0341, 0xd0},
	{0x0345, 0x06},
	{0x034b, 0xb0},
	{0x034c, 0x05},
	{0x034e, 0x02},
	{0x034f, 0xd0},
	{0x0352, 0x08},
	{0x0354, 0x08},
	{0x02d1, 0xe0},
	{0x0223, 0xf2},
	{0x0238, 0xa4},
	{0x02ce, 0x7f},
	{0x0232, 0xc4},
	{0x02d3, 0x05},
	{0x0243, 0x06},
	{0x02ee, 0x30},
	{0x026f, 0x70},
	{0x0257, 0x09},
	{0x0211, 0x02},
	{0x0219, 0x09},
	{0x023f, 0x2d},
	{0x0518, 0x00},
	{0x0519, 0x01},
	{0x0515, 0x08},
	{0x02d9, 0x3f},
	{0x02da, 0x02},
	{0x02db, 0xe8},
	{0x02e6, 0x20},
	{0x021b, 0x10},
	{0x0252, 0x22},
	{0x024e, 0x22},
	{0x02c4, 0x01},
	{0x021d, 0x17},
	{0x024a, 0x01},
	{0x02ca, 0x02},
	{0x0262, 0x10},
	{0x029a, 0x20},
	{0x021c, 0x0e},
	{0x0298, 0x03},
	{0x029c, 0x00},
	{0x027e, 0x14},
	{0x02c2, 0x10},
	{0x0540, 0x20},
	{0x0546, 0x01},
	{0x0548, 0x01},
	{0x0544, 0x01},
	{0x0242, 0x1b},
	{0x02c0, 0x1b},
	{0x02c3, 0x20},
	{0x02e4, 0x10},
	{0x022e, 0x00},
	{0x027b, 0x3f},
	{0x0269, 0x0f},
	{0x02d2, 0x40},
	{0x027c, 0x08},
	{0x023a, 0x2e},
	{0x0245, 0xce},
	{0x0530, 0x20},
	{0x0531, 0x02},
	{0x0228, 0x50},
	{0x02ab, 0x00},
	{0x0250, 0x00},
	{0x0221, 0x50},
	{0x02ac, 0x00},
	{0x02a5, 0x02},
	{0x0260, 0x0b},
	{0x0216, 0x04},
	{0x0299, 0x1C},
	{0x02bb, 0x0d},
	{0x02a3, 0x02},
	{0x02a4, 0x02},
	{0x021e, 0x02},
	{0x024f, 0x08},
	{0x028c, 0x08},
	{0x0532, 0x3f},
	{0x0533, 0x02},
	{0x0277, 0xc0},
	{0x0276, 0xc0},
	{0x0239, 0xc0},
	{0x0202, 0x05},
	{0x0203, 0xd0},
	{0x0205, 0xc0},
	{0x02b0, 0x68},
	{0x0002, 0xa9},
	{0x0004, 0x01},
	{0x021a, 0x98},
	{0x0266, 0xa0},
	{0x0020, 0x01},
	{0x0021, 0x03},
	{0x0022, 0x00},
	{0x0023, 0x04},
	{0x0342, 0x06},
	{0x0343, 0x40},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x0106, 0x78},
	{0x0108, 0x0c},
	{0x0114, 0x01},
	{0x0115, 0x12},
	{0x0180, 0x46},
	{0x0181, 0x30},
	{0x0182, 0x05},
	{0x0185, 0x01},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x0100, 0x09},

	{0x000f, 0x00},
	/* OTP */
	{0x0080, 0x02},
	{0x0097, 0x0a},
	{0x0098, 0x10},
	{0x0099, 0x05},
	{0x009a, 0xb0},
	{0x0317, 0x08},
	{0x0a67, 0x80},
	{0x0a70, 0x03},
	{0x0a82, 0x00},
	{0x0a83, 0x10},
	{0x0a80, 0x2b},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0313, 0x80},
	{0x05be, 0x01},
	{0x0317, 0x00},
	{0x0a67, 0x00},

	{GC4653_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc4653_init_regs_1920_1080_60fps_mipi[] = {
	{0x03fe,0xf0},
	{0x03fe,0x00},
	{0x0317,0x00},
	{0x0320,0x77},
	{0x0324,0xd0},
	{0x0325,0x06},
	{0x0326,0x87},
	{0x0327,0x03},
	{0x0334,0x40},
	{0x0335,0x51},
	{0x0336,0x87},
	{0x0337,0x82},
	{0x0315,0x25},
	{0x031c,0xc6},
	{0x0287,0x18},
	{0x0084,0x00},
	{0x0087,0x50},
	{0x029d,0x08},
	{0x0290,0x00},
	{0x0340,0x04},
	{0x0341,0xb0},

	/* WINDOW */
	{0x0344,0x02},
	{0x0345,0x86},
	{0x0346,0x00},
	{0x0347,0xb6},
	{0x0348,0x07},
	{0x0349,0x90},
	{0x034a,0x04},
	{0x034b,0x48},

	{0x0351,0x00},//crop
	{0x0352,0x08},
	{0x0353,0x00},
	{0x0354,0x08},
	{0x034c,0x07},
	{0x034d,0x80},
	{0x034e,0x04},
	{0x034f,0x38},

	{0x02d1,0xe0},
	{0x0223,0xf2},
	{0x0238,0xa4},
	{0x02ce,0x7f},
	{0x0232,0xc4},
	{0x02d3,0x05},
	{0x0243,0x06},
	{0x02ee,0x30},
	{0x026f,0x70},
	{0x0257,0x09},
	{0x0211,0x02},
	{0x0219,0x09},
	{0x023f,0x2d},
	{0x0518,0x00},
	{0x0519,0x01},
	{0x0515,0x08},
	{0x02d9,0x3f},
	{0x02da,0x02},
	{0x02db,0xe8},
	{0x02e6,0x20},
	{0x021b,0x10},
	{0x0252,0x22},
	{0x024e,0x22},
	{0x02c4,0x01},
	{0x021d,0x17},
	{0x024a,0x01},
	{0x02ca,0x02},
	{0x0262,0x10},
	{0x029a,0x20},
	{0x021c,0x0e},
	{0x0298,0x03},
	{0x029c,0x00},
	{0x027e,0x14},
	{0x02c2,0x10},
	{0x0540,0x20},
	{0x0546,0x01},
	{0x0548,0x01},
	{0x0544,0x01},
	{0x0242,0x1b},
	{0x02c0,0x1b},
	{0x02c3,0x20},
	{0x02e4,0x10},
	{0x022e,0x00},
	{0x027b,0x3f},
	{0x0269,0x0f},
	{0x000f,0x00},
	{0x02d2,0x40},
	{0x027c,0x08},
	{0x023a,0x2e},
	{0x0245,0xce},
	{0x0530,0x20},
	{0x0531,0x02},
	{0x0228,0x50},
	{0x02ab,0x00},
	{0x0250,0x00},
	{0x0221,0x50},
	{0x02ac,0x00},
	{0x02a5,0x02},
	{0x0260,0x0b},
	{0x0216,0x04},
	{0x0299,0x1C},
	{0x02bb,0x0d},
	{0x02a3,0x02},
	{0x02a4,0x02},
	{0x021e,0x02},
	{0x024f,0x08},
	{0x028c,0x08},
	{0x0532,0x3f},
	{0x0533,0x02},
	{0x0277,0xc0},
	{0x0276,0xc0},
	{0x0239,0xc0},
	{0x0202,0x05},
	{0x0203,0xd0},
	{0x0205,0xc0},
	{0x02b0,0x68},
	{0x0002,0xa9},
	{0x0004,0x01},
	{0x0005,0x8a},
	{0x0006,0xe0},
	{0x0007,0x65},
	{0x0008,0x66},
	{0x0009,0x56},
	{0x000a,0x55},
	{0x021a,0x98},
	{0x0266,0xa0},
	{0x0020,0x01},
	{0x0021,0x03},
	{0x0022,0x00},
	{0x0023,0x04},
	{0x0342,0x04},
	{0x0343,0xe2},
	{0x03fe,0x10},
	{0x03fe,0x00},
	{0x0106,0x78},
	{0x0108,0x0c},
	{0x0114,0x01},
	{0x0115,0x12},
	{0x0180,0x46},
	{0x0181,0x30},
	{0x0182,0x05},
	{0x0185,0x01},
	{0x03fe,0x10},
	{0x03fe,0x00},
	{0x0100,0x09},

	/* OTP */
	{0x0080,0x02},
	{0x0097,0x0a},
	{0x0098,0x10},
	{0x0099,0x05},
	{0x009a,0xb0},
	{0x0317,0x08},
	{0x0a67,0x80},
	{0x0a70,0x03},
	{0x0a82,0x00},
	{0x0a83,0x10},
	{0x0a80,0x2b},
	{0x05be,0x00},
	{0x05a9,0x01},
	{0x0313,0x80},
	{0x05be,0x01},
	{0x0317,0x00},
	{0x0a67,0x00},

	{GC4653_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the jxf23_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting gc4653_win_sizes[] = {
	/* 2560*1440 @ max 30fps mipi*/
	{
		.width		= 2560,
		.height		= 1440,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SGRBG10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= gc4653_init_regs_2560_1440_25fps_mipi,
	},
#ifdef __WDR__
	/* 2560*1440 @ max 15fps mipi*/
	{
		.width		= 2560,
		.height		= 1440,
		.fps		= 15 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SGRBG10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= gc4653_init_regs_2560_1440_15fps_mipi_dol,
	},
#endif
	/* 1280*720 @ max 60fps mipi*/
	{
		.width		= 1280,
		.height		= 720,
		.fps		= 60 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SGRBG10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= gc4653_init_regs_1280_720_60fps_mipi,
	},
	/* 1920*1080 @ max 60fps mipi*/
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 60 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SGRBG10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= gc4653_init_regs_1920_1080_60fps_mipi,
	},
};

struct tx_isp_sensor_win_setting *wsize = &gc4653_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list gc4653_stream_on[] = {
	{GC4653_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc4653_stream_off[] = {
	{GC4653_REG_END, 0x00},	/* END MARKER */
};

int gc4653_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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

int gc4653_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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
static int gc4653_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != GC4653_REG_END) {
		if (vals->reg_num == GC4653_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc4653_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}

	return 0;
}
#endif

static int gc4653_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != GC4653_REG_END) {
		if (vals->reg_num == GC4653_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc4653_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int gc4653_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int gc4653_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = gc4653_read(sd, 0x03f0, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != GC4653_CHIP_ID_H)
		return -ENODEV;
	ret = gc4653_read(sd, 0x03f1, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != GC4653_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int it_last = -1;
static int ag_last = -1;

#if 0
static int gc4653_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int expo = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	/* printk("it is %d, again is %d\n",expo,again); */

	/* expo */
	if(it_last != expo){
		ret = gc4653_write(sd, 0x0203, value & 0xff);
		ret += gc4653_write(sd, 0x0202, value >> 8);
		if (ret < 0) {
			ISP_ERROR("gc4653_write error  %d\n" ,__LINE__ );
			return ret;
		}
	}

	/* gain */
	if(ag_last != again){
		struct again_lut *val_lut = gc4653_again_lut;
		ret = gc4653_write(sd, 0x02b3, val_lut[value].reg2b3);
		ret = gc4653_write(sd, 0x02b4, val_lut[value].reg2b4);
		ret = gc4653_write(sd, 0x02b8, val_lut[value].reg2b8);
		ret = gc4653_write(sd, 0x02b9, val_lut[value].reg2b9);
		ret = gc4653_write(sd, 0x0515, val_lut[value].reg515);
		ret = gc4653_write(sd, 0x0519, val_lut[value].reg519);
		ret = gc4653_write(sd, 0x02d9, val_lut[value].reg2d9);
		if (ret < 0) {
			ISP_ERROR("gc4653_write error  %d" ,__LINE__ );
			return ret;
		}
	}

	it_last = expo;
	ag_last = again;

	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int gc4653_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = gc4653_write(sd, 0x0203, value & 0xff);
	ret += gc4653_write(sd, 0x0202, value >> 8);
	if (ret < 0) {
		ISP_ERROR("gc4653_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int gc4653_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	struct again_lut *val_lut = gc4653_again_lut;

	ret = gc4653_write(sd, 0x02b3, val_lut[value].reg2b3);
	ret += gc4653_write(sd, 0x02b4, val_lut[value].reg2b4);
	ret += gc4653_write(sd, 0x02b8, val_lut[value].reg2b8);
	ret += gc4653_write(sd, 0x02b9, val_lut[value].reg2b9);
	ret += gc4653_write(sd, 0x0515, val_lut[value].reg515);
	ret += gc4653_write(sd, 0x0519, val_lut[value].reg519);
	ret += gc4653_write(sd, 0x02d9, val_lut[value].reg2d9);
	if (ret < 0) {
		ISP_ERROR("gc4653_write error  %d" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int gc4653_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc4653_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc4653_init(struct tx_isp_subdev *sd, int enable)
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

	ret = gc4653_write_array(sd, wsize->regs);
	if (ret)
		return ret;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int gc4653_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = gc4653_write_array(sd, gc4653_stream_on);
		pr_debug("gc4653 stream on\n");
	} else {
		ret = gc4653_write_array(sd, gc4653_stream_off);
		pr_debug("gc4653 stream off\n");
	}

	return ret;
}

static int gc4653_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int wpclk = 0;
	unsigned short vts = 0;
	unsigned short hts=0;
	unsigned int max_fps = 0;
	unsigned char tmp;
	unsigned short short_time = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	wpclk = GC4653_SUPPORT_386RES_30FPS_SCLK;
	switch(sensor_resolution)
	{
		case TX_SENSOR_RES_400:
			wpclk = GC4653_SUPPORT_386RES_30FPS_SCLK;
			break;
		case TX_SENSOR_RES_100:
			wpclk = GC4653_SUPPORT_92RES_60FPS_SCLK;
			break;
		case TX_SENSOR_RES_300:
			wpclk = GC4653_SUPPORT_207RES_60FPS_SCLK;
			break;
		default:
			break;
	}
	max_fps = sensor_max_fps;

	/* the format of fps is 16/16. for example 30 << 16 | 2, the value is 30/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)){
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}
	ret += gc4653_read(sd, 0x0342, &tmp);
	hts = tmp & 0x0f;
	ret += gc4653_read(sd, 0x0343, &tmp);
	if(ret < 0)
		return -1;

	hts = ((hts << 8) + tmp) << 1;
	if(data_type == TX_SENSOR_DATA_TYPE_WDR_DOL)
		hts <<= 1, short_time = 310;
	vts = wpclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = gc4653_write(sd, 0x0340, (unsigned char)((vts & 0x3f00) >> 8));
	ret += gc4653_write(sd, 0x0341, (unsigned char)(vts & 0xff));
	if(ret < 0)
		return -1;

	sensor->video.fps = fps;
	sensor->video.attr->integration_time_limit = vts - 4 - short_time;
	sensor->video.attr->max_integration_time_native = vts - 4 - short_time;
	sensor->video.attr->max_integration_time = vts - 4 - short_time;
	sensor->video.attr->max_integration_time_short = short_time;
	sensor->video.attr->total_height = vts;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;
}

static int gc4653_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = -1;
	unsigned char val = 0x0;

	ret = gc4653_read(sd, 0x0101, &val);

	if(enable & 0x2)
		val |= 0x02;
	else
		val &= 0xfd;

	ret += gc4653_write(sd, 0x031d, 0x2d);
	ret += gc4653_write(sd, 0x0101, val);
	ret += gc4653_write(sd, 0x031d, 0x28);
	if (ret < 0)
		return ret;

	return 0;
}

static int gc4653_set_mode(struct tx_isp_subdev *sd, int value)
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

static int gc4653_g_chip_ident(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"gc4653_reset");
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
		ret = private_gpio_request(pwdn_gpio,"gc4653_pwdn");
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
	ret = gc4653_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc4653 chip.\n",
				client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("gc4653 chip found @ 0x%02x (%s)\n sensor drv version %s\n", client->addr, client->adapter->name, SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "gc4653", sizeof("gc4653"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

#ifdef __WDR__
static int gc4653_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = gc4653_write(sd, 0x0201, value & 0xff);
	ret += gc4653_write(sd, 0x0200, (value>>8)&0x3f);
	if (ret < 0) {
		ISP_ERROR("gc4653_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}
static int gc4653_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
	int ret = 0;

	ret += gc4653_write_array(sd, wsize->regs);
	ret += gc4653_write_array(sd, gc4653_stream_on);
	return 0;
}

static int gc4653_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = 0;

	if(wdr_en == 1){
		wsize = &gc4653_win_sizes[1];
		sensor_max_fps = TX_SENSOR_MAX_FPS_15;
		data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		memcpy(&gc4653_attr.mipi, &gc4653_mipi_dol, sizeof(gc4653_mipi_dol));
		gc4653_attr.data_type = data_type;
		gc4653_attr.wdr_cache = wdr_bufsize;
		gc4653_attr.one_line_expr_in_us = 27;
		gc4653_attr.total_width = 2666;
		gc4653_attr.total_height = 1800;
		gc4653_attr.max_integration_time_native = 1800 - 310 - 4;
		gc4653_attr.integration_time_limit = 1800 - 310 - 4;
		gc4653_attr.max_integration_time = 1800 - 310 - 4;
		gc4653_attr.max_integration_time_short = 310;
	}
	else if (wdr_en == 0){
		data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		switch(sensor_resolution)
		{
			case TX_SENSOR_RES_400:
				wsize = &gc4653_win_sizes[0];
				sensor_max_fps = TX_SENSOR_MAX_FPS_30;
				gc4653_mipi_linear.image_twidth = 2560;
				gc4653_mipi_linear.image_theight = 1440;
				memcpy(&gc4653_attr.mipi, &gc4653_mipi_linear, sizeof(gc4653_mipi_linear));
				gc4653_attr.data_type = data_type;
				gc4653_attr.one_line_expr_in_us = 30;
				gc4653_attr.total_width = 3000;
				gc4653_attr.total_height = 1600;
				gc4653_attr.max_integration_time_native = 1600 - 4;
				gc4653_attr.integration_time_limit = 1600 - 4;
				gc4653_attr.max_integration_time = 1600 - 4;
				gc4653_attr.max_integration_time_short = 4;
				break;
			case TX_SENSOR_RES_100:
				wsize = &gc4653_win_sizes[2];
				sensor_max_fps = TX_SENSOR_MAX_FPS_60;
				gc4653_mipi_linear.image_twidth = 1280;
				gc4653_mipi_linear.image_theight = 720;
				memcpy(&gc4653_attr.mipi, &gc4653_mipi_linear, sizeof(gc4653_mipi_linear));
				gc4653_attr.data_type = data_type;
				gc4653_attr.one_line_expr_in_us = 30;
				gc4653_attr.total_width = 0x5dd * 2;
				gc4653_attr.total_height = 0x640;
				gc4653_attr.max_integration_time_native = 0x640 - 4;
				gc4653_attr.integration_time_limit = 0x640 - 4;
				gc4653_attr.max_integration_time = 0x640 - 4;
				gc4653_attr.max_integration_time_short = 4;
				break;
			case TX_SENSOR_RES_300:
				wsize = &gc4653_win_sizes[3];
				sensor_max_fps = TX_SENSOR_MAX_FPS_60;
				gc4653_attr.data_type = data_type;
				gc4653_mipi_linear.image_twidth = 1920;
				gc4653_mipi_linear.image_theight = 1080;
				memcpy(&gc4653_attr.mipi, &gc4653_mipi_linear, sizeof(gc4653_mipi_linear));
				gc4653_attr.one_line_expr_in_us = 30;
				gc4653_attr.total_width = 0x4e2 * 2;
				gc4653_attr.total_height = 0x4b0;
				gc4653_attr.max_integration_time_native = 0x4b0 - 4;
				gc4653_attr.integration_time_limit = 0x4b0 - 4;
				gc4653_attr.max_integration_time = 0x4b0 - 4;
				gc4653_attr.max_integration_time_short = 4;
				break;
			default:
				break;
		}
	}
	else
	{
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	sensor->video.attr = &gc4653_attr;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}
#endif

static int gc4653_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
#if 0
		case TX_ISP_EVENT_SENSOR_EXPO:
			if(arg)
				ret = gc4653_set_expo(sd, *(int*)arg);
			break;
#else
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			if(arg)
				ret = gc4653_set_integration_time(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			if(arg)
				ret = gc4653_set_analog_gain(sd, *(int*)arg);
			break;
#endif
		case TX_ISP_EVENT_SENSOR_DGAIN:
			if(arg)
				ret = gc4653_set_digital_gain(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
			if(arg)
				ret = gc4653_get_black_pedestal(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if(arg)
				ret = gc4653_set_mode(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
			ret = gc4653_write_array(sd, gc4653_stream_off);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			ret = gc4653_write_array(sd, gc4653_stream_on);
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if(arg)
				ret = gc4653_set_fps(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_VFLIP:
			if(arg)
				ret = gc4653_set_vflip(sd, *(int*)arg);
			break;
#ifdef __WDR__
		case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
			if(arg)
				ret = gc4653_set_integration_time_short(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_WDR:
			if(arg)
				ret = gc4653_set_wdr(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_WDR_STOP:
			if(arg)
				ret = gc4653_set_wdr_stop(sd, *(int*)arg);
			break;
#endif
		default:
			break;
	}

	return ret;
}

static int gc4653_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = gc4653_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int gc4653_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	gc4653_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops gc4653_core_ops = {
	.g_chip_ident = gc4653_g_chip_ident,
	.reset = gc4653_reset,
	.init = gc4653_init,
	/*.ioctl = gc4653_ops_ioctl,*/
	.g_register = gc4653_g_register,
	.s_register = gc4653_s_register,
};

static struct tx_isp_subdev_video_ops gc4653_video_ops = {
	.s_stream = gc4653_s_stream,
};

static struct tx_isp_subdev_sensor_ops	gc4653_sensor_ops = {
	.ioctl	= gc4653_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops gc4653_ops = {
	.core = &gc4653_core_ops,
	.video = &gc4653_video_ops,
	.sensor = &gc4653_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "gc4653",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc4653_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	unsigned long rate = 0;
	int ret;

	it_last = -1;
	ag_last = -1;

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

	rate = clk_get_rate(clk_get_parent(sensor->mclk));
	if (((rate / 1000) % 27000) != 0) {
		struct clk *vpll;
		vpll = clk_get(NULL,"vpll");
		if (IS_ERR(vpll)) {
			pr_err("get vpll failed\n");
		} else {
			rate = clk_get_rate(vpll);
			if (((rate / 1000) % 27000) != 0) {
				clk_set_rate(vpll,1080000000);
			}
			ret = clk_set_parent(sensor->mclk, vpll);
			if (ret < 0)
				pr_err("set mclk parent as epll err\n");
		}
	}

	private_clk_set_rate(sensor->mclk, 27000000);
	private_clk_enable(sensor->mclk);

	/*
	  convert sensor-gain into isp-gain,
	*/
	if(data_type == TX_SENSOR_DATA_TYPE_WDR_DOL)
	{
		wsize = &gc4653_win_sizes[1];
        sensor_max_fps = TX_SENSOR_MAX_FPS_15;
		gc4653_attr.data_type = data_type;
		memcpy(&gc4653_attr.mipi, &gc4653_mipi_dol, sizeof(gc4653_mipi_dol));
		gc4653_attr.wdr_cache = wdr_bufsize;
		gc4653_attr.one_line_expr_in_us = 27;
		gc4653_attr.total_width = 2666;
		gc4653_attr.total_height = 1800;
		gc4653_attr.max_integration_time_native = 1800 - 310 - 4;
		gc4653_attr.integration_time_limit = 1800 - 310 - 4;
		gc4653_attr.max_integration_time = 1800 - 310 - 4;
		gc4653_attr.max_integration_time_short = 310;
	}
	else
	{
		switch(sensor_resolution)
		{
			case TX_SENSOR_RES_400:
				wsize = &gc4653_win_sizes[0];
				sensor_max_fps = TX_SENSOR_MAX_FPS_30;
				gc4653_attr.data_type = data_type;
				memcpy(&gc4653_attr.mipi, &gc4653_mipi_linear, sizeof(gc4653_mipi_linear));
				gc4653_attr.one_line_expr_in_us = 30;
				gc4653_attr.total_width = 3000;
				gc4653_attr.total_height = 1600;
				gc4653_attr.max_integration_time_native = 1600 - 4;
				gc4653_attr.integration_time_limit = 1600 - 4;
				gc4653_attr.max_integration_time = 1600 - 4;
				gc4653_attr.max_integration_time_short = 4;
				break;
			case TX_SENSOR_RES_100:
				wsize = &gc4653_win_sizes[2];
				sensor_max_fps = TX_SENSOR_MAX_FPS_60;
				gc4653_attr.data_type = data_type;
				gc4653_mipi_linear.image_twidth = 1280;
				gc4653_mipi_linear.image_theight = 720;
				memcpy(&gc4653_attr.mipi, &gc4653_mipi_linear, sizeof(gc4653_mipi_linear));
				gc4653_attr.one_line_expr_in_us = 30;
				gc4653_attr.total_width = 0x5dd * 2;
				gc4653_attr.total_height = 0x640;
				gc4653_attr.max_integration_time_native = 0x640 - 4;
				gc4653_attr.integration_time_limit = 0x640 - 4;
				gc4653_attr.max_integration_time = 0x640 - 4;
				gc4653_attr.max_integration_time_short = 4;
				break;
			case TX_SENSOR_RES_300:
				wsize = &gc4653_win_sizes[3];
				sensor_max_fps = TX_SENSOR_MAX_FPS_60;
				gc4653_attr.data_type = data_type;
				gc4653_mipi_linear.image_twidth = 1920;
				gc4653_mipi_linear.image_theight = 1080;
				memcpy(&gc4653_attr.mipi, &gc4653_mipi_linear, sizeof(gc4653_mipi_linear));
				gc4653_attr.one_line_expr_in_us = 30;
				gc4653_attr.total_width = 0x4e2 * 2;
				gc4653_attr.total_height = 0x4b0;
				gc4653_attr.max_integration_time_native = 0x4b0 - 4;
				gc4653_attr.integration_time_limit = 0x4b0 - 4;
				gc4653_attr.max_integration_time = 0x4b0 - 4;
				gc4653_attr.max_integration_time_short = 4;
				break;
			default:
				ISP_WARNING("Sensor selection failed to enable default configuration.\n");
				break;
		}
	}
	gc4653_attr.data_type = data_type;
	gc4653_attr.dbus_type = data_interface;

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &gc4653_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc4653_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->gc4653\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int gc4653_remove(struct i2c_client *client)
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

static const struct i2c_device_id gc4653_id[] = {
	{ "gc4653", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc4653_id);

static struct i2c_driver gc4653_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "gc4653",
	},
	.probe		= gc4653_probe,
	.remove		= gc4653_remove,
	.id_table	= gc4653_id,
};

static __init int init_gc4653(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init gc4653 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&gc4653_driver);
}

static __exit void exit_gc4653(void)
{
	private_i2c_del_driver(&gc4653_driver);
}

module_init(init_gc4653);
module_exit(exit_gc4653);

MODULE_DESCRIPTION("A low-level driver for Galaxycore gc4653 sensors");
MODULE_LICENSE("GPL");
