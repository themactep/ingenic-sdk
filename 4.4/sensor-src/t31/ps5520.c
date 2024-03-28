// SPDX-License-Identifier: GPL-2.0+
/*
 * ps5520.c
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

#define SENSOR_NAME "ps5520"
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x48
#define SENSOR_MAX_WIDTH 2608
#define SENSOR_MAX_HEIGHT 1960
#define SENSOR_CHIP_ID 0x5520
#define SENSOR_CHIP_ID_H (0x55)
#define SENSOR_CHIP_ID_L (0x20)
#define SENSOR_REG_END 0xff
#define SENSOR_REG_DELAY 0xfe
#define SENSOR_BANK_REG 0xef
#define SENSOR_SUPPORT_PCLK_MIPI (160000000)
#define SENSOR_OUTPUT_MAX_FPS 20
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20170911a"

typedef enum {
	SENSOR_RES_400 = 400,
	SENSOR_RES_500 = 500,
}sensor_resolution_mode;

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_resolution = SENSOR_RES_400;
module_param(sensor_resolution, int, S_IRUGO);
MODULE_PARM_DESC(sensor_resolution, "Sensor Resolution set interface");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

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

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

// 1.25x~32x
struct again_lut sensor_again_lut[] = {
	{0 , 0     },
	{1 , 5731  },
	{2 , 11136 },
	{3 , 16248 },
	{4 , 21097 },
	{5 , 25710 },
	{6 , 30109 },
	{7 , 34312 },
	{8 , 38336 },
	{9 , 42195 },
	{10, 45904 },
	{11, 49472 },
	{12, 52910 },
	{13, 56228 },
	{14, 59433 },
	{15, 62534 },
	{16, 65536 },
	{17, 71267 },
	{18, 76672 },
	{19, 81784 },
	{20, 86633 },
	{21, 91246 },
	{22, 95645 },
	{23, 99848 },
	{24, 103872},
	{25, 107731},
	{26, 111440},
	{27, 115008},
	{28, 118446},
	{29, 121764},
	{30, 124969},
	{31, 128070},
	{32, 131072},
	{33, 136803},
	{34, 142208},
	{35, 147320},
	{36, 152169},
	{37, 156782},
	{38, 161181},
	{39, 165384},
	{40, 169408},
	{41, 173267},
	{42, 176976},
	{43, 180544},
	{44, 183982},
	{45, 187300},
	{46, 190505},
	{47, 193606},
	{48, 196608},
	{49, 202339},
	{50, 207744},
	{51, 212856},
	{52, 217705},
	{53, 222318},
	{54, 226717},
	{55, 230920},
	{56, 234944},
	{57, 238803},
	{58, 242512},
	{59, 246080},
	{60, 249518},
	{61, 252836},
	{62, 256041},
	{63, 259142},
	{64, 262144},
	{65, 267875},
	{66, 273280},
	{67, 278392},
	{68, 283241},
	{69, 287854},
	{70, 292253},
	{71, 296456},
	{72, 300480},
	{73, 304339},
	{74, 308048},
	{75, 311616},
	{76, 315054},
	{77, 318372},
	{78, 321577},
	{79, 324678},
	{80, 327680},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain <= sensor_again_lut[0].gain) {
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

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 840,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 2608,
	.image_theight = 1960,
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

struct tx_isp_dvp_bus sensor_dvp={
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking = {
		.vblanking = 0,
		.hblanking = 0,
	},
};
struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x5520,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
	},
	.max_again = 327680,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1980 - 2,
	.integration_time_limit = 1980 - 2,
	.total_width = 4050,
	.total_height = 1980,   /*vts + 1*/
	.max_integration_time = 1980 - 2,
	.one_line_expr_in_us = 59,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_2608_1960_20fps_mipi[] = {
	{0xef,0x05},
	{0x0f,0x00},
	{0x43,0x02},
	{0x44,0x00},
	{0xed,0x01},
	{0xef,0x01},
	{0xf5,0x01},
	{0x09,0x01},
	{0xef,0x00},
	{0x10,0x80},
	{0x11,0x80},
	{0x35,0x01},
	{0x36,0x0f},
	{0x37,0x0f},
	{0x38,0xe0},
	{0x5f,0xc2},
	{0x60,0x2a},
	{0x61,0x54},
	{0x62,0x29},
	{0x69,0x10},
	{0x6a,0x40},
	{0x85,0x22},
	{0x98,0x02},
	{0x9e,0x00},
	{0xa0,0x02},
	{0xa2,0x0a},
	{0xd8,0x10},
	{0xdf,0x24},
	{0xe2,0x05},
	{0xe3,0x24},
	{0xe6,0x05},
	{0xf3,0xb0},
	{0xf8,0x5a},
	{0xed,0x01},
	{0xef,0x01},
	{0x05,0x0b},
	{0x0a,0x07},
	{0x0b,0xbb},/*vts =0x0a,0x0b*/
	{0x0d,0x03},
	{0x27,0x0F},
	{0x28,0xD2},/*hts = 0x27,0x28*/
	{0x2a,0x56},
	{0x37,0x2c},
	{0x39,0x36},
	{0x3f,0xa6},
	{0x40,0x8c},
	{0x42,0xf4},
	{0x43,0xd6},
	{0x51,0x28},
	{0x5c,0x1e},
	{0x5d,0x0a},
	{0x68,0xfa},
	{0x69,0xc8},
	{0x75,0x56},
	{0xa6,0xa8},
	{0xaa,0x30},
	{0xae,0x50},
	{0xb0,0x50},
	{0xc4,0x54},
	{0xc6,0x10},
	{0xc9,0x55},
	{0xce,0x30},
	{0xd0,0x02},
	{0xd1,0x50},
	{0xd3,0x01},
	{0xd4,0x04},
	{0xd5,0x61},
	{0xd8,0xa0},
	{0xdd,0x42},
	{0xe2,0x0a},
	{0xf0,0x8d},
	{0xf1,0x16},
	{0xf5,0x19},
	{0x09,0x01},
	{0xef,0x02},
	{0x2e,0x04},
	{0x33,0x84},
	{0x3c,0xfa},
	{0x4e,0x02},
	{0xed,0x01},
	{0xef,0x05},
	{0x06,0x64},
	{0x09,0x09},
	{0x0a,0x05},
	{0x0d,0x5e},
	{0x0e,0x01},
	{0x0f,0x00},
	{0x10,0x02},
	{0x11,0x01},
	{0x15,0x07},
	{0x17,0x06},
	{0x18,0x05},
	{0x3b,0x00},
	{0x40,0x16},
	{0x41,0x28},
	{0x43,0x02},
	{0x44,0x01},
	{0x49,0x01},
	{0x4f,0x01},
	{0x5b,0x10},
	{0x94,0x04},
	{0xb0,0x01},
	{0xed,0x01},
	{0xef,0x06},
	{0x00,0x0c},
	{0x02,0x13},
	{0x06,0x02},
	{0x09,0x02},
	{0x0a,0x15},
	{0x0b,0x90},
	{0x0c,0x90},
	{0x0d,0x90},
	{0x0f,0x1b},
	{0x10,0x20},
	{0x11,0x1b},
	{0x12,0x20},
	{0x18,0x40},
	{0x1a,0x40},
	{0x28,0x03},
	{0x2b,0x20},
	{0x2d,0x00},
	{0x2e,0x20},
	{0x2f,0x20},
	{0x4a,0x40},
	{0x4b,0x40},
	{0x98,0x06},
	{0x99,0x23},
	{0x9e,0x42},
	{0x9f,0x44},
	{0xf1,0x01},
	{0xef,0x05},
	{0x3b,0x00},
	{0xed,0x01},
	{0xef,0x01},
	{0x02,0xfb},
	{0x09,0x01},
	{0xef,0x00},
	{0x11,0x00},
	{SENSOR_REG_DELAY, 0x02},/* DELAY 2ms */
	{0xef,0x05},
	{0x0f,0x01},
	{0xed,0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_2560_1920_20fps_mipi[] = {
	{0xef,0x05},
	{0x0f,0x00},
	{0x43,0x02},
	{0x44,0x00},
	{0xed,0x01},
	{0xef,0x01},
	{0xf5,0x01},
	{0x09,0x01},
	{0xef,0x00},
	{0x10,0x80},
	{0x11,0x80},
	{0x35,0x01},
	{0x36,0x0f},
	{0x37,0x0f},
	{0x38,0xe0},
	{0x5f,0xc2},
	{0x60,0x2a},
	{0x61,0x54},
	{0x62,0x29},
	{0x69,0x10},
	{0x6a,0x40},
	{0x85,0x22},
	{0x98,0x02},
	{0x9e,0x00},
	{0xa0,0x02},
	{0xa2,0x0a},
	{0xd8,0x10},
	{0xdf,0x24},
	{0xe2,0x05},
	{0xe3,0x24},
	{0xe6,0x05},
	{0xf3,0xb0},
	{0xf8,0x5a},
	{0xed,0x01},
	{0xef,0x01},
	{0x05,0x0b},
	{0x0a,0x07},
	{0x0b,0xbb},/*vts =0x0a,0x0b*/
	{0x0d,0x03},
	{0x27,0x0F},
	{0x28,0xD2},/*hts = 0x27,0x28*/
	{0x2a,0x56},
	{0x37,0x2c},
	{0x39,0x36},
	{0x3f,0xa6},
	{0x40,0x8c},
	{0x42,0xf4},
	{0x43,0xd6},
	{0x51,0x28},
	{0x5c,0x1e},
	{0x5d,0x0a},
	{0x68,0xfa},
	{0x69,0xc8},
	{0x75,0x56},
	{0xA4,0x22},//0E
	{0xA6,0x80},//A8
	{0xA8,0x18},//00
	{0xAA,0x00},//30
	{0xae,0x50},
	{0xb0,0x50},
	{0xc4,0x54},
	{0xc6,0x10},
	{0xc9,0x55},
	{0xce,0x30},
	{0xd0,0x02},
	{0xd1,0x50},
	{0xd3,0x01},
	{0xd4,0x04},
	{0xd5,0x61},
	{0xd8,0xa0},
	{0xdd,0x42},
	{0xe2,0x0a},
	{0xf0,0x8d},
	{0xf1,0x16},
	{0xf5,0x19},
	{0x09,0x01},
	{0xef,0x02},
	{0x2e,0x04},
	{0x33,0x84},
	{0x3c,0xfa},
	{0x4e,0x02},
	{0xed,0x01},
	{0xef,0x05},
	{0x06,0x64},
	{0x09,0x09},
	{0x0a,0x05},
	{0x0d,0x5e},
	{0x0e,0x01},
	{0x0f,0x00},
	{0x10,0x02},
	{0x11,0x01},
	{0x15,0x07},
	{0x17,0x06},
	{0x18,0x05},
	{0x3b,0x00},
	{0x40,0x16},
	{0x41,0x28},
	{0x43,0x02},
	{0x44,0x01},
	{0x49,0x01},
	{0x4f,0x01},
	{0x5b,0x10},
	{0x94,0x04},
	{0xb0,0x01},
	{0xed,0x01},
	{0xef,0x06},
	{0x00,0x0c},
	{0x02,0x13},
	{0x06,0x02},
	{0x09,0x02},
	{0x0a,0x15},
	{0x0b,0x90},
	{0x0c,0x90},
	{0x0d,0x90},
	{0x0f,0x1b},
	{0x10,0x20},
	{0x11,0x1b},
	{0x12,0x20},
	{0x18,0x40},
	{0x1a,0x40},
	{0x28,0x03},
	{0x2b,0x20},
	{0x2d,0x00},
	{0x2e,0x20},
	{0x2f,0x20},
	{0x4a,0x40},
	{0x4b,0x40},
	{0x98,0x06},
	{0x99,0x23},
	{0x9e,0x42},
	{0x9f,0x44},
	{0xf1,0x01},
	{0xef,0x05},
	{0x3b,0x00},
	{0xed,0x01},
	{0xef,0x01},
	{0x02,0xfb},
	{0x09,0x01},
	{0xef,0x00},
	{0x11,0x00},
	{SENSOR_REG_DELAY, 0x02},/* DELAY 2ms */
	{0xef,0x05},
	{0x0f,0x01},
	{0xed,0x01},
	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* [0] 2608*1960 @20fps */
	{
		.width = 2608,
		.height = 1960,
		.fps = 20 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR12_1X12,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2608_1960_20fps_mipi,
	},
	/* [1] 2560*1920 @20fps */
	{
		.width = 2560,
		.height = 1920,
		.fps = 20 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR12_1X12,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2560_1920_20fps_mipi,
	},
};
static struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

/*
 * the part of driver was fixed.
 */


static struct regval_list sensor_stream_on_mipi[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, unsigned char reg, unsigned char *value)
{
	int ret;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct i2c_msg msg[2] = {
		[0] = {
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		[1] = {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = value,
		}
	};

	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct tx_isp_subdev *sd, unsigned char reg, unsigned char value)
{
	int ret;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
		.buf = buf,
	};

	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

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
			if (vals->reg_num == SENSOR_BANK_REG) {
				val &= 0xe0;
				val |= (vals->value & 0x1f);
				ret = sensor_write(sd, vals->reg_num, val);
				ret = sensor_read(sd, vals->reg_num, &val);
			}
			pr_debug("sensor_read_array ->> vals->reg_num:0x%02x, vals->reg_value:0x%02x\n",vals->reg_num, val);
		}
		vals++;
	}

	return 0;
}

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sensor_write(sd, vals->reg_num, vals->value);

			//			printk("  {0x%x,0x%x}\n",vals->reg_num,vals->value);

			if (ret < 0) {
				printk("sensor_write error  %d\n" ,__LINE__);
				return ret;
			}
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
	int ret;
	unsigned char v;
	ret = sensor_read(sd, 0x00, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0) {
		printk("err: ps5520 write error, ret= %d \n",ret);
		return ret;
	}
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x01, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}



