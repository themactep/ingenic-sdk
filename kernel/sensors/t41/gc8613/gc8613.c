/*
 * gc8613.c
 *
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution       fps     interface              mode
 *   0          3840*2160        25     mipi_2lane             linear
 *   0          3840*2160        30     mipi_2lane             linear
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

#define GC8613_CHIP_ID_H (0x86)
#define GC8613_CHIP_ID_L (0x13)
#define GC8613_REG_END 0xffff
#define GC8613_REG_DELAY 0x0000
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20230427a"

static int reset_gpio = GPIO_PC(27);
static int pwdn_gpio = -1;
static int shvflip = 1;

struct regval_list
{
	uint16_t reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut
{
	unsigned int index;
	unsigned char reg614;
	unsigned char reg615;
	unsigned char reg225;
	unsigned char reg1467;
	unsigned char reg1468;
	unsigned char regb8;
	unsigned char regb9;
	unsigned int gain;
};

struct again_lut gc8613_again_lut[] = {
	{0x00, 0x00, 0x00, 0x00, 0x0d, 0x0d, 0x01, 0x12, 0},	  // 1.000000
	{0x01, 0x90, 0x02, 0x00, 0x0d, 0x0d, 0x01, 0x20, 14995},  // 1.171875
	{0x02, 0x01, 0x00, 0x00, 0x0e, 0x0e, 0x01, 0x30, 31177},  // 1.390625
	{0x03, 0x91, 0x02, 0x00, 0x0e, 0x0e, 0x02, 0x05, 47704},  // 1.656250
	{0x04, 0x02, 0x00, 0x00, 0x0f, 0x0f, 0x02, 0x19, 65536},  // 2.000000
	{0x05, 0x92, 0x02, 0x00, 0x0f, 0x0f, 0x02, 0x3f, 81159},  // 2.359375
	{0x06, 0x03, 0x00, 0x00, 0x10, 0x10, 0x03, 0x20, 97243},  // 2.796875
	{0x07, 0x93, 0x02, 0x00, 0x10, 0x10, 0x04, 0x0a, 113240}, // 3.312500
	{0x08, 0x00, 0x00, 0x01, 0x11, 0x11, 0x05, 0x02, 131072}, // 4.000000
	{0x09, 0x90, 0x02, 0x01, 0x12, 0x12, 0x05, 0x39, 147007}, // 4.734375
	{0x0a, 0x01, 0x00, 0x01, 0x13, 0x13, 0x06, 0x3c, 162779}, // 5.593750
	{0x0b, 0x91, 0x02, 0x01, 0x14, 0x14, 0x08, 0x0d, 178776}, // 6.625000
	{0x0c, 0x02, 0x00, 0x01, 0x15, 0x15, 0x09, 0x21, 196608}, // 8.000000
	{0x0d, 0x92, 0x02, 0x01, 0x16, 0x16, 0x0b, 0x0f, 212543}, // 9.468750
	{0x0e, 0x03, 0x00, 0x01, 0x17, 0x17, 0x0d, 0x17, 228315}, // 11.187500
	{0x0f, 0x93, 0x02, 0x01, 0x18, 0x18, 0x0f, 0x33, 244423}, // 13.265625
	{0x10, 0x04, 0x00, 0x01, 0x19, 0x19, 0x12, 0x30, 262144}, // 16.000000
	{0x11, 0x94, 0x02, 0x01, 0x1b, 0x1b, 0x16, 0x10, 278158}, // 18.953125
	{0x12, 0x05, 0x00, 0x01, 0x1d, 0x1d, 0x1a, 0x19, 293916}, // 22.390625
	{0x13, 0x95, 0x02, 0x01, 0x1e, 0x1e, 0x1f, 0x13, 309959}, // 26.531250
	{0x14, 0x06, 0x00, 0x01, 0x20, 0x20, 0x25, 0x08, 327680}, // 32.000000
	{0x15, 0x96, 0x02, 0x01, 0x22, 0x22, 0x2c, 0x03, 343694}, // 37.906250
	{0x16, 0xb6, 0x04, 0x01, 0x23, 0x23, 0x34, 0x0f, 359485}, // 44.796875
	{0x17, 0x86, 0x06, 0x01, 0x24, 0x24, 0x3d, 0x3d, 375523}, // 53.078125
	{0x18, 0x06, 0x08, 0x01, 0x26, 0x26, 0x49, 0x1f, 393216}, // 64.000000
};
struct tx_isp_sensor_attribute gc8613_attr;

unsigned int gc8613_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;

	*sensor_it = expo;

	return isp_it;
}

unsigned int gc8613_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = gc8613_again_lut;
	while (lut->gain <= gc8613_attr.max_again)
	{
		if (isp_gain == 0)
		{
			*sensor_again = 0;
			return lut[0].gain;
		}
		else if (isp_gain < lut->gain)
		{
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		}
		else
		{
			if ((lut->gain == gc8613_attr.max_again) && (isp_gain >= lut->gain))
			{
				*sensor_again = lut->index;
				return lut->gain;
			}
		}

		lut++;
	}

	return 0;
}

unsigned int gc8613_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus gc8613_mipi = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 1107,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10, // RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 3840,
	.image_theight = 2160,
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

struct tx_isp_sensor_attribute gc8613_attr = {
	.name = "gc8613",
	.chip_id = 0x8613,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.cbus_device = 0x31,
	.max_again = 393216,
	.max_dgain = 0,
	.expo_fs = 1,
	.min_integration_time = 1,
	.min_integration_time_short = 1,
	.min_integration_time_native = 1,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = gc8613_alloc_again,
	.sensor_ctrl.alloc_dgain = gc8613_alloc_dgain,
};

static struct regval_list gc8613_init_regs_3840_2160_25fps_mipi[] = {
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x0a38, 0x01},
	{0x0a20, 0x19},
	{0x061b, 0x17},
	{0x061c, 0x48},
	{0x061d, 0x05},
	{0x061e, 0x42},
	{0x061f, 0x05},
	{0x0a21, 0x10},
	{0x0a31, 0x7b},
	{0x0a34, 0x40},
	{0x0a35, 0x08},
	{0x0a37, 0x43},
	{0x0314, 0x70},
	{0x0315, 0x00},
	{0x031c, 0xce},
	{0x0219, 0x47},
	{0x0342, 0x03},
	{0x0343, 0x20},
	{0x0259, 0x08},
	{0x025a, 0x96},
	{0x0340, 0x08},
	{0x0341, 0xca},
	{0x0351, 0x00},
	{0x0345, 0x02},
	{0x0347, 0x02},
	{0x0348, 0x0f},
	{0x0349, 0x18},
	{0x034a, 0x08},
	{0x034b, 0x88},
	{0x034f, 0xf0},
	{0x0094, 0x0f},
	{0x0095, 0x00},
	{0x0096, 0x08},
	{0x0097, 0x70},
	{0x0099, 0x0c},
	{0x009b, 0x0c},
	{0x060c, 0x06},
	{0x060e, 0x20},
	{0x060f, 0x0f},
	{0x070c, 0x06},
	{0x070e, 0x20},
	{0x070f, 0x0f},
	{0x0087, 0x50},
	{0x0907, 0xd5},
	{0x0909, 0x06},
	{0x0902, 0x0b},
	{0x0904, 0x08},
	{0x0908, 0x08},
	{0x0903, 0xc5},
	{0x090c, 0x09},
	{0x0905, 0x10},
	{0x0906, 0x00},
	{0x072a, 0x58},
	{0x0724, 0x2b},
	{0x0727, 0x2b},
	{0x072b, 0x18},
	{0x073e, 0x40},
	{0x0078, 0x88},
	{0x0618, 0x01},
	{0x1466, 0x12},
	{0x1468, 0x10},
	{0x1467, 0x10},
	{0x0709, 0x40},
	{0x0719, 0x40},
	{0x1469, 0x80},
	{0x146a, 0xc0},
	{0x146b, 0x03},
	{0x1480, 0x02},
	{0x1481, 0x80},
	{0x1484, 0x08},
	{0x1485, 0xc0},
	{0x1430, 0x80},
	{0x1407, 0x10},
	{0x1408, 0x16},
	{0x1409, 0x03},
	{0x1434, 0x04},
	{0x1447, 0x75},
	{0x1470, 0x10},
	{0x1471, 0x13},
	{0x1438, 0x00},
	{0x143a, 0x00},
	{0x024b, 0x03},
	{0x0245, 0xc7},
	{0x025b, 0x07},
	{0x02bb, 0x77},
	{0x0612, 0x01},
	{0x0613, 0x24},
	{0x0243, 0x06},
	{0x0087, 0x53},
	{0x0089, 0x02},
	{0x0002, 0xeb},
	{0x005a, 0x0c},
	{0x0040, 0x83},
	{0x0075, 0x54},
	{0x0205, 0x0c},
	{0x0202, 0x01},
	{0x0203, 0x27},
	{0x061a, 0x02},
	{0x03fe, 0x00},
	{0x0106, 0x78},
	{0x0136, 0x03},
	{0x0181, 0x30},
	{0x0185, 0x01},
	{0x0180, 0x46},
	{0x0106, 0x38},
	{0x010d, 0xc0},
	{0x010e, 0x12},
	{0x0113, 0x02},
	{0x0114, 0x01},
	{0x0115, 0x12},
	{0x0100, 0x09},
	{GC8613_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list gc8613_init_regs_3840_2160_30fps_mipi[] =
	{
		{0x03fe, 0xf0},
		{0x03fe, 0x00},
		{0x03fe, 0x10},
		{0x0a38, 0x01},
		{0x0a20, 0x19},
		{0x061b, 0x17},
		{0x061c, 0x48},
		{0x061d, 0x05},
		{0x061e, 0x50},
		{0x061f, 0x05},
		{0x0a21, 0x10},
		{0x0a31, 0xb0},
		{0x0a34, 0x40},
		{0x0a35, 0x08},
		{0x0a37, 0x43},
		{0x0314, 0x70},
		{0x0315, 0x00},
		{0x031c, 0xce},
		{0x0219, 0x47},
		{0x0342, 0x03},
		{0x0343, 0x20},
		{0x0259, 0x08},
		{0x025a, 0x96},
		{0x0340, 0x08},
		{0x0341, 0xca},
		{0x0351, 0x00},
		{0x0345, 0x02},
		{0x0347, 0x02},
		{0x0348, 0x0f},
		{0x0349, 0x18},
		{0x034a, 0x08},
		{0x034b, 0x88},
		{0x034f, 0xf0},
		{0x0094, 0x0f},
		{0x0095, 0x00},
		{0x0096, 0x08},
		{0x0097, 0x70},
		{0x0099, 0x0c},
		{0x009b, 0x0c},
		{0x060c, 0x06},
		{0x060e, 0x20},
		{0x060f, 0x0f},
		{0x070c, 0x06},
		{0x070e, 0x20},
		{0x070f, 0x0f},
		{0x0087, 0x50},
		{0x0907, 0xd5},
		{0x0909, 0x06},
		{0x0902, 0x0b},
		{0x0904, 0x08},
		{0x0908, 0x08},
		{0x0903, 0xc5},
		{0x090c, 0x09},
		{0x0905, 0x10},
		{0x0906, 0x00},
		{0x072a, 0x58},
		{0x0724, 0x2b},
		{0x0727, 0x2b},
		{0x072b, 0x18},
		{0x073e, 0x40},
		{0x0078, 0x88},
		{0x0618, 0x01},
		{0x1466, 0x12},
		{0x1468, 0x10},
		{0x1467, 0x10},
		{0x0709, 0x40},
		{0x0719, 0x40},
		{0x1469, 0x80},
		{0x146a, 0xc0},
		{0x146b, 0x03},
		{0x1480, 0x02},
		{0x1481, 0x80},
		{0x1484, 0x08},
		{0x1485, 0xc0},
		{0x1430, 0x80},
		{0x1407, 0x10},
		{0x1408, 0x16},
		{0x1409, 0x03},
		{0x1434, 0x04},
		{0x1447, 0x75},
		{0x1470, 0x10},
		{0x1471, 0x13},
		{0x1438, 0x00},
		{0x143a, 0x00},
		{0x024b, 0x03},
		{0x0245, 0xc7},
		{0x025b, 0x07},
		{0x02bb, 0x77},
		{0x0612, 0x01},
		{0x0613, 0x24},
		{0x0243, 0x06},
		{0x0087, 0x53},
		{0x0089, 0x02},
		{0x0002, 0xeb},
		{0x005a, 0x0c},
		{0x0040, 0x83},
		{0x0075, 0x54},
		{0x0205, 0x0c},
		{0x0202, 0x01},
		{0x0203, 0x27},
		{0x061a, 0x02},
		{0x03fe, 0x00},
		{0x0106, 0x78},
		{0x0136, 0x03},
		{0x0181, 0x30},
		{0x0185, 0x01},
		{0x0180, 0x46},
		{0x0106, 0x38},
		{0x010d, 0xc0},
		{0x010e, 0x12},
		{0x0113, 0x02},
		{0x0114, 0x01},
		{0x0122, 0x11},
		{0x0123, 0x40},
		{0x0126, 0x0e},
		{0x0129, 0x12},
		{0x012a, 0x1a},
		{0x012b, 0x0f},
		{0x0100, 0x09},
		{GC8613_REG_END, 0x00}, /* END MARKER */
};
/*
 * the order of the jxf23_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting gc8613_win_sizes[] = {
	/* [0] 3840*2160 @ max 25fps*/
	{
		.width = 3840,
		.height = 2160,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = gc8613_init_regs_3840_2160_25fps_mipi,
	},
	{
		.width = 3840,
		.height = 2160,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = gc8613_init_regs_3840_2160_30fps_mipi,
	}

};

