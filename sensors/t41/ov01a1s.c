/*
 * ov01a1s.c
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

#define OV01A1S_CHIP_ID_H	(0x56)
#define OV01A1S_CHIP_ID_M	(0x01)
#define OV01A1S_CHIP_ID_L	(0x41)
#define OV01A1S_REG_END	0xffff
#define OV01A1S_REG_DELAY	0xfffe

#define OV01A1S_SUPPORT_SCLK_FPS_60 (39997440)
#define SENSOR_OUTPUT_MAX_FPS 60
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20220401a"

static int reset_gpio = GPIO_PC(28);
static int pwdn_gpio = -1;

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
struct again_lut ov01a1s_again_lut[] = {
        {0x100, 0},
        {0x110, 5731},
        {0x120, 11136},
        {0x130, 16248},
        {0x140, 21097},
        {0x150, 25710},
        {0x160, 30109},
        {0x170, 34312},
        {0x180, 38336},
        {0x190, 42195},
        {0x1a0, 45904},
        {0x1b0, 49472},
        {0x1c0, 52910},
        {0x1d0, 56228},
        {0x1e0, 59433},
        {0x1f0, 62534},
        {0x200, 65536},
        {0x220, 71267},
        {0x240, 76672},
        {0x260, 81784},
        {0x280, 86633},
        {0x2a0, 91246},
        {0x2c0, 95645},
        {0x2e0, 99848},
        {0x300, 103872},
        {0x320, 107731},
        {0x340, 111440},
        {0x360, 115008},
        {0x380, 118446},
        {0x3a0, 121764},
        {0x3c0, 124969},
        {0x3e0, 128070},
        {0x400, 131072},
        {0x440, 136803},
        {0x480, 142208},
        {0x4c0, 147320},
        {0x500, 152169},
        {0x540, 156782},
        {0x580, 161181},
        {0x5c0, 165384},
        {0x600, 169408},
        {0x640, 173267},
        {0x680, 176976},
        {0x6c0, 180544},
        {0x700, 183982},
        {0x740, 187300},
        {0x780, 190505},
        {0x7c0, 193606},
        {0x800, 196608},
        {0x880, 202339},
        {0x900, 207744},
        {0x980, 212856},
        {0xa00, 217705},
        {0xa80, 222318},
        {0xb00, 226717},
        {0xb80, 230920},
        {0xc00, 234944},
        {0xc80, 238803},
        {0xd00, 242512},
        {0xd80, 246080},
        {0xe00, 249518},
        {0xe80, 252836},
        {0xf00, 256041},
        {0xf80, 259142},
};

struct tx_isp_sensor_attribute ov01a1s_attr;

unsigned int ov01a1s_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;

	*sensor_it = expo;

	return it;
}

unsigned int ov01a1s_alloc_integration_time_short(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;

	*sensor_it = expo;

	return it;
}

unsigned int ov01a1s_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = ov01a1s_again_lut;

	while(lut->gain <= ov01a1s_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == ov01a1s_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int ov01a1s_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = ov01a1s_again_lut;

	while(lut->gain <= ov01a1s_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else{
			if((lut->gain == ov01a1s_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int ov01a1s_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute ov01a1s_attr={
	.name = "ov01a1s",
	.chip_id = 0x303030,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x36,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 960,
		/* .clk = 1300, */
		.lans = 1,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
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
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.mipi_sc.data_type_en = 0,
		.mipi_sc.data_type_value = RAW10,
		.mipi_sc.del_start = 0,
		.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
		.mipi_sc.sensor_fid_mode = 0,
		.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
	},
	.max_again = 259142,
	.max_again_short = 259142,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.min_integration_time_short = 2,
	.max_integration_time_short = 405,
	.max_integration_time_native = 0x380 - 8,
	.integration_time_limit = 0x380 - 8,
	.total_width = 0x2e8 * 2,
	.total_height = 0x380,
	.max_integration_time = 0x380 - 8,
	.integration_time_apply_delay = 2,
	.again = 0,//
        .integration_time = 0x378,//
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again_short = ov01a1s_alloc_again_short,
	.sensor_ctrl.alloc_again = ov01a1s_alloc_again,
	.sensor_ctrl.alloc_dgain = ov01a1s_alloc_dgain,
};

