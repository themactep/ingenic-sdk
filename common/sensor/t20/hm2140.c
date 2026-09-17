// SPDX-License-Identifier: GPL-2.0+
/*
 * hm2140.c
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
#include <sensor-common.h>
#include <sensor-info.h>
#include <apical-isp/apical_math.h>

#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_CHIP_ID ((SENSOR_CHIP_ID_H << 8) | SENSOR_CHIP_ID_L)
#define SENSOR_CHIP_ID_H (0x21)
#define SENSOR_CHIP_ID_L (0x40)
#define SENSOR_I2C_ADDRESS 0x24
#define SENSOR_MAX_HEIGHT 1080
#define SENSOR_MAX_WIDTH 1920
#define SENSOR_NAME "hm2140"
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_REG_END 0xffff
#define SENSOR_SUPPORT_SCLK (76789800)
#define SENSOR_VERSION "20180320"


static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_LOW_10BIT;
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
	uint16_t reg_num;
	uint16_t value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x00, 0},
	{0x01, 5731},
	{0x02, 11136},
	{0x03, 16248},
	{0x04, 21097},
	{0x05, 25710},
	{0x06, 30109},
	{0x07, 34312},
	{0x08, 38336},
	{0x09, 42195},
	{0x0a, 45904},
	{0x0b, 49472},
	{0x0c, 52910},
	{0x0d, 56228},
	{0x0e, 59433},
	{0x0f, 62534},
	{0x10, 65536},
	{0x11, 71267},
	{0x12, 76672},
	{0x13, 81784},
	{0x14, 86633},
	{0x15, 91246},
	{0x16, 95645},
	{0x17, 99848},
	{0x18, 103872},
	{0x19, 107731},
	{0x1a, 111440},
	{0x1b, 115008},
	{0x1c, 118446},
	{0x1d, 121764},
	{0x1e, 124969},
	{0x1f, 128070},
	{0x20, 131072},
	{0x21, 136803},
	{0x22, 142208},
	{0x23, 147320},
	{0x24, 152169},
	{0x25, 156782},
	{0x26, 161181},
	{0x27, 165384},
	{0x28, 169408},
	{0x29, 173267},
	{0x2a, 176976},
	{0x2b, 180544},
	{0x2c, 183982},
	{0x2d, 187300},
	{0x2e, 190505},
	{0x2f, 193606},
	{0x30, 196608},
	{0x31, 202339},
	{0x32, 207744},
	{0x33, 212856},
	{0x34, 217705},
	{0x35, 222318},
	{0x36, 226717},
	{0x37, 230920},
	{0x38, 234944},
	{0x39, 238803},
	{0x3a, 242512},
	{0x3b, 246080},
	{0x3c, 249518},
	{0x3d, 252836},
	{0x3e, 256041},
	{0x3f, 259142},
};
struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	struct again_lut *lut = sensor_again_lut;

	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
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

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi = {
	.clk = 800,
	.lans = 2,
};
struct tx_isp_dvp_bus sensor_dvp = {
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking =
		{
			.vblanking = 0,
			.hblanking = 0,
		},
};

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp =
		{
			.mode = SENSOR_DVP_HREF_MODE,
			.blanking =
				{
					.vblanking = 0,
					.hblanking = 0,
				},

		},
	.max_again = 259142,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 0x534 - 2,
	.integration_time_limit = 0x534 - 2,
	.total_width = 0x902,
	.total_height = 0x534,
	.max_integration_time = 0x534 - 2,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_1920_1080_25fps_mipi[] = {

	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_25fps_dvp[] = {
	// 1080p@25fps
	{0x0103, 0x00},
	{0x5227, 0x74},
	{0x030b, 0x11},
	{0x0307, 0x00},
	{0x0309, 0x00},
	{0x030a, 0x0a},
	{0x030d, 0x02},
	{0x030f, 0x10},
	{0x5235, 0x24},
	{0x5236, 0x92},
	{0x5237, 0x23},
	{0x5238, 0xdc},
	{0x5239, 0x01},
	{0x0100, 0x02},
	{0x4001, 0x00},
	{0x4002, 0x2b},
	{0x0101, 0x00},
	{0x4026, 0xbb},
	{0x0202, 0x02},
	{0x0203, 0xec},
	{0x0340, 0x05}, // vts
	{0x0341, 0x34},
	{0x0342, 0x09}, // hts
	{0x0343, 0x02},
	{0x0344, 0x00},
	{0x0345, 0x04},
	{0x0346, 0x00},
	{0x0347, 0x04},
	{0x0348, 0x07},
	{0x0349, 0x83},
	{0x034a, 0x04},
	{0x034b, 0x3b},
	{0x034c, 0x07},
	{0x034d, 0x80},
	{0x034e, 0x04},
	{0x034f, 0x38},
	{0x0360, 0x00},
	{0x0350, 0x73},
	{0x5015, 0xb3},
	{0x50dd, 0x01},
	{0x50cb, 0xa3},
	{0x5004, 0x40},
	{0x5005, 0x28},
	{0x504d, 0x7f},
	{0x504e, 0x00},
	{0x5040, 0x07},
	{0x5011, 0x00},
	{0x501d, 0x4c},
	{0x5011, 0x0b},
	{0x5012, 0x01},
	{0x5013, 0x03},
	{0x4131, 0x01},
	{0x5282, 0xff},
	{0x5283, 0x07},
	{0x5010, 0x20},
	{0x4132, 0x20},
	{0x50d5, 0xe0},
	{0x50d7, 0x12},
	{0x50bb, 0x14},
	{0x50b7, 0x00},
	{0x50b8, 0x35},
	{0x50b9, 0x11},
	{0x50ba, 0xf0},
	{0x50b3, 0x24},
	{0x50b4, 0x00},
	{0x50fa, 0x02},
	{0x509b, 0x01},
	{0x50aa, 0xff},
	{0x50ab, 0x25},
	{0x509c, 0x00},
	{0x50ad, 0x0c},
	{0x5096, 0x00},
	{0x50a1, 0x12},
	{0x50af, 0x31},
	{0x50a0, 0x11},
	{0x50a2, 0x22},
	{0x509d, 0x20},
	{0x50ac, 0x55},
	{0x50ae, 0x26},
	{0x509e, 0x03},
	{0x509f, 0x01},
	{0x5097, 0x12},
	{0x5099, 0x00},
	{0x50b5, 0x00},
	{0x50b6, 0x10},
	{0x5094, 0x08},
	{0x5200, 0x43},
	{0x5201, 0xc0},
	{0x5202, 0x00},
	{0x5203, 0x00},
	{0x5204, 0x00},
	{0x5205, 0x05},
	{0x5206, 0xa1},
	{0x5207, 0x01},
	{0x5208, 0x05},
	{0x5209, 0x0c},
	{0x520a, 0x00},
	{0x520b, 0x45},
	{0x520c, 0x15},
	{0x520d, 0x40},
	{0x520e, 0x50},
	{0x520f, 0x10},
	{0x5214, 0x40},
	{0x5215, 0x14},
	{0x5216, 0x00},
	{0x5217, 0x02},
	{0x5218, 0x07},
	{0x521c, 0x00},
	{0x521e, 0x00},
	{0x522a, 0x3f},
	{0x522c, 0x00},
	{0x5230, 0x00},
	{0x5232, 0x05},
	{0x523a, 0x20},
	{0x523b, 0x34},
	{0x523c, 0x03},
	{0x523d, 0x00},
	{0x523e, 0x00},
	{0x523f, 0x70},
	{0x50e8, 0x16},
	{0x50e9, 0x00},
	{0x50eb, 0x0f},
	{0x4b11, 0x0f},
	{0x4b12, 0x0f},
	{0x4b31, 0x04},
	{0x4b3b, 0x02},
	{0x4b44, 0x80},
	{0x4b45, 0x00},
	{0x4b47, 0x00},
	{0x4b4e, 0x30},
	{0x4020, 0x60},
	{0x5100, 0x1b},
	{0x5101, 0x2b},
	{0x5102, 0x3b},
	{0x5103, 0x4b},
	{0x5104, 0x5f},
	{0x5105, 0x6f},
	{0x5106, 0x7f},
	{0x5108, 0x00},
	{0x5109, 0x00},
	{0x510a, 0x00},
	{0x510b, 0x00},
	{0x510c, 0x00},
	{0x510d, 0x00},
	{0x510e, 0x00},
	{0x5110, 0x0e},
	{0x5111, 0x0e},
	{0x5112, 0x0e},
	{0x5113, 0x0e},
	{0x5114, 0x0e},
	{0x5115, 0x0e},
	{0x5116, 0x0e},
	{0x5118, 0x09},
	{0x5119, 0x09},
	{0x511a, 0x09},
	{0x511b, 0x09},
	{0x511c, 0x09},
	{0x511d, 0x09},
	{0x511e, 0x09},
	{0x5120, 0xea},
	{0x5121, 0x6a},
	{0x5122, 0x6a},
	{0x5123, 0x6a},
	{0x5124, 0x6a},
	{0x5125, 0x6a},
	{0x5126, 0x6a},
	{0x5140, 0x0b},
	{0x5141, 0x1b},
	{0x5142, 0x2b},
	{0x5143, 0x3b},
	{0x5144, 0x4b},
	{0x5145, 0x5b},
	{0x5146, 0x6b},
	{0x5148, 0x02},
	{0x5149, 0x02},
	{0x514a, 0x02},
	{0x514b, 0x02},
	{0x514c, 0x02},
	{0x514d, 0x02},
	{0x514e, 0x02},
	{0x5150, 0x08},
	{0x5151, 0x08},
	{0x5152, 0x08},
	{0x5153, 0x08},
	{0x5154, 0x08},
	{0x5155, 0x08},
	{0x5156, 0x08},
	{0x5158, 0x02},
	{0x5159, 0x02},
	{0x515a, 0x02},
	{0x515b, 0x02},
	{0x515c, 0x02},
	{0x515d, 0x02},
	{0x515e, 0x02},
	{0x5160, 0x66},
	{0x5161, 0x66},
	{0x5162, 0x66},
	{0x5163, 0x66},
	{0x5164, 0x66},
	{0x5165, 0x66},
	{0x5166, 0x66},
	{0x5180, 0x00},
	{0x5189, 0x00},
	{0x5192, 0x00},
	{0x519b, 0x00},
	{0x51a4, 0x00},
	{0x51ad, 0x00},
	{0x51b6, 0x00},
	{0x51c0, 0x00},
	{0x5181, 0x00},
	{0x518a, 0x00},
	{0x5193, 0x00},
	{0x519c, 0x00},
	{0x51a5, 0x00},
	{0x51ae, 0x00},
	{0x51b7, 0x00},
	{0x51c1, 0x00},
	{0x5182, 0x85},
	{0x518b, 0x85},
	{0x5194, 0x85},
	{0x519d, 0x85},
	{0x51a6, 0x85},
	{0x51af, 0x85},
	{0x51b8, 0x85},
	{0x51c2, 0x85},
	{0x5183, 0x52},
	{0x518c, 0x52},
	{0x5195, 0x52},
	{0x519e, 0x52},
	{0x51a7, 0x52},
	{0x51b0, 0x52},
	{0x51b9, 0x52},
	{0x51c3, 0x52},
	{0x5184, 0x00},
	{0x518d, 0x00},
	{0x5196, 0x08},
	{0x519f, 0x08},
	{0x51a8, 0x08},
	{0x51b1, 0x08},
	{0x51ba, 0x08},
	{0x51c4, 0x08},
	{0x5185, 0x73},
	{0x518e, 0x73},
	{0x5197, 0x73},
	{0x51a0, 0x73},
	{0x51a9, 0x73},
	{0x51b2, 0x73},
	{0x51bb, 0x73},
	{0x51c5, 0x73},
	{0x5186, 0x34},
	{0x518f, 0xa4},
	{0x5198, 0x34},
	{0x51a1, 0x34},
	{0x51aa, 0x34},
	{0x51b3, 0x3f},
	{0x51bc, 0x3f},
	{0x51c6, 0x3f},
	{0x5187, 0x40},
	{0x5190, 0x38},
	{0x5199, 0x20},
	{0x51a2, 0x08},
	{0x51ab, 0x04},
	{0x51b4, 0x04},
	{0x51bd, 0x02},
	{0x51c7, 0x01},
	{0x5188, 0x20},
	{0x5191, 0x40},
	{0x519a, 0x40},
	{0x51a3, 0x40},
	{0x51ac, 0x40},
	{0x51b5, 0x78},
	{0x51be, 0x78},
	{0x51c8, 0x78},
	{0x51e1, 0x07},
	{0x51e3, 0x07},
	{0x51e5, 0x07},
	{0x51ed, 0x00},
	{0x51ee, 0x00},
	{0x4002, 0x2b},
	{0x3132, 0x00},
	{0x4024, 0x00},
	{0x5229, 0xfd},
	{0x4002, 0x2b},
	{0x3110, 0x03},
	{0x373d, 0x18},
	{0xbaa2, 0xc0},
	{0xbaa2, 0x40},
	{0xba90, 0x01},
	{0xba93, 0x02},
	{0x350d, 0x01},
	{0x3514, 0x00},
	{0x350c, 0x01},
	{0x3519, 0x00},
	{0x351a, 0x01},
	{0x351b, 0x1e},
	{0x351c, 0x90},
	{0x351e, 0x05},
	{0x351d, 0x05},
	{0x4b20, 0x9e},
	{0x4b18, 0x00},
	{0x4b3e, 0x00},
	{0x4b0e, 0x21},
	{0x4800, 0xac},
	{0x0104, 0x01},
	{0x0104, 0x00},
	{0x4801, 0xae},
	{0x0000, 0x00},
	/* {0x4026, 0xba},//color bar */
	/* {0x0601, 0x02},//color bar */
	/* {0x0100, 0x01},  */
	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1920*1080 */
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_25fps_dvp,
	}};