struct tx_isp_sensor_win_setting *wsize = &gc8613_win_sizes[0];

static struct regval_list gc8613_stream_on[] = {
	{GC8613_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list gc8613_stream_off[] = {
	{GC8613_REG_END, 0x00}, /* END MARKER */
};

int gc8613_read(struct tx_isp_subdev *sd, uint16_t reg,
				unsigned char *value)
{
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
		}};
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int gc8613_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int gc8613_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != GC8613_REG_END) {
		if (vals->reg_num == GC8613_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = gc8613_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
#endif

static int gc8613_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != GC8613_REG_END)
	{
		if (vals->reg_num == GC8613_REG_DELAY)
		{
			private_msleep(vals->value);
		}
		else
		{
			ret = gc8613_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int gc8613_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int gc8613_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v = 0;
	int ret;

	ret = gc8613_read(sd, 0x03f0, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != GC8613_CHIP_ID_H)
		return -ENODEV;
	ret = gc8613_read(sd, 0x03f1, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != GC8613_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int gc8613_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int expo = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;
	struct again_lut *val_lut = gc8613_again_lut;
	/*set integration time*/
	ret = gc8613_write(sd, 0x0203, expo & 0xff);
	ret += gc8613_write(sd, 0x0202, expo >> 8);
	/*set sensor analog gain*/
	ret += gc8613_write(sd, 0x031d, 0x2d);
	ret += gc8613_write(sd, 0x0614, val_lut[again].reg614);
	ret += gc8613_write(sd, 0x0615, val_lut[again].reg615);
	ret += gc8613_write(sd, 0x031d, 0x28);
	ret += gc8613_write(sd, 0x0225, val_lut[again].reg225);


	ret += gc8613_write(sd, 0x1467, val_lut[again].reg1467);
	ret += gc8613_write(sd, 0x1468, val_lut[again].reg1468);
	ret += gc8613_write(sd, 0x00b8, val_lut[again].regb8);
	ret += gc8613_write(sd, 0x00b9, val_lut[again].regb9);
	//
	if (ret < 0)
		ISP_ERROR("gc8613_write error  %d\n", __LINE__);

	return ret;
}

#if 0
static int gc8613_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = gc8613_write(sd, 0x0203, value & 0xff);
	ret += gc8613_write(sd, 0x0202, value >> 8);
	if (ret < 0)
		ISP_ERROR("gc8613_write error  %d\n" ,__LINE__ );

	return ret;
}

static int gc8613_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	struct again_lut *val_lut = gc8613_again_lut;

	ret += gc8613_write(sd, 0x031d ,0x2d);
	ret += gc8613_write(sd, 0x0614, val_lut[value].reg614);
	ret += gc8613_write(sd, 0x0615, val_lut[value].reg615);
	ret += gc8613_write(sd, 0x031d, 0x28);
	ret += gc8613_write(sd, 0x0225, val_lut[value].reg225);
	ret += gc8613_write(sd, 0x1467, val_lut[value].reg1467);
	ret += gc8613_write(sd, 0x1468, val_lut[value].reg1468);
	ret += gc8613_write(sd, 0x00b8, val_lut[value].regb8);
	ret += gc8613_write(sd, 0x00b9, val_lut[value].regb9);
	if (ret < 0)
		ISP_ERROR("gc8613_write error  %d\n" ,__LINE__ );

	return ret;
}
#endif

static int gc8613_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc8613_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_set_attr(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;

	return 0;
}

static int gc8613_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable)
		return ISP_SUCCESS;

	sensor_set_attr(sd, wsize);
	sensor->video.state = TX_ISP_MODULE_DEINIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int gc8613_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	if (init->enable)
	{
		if (sensor->video.state == TX_ISP_MODULE_DEINIT)
		{
			ret = gc8613_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if (sensor->video.state == TX_ISP_MODULE_RUNNING)
		{
			ret = gc8613_write_array(sd, gc8613_stream_on);
			ISP_WARNING("gc8613 stream on\n");
		}
	}
	else
	{
		ret = gc8613_write_array(sd, gc8613_stream_off);
		pr_debug("gc8613 stream off\n");
	}

	return ret;
}

