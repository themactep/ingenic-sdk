/*
 * gc0328.c
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
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <soc/gpio.h>
#include <tx-isp-common.h>
#include <sensor-common.h>
#include <txx-funcs.h>

#define GC0328_CHIP_ID		(0x9d)

#define GC0328_FLAG_END		0x00
#define GC0328_FLAG_DELAY	0xff
#define GC0328_PAGE_REG		0xfe

#define GC0328_SUPPORT_PCLK (6*1000*1000)
#define SENSOR_OUTPUT_MAX_FPS 10
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_2
#define SENSOR_VERSION	"H20200116a"

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};
static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = GPIO_PA(19);
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_LOW_8BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut gc0328_again_lut[] = {
	{0x40, 0},
	{0x41, 1465},
	{0x42, 2909},
	{0x43, 4331},
	{0x44, 5731},
	{0x45, 7112},
	{0x46, 8472},
	{0x47, 9813},
	{0x48, 11136},
	{0x49, 12440},
	{0x4a, 13726},
	{0x4b, 14995},
	{0x4c, 16248},
	{0x4d, 17484},
	{0x4e, 18704},
	{0x4f, 19908},
	{0x50, 21097},
	{0x51, 22272},
	{0x52, 23432},
	{0x53, 24578},
	{0x54, 25710},
	{0x55, 26829},
	{0x56, 27935},
	{0x57, 29028},
	{0x58, 30109},
	{0x59, 31177},
	{0x5a, 32234},
	{0x5b, 33278},
	{0x5c, 34312},
	{0x5d, 35334},
	{0x5e, 36345},
	{0x5f, 37346},
	{0x60, 38336},
	{0x61, 39315},
	{0x62, 40285},
	{0x63, 41245},
	{0x64, 42195},
	{0x65, 43136},
	{0x66, 44068},
	{0x67, 44990},
	{0x68, 45904},
	{0x69, 46808},
	{0x6a, 47704},
	{0x6b, 48592},
	{0x6c, 49472},
	{0x6d, 50343},
	{0x6e, 51207},
	{0x6f, 52062},
	{0x70, 52910},
	{0x71, 53751},
	{0x72, 54584},
	{0x73, 55410},
	{0x74, 56228},
	{0x75, 57040},
	{0x76, 57844},
	{0x77, 58642},
	{0x78, 59433},
	{0x79, 60218},
	{0x7a, 60996},
	{0x7b, 61768},
	{0x7c, 62534},
	{0x7d, 63293},
	{0x7e, 64047},
	{0x7f, 64794},
	{0x80, 65536},
	{0x81, 66271},
	{0x82, 67001},
	{0x83, 67726},
	{0x84, 68445},
	{0x85, 69158},
	{0x86, 69867},
	{0x87, 70570},
	{0x88, 71267},
	{0x89, 71960},
	{0x8a, 72648},
	{0x8b, 73330},
	{0x8c, 74008},
	{0x8d, 74681},
	{0x8e, 75349},
	{0x8f, 76013},
	{0x90, 76672},
	{0x91, 77326},
	{0x92, 77976},
	{0x93, 78621},
	{0x94, 79262},
	{0x95, 79899},
	{0x96, 80531},
	{0x97, 81160},
	{0x98, 81784},
	{0x99, 82404},
	{0x9a, 83020},
	{0x9b, 83632},
	{0x9c, 84240},
	{0x9d, 84844},
	{0x9e, 85444},
	{0x9f, 86041},
	{0xa0, 86633},
	{0xa1, 87222},
	{0xa2, 87808},
	{0xa3, 88390},
	{0xa4, 88968},
	{0xa5, 89543},
	{0xa6, 90114},
	{0xa7, 90682},
	{0xa8, 91246},
	{0xa9, 91808},
	{0xaa, 92365},
	{0xab, 92920},
	{0xac, 93471},
	{0xad, 94019},
	{0xae, 94564},
	{0xaf, 95106},
	{0xb0, 95645},
	{0xb1, 96180},
	{0xb2, 96713},
	{0xb3, 97243},
	{0xb4, 97770},
	{0xb5, 98293},
	{0xb6, 98814},
	{0xb7, 99332},
	{0xb8, 99848},
	{0xb9, 100360},
	{0xba, 100870},
	{0xbb, 101377},
	{0xbc, 101881},
	{0xbd, 102383},
	{0xbe, 102882},
	{0xbf, 103378},
	{0xc0, 103872},
	{0xc1, 104363},
	{0xc2, 104851},
	{0xc3, 105337},
	{0xc4, 105821},
	{0xc5, 106302},
	{0xc6, 106781},
	{0xc7, 107257},
	{0xc8, 107731},
	{0xc9, 108203},
	{0xca, 108672},
	{0xcb, 109139},
	{0xcc, 109604},
	{0xcd, 110066},
	{0xce, 110526},
	{0xcf, 110984},
	{0xd0, 111440},
	{0xd1, 111893},
	{0xd2, 112344},
	{0xd3, 112793},
	{0xd4, 113240},
	{0xd5, 113685},
	{0xd6, 114128},
	{0xd7, 114569},
	{0xd8, 115008},
	{0xd9, 115445},
	{0xda, 115879},
	{0xdb, 116312},
	{0xdc, 116743},
	{0xdd, 117171},
	{0xde, 117598},
	{0xdf, 118023},
	{0xe0, 118446},
	{0xe1, 118867},
	{0xe2, 119287},
	{0xe3, 119704},
	{0xe4, 120120},
	{0xe5, 120534},
	{0xe6, 120946},
	{0xe7, 121356},
	{0xe8, 121764},
	{0xe9, 122171},
	{0xea, 122576},
	{0xeb, 122979},
	{0xec, 123380},
	{0xed, 123780},
	{0xee, 124178},
	{0xef, 124575},
	{0xf0, 124969},
	{0xf1, 125363},
	{0xf2, 125754},
	{0xf3, 126144},
	{0xf4, 126532},
	{0xf5, 126919},
	{0xf6, 127304},
	{0xf7, 127688},
	{0xf8, 128070},
	{0xf9, 128450},
	{0xfa, 128829},
	{0xfb, 129207},
	{0xfc, 129583},
	{0xfd, 129957},
	{0xfe, 130330},
	{0xff, 130701},
};

struct tx_isp_sensor_attribute gc0328_attr;

unsigned int gc0328_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = gc0328_again_lut;

	while(lut->gain <= gc0328_attr.max_again) {
		if(isp_gain == gc0328_attr.max_again) {
			*sensor_again = lut->value;
			return lut->gain;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if(lut->gain == gc0328_attr.max_again) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int gc0328_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}
/*
 * the part of driver maybe modify about different sensor and different board.
 */

