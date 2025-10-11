/*
 * sc2336ps1.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @fsync Sync hardware connection: Primary Sensor fsync is directly connected to secondary Sensor efsync.
 */
#define DEBUG

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
#include <txx-funcs.h>

#define SC2336PS1_CHIP_ID_H	(0x9b)
#define SC2336PS1_CHIP_ID_L	(0x3a)
#define SC2336PS1_REG_END		0xffff
#define SC2336PS1_REG_DELAY	0xfffe
#define SC2336PS1_SUPPORT_30FPS_SCLK (81000000)
// ============================================================================
// SENSOR IDENTIFICATION
// ============================================================================
#define SENSOR_VERSION "H20230926a"

// ============================================================================
// TIMING AND PERFORMANCE
// ============================================================================
#define SENSOR_OUTPUT_MAX_FPS 15
#define SENSOR_OUTPUT_MIN_FPS 5

static int reset_gpio = -1;
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static int fsync_mode = 3;
module_param(fsync_mode, int, S_IRUGO);
MODULE_PARM_DESC(fsync_mode, "Sensor Indicates the frame synchronization mode");

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

struct again_lut sc2336ps1_again_lut[] = {
	//cnt_gain = 161 cnt_reg = 161
	{0x80, 0},
	{0x84, 2886},
	{0x88, 5776},
	{0x8c, 8494},
	{0x90, 11136},
	{0x94, 13706},
	{0x98, 16287},
	{0x9c, 18723},
	{0xa0, 21097},
	{0xa4, 23414},
	{0xa8, 25746},
	{0xac, 27953},
	{0xb0, 30109},
	{0xb4, 32217},
	{0xb8, 34345},
	{0xbc, 36361},
	{0xc0, 38336},
	{0xc4, 40270},
	{0xc8, 42226},
	{0xcc, 44082},
	{0xd0, 45904},
	{0xd4, 47690},
	{0xd8, 49500},
	{0xdc, 51220},
	{0xe0, 52910},
	{0xe4, 54571},
	{0xe8, 56254},
	{0xec, 57857},
	{0xf0, 59433},
	{0xf4, 60984},
	{0xf8, 62558},
	{0xfc, 64059},
	{0x880, 65536},
	{0x884, 68422},
	{0x888, 71312},
	{0x88c, 74030},
	{0x890, 76672},
	{0x894, 79242},
	{0x898, 81823},
	{0x89c, 84259},
	{0x8a0, 86633},
	{0x8a4, 88950},
	{0x8a8, 91282},
	{0x8ac, 93489},
	{0x8b0, 95645},
	{0x8b4, 97753},
	{0x8b8, 99881},
	{0x8bc, 101897},
	{0x8c0, 103872},
	{0x8c4, 105806},
	{0x8c8, 107762},
	{0x8cc, 109618},
	{0x8d0, 111440},
	{0x8d4, 113226},
	{0x8d8, 115036},
	{0x8dc, 116756},
	{0x8e0, 118446},
	{0x8e4, 120107},
	{0x8e8, 121790},
	{0x8ec, 123393},
	{0x8f0, 124969},
	{0x8f4, 126520},
	{0x8f8, 128094},
	{0x8fc, 129595},
	{0x980, 131072},
	{0x984, 133958},
	{0x988, 136848},
	{0x98c, 139566},
	{0x990, 142208},
	{0x994, 144778},
	{0x998, 147359},
	{0x99c, 149795},
	{0x9a0, 152169},
	{0x9a4, 154486},
	{0x9a8, 156818},
	{0x9ac, 159025},
	{0x9b0, 161181},
	{0x9b4, 163289},
	{0x9b8, 165417},
	{0x9bc, 167433},
	{0x9c0, 169408},
	{0x9c4, 171342},
	{0x9c8, 173298},
	{0x9cc, 175154},
	{0x9d0, 176976},
	{0x9d4, 178762},
	{0x9d8, 180572},
	{0x9dc, 182292},
	{0x9e0, 183982},
	{0x9e4, 185643},
	{0x9e8, 187326},
	{0x9ec, 188929},
	{0x9f0, 190505},
	{0x9f4, 192056},
	{0x9f8, 193630},
	{0x9fc, 195131},
	{0xb80, 196608},
	{0xb84, 199494},
	{0xb88, 202384},
	{0xb8c, 205102},
	{0xb90, 207744},
	{0xb94, 210314},
	{0xb98, 212895},
	{0xb9c, 215331},
	{0xba0, 217705},
	{0xba4, 220022},
	{0xba8, 222354},
	{0xbac, 224561},
	{0xbb0, 226717},
	{0xbb4, 228825},
	{0xbb8, 230953},
	{0xbbc, 232969},
	{0xbc0, 234944},
	{0xbc4, 236878},
	{0xbc8, 238834},
	{0xbcc, 240690},
	{0xbd0, 242512},
	{0xbd4, 244298},
	{0xbd8, 246108},
	{0xbdc, 247828},
	{0xbe0, 249518},
	{0xbe4, 251179},
	{0xbe8, 252862},
	{0xbec, 254465},
	{0xbf0, 256041},
	{0xbf4, 257592},
	{0xbf8, 259166},
	{0xbfc, 260667},
	{0xf80, 262144},
	{0xf84, 265030},
	{0xf88, 267920},
	{0xf8c, 270638},
	{0xf90, 273280},
	{0xf94, 275850},
	{0xf98, 278431},
	{0xf9c, 280867},
	{0xfa0, 283241},
	{0xfa4, 285558},
	{0xfa8, 287890},
	{0xfac, 290097},
	{0xfb0, 292253},
	{0xfb4, 294361},
	{0xfb8, 296489},
	{0xfbc, 298505},
	{0xfc0, 300480},
	{0xfc4, 302414},
	{0xfc8, 304370},
	{0xfcc, 306226},
	{0xfd0, 308048},
	{0xfd4, 309834},
	{0xfd8, 311644},
	{0xfdc, 313364},
	{0xfe0, 315054},
	{0xfe4, 316715},
	{0xfe8, 318398},
	{0xfec, 320001},
	{0xff0, 321577},
	{0xff4, 323128},
	{0xff8, 324702},
	{0xffc, 326203},
	{0x1f80, 327680},
};