static int gc8613_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int vts = 0;
	unsigned int hts = 0;
	unsigned int max_fps;
	unsigned char tmp;
	unsigned int newformat = 0; // the format is 24.8
	int ret = 0;
	switch (sensor->info.default_boot)
	{
	case 0:
		sclk = 2250 * 800 * 25;
		max_fps = 25;
		break;
	case 1:
		sclk = 0x8ca * 0x320 * 30;
		max_fps = 30;
		break;
	}
	/* the format of fps is 16/16. for example 30 << 16 | 2, the value is 30/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8))
	{
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}
	ret += gc8613_read(sd, 0x0342, &tmp);
	hts = tmp & 0x0f;
	ret += gc8613_read(sd, 0x0343, &tmp);
	if (ret < 0)
		return -1;
	hts = ((hts << 8) | tmp);
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = gc8613_write(sd, 0x0340, (unsigned char)((vts & 0x3f00) >> 8));
	ret += gc8613_write(sd, 0x0341, (unsigned char)(vts & 0xff));
	if (ret < 0)
		return -1;

	sensor->video.attr->max_integration_time_native = vts - 8;
	sensor->video.attr->integration_time_limit = vts - 8;
	sensor->video.attr->max_integration_time = vts - 8;
	sensor->video.fps = fps;
	sensor->video.attr->total_height = vts;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return 0;
}

static int gc8613_set_hvflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val, val1;

	/* 2'b01: mirror; 2'b10:flip*/
	val = gc8613_read(sd, 0x022c, &val);
	val1 = gc8613_read(sd, 0x0063, &val1);

	/* 2'b01 mirror; 2'b10 flip; 2'b11 mirror &flip */
	switch (enable)
	{
	case 0:
		gc8613_write(sd, 0x022c, val & 0x00); /*normal*/
		gc8613_write(sd, 0x0063, val1 & 0x00);
		break;
	case 1:
		gc8613_write(sd, 0x022c, 0x00); /*mirror*/
		gc8613_write(sd, 0x0063, 0x05);
		break;
	case 2:
		gc8613_write(sd, 0x022c, 0x01); /*filp*/
		gc8613_write(sd, 0x0063, 0x02);
		break;
	case 3:
		gc8613_write(sd, 0x022c, 0x01); /*mirror & filp*/
		gc8613_write(sd, 0x0063, 0x05);
		break;
	}

	return ret;
}
static int gc8613_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize)
	{
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}
	return ret;
}

