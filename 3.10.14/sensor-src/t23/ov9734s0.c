/*ov9734.c
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

#define OV9734_CHIP_ID_H	(0x97)
#define OV9734_CHIP_ID_L	(0x34)
#define OV9734_REG_END		0xffff
#define OV9734_REG_DELAY	0xfffe
#define OV9734_SUPPORT_PCLK_FPS_30 (1478 * 810 * 30)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20240219a"

static int reset_gpio = GPIO_PA(18);
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

struct again_lut ov9734_again_lut[] = {
	{0x10, 0},
	{0x11, 5731},
	{0x12, 11136},
	{0x13, 16248},
	{0x14, 21097},
	{0x15, 25710},
	{0x16, 30109},
	{0x17, 34312},
	{0x18, 38336},
	{0x19, 42195},
	{0x1a, 45904},
	{0x1b, 49472},
	{0x1c, 52910},
	{0x1d, 56228},
	{0x1e, 59433},
	{0x1f, 62534},
	{0x20, 65536},
	{0x22, 71267},
	{0x24, 76672},
	{0x26, 81784},
	{0x28, 86633},
	{0x2a, 91246},
	{0x2c, 95645},
	{0x2e, 99848},
	{0x30, 103872},
	{0x32, 107731},
	{0x34, 111440},
	{0x36, 115008},
	{0x38, 118446},
	{0x3a, 121764},
	{0x3c, 124969},
	{0x3e, 128070},
	{0x40, 131072},
	{0x44, 136803},
	{0x48, 142208},
	{0x4c, 147320},
	{0x50, 152169},
	{0x54, 156782},
	{0x58, 161181},
	{0x5c, 165384},
	{0x60, 169408},
	{0x64, 173267},
	{0x68, 176976},
	{0x6c, 180544},
	{0x70, 183982},
	{0x74, 187300},
	{0x78, 190505},
	{0x7c, 193606},
	{0x80, 196608},
	{0x88, 202339},
	{0x90, 207744},
	{0x98, 212856},
	{0xa0, 217705},
	{0xa8, 222318},
	{0xb0, 226717},
	{0xb8, 230920},
	{0xc0, 234944},
	{0xc8, 238803},
	{0xd0, 242512},
	{0xd8, 246080},
	{0xe0, 249518},
	{0xe8, 252836},
	{0xf0, 256041},
	{0xf8, 259142},
};

struct tx_isp_sensor_attribute ov9734_attr;

unsigned int ov9734_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = ov9734_again_lut;
	while(lut->gain <= ov9734_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == ov9734_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int ov9734_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute ov9734_attr={
	.name = "ov9734",
	.chip_id = 0x9734,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 360,
		.lans = 1,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 1280,
		.image_theight = 720,
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
	.max_again = 259142,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 810 - 4,
	.integration_time_limit = 810 - 4,
	.total_width = 1478,
	.total_height = 810,
	.max_integration_time = 810 - 4,
	.one_line_expr_in_us = 29,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = ov9734_alloc_again,
	.sensor_ctrl.alloc_dgain = ov9734_alloc_dgain,
	.fsync_attr = {
		.mode = TX_ISP_SENSOR_FSYNC_MODE_MS_REALTIME_MISPLACE,
		.call_times = 1,
		.sdelay = 100,
	}
};

static struct regval_list ov9734_init_regs_1920_1080_30fps_mipi[] = {
        {0x0103,0x01},
        {0x0100,0x00},
        {0x3001,0x00},
        {0x3002,0x00},
        {0x3007,0x00},
        {0x3010,0x00},
        {0x3011,0x08},
        {0x3014,0x22},
        {0x301e,0x15},
        {0x3030,0x19},
        {0x3080,0x02},
        {0x3081,0x3c},
        {0x3082,0x04},
        {0x3083,0x00},
        {0x3084,0x02},
        {0x3085,0x01},
        {0x3086,0x01},
        {0x3089,0x01},
        {0x308a,0x00},
        {0x3103,0x01},
        {0x3600,0x55},
        {0x3601,0x02},
        {0x3605,0x22},
        {0x3611,0xe7},
        {0x3654,0x10},
        {0x3655,0x77},
        {0x3656,0x77},
        {0x3657,0x07},
        {0x3658,0x22},
        {0x3659,0x22},
        {0x365a,0x02},
        {0x3784,0x05},
        {0x3785,0x55},
        {0x37c0,0x07},
        {0x3800,0x00},
        {0x3801,0x04},
        {0x3802,0x00},
        {0x3803,0x04},
        {0x3804,0x05},
        {0x3805,0x0b},
        {0x3806,0x02},
        {0x3807,0xdb},
        {0x3808,0x05},
        {0x3809,0x00},
        {0x380a,0x02},
        {0x380b,0xd0},
        {0x380c,0x05},//hts = 0x5c6 = 1478
        {0x380d,0xc6},//
        {0x380e,0x03},//vts = 0x32a = 810
        {0x380f,0x2a},//
        {0x3810,0x00},
        {0x3811,0x04},
        {0x3812,0x00},
        {0x3813,0x04},
        {0x3816,0x00},
        {0x3817,0x00},
        {0x3818,0x00},
        {0x3819,0x04},
        {0x3820,0x18},
        {0x3821,0x00},
        {0x382c,0x06},
        {0x3500,0x00},
        {0x3501,0x31},
        {0x3502,0x00},
        {0x3503,0x03},
        {0x3504,0x00},
        {0x3505,0x00},
        {0x3509,0x10},
        {0x350a,0x00},
        {0x350b,0x40},
        {0x3d00,0x00},
        {0x3d01,0x00},
        {0x3d02,0x00},
        {0x3d03,0x00},
        {0x3d04,0x00},
        {0x3d05,0x00},
        {0x3d06,0x00},
        {0x3d07,0x00},
        {0x3d08,0x00},
        {0x3d09,0x00},
        {0x3d0a,0x00},
        {0x3d0b,0x00},
        {0x3d0c,0x00},
        {0x3d0d,0x00},
        {0x3d0e,0x00},
        {0x3d0f,0x00},
        {0x3d80,0x00},
        {0x3d81,0x00},
        {0x3d82,0x38},
        {0x3d83,0xa4},
        {0x3d84,0x00},
        {0x3d85,0x00},
        {0x3d86,0x1f},
        {0x3d87,0x03},
        {0x3d8b,0x00},
        {0x3d8f,0x00},
        {0x4001,0xe0},
        {0x4009,0x0b},
        {0x4300,0x03},
        {0x4301,0xff},
        {0x4304,0x00},
        {0x4305,0x00},
        {0x4309,0x00},
        {0x4600,0x00},
        {0x4601,0x80},
        {0x4800,0x00},
        {0x4805,0x00},
        {0x4821,0x50},
        {0x4823,0x50},
        {0x4837,0x2d},
        {0x4a00,0x00},
        {0x4f00,0x80},
        {0x4f01,0x10},
        {0x4f02,0x00},
        {0x4f03,0x00},
        {0x4f04,0x00},
        {0x4f05,0x00},
        {0x4f06,0x00},
        {0x4f07,0x00},
        {0x4f08,0x00},
        {0x4f09,0x00},
        {0x5000,0x2f},
        {0x500c,0x00},
        {0x500d,0x00},
        {0x500e,0x00},
        {0x500f,0x00},
        {0x5010,0x00},
        {0x5011,0x00},
        {0x5012,0x00},
        {0x5013,0x00},
        {0x5014,0x00},
        {0x5015,0x00},
        {0x5016,0x00},
        {0x5017,0x00},
        {0x5080,0x00},
        {0x5180,0x01},
        {0x5181,0x00},
        {0x5182,0x01},
        {0x5183,0x00},
        {0x5184,0x01},
        {0x5185,0x00},
        {0x5708,0x06},
        {0x5780,0x3e},
        {0x5781,0x0f},
        {0x5782,0x44},
        {0x5783,0x02},
        {0x5784,0x01},
        {0x5785,0x01},
        {0x5786,0x00},
        {0x5787,0x04},
        {0x5788,0x02},
        {0x5789,0x0f},
        {0x578a,0xfd},
        {0x578b,0xf5},
        {0x578c,0xf5},
        {0x578d,0x03},
        {0x578e,0x08},
        {0x578f,0x0c},
        {0x5790,0x08},
        {0x5791,0x04},
        {0x5792,0x00},
        {0x5793,0x52},
        {0x5794,0xa3},
        {0x5000,0x3f},
        {0x3007,0x1f},
        {0x3008,0xff},
        {0x3014,0x22},
        {0x3081,0x3c},
        {0x3084,0x00},
        {0x3600,0xf6},
        {0x3601,0x72},
        {0x3610,0x0c},
        {0x3611,0xf0},
        {0x3612,0x35},
        {0x3658,0x22},
        {0x3659,0x22},
        {0x365a,0x02},
        {0x3700,0x1f},
        {0x3701,0x10},
        {0x3702,0x0c},
        {0x3703,0x07},
        {0x3704,0x3c},
        {0x3705,0x81},
        {0x3710,0x0c},
        {0x3782,0x58},
        {0x3501,0x31},
        {0x350a,0x00},
        {0x350b,0x10},
        {0x3d82,0x38},
        {0x3d83,0xa4},
        {0x3d86,0x1f},
        {0x3d87,0x03},
        {0x4001,0xe0},
        {0x4006,0x01},
        {0x4007,0x40},
        {0x4800,0x00},
        {0x4805,0x00},
        {0x4802,0x84},
        {0x4819,0x30},
        {0x4825,0x32},
        {0x4826,0x08},
        {0x4827,0x1d},
        {0x4821,0x50},
        {0x4823,0x50},
        {0x4837,0x37},
        {0x5000,0x0f},
        {0x5708,0x06},
        {0x5781,0x00},
        {0x5783,0x0f},
        {0x3703,0x0b},
        {0x3705,0x51},
        //{0x3001,0x01},
        //{0x3007,0x10},
        //{0x3816,0x00},
        //{0x3817,0x00},
        //{0x3818,0x00},
        //{0x3819,0x01},
        //{0x381c,0x01},
        {0x0100,0x01},
 	{OV9734_REG_DELAY,0x10},
	{OV9734_REG_END, 0x00},	/* END MARKER */
};