static enum v4l2_mbus_pixelcode sensor_mbus_code[] = {
	V4L2_MBUS_FMT_SBGGR10_1X10,
};

static struct regval_list sensor_stream_on_dvp[] = {
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_dvp[] = {
	{0x0100, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_on_mipi[] = {

	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {

	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct v4l2_subdev *sd, uint16_t reg, unsigned char *value) {
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
	struct i2c_msg msg[2] = {[0] =
					 {
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
	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct v4l2_subdev *sd, uint16_t reg, unsigned char value) {
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 3,
		.buf = buf,
	};
	int ret;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int sensor_read_array(struct v4l2_subdev *sd, struct regval_list *vals) {
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sensor_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		ISP_INFO("vals->reg_num:0x%02x, vals->value:0x%02x\n", vals->reg_num, val);
		vals++;
	}
	return 0;
}

static int sensor_write_array(struct v4l2_subdev *sd, struct regval_list *vals) {
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

static int sensor_reset(struct v4l2_subdev *sd, u32 val) {
	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd, unsigned int *ident) {
	unsigned char v;
	int ret;

	ret = sensor_read(sd, 0x0000, &v);
	ISP_INFO("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x0001, &v);
	ISP_INFO("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;
	return 0;
}

static int sensor_set_integration_time(struct v4l2_subdev *sd, int value) {
	int ret = 0;
	ret += sensor_write(sd, 0x0203, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x0202, (unsigned char)((value >> 8) & 0xff));
	ret += sensor_write(sd, 0x0104, 0x01);

	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct v4l2_subdev *sd, int value) {
	int ret = 0;

	ret += sensor_write(sd, 0x0205, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x0104, 0x01);
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_digital_gain(struct v4l2_subdev *sd, int value) {
	return 0;
}

static int sensor_get_black_pedestal(struct v4l2_subdev *sd, int value) {
	return 0;
}

static int sensor_init(struct v4l2_subdev *sd, u32 enable) {
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

static int sensor_s_stream(struct v4l2_subdev *sd, int enable) {
	int ret = 0;
	unsigned char val;
	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_on_dvp);
			msleep(100);
			while (0) {
				sensor_read(sd, 0x0005, &val);
				ISP_INFO("0005 is 0x%x\n", val);
				sensor_read(sd, 0x0100, &val);
				ISP_INFO("0x0100 is 0x%x\n", val);
				sensor_read(sd, 0x34c, &val);
				ISP_INFO("0x34c is 0x%x\n", val);
				sensor_read(sd, 0x34d, &val);
				ISP_INFO("0x34d is 0x%x\n", val);
				sensor_read(sd, 0x34e, &val);
				ISP_INFO("0x34e is 0x%x\n", val);
				sensor_read(sd, 0x34f, &val);
				ISP_INFO("0x34f is 0x%x\n", val);
			}
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);

		} else {
			ISP_INFO("Don't support this Sensor Data interface\n");
		}
		ISP_INFO("%s stream on\n", SENSOR_NAME);

	} else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);

		} else {
			ISP_INFO("Don't support this Sensor Data interface\n");
		}
		ISP_INFO("%s stream off\n", SENSOR_NAME);
	}
	return ret;
}

static int sensor_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms) {
	return 0;
}

static int sensor_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms) {
	return 0;
}

static int sensor_set_fps(struct tx_isp_sensor *sensor, int fps) {
	struct v4l2_subdev *sd = &sensor->sd;
	struct tx_isp_notify_argument arg;
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; // the format is 24.8
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		return -1;
	}
	sclk = SENSOR_SUPPORT_SCLK;

