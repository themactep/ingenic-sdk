/*
* imx585.c
*
* Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* Settings:
* sboot        resolution      fps       interface              mode
*   0          3840*2160       30        mipi_2lane           linear
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

#define IMX585_CHIP_ID_H        (0x00)
#define IMX585_CHIP_ID_L        (0x07)
#define IMX585_REG_END          0xffff
#define IMX585_REG_DELAY        0xfffe
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION  "H20241221a"
#define AGAIN_MAX_DB 0x64
#define DGAIN_MAX_DB 0x64
#define LOG2_GAIN_SHIFT 16

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
module_param(data_type, int, S_IRUGO);
MODULE_PARM_DESC(data_type, "Sensor Date Type");

// static int wdr_bufsize = 7080000;
// module_param(wdr_bufsize, int, S_IRUGO);
// MODULE_PARM_DESC(wdr_bufsize, "Wdr Buf Size");


struct regval_list {
		uint16_t reg_num;
		unsigned char value;
};

/*
* the part of driver maybe modify about different sensor and different board.
*/
struct tx_isp_sensor_attribute imx585_attr;

unsigned int imx585_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
		uint16_t again=(isp_gain*20)>>LOG2_GAIN_SHIFT;
		// Limit Max gain
		if(again>AGAIN_MAX_DB+DGAIN_MAX_DB) again=AGAIN_MAX_DB+DGAIN_MAX_DB;

		/* p_ctx->again=again; */
		*sensor_again=again;
		isp_gain= (((int32_t)again)<<LOG2_GAIN_SHIFT)/20;

		return isp_gain;
}

// unsigned int imx585_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
// {
// 		uint16_t again=(isp_gain*20)>>LOG2_GAIN_SHIFT;
// 		// Limit Max gain
// 		if(again>AGAIN_MAX_DB+DGAIN_MAX_DB) again=AGAIN_MAX_DB+DGAIN_MAX_DB;

// 		/* p_ctx->again=again; */
// 		*sensor_again=again;
// 		isp_gain= (((int32_t)again)<<LOG2_GAIN_SHIFT)/20;

// 		return isp_gain;
// }


unsigned int imx585_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
		return 0;
}