static int ag_last = -1;
static int it_last = -1;
static int sensor_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int expo = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;
	unsigned int Cmd_OffNy = 0;

	Cmd_OffNy = sensor_attr.total_height - expo - 1;

	if (expo != it_last) {
		ret = sensor_write(sd, 0xef, 0x01);
		ret += sensor_write(sd, 0x0c, (unsigned char)(Cmd_OffNy >> 8));
		ret += sensor_write(sd, 0x0d, (unsigned char)(Cmd_OffNy & 0xff));
		ret += sensor_write(sd, 0x09, 0x01);
		if (ret < 0)
			return ret;
	}

	if (again != ag_last) {
		ret += sensor_write(sd, 0xef, 0x01);
		ret += sensor_write(sd, 0x83, (unsigned char)(again & 0xff));
		ret += sensor_write(sd, 0x09, 0x01);
		if (ret < 0)
			return ret;
	}

	return 0;

}


/*
   static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
   {
   int ret = 0;
   unsigned int Cmd_OffNy = 0;
   unsigned int IntNep = 0;
   unsigned int IntNe = 0;
   unsigned int Const;

   Cmd_OffNy = sensor_attr.total_height - value - 1;
   ret = sensor_write(sd, 0xef, 0x01);
   ret += sensor_write(sd, 0x0c, (unsigned char)(Cmd_OffNy >> 8));
   ret += sensor_write(sd, 0x0d, (unsigned char)(Cmd_OffNy & 0xff));
   ret += sensor_write(sd, 0x09, 0x01);
   if (ret < 0)
   return ret;

   return 0;
   }

   static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
   {
   int ret = 0;
   unsigned int gain = value;
   ret += sensor_write(sd, 0xef, 0x01);
   ret += sensor_write(sd, 0x83, (unsigned char)(gain & 0xff));
   ret += sensor_write(sd, 0x09, 0x01);

   if (ret < 0)
   return ret;

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

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);

		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		pr_debug("%s stream on\n", SENSOR_NAME);

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);

		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		pr_debug("%s stream off\n", SENSOR_NAME);
	}
	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int pclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int Cmd_Lpf = 0;
	unsigned int Cur_OffNy = 0;
	unsigned int Cur_ExpLine = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch (sensor_resolution) {
	case SENSOR_RES_500:
		pclk = SENSOR_SUPPORT_PCLK_MIPI;
		break;
	case SENSOR_RES_400:
		pclk = SENSOR_SUPPORT_PCLK_MIPI;
		break;
	default:
		ret = -1;
		ISP_ERROR("Now we do not support this data interface!!!\n");
	}

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		printk("warn: fps(%d) not in range\n", fps);
		return -1;
	}
	ret = sensor_write(sd, 0xef, 0x01);
	if (ret < 0)
		return -1;
	ret = sensor_read(sd, 0x27, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0x28, &tmp);
	if (ret < 0)
		return -1;
	hts = (((hts & 0x1f) << 8) | tmp) ;

	vts = (pclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16));
	Cmd_Lpf = vts -1;
	ret = sensor_write(sd, 0xef, 0x01);
	ret += sensor_write(sd, 0x0b, (unsigned char)(Cmd_Lpf & 0xff));
	ret += sensor_write(sd, 0x0a, (unsigned char)(Cmd_Lpf >> 8));
	ret += sensor_write(sd, 0x09, 0x01);
	if (ret < 0) {
		printk("err: sensor_write err\n");
		return ret;
	}
	ret = sensor_read(sd, 0x0c, &tmp);
	Cur_OffNy = tmp;
	ret += sensor_read(sd, 0x0d, &tmp);
	if (ret < 0)
		return -1;
	Cur_OffNy = (((Cur_OffNy & 0xff) << 8) | tmp);
	Cur_ExpLine = sensor_attr.total_height - Cur_OffNy - 1;

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 3;
	sensor->video.attr->integration_time_limit = vts - 3;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 3;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	//	ret = sensor_set_integration_time(sd, Cur_ExpLine);
	if (ret < 0)
		return -1;

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
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val = 0;

	ret = sensor_write(sd, 0xef, 0x01);
	ret += sensor_read(sd, 0x1d, &val);
	if (enable)
		val = val | 0x80;
	else
		val = val & 0x7F;

	ret += sensor_write(sd, 0xef, 0x01);
	ret += sensor_write(sd, 0x1d, val);
	ret += sensor_write(sd, 0x09, 0x01);
	sensor->video.mbus_change = 0;
	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	/*if (pwdn_gpio != -1) {
	  ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
	  if (!ret) {
	  private_gpio_direction_output(pwdn_gpio, 1);
	  private_msleep(50);
	  private_gpio_direction_output(pwdn_gpio, 0);
	  private_msleep(10);
	  } else {
	  printk("gpio requrest fail %d\n",pwdn_gpio);
	  }
	  }*/
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(20);
		} else {
			printk("gpio requrest fail %d\n",reset_gpio);
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
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	return 0;
	long ret = 0;
	if (IS_ERR_OR_NULL(sd)) {
		printk("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {

	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = sensor_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		//		if (arg)
		//			ret = sensor_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		//		if (arg)
		//		ret = sensor_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sensor_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sensor_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sensor_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);

		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if (arg)
			ret = sensor_set_vflip(sd, *(int*)arg);
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
	if (len && strncmp(sd->chip.name, reg->name, len))
		return -EINVAL;
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
		return -EINVAL;
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
		printk("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		printk("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	clk_set_rate(sensor->mclk, 24000000);
	clk_enable(sensor->mclk);

	switch(sensor_resolution) {
	case SENSOR_RES_500:
		wsize = &sensor_win_sizes[0];
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi),sizeof(sensor_mipi));
		sensor_attr.max_integration_time_native = 1980-2;
		sensor_attr.integration_time_limit = 1980-2;
		sensor_attr.total_width = 4050;
		sensor_attr.total_height = 1980;
		sensor_attr.max_integration_time = 1980-2;
		break;
	case SENSOR_RES_400:
		wsize = &sensor_win_sizes[1];
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi),sizeof(sensor_mipi));
		sensor_attr.mipi.image_twidth = 2560;
		sensor_attr.mipi.image_theight = 1920;
		sensor_attr.max_integration_time_native = 1980-2;
		sensor_attr.integration_time_limit = 1980-2;
		sensor_attr.total_width = 4050;
		sensor_attr.total_height = 1980;
		sensor_attr.max_integration_time = 1980-2;
		break;
	default:
		ISP_ERROR("Can not support this data interface!!!\n");
	}

	sensor_attr.dbus_type = data_interface;
	sensor_attr.expo_fs = 1;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sensor_attr;
	sensor->video.mbus_change = 0;
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
	{ SENSOR_NAME, 0 },
	{ }
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
	if (ret) {
		printk("Failed to init %s driver.\n", SENSOR_NAME);
		return -1;
	}

	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	private_i2c_del_driver(&sensor_driver);
	sensor_common_exit();
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