struct tx_isp_sensor_attribute gc0328_attr={
	.name = "gc0328",
	.chip_id = 0x9b,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x21,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
	},
	.expo_fs = 1,
	.max_again = 130330,
	.max_dgain = 0,
	.total_width = 956,
	.total_height = 627,
	.min_integration_time = 4,
	.max_integration_time = 627 - 4,
	.integration_time_limit = 627 - 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 627 - 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = gc0328_alloc_again,
	.sensor_ctrl.alloc_dgain = gc0328_alloc_dgain,
};

static struct regval_list gc0328_init_regs_640_480[] = {
	{0xfe, 0x80},
	{0xfe, 0x80},
	{0xfc, 0x16},
	{0xfc, 0x16},
	{0xfc, 0x16},
	{0xfc, 0x16},
	{0xf1, 0x00},
	{0xf2, 0x00},
	{0xfe, 0x00},
	{0x4f, 0x00},
	{0x03, 0x00},
	{0x04, 0xc0},
	{0x42, 0x00},
	{0x77, 0x5a},
	{0x78, 0x40},
	{0x79, 0x56},
	{0xfe, 0x00},
	{0x09, 0x00},
	{0x0a, 0x00},
	{0x0b, 0x00},
	{0x0c, 0x00},
	{0x0d, 0x01},
	{0x0e, 0xe8},
	{0x0f, 0x02},
	{0x10, 0x88},
	{0x16, 0x00},
	{0x17, 0x14},
	{0x18, 0x0e},
	{0x19, 0x06},
	{0x1b, 0x48},
	{0x1f, 0xC8},
	{0x20, 0x01},
	{0x21, 0x78},
	{0x22, 0xb0},
	{0x23, 0x04},//0x06  20140519 GC0328C
	{0x24, 0x11},
	{0x24, 0x3f},//jx
	{0x26, 0x00},
	{0x50, 0x01}, //crop mode
	//global gain for range
	{0x70, 0xff},
	{0x71, 0x40},
	{0x72, 0xff},
	/////////////banding/////////////
#if 0
	{0x11, 0x2a},//sh_delay
	{0x05, 0x01},//hb  225¨°?¨¦?
	{0x06, 0x06},//
	{0x07, 0x00},//vb
	{0x08, 0x0c},//
#else
	{0x11, 0x2a},//sh_delay
	{0x05, 0x00},//hb  225¨°?¨¦?
	{0x06, 0x80},//
	{0x07, 0x00},//vb
	{0x08, 0x60},//
#endif
	{0x11, 0x2a},//sh_delay
	{0x05, 0x01},//hb  225¨°?¨¦?
	{0x06, 0x06},//
	{0x07, 0x00},//vb
	{0x08, 0x8b},//
	//////////
	{0xfe, 0x01},//
	{0x29, 0x00},//anti-flicker step [11:8]
	{0x2a, 0x78},//anti-flicker step [7:0]
	{0x2b, 0x01},//exp level 0  25fps
	{0x2c, 0xe0},
	{0x2d, 0x01},//exp level 1  25fps
	{0x2e, 0xe0},
	{0x2f, 0x01},//exp level 2  25fps
	{0x30, 0xe0},
	{0x31, 0x01},//exp level 3  25fps
	{0x32, 0xe0},
	{0xfe, 0x00},
	//////////// BLK//////////////////////
	{0xfe, 0x00},
	{0x27, 0xb7},
	{0x28, 0x7F},
	{0x29, 0x20},
	{0x33, 0x20},
	{0x34, 0x20},
	{0x35, 0x20},
	{0x36, 0x20},
	{0x32, 0x08},
	{0x3b, 0x00},
	{0x3c, 0x00},
	{0x3d, 0x00},
	{0x3e, 0x00},
	{0x47, 0x00},
	{0x48, 0x00},
	//////////// block enable/////////////
	{0x40, 0x00},
	{0x41, 0x00},
	{0x42, 0x00},
	{0x44, 0xb8}, //yuv//0x44=0xb8
	{0x45, 0x00},
	{0x46, 0x02},
	{0x49, 0x03}, //0x44=0xb9 0x70/0x71/0x72ÎÞÐ§¹û£»
	{0x4f, 0x00},
	{0x4b, 0x01},
	{0x50, 0x01},
	/////////output////////
	{0xfe, 0x00},
	{0xf1, 0x07},
	{0xf2, 0x01},
	{GC0328_FLAG_END, 0x00},/* END MARKER */
};

