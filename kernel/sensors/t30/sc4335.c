/*
 * sc4335.c
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

#define SC4335_CHIP_ID_H	(0xCD)
#define SC4335_CHIP_ID_L	(0x01)
#define SC4335_REG_END		0xffff
#define SC4335_REG_DELAY	0xfffe
#define SC4335_SUPPORT_SCLK (121500000)
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_1
#define SENSOR_VERSION	"H20190430a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

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
struct again_lut sc4335_again_lut[] = {
	{0x320, 0},
	{0x321, 2886},
	{0x322, 5776},
	{0x323, 8494},
	{0x324, 11136},
	{0x325, 13706},
	{0x326, 16287},
	{0x327, 18723},
	{0x328, 21097},
	{0x329, 23413},
	{0x32a, 25746},
	{0x32b, 27952},
	{0x32c, 30108},
	{0x32d, 32216},
	{0x32e, 34344},
	{0x32f, 36361},
	{0x330, 38335},
	{0x331, 40269},
	{0x332, 42225},
	{0x333, 44082},
	{0x334, 45903},
	{0x335, 47689},
	{0x336, 49499},
	{0x337, 51220},
	{0x338, 52910},
	{0x339, 54570},
	{0x33a, 56253},
	{0x33b, 57856},
	{0x33c, 59433},
	{0x33d, 60983},
	{0x33e, 62557},
	{0x33f, 64058},
	{0x720, 65535},
	{0x721, 68467},
	{0x722, 71266},
	{0x723, 74029},
	{0x724, 76671},
	{0x725, 79281},
	{0x726, 81782},
	{0x727, 84258},
	{0x728, 86632},
	{0x729, 88985},
	{0x72a, 91245},
	{0x72b, 93487},
	{0x72c, 95643},
	{0x72d, 97785},
	{0x72e, 99846},
	{0x72f, 101896},
	{0x730, 103870},
	{0x731, 105835},
	{0x732, 107730},
	{0x733, 109617},
	{0x734, 111438},
	{0x735, 113253},
	{0x736, 115006},
	{0x737, 116755},
	{0x738, 118445},
	{0x739, 120131},
	{0x73a, 121762},
	{0x73b, 123391},
	{0x73c, 124968},
	{0x73d, 126543},
	{0x73e, 128068},
	{0x73f, 129593},
	{0xf20, 131070},
	{0xf21, 133979},
	{0xf22, 136801},
	{0xf23, 139542},
	{0xf24, 142206},
	{0xf25, 144796},
	{0xf26, 147317},
	{0xf27, 149773},
	{0xf28, 152167},
	{0xf29, 154502},
	{0xf2a, 156780},
	{0xf2b, 159005},
	{0xf2c, 161178},
	{0xf2d, 163303},
	{0xf2e, 165381},
	{0xf2f, 167414},
	{0xf30, 169405},
	{0xf31, 171355},
	{0xf32, 173265},
	{0xf33, 175137},
	{0xf34, 176973},
	{0xf35, 178774},
	{0xf36, 180541},
	{0xf37, 182276},
	{0xf38, 183980},
	{0xf39, 185653},
	{0xf3a, 187297},
	{0xf3b, 188914},
	{0xf3c, 190503},
	{0xf3d, 192065},
	{0xf3e, 193603},
	{0xf3f, 195116},
	{0x1f20, 196605},
	{0x1f21, 199514},
	{0x1f22, 202336},
	{0x1f23, 205077},
	{0x1f24, 207741},
	{0x1f25, 210331},
	{0x1f26, 212852},
	{0x1f27, 215308},
	{0x1f28, 217702},
	{0x1f29, 220037},
	{0x1f2a, 222315},
	{0x1f2b, 224540},
	{0x1f2c, 226713},
	{0x1f2d, 228838},
	{0x1f2e, 230916},
	{0x1f2f, 232949},
	{0x1f30, 234940},
	{0x1f31, 236890},
	{0x1f32, 238800},
	{0x1f33, 240672},
	{0x1f34, 242508},
	{0x1f35, 244309},
	{0x1f36, 246076},
	{0x1f37, 247811},
	{0x1f38, 249515},
	{0x1f39, 251188},
	{0x1f3a, 252832},
	{0x1f3b, 254449},
	{0x1f3c, 256038},
	{0x1f3d, 257600},
	{0x1f3e, 259138},
	{0x1f3f, 260651}
};

struct tx_isp_sensor_attribute sc4335_attr;
unsigned int sc4335_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc4335_again_lut;

	while(lut->gain <= sc4335_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc4335_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc4335_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return isp_gain;
}

struct tx_isp_sensor_attribute sc4335_attr={
	.name = "sc4335",
	.chip_id = 0xcd01,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.clk = 600,
		.lans = 2,
	},
	.max_again = 260651,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 0x5dc - 4,
	.integration_time_limit = 0x5dc - 4,
	.total_width = 0xca8,
	.total_height = 0x5dc,
	.max_integration_time = 0x5dc-4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc4335_alloc_again,
	.sensor_ctrl.alloc_dgain = sc4335_alloc_dgain,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list sc4335_init_regs_2560_1440_25fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0xd4},
	{0x36f9,0xd4},
	{0x3650,0x06},
	{0x3e09,0x20},
	{0x3637,0x70},
	{0x3e25,0x03},
	{0x3e26,0x20},
	{0x3620,0xa8},
	{0x3638,0x22},
	{0x3309,0x88},
	{0x331f,0x79},
	{0x330f,0x04},
	{0x3310,0x20},
	{0x3314,0x94},
	{0x330e,0x38},
	{0x3367,0x10},
	{0x3347,0x05},
	{0x3342,0x01},
	{0x57a4,0xa0},
	{0x5785,0x40},
	{0x5787,0x18},
	{0x5788,0x10},
	{0x3908,0x41},
	{0x330a,0x01},
	{0x363a,0x90},
	{0x3306,0x60},
	{0x3614,0x80},
	{0x3213,0x06},
	{0x3633,0x33},
	{0x3622,0xf6},
	{0x3630,0xc0},
	{0x360f,0x05},
	{0x367a,0x08},
	{0x367b,0x38},
	{0x3671,0xf6},
	{0x3672,0xf6},
	{0x3673,0x16},
	{0x366e,0x04},
	{0x369c,0x38},
	{0x369d,0x3f},
	{0x3690,0x33},
	{0x3691,0x34},
	{0x3692,0x44},
	{0x3364,0x1d},
	{0x33b8,0x10},
	{0x33b9,0x18},
	{0x33ba,0x70},
	{0x33b6,0x07},
	{0x33b7,0x2f},
	{0x3670,0x0a},
	{0x367c,0x38},
	{0x367d,0x3f},
	{0x3674,0xc0},
	{0x3675,0xc8},
	{0x3676,0xaf},
	{0x330b,0x08},
	{0x301f,0x01},
	{0x5781,0x04},
	{0x5782,0x04},
	{0x5783,0x02},
	{0x5784,0x02},
	{0x5786,0x20},
	{0x5789,0x10},
	{0x363b,0x09},
	{0x3625,0x0a},
	{0x3904,0x10},
	{0x3902,0xc5},
	{0x3933,0x0a},
	{0x3934,0x0d},
	{0x3942,0x02},
	{0x3943,0x12},
	{0x3940,0x65},
	{0x3941,0x18},
	{0x395e,0xa0},
	{0x3960,0x9d},
	{0x3961,0x9d},
	{0x3966,0x4e},
	{0x3962,0x89},
	{0x3963,0x80},
	{0x3980,0x60},
	{0x3981,0x30},
	{0x3982,0x15},
	{0x3983,0x10},
	{0x3984,0x0d},
	{0x3985,0x20},
	{0x3986,0x30},
	{0x3987,0x60},
	{0x3988,0x04},
	{0x3989,0x0c},
	{0x398a,0x14},
	{0x398b,0x24},
	{0x398c,0x50},
	{0x398d,0x32},
	{0x398e,0x1e},
	{0x398f,0x0a},
	{0x3990,0xc0},
	{0x3991,0x50},
	{0x3992,0x22},
	{0x3993,0x0c},
	{0x3994,0x10},
	{0x3995,0x38},
	{0x3996,0x80},
	{0x3997,0xff},
	{0x3998,0x08},
	{0x3999,0x16},
	{0x399a,0x28},
	{0x399b,0x40},
	{0x399c,0x50},
	{0x399d,0x28},
	{0x399e,0x18},
	{0x399f,0x0c},
	{0x3e01,0xbb},
	{0x3e02,0x00},
	{0x3301,0x10},
	{0x3632,0x18},
	{0x3631,0x88},
	{0x3636,0x25},
	{0x320c,0x0c},
	{0x320d,0xa8},
	{0x36e9,0x54},
	{0x36f9,0x54},
	{0x0100,0x00},

	{SC4335_REG_END, 0x00},/* END MARKER */
};
/*
 * the order of the sc4335_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc4335_win_sizes[] = {
	/* 2560*1440 @ 25fps */
	{
		.width		= 2560,
		.height		= 1440,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc4335_init_regs_2560_1440_25fps_mipi,
	}
};

