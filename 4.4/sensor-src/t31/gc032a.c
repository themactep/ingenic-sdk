// SPDX-License-Identifier: GPL-2.0+
/*
 * gc032a.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 */

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
#include <sensor-info.h>
#include <txx-funcs.h>

#define SENSOR_NAME "gc032a"
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x21
#define SENSOR_CHIP_ID 0x9b
#define SENSOR_CHIP_ID_H (0x23)
#define SENSOR_CHIP_ID_L (0x2a)
#define SENSOR_REG_END 0x00
#define SENSOR_REG_DELAY 0xff
#define SENSOR_SUPPORT_PCLK (16970760)
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_2
#define SENSOR_VERSION "H20200116a"

static struct sensor_info sensor_info = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.version = SENSOR_VERSION,
	.min_fps = SENSOR_OUTPUT_MIN_FPS,
	.max_fps = SENSOR_OUTPUT_MAX_FPS,
	.chip_i2c_addr = SENSOR_I2C_ADDRESS,
	.width = SENSOR_MAX_WIDTH,
	.height = SENSOR_MAX_HEIGHT,
};

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};
static int reset_gpio = -1;
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = GPIO_PA(11);
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_LOW_8BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x20, 0},
	{0x21, 2909},
	{0x22, 5731},
	{0x23, 8472},
	{0x24, 11136},
	{0x25, 13726},
	{0x26, 16248},
	{0x27, 18704},
	{0x28, 21097},
	{0x29, 23432},
	{0x2a, 25710},
	{0x2b, 27935},
	{0x2c, 30109},
	{0x2d, 32234},
	{0x2e, 34312},
	{0x2f, 36345},
	{0x30, 38336},
	{0x31, 40285},
	{0x32, 42195},
	{0x33, 44068},
	{0x34, 45904},
	{0x35, 47704},
	{0x36, 49472},
	{0x37, 51207},
	{0x38, 52910},
	{0x39, 54584},
	{0x3a, 56228},
	{0x3b, 57844},
	{0x3c, 59433},
	{0x3d, 60996},
	{0x3e, 62534},
	{0x3f, 64047},
	{0x40, 65536},
	{0x41, 67001},
	{0x42, 68445},
	{0x43, 69867},
	{0x44, 71267},
	{0x45, 72648},
	{0x46, 74008},
	{0x47, 75349},
	{0x48, 76672},
	{0x49, 77976},
	{0x4a, 79262},
	{0x4b, 80531},
	{0x4c, 81784},
	{0x4d, 83020},
	{0x4e, 84240},
	{0x4f, 85444},
	{0x50, 86633},
	{0x51, 87808},
	{0x52, 88968},
	{0x53, 90114},
	{0x54, 91246},
	{0x55, 92365},
	{0x56, 93471},
	{0x57, 94564},
	{0x58, 95645},
	{0x59, 96713},
	{0x5a, 97770},
	{0x5b, 98814},
	{0x5c, 99848},
	{0x5d, 100870},
	{0x5e, 101881},
	{0x5f, 102882},
	{0x60, 103872},
	{0x61, 104851},
	{0x62, 105821},
	{0x63, 106781},
	{0x64, 107731},
	{0x65, 108672},
	{0x66, 109604},
	{0x67, 110526},
	{0x68, 111440},
	{0x69, 112344},
	{0x6a, 113240},
	{0x6b, 114128},
	{0x6c, 115008},
	{0x6d, 115879},
	{0x6e, 116743},
	{0x6f, 117598},
	{0x70, 118446},
	{0x71, 119287},
	{0x72, 120120},
	{0x73, 120946},
	{0x74, 121764},
	{0x75, 122576},
	{0x76, 123380},
	{0x77, 124178},
	{0x78, 124969},
	{0x79, 125754},
	{0x7a, 126532},
	{0x7b, 127304},
	{0x7c, 128070},
	{0x7d, 128829},
	{0x7e, 129583},
	{0x7f, 130330},
	{0x80, 131072},
	{0x81, 131807},
	{0x82, 132537},
	{0x83, 133262},
	{0x84, 133981},
	{0x85, 134694},
	{0x86, 135403},
	{0x87, 136106},
	{0x88, 136803},
	{0x89, 137496},
	{0x8a, 138184},
	{0x8b, 138866},
	{0x8c, 139544},
	{0x8d, 140217},
	{0x8e, 140885},
	{0x8f, 141549},
	{0x90, 142208},
	{0x91, 142862},
	{0x92, 143512},
	{0x93, 144157},
	{0x94, 144798},
	{0x95, 145435},
	{0x96, 146067},
	{0x97, 146696},
	{0x98, 147320},
	{0x99, 147940},
	{0x9a, 148556},
	{0x9b, 149168},
	{0x9c, 149776},
	{0x9d, 150380},
	{0x9e, 150980},
	{0x9f, 151577},
	{0xa0, 152169},
	{0xa1, 152758},
	{0xa2, 153344},
	{0xa3, 153926},
	{0xa4, 154504},
	{0xa5, 155079},
	{0xa6, 155650},
	{0xa7, 156218},
	{0xa8, 156782},
	{0xa9, 157344},
	{0xaa, 157901},
	{0xab, 158456},
	{0xac, 159007},
	{0xad, 159555},
	{0xae, 160100},
	{0xaf, 160642},
	{0xb0, 161181},
	{0xb1, 161716},
	{0xb2, 162249},
	{0xb3, 162779},
	{0xb4, 163306},
	{0xb5, 163829},
	{0xb6, 164350},
	{0xb7, 164868},
	{0xb8, 165384},
	{0xb9, 165896},
	{0xba, 166406},
	{0xbb, 166913},
	{0xbc, 167417},
	{0xbd, 167919},
	{0xbe, 168418},
	{0xbf, 168914},
	{0xc0, 169408},
	{0xc1, 169899},
	{0xc2, 170387},
	{0xc3, 170873},
	{0xc4, 171357},
	{0xc5, 171838},
	{0xc6, 172317},
	{0xc7, 172793},
	{0xc8, 173267},
	{0xc9, 173739},
	{0xca, 174208},
	{0xcb, 174675},
	{0xcc, 175140},
	{0xcd, 175602},
	{0xce, 176062},
	{0xcf, 176520},
	{0xd0, 176976},
	{0xd1, 177429},
	{0xd2, 177880},
	{0xd3, 178329},
	{0xd4, 178776},
	{0xd5, 179221},
	{0xd6, 179664},
	{0xd7, 180105},
	{0xd8, 180544},
	{0xd9, 180981},
	{0xda, 181415},
	{0xdb, 181848},
	{0xdc, 182279},
	{0xdd, 182707},
	{0xde, 183134},
	{0xdf, 183559},
	{0xe0, 183982},
	{0xe1, 184403},
	{0xe2, 184823},
	{0xe3, 185240},
	{0xe4, 185656},
	{0xe5, 186070},
	{0xe6, 186482},
	{0xe7, 186892},
	{0xe8, 187300},
	{0xe9, 187707},
	{0xea, 188112},
	{0xeb, 188515},
	{0xec, 188916},
	{0xed, 189316},
	{0xee, 189714},
	{0xef, 190111},
	{0xf0, 190505},
	{0xf1, 190899},
	{0xf2, 191290},
	{0xf3, 191680},
	{0xf4, 192068},
	{0xf5, 192455},
	{0xf6, 192840},
	{0xf7, 193224},
	{0xf8, 193606},
	{0xf9, 193986},
	{0xfa, 194365},
	{0xfb, 194743},
	{0xfc, 195119},
	{0xfd, 195493},
	{0xfe, 195866},
	{0xff, 196237},

};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;

	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == sensor_attr.max_again) {
			*sensor_again = lut->value;
			return lut->gain;
		}
		else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else {
			if (lut->gain == sensor_attr.max_again) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x9b,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
	},
	.expo_fs = 1,
	.max_again = 195866,
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
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_640_480[] = {
	{0xfe, 0xf0},
	{0xf3, 0xff},
	{0xf5, 0x06},
	{0xf7, 0x01},
	{0xf8, 0x03},
	{0xf9, 0xce},
	{0xfa, 0x00},
	{0xfc, 0x02},
	{0xfe, 0x02},
	{0x81, 0x03},
	{0xfe, 0x00},
	{0x77, 0x64},
	{0x78, 0x40},
	{0x79, 0x60},
	/*ANALOG & CISCTL*/
	{0xfe, 0x00},
	{0x03, 0x01},
	{0x04, 0xce},
	{0x05, 0x01},
	{0x06, 0xad},
	{0x07, 0x00},
	{0x08, 0x10},
	{0x0a, 0x00},
	{0x0c, 0x00},
	{0x0d, 0x01},
	{0x0e, 0xe8},
	{0x0f, 0x02},
	{0x10, 0x88},
	{0x17, 0x54},
	{0x19, 0x08},
	{0x1a, 0x0a},
	{0x1f, 0x40},
	{0x20, 0x30},
	{0x2e, 0x80},
	{0x2f, 0x2b},
	{0x30, 0x1a},
	{0xfe, 0x02},
	{0x03, 0x02},
	{0x05, 0xd7},
	{0x06, 0x60},
	{0x08, 0x80},
	{0x12, 0x89},
	/* BLK*/
	{0xfe, 0x00},
	{0x18, 0x02},
	{0xfe, 0x02},
	{0x40, 0x22},
	{0x45, 0x00},
	{0x46, 0x00},
	{0x49, 0x20},
	{0x4b, 0x3c},
	{0x50, 0x20},
	{0x42, 0x10},
	/*ISP*/
	{0xfe, 0x01},
	{0x0a, 0xc5},
	{0x45, 0x00},
	{0xfe, 0x00},
	{0x40, 0x00},
	{0x41, 0x00},
	{0x42, 0x00},
	{0x43, 0x00},
	{0x44, 0x89},//raw 输出
	{0x46, 0x22},
	{0x49, 0x03},
	{0x52, 0x02},
	{0x54, 0x00},
	{0xfe, 0x02},
	{0x22, 0xf6},
	/*Gain*/
	{0xfe, 0x00},
	{0x70, 0x50},
	/*AEC*/
	{0xfe, 0x00},
	{0x4f, 0x00},
	{SENSOR_REG_END,  0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 640,
		.height = 480,
		.fps = 25 << 16|1,
		.mbus_code = V4L2_MBUS_FMT_SRGGB8_1X8,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_640_480,
	}
};

