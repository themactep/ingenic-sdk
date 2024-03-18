/*
 * ar0237.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <sensor-common.h>
#include <sensor-info.h>
#include <apical-isp/apical_math.h>
#include <linux/proc_fs.h>
#include <soc/gpio.h>

#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x10
#define SENSOR_CHIP_ID_H (0x02)
#define SENSOR_CHIP_ID_L (0x56)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK (37125000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define LCG 0x0
#define HCG 0x1

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_12BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

static int data_interface = TX_SENSOR_DATA_INTERFACE_DVP;
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
	unsigned short reg_num;
	unsigned short value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	/* unsigned char mode; */
	unsigned int value;
	unsigned int gain;
};

/* static unsigned char again_mode = LCG; */
struct again_lut sensor_again_lut[] = {
	{ 0x0,	 0 },
	{ 0x1,	 2794 },
	{ 0x2,	 6397 },
	{ 0x3,	 9011 },
	{ 0x4,	 12388 },
	{ 0x5,	 16447 },
	{ 0x6,	 19572 },
	{ 0x7,	 23340 },
	{ 0x8,	 26963 },
	{ 0x9,	 31135 },
	{ 0xa,	 35780 },
	{ 0xb,	 39588 },
	{ 0xc,	 44438 },
	{ 0xd,	 49051 },
	{ 0xe,	 54517 },
	{ 0xf,	 59685 },
	{ 0x10,	 65536 },
	{ 0x12,	 71490 },
	{ 0x14,	 78338 },
	{ 0x16,	 85108 },
	{ 0x18,	 92854 },
	{ 0x1a,	 100992 },
	{ 0x1c,	 109974 },
	{ 0x1e,	 120053 },
	{ 0x20,	 131072 },
	{ 0x22,	 137247 },
	{ 0x24,	 143667 },
	{ 0x26,	 150644 },
	{ 0x28,	 158212 },
	{ 0x2a,	 166528 },
	{ 0x2c,	 175510 },
	{ 0x2e,	 185457 },
	{ 0x30,	 196608 },
	{ 0x32,	 202783 },
	{ 0x34,	 209203 },
	{ 0x36,	 216276 },
	{ 0x38,	 223748 },
	{ 0x3a,	 232064 },
	{ 0x3c,	 241046 },
	{ 0x3e,	 250993 },
	{ 0x40,	 262144 },

#if 0
	{LCG,	 0xb,	  39588},
	{LCG,	 0xc,	  44438},
	{LCG,	 0xd,	  49051},
	{LCG,	 0xe,	  54517},
	{LCG,	 0xf,	  59685},
	{LCG,	 0x10,	  65536},
	{LCG,	 0x12,	  71490},
	{LCG,	 0x14,	  78338},
	{LCG,	 0x16,	  85108},
	{LCG,	 0x18,	  92854},
	/* {HCG,	 0x0,	  93910}, */
	/* {HCG,	 0x1,	  97010}, */
	{HCG,	 0x2,	  100012},
	{HCG,	 0x3,	  103239},
	{HCG,	 0x4,	  106666},
	{HCG,	 0x5,	  109974},
	{HCG,	 0x6,	  113454},
	{HCG,	 0x7,	  117360},
	{HCG,	 0x8,	  121110},
	{HCG,	 0x9,	  125221},
	{HCG,	 0xa,	  129402},
	{HCG,	 0xb,	  133636},
	{HCG,	 0xc,	  138348},
	{HCG,	 0xd,	  143252},
	{HCG,	 0xe,	  148310},
	{HCG,	 0xf,	  153670},
	{HCG,	 0x10,	  159446},
	{HCG,	 0x12,	  165548},
	{HCG,	 0x14,	  172049},
	{HCG,	 0x16,	  179133},
	{HCG,	 0x18,	  186646},
	{HCG,	 0x1a,	  194938},
	{HCG,	 0x1c,	  203884},
	{HCG,	 0x1e,	  213846},
	{HCG,	 0x20,	  224982},
	{HCG,	 0x22,	  231084},
	{HCG,	 0x24,	  237585},
	{HCG,	 0x26,	  244598},
	{HCG,	 0x28,	  252182},
	{HCG,	 0x2a,	  260414},
	{HCG,	 0x2c,	  269420},
	{HCG,	 0x2e,	  279382},
	{HCG,	 0x30,	  290518},
	{HCG,	 0x32,	  296661},
	{HCG,	 0x34,	  303160},
	{HCG,	 0x36,	  310169},
	{HCG,	 0x38,	  317685},
	{HCG,	 0x3a,	  325980},
	{HCG,	 0x3c,	  334956},
	{HCG,	 0x3e,	  344918},
	{HCG,	 0x40,	  356054},
#endif
};
struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;

	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = 0;
			/* again_mode = LCG; */
			return 0;
		}
		else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			/* again_mode = (lut - 1)->mode; */
			return (lut - 1)->gain;
		}
		else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				/* again_mode = lut->mode; */
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;

}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return isp_gain;
}