struct tx_isp_sensor_attribute sc2336ps1_attr;

unsigned int sc2336ps1_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc2336ps1_again_lut;
	while(lut->gain <= sc2336ps1_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == sc2336ps1_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc2336ps1_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sc2336ps1_attr={
	.name = "sc2336ps1",
	.chip_id = 0x9b3a,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x32,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 396,
		.lans = 2,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 1920,
		.image_theight = 1080,
		.mipi_sc.mipi_crop_start0x = 0,
		.mipi_sc.mipi_crop_start0y = 0,
		.mipi_sc.mipi_crop_start1x = 0,
		.mipi_sc.mipi_crop_start1y = 0,
		.mipi_sc.mipi_crop_start2x = 0,
		.mipi_sc.mipi_crop_start2y = 0,
		.mipi_sc.mipi_crop_start3x = 0,
		.mipi_sc.mipi_crop_start3y = 0,
		.mipi_sc.data_type_en = 0,
		.mipi_sc.data_type_value = RAW10,
		.mipi_sc.del_start = 0,
		.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
		.mipi_sc.sensor_fid_mode = 0,
		.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 327680,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 0x95d - 6,
	.integration_time_limit = 0x95d - 6,
	.total_width = 0x898,
	.total_height = 0x95d,
	.max_integration_time = 0x95d - 6,
	.one_line_expr_in_us = 13,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc2336ps1_alloc_again,
	.sensor_ctrl.alloc_dgain = sc2336ps1_alloc_dgain,
        .fsync_attr = {
                .mode = TX_ISP_SENSOR_FSYNC_MODE_MS_REALTIME_MISPLACE,
                .call_times = 1,
                .sdelay = 1000,
        }
};

static struct regval_list sc2336ps1_init_regs_1920_1080_15fps_mipi[] = {
        {0x0103, 0x01},
        {0x0100, 0x00},
        {0x36e9, 0x80},
        {0x37f9, 0x80},
        {0x300a, 0x24},
        {0x301f, 0x1b},
        {0x3106, 0x05},
        {0x320e, 0x09},
        {0x320f, 0x60},
        {0x3222, 0x02},
        {0x3224, 0x82},
        {0x322e, 0x09},
        {0x322f, 0x5c},
        {0x3248, 0x04},
        {0x3249, 0x0b},
        {0x3253, 0x08},
        {0x3301, 0x09},
        {0x3302, 0xff},
        {0x3303, 0x10},
        {0x3306, 0x80},
        {0x3307, 0x02},
        {0x3309, 0xc8},
        {0x330a, 0x01},
        {0x330b, 0x30},
        {0x330c, 0x16},
        {0x330d, 0xff},
        {0x3318, 0x02},
        {0x331f, 0xb9},
        {0x3321, 0x0a},
        {0x3327, 0x0e},
        {0x332b, 0x12},
        {0x3333, 0x10},
        {0x3334, 0x40},
        {0x335e, 0x06},
        {0x335f, 0x0a},
        {0x3364, 0x1f},
        {0x337c, 0x02},
        {0x337d, 0x0e},
        {0x3390, 0x09},
        {0x3391, 0x0f},
        {0x3392, 0x1f},
        {0x3393, 0x20},
        {0x3394, 0x20},
        {0x3395, 0xe0},
        {0x33a2, 0x04},
        {0x33b1, 0x80},
        {0x33b2, 0x68},
        {0x33b3, 0x42},
        {0x33f9, 0x90},
        {0x33fb, 0xd0},
        {0x33fc, 0x0f},
        {0x33fd, 0x1f},
        {0x349f, 0x03},
        {0x34a6, 0x0f},
        {0x34a7, 0x1f},
        {0x34a8, 0x42},
        {0x34a9, 0x18},
        {0x34aa, 0x01},
        {0x34ab, 0x43},
        {0x34ac, 0x01},
        {0x34ad, 0x80},
        {0x3630, 0xf4},
        {0x3632, 0x44},
        {0x3633, 0x22},
        {0x3639, 0xf4},
        {0x363c, 0x47},
        {0x3670, 0x09},
        {0x3674, 0xf4},
        {0x3675, 0xfb},
        {0x3676, 0xed},
        {0x367c, 0x09},
        {0x367d, 0x0f},
        {0x3690, 0x22},
        {0x3691, 0x22},
        {0x3692, 0x22},
        {0x3698, 0x89},
        {0x3699, 0x96},
        {0x369a, 0xd0},
        {0x369b, 0xd0},
        {0x369c, 0x09},
        {0x369d, 0x0f},
        {0x36a2, 0x09},
        {0x36a3, 0x0f},
        {0x36a4, 0x1f},
        {0x36d0, 0x01},
        {0x36ea, 0x0b},
        {0x36eb, 0x0c},
        {0x36ec, 0x1c},
        {0x36ed, 0x18},
        {0x3722, 0xc1},
        {0x3724, 0x41},
        {0x3725, 0xc1},
        {0x3728, 0x20},
        {0x37fa, 0xcb},
        {0x37fb, 0x32},
        {0x37fc, 0x11},
        {0x37fd, 0x07},
        {0x3900, 0x0d},
        {0x3905, 0x98},
        {0x3919, 0x04},
        {0x391b, 0x81},
        {0x391c, 0x10},
        {0x3933, 0x81},
        {0x3934, 0xd0},
        {0x3940, 0x75},
        {0x3941, 0x00},
        {0x3942, 0x01},
        {0x3943, 0xd1},
        {0x3952, 0x02},
        {0x3953, 0x0f},
        {0x3e01, 0x95},
        {0x3e02, 0xa0},
        {0x3e08, 0x1f},
        {0x3e1b, 0x14},
        {0x440e, 0x02},
        {0x4509, 0x38},
        {0x4819, 0x06},
        {0x481b, 0x03},
        {0x481d, 0x0b},
        {0x481f, 0x02},
        {0x4821, 0x08},
        {0x4823, 0x03},
        {0x4825, 0x02},
        {0x4827, 0x03},
        {0x4829, 0x04},
        {0x484c, 0x0a},
        {0x5799, 0x06},
        {0x5ae0, 0xfe},
        {0x5ae1, 0x40},
        {0x5ae2, 0x30},
        {0x5ae3, 0x28},
        {0x5ae4, 0x20},
        {0x5ae5, 0x30},
        {0x5ae6, 0x28},
        {0x5ae7, 0x20},
        {0x5ae8, 0x3c},
        {0x5ae9, 0x30},
        {0x5aea, 0x28},
        {0x5aeb, 0x3c},
        {0x5aec, 0x30},
        {0x5aed, 0x28},
        {0x5aee, 0xfe},
        {0x5aef, 0x40},
        {0x5af4, 0x30},
        {0x5af5, 0x28},
        {0x5af6, 0x20},
        {0x5af7, 0x30},
        {0x5af8, 0x28},
        {0x5af9, 0x20},
        {0x5afa, 0x3c},
        {0x5afb, 0x30},
        {0x5afc, 0x28},
        {0x5afd, 0x3c},
        {0x5afe, 0x30},
        {0x5aff, 0x28},
        {0x320e, 0x09},
        {0x320f, 0x5d},
        {0x3e01, 0x95},
        {0x3e02, 0x70},
        {0x36e9, 0x53},
        {0x37f9, 0x33},
        {0x0100, 0x01},
        {SC2336PS1_REG_END, 0x00},	/* END MARKER */
};

static struct tx_isp_sensor_win_setting sc2336ps1_win_sizes[] = {
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 15 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc2336ps1_init_regs_1920_1080_15fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sc2336ps1_win_sizes[0];

static struct regval_list sc2336ps1_stream_on_mipi[] = {
	{0x0100, 0x01},
	{SC2336PS1_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc2336ps1_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SC2336PS1_REG_END, 0x00},	/* END MARKER */
};

int sc2336ps1_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
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

int sc2336ps1_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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
static int sc2336ps1_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC2336PS1_REG_END) {
		if (vals->reg_num == SC2336PS1_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2336ps1_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc2336ps1_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC2336PS1_REG_END) {
		if (vals->reg_num == SC2336PS1_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2336ps1_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc2336ps1_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sc2336ps1_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret = 0;
	unsigned char v;

        ret = sc2336ps1_write(sd, 0x301a, 0xf8);
        ret += sc2336ps1_write(sd, 0x0100, 0x01);
        private_msleep(1);

	ret += sc2336ps1_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC2336PS1_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret += sc2336ps1_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC2336PS1_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

        ret += sc2336ps1_write(sd, 0x0100, 0x00);

	return 0;
}

static int sc2336ps1_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	//integration time
	ret = sc2336ps1_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc2336ps1_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc2336ps1_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));

	//sensor analog gain
	ret += sc2336ps1_write(sd, 0x3e09, (unsigned char)(((again >> 8) & 0xff)));
	//sensor dig fine gain
	ret += sc2336ps1_write(sd, 0x3e07, (unsigned char)(again & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int sc2336ps1_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
	ret += sc2336ps1_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc2336ps1_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc2336ps1_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));

	if (ret < 0)
		return ret;

	return 0;
}