static struct tx_isp_sensor_win_setting ov9734_win_sizes[] = {
	{
		.width		= 1280,
		.height		= 720,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ov9734_init_regs_1920_1080_30fps_mipi,
	},

};
struct tx_isp_sensor_win_setting *wsize = &ov9734_win_sizes[0];

static struct regval_list ov9734_stream_on[] = {
	{0x0100, 0x01},
	{OV9734_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov9734_stream_off[] = {
	{0x0100, 0x00},
	{OV9734_REG_END, 0x00},	/* END MARKER */
};

int ov9734_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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

int ov9734_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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
static int ov9734_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != OV9734_REG_END) {
		if (vals->reg_num == OV9734_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = ov9734_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int ov9734_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != OV9734_REG_END) {
		if (vals->reg_num == OV9734_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = ov9734_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int ov9734_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int ov9734_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = ov9734_read(sd, 0x300a, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OV9734_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = ov9734_read(sd, 0x300b, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OV9734_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int ov9734_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	ret += ov9734_write(sd, 0x3500, (unsigned char)((it >> 12) & 0x0f));
	ret += ov9734_write(sd, 0x3501, (unsigned char)((it >> 4) & 0xff));
	ret += ov9734_write(sd, 0x3502, (unsigned char)((it & 0x0f) << 4));
	ret += ov9734_write(sd, 0x350a, (unsigned char)((again >> 8) & 0x03));
	ret += ov9734_write(sd, 0x350b, (unsigned char)(again & 0xff));

	if (ret != 0) {
		ISP_ERROR("err: ov9734 write err %d\n",__LINE__);
		return ret;
	}

	return 0;
}

#if 0
static int ov9734_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += ov9734_write(sd, 0x3500, (unsigned char)((value >> 12) & 0x0f));
	ret += ov9734_write(sd, 0x3501, (unsigned char)((value >> 4) & 0xff));
	ret += ov9734_write(sd, 0x3502, (unsigned char)((value & 0x0f) << 4));
	if (ret < 0)
		return ret;

	return 0;
}

static int ov9734_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += ov9734_write(sd, 0x350a, (unsigned char)((value >> 8) & 0x03));
	ret += ov9734_write(sd, 0x350b, (unsigned char)(value & 0xff));
	return 0;
}
#endif

static int ov9734_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int ov9734_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int ov9734_init(struct tx_isp_subdev *sd, int enable)
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
	ret = ov9734_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int ov9734_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = ov9734_write_array(sd, ov9734_stream_on);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("ov9734 stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = ov9734_write_array(sd, ov9734_stream_off);
		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("ov9734 stream off\n");
	}

	return ret;
}

static int ov9734_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;
	unsigned int max_fps = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	sclk = OV9734_SUPPORT_PCLK_FPS_30;
	max_fps = TX_SENSOR_MAX_FPS_30;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	ret = ov9734_read(sd, 0x380c, &tmp);
	hts = tmp;
	ret += ov9734_read(sd, 0x380d, &tmp);
	hts = ((hts << 8) | tmp);
	if (0 != ret) {
		ISP_ERROR("err: ov9734 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += ov9734_write(sd, 0x380f, (unsigned char)(vts & 0xff));
	ret += ov9734_write(sd, 0x380e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: ov9734_write err\n");
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

static int ov9734_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	val = ov9734_read(sd, 0x3820, &val);
	switch(enable) {
	case 0:
		val &= 0xF3;
		break;
	case 1:
		val = ((val & 0xF3) | 0x08);
		break;
	case 2:
		val = ((val & 0xF3) | 0x04);
		break;
	case 3:
		val |= 0x0C;
		break;
	}
	ov9734_write(sd, 0x3820, val);
	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int ov9734_set_mode(struct tx_isp_subdev *sd, int value)
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

static int ov9734_g_chip_ident(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"ov9734_reset");
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
		ret = private_gpio_request(pwdn_gpio,"ov9734_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = ov9734_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an ov9734 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("ov9734 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "ov9734", sizeof("ov9734"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int ov9734_fsync(struct tx_isp_subdev *sd, struct tx_isp_sensor_fsync *fsync)
{
        uint8_t val;
        uint16_t ret_val;

        if (fsync->place != TX_ISP_SENSOR_FSYNC_PLACE_STREAMON_AFTER)
                return 0;
        switch (fsync->call_index) {
        case 0:
                switch (fsync_mode) {
                case 2:
                        printk("===================>> %s %d\n", __func__, __LINE__);
                        //ov9734_read(sd, 0x380e, &val);
                        //ret_val = val << 8;
                        //ov9734_read(sd, 0x380f, &val);
                        //ret_val |= val;
                        //ret_val = ret_val + 4;
                        //ov9734_write(sd, 0x380e, (ret_val >> 8));
                        //ov9734_write(sd, 0x380f, (ret_val & 0xff));

                        ov9734_write(sd, 0x3001, 0x01);
                        ov9734_write(sd, 0x3007, 0x10);
                        ov9734_write(sd, 0x3816, 0x00);
                        ov9734_write(sd, 0x3817, 0x00);
                        ov9734_write(sd, 0x3818, 0x00);
                        ov9734_write(sd, 0x3819, 0x01);
                        ov9734_write(sd, 0x381c, 0x01);
                        break;
                case 3:
                        printk("===================>> %s %d\n", __func__, __LINE__);
                        ov9734_read(sd, 0x380e, &val);
                        ret_val = val << 8;
                        ov9734_read(sd, 0x380f, &val);
                        ret_val |= val;
                        ret_val = ret_val << 1;
                        ov9734_write(sd, 0x380e, (ret_val >> 8));
                        ov9734_write(sd, 0x380f, (ret_val & 0xff));

                        ov9734_write(sd, 0x3001, 0x01);
                        ov9734_write(sd, 0x3007, 0x10);
                        ov9734_write(sd, 0x3816, 0x06);
                        ov9734_write(sd, 0x3817, 0x54);
                        ov9734_write(sd, 0x3818, 0x06);
                        ov9734_write(sd, 0x3819, 0x55);
                        ov9734_write(sd, 0x381c, 0x01);
                        break;
                }
                break;
        }

        return 0;
}

static int ov9734_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = ov9734_set_expo(sd, *(int*)arg);
		break;
	/*
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = ov9734_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = ov9734_set_analog_gain(sd, *(int*)arg);
		break;
	*/
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = ov9734_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = ov9734_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = ov9734_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = ov9734_write_array(sd, ov9734_stream_off);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = ov9734_write_array(sd, ov9734_stream_on);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = ov9734_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = ov9734_set_vflip(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int ov9734_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = ov9734_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int ov9734_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	ov9734_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops ov9734_core_ops = {
	.g_chip_ident = ov9734_g_chip_ident,
	.reset = ov9734_reset,
	.init = ov9734_init,
	/*.ioctl = ov9734_ops_ioctl,*/
	.g_register = ov9734_g_register,
	.s_register = ov9734_s_register,
};

static struct tx_isp_subdev_video_ops ov9734_video_ops = {
	.s_stream = ov9734_s_stream,
};

static struct tx_isp_subdev_sensor_ops	ov9734_sensor_ops = {
	.ioctl	= ov9734_sensor_ops_ioctl,
        .fsync = ov9734_fsync,
};

static struct tx_isp_subdev_ops ov9734_ops = {
	.core = &ov9734_core_ops,
	.video = &ov9734_video_ops,
	.sensor = &ov9734_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "ov9734",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sensor_mclk_config(struct tx_isp_sensor *sensor, unsigned long want_rate)
{
        unsigned long rate = 0;
        struct clk *pll = NULL;
        char *plls[] = {"mpll", "sclka"};
        int psize = sizeof(plls) / sizeof(char *);
        char *ppll = plls[psize - 1];
        int ret = 0, i = 0;

        pll = clk_get_parent(sensor->mclk);
        rate = clk_get_rate(pll);
        if (rate % want_rate) {
                for (i = 0; i < psize; i++) {
                        pll = clk_get(NULL, plls[i]);
                        rate = clk_get_rate(pll);
                        if (!(rate % want_rate)) {
                                ret = clk_set_parent(sensor->mclk, pll);
                                if (ret) {
                                        ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
                                                    __func__, __LINE__, plls[i]);
                                        continue;
                                } else {
                                        break;
                                }
                        }
                }
                if (i == psize) {
                        if (!ret) {
                                pll = clk_get(NULL, ppll);
                                rate = clk_get_rate(pll);
                                if(want_rate == 37125000){
                                        if((rate >= 1188000000)) {
                                                rate = 1188000000;
                                        } else if (rate >= 891000000) {
                                                rate = 891000000;
                                        } else {
                                                ISP_ERROR("[%s %d] The %s clock setting failed !!!\n",
                                                          __func__, __LINE__, ppll);
                                                ret = -1;
                                                goto error;
                                        }
                                } else if (want_rate == 24000000 || want_rate == 27000000) {
                                        rate -= rate % want_rate;
                                } else {
                                        ret = -1;
                                        goto error;
                                }
                                ret = private_clk_set_rate(pll, rate);
                                if (ret) {
                                        ISP_WARNING("[%s %d] Failed to set %s !!!\n",
                                                    __func__, __LINE__, ppll);
                                        goto error;
                                } else {
                                        ISP_WARNING("[%s %d] !!!!!!!!!!! The %s frequency has been changed to %ld !!!\n",
                                                    __func__, __LINE__, ppll, rate);
                                }
                                ret = clk_set_parent(sensor->mclk, pll);
                                if (ret) {
                                        ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
                                                    __func__, __LINE__, ppll);
                                        goto error;
                                }
                        } else {
                                goto error;
                        }
                }
        }
        private_clk_set_rate(sensor->mclk, want_rate);
        private_clk_enable(sensor->mclk);

        rate = clk_get_rate(sensor->mclk);
        if (rate % want_rate) {
                ret = -1;
                goto error;
        }

        return ret;

error:
        ISP_ERROR("[%s %d] Unable to allocate the required MCLK %ld !!!\n",
                  __func__, __LINE__, want_rate);
        return ret;
}

uint16_t theight_tmp;
uint32_t fps_tmp;
static int ov9734_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

#ifdef CONFIG_KERNEL_4_4_94
	sensor->mclk = clk_get(NULL, "div_cim");
#else
	sensor->mclk = clk_get(NULL, "cgu_cim");
#endif
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
    sensor_mclk_config(sensor, 24000000);

        ov9734_attr.fsync_attr.mode = fsync_mode;
        if (fsync_mode == TX_ISP_SENSOR_FSYNC_MODE_MS_REALTIME_MISPLACE) {
                theight_tmp = ov9734_attr.total_height;
                fps_tmp = wsize->fps;
                ov9734_attr.total_height = ov9734_attr.total_height * 2;
                wsize->fps = (wsize->fps & 0xffff0000) | ((wsize->fps & 0xffff) * 2);
        }
        ov9734_attr.max_integration_time_native = ov9734_attr.total_height - 4;
        ov9734_attr.integration_time_limit = ov9734_attr.total_height - 4;
        ov9734_attr.max_integration_time = ov9734_attr.total_height - 4;

	ov9734_attr.max_again = 259142;
	ov9734_attr.max_dgain = 0; //ov9734_attr.max_dgain;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	ov9734_attr.expo_fs = 1;
	sensor->video.attr = &ov9734_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &ov9734_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->ov9734\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int ov9734_remove(struct i2c_client *client)
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

static const struct i2c_device_id ov9734_id[] = {
	{ "ov9734", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov9734_id);

static struct i2c_driver ov9734_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov9734",
	},
	.probe		= ov9734_probe,
	.remove		= ov9734_remove,
	.id_table	= ov9734_id,
};

static __init int init_ov9734(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init ov9734 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&ov9734_driver);
}

static __exit void exit_ov9734(void)
{
	private_i2c_del_driver(&ov9734_driver);
}

module_init(init_ov9734);
module_exit(exit_ov9734);

MODULE_DESCRIPTION("A low-level driver for SmartSens ov9734 sensors");
MODULE_LICENSE("GPL");