static enum v4l2_mbus_pixelcode sc4335_mbus_code[] = {
	V4L2_MBUS_FMT_SBGGR10_1X10,
};

/*
 * the part of driver was fixed.
 */

static struct regval_list sc4335_stream_on_mipi[] = {
	{0x0100, 0x01},
	{SC4335_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc4335_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SC4335_REG_END, 0x00},	/* END MARKER */
};

int sc4335_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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

int sc4335_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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

static int sc4335_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC4335_REG_END) {
		if (vals->reg_num == SC4335_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sc4335_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
static int sc4335_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC4335_REG_END) {
		if (vals->reg_num == SC4335_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sc4335_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc4335_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sc4335_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sc4335_read(sd, 0x3107, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC4335_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc4335_read(sd, 0x3108, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC4335_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc4335_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
	ret = sc4335_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0x0f));
	ret += sc4335_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc4335_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));

	if (ret < 0)
		return ret;

	return 0;
}

static int sc4335_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sc4335_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sc4335_write(sd, 0x3e08, (unsigned char)((value & 0xff00) >> 8));
	/* denoise logic */
	if (value < 0x720) {
		ret += sc4335_write(sd,0x3812,0x00);
		ret += sc4335_write(sd,0x3631,0x88);
		ret += sc4335_write(sd,0x3632,0x18);
		ret += sc4335_write(sd,0x3636,0x25);
		ret += sc4335_write(sd,0x3812,0x30);
	}else if (value < 0xf20){
		ret += sc4335_write(sd,0x3812,0x00);
		ret += sc4335_write(sd,0x3631,0x8e);
		ret += sc4335_write(sd,0x3632,0x18);
		ret += sc4335_write(sd,0x3636,0x25);
		ret += sc4335_write(sd,0x3812,0x30);
	}else if(value < 0x1f20){
		ret += sc4335_write(sd,0x3812,0x00);
		ret += sc4335_write(sd,0x3631,0x80);
		ret += sc4335_write(sd,0x3632,0x18);
		ret += sc4335_write(sd,0x3636,0x65);
		ret += sc4335_write(sd,0x3812,0x30);
	}else if(value < 0x1f3f){
		ret += sc4335_write(sd,0x3812,0x00);
		ret += sc4335_write(sd,0x3631,0x80);
		ret += sc4335_write(sd,0x3632,0x18);
		ret += sc4335_write(sd,0x3636,0x65);
		ret += sc4335_write(sd,0x3812,0x30);
	}else {
		ret += sc4335_write(sd,0x3812,0x00);
		ret += sc4335_write(sd,0x3631,0x80);
		ret += sc4335_write(sd,0x3632,0xd8);
		ret += sc4335_write(sd,0x3636,0x65);
		ret += sc4335_write(sd,0x3812,0x30);
	}
	if (ret < 0)
		return ret;

	return 0;
}

