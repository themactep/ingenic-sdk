// SPDX-License-Identifier: GPL-2.0+
/*
 * gc4023.c
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

#define SENSOR_NAME "gc4023"
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x29
#define SENSOR_MAX_WIDTH 2560
#define SENSOR_MAX_HEIGHT 1440
#define SENSOR_CHIP_ID 0x4023
#define SENSOR_CHIP_ID_H (0x40)
#define SENSOR_CHIP_ID_L (0x23)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0x0000
#define SENSOR_SUPPORT_386RES_30FPS_SCLK 108*1000*1000
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20220510a"

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

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static int sensor_resolution = TX_SENSOR_RES_400;
module_param(sensor_resolution, int, S_IRUGO);
MODULE_PARM_DESC(sensor_resolution, "Sensor resolution");

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
 	{0x00, 0x00,0x00,0x00,0x07,0x0f,0x01,0x00,     0 },
 	{0x01, 0x80,0x02,0x00,0x07,0x0f,0x01,0x0B, 16208 },
 	{0x02, 0x01,0x00,0x00,0x07,0x0f,0x01,0x19, 32217 },
 	{0x03, 0x81,0x02,0x00,0x07,0x0f,0x01,0x2A, 47690 },
 	{0x04, 0x02,0x00,0x00,0x08,0x10,0x02,0x00, 65536 },
 	{0x05, 0x82,0x02,0x00,0x08,0x10,0x02,0x17, 81784 },
 	{0x06, 0x03,0x00,0x00,0x08,0x10,0x02,0x33, 97213 },
	{0x07, 0x83,0x02,0x00,0x09,0x11,0x03,0x14,113226 },
	{0x08, 0x04,0x00,0x00,0x0A,0x12,0x04,0x00,131072 },
	{0x09, 0x80,0x02,0x20,0x0A,0x12,0x04,0x2F,147001 },
	{0x0A, 0x01,0x00,0x20,0x0B,0x13,0x05,0x26,162766 },
	{0x0B, 0x81,0x02,0x20,0x0C,0x14,0x06,0x28,178990 },
	{0x0C, 0x02,0x00,0x20,0x0D,0x15,0x08,0x00,196608 },
	{0x0D, 0x82,0x02,0x20,0x0D,0x15,0x09,0x1E,212696 },
	{0x0E, 0x03,0x00,0x20,0x0E,0x16,0x0B,0x0C,228446 },
	{0x0F, 0x83,0x02,0x20,0x0E,0x16,0x0D,0x11,244419 },
	{0x10, 0x04,0x00,0x20,0x0F,0x17,0x10,0x00,262144 },
	{0x11, 0x84,0x02,0x20,0x0F,0x17,0x12,0x3D,278158 },
	{0x12, 0x05,0x00,0x20,0x10,0x18,0x16,0x19,293982 },
	{0x13, 0x85,0x02,0x20,0x10,0x18,0x1A,0x22,310012 },
	{0x14, 0xb5,0x04,0x20,0x11,0x19,0x20,0x00,327680 },
	{0x15, 0x85,0x05,0x20,0x11,0x19,0x25,0x3A,343731 },
	{0x16, 0x05,0x08,0x20,0x12,0x1a,0x2C,0x33,359484 },
	{0x17, 0x45,0x09,0x20,0x15,0x1d,0x35,0x05,375550 },
	{0x18, 0x55,0x0a,0x20,0x16,0x1e,0x40,0x00,393216 },
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;

	*sensor_it = expo;

	return isp_it;
}
unsigned int sensor_alloc_integration_time_short(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;
#if 0
	if (expo % 2 == 0)
		expo = expo - 1;
	if (expo < sensor_attr.min_integration_time_short)
		expo = 3;
	isp_it = expo << shift;
	expo = (expo - 1) / 2;
	if (expo < 0)
		expo = 0;
#endif
	*sensor_it = expo;

	return isp_it;
}

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
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

	return 0;
}
unsigned int sensor_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again_short) {
		if (isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->gain;
			return (lut - 1)->gain;
		}
		else {
			if ((lut->gain == sensor_attr.max_again_short) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
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

struct tx_isp_mipi_bus sensor_mipi_linear = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 864,
	.lans = 2,
	.settle_time_apative_en = 1,
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
	.mipi_sc.data_type_value = 0,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};
struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x4023,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
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
	.sensor_ctrl.alloc_again_short = sensor_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	.sensor_ctrl.alloc_integration_time_short = sensor_alloc_integration_time_short,
	// void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list sensor_init_regs_2560_1440_25fps_mipi[] = {
	/*
	 * version 0.2 mclk 27Mhz wpclk 216Mhz rpclk 172.2Mhz mipi 864Mbps/lane
	 * cpclk 27Mhz vts = 1500 window 2560 1440
	 */
	/*SYSTEM*/
   	{0x03fe, 0xf0},
   	{0x03fe, 0x00},
   	{0x03fe, 0x10},
   	{0x03fe, 0x00},
   	{0x0a38, 0x00},
   	{0x0a38, 0x01},
   	{0x0a20, 0x07},
   	{0x061c, 0x50},
   	{0x061d, 0x22},
   	{0x061e, 0x78},
   	{0x061f, 0x06},
   	{0x0a21, 0x10},
   	{0x0a34, 0x40},
   	{0x0a35, 0x01},
   	{0x0a36, 0x60},
   	{0x0a37, 0x06},
   	{0x0314, 0x50},
   	{0x0315, 0x00},
   	{0x031c, 0xce},
   	{0x0219, 0x47},
   	{0x0342, 0x04},
   	{0x0343, 0xb0},
   	{0x0259, 0x05},
   	{0x025a, 0xa0},
   	{0x0340, 0x05},
   	{0x0341, 0xdc},
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
   	{0x072a, 0x18},
   	{0x072b, 0x08},
   	{0x1466, 0x10},
   	{0x1468, 0x0f},
   	{0x1467, 0x07},
   	{0x1469, 0x80},
   	{0x146a, 0xbc},
   	{0x0707, 0x07},
   	{0x0737, 0x0f},
   	{0x061a, 0x00},
   	{0x1430, 0x80},
   	{0x1407, 0x10},
   	{0x1408, 0x16},
   	{0x1409, 0x03},
   	{0x146d, 0x0e},
   	{0x146e, 0x32},
   	{0x146f, 0x33},
   	{0x1470, 0x2c},
   	{0x1471, 0x2d},
   	{0x1472, 0x3a},
   	{0x1473, 0x3a},
   	{0x1474, 0x40},
   	{0x1475, 0x36},
   	{0x1420, 0x14},
   	{0x1464, 0x15},
   	{0x146c, 0x40},
   	{0x146d, 0x40},
   	{0x1423, 0x08},
   	{0x1428, 0x10},
   	{0x1462, 0x08},
   	{0x02ce, 0x04},
   	{0x143a, 0x0b},
   	{0x142b, 0x88},
   	{0x0245, 0xc9},
   	{0x023a, 0x08},
   	{0x02cd, 0x88},
   	{0x0612, 0x02},
   	{0x0613, 0xc7},
   	{0x0243, 0x03},
   	{0x021b, 0x09},
   	{0x0089, 0x03},
   	{0x0040, 0xa3},
   	{0x0075, 0x64},
   	{0x0004, 0x0f},
   	{0x0002, 0xa9},
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
   	{0x0100, 0x09},

	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* [0] 2560*1440 @max 30fps*/
	{
		.width = 2560,
		.height = 1440,
		.fps = 30 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2560_1440_25fps_mipi,
	}

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

