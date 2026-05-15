/*
 * sc5336.c
 *
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          2880*1620       20        mipi_2lane            linear

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

#define SC5336_CHIP_ID_H	(0xce)
#define SC5336_CHIP_ID_L	(0x50)
#define SC5336_REG_END		0xffff
#define SC5336_REG_DELAY	0xfffe
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20220817a"

static int reset_gpio = GPIO_PA(18);
static int pwdn_gpio = GPIO_PA(19);

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

struct again_lut sc5336_again_lut[] = {
	{0x80,0},
	{0x84,2886},
	{0x88,5776},
	{0x8c,8494},
	{0x90,11136},
	{0x94,13706},
	{0x98,16287},
	{0x9c,18723},
	{0xa0,21097},
	{0xa4,23414},
	{0xa8,25746},
	{0xac,27953},
	{0xb0,30109},
	{0xb4,32217},
	{0xb8,34345},
	{0xbc,36361},
	{0xc0,38336},
	{0xc4,40270},
	{0xc8,42226},
	{0xcc,44082},
	{0xd0,45904},
	{0xd4,47690},
	{0xd8,49500},
	{0xdc,51220},
	{0xe0,52910},
	{0xe4,54571},
	{0xe8,56254},
	{0xec,57857},
	{0xf0,59433},
	{0xf4,60984},
	{0xf8,62558},
	{0xfc,64059},
	{0x880,65536},
	{0x884,68422},
	{0x888,71312},
	{0x88c,74030},
	{0x890,76672},
	{0x894,79242},
	{0x898,81823},
	{0x89c,84259},
	{0x8a0,86633},
	{0x8a4,88950},
	{0x8a8,91282},
	{0x8ac,93489},
	{0x8b0,95645},
	{0x8b4,97753},
	{0x8b8,99881},
	{0x8bc,101897},
	{0x8c0,103872},
	{0x8c4,105806},
	{0x8c8,107762},
	{0x8cc,109618},
	{0x8d0,111440},
	{0x8d4,113226},
	{0x8d8,115036},
	{0x8dc,116756},
	{0x8e0,118446},
	{0x8e4,120107},
	{0x8e8,121790},
	{0x8ec,123393},
	{0x8f0,124969},
	{0x8f4,126520},
	{0x8f8,128094},
	{0x8fc,129595},
	{0x980,131072},
	{0x984,133958},
	{0x988,136848},
	{0x98c,139566},
	{0x990,142208},
	{0x994,144778},
	{0x998,147359},
	{0x99c,149795},
	{0x9a0,152169},
	{0x9a4,154486},
	{0x9a8,156818},
	{0x9ac,159025},
	{0x9b0,161181},
	{0x9b4,163289},
	{0x9b8,165417},
	{0x9bc,167433},
	{0x9c0,169408},
	{0x9c4,171342},
	{0x9c8,173298},
	{0x9cc,175154},
	{0x9d0,176976},
	{0x9d4,178762},
	{0x9d8,180572},
	{0x9dc,182292},
	{0x9e0,183982},
	{0x9e4,185643},
	{0x9e8,187326},
	{0x9ec,188929},
	{0x9f0,190505},
	{0x9f4,192056},
	{0x9f8,193630},
	{0x9fc,195131},
	{0xb80,196608},
	{0xb84,199494},
	{0xb88,202384},
	{0xb8c,205102},
	{0xb90,207744},
	{0xb94,210314},
	{0xb98,212895},
	{0xb9c,215331},
	{0xba0,217705},
	{0xba4,220022},
	{0xba8,222354},
	{0xbac,224561},
	{0xbb0,226717},
	{0xbb4,228825},
	{0xbb8,230953},
	{0xbbc,232969},
	{0xbc0,234944},
	{0xbc4,236878},
	{0xbc8,238834},
	{0xbcc,240690},
	{0xbd0,242512},
	{0xbd4,244298},
	{0xbd8,246108},
	{0xbdc,247828},
	{0xbe0,249518},
	{0xbe4,251179},
	{0xbe8,252862},
	{0xbec,254465},
	{0xbf0,256041},
	{0xbf4,257592},
	{0xbf8,259166},
	{0xbfc,260667},
	{0xf80,262144},
	{0xf84,265030},
	{0xf88,267920},
	{0xf8c,270638},
	{0xf90,273280},
	{0xf94,275850},
	{0xf98,278431},
	{0xf9c,280867},
	{0xfa0,283241},
	{0xfa4,285558},
	{0xfa8,287890},
	{0xfac,290097},
	{0xfb0,292253},
	{0xfb4,294361},
	{0xfb8,296489},
	{0xfbc,298505},
	{0xfc0,300480},
	{0xfc4,302414},
	{0xfc8,304370},
	{0xfcc,306226},
	{0xfd0,308048},
	{0xfd4,309834},
	{0xfd8,311644},
	{0xfdc,313364},
	{0xfe0,315054},
	{0xfe4,316715},
	{0xfe8,318398},
	{0xfec,320001},
	{0xff0,321577},
	{0xff4,323128},
	{0xff8,324702},
	{0xffc,326203},
	{0x1f80,327680},
};

struct tx_isp_sensor_attribute sc5336_attr;

unsigned int sc5336_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc5336_again_lut;
	while(lut->gain <= sc5336_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc5336_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc5336_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sc5336_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 864,
	.lans = 2,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.image_twidth = 2880,
	.image_theight = 1620,
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

struct tx_isp_sensor_attribute sc5336_attr={
	.name = "sc5336",
	.chip_id = 0xce50,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.max_again = 327680,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc5336_alloc_again,
	.sensor_ctrl.alloc_dgain = sc5336_alloc_dgain,
};

static struct regval_list sc5336_init_regs_2880_1620_20fps_mipi_2lane[] = {
	{0x0103,0x01},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x01},
	{0x320c,0x06},//hts 1600
	{0x320d,0x40},//
	{0x320e,0x0a},//vts 2700
	{0x320f,0x8c},//
	{0x3213,0x04},
	{0x3241,0x00},
	{0x3243,0x01},
	{0x3248,0x02},
	{0x3249,0x0b},
	{0x3253,0x10},
	{0x3258,0x0c},
	{0x3301,0x0a},
	{0x3305,0x00},
	{0x3306,0x58},
	{0x3308,0x08},
	{0x3309,0xb0},
	{0x330a,0x00},
	{0x330b,0xc8},
	{0x3314,0x14},
	{0x331f,0xa1},
	{0x3321,0x10},
	{0x3327,0x14},
	{0x3328,0x0b},
	{0x3329,0x0e},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x3356,0x10},
	{0x3364,0x5e},
	{0x338f,0x80},
	{0x3390,0x09},
	{0x3391,0x0b},
	{0x3392,0x0f},
	{0x3393,0x10},
	{0x3394,0x16},
	{0x3395,0x98},
	{0x3396,0x08},
	{0x3397,0x09},
	{0x3398,0x0f},
	{0x3399,0x0a},
	{0x339a,0x18},
	{0x339b,0x60},
	{0x339c,0xff},
	{0x33ad,0x0c},
	{0x33ae,0x5c},
	{0x33af,0x52},
	{0x33b1,0xa0},
	{0x33b2,0x38},
	{0x33b3,0x18},
	{0x33f8,0x00},
	{0x33f9,0x60},
	{0x33fa,0x00},
	{0x33fb,0x80},
	{0x33fc,0x0b},
	{0x33fd,0x1f},
	{0x349f,0x03},
	{0x34a6,0x0b},
	{0x34a7,0x1f},
	{0x34a8,0x08},
	{0x34a9,0x08},
	{0x34aa,0x00},
	{0x34ab,0xd0},
	{0x34ac,0x00},
	{0x34ad,0xf0},
	{0x34f8,0x3f},
	{0x34f9,0x08},
	{0x3630,0xc0},
	{0x3631,0x83},
	{0x3632,0x54},
	{0x3633,0x33},
	{0x3638,0xcf},
	{0x363f,0xc0},
	{0x3641,0x20},
	{0x3670,0x56},
	{0x3674,0xc0},
	{0x3675,0xa0},
	{0x3676,0xa0},
	{0x3677,0x83},
	{0x3678,0x86},
	{0x3679,0x8a},
	{0x367c,0x08},
	{0x367d,0x0f},
	{0x367e,0x08},
	{0x367f,0x0f},
	{0x3696,0x23},
	{0x3697,0x33},
	{0x3698,0x34},
	{0x36a0,0x09},
	{0x36a1,0x0f},
	{0x36b0,0x88},
	{0x36b1,0x92},
	{0x36b2,0xa4},
	{0x36b3,0xc7},
	{0x36b4,0x09},
	{0x36b5,0x0b},
	{0x36b6,0x0f},
	{0x36ea,0x0c},
	{0x370f,0x01},
	{0x3721,0x6c},
	{0x3722,0x89},
	{0x3724,0x21},
	{0x3725,0xb4},
	{0x3727,0x14},
	{0x3771,0x89},
	{0x3772,0x89},
	{0x3773,0xc5},
	{0x377a,0x0b},
	{0x377b,0x1f},
	{0x37fa,0x0c},
	{0x3901,0x00},
	{0x3904,0x04},
	{0x3905,0x8c},
	{0x391d,0x04},
	{0x391f,0x49},
	{0x3926,0x21},
	{0x3933,0x80},
	{0x3934,0x0a},
	{0x3935,0x00},
	{0x3936,0xff},
	{0x3937,0x75},
	{0x3938,0x74},
	{0x393c,0x1e},
	{0x39dc,0x02},
	{0x3e00,0x00},
	{0x3e01,0x70},
	{0x3e02,0x00},
	{0x3e09,0x00},
	{0x440d,0x10},
	{0x440e,0x02},
	{0x450d,0x18},
	{0x4819,0x0b},
	{0x481b,0x06},
	{0x481d,0x17},
	{0x481f,0x05},
	{0x4821,0x0b},
	{0x4823,0x06},
	{0x4825,0x05},
	{0x4827,0x05},
	{0x4829,0x09},
	{0x5780,0x66},
	{0x5787,0x08},
	{0x5788,0x03},
	{0x5789,0x00},
	{0x578a,0x08},
	{0x578b,0x03},
	{0x578c,0x00},
	{0x578d,0x40},
	{0x5790,0x08},
	{0x5791,0x04},
	{0x5792,0x01},
	{0x5793,0x08},
	{0x5794,0x04},
	{0x5795,0x01},
	{0x5799,0x46},
	{0x57aa,0x2a},
	{0x5ae0,0xfe},
	{0x5ae1,0x40},
	{0x5ae2,0x38},
	{0x5ae3,0x30},
	{0x5ae4,0x0c},
	{0x5ae5,0x38},
	{0x5ae6,0x30},
	{0x5ae7,0x28},
	{0x5ae8,0x3f},
	{0x5ae9,0x34},
	{0x5aea,0x2c},
	{0x5aeb,0x3f},
	{0x5aec,0x34},
	{0x5aed,0x2c},
	{0x36e9,0x44},
	{0x37f9,0x44},
	{0x0100,0x01},
	{SC5336_REG_END, 0x00},/* END MARKER */
};