struct tx_isp_mipi_bus imx585_mipi_linear = {
		.mode = SENSOR_MIPI_SONY_MODE,
		.clk = 1440,
		.lans = 2,
		.settle_time_apative_en = 0,
		.image_twidth = 3856,
		.image_theight = 2160,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.mipi_crop_start0x = 16,
		.mipi_sc.mipi_crop_start0y = 32,
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


struct tx_isp_sensor_attribute imx585_attr={
		.name = "imx585",
		.chip_id = 0x0007,
		.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
		.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
		.cbus_device = 0x1a,
		.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
		.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
		.max_again = 589824,
		.max_dgain = 0,
		.min_integration_time = 2,
		.min_integration_time_native = 2,
		// .min_integration_time_short = 4,
		.integration_time_apply_delay = 2,
		.again_apply_delay = 2,
		.dgain_apply_delay = 0,
		.sensor_ctrl.alloc_again = imx585_alloc_again,
		// .sensor_ctrl.alloc_again_short = imx585_alloc_again_short,
		.sensor_ctrl.alloc_dgain = imx585_alloc_dgain,
		.wdr_cache = 0,
};

static struct regval_list imx585_init_regs_3840_2160_30fps_mipi[] = {
	{0x3000,0x01},
	{0x3001,0x00},
	{0x3002,0x01},
	{0x3014,0x04},
	{0x3015,0x03},
	{0x3018,0x10},
	{0x3019,0x00},
	{0x301A,0x00},
	{0x301B,0x00},
	{0x301C,0x00},
	{0x301E,0x01},
	{0x3020,0x00},
	{0x3021,0x00},
	{0x3022,0x00},
	{0x3023,0x00},
	{0x3024,0x00},
	{0x3028,0xCA},
	{0x3029,0x08},
	{0x302A,0x00},
	{0x302C,0x4C},
	{0x302D,0x04},
	{0x3030,0x00},
	{0x3031,0x00},
	{0x3032,0x00},
	{0x303C,0x00},
	{0x303D,0x00},
	{0x303E,0x10},
	{0x303F,0x0F},
	{0x3040,0x01},
	{0x3042,0x00},
	{0x3043,0x00},
	{0x3044,0x00},
	{0x3045,0x00},
	{0x3046,0x84},
	{0x3047,0x08},
	{0x3050,0x08},
	{0x3051,0x00},
	{0x3052,0x00},
	{0x3054,0x0E},
	{0x3055,0x00},
	{0x3056,0x00},
	{0x3058,0x8A},
	{0x3059,0x01},
	{0x305A,0x00},
	{0x3060,0x16},
	{0x3061,0x01},
	{0x3062,0x00},
	{0x3064,0xC4},
	{0x3065,0x0C},
	{0x3066,0x00},
	{0x3069,0x00},
	{0x306A,0x00},
	{0x306C,0x00},
	{0x306D,0x00},
	{0x306E,0x00},
	{0x306F,0x00},
	{0x3070,0x00},
	{0x3071,0x00},
	{0x3074,0x64},
	{0x3081,0x00},
	{0x308C,0x00},
	{0x308D,0x01},
	{0x3094,0x00},
	{0x3095,0x00},
	{0x3096,0x00},
	{0x3097,0x00},
	{0x309C,0x00},
	{0x309D,0x00},
	{0x30A4,0xAA},
	{0x30A6,0x00},
	{0x30CC,0x00},
	{0x30CD,0x00},
	{0x30D5,0x04},
	{0x30DC,0x32},
	{0x30DD,0x00},
	{0x3400,0x01},
	{0x3460,0x21},
	{0x3478,0xA1},
	{0x347C,0x01},
	{0x3480,0x01},
	{0x36D0,0x00},
	{0x36D1,0x10},
	{0x36D4,0x00},
	{0x36D5,0x10},
	{0x36E2,0x00},
	{0x36E4,0x00},
	{0x36E5,0x00},
	{0x36E6,0x00},
	{0x36E8,0x00},
	{0x36E9,0x00},
	{0x36EA,0x00},
	{0x36EC,0x00},
	{0x36EE,0x00},
	{0x36EF,0x00},
	{0x3930,0x66},
	{0x3931,0x00},
	{0x3A4C,0x39},
	{0x3A4D,0x01},
	{0x3A4E,0x14},
	{0x3A50,0x48},
	{0x3A51,0x01},
	{0x3A52,0x14},
	{0x3A56,0x00},
	{0x3A5A,0x00},
	{0x3A5E,0x00},
	{0x3A62,0x00},
	{0x3A6A,0x20},
	{0x3A6C,0x42},
	{0x3A6E,0xA0},
	{0x3B2C,0x0C},
	{0x3B30,0x1C},
	{0x3B34,0x0C},
	{0x3B38,0x1C},
	{0x3BA0,0x0C},
	{0x3BA4,0x1C},
	{0x3BA8,0x0C},
	{0x3BAC,0x1C},
	{0x3D3C,0x11},
	{0x3D46,0x0B},
	{0x3DE0,0x3F},
	{0x3DE1,0x08},
	{0x3E10,0x10},
	{0x3E14,0x87},
	{0x3E16,0x91},
	{0x3E18,0x91},
	{0x3E1A,0x87},
	{0x3E1C,0x78},
	{0x3E1E,0x50},
	{0x3E20,0x50},
	{0x3E22,0x50},
	{0x3E24,0x87},
	{0x3E26,0x91},
	{0x3E28,0x91},
	{0x3E2A,0x87},
	{0x3E2C,0x78},
	{0x3E2E,0x50},
	{0x3E30,0x50},
	{0x3E32,0x50},
	{0x3E34,0x87},
	{0x3E36,0x91},
	{0x3E38,0x91},
	{0x3E3A,0x87},
	{0x3E3C,0x78},
	{0x3E3E,0x50},
	{0x3E40,0x50},
	{0x3E42,0x50},
	{0x4054,0x64},
	{0x4148,0xFE},
	{0x4149,0x05},
	{0x414A,0xFF},
	{0x414B,0x05},
	{0x420A,0x03},
	{0x4231,0x18},
	{0x423D,0x9C},
	{0x4242,0xB4},
	{0x4246,0xB4},
	{0x424E,0xB4},
	{0x425C,0xB4},
	{0x425E,0xB6},
	{0x426C,0xB4},
	{0x426E,0xB6},
	{0x428C,0xB4},
	{0x428E,0xB6},
	{0x4708,0x00},
	{0x4709,0x00},
	{0x470A,0xFF},
	{0x470B,0x03},
	{0x470C,0x00},
	{0x470D,0x00},
	{0x470E,0xFF},
	{0x470F,0x03},
	{0x47EB,0x1C},
	{0x47F0,0xA6},
	{0x47F2,0xA6},
	{0x47F4,0xA0},
	{0x47F6,0x96},
	{0x4808,0xA6},
	{0x480A,0xA6},
	{0x480C,0xA0},
	{0x480E,0x96},
	{0x492C,0xB2},
	{0x4930,0x03},
	{0x4932,0x03},
	{0x4936,0x5B},
	{0x4938,0x82},
	{0x493C,0x23},
	{0x493E,0x23},
	{0x4940,0x23},
	{0x4BA8,0x1C},
	{0x4BA9,0x03},
	{0x4BAC,0x1C},
	{0x4BAD,0x1C},
	{0x4BAE,0x1C},
	{0x4BAF,0x1C},
	{0x4BB0,0x1C},
	{0x4BB1,0x1C},
	{0x4BB2,0x1C},
	{0x4BB3,0x1C},
	{0x4BB4,0x1C},
	{0x4BB8,0x03},
	{0x4BB9,0x03},
	{0x4BBA,0x03},
	{0x4BBB,0x03},
	{0x4BBC,0x03},
	{0x4BBD,0x03},
	{0x4BBE,0x03},
	{0x4BBF,0x03},
	{0x4BC0,0x03},
	{0x4C14,0x87},
	{0x4C16,0x91},
	{0x4C18,0x91},
	{0x4C1A,0x87},
	{0x4C1C,0x78},
	{0x4C1E,0x50},
	{0x4C20,0x50},
	{0x4C22,0x50},
	{0x4C24,0x87},
	{0x4C26,0x91},
	{0x4C28,0x91},
	{0x4C2A,0x87},
	{0x4C2C,0x78},
	{0x4C2E,0x50},
	{0x4C30,0x50},
	{0x4C32,0x50},
	{0x4C34,0x87},
	{0x4C36,0x91},
	{0x4C38,0x91},
	{0x4C3A,0x87},
	{0x4C3C,0x78},
	{0x4C3E,0x50},
	{0x4C40,0x50},
	{0x4C42,0x50},
	{0x4D12,0x1F},
	{0x4D13,0x1E},
	{0x4D26,0x33},
	{0x4E0E,0x59},
	{0x4E14,0x55},
	{0x4E16,0x59},
	{0x4E1E,0x3B},
	{0x4E20,0x47},
	{0x4E22,0x54},
	{0x4E26,0x81},
	{0x4E2C,0x7D},
	{0x4E2E,0x81},
	{0x4E36,0x63},
	{0x4E38,0x6F},
	{0x4E3A,0x7C},
	{0x4F3A,0x3C},
	{0x4F3C,0x46},
	{0x4F3E,0x59},
	{0x4F42,0x64},
	{0x4F44,0x6E},
	{0x4F46,0x81},
	{0x4F4A,0x82},
	{0x4F5A,0x81},
	{0x4F62,0xAA},
	{0x4F72,0xA9},
	{0x4F78,0x36},
	{0x4F7A,0x41},
	{0x4F7C,0x61},
	{0x4F7D,0x01},
	{0x4F7E,0x7C},
	{0x4F7F,0x01},
	{0x4F80,0x77},
	{0x4F82,0x7B},
	{0x4F88,0x37},
	{0x4F8A,0x40},
	{0x4F8C,0x62},
	{0x4F8D,0x01},
	{0x4F8E,0x76},
	{0x4F8F,0x01},
	{0x4F90,0x5E},
	{0x4F91,0x02},
	{0x4F92,0x69},
	{0x4F93,0x02},
	{0x4F94,0x89},
	{0x4F95,0x02},
	{0x4F96,0xA4},
	{0x4F97,0x02},
	{0x4F98,0x9F},
	{0x4F99,0x02},
	{0x4F9A,0xA3},
	{0x4F9B,0x02},
	{0x4FA0,0x5F},
	{0x4FA1,0x02},
	{0x4FA2,0x68},
	{0x4FA3,0x02},
	{0x4FA4,0x8A},
	{0x4FA5,0x02},
	{0x4FA6,0x9E},
	{0x4FA7,0x02},
	{0x519E,0x79},
	{0x51A6,0xA1},
	{0x51F0,0xAC},
	{0x51F2,0xAA},
	{0x51F4,0xA5},
	{0x51F6,0xA0},
	{0x5200,0x9B},
	{0x5202,0x91},
	{0x5204,0x87},
	{0x5206,0x82},
	{0x5208,0xAC},
	{0x520A,0xAA},
	{0x520C,0xA5},
	{0x520E,0xA0},
	{0x5210,0x9B},
	{0x5212,0x91},
	{0x5214,0x87},
	{0x5216,0x82},
	{0x5218,0xAC},
	{0x521A,0xAA},
	{0x521C,0xA5},
	{0x521E,0xA0},
	{0x5220,0x9B},
	{0x5222,0x91},
	{0x5224,0x87},
	{0x5226,0x82},
	{0x5B3C,0x7F},
	{0x3000,0x00},
	{IMX585_REG_DELAY, 0x18},//wait(24ms)
	{0x3002,0x00},
	{0x30A4,0x28},
	{IMX585_REG_END, 0x00},/* END MARKER */
};

/*
* the order of the imx585_win_sizes is [full_resolution, preview_resolution].
*/
static struct tx_isp_sensor_win_setting imx585_win_sizes[] = {
		{
				.width          = 3840,
				.height         = 2160,
				.fps            = 30 << 16 | 1,
				.mbus_code      = TISP_VI_FMT_SGBRG10_1X10,
				.colorspace     = TISP_COLORSPACE_SRGB,
				.regs           = imx585_init_regs_3840_2160_30fps_mipi,
		},
};

static struct tx_isp_sensor_win_setting *wsize = &imx585_win_sizes[0];

/*
* the part of driver was fixed.
*/

static struct regval_list imx585_stream_on_mipi[] = {
		// {0x3000, 0x00},
		{IMX585_REG_END, 0x00}, /* END MARKER */
};

static struct regval_list imx585_stream_off_mipi[] = {
		// {0x3000, 0x01},
		{IMX585_REG_END, 0x00}, /* END MARKER */
};

int imx585_read(struct tx_isp_subdev *sd, uint16_t reg,
				unsigned char *value)
{
		int ret;
		struct i2c_client *client = tx_isp_get_subdevdata(sd);
		uint8_t buf[2] = {(reg>>8)&0xff, reg&0xff};
		struct i2c_msg msg[2] = {
				[0] = {
						.addr   = client->addr,
						.flags  = 0,
						.len    = 2,
						.buf    = buf,
				},
				[1] = {
						.addr   = client->addr,
						.flags  = I2C_M_RD,
						.len    = 1,
						.buf    = value,
				}
		};

		ret = private_i2c_transfer(client->adapter, msg, 2);
		if (ret > 0)
				ret = 0;

		return ret;
}

int imx585_write(struct tx_isp_subdev *sd, uint16_t reg,
				unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg>>8)&0xff, reg&0xff, value};
	struct i2c_msg msg = {
		.addr   = client->addr,
		.flags  = 0,
		.len    = 3,
		.buf    = buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 0
static int imx585_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
		int ret;
		unsigned char val;
		while (vals->reg_num != IMX585_REG_END) {
				if (vals->reg_num == IMX585_REG_DELAY) {
						private_msleep(vals->value);
				} else {
						ret = imx585_read(sd, vals->reg_num, &val);
						if (ret < 0)
								return ret;
				}
				vals++;
		}
		return 0;
}
#endif