/*
 * the order of the gc0328_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting gc0328_win_sizes[] = {
	{
		.width		= 640,
		.height		= 480,
		.fps		= 10 << 16|1,
		.mbus_code	= V4L2_MBUS_FMT_SRGGB8_1X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= gc0328_init_regs_640_480,
	}
};

/*
 * the part of driver was fixed.
 */

static struct regval_list gc0328_stream_on[] = {
	{0xfe, 0x00},
	{0x44, 0xb8},
	{GC0328_FLAG_END, 0x00},	/* END MARKER */
};

static struct regval_list gc0328_stream_off[] = {
	{0xfe, 0x00},
	{GC0328_FLAG_END, 0x00},	/* END MARKER */
};

int gc0328_read(struct tx_isp_subdev *sd, unsigned char reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= &reg,
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

static int gc0328_write(struct tx_isp_subdev *sd, unsigned char reg,
		unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 2,
		.buf	= buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}


static int gc0328_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != GC0328_FLAG_END) {
		if(vals->reg_num == GC0328_FLAG_DELAY) {
				msleep(vals->value);
		} else {
			ret = gc0328_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
//			if (vals->reg_num == GC0328_PAGE_REG){
//				val &= 0xf8;
//				val |= (vals->value & 0x07);
//				ret = gc0328_write(sd, vals->reg_num, val);
//				ret = gc0328_read(sd, vals->reg_num, &val);
//			}
		}
		vals++;
	}
	return 0;
}
static int gc0328_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != GC0328_FLAG_END) {
		if (vals->reg_num == GC0328_FLAG_DELAY) {
				msleep(vals->value);
		} else {
			ret = gc0328_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int gc0328_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int gc0328_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = gc0328_write(sd, 0xfe, 0x00);
	ret = gc0328_read(sd, 0xf0, &v);

	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != GC0328_CHIP_ID)
		return -ENODEV;

	*ident = v;
	return 0;
}