static struct tx_isp_sensor_win_setting sc5336_win_sizes[] = {
	{
		.width		= 2880,
		.height		= 1620,
		.fps		= 20 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= sc5336_init_regs_2880_1620_20fps_mipi_2lane,
	}
};
struct tx_isp_sensor_win_setting *wsize = &sc5336_win_sizes[0];

static struct regval_list sc5336_stream_on_mipi[] = {
	{0x0100, 0x01},
	{SC5336_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc5336_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SC5336_REG_END, 0x00},	/* END MARKER */
};

int sc5336_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
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

int sc5336_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc5336_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC5336_REG_END) {
		if (vals->reg_num == SC5336_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sc5336_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc5336_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC5336_REG_END) {
		if (vals->reg_num == SC5336_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sc5336_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc5336_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sc5336_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sc5336_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC5336_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc5336_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC5336_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc5336_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = -1;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	ret += sc5336_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc5336_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc5336_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
	ret = sc5336_write(sd, 0x3e07, (unsigned char)(again & 0xff));
	ret += sc5336_write(sd, 0x3e09, (unsigned char)(((again >> 8) & 0xff)));

	return 0;
}

#if 0
static int sc5336_set_integration_time(struct tx_isp_subdev *sd, int value)
{
       int ret = 0;
       ret = sc5336_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0x0f));
       ret += sc5336_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
       ret += sc5336_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));
       if (ret < 0)
               return ret;

       return 0;
}

