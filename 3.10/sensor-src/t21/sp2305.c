// SPDX-License-Identifier: GPL-2.0+
/*
 * sp2305.c
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
#include <tx-isp-common.h>
#include <sensor-common.h>
#include <sensor-info.h>

#define SENSOR_NAME "sp2305"
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x3c
#define SENSOR_MAX_WIDTH 1920
#define SENSOR_MAX_HEIGHT 1080
#define SENSOR_CHIP_ID 0x2735
#define SENSOR_CHIP_ID_H (0x27)
#define SENSOR_CHIP_ID_L (0x35)
#define SENSOR_REG_END 0xff
#define SENSOR_REG_DELAY 0x0
#define SENSOR_PAGE_REG 0xfd
#define SENSOR_SUPPORT_SCLK (84000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20191120a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = GPIO_PA(22);
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_HIGH_10BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_25;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

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
    unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};
struct again_lut sensor_again_lut[] = {
	{0x10,	0},
	{0x11,	5731},
	{0x12,	11136},
	{0x13,	16248},
	{0x14,	21097},
	{0x15,	25710},
	{0x16,	30109},
	{0x17,	34312},
	{0x18,	38336},
	{0x19,	42195},
	{0x1a,	45904},
	{0x1b,	49472},
	{0x1c,	52910},
	{0x1d,	56228},
	{0x1e,	59433},
	{0x1f,	62534},
	{0x20,	65536},
	{0x22,	71267},
	{0x24,	76672},
	{0x26,	81784},
	{0x28,	86633},
	{0x2a,	91246},
	{0x2c,	95645},
	{0x2e,	99848},
	{0x30,	103872},
	{0x32,	107731},
	{0x34,	111440},
	{0x36,	115008},
	{0x38,	118446},
	{0x3a,	121764},
	{0x3c,	124969},
	{0x3e,	128070},
	{0x40,	131072},
	{0x44,	136803},
	{0x48,	142208},
	{0x4c,	147320},
	{0x50,	152169},
	{0x54,	156782},
	{0x58,	161181},
	{0x5c,	165384},
	{0x60,	169408},
	{0x64,	173267},
	{0x68,	176976},
	{0x6c,	180544},
	{0x70,	183982},
	{0x74,	187300},
	{0x78,	190505},
	{0x7c,	193606},
	{0x80,	196608},
	{0x88,	202339},
	{0x90,	207744},
	{0x98,	212856},
	{0xa0,	217705},
	{0xa8,	222318},
	{0xb0,	226717},
	{0xb8,	230920},
	{0xc0,	234944},
	{0xc8,	238803},
	{0xd0,	242512},
	{0xd8,	246080},
	{0xe0,	249518},
	{0xe8,	252836},
	{0xf0,	256041},
	{0xf8,	259142},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;

	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
		}
		else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else {
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

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x2735,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
		.polar = {
			.hsync_polar = 0,
			.vsync_polar = 2,
			.pclk_polar = 2,
		},
	},
	.max_again = 259142,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 0x63b - 4,
	.integration_time_limit = 0x63b - 4,
	.total_width = 0x83a,
	.total_height = 0x63b,
	.max_integration_time = 0x63b - 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 1,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	// void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list sensor_init_regs_1920_1080_25fps_dvp[] = {
	{0xfd, 0x00},
	{SENSOR_REG_DELAY, 0x05},
	{0xfd, 0x00},
	{0x2f, 0x10},
	{0x34, 0x00},
	{0x30, 0x15},
	{0x33, 0x01},
	{0x35, 0x20},
	{0x1d, 0xa5},
	{0xfd, 0x01},
	{0x0d, 0x00},
	{0x30, 0x00},
	{0x03, 0x01},
	{0x04, 0x8f},
	{0x01, 0x01},
	{0x09, 0x00},
	{0x0a, 0x20},
	{0x06, 0x0a},
	{0x24, 0x10},
	{0x01, 0x01},
	{0xfb, 0x73},
	{0x01, 0x01},
	{0xfd, 0x01},
	{0x1a, 0x6b},
	{0x1c, 0xea},
	{0x16, 0x0c},
	{0x21, 0x00},
	{0x11, 0x63},
	{0x19, 0xc3},
	{0x26, 0xda},
	{0x29, 0x01},
	{0x33, 0x6f},
	{0x2a, 0xd2},
	{0x2c, 0x40},
	{0xd0, 0x02},
	{0xd1, 0x01},
	{0xd2, 0x20},
	{0xd3, 0x04},
	{0xd4, 0x2a},
	{0x50, 0x00},
	{0x51, 0x2c},
	{0x52, 0x29},
	{0x53, 0x00},
	{0x55, 0x44},
	{0x58, 0x29},
	{0x5a, 0x00},
	{0x5b, 0x00},
	{0x5d, 0x00},
	{0x64, 0x2f},
	{0x66, 0x62},
	{0x68, 0x5b},
	{0x75, 0x46},
	{0x76, 0xf0},
	{0x77, 0x4f},
	{0x78, 0xef},
	{0x72, 0xcf},
	{0x73, 0x36},
	{0x7d, 0x0d},
	{0x7e, 0x0d},
	{0x8a, 0x77},
	{0x8b, 0x77},
	{0xfd, 0x01},
	{0xb1, 0x82},
	{0xb3, 0x0b},
	{0xb4, 0x14},
	{0x9d, 0x40},
	{0xa1, 0x05},
	{0x94, 0x44},
	{0x95, 0x33},
	{0x96, 0x1f},
	{0x98, 0x45},
	{0x9c, 0x10},
	{0xb5, 0x70},
	{0xa0, 0x00},
	{0x25, 0xe0},
	{0x20, 0x7b},
	{0x8f, 0x88},
	{0x91, 0x40},
	{0xfd, 0x01},
	{0xfd, 0x02},
	{0x5e, 0x03},
	{0xfd, 0x02},
	{0x60, 0xf0},
	{0xa1, 0x04},
	{0xa3, 0x40},
	{0xa5, 0x02},
	{0xa7, 0xc4},
	{0xfd, 0x01},
	{0x86, 0x77},
	{0x89, 0x77},
	{0x87, 0x74},
	{0x88, 0x74},
	{0xfc, 0xe0},
	{0xfe, 0xe0},
	{0xf0, 0x40},
	{0xf1, 0x40},
	{0xf2, 0x40},
	{0xf3, 0x40},
	{0xfd, 0x02},
	{0x36, 0x08},
	{0xa0, 0x00},
	{0xa1, 0x08},
	{0xa2, 0x04},
	{0xa3, 0x38},
	{0xa4, 0x00},
	{0xa5, 0x04},
	{0xa6, 0x03},
	{0xa7, 0xc0},
	{0xfd, 0x03},
	{0xc0, 0x01},//OTP transf
	{0xfd, 0x04},
	{0x22, 0x14},
	{0x23, 0x14},
	{0xfd, 0x01},
	{0x06, 0xe0},
	{0x01, 0x01},
	{0xfd, 0x00},
	{0x1b, 0x00},
	{0xfd, 0x01},
	{0x0d, 0x10},
	{0x0e, 0x06},
	{0x0f, 0x3b},
	{0x01, 0x01},

	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_15fps_dvp[] = {
	{0xfd, 0x00},
	{0x20, 0x01},
	/*{SENSOR_REG_DELAY, 200},*/
	{0xfd, 0x00},
	{0x2f, 0x10},
	{0x34, 0x00},
	{0x30, 0x15},
	{0x33, 0x01},
	{0x35, 0x20},
	{0x1b, 0x00},
	{0x1d, 0xa5},
	{0xfd, 0x01},
	{0x0d, 0x00},
	{0x30, 0x00},
	{0x03, 0x01},
	{0x04, 0x8f},
	{0x01, 0x01},
	{0x09, 0x00},
	{0x0a, 0x20},
	{0x06, 0x0a},
	{0x24, 0x10},
	{0x01, 0x01},
	{0xfb, 0x73},
	{0x01, 0x01},
	{0xfd, 0x01},
	{0x1a, 0x6b},
	{0x1c, 0xea},
	{0x16, 0x0c},
	{0x21, 0x00},
	{0x11, 0xe8},//	;63
	{0x19, 0xc3},
	{0x26, 0xda},
	{0x29, 0x01},
	{0x33, 0x6f},
	{0x2a, 0xd2},
	{0x2c, 0x40},
	{0xd0, 0x02},
	{0xd1, 0x01},
	{0xd2, 0x20},
	{0xd3, 0x03},//	;04
	{0xd4, 0xa4},//	;2a
	{0x50, 0x00},
	{0x51, 0x2c},
	{0x52, 0x29},
	{0x53, 0x00},
	{0x55, 0x44},
	{0x58, 0x29},
	{0x5a, 0x00},
	{0x5b, 0x00},
	{0x5d, 0x00},
	{0x64, 0x2f},
	{0x66, 0x62},
	{0x68, 0x5b},
	{0x75, 0x46},
	{0x76, 0xf0},
	{0x77, 0x4f},
	{0x78, 0xef},
	{0x72, 0xcf},
	{0x73, 0x36},
	{0x7d, 0x0d},
	{0x7e, 0x0d},
	{0x8a, 0x77},
	{0x8b, 0x77},
	{0xfd, 0x01},
	{0xb1, 0x82},//;DPHY enable 8b
	{0xb3, 0x0b},//;0b;09;1d
	{0xb4, 0x14},//;MIPI PLL enable;14;35;36
	{0x9d, 0x40},//;mipi hs dc level 40/03/55
	{0xa1, 0x05},//;speed/03
	{0x94, 0x44},//;dphy time
	{0x95, 0x33},//;dphy time
	{0x96, 0x1f},//;dphy time
	{0x98, 0x45},//;dphy time
	{0x9c, 0x10},//;dphy time
	{0xb5, 0x70},//;30
	{0xa0, 0x00},//;mipi enable
	{0x25, 0xe0},
	{0x20, 0x7b},
	{0x8f, 0x88},
	{0x91, 0x40},
	{0xfd, 0x01},
	{0xfd, 0x02},
	{0x36, 0x08},//	;invert Vsync polarity
	{0x5e, 0x03},
	{0xfd, 0x02},
	{0xa1, 0x04},
	{0xa3, 0x40},
	{0xa5, 0x02},
	{0xa7, 0xc4},
	{0xfd, 0x01},
	{0x86, 0x77},
	{0x89, 0x77},
	{0x87, 0x74},
	{0x88, 0x74},
	{0xfc, 0xe0},
	{0xfe, 0xe0},
	{0xf0, 0x40},
	{0xf1, 0x40},
	{0xf2, 0x40},
	{0xf3, 0x40},
	{0xfd, 0x02},
	{0x36, 0x08},//	;invert Vsync polarity for Gaia
	{0xa0, 0x00},//	;Image vertical start MSB3bits
	{0xa1, 0x08},//	;Image vertical start LSB8bits
	{0xa2, 0x04},//	;image vertical size  MSB8bits
	{0xa3, 0x38},//	;image vertical size  LSB8bits
	{0xa4, 0x00},
	{0xa5, 0x04},//	;H start 8Lsb
	{0xa6, 0x03},
	{0xa7, 0xc0},//	;Half H size Lsb8bits
	{0xfd, 0x03},
	{0xc0, 0x01},//	;enable transfer OTP BP information
	{0xfd, 0x04},
	{0x21, 0x14},
	{0x22, 0x14},
	{0x23, 0x14},//	;enhance normal and dummy BPC
	{0xfd, 0x01},
	{0x05, 0x06},
	{0x06, 0x11}, //;e0	;insert dummy line , the frame rate is 30.01.
	{0x01, 0x01},//	;
	{0xfd, 0x00},
	{0x1b, 0x00},//	;enable output
	{0xfd, 0x01},//	;
	{0x03, 0x03},//Mirror_Off_Flip_Off
	{0xfd, 0x01},
	{0x3f, 0x00},// 03
	{0xf8, 0x00},// 02
	{0x01, 0x01},
	{0xfd, 0x02},
	{0x62, 0x01},//	;full size mode flip off row start for OTP BPC
	{0x63, 0x00},
	{0xfd, 0x01},
	{0x0d, 0x10},
	{0x0e, 0x0a},
	{0x0f, 0x63},

	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1280*960 */
	{
		.width = 1920,
		.height = 1080,
		.fps = 15 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,//V4L2_MBUS_FMT_SGBRG10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_15fps_dvp,
	}
};