static int ag_last = -1;
static int it_last = -1;
static int gc0328_set_expo(struct tx_isp_subdev* sd, int value)
{
	int ret = 0;
	int expo = (value & 0x0000ffff);
	int again = (value & 0xffff0000) >> 16;

	ret = gc0328_write(sd, 0xfe, 0x00);
	if(it_last != expo)
	{
		ret = gc0328_write(sd, 0x4, expo & 0xff);
		ret = gc0328_write(sd, 0x3, (expo & 0x1f00)>>8);
	}
	if(ag_last != again)
	{
		ret = gc0328_write(sd, 0x71, (unsigned short)again);
	}

	return ret;
}

static int gc0328_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = gc0328_write(sd, 0xfe, 0x00);
	ret = gc0328_write(sd, 0x4, value & 0xff);
	if (ret < 0) {
		ISP_ERROR("gc0328_write error  %d\n" ,__LINE__);
		return ret;
	}
	ret = gc0328_write(sd, 0x3, (value & 0x1f00)>>8);
	if (ret < 0) {
		ISP_ERROR("gc0328_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int gc0328_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = gc0328_write(sd, 0xfe, 0x00);
	ret = gc0328_write(sd, 0x71, (unsigned short)value);
	if (ret < 0) {
		ISP_ERROR("gc0328_write error  %d" ,__LINE__ );
		return ret;
	}
	return 0;
}

static int gc0328_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc0328_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int gc0328_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = &gc0328_win_sizes[0];
	int ret = 0;
	if(!enable)
		return ISP_SUCCESS;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = gc0328_write_array(sd, wsize->regs);
	if (ret)
		return ret;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;
	return 0;
}

static int gc0328_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	if (enable) {
		ret = gc0328_write_array(sd, gc0328_stream_on);
		pr_debug("gc0328 stream on\n");
	}
	else {
		ret = gc0328_write_array(sd, gc0328_stream_off);
		pr_debug("gc0328 stream off\n");
	}
	return ret;
}