static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	unsigned long rate;
	int ret = 0;

	switch (info->default_boot)
	{
	case 0:
		wsize = &gc8613_win_sizes[0];
		memcpy(&gc8613_attr.mipi, &gc8613_mipi, sizeof(gc8613_mipi));
		gc8613_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		gc8613_attr.mipi.clk = 1107;

		gc8613_attr.one_line_expr_in_us = 19;
		gc8613_attr.total_width = 3934;
		gc8613_attr.total_height = 2250;
		gc8613_attr.max_integration_time_native = 2250 - 8;
		gc8613_attr.integration_time_limit = 2250 - 8;
		gc8613_attr.max_integration_time = 2250 - 8;
		gc8613_attr.again = 0;
		gc8613_attr.integration_time = 0x10;
		break;
	case 1:
		wsize = &gc8613_win_sizes[1];
		memcpy(&gc8613_attr.mipi, &gc8613_mipi, sizeof(gc8613_mipi));
		gc8613_attr.mipi.clk = 1584;
		gc8613_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		gc8613_attr.one_line_expr_in_us = 14;
		gc8613_attr.total_width = 4693;
		gc8613_attr.total_height = 2250;
		gc8613_attr.max_integration_time_native = 2250 - 8;
		gc8613_attr.integration_time_limit = 2250 - 8;
		gc8613_attr.max_integration_time = 2250 - 8;
		gc8613_attr.again = 0;
		gc8613_attr.integration_time = 0x10;
		break;
	default:
		ISP_ERROR("Have no this setting!!!\n");
	}

	switch (info->mclk)
	{
	case TISP_SENSOR_MCLK0:
	case TISP_SENSOR_MCLK1:
	case TISP_SENSOR_MCLK2:
		sclka = private_devm_clk_get(&client->dev, SEN_MCLK);
		sensor->mclk = private_devm_clk_get(sensor->dev, SEN_BCLK);
		set_sensor_mclk_function(0);
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	if (IS_ERR(sensor->mclk))
	{
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	rate = private_clk_get_rate(sensor->mclk);
	if (((rate / 1000) % 27000) != 0)
	{
		ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
		sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
		if (IS_ERR(sclka))
		{
			pr_err("get sclka failed\n");
		}
		else
		{
			rate = private_clk_get_rate(sclka);
			if (((rate / 1000) % 27000) != 0)
			{
				private_clk_set_rate(sclka, 891000000);
			}
		}
	}

	private_clk_set_rate(sensor->mclk, 27000000);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

	return 0;

err_get_mclk:
	return -1;
}

static int gc8613_g_chip_ident(struct tx_isp_subdev *sd,
							   struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if (reset_gpio != -1)
	{
		ret = private_gpio_request(reset_gpio, "gc8613_reset");
		if (!ret)
		{
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}
		else
		{
			ISP_ERROR("gpio requrest fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1)
	{
		ret = private_gpio_request(pwdn_gpio, "gc8613_pwdn");
		if (!ret)
		{
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		}
		else
		{
			ISP_ERROR("gpio requrest fail %d\n", pwdn_gpio);
		}
	}
	ret = gc8613_detect(sd, &ident);
	if (ret)
	{
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc8613 chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("gc8613 chip found @ 0x%02x (%s)\n sensor drv version %s", client->addr, client->adapter->name, SENSOR_VERSION);
	if (chip)
	{
		memcpy(chip->name, "gc8613", sizeof("gc8613"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int gc8613_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	//	struct tx_isp_initarg *init = arg;

	if (IS_ERR_OR_NULL(sd))
	{
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd)
	{
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = gc8613_set_expo(sd, sensor_val->value);
		break;
		/*
			case TX_ISP_EVENT_SENSOR_INT_TIME:
				if(arg)
					ret = gc8613_set_integration_time(sd, sensor_val->value);
				break;
			case TX_ISP_EVENT_SENSOR_AGAIN:
				if(arg)
					ret = gc8613_set_analog_gain(sd, sensor_val->value);
				break;
		*/
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = gc8613_set_digital_gain(sd, sensor_val->value);
		break;

	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = gc8613_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = gc8613_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = gc8613_write_array(sd, gc8613_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = gc8613_write_array(sd, gc8613_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = gc8613_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if (arg)
			ret = gc8613_set_hvflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int gc8613_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len))
	{
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = gc8613_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int gc8613_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len))
	{
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc8613_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops gc8613_core_ops = {
	.g_chip_ident = gc8613_g_chip_ident,
	.reset = gc8613_reset,
	.init = gc8613_init,
	/*.ioctl = gc8613_ops_ioctl,*/
	.g_register = gc8613_g_register,
	.s_register = gc8613_s_register,
};

static struct tx_isp_subdev_video_ops gc8613_video_ops = {
	.s_stream = gc8613_s_stream,
};

static struct tx_isp_subdev_sensor_ops gc8613_sensor_ops = {
	.ioctl = gc8613_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops gc8613_ops = {
	.core = &gc8613_core_ops,
	.video = &gc8613_video_ops,
	.sensor = &gc8613_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "gc8613",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc8613_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
	{
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));
	sensor->dev = &client->dev;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	gc8613_attr.expo_fs = 1;
	sensor->video.attr = &gc8613_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc8613_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->gc8613\n");

	return 0;
}

static int gc8613_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id gc8613_id[] = {
	{"gc8613", 0},
	{}};
MODULE_DEVICE_TABLE(i2c, gc8613_id);

static struct i2c_driver gc8613_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "gc8613",
	},
	.probe = gc8613_probe,
	.remove = gc8613_remove,
	.id_table = gc8613_id,
};

static __init int init_gc8613(void)
{
	return private_i2c_add_driver(&gc8613_driver);
}

static __exit void exit_gc8613(void)
{
	private_i2c_del_driver(&gc8613_driver);
}

module_init(init_gc8613);
module_exit(exit_gc8613);

MODULE_DESCRIPTION("A low-level driver for gc8613 sensors");
MODULE_LICENSE("GPL");
