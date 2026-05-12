/*
 * jxk351p.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution       fps      interface            mode
 *   0          1984*1984        30       mipi_2lane           linear
 *   1          2000*2000        30       mipi_2lane           linear
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

#define JXK351P_CHIP_ID_H (0x08)
#define JXK351P_CHIP_ID_L (0x56)
#define JXK351P_REG_END 0xffff
#define JXK351P_REG_DELAY 0xfffe
#define JXK351P_SUPPORT_30FPS_SCLK 2400*2100*30
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20240808a"

uint8_t dismode;
static int rst_gpio = GPIO_PA(18);
//static int pwdn_gpio = -1;

static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

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
	unsigned int value;
	unsigned int gain;
};

struct again_lut jxk351p_again_lut[] = {
	{0x0, 0},
	{0x1, 5731},
	{0x2, 11136},
	{0x3, 16247},
	{0x4, 21097},
	{0x5, 25710},
	{0x6, 30108},
	{0x7, 34311},
	{0x8, 38335},
	{0x9, 42195},
	{0xa, 45903},
	{0xc, 49471},
	{0xc, 52910},
	{0xd, 56227},
	{0xe, 59433},
	{0xf, 62533},
	{0x10, 65535},
	{0x11, 71266},
	{0x12, 76671},
	{0x13, 81782},
	{0x14, 86632},
	{0x15, 91245},
	{0x16, 95643},
	{0x17, 99846},
	{0x18, 103870},
	{0x19, 107730},
	{0x1a, 111438},
	{0x1b, 115006},
	{0x1c, 118445},
	{0x1d, 121762},
	{0x1e, 124968},
	{0x1f, 128068},
	{0x20, 131070},
	{0x21, 136801},
	{0x22, 142206},
	{0x23, 147317},
	{0x24, 152167},
	{0x25, 156780},
	{0x26, 161178},
	{0x27, 165381},
	{0x28, 169405},
	{0x29, 173265},
	{0x2a, 176973},
	{0x2b, 180541},
	{0x2c, 183980},
	{0x2d, 187297},
	{0x2e, 190503},
	{0x2f, 193603},
	{0x30, 196605},
	{0x31, 202336},
	{0x32, 207741},
	{0x33, 212852},
	{0x34, 217702},
	{0x35, 222315},
	{0x36, 226713},
	{0x37, 230916},
	{0x38, 234940},
	{0x39, 238800},
	{0x3a, 242508},
	{0x3b, 246076},
	{0x3c, 249515},
	{0x3d, 252832},
	{0x3e, 256038},
	{0x3f, 259138},
	{0x40, 262140},
	{0x41, 267871},
	{0x42, 273276},
	{0x43, 278387},
	{0x44, 283237},
	{0x45, 287850},
	{0x46, 292248},
	{0x47, 296451},
	{0x48, 300475},
	{0x49, 304335},
	{0x4a, 308043},
	{0x4b, 311611},
	{0x4c, 315050},
	{0x4d, 318367},
	{0x4e, 321573},
	{0x4f, 324673},
};

struct tx_isp_sensor_attribute jxk351p_attr;

unsigned int jxk351p_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = jxk351p_again_lut;

	while (lut->gain <= jxk351p_attr.max_again)
	{
		if (isp_gain == 0)
		{
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if (isp_gain < lut->gain)
		{
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else
		{
			if ((lut->gain == jxk351p_attr.max_again) && (isp_gain >= lut->gain))
			{
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}
	return isp_gain;
}

unsigned int jxk351p_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}


struct tx_isp_mipi_bus jxk351p_mipi_1984_1984 = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 378,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10, // RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 1984,
	.image_theight = 1984,
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


struct tx_isp_mipi_bus jxk351p_mipi_2000_2000 = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 378,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10, // RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 2000,
	.image_theight = 2000,
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

struct tx_isp_sensor_attribute jxk351p_attr = {
	.name = "jxk351p",
	.chip_id = 0x856,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x40,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 378,
		.lans = 2,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10, // RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.image_twidth = 2000,
		.image_theight = 2000,
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
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 324673,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 2100 - 4,
	.integration_time_limit = 2100 - 4,
	.total_width = 2400,
	.total_height = 2100,
	.max_integration_time = 2100 - 4,
	.one_line_expr_in_us = 15,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = jxk351p_alloc_again,
	.sensor_ctrl.alloc_dgain = jxk351p_alloc_dgain,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list jxk351p_init_regs_1984_1984_30fps_mipi[] = {
	{0x0012,0x40},
	{0x00AD,0x01},
	{0x00AD,0x00},
	{0x000E,0x11},
	{0x000F,0x0C},
	{0x0010,0x3F},
	{0x000C,0x00},
	{0x0067,0xA2},
	{0x000D,0x21},
	{0x0064,0x31},
	{0x0065,0x9D},
	{0x00BE,0x18},
	{0x00BF,0x60},
	{0x00BC,0xC0},
	{0x0020,0x2C},
	{0x0021,0x01},
	{0x0022,0x34},
	{0x0023,0x08},
	{0x0024,0xF0},
	{0x0025,0xC0},
	{0x0026,0x71},
	{0x0027,0x10},
	{0x0028,0x0D},
	{0x0029,0x00},
	{0x002B,0x10},
	{0x002C,0x02},
	{0x002D,0x08},
	{0x002E,0xF8},
	{0x002F,0x14},
	{0x0030,0xF4},
	{0x0087,0xC5},
	{0x009D,0xB9},
	{0x00AC,0x00},
	{0x001D,0x00},
	{0x001E,0x10},
	{0x003A,0xD5},
	{0x003B,0x9B},
	{0x003C,0x6D},
	{0x003D,0x59},
	{0x003E,0x12},
	{0x003F,0x14},
	{0x0042,0x11},
	{0x0043,0x00},
	{0x0070,0xA0},
	{0x0071,0x24},
	{0x0076,0x08},
	{0x0006,0x00},
	{0x0008,0x04},
	{0x009F,0x4C},
	{0x007E,0x0B},
	{0x0031,0x08},
	{0x0032,0x04},
	{0x0033,0xCC},
	{0x0038,0xCA},
	{0x006F,0x00},
	{0x0078,0x5F},
	{0x00B0,0x14},
	{0x00B1,0xA0},
	{0x00B2,0x24},
	{0x00B3,0x2A},
	{0x00B5,0x50},
	{0x00B6,0x57},
	{0x00B8,0x06},
	{0x00B9,0x08},
	{0x00BA,0x8B},
	{0x00BB,0x8E},
	{0x00C3,0x90},
	{0x00F9,0x00},
	{0x0056,0xE1},
	{0x0057,0x48},
	{0x0058,0x42},
	{0x0059,0x24},
	{0x005A,0x80},
	{0x005B,0x10},
	{0x005C,0x10},
	{0x005D,0x49},
	{0x0060,0x48},
	{0x0061,0x00},
	{0x0062,0x20},
	{0x0068,0x00},
	{0x0069,0x90},
	{0x00A5,0x08},
	{0x00AA,0x00},
	{0x00C1,0xC0},
	{0x00C4,0x00},
	{0x00D4,0xFF},
	{0x00EB,0x15},
	{0x00EC,0x43},
	{0x00E1,0xF2},
	{0x0080,0x81},
	{0x0081,0x44},
	{0x00FB,0x20},
	{0x00FC,0x32},
	{0x00FA,0x01},
	{0x0016,0xFF},
	{0x0017,0x08},
	{0x0049,0x40},
	{0x0085,0x00},
	{0x00B4,0x01},
	{0x00D2,0x80},
	{0x00D0,0x00},
	{0x00D3,0x27},
	{0x0039,0x8A},
	{0x00FF,0x01},
	{0x0074,0x04},
	{0x00FF,0x00},
	{0x0089,0x00},
	{0x0012,0x00},
	{JXK351P_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list jxk351p_init_regs_2000_2000_30fps_mipi[] = {
	{0x0012,0x40},
	{0x00AD,0x01},
	{0x00AD,0x00},
	{0x000E,0x11},
	{0x000F,0x0C},
	{0x0010,0x38},
	{0x000C,0x00},
	{0x0067,0xA2},
	{0x000D,0x21},
	{0x0064,0x31},
	{0x0065,0x9A},
	{0x00BE,0x18},
	{0x00BF,0x60},
	{0x00BC,0xC0},
	{0x0020,0x2C},
	{0x0021,0x01},
	{0x0022,0x34},
	{0x0023,0x08},
	{0x0024,0xF4},
	{0x0025,0xD0},
	{0x0026,0x71},
	{0x0027,0x10},
	{0x0028,0x0D},
	{0x0029,0x00},
	{0x002B,0x10},
	{0x002C,0x00},
	{0x002D,0x05},
	{0x002E,0xFA},
	{0x002F,0x14},
	{0x0030,0xF8},
	{0x0087,0xC5},
	{0x009D,0xB9},
	{0x00AC,0x00},
	{0x001D,0x00},
	{0x001E,0x10},
	{0x003A,0xD5},
	{0x003B,0x9B},
	{0x003C,0x6D},
	{0x003D,0x59},
	{0x003E,0x12},
	{0x003F,0x14},
	{0x0042,0x11},
	{0x0043,0x00},
	{0x0070,0xA0},
	{0x0071,0x24},
	{0x0076,0x08},
	{0x0006,0x00},
	{0x0008,0x04},
	{0x009F,0x4C},
	{0x007E,0x0B},
	{0x0031,0x08},
	{0x0032,0x04},
	{0x0033,0xCC},
	{0x0038,0xCA},
	{0x006F,0x00},
	{0x0078,0x5F},
	{0x00B0,0x14},
	{0x00B1,0xA0},
	{0x00B2,0x24},
	{0x00B3,0x2A},
	{0x00B5,0x50},
	{0x00B6,0x57},
	{0x00B8,0x06},
	{0x00B9,0x08},
	{0x00BA,0x8B},
	{0x00BB,0x8E},
	{0x00C3,0x90},
	{0x00F9,0x00},
	{0x0056,0xF1},
	{0x0057,0x40},
	{0x0058,0x42},
	{0x0059,0x66},
	{0x005A,0x80},
	{0x005B,0x10},
	{0x005C,0x10},
	{0x005D,0x49},
	{0x0060,0x40},
	{0x0061,0x00},
	{0x0062,0x60},
	{0x0068,0x00},
	{0x0069,0x90},
	{0x00A5,0x08},
	{0x00AA,0x00},
	{0x00C1,0xC0},
	{0x00C4,0x00},
	{0x00D4,0xFF},
	{0x00EB,0x15},
	{0x00EC,0x03},
	{0x00E1,0xF2},
	{0x0080,0x81},
	{0x0081,0x44},
	{0x00FB,0x20},
	{0x00FC,0x32},
	{0x00FA,0x01},
	{0x0016,0xFF},
	{0x0017,0x08},
	{0x0049,0x40},
	{0x0085,0x00},
	{0x00B4,0x01},
	{0x00D2,0x80},
	{0x00D0,0x00},
	{0x00D3,0x27},
	{0x0039,0x8A},
	{0x00FF,0x01},
	{0x0074,0x04},
	{0x00FF,0x00},
	{0x0089,0x00},
	{0x0012,0x00},
	{JXK351P_REG_END, 0x00}, /* END MARKER */
};