	val = 0;
	ret += sensor_read(sd, 0x342, &val);
	hts = val << 8;
	val = 0;
	ret += sensor_read(sd, 0x343, &val);
	hts |= val;
	if (0 != ret) {
		ISP_INFO("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += sensor_write(sd, 0x341, vts & 0xff);
	ret += sensor_write(sd, 0x340, (vts >> 8) & 0xff);
	ret += sensor_write(sd, 0x0104, 0x01);
	if (0 != ret) {
		ISP_INFO("err: sensor_write err\n");
		return ret;
	}
	sensor->video.fps = fps;

	sensor->video.attr->max_integration_time_native = vts - 2;
	sensor->video.attr->integration_time_limit = vts - 2;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 2;
	arg.value = (int)&sensor->video;
	sd->v4l2_dev->notify(sd, TX_ISP_NOTIFY_SYNC_VIDEO_IN, &arg);

	return ret;
}

static int sensor_set_mode(struct tx_isp_sensor *sensor, int value) {
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

static int sensor_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip) {
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if (reset_gpio != -1) {
		ret = gpio_request(reset_gpio, "sensor_reset");
		if (!ret) {
			gpio_direction_output(reset_gpio, 0);
			msleep(10);
			gpio_direction_output(reset_gpio, 1);
			msleep(10);
		} else {
			ISP_INFO("gpio request failed %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			gpio_direction_output(pwdn_gpio, 0);
			msleep(10);
			gpio_direction_output(pwdn_gpio, 1);
			msleep(10);
		} else {
			ISP_INFO("gpio request failed %d\n", pwdn_gpio);
		}
	}
	ret = sensor_detect(sd, &ident);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an %s chip.\n",
			client->addr,
			client->adapter->name,
			SENSOR_NAME);
		return ret;
	}
	v4l_info(client, "%s chip found @ 0x%02x (%s)\n", SENSOR_NAME, client->addr, client->adapter->name);
	return v4l2_chip_ident_i2c_client(client, chip, ident, 0);
}

static int sensor_s_power(struct v4l2_subdev *sd, int on) {
	return 0;
}

static long sensor_ops_private_ioctl(struct tx_isp_sensor *sensor, struct isp_private_ioctl *ctrl) {
	struct v4l2_subdev *sd = &sensor->sd;
	long ret = 0;
	switch (ctrl->cmd) {
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
		ret = sensor_set_mode(sensor, ctrl->value);
		break;
	case TX_ISP_PRIVATE_IOCTL_SUBDEV_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);

		} else {
			ISP_INFO("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_PRIVATE_IOCTL_SUBDEV_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);

		} else {
			ISP_INFO("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_PRIVATE_IOCTL_SENSOR_FPS:
		ret = sensor_set_fps(sensor, ctrl->value);
		break;
	default:
		ISP_INFO("do not support ctrl->cmd ====%d\n", ctrl->cmd);
		break;
	}
	return 0;
}

static long sensor_ops_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg) {
	struct tx_isp_sensor *sensor = container_of(sd, struct tx_isp_sensor, sd);
	int ret;
	switch (cmd) {
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
static int sensor_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg) {
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = sensor_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int sensor_s_register(struct v4l2_subdev *sd, const struct v4l2_dbg_register *reg) {
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	sensor_write(sd, reg->reg & 0xffff, reg->val & 0xff);
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

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id) {
	struct v4l2_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
	int ret;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_INFO("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_INFO("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	clk_set_rate(sensor->mclk, 24000000);
	clk_enable(sensor->mclk);

	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;

	sensor_attr.dbus_type = data_interface;
	if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
		wsize->regs = sensor_init_regs_1920_1080_25fps_dvp;
		memcpy((void *)(&(sensor_attr.dvp)), (void *)(&sensor_dvp), sizeof(sensor_dvp));
	} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
		wsize->regs = sensor_init_regs_1920_1080_25fps_mipi;
		memcpy((void *)(&(sensor_attr.mipi)), (void *)(&sensor_mipi), sizeof(sensor_mipi));
	} else {
		ISP_INFO("Don't support this Sensor Data Output Interface.\n");
		goto err_set_sensor_data_interface;
	}

	sensor_attr.max_again = 259142;
	sensor_attr.max_dgain = 0;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sensor_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	v4l2_i2c_subdev_init(sd, client, &sensor_ops);
	v4l2_set_subdev_hostdata(sd, sensor);
	ISP_INFO("probe ok ------->%s\n", SENSOR_NAME);
	return 0;
err_set_sensor_data_interface:
err_set_sensor_gpio:
	clk_disable(sensor->mclk);
	clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int sensor_remove(struct i2c_client *client) {
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

static const struct i2c_device_id sensor_id[] = {{SENSOR_NAME, 0}, {}};
MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver =
		{
			.owner = THIS_MODULE,
			.name = SENSOR_NAME,
		},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

static __init int init_sensor(void) {
	sensor_common_init(&sensor_info);
	return i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	sensor_common_exit();
	i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for " SENSOR_NAME " sensor");
MODULE_LICENSE("GPL");
