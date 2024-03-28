// SPDX-License-Identifier: GPL-2.0+
/*
 * 20161117
 * bg0806.c
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

#define SENSOR_NAME "bg0806"
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x32
#define SENSOR_MAX_WIDTH 1920
#define SENSOR_MAX_HEIGHT 1080
#define SENSOR_CHIP_ID 0x0806
#define SENSOR_CHIP_ID_H (0x08)
#define SENSOR_CHIP_ID_L (0x06)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK (75*1000*1000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20170911a"
#define DRIVE_CAPABILITY_1

#define Vrefh_min_tlb 0x0c

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

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct tx_isp_sensor_attribute sensor_attr;

static int g_vrefh = 0x7f;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	unsigned int gain_one = 0;
	unsigned int gain_one1 = 0;
	uint16_t regs = 0;
	unsigned int isp_gain1 = 0;
	uint32_t mask;
	/* low 4 bits are fraction bits */
	gain_one = private_math_exp2(isp_gain, shift, TX_ISP_GAIN_FIXED_POINT);
	if (gain_one >= (uint32_t) (15.5 * (1 << TX_ISP_GAIN_FIXED_POINT)))
		gain_one = (uint32_t) (15.5 * (1 << TX_ISP_GAIN_FIXED_POINT));
	regs = gain_one >> (TX_ISP_GAIN_FIXED_POINT - 6);
	*sensor_again = regs;
	mask = ~0;
	mask = mask >> (TX_ISP_GAIN_FIXED_POINT - 4);
	mask = mask << (TX_ISP_GAIN_FIXED_POINT - 4);
	gain_one1 = gain_one & mask;
	isp_gain1 = private_log2_fixed_to_fixed(gain_one1, TX_ISP_GAIN_FIXED_POINT, shift);
	return isp_gain1;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi={
	.clk = 800,
	.lans = 1,
};

struct tx_isp_dvp_bus sensor_dvp={
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking = {
		.vblanking = 0,
		.hblanking = 0,
	},
	.polar = {
		.hsync_polar = DVP_POLARITY_DEFAULT,
		.vsync_polar = DVP_POLARITY_LOW,
	},
};

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x0806,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
		.polar = {
			.hsync_polar = DVP_POLARITY_DEFAULT,
			.vsync_polar = DVP_POLARITY_LOW,
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
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	// void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list sensor_init_regs_1920_1080_30fps_mipi[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_30fps_dvp[] = {
	/*
	  @@ DVP interface 1920*1080 30fps
	*/
	{0x0200, 0x0001},
	{0x000e, 0x0008},//0806_4times_74.25M_30fps
	{0x000f, 0x00ae},//row time F_W=2222 //0x08ae
	{0x0013, 0x0001},
	{0x0021, 0x0001},
	{0x0022, 0x000e},//vblank //0025
	{0x0028, 0x0000},
	{0x0029, 0x0030},
	{0x002a, 0x0000},
	{0x002b, 0x0050},//renzhq 20170621
	{0x0030, 0x0000},
	{0x0031, 0x00c0},//rstb2rmp1 gap//renzhq 20170621
	{0x0034, 0x0000},
	{0x0035, 0x00c0},//tx2rmp2 gap//renzhq 20170621
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
	{0x0088, 0x0005},//pclk dly 0x5
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
	{0x008d, 0x0033},//0x3f
	{0x008e, 0x0000},//oen_ctrl,0c
	{0x008f, 0x0000},//io_sel_ctrl,03
	{0x00fa, 0x008F},//ispc,c7
	{0x0391, 0x0001},//mipi_ctrl1,(raw12)
	{0x0392, 0x0000},//mipi_ctrl2,default
	{0x0393, 0x0001},//mipi_ctrl3,default
	{0x0398, 0x0008},//28(25M),14(50M),0a(100M),08(111.375)
	{0x0390, 0x0000},//mipi_ctrl0,bit[1],mipi enable
	{0x001d, 0x0001},
	{SENSOR_REG_END, 0x00},
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
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1920*1080 */
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SRGGB12_1X12,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_dvp,
	}
};

static enum v4l2_mbus_pixelcode sensor_mbus_code[] = {
	V4L2_MBUS_FMT_SRGGB12_1X12,
};

