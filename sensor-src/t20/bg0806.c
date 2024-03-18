/*
 * 20161117
 * bg0806.c
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
#include <apical-isp/apical_math.h>
#include <linux/proc_fs.h>
#include <soc/gpio.h>

#define SENSOR_CHIP_ID_H (0x08)
#define SENSOR_CHIP_ID_L (0x06)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK (75*1000*1000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVER_VERSION "BG080620170310"
#define DRIVE_CAPABILITY_1

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
	uint16_t reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct tx_isp_sensor_attribute bg0806_attr;

unsigned int bg0806_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	unsigned int gain_one = 0;
	unsigned int gain_one1 = 0;
	uint16_t regs = 0;
	unsigned int isp_gain1 = 0;
	uint32_t mask;

	/* low 4 bits are fraction bits */
	gain_one = math_exp2(isp_gain, shift, TX_ISP_GAIN_FIXED_POINT);
	if (gain_one >= (uint32_t)(15.5*(1<<TX_ISP_GAIN_FIXED_POINT)))
		gain_one = (uint32_t)(15.5*(1<<TX_ISP_GAIN_FIXED_POINT));

	regs = gain_one>>(TX_ISP_GAIN_FIXED_POINT-6);
	*sensor_again = regs;

	mask = ~0;
	mask = mask >> (TX_ISP_GAIN_FIXED_POINT-4);
	mask = mask << (TX_ISP_GAIN_FIXED_POINT-4);
	gain_one1 = gain_one&mask;
	isp_gain1 = log2_fixed_to_fixed(gain_one1, TX_ISP_GAIN_FIXED_POINT, shift);
	return isp_gain1;
}

unsigned int bg0806_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return isp_gain;
}

struct tx_isp_mipi_bus bg0806_mipi={
	.clk = 800,
	.lans = 1,
};
struct tx_isp_dvp_bus bg0806_dvp={
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking = {
		.vblanking = 0,
		.hblanking = 0,
	},
};

struct tx_isp_sensor_attribute bg0806_attr={
	.name = "bg0806",
	.chip_id = 0x0806,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x32,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
	},
	.max_again = 390214,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 0x546-4,
	.integration_time_limit = 0x546-4,
	.total_width = 0x8ae,
	.total_height = 0x546,
	.max_integration_time = 0x546-4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = bg0806_alloc_again,
	.sensor_ctrl.alloc_dgain = bg0806_alloc_dgain,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};