struct tx_isp_sensor_attribute sensor_attr={
	.name = "ar0237",
	.chip_id = 0x0056,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_16BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS;
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
	},
	.max_again = 262144,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 0x52F-4,
	.integration_time_limit = 0x52F-4,
	.total_width = 0x45E,
	.total_height = 0x52F,
	.max_integration_time = 0x52F-4,
	.integration_time_apply_delay = 1,
	.again_apply_delay = 1,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};


static struct regval_list sensor_init_regs_1920_1080_30fps_mipi[] = {

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list sensor_init_regs_1920_1080_30fps_dvp[] = {
	{0x301A, 0x0001},
	{SENSOR_REG_DELAY,200},
	{0x301A, 0x10D8},
	{0x3088, 0x8000},
	{0x3086, 0x4558},
	{0x3086, 0x72A6},
	{0x3086, 0x4A31},
	{0x3086, 0x4342},
	{0x3086, 0x8E03},
	{0x3086, 0x2A14},
	{0x3086, 0x4578},
	{0x3086, 0x7B3D},
	{0x3086, 0xFF3D},
	{0x3086, 0xFF3D},
	{0x3086, 0xEA2A},
	{0x3086, 0x043D},
	{0x3086, 0x102A},
	{0x3086, 0x052A},
	{0x3086, 0x1535},
	{0x3086, 0x2A05},
	{0x3086, 0x3D10},
	{0x3086, 0x4558},
	{0x3086, 0x2A04},
	{0x3086, 0x2A14},
	{0x3086, 0x3DFF},
	{0x3086, 0x3DFF},
	{0x3086, 0x3DEA},
	{0x3086, 0x2A04},
	{0x3086, 0x622A},
	{0x3086, 0x288E},
	{0x3086, 0x0036},
	{0x3086, 0x2A08},
	{0x3086, 0x3D64},
	{0x3086, 0x7A3D},
	{0x3086, 0x0444},
	{0x3086, 0x2C4B},
	{0x3086, 0xA403},
	{0x3086, 0x430D},
	{0x3086, 0x2D46},
	{0x3086, 0x4316},
	{0x3086, 0x2A90},
	{0x3086, 0x3E06},
	{0x3086, 0x2A98},
	{0x3086, 0x5F16},
	{0x3086, 0x530D},
	{0x3086, 0x1660},
	{0x3086, 0x3E4C},
	{0x3086, 0x2904},
	{0x3086, 0x2984},
	{0x3086, 0x8E03},
	{0x3086, 0x2AFC},
	{0x3086, 0x5C1D},
	{0x3086, 0x5754},
	{0x3086, 0x495F},
	{0x3086, 0x5305},
	{0x3086, 0x5307},
	{0x3086, 0x4D2B},
	{0x3086, 0xF810},
	{0x3086, 0x164C},
	{0x3086, 0x0955},
	{0x3086, 0x562B},
	{0x3086, 0xB82B},
	{0x3086, 0x984E},
	{0x3086, 0x1129},
	{0x3086, 0x9460},
	{0x3086, 0x5C19},
	{0x3086, 0x5C1B},
	{0x3086, 0x4548},
	{0x3086, 0x4508},
	{0x3086, 0x4588},
	{0x3086, 0x29B6},
	{0x3086, 0x8E01},
	{0x3086, 0x2AF8},
	{0x3086, 0x3E02},
	{0x3086, 0x2AFA},
	{0x3086, 0x3F09},
	{0x3086, 0x5C1B},
	{0x3086, 0x29B2},
	{0x3086, 0x3F0C},
	{0x3086, 0x3E03},
	{0x3086, 0x3E15},
	{0x3086, 0x5C13},
	{0x3086, 0x3F11},
	{0x3086, 0x3E0F},
	{0x3086, 0x5F2B},
	{0x3086, 0x902B},
	{0x3086, 0x803E},
	{0x3086, 0x062A},
	{0x3086, 0xF23F},
	{0x3086, 0x103E},
	{0x3086, 0x0160},
	{0x3086, 0x29A2},
	{0x3086, 0x29A3},
	{0x3086, 0x5F4D},
	{0x3086, 0x1C2A},
	{0x3086, 0xFA29},
	{0x3086, 0x8345},
	{0x3086, 0xA83E},
	{0x3086, 0x072A},
	{0x3086, 0xFB3E},
	{0x3086, 0x6745},
	{0x3086, 0x8824},
	{0x3086, 0x3E08},
	{0x3086, 0x2AFA},
	{0x3086, 0x5D29},
	{0x3086, 0x9288},
	{0x3086, 0x102B},
	{0x3086, 0x048B},
	{0x3086, 0x1686},
	{0x3086, 0x8D48},
	{0x3086, 0x4D4E},
	{0x3086, 0x2B80},
	{0x3086, 0x4C0B},
	{0x3086, 0x3F36},
	{0x3086, 0x2AF2},
	{0x3086, 0x3F10},
	{0x3086, 0x3E01},
	{0x3086, 0x6029},
	{0x3086, 0x8229},
	{0x3086, 0x8329},
	{0x3086, 0x435C},
	{0x3086, 0x155F},
	{0x3086, 0x4D1C},
	{0x3086, 0x2AFA},
	{0x3086, 0x4558},
	{0x3086, 0x8E00},
	{0x3086, 0x2A98},
	{0x3086, 0x3F0A},
	{0x3086, 0x4A0A},
	{0x3086, 0x4316},
	{0x3086, 0x0B43},
	{0x3086, 0x168E},
	{0x3086, 0x032A},
	{0x3086, 0x9C45},
	{0x3086, 0x783F},
	{0x3086, 0x072A},
	{0x3086, 0x9D3E},
	{0x3086, 0x305D},
	{0x3086, 0x2944},
	{0x3086, 0x8810},
	{0x3086, 0x2B04},
	{0x3086, 0x530D},
	{0x3086, 0x4558},
	{0x3086, 0x3E08},
	{0x3086, 0x8E01},
	{0x3086, 0x2A98},
	{0x3086, 0x8E00},
	{0x3086, 0x76A7},
	{0x3086, 0x77A7},
	{0x3086, 0x4644},
	{0x3086, 0x1616},
	{0x3086, 0xA57A},
	{0x3086, 0x1244},
	{0x3086, 0x4B18},
	{0x3086, 0x4A04},
	{0x3086, 0x4316},
	{0x3086, 0x0643},
	{0x3086, 0x1605},
	{0x3086, 0x4316},
	{0x3086, 0x0743},
	{0x3086, 0x1658},
	{0x3086, 0x4316},
	{0x3086, 0x5A43},
	{0x3086, 0x1645},
	{0x3086, 0x588E},
	{0x3086, 0x032A},
	{0x3086, 0x9C45},
	{0x3086, 0x787B},
	{0x3086, 0x3F07},
	{0x3086, 0x2A9D},
	{0x3086, 0x530D},
	{0x3086, 0x8B16},
	{0x3086, 0x863E},
	{0x3086, 0x2345},
	{0x3086, 0x5825},
	{0x3086, 0x3E10},
	{0x3086, 0x8E01},
	{0x3086, 0x2A98},
	{0x3086, 0x8E00},
	{0x3086, 0x3E10},
	{0x3086, 0x8D60},
	{0x3086, 0x1244},
	{0x3086, 0x4BB9},
	{0x3086, 0x2C2C},
	{0x3086, 0x2C2C},
	{0x301A, 0x10D8},
	{0x30B0, 0x1A38},
	{0x31AC, 0x0C0C},
	/* {0x3028, 0x0000},//PCLK Delay */
	{0x302A, 0x0008},
	{0x302C, 0x0001},
	{0x302E, 0x0002},
	{0x3030, 0x002C},
	{0x3036, 0x000C},
	{0x3038, 0x0001},
	{0x3002, 0x0000},
	{0x3004, 0x0000},
	{0x3006, 0x0437},
	{0x3008, 0x077F},
	{0x300A, 0x052F},//vts
	{0x300C, 0x045E},
	{0x3012, 0x0416},
	{0x30A2, 0x0001},
	{0x30A6, 0x0001},
	{0x30AE, 0x0001},
	{0x30A8, 0x0001},
	{0x3040, 0x0000},
	{0x31AE, 0x0301},
	{0x3082, 0x0009},
	{0x30BA, 0x760C},
	{0x3100, 0x0000},
	{0x3060, 0x000B},
	{0x31D0, 0x0000},
	{0x3064, 0x1802},
	{0x3EEE, 0xA0AA},
	{0x30BA, 0x762C},
	{0x3F4A, 0x0F70},
	{0x309E, 0x016C},
	{0x3092, 0x006F},
	{0x3EE4, 0x9937},
	{0x3EE6, 0x3863},
	{0x3EEC, 0x3B0C},
	{0x30B0, 0x1A3A},
	{0x30B0, 0x1A3A},
	{0x30BA, 0x762C},
	{0x30B0, 0x1A3A},
	{0x30B0, 0x0A3A},
	{0x3EEA, 0x2838},
	{0x3ECC, 0x4E2D},
	{0x3ED2, 0xFEA6},
	{0x3ED6, 0x2CB3},
	{0x3EEA, 0x2819},
	{0x30B0, 0x1A3A},
	{0x306E, 0x2418},
	/* {0x3070, 0x2}, //color bar */
	/* {0x301A, 0x10DC}, */
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1280*1080 */
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SGRBG12_1X12,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_dvp,
	}
};