//static enum v4l2_mbus_pixelcode sensor_mbus_code[] = {
//	V4L2_MBUS_FMT_SBGGR10_1X10,//V4L2_MBUS_FMT_SGBRG10_1X10,
//};


static struct regval_list sensor_stream_on[] = {
	{0xfd, 0x00},
	{0x36, 0x00},
	{0x37, 0x00},//fake stream on

	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	{0xfd, 0x00},
	{0x36, 0x01},
	{0x37, 0x01},//fake stream off

	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, unsigned char reg, unsigned char *value)
{
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
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int sensor_write(struct tx_isp_subdev *sd, unsigned char reg,
			unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
		.buf = buf,
	};
	int ret;
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
			if (vals->reg_num == SENSOR_PAGE_REG) {
				val &= 0xf8;
				val |= (vals->value & 0x07);
				ret = sensor_write(sd, vals->reg_num, val);
				ret = sensor_read(sd, vals->reg_num, &val);
			}
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
	int ret;
	unsigned char v;
	unsigned char page=0;
	sensor_read(sd, 0xfd, &page);
	if (page!=0) {
		sensor_write(sd, 0xfd, 0x0);
	}

	ret = sensor_read(sd, 0x02, &v);
	printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;
	ret = sensor_read(sd, 0x03, &v);
	printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;


	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{

	int ret = 0;
	ret = sensor_write(sd, 0xfd, 0x01);
	ret += sensor_write(sd, 0x4, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3, (unsigned char)((value & 0xff00) >> 8));
	ret += sensor_write(sd, 0x01, 0x01);
	if (ret < 0) {
		printk("sensor_write error  %d\n" ,__LINE__);
		return ret;
	}
	return 0;

}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret = sensor_write(sd, 0xfd, 0x01);
	ret += sensor_write(sd, 0x24, (unsigned char)value);
	ret += sensor_write(sd, 0x01, 0x01);
	if (ret < 0) {
		printk("sensor_again_write error  %d\n" ,__LINE__);
		return ret;
	}
	return 0;
}

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

	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		wsize->regs = sensor_init_regs_1920_1080_25fps_dvp;
		wsize->fps = 25 << 16 | 1;
		break;
	case TX_SENSOR_MAX_FPS_15:
		wsize->regs = sensor_init_regs_1920_1080_15fps_dvp;
		wsize->fps = 15 << 16 | 1;
		break;
	default:
		printk("Now we do not support this framerate!!!\n");
	}
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
		ret = sensor_write_array(sd, sensor_stream_on);
		pr_debug("%s stream on\n", SENSOR_NAME);
	}
	else {
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
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0;
	int ret = 0;

	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		sclk = SENSOR_SUPPORT_SCLK;
		max_fps = SENSOR_OUTPUT_MAX_FPS;
		break;
	case TX_SENSOR_MAX_FPS_15:
		sclk = SENSOR_SUPPORT_SCLK;
		max_fps = TX_SENSOR_MAX_FPS_15;
		break;
	default:
		printk("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		printk("warn: fps(%d) not in range\n", fps);
		return -1;
	}

	ret = sensor_write(sd, 0xfd, 0x01);
	if (ret < 0)
		return ret;
	ret = sensor_read(sd, 0x8c, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0x8d, &tmp);
	if (0 != ret) {
		printk("err: sensor_write err\n");
		return ret;
	}
	hts = (hts << 8) + tmp;
	vts = sclk * (fps & 0xffff) / 2 / hts / ((fps & 0xffff0000) >> 16);

	ret = 0;
	ret += sensor_write(sd, 0xfd, 0x01);
	ret += sensor_write(sd, 0x0e, (vts >> 8) & 0xff);
	ret += sensor_write(sd, 0x0f, vts & 0xff);
	ret += sensor_write(sd, 0x01, 0x01);
	if (0 != ret) {
		printk("err: sensor_write err\n");
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

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
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
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(50);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		} else {
			printk("gpio request fail %d\n", pwdn_gpio);
		}
	}
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(30);
		} else {
			printk("gpio request fail %d\n",reset_gpio);
		}
	}
	ret = sensor_detect(sd, &ident);
	if (ret) {
		printk("chip found @ 0x%x (%s) is not an %s chip.\n",
		       client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}
	printk("%s chip found @ 0x%02x (%s)\n",
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
	long ret = 0;
	if (IS_ERR_OR_NULL(sd)) {
		printk("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sensor_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sensor_set_analog_gain(sd, *(int*)arg);
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
		ret = sensor_write_array(sd, sensor_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sensor_write_array(sd, sensor_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, *(int*)arg);
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

static int sensor_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
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
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;

	sensor_attr.dvp.gpio = sensor_gpio_func;
#if 0
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
	switch (sensor_max_fps) {
	case TX_SENSOR_MAX_FPS_25:
		break;
	case TX_SENSOR_MAX_FPS_15:
		sensor_attr.max_integration_time_native = 0xa63 - 4;
		sensor_attr.integration_time_limit = 0xa63 - 4;
		sensor_attr.total_width = 0x83a;
		sensor_attr.total_height = 0xa63;
		sensor_attr.max_integration_time = 0xa63 - 4;
		break;
	default:
		printk("Now we do not support this framerate!!!\n");
	}
	/*
	  convert sensor-gain into isp-gain,
	*/
	sensor_attr.max_again = 259142;
	sensor_attr.max_dgain = 0;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sensor_attr;
	sensor->video.mbus_change = 1;
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
err_set_sensor_gpio:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
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