/*
 * the order of the jxk351p_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting jxk351p_win_sizes[] = {
	/* resolution 1984*1984 2line @30fps*/
	{
		.width = 1984,
		.height = 1984,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = jxk351p_init_regs_1984_1984_30fps_mipi,
	},
	/* resolution 2000*2000 2line @30fps*/
	{
		.width = 2000,
		.height = 2000,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = jxk351p_init_regs_2000_2000_30fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &jxk351p_win_sizes[0];
/*
 * the part of driver was fixed.
 */
static struct regval_list jxk351p_stream_on_mipi[] = {
	{JXK351P_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list jxk351p_stream_off_mipi[] = {
	{JXK351P_REG_END, 0x00}, /* END MARKER */
};
int jxk351p_read(struct tx_isp_subdev *sd, unsigned char reg,
				 unsigned char *value)
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
		}};
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}
int jxk351p_write(struct tx_isp_subdev *sd, unsigned char reg,
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
/*
   static int jxk351p_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
   {
   int ret;
   unsigned char val;
   while (vals->reg_num != JXK351P_REG_END) {
   if (vals->reg_num == JXK351P_REG_DELAY) {
   private_msleep(vals->value);
   } else {
   ret = jxk351p_read(sd, vals->reg_num, &val);
   printk("{0x%x, 0x%x}\n", vals->reg_num, val);
   if (ret < 0)
   return ret;
   }
   vals++;
   }

   return 0;
   }
   */
static int jxk351p_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val = 0;
	while (vals->reg_num != JXK351P_REG_END)
	{
		if (vals->reg_num == JXK351P_REG_DELAY)
		{
			private_msleep(vals->value);
		}
		else
		{
			ret = jxk351p_write(sd, vals->reg_num, vals->value);
			ret = jxk351p_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int jxk351p_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int jxk351p_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = jxk351p_read(sd, 0x0a, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
	{
		return ret;
	}
	if (v != JXK351P_CHIP_ID_H)
		// return -ENODEV;
		*ident = v;

	ret = jxk351p_read(sd, 0x0b, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
	{
		return ret;
	}
	if (v != JXK351P_CHIP_ID_L)
		// return -ENODEV;
		*ident = (*ident << 8) | v;
	return 0;
}

static int jxk351p_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int expo = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	// set integration_time
	ret = jxk351p_write(sd, 0x00, (unsigned char)(again & 0x3f));

	// set sensor again
	ret += jxk351p_write(sd, 0x01, (unsigned char)(expo & 0xff));
	ret += jxk351p_write(sd, 0x02, (unsigned char)((expo >> 8) & 0xff));
	if (ret < 0)
		ISP_ERROR("err: jxk351p_write err\n");

	return ret;
}
/*
   static int jxk351p_set_integration_time(struct tx_isp_subdev *sd, int value)
   {
   int ret = 0;
   unsigned int expo = value;

   ret = jxk351p_write(sd,	0x01, (unsigned char)(expo & 0xff));
   ret += jxk351p_write(sd, 0x02, (unsigned char)((expo >> 8) & 0xff));
   if (ret < 0)
   return ret;

   return 0;
   }

   static int jxk351p_set_analog_gain(struct tx_isp_subdev *sd, int value)
   {
   int ret = 0;
   ret += jxk351p_write(sd, 0x00, (unsigned char)(value & 0x7f));
   if (ret < 0)
   ISP_ERROR("%s %d, sensor reg write err!!\n",__func__,__LINE__);
   return ret;

   return 0;
   }
   */
static int jxk351p_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int jxk351p_get_black_pedestal(struct tx_isp_subdev *sd, int value)
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
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

	return 0;
}

static int jxk351p_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable)
		return ISP_SUCCESS;

	sensor_set_attr(sd, wsize);
	sensor->video.state = TX_ISP_MODULE_INIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int jxk351p_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable)
	{
		if (sensor->video.state == TX_ISP_MODULE_INIT)
		{
			ret = jxk351p_write_array(sd, wsize->regs);
			ret = jxk351p_read(sd, 0x27, &dismode);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if (sensor->video.state == TX_ISP_MODULE_RUNNING)
		{

			ret = jxk351p_write_array(sd, jxk351p_stream_on_mipi);
			ISP_WARNING("jxk351p stream on\n");
		}
	}
	else
	{
		ret = jxk351p_write_array(sd, jxk351p_stream_off_mipi);
		ISP_WARNING("jxk351p stream off\n");
	}

	return ret;
}