static int sc5336_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
       int ret = 0;

       ret += sc5336_write(sd, 0x3e07, (unsigned char)(value & 0xff));
       ret += sc5336_write(sd, 0x3e09, (unsigned char)((value & 0xff00) >> 8));
       if (ret < 0)
               return ret;

       return 0;
}
#endif

static int sc5336_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc5336_get_black_pedestal(struct tx_isp_subdev *sd, int value)
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

static int sc5336_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!init->enable)
		return ISP_SUCCESS;

	sensor_set_attr(sd, wsize);
	sensor->video.state = TX_ISP_MODULE_INIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc5336_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned char val = 0;
	int ret = 0;
	if (init->enable) {
		if(sensor->video.state == TX_ISP_MODULE_INIT){
			ret = sc5336_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}

		sc5336_read(sd, 0x3040, &val);
		if (0x00 == val) {
			sc5336_write(sd, 0x3258, 0x0c);
			sc5336_write(sd, 0x3249, 0x0b);
			sc5336_write(sd, 0x3934, 0x0a);
			sc5336_write(sd, 0x3935, 0x00);
			sc5336_write(sd, 0x3937, 0x75);
		}else if (0x03 == val) {
			sc5336_write(sd, 0x3258, 0x08);
			sc5336_write(sd, 0x3249, 0x07);
			sc5336_write(sd, 0x3934, 0x05);
			sc5336_write(sd, 0x3935, 0x07);
			sc5336_write(sd, 0x3937, 0x74);
		}

		if(sensor->video.state == TX_ISP_MODULE_RUNNING){

			ret = sc5336_write_array(sd, sc5336_stream_on_mipi);
			ISP_WARNING("sc5336 stream on\n");
		}
	}
	else {
		ret = sc5336_write_array(sd, sc5336_stream_off_mipi);
		ISP_WARNING("sc5336 stream off\n");
	}

	return ret;
}

