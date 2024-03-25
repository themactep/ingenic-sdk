// SPDX-License-Identifier: GPL-2.0+
/*
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

#define SENSOR_NAME "gc4c33"
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x29
#define SENSOR_MAX_WIDTH 2560
#define SENSOR_MAX_HEIGHT 1440
#define SENSOR_CHIP_ID 0x4c33
#define SENSOR_CHIP_ID_H (0x46)
#define SENSOR_CHIP_ID_L (0xc3)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0x0000
#define SENSOR_SUPPORT_30FPS_SCLK (45900000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20210112a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

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
	unsigned int index;
	unsigned char reg2fd;
	unsigned char reg2fc;
	unsigned char reg263;
	unsigned char reg267;
	unsigned char reg2b3;
	unsigned char reg2b4;
	unsigned char reg2b8;
	unsigned char reg2b9;
	unsigned char reg515;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x0, 0x00, 0x3b, 0x00, 0x3b, 0x00, 0x00, 0x01, 0x00, 0x20,0},
	{0x01, 0x00, 0x3b, 0x00, 0x3b, 0x08, 0x00, 0x01, 0x0B, 0x20,14995},
	{0x02, 0x00, 0x3b, 0x00, 0x3b, 0x01, 0x00, 0x01, 0x1B, 0x1e,33278},
	{0x03, 0x00, 0x3b, 0x00, 0x3b, 0x09, 0x00, 0x01, 0x2A, 0x1c,47704},
	{0x04, 0x00, 0x3b, 0x00, 0x3b, 0x10, 0x00, 0x01, 0x3E, 0x1a,64046},
	{0x05, 0x00, 0x3b, 0x00, 0x3b, 0x18, 0x00, 0x02, 0x13, 0x18,78620},
	{0x06, 0x00, 0x3b, 0x00, 0x3b, 0x11, 0x00, 0x02, 0x33, 0x18,97241},
	{0x07, 0x00, 0x3b, 0x00, 0x3b, 0x19, 0x00, 0x03, 0x11, 0x16,111891},
	{0x08, 0x00, 0x3b, 0x00, 0x3b, 0x30, 0x00, 0x03, 0x3B, 0x16,129205},
	{0x09, 0x00, 0x3b, 0x00, 0x3b, 0x38, 0x00, 0x04, 0x26, 0x14,144155},
	{0x0a, 0x00, 0x3b, 0x00, 0x3b, 0x31, 0x00, 0x05, 0x24, 0x14,162247},
	{0x0b, 0x00, 0x3b, 0x00, 0x3b, 0x39, 0x00, 0x06, 0x21, 0x12,177200},
	{0x0c, 0x00, 0x3b, 0x00, 0x3b, 0x32, 0x00, 0x07, 0x28, 0x12,192065},
	{0x0d, 0x00, 0x3b, 0x00, 0x3b, 0x3a, 0x00, 0x08, 0x3C, 0x12,207082},
	{0x0e, 0x00, 0x3b, 0x00, 0x3b, 0x33, 0x00, 0x0A, 0x3F, 0x10,226579},
	{0x0f, 0x00, 0x3b, 0x00, 0x3b, 0x3b, 0x00, 0x0C, 0x38, 0x10,241594},
	{0x10,0x00, 0x3b, 0x00, 0x3b, 0x34, 0x00, 0x0F, 0x17, 0x0e, 258276},
	{0x11,0x00, 0x3b, 0x00, 0x3b, 0x3c, 0x00, 0x11, 0x3F, 0x0c, 273193},
	{0x12,0x00, 0x3b, 0x00, 0x3b, 0xb4, 0x00, 0x15, 0x34, 0x0a, 291439},
	{0x13,0x00, 0x3b, 0x00, 0x3b, 0xbc, 0x00, 0x19, 0x22, 0x08, 306323},
	{0x14,0x00, 0x3b, 0x00, 0x3b, 0x34, 0x01, 0x1E, 0x09, 0x06, 322015},
	{0x15,0x00, 0x3b, 0x00, 0x3b, 0x3c, 0x01, 0x1A, 0x31, 0x04, 336946},
	{0x16,0x00, 0x3b, 0x00, 0x3b, 0xb4, 0x01, 0x20, 0x12, 0x02, 355058},
	{0x17,0x00, 0x3b, 0x00, 0x3b, 0xbc, 0x01, 0x25, 0x28, 0x02, 369988},
	{0x18,0x00, 0x3b, 0x00, 0x3b, 0x34, 0x02, 0x2D, 0x28, 0x02, 375657},
	{0x19,0x00, 0x3b, 0x00, 0x3b, 0x3c, 0x02, 0x35, 0x0A, 0x10, 387206},
	{0x1a,0x00, 0x3b, 0x00, 0x3b, 0xf4, 0x01, 0x3F, 0x22, 0x0e, 393210},
	{0x1b,0x00, 0x3b, 0x00, 0x3b, 0xfc, 0x01, 0x4A, 0x02, 0x0c, 406976},
	{0x1c,0x00, 0xbb, 0x00, 0x3b, 0x74, 0x02, 0x5A, 0x36, 0x0a, 426325},
	{0x1d,0x00, 0xbc, 0x00, 0xbc, 0x34, 0x12, 0x69, 0x37, 0x0a, 440788},
	{0x1e,0x01, 0x34, 0x10, 0x34, 0x34, 0x12, 0x7E, 0x13, 0x08, 457478},
};

struct tx_isp_sensor_attribute sensor_attr;

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

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x4c33,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 630,
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
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 457478,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 1800 - 4,
	.integration_time_limit = 1800 - 4,
	.total_width = 1020,
	.total_height = 1800,
	.max_integration_time = 1800 - 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	// void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list sensor_init_regs_1920_1080_25fps_mipi[] = {
		/*SYSTEM*/
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x03fe, 0x00},
	{0x031c, 0x01},
	{0x0317, 0x24},
	{0x0320, 0x77},
	{0x0106, 0x78},
	{0x0324, 0x84},
	{0x0327, 0x05},
	{0x0325, 0x08},
	{0x0326, 0x33},
	{0x031a, 0x00},
	{0x0314, 0x30},
	{0x0315, 0x23},
	{0x0334, 0x00},
	{0x0337, 0x02},
	{0x0335, 0x02},
	{0x0336, 0x23},
	{0x0324, 0xc4},
	{0x0334, 0x40},
	{0x031c, 0x03},
	{0x031c, 0xd2},
	{0x0180, 0x26},
	{0x031c, 0xd6},
	{0x0287, 0x18},
	{0x02ee, 0x70},
	{0x0202, 0x05},
	{0x0203, 0xd0},
	{0x0213, 0x1c},
	{0x0214, 0x04},
	{0x0290, 0x00},
	{0x029d, 0x08},
	{0x0340, 0x07},
	{0x0341, 0x08}, //frmae length
	{0x0342, 0x01},
	{0x0343, 0xfe}, //hb
	{0x00f2, 0x04},
	{0x00f1, 0x0a},
	{0x00f0, 0xa0},
	{0x00c1, 0x05},
	{0x00c2, 0xa0},
	{0x00c3, 0x0a},
	{0x00c4, 0x00},

	{0x00da, 0x05},
	{0x00db, 0xa0},//1440
	{0x00d8, 0x0a},
	{0x00d9, 0x00},//2560

	{0x00c5, 0x0a},
	{0x00c6, 0xa0},
	{0x00bf, 0x17},
	{0x00ce, 0x0a},
	{0x00cd, 0x01},
	{0x00cf, 0x89},
	{0x023c, 0x06},
	{0x02d1, 0xc2},
	{0x027d, 0xcc},
	{0x0238, 0xa4},
	{0x02ce, 0x1f},
	{0x02f9, 0x00},
	{0x0227, 0x74},
	{0x0232, 0xc8},
	{0x0245, 0xa8},
	{0x027d, 0xcc},
	{0x02fa, 0xb0},
	{0x02e7, 0x23},
	{0x02e8, 0x50},
	{0x021d, 0x03},
	{0x0220, 0x43},
	{0x0228, 0x10},
	{0x022c, 0x2c},
	{0x024b, 0x11},
	{0x024e, 0x11},
	{0x024d, 0x11},
	{0x0255, 0x11},
	{0x025b, 0x11},
	{0x0262, 0x01},
	{0x02d4, 0x10},
	{0x0540, 0x10},
	{0x0239, 0x00},
	{0x0231, 0xc4},
	{0x024f, 0x11},
	{0x028c, 0x1a},
	{0x02d3, 0x01},
	{0x02da, 0x35},
	{0x02db, 0xd0},
	{0x02e6, 0x30},
	{0x0512, 0x00},
	{0x0513, 0x00},
	{0x0515, 0x20},
	{0x0518, 0x00},
	{0x0519, 0x00},
	{0x051d, 0x50},
	{0x0211, 0x00},
	{0x0216, 0x00},
	{0x0221, 0x50},
	{0x0223, 0xcc},
	{0x0225, 0x07},
	{0x0229, 0x36},
	{0x022b, 0x0c},
	{0x022e, 0x0c},
	{0x0230, 0x03},
	{0x023a, 0x38},
	{0x027b, 0x3c},
	{0x027c, 0x0c},
	{0x0298, 0x13},
	{0x02a4, 0x07},
	{0x02ab, 0x00},
	{0x02ac, 0x00},
	{0x02ad, 0x07},
	{0x02af, 0x01},
	{0x02cd, 0x3c},
	{0x02d2, 0xe8},
	{0x02e4, 0x00},
	{0x0530, 0x04},
	{0x0531, 0x04},
	{0x0243, 0x36},
	{0x0219, 0x07},
	{0x02e5, 0x28},
	{0x0338, 0xaa},
	{0x0339, 0xaa},
	{0x033a, 0x02},
	{0x023b, 0x20},
	{0x0212, 0x48},
	{0x0523, 0x02},
	{0x0347, 0x06},

	{0x0348, 0x0a},
	{0x0349, 0x10},
	{0x034a, 0x05},
	{0x034b, 0xb0},

	{0x0097, 0x0a},
	{0x0098, 0x10},
	{0x0099, 0x05},
	{0x009a, 0xb0},

	{0x034c, 0x0a},
	{0x034d, 0x00}, //2560
	{0x034e, 0x05},
	{0x034f, 0xa0}, //1440

	{0x0354, 0x00},
	{0x0352, 0x00},

	{0x0295, 0xff},
	{0x0296, 0xff},
	{0x02f0, 0x22},
	{0x02f1, 0x22},
	{0x02f2, 0xff},
	{0x02f4, 0x32},
	{0x02f5, 0x20},
	{0x02f6, 0x1c},
	{0x02f7, 0x1f},
	{0x02f8, 0x00},
	{0x0291, 0x04},
	{0x0292, 0x22},
	{0x0297, 0x22},
	{0x02d5, 0xfe},
	{0x02d6, 0xd0},
	{0x02d7, 0x35},
	{0x0268, 0x3b},
	{0x0269, 0x3b},
	{0x0272, 0x80},
	{0x0273, 0x80},
	{0x0274, 0x80},
	{0x0275, 0x80},
	{0x0276, 0x80},
	{0x0277, 0x80},
	{0x0278, 0x80},
	{0x0279, 0x80},
	{0x0555, 0x50},
	{0x0556, 0x23},
	{0x0557, 0x50},
	{0x0558, 0x23},
	{0x0559, 0x50},
	{0x055a, 0x23},
	{0x055b, 0x50},
	{0x055c, 0x23},
	{0x055d, 0x50},
	{0x055e, 0x23},
	{0x0550, 0x28},
	{0x0551, 0x28},
	{0x0552, 0x28},
	{0x0553, 0x28},
	{0x0554, 0x28},
	{0x0220, 0x43},
	{0x021f, 0x03},
	{0x0233, 0x01},
	{0x0234, 0x80},
	{0x02be, 0x81},
	{0x00a0, 0x5d},
	{0x00c7, 0x94},
	{0x00c8, 0x15},
	{0x00df, 0x0a},
	{0x00de, 0xfe},
	{0x00aa, 0x3a},
	{0x00c0, 0x0a},
	{0x031c, 0x80},
	{0x031f, 0x10},
	{0x031f, 0x00},
	{0x031c, 0xd2},
	{0x031c, 0xd2},
	{0x031c, 0xd2},
	{0x031c, 0xd2},
	{0x031c, 0x80},
	{0x031f, 0x10},
	{0x031f, 0x00},
	{0x031c, 0xd6},
	{0x0053, 0x00},
	{0x008e, 0x55},
	{0x0205, 0xc0},
	{0x02b0, 0xe0},
	{0x02b1, 0xe0},
	{0x02b3, 0x00},
	{0x02b4, 0x00},
	{0x02fc, 0x00},
	{0x02fd, 0x00},
	{0x0263, 0x00},
	{0x0267, 0x00},

	{0x0451, 0xff},//rgain 7ff=2047
	{0x0455, 0x07},
	{0x0452, 0xf2},//bgain 6f =1778
	{0x0456, 0x06},
	{0x0450, 0xbd},//g 3bd=957
	{0x0454, 0x03},
	{0x0453, 0xbd},
	{0x0457, 0x03},

	{0x0226, 0x30},
	{0x0042, 0x20},
	{0x0458, 0x01},
	{0x0459, 0x01},
	{0x045a, 0x01},
	{0x045b, 0x01},
	{0x044c, 0x80},
	{0x044d, 0x80},
	{0x044e, 0x80},
	{0x044f, 0x80},
	{0x0060, 0x40},
	{0x00e1, 0x81},
	{0x00e2, 0x1c},
	{0x00e4, 0x01},
	{0x00e5, 0x01},
	{0x00e6, 0x01},
	{0x00e7, 0x00},
	{0x00e8, 0x00},
	{0x00e9, 0x00},
	{0x00ea, 0xf0},
	{0x00ef, 0x04},
	{0x00a1, 0x05},//0x10 dpc
	{0x00a2, 0x05},//0x10
	{0x00a7, 0x00},//0x22
	{0x00a8, 0x20},//0x20
	{0x00a9, 0x20},
	{0x00b3, 0x00},
	{0x00b4, 0x10},
	{0x00b5, 0x20},
	{0x00b6, 0x30},
	{0x00b7, 0x40},
	{0x00d1, 0x06},//re_masic
	{0x00d2, 0x04},
	{0x00d4, 0x02},
	{0x00d5, 0x04},
	{0x0089, 0x03},//
	{0x008c, 0x10},
	{0x0080, 0x06},
	{0x0180, 0x66}, //mipi
	{0x0181, 0x30},
	{0x0182, 0x55},
	{0x0185, 0x01},
	{0x0114, 0x01},
	{0x0115, 0x12},
	{0x0103, 0x00},
	{0x0104, 0x20},
	{0x0100, 0x09},

	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1920*1080 */
	{
		.width = 2560,
		.height = 1440,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SGRBG10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_25fps_mipi,
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

/*static int sensor_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff) * 2;
	struct again_lut *val_lut = sensor_again_lut;
	ret = sensor_write(sd, 0x0203, it & 0xfe);
	ret += sensor_write(sd, 0x0202, it >> 8);
	ret = sensor_write(sd, 0x031d, 0x2a);
	ret = sensor_write(sd, 0x02fd, val_lut[value].reg2fd);
	ret = sensor_write(sd, 0x02fc, val_lut[value].reg2fc);
	ret = sensor_write(sd, 0x0263, val_lut[value].reg263);
	ret = sensor_write(sd, 0x0267, val_lut[value].reg267);
	ret = sensor_write(sd, 0x031d, 0x28);
	ret = sensor_write(sd, 0x02b3, val_lut[value].reg2b3);
	ret = sensor_write(sd, 0x02b4, val_lut[value].reg2b4);
	ret = sensor_write(sd, 0x02b8, val_lut[value].reg2b8);
	ret = sensor_write(sd, 0x02b9, val_lut[value].reg2b9);
	ret = sensor_write(sd, 0x0515, val_lut[value].reg515);

	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d" ,__LINE__ );
		return ret;
	}

	return 0;
} */
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = sensor_write(sd, 0x0203, value & 0xfe);
	ret += sensor_write(sd, 0x0202, value >> 8);
	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	struct again_lut *val_lut = sensor_again_lut;
	ret = sensor_write(sd, 0x031d, 0x2a);
	ret = sensor_write(sd, 0x02fd, val_lut[value].reg2fd);
	ret = sensor_write(sd, 0x02fc, val_lut[value].reg2fc);
	ret = sensor_write(sd, 0x0263, val_lut[value].reg263);
	ret = sensor_write(sd, 0x0267, val_lut[value].reg267);
	ret = sensor_write(sd, 0x031d, 0x28);
	ret = sensor_write(sd, 0x02b3, val_lut[value].reg2b3);
	ret = sensor_write(sd, 0x02b4, val_lut[value].reg2b4);
	ret = sensor_write(sd, 0x02b8, val_lut[value].reg2b8);
	ret = sensor_write(sd, 0x02b9, val_lut[value].reg2b9);
	ret = sensor_write(sd, 0x0515, val_lut[value].reg515);

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
	unsigned int max_fps = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	wpclk = SENSOR_SUPPORT_30FPS_SCLK;
	max_fps = SENSOR_OUTPUT_MAX_FPS;

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
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

	vts = wpclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = sensor_write(sd, 0x0340, (unsigned char)((vts & 0x3f00) >> 8));
	ret += sensor_write(sd, 0x0341, (unsigned char)(vts & 0xff));
	if (ret < 0)
		return -1;
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;
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
	long ret = 0;
	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
/*	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
	     		ret = sensor_set_expo(sd, *(int*)arg);
		break;*/
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
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	/*
	  convert sensor-gain into isp-gain,
	*/

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sensor_attr;
/*	sensor_attr.expo_fs=1;  */
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