static struct regval_list ov01a1s_init_regs_1280_720_60fps[] = {
/* @@ Res 1280x720 crop MIPI0960Mbps Linear10 60fps */
        {0x0100, 0x00},
        {0x0103, 0x01},
        {0x0302, 0x00},
        {0x0303, 0x06},
        {0x0304, 0x01},
        {0x0305, 0xe0},
        {0x0306, 0x00},
        {0x0308, 0x01},
        {0x0309, 0x00},
        {0x030c, 0x01},
        {0x0322, 0x01},
        {0x0323, 0x06},
        {0x0324, 0x01},
        {0x0325, 0x68},
        {0x3002, 0xa1},
        {0x301e, 0xf0},
        {0x3022, 0x01},
        {0x3500, 0x00},//
        {0x3501, 0x03},
        {0x3502, 0x78},
        {0x3504, 0x0c},
        {0x3508, 0x01},
        {0x3509, 0x00},
        {0x3601, 0xc0},
        {0x3603, 0x71},
        {0x3610, 0x68},
        {0x3611, 0x86},
        {0x3640, 0x10},
        {0x3641, 0x80},
        {0x3642, 0xdc},
        {0x3646, 0x55},
        {0x3647, 0x57},
        {0x364b, 0x00},
        {0x3653, 0x10},
        {0x3655, 0x00},
        {0x3656, 0x00},
        {0x365f,  0x0f},
        {0x3661, 0x45},
        {0x3662, 0x24},
        {0x3663, 0x11},
        {0x3664, 0x07},
        {0x3709, 0x34},
        {0x370b, 0x6f},
        {0x3714, 0x22},
        {0x371b, 0x27},
        {0x371c, 0x67},
        {0x371d, 0xa7},
        {0x371e, 0xe7},
        {0x3730, 0x81},
        {0x3733, 0x10},
        {0x3734, 0x40},
        {0x3737, 0x04},
        {0x3739, 0x1c},
        {0x3767, 0x00},
        {0x376c, 0x81},
        {0x3772, 0x14},
        {0x37c2, 0x04},
        {0x37d8, 0x03},
        {0x37d9, 0x0c},
        {0x37e0, 0x00},
        {0x37e1, 0x08},
        {0x37e2, 0x10},
        {0x37e3, 0x04},
        {0x37e4, 0x04},
        {0x37e5, 0x03},
        {0x37e6, 0x04},
        {0x3800, 0x00},
        {0x3801, 0x00},
        {0x3802, 0x00},
        {0x3803, 0x28},
        {0x3804, 0x05},
        {0x3805, 0x0f},
        {0x3806, 0x03},
        {0x3807, 0x07},
        {0x3808, 0x05},
        {0x3809, 0x00},
        {0x380a, 0x02},
        {0x380b, 0xd0},
        {0x380c, 0x02},
        {0x380d, 0xe8},
        {0x380e, 0x03},
        {0x380f,  0x80},
        {0x3810, 0x00},
        {0x3811, 0x09},
        {0x3812, 0x00},
        {0x3813, 0x08},
        {0x3814, 0x01},
        {0x3815, 0x01},
        {0x3816, 0x01},
        {0x3817, 0x01},
        {0x3820, 0xa8},
        {0x3822, 0x03},
        {0x3832, 0x28},
        {0x3833, 0x10},
        {0x3b00, 0x00},
        {0x3c80, 0x00},
        {0x3c88, 0x02},
        {0x3c8c, 0x07},
        {0x3c8d, 0x40},
        {0x3cc7, 0x80},
        {0x4000, 0xc3},
        {0x4001, 0xe0},
        {0x4003, 0x40},
        {0x4008, 0x02},
        {0x4009, 0x19},
        {0x400a, 0x01},
        {0x400b, 0x6c},
        {0x4011, 0x00},
        {0x4041, 0x00},
        {0x4300, 0xff},
        {0x4301, 0x00},
        {0x4302, 0x0f},
        {0x4503, 0x00},
        {0x4601, 0x50},
        {0x4800, 0x64},
        {0x481f,  0x34},
        {0x4825, 0x33},
        {0x4837, 0x11},
        {0x4881, 0x40},
        {0x4883, 0x01},
        {0x4890, 0x00},
        {0x4901, 0x00},
        {0x4902, 0x00},
        {0x4b00, 0x2a},
        {0x4b0d, 0x00},
        {0x450a, 0x04},
        {0x450b, 0x00},
        {0x5000, 0x65},
        {0x5004, 0x00},
        {0x5080, 0x40},
        {0x4800, 0x64},
        {0x4880, 0x00},	  //; write R4880 back to 0x00 after writing R4800 for pre-ECO chip
        {0x5200, 0x18},
        {0x4837, 0x11},
        {0x0100, 0x01},
        {0x0100, 0x01},
        {0x0100, 0x01},
        {0x0100, 0x01},
        {0x0100, 0x01},
        {0x0100, 0x01},
        {0x0100, 0x01},
	{OV01A1S_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the ov01a1s_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting ov01a1s_win_sizes[] = {
	/* [0] 720p @60fps*/
	{
		.width		= 1280,
		.height		= 720,
		.fps		        = 60 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGI10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= ov01a1s_init_regs_1280_720_60fps,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &ov01a1s_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list ov01a1s_stream_on[] = {
	{0x0100, 0x01},
	{OV01A1S_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov01a1s_stream_off[] = {
	{0x0100, 0x00},
	{OV01A1S_REG_END, 0x00},	/* END MARKER */
};

int ov01a1s_read(struct tx_isp_subdev *sd, uint16_t reg,
		 unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg>>8)&0xff, reg&0xff};
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

int ov01a1s_write(struct tx_isp_subdev *sd, uint16_t reg,
		  unsigned char value)
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
static int ov01a1s_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != OV01A1S_REG_END) {
		if (vals->reg_num == OV01A1S_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = ov01a1s_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%02x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}
	return 0;
}
#endif

static int ov01a1s_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != OV01A1S_REG_END) {
		if (vals->reg_num == OV01A1S_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = ov01a1s_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int ov01a1s_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int ov01a1s_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = ov01a1s_read(sd, 0x300a, &v);
	pr_debug("-----%s: %d ret = %d,  0x300a, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != OV01A1S_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = ov01a1s_read(sd, 0x300b, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OV01A1S_CHIP_ID_M)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	ret = ov01a1s_read(sd, 0x300c, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OV01A1S_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 16) | v;
	return 0;
}

static int ov01a1s_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;

	ret += ov01a1s_write(sd, 0x3502, (unsigned char)(expo & 0xff));
	ret += ov01a1s_write(sd, 0x3501, (unsigned char)((expo >> 8) & 0xff));
	ret += ov01a1s_write(sd, 0x3500, (unsigned char)((expo >> 16) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int ov01a1s_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += ov01a1s_write(sd, 0x3509, (unsigned char)((value & 0xff)));
	ret += ov01a1s_write(sd, 0x3508, (unsigned char)((value >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int ov01a1s_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int ov01a1s_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int ov01a1s_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!init->enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	sensor->video.state = TX_ISP_MODULE_DEINIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int ov01a1s_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT){
			ret = ov01a1s_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = ov01a1s_write_array(sd, ov01a1s_stream_on);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("ov01a1s stream on\n");
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
	}
	else {
		ret = ov01a1s_write_array(sd, ov01a1s_stream_off);
		pr_debug("ov01a1s stream off\n");
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}
	return ret;
}

static int ov01a1s_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = OV01A1S_SUPPORT_SCLK_FPS_60;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = SENSOR_OUTPUT_MAX_FPS;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}

	ret += ov01a1s_read(sd, 0x380c, &val);
	hts = val<<8;
	val = 0;
	ret += ov01a1s_read(sd, 0x380d, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("err: ov01a1s read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	/* ret = ov01a1s_write(sd, 0x3208, 0x02); */
	ret += ov01a1s_write(sd, 0x380f, vts & 0xff);
	ret += ov01a1s_write(sd, 0x380e, (vts >> 8) & 0xff);
	/* ret += ov01a1s_write(sd, 0x3208, 0x12); */
	/* ret += ov01a1s_write(sd, 0x320d, 0x00); */
	/* ret += ov01a1s_write(sd, 0x3208, 0xe2); */
	if (0 != ret) {
		ISP_ERROR("err: ov01a1s_write err\n");
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

static int ov01a1s_set_mode(struct tx_isp_subdev *sd, int value)
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

static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
        struct clk *sclka;
        unsigned long rate;
        int ret = 0;

	switch(info->video_interface){
	case TISP_SENSOR_VI_MIPI_CSI0:
	case TISP_SENSOR_VI_MIPI_CSI1:
		ov01a1s_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		ov01a1s_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_DVP:
		ov01a1s_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

	rate = private_clk_get_rate(sensor->mclk);
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	if (((rate / 1000) % 24000) != 0) {
                ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
                sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
		if (IS_ERR(sclka)) {
			pr_err("get sclka failed\n");
		} else {
			rate = private_clk_get_rate(sclka);
			if (((rate / 1000) % 24000) != 0) {
				private_clk_set_rate(sclka, 1200000000);
			}
		}
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;
        sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

	return 0;

err_get_mclk:
	return -1;
}

static int ov01a1s_g_chip_ident(struct tx_isp_subdev *sd,
				struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	sensor_attr_check(sd);
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"ov01a1s_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(15);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"ov01a1s_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = ov01a1s_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an ov01a1s chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("ov01a1s chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "ov01a1s", sizeof("ov01a1s"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int ov01a1s_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = ov01a1s_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = ov01a1s_set_analog_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = ov01a1s_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = ov01a1s_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = ov01a1s_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if(arg)
			ret = ov01a1s_write_array(sd, ov01a1s_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if(arg)
			ret = ov01a1s_write_array(sd, ov01a1s_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = ov01a1s_set_fps(sd, sensor_val->value);
		break;
	default:
		break;;
	}

	return ret;
}

static int ov01a1s_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = ov01a1s_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int ov01a1s_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov01a1s_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}

static struct tx_isp_subdev_core_ops ov01a1s_core_ops = {
	.g_chip_ident = ov01a1s_g_chip_ident,
	.reset = ov01a1s_reset,
	.init = ov01a1s_init,
	/*.ioctl = ov01a1s_ops_ioctl,*/
	.g_register = ov01a1s_g_register,
	.s_register = ov01a1s_s_register,
};

static struct tx_isp_subdev_video_ops ov01a1s_video_ops = {
	.s_stream = ov01a1s_s_stream,
};

static struct tx_isp_subdev_sensor_ops	ov01a1s_sensor_ops = {
	.ioctl	= ov01a1s_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops ov01a1s_ops = {
	.core = &ov01a1s_core_ops,
	.video = &ov01a1s_video_ops,
	.sensor = &ov01a1s_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "ov01a1s",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int ov01a1s_probe(struct i2c_client *client,
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
	sensor->dev = &client->dev;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &ov01a1s_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &ov01a1s_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->ov01a1s\n");
	return 0;
}

static int ov01a1s_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	private_devm_clk_put(&client->dev, sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id ov01a1s_id[] = {
	{ "ov01a1s", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov01a1s_id);

static struct i2c_driver ov01a1s_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov01a1s",
	},
	.probe		= ov01a1s_probe,
	.remove		= ov01a1s_remove,
	.id_table	= ov01a1s_id,
};

static __init int init_ov01a1s(void)
{
	return private_i2c_add_driver(&ov01a1s_driver);
}

static __exit void exit_ov01a1s(void)
{
	private_i2c_del_driver(&ov01a1s_driver);
}

module_init(init_ov01a1s);
module_exit(exit_ov01a1s);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov01a1s sensors");
MODULE_LICENSE("GPL");
