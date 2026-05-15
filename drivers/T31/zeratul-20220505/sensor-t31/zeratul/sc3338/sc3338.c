/*
 * sc3338.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#define SC3338_CHIP_ID_H	(0xcc)
#define SC3338_CHIP_ID_L	(0x41)
#define sc3338_REG_END		0xffff
#define sc3338_REG_DELAY	0xfffe
#define sc3338_SUPPORT_30FPS_SCLK (102000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20210427a"

#define SENSOR_WITHOUT_INIT

static int reset_gpio = GPIO_PA(18);
//module_param(reset_gpio, int, S_IRUGO);
//MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
//module_param(pwdn_gpio, int, S_IRUGO);
//MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
//module_param(data_interface, int, S_IRUGO);
//MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_30;
//module_param(sensor_max_fps, int, S_IRUGO);
//MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

static int shvflip = 0;
//module_param(shvflip, int, S_IRUGO);
//MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

extern int riscv_is_pass;
static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
int fast_wdr_mode = 0;

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

struct again_lut sc3338_again_lut[] = {
	{0x0080, 0},
	{0x0084, 2886},
	{0x0088, 5776},
	{0x008c, 8494},
	{0x0090, 11136},
	{0x0094, 13706},
	{0x0098, 16287},
	{0x009c, 18723},
	{0x00a0, 21097},
	{0x00a4, 23414},
	{0x00a8, 25746},
	{0x00ac, 27953},
	{0x00b0, 30109},
	{0x00b4, 32217},
	{0x00b8, 34345},
	{0x00bc, 36361},
	{0x00c0, 38336},
	{0x4080, 39588},
	{0x4084, 42467},
	{0x4088, 45378},
	{0x408c, 48089},
	{0x4090, 50724},
	{0x4094, 53288},
	{0x4098, 55888},
	{0x409c, 58318},
	{0x40a0, 60686},
	{0x40a4, 62996},
	{0x40a8, 65346},
	{0x40ac, 67547},
	{0x40b0, 69697},
	{0x40b4, 71800},
	{0x40b8, 73943},
	{0x40bc, 75955},
	{0x40c0, 77924},
	{0x40c4, 79853},
	{0x40c8, 81823},
	{0x40cc, 83676},
	{0x40d0, 85492},
	{0x40d4, 87274},
	{0x40d8, 89097},
	{0x40dc, 90813},
	{0x40e0, 92499},
	{0x40e4, 94155},
	{0x40e8, 95851},
	{0x40ec, 97450},
	{0x40f0, 99022},
	{0x40f4, 100568},
	{0x40f8, 102154},
	{0x40fc, 103651},
	{0x4880, 105124},
	{0x4884, 108003},
	{0x4888, 110914},
	{0x488c, 113625},
	{0x4890, 116260},
	{0x4894, 118824},
	{0x4898, 121424},
	{0x489c, 123854},
	{0x48a0, 126222},
	{0x48a4, 128532},
	{0x48a8, 130882},
	{0x48ac, 133083},
	{0x48b0, 135233},
	{0x48b4, 137336},
	{0x48b8, 139479},
	{0x48bc, 141491},
	{0x48c0, 143460},
	{0x48c4, 145389},
	{0x48c8, 147359},
	{0x48cc, 149212},
	{0x48d0, 151028},
	{0x48d4, 152810},
	{0x48d8, 154633},
	{0x48dc, 156349},
	{0x48e0, 158035},
	{0x48e4, 159691},
	{0x48e8, 161387},
	{0x48ec, 162986},
	{0x48f0, 164558},
	{0x48f4, 166104},
	{0x48f8, 167690},
	{0x48fc, 169187},
	{0x4980, 170660},
	{0x4984, 173539},
	{0x4988, 176436},
	{0x498c, 179161},
	{0x4990, 181796},
	{0x4994, 184360},
	{0x4998, 186947},
	{0x499c, 189390},
	{0x49a0, 191758},
	{0x49a4, 194068},
	{0x49a8, 196406},
	{0x49ac, 198619},
	{0x49b0, 200769},
	{0x49b4, 202872},
	{0x49b8, 205005},
	{0x49bc, 207027},
	{0x49c0, 208996},
	{0x49c4, 210925},
	{0x49c8, 212886},
	{0x49cc, 214748},
	{0x49d0, 216564},
	{0x49d4, 218346},
	{0x49d8, 220160},
	{0x49dc, 221885},
	{0x49e0, 223571},
	{0x49e4, 225227},
	{0x49e8, 226914},
	{0x49ec, 228522},
	{0x49f0, 230094},
	{0x49f4, 231640},
	{0x49f8, 233218},
	{0x49fc, 234723},
	{0x4b80, 236196},
	{0x4b84, 239083},
	{0x4b88, 241972},
	{0x4b8c, 244690},
	{0x4b90, 247332},
	{0x4b94, 249902},
	{0x4b98, 252483},
	{0x4b9c, 254919},
	{0x4ba0, 257294},
	{0x4ba4, 259610},
	{0x4ba8, 261942},
	{0x4bac, 264149},
	{0x4bb0, 266305},
	{0x4bb4, 268413},
	{0x4bb8, 270541},
	{0x4bbc, 272557},
	{0x4bc0, 274532},
	{0x4bc4, 276466},
	{0x4bc8, 278422},
	{0x4bcc, 280279},
	{0x4bd0, 282100},
	{0x4bd4, 283887},
	{0x4bd8, 285696},
	{0x4bdc, 287417},
	{0x4be0, 289107},
	{0x4be4, 290767},
	{0x4be8, 292450},
	{0x4bec, 294053},
	{0x4bf0, 295630},
	{0x4bf4, 297180},
	{0x4bf8, 298754},
	{0x4bfc, 300255},

};

struct tx_isp_sensor_attribute sc3338_attr;

unsigned int sc3338_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc3338_again_lut;

	while(lut->gain <= sc3338_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == sc3338_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}
		lut++;
	}

	return isp_gain;
}

unsigned int sc3338_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sc3338_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 510,
	.lans = 2,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.image_twidth = 2304,
	.image_theight = 1296,
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
};

struct tx_isp_sensor_attribute sc3338_attr={
	.name = "sc3338",
	.chip_id = 0xcc41,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
		.dvp_hcomp_en = 0,
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 300255,
	.max_dgain = 0,
	.min_integration_time = 0,
	.min_integration_time_native = 0,
	.max_integration_time_native = 1360 - 10,
	.integration_time_limit = 1360 - 10,
	.total_width = 0x4e2 * 2,
	.total_height = 1360,
	.max_integration_time = 1360 - 10,
	.one_line_expr_in_us = 25,
	.expo_fs = 1,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc3338_alloc_again,
	.sensor_ctrl.alloc_dgain = sc3338_alloc_dgain,
};

static struct regval_list sc3338_init_regs_2304_1296_30fps_mipi[] = {
	{0x0103,0x01},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x02},
	{0x320e,0x05},/*vts = 0x550 = 1360*/
	{0x320f,0x50},//
	{0x3253,0x10},
	{0x325f,0x20},
	{0x3301,0x04},
	{0x3306,0x50},
	{0x330a,0x00},
	{0x330b,0xd8},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x335e,0x06},
	{0x335f,0x0a},
	{0x3364,0x5e},
	{0x337c,0x02},
	{0x337d,0x0e},
	{0x3390,0x01},
	{0x3391,0x03},
	{0x3392,0x07},
	{0x3393,0x04},
	{0x3394,0x04},
	{0x3395,0x04},
	{0x3396,0x08},
	{0x3397,0x0b},
	{0x3398,0x1f},
	{0x3399,0x04},
	{0x339a,0x0a},
	{0x339b,0x3a},
	{0x339c,0xb0},
	{0x33a2,0x04},
	{0x33ac,0x08},
	{0x33ad,0x1c},
	{0x33ae,0x10},
	{0x33af,0x30},
	{0x33b1,0x80},
	{0x33b3,0x48},
	{0x33f9,0x60},
	{0x33fb,0x74},
	{0x33fc,0x4b},
	{0x33fd,0x5f},
	{0x349f,0x03},
	{0x34a6,0x4b},
	{0x34a7,0x5f},
	{0x34a8,0x20},
	{0x34a9,0x18},
	{0x34ab,0xe8},
	{0x34ad,0xf8},
	{0x34f8,0x5f},
	{0x34f9,0x18},
	{0x3630,0xc0},
	{0x3631,0x84},
	{0x3633,0x32},
	{0x363b,0x26},
	{0x363c,0x08},
	{0x3641,0x38},
	{0x3670,0x4e},
	{0x3674,0xc0},
	{0x3675,0xc0},
	{0x3676,0xc0},
	{0x3677,0x84},
	{0x3678,0x8a},
	{0x3679,0x8c},
	{0x367c,0x48},
	{0x367d,0x49},
	{0x367e,0x4b},
	{0x367f,0x5f},
	{0x3690,0x33},
	{0x3691,0x43},
	{0x3692,0x54},
	{0x369c,0x4b},
	{0x369d,0x5f},
	{0x36b0,0x87},
	{0x36b1,0x90},
	{0x36b2,0xa1},
	{0x36b3,0xc2},
	{0x36b4,0x49},
	{0x36b5,0x4b},
	{0x36b6,0x4f},
	{0x36ea,0x11},
	{0x36eb,0x0d},
	{0x36ec,0x1c},
	{0x36ed,0x26},
	{0x370f,0x01},
	{0x3722,0x09},
	{0x3724,0x41},
	{0x3725,0xc1},
	{0x3771,0x09},
	{0x3772,0x09},
	{0x3773,0x05},
	{0x377a,0x48},
	{0x377b,0x5f},
	{0x37fa,0x11},
	{0x37fb,0x33},
	{0x37fc,0x11},
	{0x37fd,0x08},
	{0x3905,0x8c},
	{0x391d,0x04},
	{0x3921,0x20},
	{0x3926,0x21},
	{0x3933,0x80},
	{0x3934,0x03},
	{0x3935,0x00},
	{0x3936,0x12},
	{0x3937,0x77},
	{0x3938,0x74},
	{0x39dc,0x02},
	{0x3e01,0x54},
	{0x3e02,0x80},
	{0x3e09,0x00},
	{0x4509,0x20},
	{0x5ae0,0xfe},
	{0x5ae1,0x40},
	{0x5ae2,0x38},
	{0x5ae3,0x30},
	{0x5ae4,0x28},
	{0x5ae5,0x38},
	{0x5ae6,0x30},
	{0x5ae7,0x28},
	{0x5ae8,0x3f},
	{0x5ae9,0x34},
	{0x5aea,0x2c},
	{0x5aeb,0x3f},
	{0x5aec,0x34},
	{0x5aed,0x2c},
	{0x36e9,0x54},
	{0x37f9,0x47},
	{0x0100,0x01},

	{sc3338_REG_END, 0x00},	/* END MARKER */
};