static enum v4l2_mbus_pixelcode sensor_mbus_code[] = {
	V4L2_MBUS_FMT_SGRBG12_1X12,
};

/*
 * the part of driver was fixed.
 */

static struct regval_list sensor_stream_on_dvp[] = {
	{0x301A, 0x10DC},	//SENSOR_REGISTER
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sensor_stream_off_dvp[] = {
	{0x301A, 0x10D8},	//SENSOR_REGISTER
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sensor_stream_on_mipi[] = {
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sensor_stream_off_mipi[] = {
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

int sensor_read(struct v4l2_subdev *sd, unsigned short reg,
		unsigned char *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};

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
			.len = 2,
			.buf = value,
		}
	};
	int ret;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct v4l2_subdev *sd, unsigned short reg,
		unsigned short value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	unsigned char buf[4] = {reg >> 8, reg & 0xff, value >> 8, value & 0xff};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 4,
		.buf = buf,
	};
	int ret;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int sensor_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val[2];
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
				msleep(vals->value);
		} else {
			ret = sensor_read(sd, vals->reg_num, val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
static int sensor_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
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

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd, unsigned int *ident)
{
	unsigned char v[2];
	int ret;

	ret = sensor_read(sd, 0x3000, v);
	if (ret < 0)
		return ret;
	if (v[0] != SENSOR_CHIP_ID_H)
		return -ENODEV;
	if (v[1] != SENSOR_CHIP_ID_L)
		return -ENODEV;
	return 0;
}

static int sensor_set_integration_time(struct v4l2_subdev *sd, int value)
{
	int ret;

	ret = sensor_write(sd,0x3012, value);
	if (ret < 0)
		return ret;
	return 0;

}

static int sensor_set_analog_gain(struct v4l2_subdev *sd, int value)
{
	int ret;
#if 0
	if (again_mode == LCG) {
		ret = sensor_write(sd,0x3202, 0x0080);
		ret += sensor_write(sd,0x3206, 0x0B08);
		ret += sensor_write(sd,0x3208, 0x1E13);
		ret += sensor_write(sd,0x3100, 0x00);

	} else if (again_mode == HCG) {
		ret = sensor_write(sd,0x3202, 0x00B0);
		ret += sensor_write(sd,0x3206, 0x1C0E);
		ret += sensor_write(sd,0x3208, 0x4E39);
		ret += sensor_write(sd,0x3100, 0x04);
	} else {
		printk("Do not support this Again mode!\n");
	}

#endif
	ret = sensor_write(sd,0x3060, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_digital_gain(struct v4l2_subdev *sd, int value)
{
	return 0;
}

static int sensor_get_black_pedestal(struct v4l2_subdev *sd, int value)
{
	return 0;
}

static int sensor_init(struct v4l2_subdev *sd, u32 enable)
{
	struct tx_isp_sensor *sensor = (container_of(sd, struct tx_isp_sensor, sd));
	struct tx_isp_notify_argument arg;
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
	arg.value = (int)&sensor->video;
	sd->v4l2_dev->notify(sd, TX_ISP_NOTIFY_SYNC_VIDEO_IN, &arg);
	sensor->priv = wsize;
	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);

		} else {
			printk("Don't support this Sensor Data interface\n");
		}
		pr_debug("ar0237 stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);

		} else {
			printk("Don't support this Sensor Data interface\n");
		}
		pr_debug("ar0237 stream off\n");
	}
	return ret;
}