/*
 * the part of driver was fixed.
 */

static struct regval_list sensor_stream_on[] = {
	{0xfe, 0x00},
	{0x44, 0xb8},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	{0xfe, 0x00},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, unsigned char reg,
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
		}
	};
	int ret;

	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int sensor_write(struct tx_isp_subdev *sd, unsigned char reg,
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


static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sensor_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
			//			if (vals->reg_num == SENSOR_PAGE_REG) {
			//				val &= 0xf8;
			//				val |= (vals->value & 0x07);
			//				ret = sensor_write(sd, vals->reg_num, val);
			//				ret = sensor_read(sd, vals->reg_num, &val);
			//			}
		}
		vals++;
	}
	return 0;
}
static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = sensor_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int sensor_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sensor_write(sd, 0xfe, 0x00);
	ret = sensor_read(sd, 0xf0, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;

	ret = sensor_read(sd, 0xf1, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;


	*ident = v;
	return 0;
}

static int ag_last = -1;
static int it_last = -1;
static int sensor_set_expo(struct tx_isp_subdev* sd, int value)
{
	int ret = 0;
	int expo = (value & 0x0000ffff);
	int again = (value & 0xffff0000) >> 16;

	ret = sensor_write(sd, 0xfe, 0x00);
	if (it_last != expo)
	{
		ret = sensor_write(sd, 0x4, expo & 0xff);
		ret = sensor_write(sd, 0x3, (expo & 0x1f00)>>8);
	}
	if (ag_last != again)
	{
		ret = sensor_write(sd, 0x71, (unsigned short)again);
	}

	return ret;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = sensor_write(sd, 0xfe, 0x00);
	ret = sensor_write(sd, 0x4, value & 0xff);
	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d\n" ,__LINE__);
		return ret;
	}
	ret = sensor_write(sd, 0x3, (value & 0x1f00)>>8);
	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = sensor_write(sd, 0xfe, 0x00);
	ret = sensor_write(sd, 0x71, (unsigned short)value);
	if (ret < 0) {
		ISP_ERROR("sensor_write error  %d" ,__LINE__ );
		return ret;
	}
	return 0;
}

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];
	int ret = 0;
	if (!enable)
		return ISP_SUCCESS;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = sensor_write_array(sd, wsize->regs);
	if (ret)
		return ret;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;
	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	if (enable) {
		ret = sensor_write_array(sd, sensor_stream_on);
		pr_debug("%s stream on\n", SENSOR_NAME);
	}
	else {
		ret = sensor_write_array(sd, sensor_stream_off);
		pr_debug("%s stream off\n", SENSOR_NAME);
	}
	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int pclk = SENSOR_SUPPORT_PCLK;
	unsigned int newformat = 0; //the format is 24.8
	unsigned short vts = 0;
	unsigned short hts = 0;
	unsigned short hb = 0;
	unsigned short vb = 0;
	int ret = 0;
	unsigned char tmp;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_WARNING("set_fps error ,should be %d  ~ %d \n",SENSOR_OUTPUT_MIN_FPS, SENSOR_OUTPUT_MAX_FPS);
		return -1;
	}

	ret = sensor_write(sd, 0xfe, 0x00);
	ret = sensor_read(sd, 0x05, &tmp);
	hb = tmp & 0x0f;
	ret += sensor_read(sd, 0x06, &tmp);
	hb = (hb << 8) | tmp;
	ret += sensor_read(sd, 0x11, &tmp);
	hb += tmp;

	//	vts = vb + 488;
	hts = hb + 652;

	vts = pclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	vb = vts - 488;

	ret = sensor_write(sd, 0x08, (unsigned char)(vb & 0x00ff));
	ret += sensor_read(sd, 0x07, &tmp);
	tmp = ((unsigned char)((vb & 0x0f00) >> 8)) | (tmp & 0x0f);
	ret += sensor_write(sd, 0x07, tmp);

	if (ret < 0)
		return -1;

	sensor->video.fps = fps;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->max_integration_time_native = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;
	wsize = &sensor_win_sizes[0];

	if (wsize) {
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
static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
		struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}

	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sensor_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an %s chip.\n",
			  client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}
	ISP_WARNING("%s chip found @ 0x%02x (%s)\n",
		    SENSOR_NAME, client->addr, client->adapter->name);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}