static int gc0328_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int pclk = GC0328_SUPPORT_PCLK;
	unsigned short win_width=0;
	unsigned short win_high=0;
	unsigned short vts = 0;
	unsigned short hb=0;
	unsigned short sh_delay=0;
	unsigned short vb = 0;
	unsigned short hts=0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)){
		ISP_WARNING("set_fps error ,should be %d  ~ %d \n",SENSOR_OUTPUT_MIN_FPS, SENSOR_OUTPUT_MAX_FPS);
		return -1;
	}

	ret = gc0328_write(sd, 0xfe, 0x00);
	ret = gc0328_read(sd, 0x05, &tmp);
	hb = tmp & 0x0f;
	ret += gc0328_read(sd, 0x06, &tmp);
	hb = (hb << 8) | tmp;

	ret += gc0328_read(sd, 0x11, &tmp);
	sh_delay = tmp;

	ret += gc0328_read(sd, 0x0f, &tmp);
	win_width = tmp;
	ret += gc0328_read(sd, 0x10, &tmp);
	win_width = (win_width << 8) + tmp;

	ret = gc0328_read(sd, 0x0d, &tmp);
	win_high = tmp;
	ret += gc0328_read(sd, 0x0e, &tmp);
	win_high = (win_high << 8) + tmp;

	hts = win_width + hb + sh_delay + 4;
	vts = pclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	vb = vts - win_high;

	ret = gc0328_write(sd, 0x08, (unsigned char)(vb & 0x00ff));
	ret += gc0328_read(sd, 0x07, &tmp);
	tmp = ((unsigned char)((vb & 0x0f00) >> 8)) | (tmp & 0x0f);
	ret += gc0328_write(sd, 0x07, tmp);

	if(ret < 0)
		return -1;

	sensor->video.fps = fps;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->max_integration_time_native = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int gc0328_set_mode(struct tx_isp_subdev *sd, int value)
{
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;
	wsize = &gc0328_win_sizes[0];

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
static int gc0328_g_chip_ident(struct tx_isp_subdev *sd,
		struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"gc0328_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"gc0328_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = gc0328_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an gc0328 chip.\n",
			client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("gc0328 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "gc0328", sizeof("gc0328"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
        }
        return 0;
}
static int gc0328_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
#if 1
		case TX_ISP_EVENT_SENSOR_EXPO:
			if(arg)
				ret = gc0328_set_expo(sd, *(int*)arg);
		break;
#else
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			if(arg)
				ret = gc0328_set_integration_time(sd, *(int*)arg);
		break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			if(arg)
				ret = gc0328_set_analog_gain(sd, *(int*)arg);
		break;
#endif
		case TX_ISP_EVENT_SENSOR_DGAIN:
			if(arg)
				ret = gc0328_set_digital_gain(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
			if(arg)
				ret = gc0328_get_black_pedestal(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if(arg)
				ret = gc0328_set_mode(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
				ret = gc0328_write_array(sd, gc0328_stream_off);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
				ret = gc0328_write_array(sd, gc0328_stream_on);
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if(arg)
				ret = gc0328_set_fps(sd, *(int*)arg);
			break;
	        default:
		        break;
	}
	return 0;
}

static int gc0328_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = gc0328_write(sd, 0xfe, 0x00);
	ret = gc0328_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int gc0328_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	gc0328_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}

static struct tx_isp_subdev_core_ops gc0328_core_ops = {
	.g_chip_ident = gc0328_g_chip_ident,
	.reset = gc0328_reset,
	.init = gc0328_init,
	.g_register = gc0328_g_register,
	.s_register = gc0328_s_register,
};

static struct tx_isp_subdev_video_ops gc0328_video_ops = {
	.s_stream = gc0328_s_stream,
};

static struct tx_isp_subdev_sensor_ops	gc0328_sensor_ops = {
	.ioctl	= gc0328_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops gc0328_ops = {
	.core = &gc0328_core_ops,
	.video = &gc0328_video_ops,
	.sensor = &gc0328_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "gc0328",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int gc0328_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret;
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &gc0328_win_sizes[0];

	it_last = -1;
	ag_last = -1;
	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
//	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_set_rate(sensor->mclk, 12000000);
	private_clk_enable(sensor->mclk);

	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;

	gc0328_attr.dvp.gpio = sensor_gpio_func;
	gc0328_attr.max_again = 130330;
	gc0328_attr.max_dgain = 0;
	gc0328_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &gc0328_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &gc0328_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("@@@@@@@ probe ok ------->gc0328\n");

	return 0;

err_get_mclk:
	kfree(sensor);
err_set_sensor_gpio:
	clk_disable(sensor->mclk);
	clk_put(sensor->mclk);

	return -1;
}

static int gc0328_remove(struct i2c_client *client)
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

static const struct i2c_device_id gc0328_id[] = {
	{ "gc0328", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc0328_id);

static struct i2c_driver gc0328_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "gc0328",
	},
	.probe		= gc0328_probe,
	.remove		= gc0328_remove,
	.id_table	= gc0328_id,
};

static __init int init_gc0328(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init gc0328 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&gc0328_driver);
}

static __exit void exit_gc0328(void)
{
	private_i2c_del_driver(&gc0328_driver);
}

module_init(init_gc0328);
module_exit(exit_gc0328);

MODULE_DESCRIPTION("A low-level driver for Gcoreinc gc0328 sensors");
MODULE_LICENSE("GPL");