static struct tx_isp_sensor_win_setting sc3338_win_sizes[] = {
	/* resolution 2304*1296 @30fps max*/
	{
		.width		= 2304,
		.height		= 1296,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc3338_init_regs_2304_1296_30fps_mipi,
	}
};
struct tx_isp_sensor_win_setting *wsize = &sc3338_win_sizes[0];

static struct regval_list sc3338_stream_on_mipi[] = {
	{0x0100, 0x01},
	{sc3338_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc3338_stream_off_mipi[] = {
	{0x0100, 0x00},
	{sc3338_REG_END, 0x00},	/* END MARKER */
};

int sc3338_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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

int sc3338_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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

static int sc3338_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != sc3338_REG_END) {
		if (vals->reg_num == sc3338_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc3338_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
static int sc3338_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != sc3338_REG_END) {
		if (vals->reg_num == sc3338_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc3338_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc3338_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sc3338_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sc3338_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC3338_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc3338_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC3338_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc3338_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff);
	int gain = (value & 0xffff0000) >> 16;
	
	//integration time
	ret = sc3338_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc3338_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc3338_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));

	//sensor analog gain 
	ret += sc3338_write(sd, 0x3e09, (unsigned char)(((gain >> 8) & 0xff)));

	//sensor dig fine gain 
	ret += sc3338_write(sd, 0x3e07, (unsigned char)(gain & 0xff));
}