static int sc5336_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int max_fps;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;
	printk(" \nEnter sc5336_set_fps !\n ");
	switch(sensor->info.default_boot){
	case 0:
		sclk = 172800000; /* 1600 * 2700 * 20 * 2 */
		max_fps = 20;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps<< 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}

	ret += sc5336_read(sd, 0x320c, &val);
	hts = val << 8;
	ret += sc5336_read(sd, 0x320d, &val);
	hts = (hts | val);
	if (0 != ret) {
		ISP_ERROR("err: sc5336 read err\n");
		return -1;
	}
	hts = hts << 1;

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = sc5336_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc5336_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: sc5336_write err\n");
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

static int sc5336_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int sc5336_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	val = sc5336_read(sd, 0x3221, &val);
	switch(enable) {
	case 0:
		sc5336_write(sd, 0x3221, val & 0x99);
		break;
	case 1:
		sc5336_write(sd, 0x3221, val | 0x06);
		break;
	case 2:
		sc5336_write(sd, 0x3221, val | 0x60);
		break;
	case 3:
		sc5336_write(sd, 0x3221, val | 0x66);
		break;
	}

	if(!ret)
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
	int ret;

	switch(info->default_boot){
	case 0:
		wsize = &sc5336_win_sizes[0];
		memcpy(&(sc5336_attr.mipi), &sc5336_mipi, sizeof(sc5336_mipi));
		sc5336_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sc5336_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc5336_attr.max_integration_time_native = 2700 -8;
		sc5336_attr.integration_time_limit = 2700 -8;
		sc5336_attr.total_width = 3200;
		sc5336_attr.total_height = 2700;
		sc5336_attr.max_integration_time = 2700 -8;
		sc5336_attr.again =0;
		sc5336_attr.integration_time = 0x700;
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
	}

	switch(info->video_interface){
	case TISP_SENSOR_VI_MIPI_CSI0:
		sc5336_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc5336_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_DVP:
		sc5336_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this Interface Source!!!\n");
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
	switch(info->default_boot){
	case 0:
            if (((rate / 1000) % 27000) != 0) {
                    ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
                    sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
                    if (IS_ERR(sclka)) {
                            pr_err("get sclka failed\n");
                    } else {
                            rate = private_clk_get_rate(sclka);
                            if (((rate / 1000) % 27000) != 0) {
                                    private_clk_set_rate(sclka, 1188000000);
                            }
                    }
            }
            private_clk_set_rate(sensor->mclk, 27000000);
            private_clk_prepare_enable(sensor->mclk);
            break;
	}

	ISP_WARNING("\n====>[default_boot=%d] [resolution=%dx%d] [video_interface=%d] [MCLK=%d] \n", info->default_boot, wsize->width, wsize->height, info->video_interface, info->mclk);
	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;
        sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;
	return 0;

}