static int imx585_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != IMX585_REG_END) {
		if (vals->reg_num == IMX585_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = imx585_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int imx585_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int imx585_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = imx585_read(sd, 0x3b00, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
			return ret;
	if (v != IMX585_CHIP_ID_H)
			return -ENODEV;
	*ident = v;

	ret = imx585_read(sd, 0x3b06, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
			return ret;
	if (v != IMX585_CHIP_ID_L)
			return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

// static int imx585_set_integration_time_short(struct tx_isp_subdev *sd, int value)
// {
// 		int ret = 0;
// 		unsigned short shs1 = 0;
// 		int rhs1 = 473;

// 		shs1 = rhs1 - (value << 1);
// 		ret = imx585_write(sd, 0x3054, (unsigned char)(shs1 & 0xff));
// 		ret += imx585_write(sd, 0x3055, (unsigned char)((shs1 >> 8) & 0xff));
// 		ret += imx585_write(sd, 0x3056, (unsigned char)((shs1 >> 16) & 0x3));
// 		return 0;
// }

static int imx585_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned short shr = 0;
	unsigned short vmax = 0;

	if (data_type == TX_SENSOR_DATA_TYPE_LINEAR)
	{
		vmax = imx585_attr.total_height;
		shr = ((vmax - value) >> 1 ) << 1;
		if(shr <= 10) shr = 10;	//shr_min=8
		ret = imx585_write(sd, 0x3050, (unsigned char)(shr & 0xff));
		ret += imx585_write(sd, 0x3051, (unsigned char)((shr >> 8) & 0xff));
		ret += imx585_write(sd, 0x3052, (unsigned char)((shr >> 16) & 0x07));
	}
	if (ret < 0)
			return ret;

	return 0;
}

// static int imx585_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
// {
// 		int ret = 0;
// 		ret += imx585_write(sd, 0x3092, (unsigned char)(value & 0xff));
// 		ret += imx585_write(sd, 0x3093, (unsigned char)((value >> 8) & 0xff));
// 		if (ret < 0)
// 				return ret;

// 		return 0;
// }

static int imx585_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret += imx585_write(sd, 0x306c, (unsigned char)(value & 0xff));
	ret += imx585_write(sd, 0x306d, (unsigned char)((value >> 8) & 0x07));
	if (ret < 0)
			return ret;

	return 0;
}

static int imx585_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx585_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx585_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!init->enable){
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		return ISP_SUCCESS;
	} else {
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = TISP_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		sensor->video.state = TX_ISP_MODULE_DEINIT;

		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
		sensor->priv = wsize;
	}

	return 0;
}