static int sc2336ps1_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sc2336ps1_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sc2336ps1_write(sd, 0x3e08, (unsigned char)(((value >> 8) & 0xff)));
	if (ret < 0)
		return ret;
	gain_val = value;

	return 0;
}
#endif

static int sc2336ps1_set_logic(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc2336ps1_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc2336ps1_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc2336ps1_init(struct tx_isp_subdev *sd, int enable)
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

	ret = sc2336ps1_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc2336ps1_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2336ps1_write_array(sd, sc2336ps1_stream_on_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc2336ps1 stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2336ps1_write_array(sd, sc2336ps1_stream_off_mipi);
		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc2336ps1 stream off\n");
	}

	return ret;
}

static int sc2336ps1_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

        ISP_WARNING("[%s %d] Frame rate setting is not supported !!!\n", __func__, __LINE__);
        return 0;
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	sclk = SC2336PS1_SUPPORT_30FPS_SCLK;

	ret = sc2336ps1_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sc2336ps1_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("err: sc2336ps1 read err\n");
		return ret;
	}
	hts = ((hts << 8) + tmp);
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += sc2336ps1_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc2336ps1_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: sc2336ps1_write err\n");
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

static int sc2336ps1_set_mode(struct tx_isp_subdev *sd, int value)
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

static int sc2336ps1_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc2336ps1_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"sc2336ps1_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc2336ps1_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc2336ps1 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("sc2336ps1 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc2336ps1", sizeof("sc2336ps1"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc2336ps1_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	val = sc2336ps1_read(sd, 0x3221, &val);
	switch(enable) {
	case 0:
		sc2336ps1_write(sd, 0x3221, val & 0x99);
		break;
	case 1:
		sc2336ps1_write(sd, 0x3221, val | 0x06);
		break;
	case 2:
		sc2336ps1_write(sd, 0x3221, val | 0x60);
		break;
	case 3:
		sc2336ps1_write(sd, 0x3221, val | 0x66);
		break;
	}

	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sc2336ps1_fsync(struct tx_isp_subdev *sd, struct tx_isp_sensor_fsync *fsync)
{

        if (fsync->place != TX_ISP_SENSOR_FSYNC_PLACE_STREAMON_AFTER)
                return 0;
        switch (fsync->call_index) {
        case 0:
                switch (fsync_mode) {
                case 2:
                        break;
                case 3:
                        sc2336ps1_write(sd, 0x3224, 0x83);
                        break;
                }
                break;
        }

        return 0;
}