static int sensor_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int sensor_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int sensor_set_fps(struct tx_isp_sensor *sensor, int fps)
{
	struct tx_isp_notify_argument arg;
	struct v4l2_subdev *sd = &sensor->sd;
	unsigned int pclk = SENSOR_SUPPORT_SCLK;
	unsigned short hts=0;
	unsigned short vts = 0;
	unsigned char tmp[2];
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;
	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8))
		return -1;
	ret += sensor_read(sd, 0x300C, tmp);
	hts =tmp[0];
	if (ret < 0)
		return -1;
	hts = (hts << 8) + tmp[1];
	vts = pclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = sensor_write(sd, 0x300A, vts);
	if (ret < 0)
		return -1;
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	arg.value = (int)&sensor->video;
	sd->v4l2_dev->notify(sd, TX_ISP_NOTIFY_SYNC_VIDEO_IN, &arg);
	return ret;
}

static int sensor_set_mode(struct tx_isp_sensor *sensor, int value)
{
	struct tx_isp_notify_argument arg;
	struct v4l2_subdev *sd = &sensor->sd;
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;

	if (value == TX_ISP_SENSOR_FULL_RES_MAX_FPS) {
		wsize = &sensor_win_sizes[0];
	} else if (value == TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS) {
		wsize = &sensor_win_sizes[0];
	}

	if (wsize) {
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		arg.value = (int)&sensor->video;
		sd->v4l2_dev->notify(sd, TX_ISP_NOTIFY_SYNC_VIDEO_IN, &arg);
	}
	return ret;
}
static int sensor_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if (reset_gpio != -1) {
		ret = gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			gpio_direction_output(reset_gpio, 1);
			msleep(20);
			gpio_direction_output(reset_gpio, 0);
			msleep(20);
			gpio_direction_output(reset_gpio, 1);
			msleep(20);
		} else {
			printk("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = gpio_request(pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			gpio_direction_output(pwdn_gpio, 1);
			msleep(150);
			gpio_direction_output(pwdn_gpio, 0);
			msleep(10);
		} else {
			printk("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sensor_detect(sd, &ident);
	if (ret) {
		v4l_err(client,
				"chip found @ 0x%x (%s) is not an ar0237 chip.\n",
				client->addr, client->adapter->name);
		return ret;
	}
	v4l_info(client, "ar0237 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);
	return v4l2_chip_ident_i2c_client(client, chip, ident, 0);
}

static int sensor_s_power(struct v4l2_subdev *sd, int on)
{

	return 0;
}
static long sensor_ops_private_ioctl(struct tx_isp_sensor *sensor, struct isp_private_ioctl *ctrl)
{
	struct v4l2_subdev *sd = &sensor->sd;
	long ret = 0;
	switch(ctrl->cmd) {
		case TX_ISP_PRIVATE_IOCTL_SENSOR_INT_TIME:
			ret = sensor_set_integration_time(sd, ctrl->value);
			break;
		case TX_ISP_PRIVATE_IOCTL_SENSOR_AGAIN:
			ret = sensor_set_analog_gain(sd, ctrl->value);
			break;
		case TX_ISP_PRIVATE_IOCTL_SENSOR_DGAIN:
			ret = sensor_set_digital_gain(sd, ctrl->value);
			break;
		case TX_ISP_PRIVATE_IOCTL_SENSOR_BLACK_LEVEL:
			ret = sensor_get_black_pedestal(sd, ctrl->value);
			break;
		case TX_ISP_PRIVATE_IOCTL_SENSOR_RESIZE:
			ret = sensor_set_mode(sensor,ctrl->value);
			break;
		case TX_ISP_PRIVATE_IOCTL_SUBDEV_PREPARE_CHANGE:
			if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
				ret = sensor_write_array(sd, sensor_stream_off_dvp);
			} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
				ret = sensor_write_array(sd, sensor_stream_off_mipi);

			} else {
				printk("Don't support this Sensor Data interface\n");
			}
			break;
		case TX_ISP_PRIVATE_IOCTL_SUBDEV_FINISH_CHANGE:
			if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
				ret = sensor_write_array(sd, sensor_stream_on_dvp);
			} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
				ret = sensor_write_array(sd, sensor_stream_on_mipi);

			} else {
				printk("Don't support this Sensor Data interface\n");
			}
			break;
		case TX_ISP_PRIVATE_IOCTL_SENSOR_FPS:
			ret = sensor_set_fps(sensor, ctrl->value);
			break;
		default:
			pr_debug("do not support ctrl->cmd ====%d\n",ctrl->cmd);
			break;;
	}
	return 0;
}
static long sensor_ops_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct tx_isp_sensor *sensor =container_of(sd, struct tx_isp_sensor, sd);
	int ret;
	switch(cmd) {
		case VIDIOC_ISP_PRIVATE_IOCTL:
			ret = sensor_ops_private_ioctl(sensor, arg);
			break;
		default:
			return -1;
			break;
	}
	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int sensor_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val[2];
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = sensor_read(sd, reg->reg & 0xffff, val);
	reg->val = val[0];
	reg->val = (reg->val<<8)+val[1];
	reg->size = 2;
	return ret;
}