int sensor_read(struct tx_isp_subdev *sd,  uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg>>8)&0xff, reg&0xff};
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
static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
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
		pr_debug("vals->reg_num:0x%x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}
	return 0;
}
#endif

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
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

static int sensor_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = sensor_read(sd, 0x03f0, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	ret = sensor_read(sd, 0x03f1, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

#if 0
static int sensor_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	struct again_lut *val_lut = sensor_again_lut;

	/* printk("it is %d, again is %d\n",expo,again); */
	/*expo*/
	ret = sensor_write(sd, 0x0203, value & 0xff);
	ret += sensor_write(sd, 0x0202, value >> 8);
	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d\n" ,__LINE__ );
		return ret;
	}

	/*gain*/

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

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = sensor_write(sd, 0x0203, value & 0xff);
	ret += sensor_write(sd, 0x0202, value >> 8);
	if (ret < 0)
		ISP_ERROR("sensor_write error  %d\n" ,__LINE__ );

	return ret;
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
		ret = sensor_write_array(sd, sensor_stream_on);
		pr_debug("%s stream on\n", SENSOR_NAME);
	} else {
		ret = sensor_write_array(sd, sensor_stream_off);
		pr_debug("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int wpclk = 0;
	unsigned short vts = 0;
	unsigned short hts=0;
	unsigned char tmp;
	unsigned int max_fps = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	wpclk = SENSOR_SUPPORT_386RES_30FPS_SCLK;

	max_fps = sensor_max_fps;

	/* the format of fps is 16/16. for example 30 << 16 | 2, the value is 30/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}
	ret += sensor_read(sd, 0x0342, &tmp);
	hts = tmp & 0x0f;
	ret += sensor_read(sd, 0x0343, &tmp);
	if (ret < 0)
		return -1;
	hts = ((hts << 8) + tmp) << 1;

	vts = wpclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16) ;
	ret = sensor_write(sd, 0x0340, (unsigned char)((vts & 0x3f00) >> 8));
	ret += sensor_write(sd, 0x0341, (unsigned char)(vts & 0xff));
	if (ret < 0)
		return -1;

	sensor->video.fps = fps;
	sensor->video.attr->integration_time_limit = vts - 8;
	sensor->video.attr->max_integration_time_native = vts - 8;
	sensor->video.attr->max_integration_time = vts - 8;
	sensor->video.attr->total_height = vts;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;
}

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = -1;
	unsigned char val = 0x0;

	ret = sensor_read(sd, 0x022c, &val);

	if (enable & 0x2)
		val |= 0x02;
	else
		val &= 0xfd;

	ret += sensor_write(sd, 0x022c, val);
	if (ret < 0)
		ISP_ERROR("sensor_write error  %d" ,__LINE__ );

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

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
		struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

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
			ISP_ERROR("gpio request fail %d\n",reset_gpio);
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
			ISP_ERROR("gpio request fail %d\n",pwdn_gpio);
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

#if 0
static int sensor_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = sensor_write(sd, 0x0201, value & 0xff);
	ret += sensor_write(sd, 0x0200, (value>>8)&0x3f);
	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}
#endif

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
#if 0
		case TX_ISP_EVENT_SENSOR_EXPO:
			if (arg)
				ret = sensor_set_expo(sd, *(int*)arg);
			break;
#else
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			if (arg)
				ret = sensor_set_integration_time(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			if (arg)
				ret = sensor_set_analog_gain(sd, *(int*)arg);
			break;
#endif
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
	/*.ioctl = sensor_ops_ioctl,*/
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

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	unsigned long rate = 0;
	int ret;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
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

	wsize = &sensor_win_sizes[0];
	sensor_max_fps = TX_SENSOR_MAX_FPS_30;
	sensor_attr.data_type = data_type;
	memcpy(&sensor_attr.mipi, &sensor_mipi_linear, sizeof(sensor_mipi_linear));
	sensor_attr.one_line_expr_in_us = 22;
	sensor_attr.total_width = 3840;
	sensor_attr.total_height = 1500;
	sensor_attr.max_integration_time_native = 1500 - 8;
	sensor_attr.integration_time_limit = 1500 - 8;
	sensor_attr.max_integration_time = 1500 - 8;
	sensor_attr.max_integration_time_short = 8;
	sensor_attr.data_type = data_type;
	sensor_attr.dbus_type = data_interface;

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
		ISP_ERROR("Failed to init %s driver.\n", SENSOR_NAME);
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