static int sc5336_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc5336_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"sc5336_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc5336_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc5336 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("sc5336 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc5336", sizeof("sc5336"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc5336_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = sc5336_set_expo(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		//if(arg)
		//	ret = sc5336_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		//if(arg)
		//	ret = sc5336_set_analog_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc5336_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc5336_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc5336_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = sc5336_write_array(sd, sc5336_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sc5336_write_array(sd, sc5336_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc5336_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = sc5336_set_vflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int sc5336_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc5336_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc5336_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc5336_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc5336_core_ops = {
	.g_chip_ident = sc5336_g_chip_ident,
	.reset = sc5336_reset,
	.init = sc5336_init,
	.g_register = sc5336_g_register,
	.s_register = sc5336_s_register,
};

static struct tx_isp_subdev_video_ops sc5336_video_ops = {
	.s_stream = sc5336_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc5336_sensor_ops = {
	.ioctl	= sc5336_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc5336_ops = {
	.core = &sc5336_core_ops,
	.video = &sc5336_video_ops,
	.sensor = &sc5336_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc5336",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc5336_probe(struct i2c_client *client,
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
	sensor->dev = &client->dev;
	sc5336_attr.expo_fs = 1;
	sensor->video.shvflip = 1;
	sensor->video.attr = &sc5336_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc5336_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc5336\n");

	return 0;
}

static int sc5336_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc5336_id[] = {
	{ "sc5336", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc5336_id);

static struct i2c_driver sc5336_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc5336",
	},
	.probe		= sc5336_probe,
	.remove		= sc5336_remove,
	.id_table	= sc5336_id,
};

static __init int init_sc5336(void)
{
	return private_i2c_add_driver(&sc5336_driver);
}

static __exit void exit_sc5336(void)
{
	private_i2c_del_driver(&sc5336_driver);
}

module_init(init_sc5336);
module_exit(exit_sc5336);

MODULE_DESCRIPTION("A low-level driver for Smartsens sc5336 sensors");
MODULE_LICENSE("GPL");