static int imx585_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
			if(sensor->video.state == TX_ISP_MODULE_DEINIT){
					ret = imx585_write_array(sd, wsize->regs);
					if (ret)
							return ret;
					sensor->video.state = TX_ISP_MODULE_INIT;
			}
			if(sensor->video.state == TX_ISP_MODULE_INIT){
					ret = imx585_write_array(sd, imx585_stream_on_mipi);
					sensor->video.state = TX_ISP_MODULE_RUNNING;
					pr_debug("imx585 stream on\n");
			}

	} else {
			ret = imx585_write_array(sd, imx585_stream_off_mipi);
			sensor->video.state = TX_ISP_MODULE_INIT;
			pr_debug("imx585 stream off\n");
	}

	return ret;
}

static int imx585_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int max_fps;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch(sensor->info.default_boot){
	case 0:
			sclk = 1100 * 2250 * 30;
			max_fps = TX_SENSOR_MAX_FPS_30;
			break;
	default:
			ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps<< 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
			ISP_ERROR("warn: fps(%x) no in range\n", fps);
			return -1;
	}

	ret += imx585_read(sd, 0x302d, &val);
	hts = val;
	ret += imx585_read(sd, 0x302c, &val);
	hts = (hts << 8) + val;
	if (0 != ret) {
			ISP_ERROR("err: imx585 read err\n");
			return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += imx585_write(sd, 0x3001, 0x01);
	ret += imx585_write(sd, 0x302a, (unsigned char)((vts & 0xf0000) >> 16));
	ret += imx585_write(sd, 0x3029, (unsigned char)((vts & 0xff00) >> 8));
	ret += imx585_write(sd, 0x3028, (unsigned char)(vts & 0xff));
	ret += imx585_write(sd, 0x3001, 0x00);
	if (0 != ret) {
			ISP_ERROR("err: imx585_write err\n");
			return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 8;
	sensor->video.attr->integration_time_limit = vts - 8;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 8;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int imx585_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t h_val;
	uint8_t v_val;

	ret = imx585_read(sd, 0x3020, &h_val);
	ret = imx585_read(sd, 0x3021, &v_val);
	switch(enable) {
	case 0:
		h_val &= 0xFE;
		v_val &= 0xFE;
		break;
	case 1:
		h_val |= 0x01;
		v_val &= 0xFE;
		break;
	case 2:
		h_val &= 0xFE;
		v_val |= 0x01;
		break;
	case 3:
		h_val |= 0x01;
		v_val |= 0x01;
		break;
	}
	ret = imx585_write(sd, 0x3020, h_val);
	ret = imx585_write(sd, 0x3021, v_val);

	return ret;
}

#if 0
static int imx585_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
		struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
		int ret = 0;
		/* struct timeval tv; */

		/* do_gettimeofday(&tv); */
		/* pr_debug("%d:before:time is %d.%d\n", __LINE__,tv.tv_sec,tv.tv_usec); */
		ret = imx585_write(sd, 0x3000, 0x1);
		if(wdr_en == 1){
				memcpy((void*)(&(imx585_attr.mipi)),(void*)(&imx585_mipi_dol),sizeof(imx585_mipi_dol));
				data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
				wsize = &imx585_win_sizes[1];
				imx585_attr.wdr_cache = wdr_bufsize;
				imx585_attr.max_again = 404346;
				imx585_attr.max_again_short = 404346;
				imx585_attr.max_dgain = 0;
				imx585_attr.min_integration_time = 4;
				imx585_attr.min_integration_time_native = 4;
				imx585_attr.max_integration_time_native = 2181;
				imx585_attr.min_integration_time_short = 4;
				imx585_attr.max_integration_time_short = 232;
				imx585_attr.integration_time_limit = 2181;
				imx585_attr.total_width = 1022;
				imx585_attr.total_height = 2422;
				imx585_attr.max_integration_time = 2181;
				imx585_attr.integration_time_apply_delay = 2;
				imx585_attr.again_apply_delay = 2;
				imx585_attr.dgain_apply_delay = 0;
				imx585_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
				imx585_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
				sensor->video.attr = &imx585_attr;
				ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
		} else if (wdr_en == 0){
				memcpy((void*)(&(imx585_attr.mipi)),(void*)(&imx585_mipi_linear),sizeof(imx585_mipi_linear));
				data_type = TX_SENSOR_DATA_TYPE_LINEAR;
				imx585_attr.data_type = data_type;
				wsize = &imx585_win_sizes[0];

				imx585_attr.data_type = data_type;
				imx585_attr.wdr_cache = wdr_bufsize;
				imx585_attr.max_again = 404346;
				imx585_attr.max_again_short = 404346;
				imx585_attr.max_dgain = 0;
				imx585_attr.min_integration_time = 4;
				imx585_attr.min_integration_time_native = 4;
				imx585_attr.max_integration_time_native = 2485 - 8;
				imx585_attr.integration_time_limit = 2485 - 8;
				imx585_attr.total_width = 1494;
				imx585_attr.total_height = 2485;
				imx585_attr.max_integration_time = 2485 - 8;
				imx585_attr.integration_time_apply_delay = 2;
				imx585_attr.again_apply_delay = 2;
				imx585_attr.dgain_apply_delay = 0;

				sensor->video.attr = &imx585_attr;
				ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
		} else {
				ISP_ERROR("Can not support this data type!!!");
				return -1;
		}

		return 0;
}