static int sc2336ps1_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
	     	ret = sc2336ps1_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
	//	if(arg)
	//		ret = sc2336ps1_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
	//	if(arg)
	//		ret = sc2336ps1_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc2336ps1_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc2336ps1_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc2336ps1_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2336ps1_write_array(sd, sc2336ps1_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2336ps1_write_array(sd, sc2336ps1_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc2336ps1_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = sc2336ps1_set_vflip(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if(arg)
			ret = sc2336ps1_set_logic(sd, *(int*)arg);
	default:
		break;
	}

	return ret;
}

static int sc2336ps1_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc2336ps1_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc2336ps1_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	sc2336ps1_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc2336ps1_core_ops = {
	.g_chip_ident = sc2336ps1_g_chip_ident,
	.reset = sc2336ps1_reset,
	.init = sc2336ps1_init,
	/*.ioctl = sc2336ps1_ops_ioctl,*/
	.g_register = sc2336ps1_g_register,
	.s_register = sc2336ps1_s_register,
};

static struct tx_isp_subdev_video_ops sc2336ps1_video_ops = {
	.s_stream = sc2336ps1_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc2336ps1_sensor_ops = {
	.ioctl	= sc2336ps1_sensor_ops_ioctl,
        .fsync = sc2336ps1_fsync,
};

static struct tx_isp_subdev_ops sc2336ps1_ops = {
	.core = &sc2336ps1_core_ops,
	.video = &sc2336ps1_video_ops,
	.sensor = &sc2336ps1_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc2336ps1",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc2336ps1_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

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
        {
                unsigned int arate = 0,mrate = 0;
                unsigned int want_rate = 0;
                struct clk *clka = NULL;
                struct clk *clkm = NULL;

                want_rate=24000000;
                clka = clk_get(NULL, "sclka");
                clkm = clk_get(NULL, "mpll");
                arate = clk_get_rate(clka);
                mrate = clk_get_rate(clkm);
                if((arate%want_rate) && (mrate%want_rate)) {
                        if(want_rate == 37125000){
                                if(arate >= 1400000000) {
                                        arate = 1485000000;
                                } else if((arate >= 1100) || (arate < 1400)) {
                                        arate = 1188000000;
                                } else if(arate <= 1100) {
                                        arate = 891000000;
                                }
                        } else {
                                mrate = arate%want_rate;
                                arate = arate-mrate;
                        }
                        clk_set_rate(clka, arate);
                        clk_set_parent(sensor->mclk, clka);
                } else if(!(arate%want_rate)) {
                        clk_set_parent(sensor->mclk, clka);
                } else if(!(mrate%want_rate)) {
                        clk_set_parent(sensor->mclk, clkm);
                }
                private_clk_set_rate(sensor->mclk, want_rate);
                private_clk_enable(sensor->mclk);
        }

	/*
	  convert sensor-gain into isp-gain,
	*/
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sc2336ps1_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc2336ps1_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc2336ps1\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int sc2336ps1_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc2336ps1_id[] = {
	{ "sc2336ps1", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc2336ps1_id);

static struct i2c_driver sc2336ps1_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc2336ps1",
	},
	.probe		= sc2336ps1_probe,
	.remove		= sc2336ps1_remove,
	.id_table	= sc2336ps1_id,
};

static __init int init_sc2336ps1(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init sc2336ps1 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&sc2336ps1_driver);
}

static __exit void exit_sc2336ps1(void)
{
	private_i2c_del_driver(&sc2336ps1_driver);
}

module_init(init_sc2336ps1);
module_exit(exit_sc2336ps1);

MODULE_DESCRIPTION("A low-level driver for SmartSens sc2336ps1 sensors");
MODULE_LICENSE("GPL");