static struct regval_list bg0806_init_regs_1920_1080_30fps_mipi[] = {
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list bg0806_init_regs_1920_1080_30fps_dvp[] = {
	/*
	  @@ DVP interface 1920*1080 30fps
	*/
	{0x0200, 0x0001},
	{0x000e, 0x0008},//0806_4times_74.25M_30fps
	{0x000f, 0x00ae},//row time F_W=2200 //0x0098
	{0x0013, 0x0001},
	{0x0021, 0x0001},
	{0x0022, 0x000e},//vblank //0025
	{0x0028, 0x0000},
	{0x0029, 0x0030},
	{0x002a, 0x0000},
	{0x002b, 0x0030},
	{0x0030, 0x0000},
	{0x0031, 0x00d0},//rstb2rmp1 gap
	{0x0034, 0x0000},
	{0x0035, 0x00d0},//tx2rmp2 gap
	{0x003c, 0x0001},
	{0x003d, 0x0084},//rmp1_w@pclk domain  //0x80
	{0x003e, 0x0005},//ncp
	{0x0042, 0x0001},
	{0x005c, 0x0000},//lsh_io ctrlbit for 1.8VDDIO
	{0x0061, 0x0004},
	{0x0062, 0x005c},//rmp2_w@pclk domain //0x50
	{0x0064, 0x0000},
	{0x0065, 0x0080},//rmp1_w@rmpclk domain
	{0x0067, 0x0001},
	{0x0068, 0x0090},//rmp2_w@rmpclk domain
	{0x006c, 0x0003},//pd mipi dphy&dphy ldo
	{0x007f, 0x0000},
	{0x0080, 0x0001}, //dot en disable
	{0x0081, 0x0000},
	{0x0082, 0x000b},
	{0x0084, 0x0008},
	{0x0088, 0x0005},//pclk dly
	{0x008e, 0x0000},
	{0x008f, 0x0000},
	{0x0090, 0x0001},//hxb 20170210  change system voltage 2.8V to 3.3V
	{0x0094, 0x0001},//rmp div1
	{0x0095, 0x0001},//rmp div4
	{0x009e, 0x0003},//4 times
	{0x009f, 0x0020},//rmp2gap@rmpclk
	{0x00b1, 0x002f},
	{0x00b2, 0x0002},
	{0x00bc, 0x0002},
	{0x00bd, 0x0000},
	{0x0120, 0x0001},//blc on 01 direct

	{0x0132, 0x0001},//k
	{0x0206, 0x0002},
	{0x006e, 0x0000},

	{0x0139, 0x0007},
	{0x0139, 0x00ff},
	{0x013b, 0x0008},
	{0x01a5, 0x0007},//row noise on 07
	{0x0160, 0x0000},
	{0x0161, 0x0030},
	{0x0162, 0x0000},
	{0x0163, 0x0030},
	{0x0164, 0x0000},
	{0x0165, 0x0030},
	{0x0166, 0x0000},
	{0x0167, 0x0030},
	{0x00f3, 0x0000},
	{0x00f4, 0x0049}, //plm
	{0x00f5, 0x0002}, //pln, 445.5M pll

	{0x0053, 0x0000},//hxb20161121
	{0x0054, 0x0028},//hxb20161121
	{0x0055, 0x0000},//hxb20161121

	{0x006d, 0x000c},//pclk_ctrl, 03 for 1p25,0c for 1p5
	{0x006c, 0x0000},//ldo_ctrl,00
	{0x008d, 0x003f},//
	{0x008e, 0x0000},//oen_ctrl,0c
	{0x008f, 0x0000},//io_sel_ctrl,03
	{0x00fa, 0x008F},//ispc,c7
	{0x0391, 0x0001},//mipi_ctrl1,(raw12)
	{0x0392, 0x0000},//mipi_ctrl2,default
	{0x0393, 0x0001},//mipi_ctrl3,default
	{0x0398, 0x0008},//28(25M),14(50M),0a(100M),08(111.375)
	{0x0390, 0x0000},//mipi_ctrl0,bit[1],mipi enable
	{0x001d, 0x0001},
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

const unsigned char Tab_sensor_dsc[768] = {
 0x0a, 0x5c, 0x09, 0x4a, 0x08, 0x07, 0x06, 0xee, 0x06, 0x23, 0x05, 0x7e, 0x05, 0x21, 0x05, 0x01, 0x05, 0xcc, 0x06, 0xf5,
 0x08, 0x0b, 0x09, 0x0e, 0x0a, 0x0e, 0x0b, 0x34, 0x0c, 0x42, 0x0e, 0x9b, 0x09, 0xe0, 0x08, 0x71, 0x06, 0xce, 0x05, 0xd7,
 0x05, 0x19, 0x04, 0x9c, 0x04, 0x54, 0x04, 0x4d, 0x04, 0xd2, 0x05, 0xf8, 0x07, 0x04, 0x07, 0xe3, 0x08, 0xe2, 0x09, 0xfe,
 0x0b, 0x69, 0x0d, 0xfa, 0x09, 0x62, 0x07, 0x83, 0x05, 0xd8, 0x04, 0xe6, 0x04, 0x44, 0x03, 0xe5, 0x03, 0x82, 0x03, 0x88,
 0x04, 0x07, 0x05, 0x09, 0x05, 0xfe, 0x06, 0xc5, 0x07, 0xbf, 0x08, 0xe2, 0x0a, 0x68, 0x0d, 0x59, 0x08, 0xf7, 0x06, 0xea,
 0x05, 0x43, 0x04, 0x55, 0x03, 0xbf, 0x03, 0x43, 0x03, 0x09, 0x02, 0xf7, 0x03, 0x83, 0x04, 0x75, 0x05, 0x5c, 0x06, 0x09,
 0x06, 0xf5, 0x08, 0x1c, 0x09, 0xa6, 0x0c, 0xc6, 0x08, 0x6a, 0x06, 0x5e, 0x04, 0xa9, 0x03, 0xc7, 0x03, 0x36, 0x02, 0xd1,
 0x02, 0x99, 0x02, 0x91, 0x02, 0xea, 0x03, 0xdb, 0x04, 0xad, 0x05, 0x4f, 0x06, 0x48, 0x07, 0x62, 0x08, 0xf4, 0x0b, 0x71,
 0x07, 0xbd, 0x05, 0xd0, 0x04, 0x2a, 0x03, 0x43, 0x02, 0xa1, 0x02, 0x4d, 0x02, 0x08, 0x01, 0xf8, 0x02, 0x58, 0x03, 0x3f,
 0x03, 0xec, 0x04, 0xa1, 0x05, 0x87, 0x06, 0xa5, 0x08, 0x31, 0x0a, 0xc0, 0x07, 0x5a, 0x05, 0x54, 0x03, 0xa0, 0x02, 0xca,
 0x02, 0x4a, 0x01, 0xd9, 0x01, 0x9c, 0x01, 0x98, 0x01, 0xf0, 0x02, 0xba, 0x03, 0x70, 0x04, 0x02, 0x04, 0xe5, 0x06, 0x0b,
 0x07, 0x84, 0x0a, 0x2f, 0x06, 0xdf, 0x04, 0xe4, 0x03, 0x3f, 0x02, 0x6d, 0x01, 0xe9, 0x01, 0x84, 0x01, 0x47, 0x01, 0x3f,
 0x01, 0x8e, 0x02, 0x31, 0x02, 0xea, 0x03, 0x78, 0x04, 0x5e, 0x05, 0x68, 0x06, 0xea, 0x09, 0x27, 0x06, 0x82, 0x04, 0x81,
 0x02, 0xe8, 0x02, 0x20, 0x01, 0x9c, 0x01, 0x2c, 0x00, 0xfa, 0x00, 0xea, 0x01, 0x37, 0x01, 0xdc, 0x02, 0x76, 0x02, 0xf6,
 0x03, 0xd5, 0x04, 0xd7, 0x06, 0x49, 0x08, 0xb0, 0x06, 0x4b, 0x04, 0x42, 0x02, 0xaf, 0x01, 0xde, 0x01, 0x4f, 0x00, 0xe7,
 0x00, 0xa2, 0x00, 0x92, 0x00, 0xb6, 0x01, 0x5a, 0x01, 0xe4, 0x02, 0x7e, 0x03, 0x3b, 0x04, 0x31, 0x05, 0x88, 0x07, 0xb8,
 0x06, 0x25, 0x04, 0x0d, 0x02, 0x7a, 0x01, 0xa1, 0x01, 0x1b, 0x00, 0xab, 0x00, 0x5e, 0x00, 0x4e, 0x00, 0x68, 0x00, 0xf2,
 0x01, 0x62, 0x01, 0xec, 0x02, 0x9e, 0x03, 0x8f, 0x04, 0xe3, 0x07, 0x01, 0x06, 0x0e, 0x04, 0x0d, 0x02, 0x66, 0x01, 0x94,
 0x01, 0x10, 0x00, 0x99, 0x00, 0x44, 0x00, 0x21, 0x00, 0x34, 0x00, 0x97, 0x01, 0x13, 0x01, 0x94, 0x02, 0x49, 0x03, 0x40,
 0x04, 0x75, 0x06, 0x92, 0x06, 0x54, 0x04, 0x31, 0x02, 0x96, 0x01, 0xc5, 0x01, 0x18, 0x00, 0x99, 0x00, 0x39, 0x00, 0x21,
 0x00, 0x21, 0x00, 0x65, 0x00, 0xd6, 0x01, 0x5f, 0x02, 0x0c, 0x02, 0xfa, 0x04, 0x31, 0x06, 0x5b, 0x06, 0x5c, 0x04, 0x41,
 0x02, 0x9e, 0x01, 0xc5, 0x01, 0x18, 0x00, 0x99, 0x00, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x59, 0x00, 0xc6, 0x01, 0x4b,
 0x01, 0xf9, 0x02, 0xf3, 0x04, 0x1d, 0x06, 0x37, 0x06, 0x0e, 0x04, 0x1a, 0x02, 0x92, 0x01, 0xbd, 0x01, 0x18, 0x00, 0xa1,
 0x00, 0x4a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x59, 0x00, 0xce, 0x01, 0x4b, 0x02, 0x14, 0x02, 0xfa, 0x04, 0x25, 0x06, 0x3f,
 0x05, 0x82, 0x03, 0xf2, 0x02, 0x87, 0x01, 0xa0, 0x01, 0x28, 0x00, 0xaa, 0x00, 0x62, 0x00, 0x31, 0x00, 0x3c, 0x00, 0x89,
 0x00, 0xfe, 0x01, 0x7b, 0x02, 0x3a, 0x03, 0x26, 0x04, 0x69, 0x06, 0x82, 0x05, 0xb3, 0x04, 0x02, 0x02, 0x97, 0x01, 0xd2,
 0x01, 0x43, 0x00, 0xdb, 0x00, 0x99, 0x00, 0x5d, 0x00, 0x75, 0x00, 0xd6, 0x01, 0x4b, 0x01, 0xc7, 0x02, 0x71, 0x03, 0x73,
 0x04, 0xc7, 0x06, 0xe2, 0x05, 0xcf, 0x04, 0x3c, 0x02, 0xe4, 0x02, 0x1f, 0x01, 0x90, 0x01, 0x30, 0x00, 0xe6, 0x00, 0xcb,
 0x00, 0xd6, 0x01, 0x2b, 0x01, 0xb5, 0x02, 0x25, 0x02, 0xf3, 0x03, 0xe4, 0x05, 0x42, 0x07, 0xb2, 0x05, 0xe0, 0x04, 0x83,
 0x03, 0x22, 0x02, 0x67, 0x01, 0xed, 0x01, 0x7d, 0x01, 0x3f, 0x01, 0x14, 0x01, 0x27, 0x01, 0x93, 0x02, 0x10, 0x02, 0x8d,
 0x03, 0x4c, 0x04, 0x3f, 0x05, 0x9c, 0x07, 0xc1, 0x06, 0x1d, 0x04, 0xce, 0x03, 0x7d, 0x02, 0xb5, 0x02, 0x42, 0x01, 0xd1,
 0x01, 0x85, 0x01, 0x74, 0x01, 0x88, 0x01, 0xe8, 0x02, 0x71, 0x02, 0xe1, 0x03, 0xc0, 0x04, 0xb6, 0x06, 0x04, 0x08, 0x7f,
 0x06, 0x40, 0x05, 0x10, 0x03, 0xe0, 0x03, 0x1b, 0x02, 0xab, 0x02, 0x3a, 0x01, 0xe4, 0x01, 0xd4, 0x01, 0xdc, 0x02, 0x50,
 0x02, 0xce, 0x03, 0x60, 0x04, 0x29, 0x05, 0x32, 0x06, 0x88, 0x09, 0x16, 0x06, 0x63, 0x05, 0x8c, 0x04, 0x66, 0x03, 0xbc,
 0x03, 0x1b, 0x02, 0xc2, 0x02, 0x79, 0x02, 0x41, 0x02, 0x69, 0x02, 0xde, 0x03, 0x52, 0x03, 0xdf, 0x04, 0xc9, 0x05, 0xc8,
 0x06, 0xfd, 0x09, 0x46, 0x06, 0x88, 0x05, 0xf8, 0x05, 0x2a, 0x04, 0x42, 0x03, 0xb0, 0x03, 0x4b, 0x02, 0xf3, 0x02, 0xcf,
 0x02, 0xc5, 0x03, 0x46, 0x03, 0xdf, 0x04, 0x6d, 0x05, 0x46, 0x06, 0x54, 0x07, 0x69, 0x09, 0xe9, 0x06, 0x9d, 0x05, 0xe7,
 0x06, 0x75, 0x04, 0x68, 0x04, 0x10, 0x03, 0x5c, 0x03, 0x0f, 0x02, 0xeb, 0x02, 0xf6, 0x03, 0x7d, 0x04, 0x0e, 0x04, 0xa7,
 0x05, 0xc7, 0x06, 0x72, 0x07, 0x84, 0x0a, 0x0d
};
/*
 * the order of the bg0806_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting bg0806_win_sizes[] = {
	/* 1920*1080 */
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SRGGB12_1X12,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = bg0806_init_regs_1920_1080_30fps_dvp,
	}
};