static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
#if 1
		case TX_ISP_EVENT_SENSOR_EXPO:
			if (arg)
				ret = sensor_set_expo(sd, *(int*)arg);
			break;
#else
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			if (arg)
				ret = sensor_set_integration_time(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			if (arg)
				ret = sensor_set_analog_gain(sd, *(int*)arg);
			break;
#endif
		case TX_ISP_EVENT_SENSOR_DGAIN:
			if (arg)
				ret = sensor_set_digital_gain(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
			if (arg)
				ret = sensor_get_black_pedestal(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if (arg)
				ret = sensor_set_mode(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
			ret = sensor_write_array(sd, sensor_stream_off);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			ret = sensor_write_array(sd, sensor_stream_on);
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if (arg)
				ret = sensor_set_fps(sd, *(int*)arg);
			break;
		default:
			break;
	}
	return 0;
}

static int sensor_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = sensor_write(sd, 0xfe, 0x00);
	ret = sensor_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int sensor_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sensor_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}

static struct tx_isp_subdev_core_ops sensor_core_ops = {
	.g_chip_ident = sensor_g_chip_ident,
	.reset = sensor_reset,
	.init = sensor_init,
	.g_register = sensor_g_register,
	.s_register = sensor_s_register,
};

static struct tx_isp_subdev_video_ops sensor_video_ops = {
	.s_stream = sensor_s_stream,
};

static struct tx_isp_subdev_sensor_ops sensor_sensor_ops = {
	.ioctl = sensor_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.sensor = &sensor_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = SENSOR_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sensor_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret;
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

	it_last = -1;
	ag_last = -1;
	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
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
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;

	sensor_attr.dvp.gpio = sensor_gpio_func;
	sensor_attr.max_again = 195866;
	sensor_attr.max_dgain = 0;
	sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sensor_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

	return 0;

err_get_mclk:
	kfree(sensor);
err_set_sensor_gpio:
	clk_disable(sensor->mclk);
	clk_put(sensor->mclk);

	return -1;
}

static int sensor_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);
	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
	},
	.probe = sensor_probe,
	.remove = sensor_remove,
	.id_table = sensor_id,
};

static __init int init_sensor(void)
{
	int ret = 0;
	sensor_common_init(&sensor_info);

	ret = private_driver_get_interface();
	if (ret) {
		ISP_ERROR("Failed to init %s driver.\n", SENSOR_NAME);
		return -1;
	}
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	private_i2c_del_driver(&sensor_driver);
	sensor_common_exit();
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