static int imx585_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
		int ret = 0;

		private_gpio_direction_output(reset_gpio, 0);
		private_msleep(1);
		private_gpio_direction_output(reset_gpio, 1);
		private_msleep(1);

		ret = imx585_write_array(sd, wsize->regs);
		ret = imx585_write_array(sd, imx585_stream_on_mipi);

		return 0;
}
#endif

static int imx585_set_mode(struct tx_isp_subdev *sd, int value)
{
		struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
		int ret = ISP_SUCCESS;

		if(wsize){
				sensor->video.mbus.width = wsize->width;
				sensor->video.mbus.height = wsize->height;
				sensor->video.mbus.code = wsize->mbus_code;
				sensor->video.mbus.field = TISP_FIELD_NONE;
				sensor->video.mbus.colorspace = wsize->colorspace;
				sensor->video.fps = wsize->fps;
				ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
		}

		return ret;
}

struct clk *sclka;
static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = 0;

	switch(info->default_boot){
	case 0:
		wsize = &imx585_win_sizes[0];
		data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		imx585_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		imx585_attr.max_again = 589824;//589824
		// imx585_attr.max_again_short = 404346;
		imx585_attr.max_dgain = 0;
		imx585_attr.min_integration_time = 2;
		imx585_attr.min_integration_time_native = 2;
		imx585_attr.max_integration_time_native = 2250 - 8;
		imx585_attr.integration_time_limit = 2250 - 8;
		imx585_attr.total_width = 1100;
		imx585_attr.total_height = 2250;
		imx585_attr.max_integration_time = 2250 - 8;
		imx585_attr.integration_time_apply_delay = 2;
		imx585_attr.again_apply_delay = 2;
		imx585_attr.dgain_apply_delay = 0;
		memcpy((void*)(&(imx585_attr.mipi)),(void*)(&imx585_mipi_linear),sizeof(imx585_mipi_linear));
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
	}

	switch(info->video_interface){
	case TISP_SENSOR_VI_MIPI_CSI0:
	case TISP_SENSOR_VI_MIPI_CSI1:
			imx585_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			imx585_attr.mipi.index = 0;
			break;
	default:
			ISP_ERROR("Have no this interface!!!\n");
	}

	switch(info->mclk){
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

	if (IS_ERR(sensor->mclk)) {
			ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
			goto err_get_mclk;
	}
	rate = private_clk_get_rate(sensor->mclk);
	if (((rate / 1000) % 37125) != 0) {
		ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
		sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
		if (IS_ERR(sclka)) {
				pr_err("get sclka failed\n");
		} else {
				rate = private_clk_get_rate(sclka);
				if (((rate / 1000) % 37125) != 0) {
						private_clk_set_rate(sclka, 891000000);
				}
		}
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

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
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

		return 0;

err_get_mclk:
		return -1;
}

static int imx585_g_chip_ident(struct tx_isp_subdev *sd,
							struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"imx585_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(100);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(100);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"imx585_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	/* while(1) */
	ret = imx585_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an imx585 chip.\n",
				client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("imx585 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "imx585", sizeof("imx585"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int imx585_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	// struct tx_isp_initarg *init = arg;
	// return 0;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = imx585_set_integration_time(sd, sensor_val->value);
		break;
	// case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
	// 		if(arg)
	// 				ret = imx585_set_integration_time_short(sd, sensor_val->value);
	// 		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = imx585_set_analog_gain(sd, sensor_val->value);
		break;
	// case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
	// 		if(arg)
	// 				ret = imx585_set_analog_gain_short(sd, sensor_val->value);
	// 		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = imx585_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = imx585_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = imx585_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if(arg)
			ret = imx585_write_array(sd, imx585_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if(arg)
			ret = imx585_write_array(sd, imx585_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = imx585_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
        if(arg)
            ret = imx585_set_vflip(sd, sensor_val->value);
        break;
	// case TX_ISP_EVENT_SENSOR_WDR:
	//         if(arg)
	//                 ret = imx585_set_wdr(sd, init->enable);
	//         break;
	// case TX_ISP_EVENT_SENSOR_WDR_STOP:
	//         if(arg)
	//                 ret = imx585_set_wdr_stop(sd, init->enable);
	//         break;
	default:
		break;
	}

	return 0;
}

static int imx585_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = imx585_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int imx585_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
			return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
			return -EPERM;
	imx585_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops imx585_core_ops = {
	.g_chip_ident = imx585_g_chip_ident,
	.reset = imx585_reset,
	.init = imx585_init,
	.g_register = imx585_g_register,
	.s_register = imx585_s_register,
};

static struct tx_isp_subdev_video_ops imx585_video_ops = {
	.s_stream = imx585_s_stream,
};

static struct tx_isp_subdev_sensor_ops  imx585_sensor_ops = {
	.ioctl  = imx585_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops imx585_ops = {
	.core = &imx585_core_ops,
	.video = &imx585_video_ops,
	.sensor = &imx585_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "imx585",
	.id = -1,
	.dev = {
			.dma_mask = &tx_isp_module_dma_mask,
			.coherent_dma_mask = 0xffffffff,
			.platform_data = NULL,
	},
	.num_resources = 0,
};


static int imx585_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
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
	sd = &sensor->sd;
	video = &sensor->video;

	sensor->video.attr = &imx585_attr;
	imx585_attr.expo_fs = 1;
	sensor->dev = &client->dev;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	sensor->video.shvflip = 1;
	tx_isp_subdev_init(&sensor_platform_device, sd, &imx585_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->imx585\n");

	return 0;
}

static int imx585_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
			private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
			private_gpio_free(pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	tx_isp_subdev_deinit(sd);

	kfree(sensor);

	return 0;
}

static const struct i2c_device_id imx585_id[] = {
	{ "imx585", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, imx585_id);

static struct i2c_driver imx585_driver = {
	.driver = {
		.owner  = THIS_MODULE,
		.name   = "imx585",
	},
	.probe          = imx585_probe,
	.remove         = imx585_remove,
	.id_table       = imx585_id,
};

static __init int init_imx585(void)
{
		return private_i2c_add_driver(&imx585_driver);
}

static __exit void exit_imx585(void)
{
		private_i2c_del_driver(&imx585_driver);
}

module_init(init_imx585);
module_exit(exit_imx585);

MODULE_DESCRIPTION("A low-level driver for Sony imx585 sensor");
MODULE_LICENSE("GPL");