static enum v4l2_mbus_pixelcode bg0806_mbus_code[] = {
	V4L2_MBUS_FMT_SRGGB12_1X12,
};

static struct regval_list bg0806_stream_on_dvp[] = {
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bg0806_stream_off_dvp[] = {
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bg0806_stream_on_mipi[] = {
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list bg0806_stream_off_mipi[] = {
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

int bg0806_read(struct v4l2_subdev *sd, uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
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
	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int bg0806_write(struct v4l2_subdev *sd, uint16_t reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	uint8_t buf[3] = {(reg>>8)&0xff, reg&0xff, value};
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

static int bg0806_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
				msleep(vals->value);
		} else {
			ret = bg0806_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
static int bg0806_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
				msleep(vals->value);
		} else {
			ret = bg0806_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int bg0806_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int bg0806_detect(struct v4l2_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = bg0806_read(sd, 0x0000, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = bg0806_read(sd, 0x0001, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;
	return 0;
}

static int bg0806_set_integration_time(struct v4l2_subdev *sd, int value)
{

	int ret = 0;
	/* low 4 bits are fraction bits */
	unsigned int expo = value;
	ret = bg0806_write(sd, 0x000c, (unsigned char)((expo >> 8) & 0xff));
	if (ret < 0)
		return ret;
	ret = bg0806_write(sd, 0x000d, (unsigned char)(expo & 0xff));
	if (ret < 0)
		return ret;
	ret = bg0806_write(sd, 0x001d, 0x02);
	return 0;
}

static unsigned int bg0806_clip(unsigned int value, unsigned int limit_l, unsigned int limit_h)
{
	if (value < limit_l)
		return limit_l;
	else if (value > limit_h)
		return limit_h;
	else
		return value;
}

static int bg0806_set_analog_gain(struct v4l2_subdev *sd, int value)
{
	unsigned char vrefh;
	unsigned char vrefh_min_tlb = 0x0c;//hxb 20170210  change system voltage 2.8V to 3.3V
	unsigned int dgain = 0,again;
	unsigned int total_gain = value;
	int ret = 0;
	total_gain = bg0806_clip(total_gain, 0x0040, 0x0f00);
	vrefh = (128<<6)/total_gain - 1;
	vrefh = bg0806_clip(vrefh, vrefh_min_tlb, 0x7F);
	again = (128<<6)/(vrefh+1); //recalculate real again
	dgain = total_gain*512/again; // dgain
	dgain = bg0806_clip(dgain, 512, 512); //min=1x,max=8x
//	temp = (128<<6)%(total_gain+1);

	if ((vrefh>vrefh_min_tlb)&&(vrefh<=0x7f)) {
		ret = bg0806_write(sd, 0x002b, 0x30);
		ret += bg0806_write(sd, 0x0030, 0x00);
		ret += bg0806_write(sd, 0x0034, 0x00);
		ret += bg0806_write(sd, 0x004d, 0x00);
		ret += bg0806_write(sd, 0x004f, 0x09);
		ret += bg0806_write(sd, 0x0061, 0x04);
		ret += bg0806_write(sd, 0x0067, 0x01);
		ret += bg0806_write(sd, 0x0068, 0x90);
		if (ret < 0)
			return ret;
	} else if (vrefh==vrefh_min_tlb)	{
		ret = bg0806_write(sd, 0x002b, 0x10);
		ret += bg0806_write(sd, 0x0030, 0x01);
		ret += bg0806_write(sd, 0x0034, 0x01);
		ret += bg0806_write(sd, 0x004d, 0x03);
		ret += bg0806_write(sd, 0x004f, 0x0c);
		ret += bg0806_write(sd, 0x0061, 0x02);
		ret += bg0806_write(sd, 0x0067, 0x00);
		ret += bg0806_write(sd, 0x0068, 0x80);
		if (ret < 0)
			return ret;
	}
	ret += bg0806_write(sd, 0x00B1, vrefh);
	ret += bg0806_write(sd, 0x00BC, 0xFF&(dgain>>8));
	ret += bg0806_write(sd, 0x00BD, 0xFF&(dgain>>0));
	ret += bg0806_write(sd, 0x014a, 0x01);
	if (ret < 0)
		return ret;
	ret = bg0806_write(sd, 0x001D, 0x02);
	if (ret < 0)
		return ret;
	return 0;
}

static int bg0806_set_digital_gain(struct v4l2_subdev *sd, int value)
{
	return 0;
}

static int bg0806_get_black_pedestal(struct v4l2_subdev *sd, int value)
{
	return 0;
}

static int bg0806_init(struct v4l2_subdev *sd, u32 enable)
{
	struct tx_isp_sensor *sensor = (container_of(sd, struct tx_isp_sensor, sd));
	struct tx_isp_notify_argument arg;
	struct tx_isp_sensor_win_setting *wsize = &bg0806_win_sizes[0];
	int i;
	unsigned char pid;
	int ret = 0;
	if (!enable)
		return ISP_SUCCESS;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = bg0806_write_array(sd, wsize->regs);
	if (ret)
		return ret;

//dsc_k blcc_k different with chip pid
#define BG0806A 0x01
#define BG0806C1 0x07
#define BG0806C2 0x0b
#define BG0806B1 0x03
#define BG0806B2 0x0f

	bg0806_read(sd,0x45,&pid);
	pid &= 0x3f;
	switch(pid) {
	case BG0806A:
		bg0806_write(sd,0x0207, 0xc8);
		bg0806_write(sd,0x0133, 0x38);
		break;
	case BG0806B1:
	case BG0806B2:
		bg0806_write(sd,0x0207, 0xde);
		bg0806_write(sd,0x0133, 0x22);
		break;
	case BG0806C1:
	case BG0806C2:
	default:
		bg0806_write(sd,0x0207, 0xaa);
		bg0806_write(sd,0x0133, 0x56);
		break;
	}
	for (i=0; i<768; i++)
	{
		ret = bg0806_write(sd, 0x0400+i, Tab_sensor_dsc[i]);
		usleep(10);
		if (ret < 0)
			return ret;
	}

	arg.value = (int)&sensor->video;
	sd->v4l2_dev->notify(sd, TX_ISP_NOTIFY_SYNC_VIDEO_IN, &arg);
	sensor->priv = wsize;
	return 0;
}

static int bg0806_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = bg0806_write_array(sd, bg0806_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = bg0806_write_array(sd, bg0806_stream_on_mipi);

		} else {
			printk("Don't support this Sensor Data interface\n");
		}
		pr_debug("bg0806 stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = bg0806_write_array(sd, bg0806_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = bg0806_write_array(sd, bg0806_stream_off_mipi);

		} else {
			printk("Don't support this Sensor Data interface\n");
		}
		pr_debug("bg0806 stream off\n");
	}
	return ret;
}

static int bg0806_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int bg0806_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int bg0806_set_fps(struct tx_isp_sensor *sensor, int fps)
{
	struct v4l2_subdev *sd = &sensor->sd;
	struct tx_isp_notify_argument arg;
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int height = 0;
	unsigned int newformat = 0; //the format is 24.8

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		printk("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	sclk = SENSOR_SUPPORT_SCLK;

	val = 0;
	ret += bg0806_read(sd, 0x000e, &val);
	hts = val<<8;
	val = 0;
	ret += bg0806_read(sd, 0x000f, &val);
	hts = val;
	if (0 != ret) {
		printk("err: bg0806 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	val = 0;
	ret += bg0806_read(sd, 0x0008, &val);
	height = val<<8;
	val = 0;
	ret += bg0806_read(sd, 0x0009, &val);
	height = val;
	if (0 != ret) {
		printk("err: bg0806 read err\n");
		return ret;
	}
	ret += bg0806_write(sd, 0x0021, ((vts-height) >> 8) & 0xff);
	ret = bg0806_write(sd, 0x0022, (vts-height) & 0xff);
	ret += bg0806_write(sd, 0x001d, 0x02);
	if (0 != ret) {
		printk("err: bg0806_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	arg.value = (int)&sensor->video;
	sd->v4l2_dev->notify(sd, TX_ISP_NOTIFY_SYNC_VIDEO_IN, &arg);
	return ret;
}

static int bg0806_set_mode(struct tx_isp_sensor *sensor, int value)
{
	struct tx_isp_notify_argument arg;
	struct v4l2_subdev *sd = &sensor->sd;
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;

	if (value == TX_ISP_SENSOR_FULL_RES_MAX_FPS) {
		wsize = &bg0806_win_sizes[0];
	} else if (value == TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS) {
		wsize = &bg0806_win_sizes[0];
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
static int bg0806_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if (reset_gpio != -1) {
		ret = gpio_request(reset_gpio,"bg0806_reset");
		if (!ret) {
			gpio_direction_output(reset_gpio, 1);
			msleep(10);
			gpio_direction_output(reset_gpio, 0);
			msleep(10);
			gpio_direction_output(reset_gpio, 1);
			msleep(30);
		} else {
			printk("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = gpio_request(pwdn_gpio,"bg0806_pwdn");
		if (!ret) {
			gpio_direction_output(pwdn_gpio, 1);
			msleep(150);
			gpio_direction_output(pwdn_gpio, 0);
			msleep(10);
		} else {
			printk("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = bg0806_detect(sd, &ident);
	if (ret) {
		v4l_err(client,
				"chip found @ 0x%x (%s) is not an bg0806 chip.\n",
				client->addr, client->adapter->name);
		return ret;
	}
	v4l_info(client, "bg0806 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);
	return v4l2_chip_ident_i2c_client(client, chip, ident, 0);
}

static int bg0806_s_power(struct v4l2_subdev *sd, int on)
{

	return 0;
}
static long bg0806_ops_private_ioctl(struct tx_isp_sensor *sensor, struct isp_private_ioctl *ctrl)
{
	struct v4l2_subdev *sd = &sensor->sd;
	long ret = 0;
	switch(ctrl->cmd) {
		case TX_ISP_PRIVATE_IOCTL_SENSOR_INT_TIME:
			ret = bg0806_set_integration_time(sd, ctrl->value);
			break;
		case TX_ISP_PRIVATE_IOCTL_SENSOR_AGAIN:
			ret = bg0806_set_analog_gain(sd, ctrl->value);
			break;
		case TX_ISP_PRIVATE_IOCTL_SENSOR_DGAIN:
			ret = bg0806_set_digital_gain(sd, ctrl->value);
			break;
		case TX_ISP_PRIVATE_IOCTL_SENSOR_BLACK_LEVEL:
			ret = bg0806_get_black_pedestal(sd, ctrl->value);
			break;
		case TX_ISP_PRIVATE_IOCTL_SENSOR_RESIZE:
			ret = bg0806_set_mode(sensor,ctrl->value);
			break;
		case TX_ISP_PRIVATE_IOCTL_SUBDEV_PREPARE_CHANGE:
			if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
				ret = bg0806_write_array(sd, bg0806_stream_off_dvp);
			} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
				ret = bg0806_write_array(sd, bg0806_stream_off_mipi);

			} else {
				printk("Don't support this Sensor Data interface\n");
			}
			break;
		case TX_ISP_PRIVATE_IOCTL_SUBDEV_FINISH_CHANGE:
			if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
				ret = bg0806_write_array(sd, bg0806_stream_on_dvp);
			} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
				ret = bg0806_write_array(sd, bg0806_stream_on_mipi);

			} else {
				printk("Don't support this Sensor Data interface\n");
			}
			break;
		case TX_ISP_PRIVATE_IOCTL_SENSOR_FPS:
			ret = bg0806_set_fps(sensor, ctrl->value);
			break;
		default:
			pr_debug("do not support ctrl->cmd ====%d\n",ctrl->cmd);
			break;
	}
	return 0;
}
static long bg0806_ops_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct tx_isp_sensor *sensor =container_of(sd, struct tx_isp_sensor, sd);
	int ret;
	switch(cmd) {
		case VIDIOC_ISP_PRIVATE_IOCTL:
			ret = bg0806_ops_private_ioctl(sensor, arg);
			break;
		default:
			return -1;
			break;
	}
	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int bg0806_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = bg0806_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int bg0806_s_register(struct v4l2_subdev *sd, const struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	bg0806_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static const struct v4l2_subdev_core_ops bg0806_core_ops = {
	.g_chip_ident = bg0806_g_chip_ident,
	.reset = bg0806_reset,
	.init = bg0806_init,
	.s_power = bg0806_s_power,
	.ioctl = bg0806_ops_ioctl,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = bg0806_g_register,
	.s_register = bg0806_s_register,
#endif
};

static const struct v4l2_subdev_video_ops bg0806_video_ops = {
	.s_stream = bg0806_s_stream,
	.s_parm = bg0806_s_parm,
	.g_parm = bg0806_g_parm,
};

static const struct v4l2_subdev_ops bg0806_ops = {
	.core = &bg0806_core_ops,
	.video = &bg0806_video_ops,
};

static int bg0806_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &bg0806_win_sizes[0];
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
	clk_set_rate(sensor->mclk, 24000000);
	clk_enable(sensor->mclk);

	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;

	bg0806_attr.dbus_type = data_interface;
	if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
		wsize->regs = bg0806_init_regs_1920_1080_30fps_dvp;
		memcpy((void*)(&(bg0806_attr.dvp)),(void*)(&bg0806_dvp),sizeof(bg0806_dvp));
	} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
		wsize->regs = bg0806_init_regs_1920_1080_30fps_mipi;
		memcpy((void*)(&(bg0806_attr.mipi)),(void*)(&bg0806_mipi),sizeof(bg0806_mipi));
	} else {
		printk("Don't support this Sensor Data Output Interface.\n");
		goto err_set_sensor_data_interface;
	}

	bg0806_attr.max_again = 390214;
	bg0806_attr.max_dgain = 0;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &bg0806_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	v4l2_i2c_subdev_init(sd, client, &bg0806_ops);
	v4l2_set_subdev_hostdata(sd, sensor);

	pr_debug("probe ok ------->bg0806\n");
	return 0;
err_set_sensor_data_interface:
err_set_sensor_gpio:
	clk_disable(sensor->mclk);
	clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int bg0806_remove(struct i2c_client *client)
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

static const struct i2c_device_id bg0806_id[] = {
	{ "bg0806", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bg0806_id);

static struct i2c_driver bg0806_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "bg0806",
	},
	.probe = bg0806_probe,
	.remove = bg0806_remove,
	.id_table = bg0806_id,
};

static __init int init_sensor(void)
{
	return i2c_add_driver(&bg0806_driver);
}

static __exit void exit_sensor(void)
{
	i2c_del_driver(&bg0806_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for OmniVision bg0806 sensors");
MODULE_LICENSE("GPL");