static int sc4335_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc4335_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc4335_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize;
	int ret = 0;

	if(!enable)
		return ISP_SUCCESS;

	wsize = &sc4335_win_sizes[0];
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = sc4335_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc4335_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = sc4335_write_array(sd, sc4335_stream_on_mipi);
		pr_debug("sc4335 stream on\n");

	}
	else {
		ret = sc4335_write_array(sd, sc4335_stream_off_mipi);
		pr_debug("sc4335 stream off\n");
	}

	return ret;
}

static int sc4335_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		printk("warn: fps(%d) no in range\n", fps);
		return -1;
	}

	sclk = SC4335_SUPPORT_SCLK;

	val = 0;
	ret += sc4335_read(sd, 0x320c, &val);
	hts = val << 8;
	val = 0;
	ret += sc4335_read(sd, 0x320d, &val);
	hts |= val;
	if (0 != ret) {
		printk("err: sc4335 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = sc4335_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc4335_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		printk("err: sc4335_write err\n");
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

static int sc4335_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;

	if(value == TX_ISP_SENSOR_FULL_RES_MAX_FPS){
		wsize = &sc4335_win_sizes[0];
	}else if(value == TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS){
		wsize = &sc4335_win_sizes[0];
	}

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

static int sc4335_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned char val = 0;

	val = enable ? 0x60 : 0;
	ret = sc4335_write(sd, 0x3221, val);
	sensor->video.mbus_change = 0;
	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sc4335_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc4335_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}else{
			printk("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"sc4335_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			printk("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc4335_detect(sd, &ident);
	if (ret) {
		printk("chip found @ 0x%x (%s) is not an sc4335 chip.\n",
		       client->addr, client->adapter->name);
		return ret;
	}
	printk("sc4335 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "sc4335", sizeof("sc4335"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}
static int sc4335_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;

	if(IS_ERR_OR_NULL(sd)){
		printk("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = sc4335_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = sc4335_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc4335_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc4335_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc4335_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = sc4335_write_array(sd, sc4335_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sc4335_write_array(sd, sc4335_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc4335_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = sc4335_set_vflip(sd, *(int*)arg);
		break;
	default:
		break;;
	}
	return 0;
}

static int sc4335_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc4335_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc4335_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc4335_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc4335_core_ops = {
	.g_chip_ident = sc4335_g_chip_ident,
	.reset = sc4335_reset,
	.init = sc4335_init,
	.g_register = sc4335_g_register,
	.s_register = sc4335_s_register,
};

static struct tx_isp_subdev_video_ops sc4335_video_ops = {
	.s_stream = sc4335_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc4335_sensor_ops = {
	.ioctl	= sc4335_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc4335_ops = {
	.core = &sc4335_core_ops,
	.video = &sc4335_video_ops,
	.sensor = &sc4335_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc4335",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc4335_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &sc4335_win_sizes[0];
	unsigned long rate = 0;
	int ret;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		printk("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		printk("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	rate = clk_get_rate(clk_get_parent(sensor->mclk));
	if (((rate / 1000) % 27000) != 0) {
		struct clk *epll;
		epll = clk_get(NULL,"epll");
		if (IS_ERR(epll)) {
			pr_err("get epll failed\n");
		} else {
			rate = clk_get_rate(epll);
			if (((rate / 1000) % 27000) != 0) {
				clk_set_rate(epll,891000000);
			}
			ret = clk_set_parent(sensor->mclk, epll);
			if (ret < 0)
				pr_err("set mclk parent as epll err\n");
		}
	}
	private_clk_set_rate(sensor->mclk, 27000000);
	private_clk_enable(sensor->mclk);

	/*
	  convert sensor-gain into isp-gain,
	*/
	sc4335_attr.max_again = 261773;
	sc4335_attr.max_dgain = 0;

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sc4335_attr;
	sensor->video.mbus_change = 0;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc4335_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc4335\n");
	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int sc4335_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc4335_id[] = {
	{ "sc4335", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc4335_id);

static struct i2c_driver sc4335_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc4335",
	},
	.probe		= sc4335_probe,
	.remove		= sc4335_remove,
	.id_table	= sc4335_id,
};

static __init int init_sc4335(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		printk("Failed to init sc4335 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&sc4335_driver);
}

static __exit void exit_sc4335(void)
{
	private_i2c_del_driver(&sc4335_driver);
}

module_init(init_sc4335);
module_exit(exit_sc4335);

MODULE_DESCRIPTION("A low-level driver for Smartsens sc4335 sensors");
MODULE_LICENSE("GPL");