static int sensor_s_register(struct v4l2_subdev *sd, const struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	sensor_write(sd, reg->reg & 0xffff, reg->val & 0xffff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops sensor_core_ops = {
	.g_chip_ident = sensor_g_chip_ident,
	.reset = sensor_reset,
	.init = sensor_init,
	.s_power = sensor_s_power,
	.ioctl = sensor_ops_ioctl,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = sensor_g_register,
	.s_register = sensor_s_register,
#endif
};

static const struct v4l2_subdev_video_ops sensor_video_ops = {
	.s_stream = sensor_s_stream,
	.s_parm = sensor_s_parm,
	.g_parm = sensor_g_parm,
};

static const struct v4l2_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
};

static int sensor_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
	int ret;

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
	clk_set_rate(sensor->mclk, 27000000);
	clk_enable(sensor->mclk);

	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;

	sensor_attr.dbus_type = data_interface;
	if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
		wsize->regs = sensor_init_regs_1920_1080_30fps_dvp;
	} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
		wsize->regs = sensor_init_regs_1920_1080_30fps_mipi;
	} else {
		printk("Don't support this Sensor Data Output Interface.\n");
		goto err_set_sensor_data_interface;
	}
#if 0
	sensor_attr.dvp.gpio = sensor_gpio_func;

	switch(sensor_gpio_func) {
		case DVP_PA_LOW_10BIT:
		case DVP_PA_HIGH_10BIT:
			mbus = sensor_mbus_code[0];
			break;
		case DVP_PA_12BIT:
			mbus = sensor_mbus_code[1];
			break;
		default:
			goto err_set_sensor_gpio;
	}

	for(i = 0; i < ARRAY_SIZE(sensor_win_sizes); i++)
		sensor_win_sizes[i].mbus_code = mbus;

#endif
	 /*
		convert sensor-gain into isp-gain,
	 */
	sensor_attr.max_again = 262144;
	sensor_attr.max_dgain = 0;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sensor_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	v4l2_i2c_subdev_init(sd, client, &sensor_ops);
	v4l2_set_subdev_hostdata(sd, sensor);

	pr_debug("probe ok ------->ar0237\n");
	return 0;
err_set_sensor_data_interface:
err_set_sensor_gpio:
	clk_disable(sensor->mclk);
	clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int sensor_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = v4l2_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		gpio_free(pwdn_gpio);

	clk_disable(sensor->mclk);
	clk_put(sensor->mclk);

	v4l2_device_unregister_subdev(sd);
	kfree(sensor);
	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{ "ar0237", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "ar0237",
	},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

static __init int init_sensor(void)
{
	sensor_common_init(&sensor_info);
	return i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	sensor_common_exit();
	i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for OmniVision ar0237 sensors");
MODULE_LICENSE("GPL");