static struct regval_list sensor_stream_on_dvp[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_dvp[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_on_mipi[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value) {
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

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value) {
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

static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
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
		vals++;
	}
	return 0;
}

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
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

static int sensor_reset(struct tx_isp_subdev *sd, int val) {
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident) {
	unsigned char v;
	int ret;
	ret = sensor_read(sd, 0x0000, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;

	*ident = v;
	ret = sensor_read(sd, 0x0001, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;

	*ident = (*ident << 8) | v;
	return 0;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	/* low 4 bits are fraction bits */
	unsigned int expo = value;
	ret = sensor_write(sd, 0x000c, (unsigned char) ((expo >> 8) & 0xff));
	if (ret < 0)
		return ret;
	ret = sensor_write(sd, 0x000d, (unsigned char) (expo & 0xff));
	if (ret < 0)
		return ret;
#if 0
	if (g_vrefh==Vrefh_min_tlb) {  //增益最大
		printk("sensor_set_integration_time:expo = %d\n", expo);
		if (expo > 2400 ) {
			// low fps
			ret = sensor_write(sd, 0x0082, 0x04);
			// ret += sensor_write(sd, 0x007f, 0x01);
			ret += sensor_write(sd, 0x0052, 0x07);
			ret += sensor_write(sd, 0x00f2, 0x07);
			ret += sensor_write(sd, 0x00fb, 0x02);
		} else {
			ret = sensor_write(sd, 0x0082, 0x08);
			// ret += sensor_write(sd, 0x007f, 0x01);
			ret += sensor_write(sd, 0x0052, 0x06);
			ret += sensor_write(sd, 0x00f2, 0x06);
			ret += sensor_write(sd, 0x00fb, 0x01);
		}
	}
	else {
		//normal
		ret = sensor_write(sd, 0x0082, 0x0b);
		// ret += sensor_write(sd, 0x007f, 0x01);
		ret += sensor_write(sd, 0x0052, 0x06);
		ret += sensor_write(sd, 0x00f2, 0x06);
		ret += sensor_write(sd, 0x00fb, 0x01);
	}
#endif

	ret = sensor_write(sd, 0x001d, 0x02);
	return 0;
}

static unsigned int sensor_clip(unsigned int value, unsigned int limit_l, unsigned int limit_h) {
	if (value < limit_l)
		return limit_l;
	else if (value > limit_h)
		return limit_h;
	else
		return value;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value) {
	unsigned char vrefh;
	// unsigned char Vrefh_min_tlb = 0x0c;//hxb 20170210  change system voltage 2.8V to 3.3V
	unsigned int dgain = 0, again;
	unsigned int total_gain = value;
	int ret = 0;
	total_gain = sensor_clip(total_gain, 0x0040, 0x0f00);
	vrefh = (128 << 6) / total_gain - 1;
	vrefh = sensor_clip(vrefh, Vrefh_min_tlb, 0x7F);
	again = (128 << 6) / (vrefh + 1); //recalculate real again
	dgain = total_gain * 512 / again; // dgain
	dgain = sensor_clip(dgain, 512, 512); //min=1x,max=8x
//	temp = (128<<6)%(total_gain+1);

	if ((vrefh > Vrefh_min_tlb) && (vrefh <= 0x7f)) {
		ret = sensor_write(sd, 0x0030, 0x00);
		ret += sensor_write(sd, 0x0031, 0xc0);
		ret += sensor_write(sd, 0x0034, 0x00);
		ret += sensor_write(sd, 0x0035, 0xc0);
		ret += sensor_write(sd, 0x004d, 0x00);
		ret += sensor_write(sd, 0x004f, 0x09);
		ret += sensor_write(sd, 0x0061, 0x04);
		ret += sensor_write(sd, 0x0067, 0x01);
		ret += sensor_write(sd, 0x0068, 0x90);
		if (ret < 0)
			return ret;
	} else if (vrefh == Vrefh_min_tlb)	{
		ret = sensor_write(sd, 0x0030, 0x01);
		ret += sensor_write(sd, 0x0031, 0xb0);
		ret += sensor_write(sd, 0x0034, 0x01);
		ret += sensor_write(sd, 0x0035, 0xb0);
		ret += sensor_write(sd, 0x004d, 0x03);
		ret += sensor_write(sd, 0x004f, 0x0c);
		ret += sensor_write(sd, 0x0061, 0x02);
		ret += sensor_write(sd, 0x0067, 0x00);
		ret += sensor_write(sd, 0x0068, 0x80);
		if (ret < 0)
			return ret;
	}

	g_vrefh = vrefh;

	ret += sensor_write(sd, 0x00B1, vrefh);
	ret += sensor_write(sd, 0x00BC, 0xFF & (dgain >> 8));
	ret += sensor_write(sd, 0x00BD, 0xFF & (dgain >> 0));
	ret += sensor_write(sd, 0x014a, 0x01);
	if (ret < 0)
		return ret;

	ret = sensor_write(sd, 0x001D, 0x02);
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, int enable) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
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
	ret = sensor_write_array(sd, wsize->regs);
	if (ret)
		return ret;

//dsc_k blcc_k different with chip pid
#define BG0806A 0x01
#define BG0806C1 0x07
#define BG0806C2 0x0b
#define BG0806B1 0x03
#define BG0806B2 0x0f

	sensor_read(sd, 0x45, &pid);
	pid &= 0x3f;
	switch (pid) {
		case BG0806A:
			sensor_write(sd, 0x0207, 0xc8);
			sensor_write(sd, 0x0133, 0x38);
			break;
		case BG0806B1:
		case BG0806B2:
			sensor_write(sd, 0x0207, 0xde);
			sensor_write(sd, 0x0133, 0x22);
			break;
		case BG0806C1:
		case BG0806C2:
		default:
			sensor_write(sd, 0x0207, 0xaa);
			sensor_write(sd, 0x0133, 0x56);
			break;
	}
	for (i = 0; i < 768; i++) {
		ret = sensor_write(sd, 0x0400 + i, Tab_sensor_dsc[i]);
		msleep(10);
		if (ret < 0)
			return ret;
	}

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;
	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, int enable) {
	int ret = 0;
	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
		} else {
			printk("Don't support this Sensor Data interface\n");
		}
		pr_debug("%s stream on\n", SENSOR_NAME);
	} else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
			ret = sensor_write_array(sd, sensor_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);
		} else {
			printk("Don't support this Sensor Data interface\n");
		}
		pr_debug("%s stream off\n", SENSOR_NAME);
	}
	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	int ret = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int height = 0;
	unsigned int newformat = 0; //the format is 24.8
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		printk("warn: fps(%d) not in range\n", fps);
		return -1;
	}

	sclk = SENSOR_SUPPORT_SCLK;
	val = 0;
	ret += sensor_read(sd, 0x000e, &val);
	hts = val << 8;
	val = 0;
	ret += sensor_read(sd, 0x000f, &val);
	hts |= val;
	if (0 != ret) {
		printk("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	val = 0;
	ret += sensor_read(sd, 0x0008, &val);
	height = val << 8;
	val = 0;
	ret += sensor_read(sd, 0x0009, &val);
	height |= val;
	if (0 != ret) {
		printk("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}

	ret += sensor_write(sd, 0x0021, ((vts - height) >> 8) & 0xff);
	ret = sensor_write(sd, 0x0022, (vts - height) & 0xff);
	ret += sensor_write(sd, 0x001d, 0x02);
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

static int sensor_set_mode(struct tx_isp_subdev *sd, int value) {
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

static int sensor_g_chip_ident(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio, "sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(30);
		} else {
			printk("gpio requrest fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(150);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		} else {
			printk("gpio requrest fail %d\n", pwdn_gpio);
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

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg) {
	long ret = 0;
	if (IS_ERR_OR_NULL(sd)) {
		printk("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd) {
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			if (arg)
				ret = sensor_set_integration_time(sd, *(int *) arg);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			if (arg)
				ret = sensor_set_analog_gain(sd, *(int *) arg);
			break;
		case TX_ISP_EVENT_SENSOR_DGAIN:
			if (arg)
				ret = sensor_set_digital_gain(sd, *(int *) arg);
			break;
		case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
			if (arg)
				ret = sensor_get_black_pedestal(sd, *(int *) arg);
			break;
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if (arg)
				ret = sensor_set_mode(sd, *(int *) arg);
			break;
		case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
			if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
				ret = sensor_write_array(sd, sensor_stream_off_dvp);
			} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
				ret = sensor_write_array(sd, sensor_stream_off_mipi);

			} else {
				printk("Don't support this Sensor Data interface\n");
			}
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
				ret = sensor_write_array(sd, sensor_stream_on_dvp);
			} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
				ret = sensor_write_array(sd, sensor_stream_on_mipi);

			} else {
				printk("Don't support this Sensor Data interface\n");
			}
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if (arg)
				ret = sensor_set_fps(sd, *(int *) arg);
			break;
		default:
			break;
	}
	return 0;
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
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
	int ret;
	sensor = (struct tx_isp_sensor *) kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		printk("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}

	memset(sensor, 0, sizeof(*sensor));
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

	sensor_attr.dvp.gpio = sensor_gpio_func;
	sensor_attr.dbus_type = data_interface;
	if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP) {
		wsize->regs = sensor_init_regs_1920_1080_30fps_dvp;
		memcpy((void *) (&(sensor_attr.dvp)), (void *) (&sensor_dvp), sizeof(sensor_dvp));
	} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
		wsize->regs = sensor_init_regs_1920_1080_30fps_mipi;
		memcpy((void *) (&(sensor_attr.mipi)), (void *) (&sensor_mipi), sizeof(sensor_mipi));
	} else {
		printk("Don't support this Sensor Data Output Interface.\n");
		goto err_set_sensor_data_interface;
	}
	sensor_attr.max_again = 390214;
	sensor_attr.max_dgain = 0;
	sd = &sensor->sd;
	video = &sensor->video;
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

err_set_sensor_data_interface:
err_set_sensor_gpio:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);
	return -1;
}

static int sensor_remove(struct i2c_client *client) {
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

static __init int init_sensor(void) {
	int ret = 0;
	sensor_common_init(&sensor_info);
	ret = private_driver_get_interface();
	if (ret) {
		printk("Failed to init %s driver.\n", SENSOR_NAME);
		return -1;
	}

	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	private_i2c_del_driver(&sensor_driver);
	sensor_common_exit();
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