static int jxk351p_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int max_fps;
	unsigned char val = 0;
	unsigned int newformat = 0; // the format is 24.8
	int ret = 0;
	struct tx_isp_sensor_register_info *info = &sensor->info;

	switch (info->default_boot)
	{
	case 0:
		sclk = 2400 * 2100 * 30;
		max_fps = SENSOR_OUTPUT_MAX_FPS;
		break;
	case 1:
		sclk = 2400 * 2100 * 30;
		max_fps = SENSOR_OUTPUT_MAX_FPS;
		break;
	default:
		ISP_WARNING("warn: fps(%d) no in range\n", fps);
	}
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8))
	{
		ISP_WARNING("warn: fps(%d) no in range\n", fps);
		return -1;
	}

	ret += jxk351p_read(sd, 0x21, &val);
	hts = val;
	ret += jxk351p_read(sd, 0x20, &val);
	hts = (hts << 8) + val; /* frame width = hts*8 */
	hts=hts*8;
	if (0 != ret)
	{
		ISP_ERROR("err: jxk351p read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
#if 0
	/*use group write*/
	jxk351p_write(sd, 0xc0, 0x22);
	jxk351p_write(sd, 0xc1, (unsigned char)(vts & 0xff));
	jxk351p_write(sd, 0xc2, 0x23);
	jxk351p_write(sd, 0xc3, (unsigned char)(vts >> 8));
	ret = jxk351p_read(sd, 0x1f, &val);
	if(ret < 0)
		return -1;
	val |= (1 << 7); //set bit[7],	register group write function,	auto clean
	jxk351p_write(sd, 0x1f, val);
	pr_debug("after register 0x1f value : 0x%02x\n", val);
#else
	//vts = vts >> 2;
	ret = jxk351p_write(sd, 0x22, (unsigned char)(vts & 0xff));
	ret += jxk351p_write(sd, 0x23, (unsigned char)(vts >> 8));
#endif
	if (0 != ret)
	{
		ISP_ERROR("err: jxk351p_write err\n");
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

static int jxk351p_set_mode(struct tx_isp_subdev *sd, int value)
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

static int jxk351p_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val = 0;

	/* 2'b01: mirror; 2'b10:flip*/
	enable &= 0x3;
	switch (enable)
	{
	case 0: /*normal*/
		val &= 0xCF;
		val |= 0x00;
		break;
	case 1: /*mirror*/
		val &= 0xCF;
		val |= 0x20; /*mirror*/
		break;
	case 2: /*filp*/
		val &= 0xCF;
		val |= 0x10;
		break;
	case 3: /*mirror & filp*/
		val &= 0xCF;
		val |= 0x30;
		break;
	default:
		break;
	}

	ret = jxk351p_write(sd, 0x12, val);
	if (0 != ret)
	{
		ISP_ERROR("%s:%d, jxk351p_write err!!\n", __func__, __LINE__);
		return ret;
	}

	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	unsigned long rate;

	switch (info->default_boot)
	{
	case 0:
		wsize = &jxk351p_win_sizes[0];
		memcpy((void*)(&(jxk351p_attr.mipi)),(void*)(&jxk351p_mipi_1984_1984),sizeof(jxk351p_mipi_1984_1984));
		break;
	case 1:
		wsize = &jxk351p_win_sizes[1];
		memcpy((void*)(&(jxk351p_attr.mipi)),(void*)(&jxk351p_mipi_2000_2000),sizeof(jxk351p_mipi_2000_2000));
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
		break;
	}

	switch (info->video_interface)
	{
	case TISP_SENSOR_VI_MIPI_CSI0:
		jxk351p_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		jxk351p_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		jxk351p_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		jxk351p_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		jxk351p_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this interface!!!\n");
	}

	switch (info->mclk)
	{
	case TISP_SENSOR_MCLK0:
	case TISP_SENSOR_MCLK1:
	case TISP_SENSOR_MCLK2:
		sclka = private_devm_clk_get(&client->dev, "mux_cim");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim");
		set_sensor_mclk_function(0);
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	rate = private_clk_get_rate(sensor->mclk);
	if (IS_ERR(sensor->mclk))
	{
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 27000000);
	private_clk_prepare_enable(sensor->mclk);

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

	return 0;

err_get_mclk:
	return -1;
}

static int jxk351p_g_chip_ident(struct tx_isp_subdev *sd,
								struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	sensor_attr_check(sd);
	info->rst_gpio = rst_gpio;
	if (1)
	{
		ret = private_gpio_request(info->rst_gpio, "jxk351p_reset");
		if (!ret)
		{
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(info->rst_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(info->rst_gpio, 1);
			private_msleep(5);
		}
		else
		{
			ISP_ERROR("gpio requrest fail %d\n", info->rst_gpio);
		}
	}
	if (info->pwdn_gpio != -1)
	{
		ret = private_gpio_request(info->pwdn_gpio, "jxk351p_pwdn");
		if (!ret)
		{
			private_gpio_direction_output(info->pwdn_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(info->pwdn_gpio, 0);
			private_msleep(5);
		}
		else
		{
			ISP_ERROR("gpio requrest fail %d\n", info->pwdn_gpio);
		}
	}
	ret = jxk351p_detect(sd, &ident);
	if (ret)
	{
		ISP_ERROR("chip found @ 0x%x (%s) is not an jxk351p chip.\n",
				  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("jxk351p chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n", SENSOR_VERSION);
	if (chip)
	{
		memcpy(chip->name, "jxk351p", sizeof("jxk351p"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int jxk351p_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd))
	{
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd)
	{
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = jxk351p_set_expo(sd, sensor_val->value);
		break;

		/*case TX_ISP_EVENT_SENSOR_INT_TIME:
		  if(arg)
		  ret = jxk351p_set_integration_time(sd, sensor_val->value);
		  break;
		  case TX_ISP_EVENT_SENSOR_AGAIN:
		  if(arg)
		  ret = jxk351p_set_analog_gain(sd, sensor_val->value);
		  break;
		  */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = jxk351p_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = jxk351p_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = jxk351p_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = jxk351p_write_array(sd, jxk351p_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = jxk351p_write_array(sd, jxk351p_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = jxk351p_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if (arg)
			ret = jxk351p_set_vflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int jxk351p_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = jxk351p_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int jxk351p_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len))
	{
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	jxk351p_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops jxk351p_core_ops = {
	.g_chip_ident = jxk351p_g_chip_ident,
	.reset = jxk351p_reset,
	.init = jxk351p_init,
	/*.ioctl = jxk351p_ops_ioctl,*/
	.g_register = jxk351p_g_register,
	.s_register = jxk351p_s_register,
};

static struct tx_isp_subdev_video_ops jxk351p_video_ops = {
	.s_stream = jxk351p_s_stream,
};

static struct tx_isp_subdev_sensor_ops jxk351p_sensor_ops = {
	.ioctl = jxk351p_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops jxk351p_ops = {
	.core = &jxk351p_core_ops,
	.video = &jxk351p_video_ops,
	.sensor = &jxk351p_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "jxk351p",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int jxk351p_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

	/*
	   convert sensor-gain into isp-gain,
	   */
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->dev = &client->dev;
	jxk351p_attr.expo_fs = 1;
	sensor->video.shvflip = 1;
	sensor->video.attr = &jxk351p_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &jxk351p_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	ISP_WARNING("probe ok ------->jxk351p\n");

	return 0;
}

static int jxk351p_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	if (info->rst_gpio != -1)
		private_gpio_free(info->rst_gpio);
	if (info->pwdn_gpio != -1)
		private_gpio_free(info->pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	private_devm_clk_put(&client->dev, sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);
	return 0;
}

static const struct i2c_device_id jxk351p_id[] = {
	{"jxk351p", 0},
	{}};
MODULE_DEVICE_TABLE(i2c, jxk351p_id);

static struct i2c_driver jxk351p_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "jxk351p",
	},
	.probe = jxk351p_probe,
	.remove = jxk351p_remove,
	.id_table = jxk351p_id,
};

static __init int init_jxk351p(void)
{
	/* ret = private_driver_get_interface(); */
	/* if(ret){ */
	/*	ISP_ERROR("Failed to init jxk351p dirver.\n"); */
	/*	return -1; */
	/* } */
	return private_i2c_add_driver(&jxk351p_driver);
}

static __exit void exit_jxk351p(void)
{
	private_i2c_del_driver(&jxk351p_driver);
}

module_init(init_jxk351p);
module_exit(exit_jxk351p);

MODULE_DESCRIPTION("A low-level driver for SOI jxk351p sensors");
MODULE_LICENSE("GPL");