#if 0
static int sc3338_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 1;
	ret += sc3338_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc3338_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc3338_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));
	if (ret < 0)
		return ret;

	return 0;
}

static int sc3338_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	gain_val = value;

	ret += sc3338_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sc3338_write(sd, 0x3e08, (unsigned char)(((value >> 8) & 0xff)));
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int sc3338_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc3338_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc3338_init(struct tx_isp_subdev *sd, int enable)
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
#ifndef SENSOR_WITHOUT_INIT
	ret = sc3338_write_array(sd, wsize->regs);
	if (ret)
		return ret;
#endif
	if(!riscv_is_pass){
		ret = sc3338_write_array(sd,wsize->regs);
		if(ret)
			return ret;
	}
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc3338_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = sc3338_write_array(sd, sc3338_stream_on_mipi);
		ISP_WARNING("sc3338 stream on\n");

	} else {
		ret = sc3338_write_array(sd, sc3338_stream_off_mipi);
		ISP_WARNING("sc3338 stream off\n");
	}

	return ret;
}

static int sc3338_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;
	unsigned int max_fps = 0;

	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch(sensor_max_fps){
	case TX_SENSOR_MAX_FPS_30:
		sclk = sc3338_SUPPORT_30FPS_SCLK;
		max_fps = TX_SENSOR_MAX_FPS_30;
		break;
	default:
		ISP_ERROR("do not support max framerate %d in mipi mode\n",sensor_max_fps);
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(0x%x) no in range\n", fps);
		return -1;
	}

	ret = sc3338_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sc3338_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("err: sc3338 read err\n");
		return ret;
	}
	hts = ((hts << 8) + tmp) << 1;
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += sc3338_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc3338_write(sd, 0x320e, (unsigned char)(vts >> 8));

	if (0 != ret) {
		ISP_ERROR("err: sc3338_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 10;
	sensor->video.attr->integration_time_limit = vts - 10;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 10;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sc3338_set_mode(struct tx_isp_subdev *sd, int value)
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

static int sc3338_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
#ifndef SENSOR_WITHOUT_INIT
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc3338_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(50);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(35);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(35);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"sc3338_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(150);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc3338_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc3338 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
#else
	ident = 0xcc41;
#endif
	ISP_WARNING("sc3338 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc3338", sizeof("sc3338"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int sc3338_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = -1;
	unsigned char val = 0x0;

	ret = sc3338_read(sd, 0x3221, &val);
	if(enable & 0x2)
		val |= 0x60;
	else
		val &= 0x9f;
	ret += sc3338_write(sd, 0x3221, val);
	if(0 != ret)
		return ret;

	return 0;
}

static int sc3338_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
//	return 0;
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = sc3338_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
//		if(arg)
//			ret = sc3338_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
//		if(arg)
//			ret = sc3338_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc3338_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc3338_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc3338_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = sc3338_write_array(sd, sc3338_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sc3338_write_array(sd, sc3338_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc3338_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = sc3338_set_vflip(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int sc3338_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc3338_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc3338_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc3338_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc3338_core_ops = {
	.g_chip_ident = sc3338_g_chip_ident,
	.reset = sc3338_reset,
	.init = sc3338_init,
	/*.ioctl = sc3338_ops_ioctl,*/
	.g_register = sc3338_g_register,
	.s_register = sc3338_s_register,
};

static struct tx_isp_subdev_video_ops sc3338_video_ops = {
	.s_stream = sc3338_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc3338_sensor_ops = {
	.ioctl	= sc3338_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc3338_ops = {
	.core = &sc3338_core_ops,
	.video = &sc3338_video_ops,
	.sensor = &sc3338_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc3338",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc3338_probe(struct i2c_client *client, const struct i2c_device_id *id)
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
#ifndef SENSOR_WITHOUT_INIT
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);
#endif
	switch(sensor_max_fps){
	case TX_SENSOR_MAX_FPS_30:
		wsize = &sc3338_win_sizes[0];
		memcpy((void*)(&(sc3338_attr.mipi)),(void*)(&sc3338_mipi),sizeof(sc3338_mipi));
		break;
	default:
		ISP_ERROR("do not support max framerate %d in mipi mode\n",sensor_max_fps);
	}

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sc3338_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc3338_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc3338\n");

	return 0;
err_set_sensor_data_interface:
err_set_sensor_gpio:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int sc3338_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc3338_id[] = {
	{ "sc3338", 0 },
	{ }
};
//MODULE_DEVICE_TABLE(i2c, sc3338_id);

static struct i2c_driver sc3338_driver = {
	.driver = {
		.owner	= NULL,//THIS_MODULE,
		.name	= "sc3338",
	},
	.probe		= sc3338_probe,
	.remove		= sc3338_remove,
	.id_table	= sc3338_id,
};

char * get_sensor_name(void)
{
	return "sc3338";
}

int get_sensor_i2c_addr(void)
{
	return 0x30;
}

int get_sensor_width()
{
	return sc3338_win_sizes->width;
}

int get_sensor_height()
{
	return sc3338_win_sizes->height;
}

int init_sensor(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init sc3338 dirver.\n");
		return -1;
	}

	if(data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) {
		fast_wdr_mode = 1;
	} else {
		fast_wdr_mode = 0;
	}

	return private_i2c_add_driver(&sc3338_driver);
}

void exit_sensor(void)
{
	private_i2c_del_driver(&sc3338_driver);
}

//module_init(init_sc3338);
//module_exit(exit_sc3338);

//MODULE_DESCRIPTION("A low-level driver for SmartSens sc3338 sensors");
//MODULE_LICENSE("GPL");
