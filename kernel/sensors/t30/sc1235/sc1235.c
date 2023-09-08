/*
 * sc1235.c
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
#include <apical-isp/apical_math.h>
#include <soc/gpio.h>

#include <tx-isp-common.h>
#include <sensor-common.h>

#define SC1235_CHIP_ID_H	(0x12)
#define SC1235_CHIP_ID_L	(0x35)
#define SC1235_REG_END		0xffff
#define SC1235_REG_DELAY	0xfffe

#define SC1235_SUPPORT_PCLK (54*1000*1000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_2
#define SENSOR_VERSION	"H20180711a"

struct regval_list {
	unsigned short reg_num;
	unsigned char value;
};

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_12BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

struct again_lut {
	unsigned char value;
	unsigned int gain;
};
struct again_lut sc1235_again_lut[] = {
	{0x0010, 0},
	{0x0011, 5731},
	{0x0012, 11136},
	{0x0013, 16248},
	{0x0014, 21097},
	{0x0015, 25710},
	{0x0016, 30109},
	{0x0017, 34312},
	{0x0018, 38336},
	{0x0019, 42195},
	{0x001a, 45904},
	{0x001b, 49472},
	{0x001c, 52910},
	{0x001d, 56228},
	{0x001e, 59433},
	{0x001f, 62534},
	{0x0020, 65536},
	{0x0021, 68445},
	{0x0022, 71267},
	{0x0023, 74008},
	{0x0024, 76672},
	{0x0025, 79262},
	{0x0026, 81784},
	{0x0027, 84240},
	{0x0028, 86633},
	{0x0029, 88968},
	{0x002a, 91246},
	{0x002b, 93471},
	{0x002c, 95645},
	{0x002d, 97770},
	{0x002e, 99848},
	{0x002f, 101881},
	{0x0030, 103872},
	{0x0031, 105821},
	{0x0032, 107731},
	{0x0033, 109604},
	{0x0034, 111440},
	{0x0035, 113241},
	{0x0036, 115008},
	{0x0037, 116743},
	{0x0038, 118446},
	{0x0039, 120120},
	{0x003a, 121764},
	{0x003b, 123380},
	{0x003c, 124969},
	{0x003d, 126532},
	{0x003e, 128070},
	{0x003f, 129583},
	{0x0040, 131072},
	{0x0041, 132537},
	{0x0042, 133981},
	{0x0043, 135403},
	{0x0044, 136803},
	{0x0045, 138184},
	{0x0046, 139544},
	{0x0047, 140885},
	{0x0048, 142208},
	{0x0049, 143512},
	{0x004a, 144798},
	{0x004b, 146067},
	{0x004c, 147320},
	{0x004d, 148556},
	{0x004e, 149776},
	{0x004f, 150980},
	{0x0050, 152169},
	{0x0051, 153344},
	{0x0052, 154504},
	{0x0053, 155650},
	{0x0054, 156782},
	{0x0055, 157901},
	{0x0056, 159007},
	{0x0057, 160100},
	{0x0058, 161181},
	{0x0059, 162249},
	{0x005a, 163306},
	{0x005b, 164350},
	{0x005c, 165384},
	{0x005d, 166406},
	{0x005e, 167417},
	{0x005f, 168418},
	{0x0060, 169408},
	{0x0061, 170387},
	{0x0062, 171357},
	{0x0063, 172317},
	{0x0064, 173267},
	{0x0065, 174208},
	{0x0066, 175140},
	{0x0067, 176062},
	{0x0068, 176976},
	{0x0069, 177880},
	{0x006a, 178777},
	{0x006b, 179664},
	{0x006c, 180544},
	{0x006d, 181415},
	{0x006e, 182279},
	{0x006f, 183134},
	{0x0070, 183982},
	{0x0071, 184823},
	{0x0072, 185656},
	{0x0073, 186482},
	{0x0074, 187300},
	{0x0075, 188112},
	{0x0076, 188916},
	{0x0077, 189714},
	{0x0078, 190505},
	{0x0079, 191290},
	{0x007a, 192068},
	{0x007b, 192840},
	{0x007c, 193606},
	{0x007d, 194365},
	{0x007e, 195119},
	{0x007f, 195866},
	{0x0080, 196608},
	{0x0081, 197343},
	{0x0082, 198073},
	{0x0083, 198798},
	{0x0084, 199517},
	{0x0085, 200230},
	{0x0086, 200939},
	{0x0087, 201642},
	{0x0088, 202339},
	{0x0089, 203032},
	{0x008a, 203720},
	{0x008b, 204402},
	{0x008c, 205080},
	{0x008d, 205753},
	{0x008e, 206421},
	{0x008f, 207085},
	{0x0090, 207744},
	{0x0091, 208398},
	{0x0092, 209048},
	{0x0093, 209693},
	{0x0094, 210334},
	{0x0095, 210971},
	{0x0096, 211603},
	{0x0097, 212232},
	{0x0098, 212856},
	{0x0099, 213476},
	{0x009a, 214092},
	{0x009b, 214704},
	{0x009c, 215312},
	{0x009d, 215916},
	{0x009e, 216516},
	{0x009f, 217113},
	{0x00a0, 217705},
	{0x00a1, 218294},
	{0x00a2, 218880},
	{0x00a3, 219462},
	{0x00a4, 220040},
	{0x00a5, 220615},
	{0x00a6, 221186},
	{0x00a7, 221754},
	{0x00a8, 222318},
	{0x00a9, 222880},
	{0x00aa, 223437},
	{0x00ab, 223992},
	{0x00ac, 224543},
	{0x00ad, 225091},
	{0x00ae, 225636},
	{0x00af, 226178},
	{0x00b0, 226717},
	{0x00b1, 227253},
	{0x00b2, 227785},
	{0x00b3, 228315},
	{0x00b4, 228842},
	{0x00b5, 229365},
	{0x00b6, 229886},
	{0x00b7, 230404},
	{0x00b8, 230920},
	{0x00b9, 231432},
	{0x00ba, 231942},
	{0x00bb, 232449},
	{0x00bc, 232953},
	{0x00bd, 233455},
	{0x00be, 233954},
	{0x00bf, 234450},
	{0x00c0, 234944},
	{0x00c1, 235435},
	{0x00c2, 235923},
	{0x00c3, 236410},
	{0x00c4, 236893},
	{0x00c5, 237374},
	{0x00c6, 237853},
	{0x00c7, 238329},
	{0x00c8, 238803},
	{0x00c9, 239275},
	{0x00ca, 239744},
	{0x00cb, 240211},
	{0x00cc, 240676},
	{0x00cd, 241138},
	{0x00ce, 241598},
	{0x00cf, 242056},
	{0x00d0, 242512},
	{0x00d1, 242965},
	{0x00d2, 243416},
	{0x00d3, 243865},
	{0x00d4, 244313},
	{0x00d5, 244757},
	{0x00d6, 245200},
	{0x00d7, 245641},
	{0x00d8, 246080},
	{0x00d9, 246517},
	{0x00da, 246951},
	{0x00db, 247384},
	{0x00dc, 247815},
	{0x00dd, 248243},
	{0x00de, 248670},
	{0x00df, 249095},
	{0x00e0, 249518},
	{0x00e1, 249939},
	{0x00e2, 250359},
	{0x00e3, 250776},
	{0x00e4, 251192},
	{0x00e5, 251606},
	{0x00e6, 252018},
	{0x00e7, 252428},
	{0x00e8, 252836},
	{0x00e9, 253243},
	{0x00ea, 253648},
	{0x00eb, 254051},
	{0x00ec, 254452},
	{0x00ed, 254852},
	{0x00ee, 255250},
	{0x00ef, 255647},
	{0x00f0, 256041},
	{0x00f1, 256435},
	{0x00f2, 256826},
	{0x00f3, 257216},
	{0x00f4, 257604},
	{0x00f5, 257991},
	{0x00f6, 258376},
	{0x00f7, 258760},
	{0x00f8, 259142},
	/* {0x00f9, 259522}, */
	/* {0x00fa, 259901}, */
	/* {0x00fb, 260279}, */
	/* {0x00fc, 260655}, */
	/* {0x00fd, 261029}, */
	/* {0x00fe, 261402}, */
	/* {0x00ff, 261773}, */
	/* {0x0100, 262144} */
};

struct tx_isp_sensor_attribute sc1235_attr;

unsigned int sc1235_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc1235_again_lut;
	while(lut->gain <= sc1235_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == sc1235_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc1235_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}
struct tx_isp_sensor_attribute sc1235_attr={
	.name = "sc1235",
	.chip_id = 0x1235,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
	},
	.max_again = 256410,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 1196,
	.integration_time_limit = 1196,
	.total_width = 1800,
	.total_height = 1200,
	.max_integration_time = 1196,
	//.one_line_expr_in_us = 33,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc1235_alloc_again,
	.sensor_ctrl.alloc_dgain = sc1235_alloc_dgain,
	//void priv; /* point to struct tx_isp_sensor_board_info */
};


static struct regval_list sc1235_init_regs_1280_960_25fps[] = {

	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x3635, 0xa0},
	{0x3305, 0x00},
	{0x363a, 0x1f},
	{0x363b, 0x09},
	{0x33b5, 0x10},
	{0x4500, 0x59},
	{0x3211, 0x08},
	{0x335c, 0x57},
	{0x3d08, 0x01},
	{0x3621, 0x28},
	{0x3303, 0x28},
	{0x333a, 0x0a},
	{0x3908, 0x11},
	{0x3366, 0x7c},
	{0x3e08, 0x03},
	{0x3e09, 0x10},
	{0x3636, 0x25},
	{0x3625, 0x01},
	{0x320c, 0x07},
	{0x320d, 0x08},
	{0x391e, 0x00},
	{0x3034, 0x05},
	{0x330a, 0x01},
	{0x3634, 0x21},
	{0x3e01, 0x3e},
	{0x3364, 0x05},
	{0x363c, 0x06},
	{0x3637, 0x0e},
	{0x335e, 0x01},
	{0x335f, 0x03},
	{0x337c, 0x04},
	{0x337d, 0x06},
	{0x33a0, 0x05},
	{0x3301, 0x05},
	{0x3302, 0xff},
	{0x3633, 0x2f},
	{0x330b, 0x6c},
	{0x3638, 0x0f},
	{0x3306, 0x68},
	{0x366e, 0x08},
	{0x366f, 0x2f},
	{0x3e23, 0x07},
	{0x3e24, 0x10},
	{0x331d, 0x0a},
	{0x333b, 0x00},
	{0x3357, 0x5a},
	{0x3309, 0xa8},
	{0x331f, 0x8d},
	{0x3321, 0x8f},
	{0x3631, 0x84},
	{0x3038, 0xff},
	{0x391b, 0x4d},
	{0x337f, 0x03},
	{0x3368, 0x02},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x3367, 0x08},
	{0x330e, 0x30},
	{0x303a, 0xbe},
	{0x3035, 0xba},
	{0x3213, 0x02},
	{0x3802, 0x01},
	{0x3235, 0x03},
	{0x3236, 0xe6},
	{0x3f00, 0x07},
	{0x3f04, 0x06},
	{0x3f05, 0xe4},
	{0x5780, 0xff},
	{0x5781, 0x04},
	{0x5785, 0x18},
	{0x3622, 0x06},
	{0x3630, 0x28},
	{0x3313, 0x05},
	{0x3639, 0x09},
	{0x3e03, 0x03},
	{0x3e08, 0x00},
#ifdef  DRIVE_CAPABILITY_1
	{0x3640,0x00},//drv
#elif defined(DRIVE_CAPABILITY_2)
	{0x3640,0x01},
#endif
	{0x3641,0x01},
	{0x320e, 0x04},
	{0x320f, 0xb0},
	{SC1235_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the sc1235_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc1235_win_sizes[] = {
	/* 1280*960 */
	{
		.width		= 1280,
		.height		= 960,
		.fps		= 25 << 16 | 1, /* 25 fps */
		.mbus_code	= V4L2_MBUS_FMT_SBGGR12_1X12,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc1235_init_regs_1280_960_25fps,
	}
};

static enum v4l2_mbus_pixelcode sc1235_mbus_code[] = {
	V4L2_MBUS_FMT_SBGGR10_1X10,
	V4L2_MBUS_FMT_SBGGR12_1X12,
};

/*
 * the part of driver was fixed.
 */

static struct regval_list sc1235_stream_on[] = {

	{0x0100, 0x01},
	{SC1235_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc1235_stream_off[] = {

	{0x0100,0x00},
	{SC1235_REG_END, 0x00},	/* END MARKER */
};

int sc1235_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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

int sc1235_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg>>8)&0xff, reg&0xff, value};
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

static int sc1235_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC1235_REG_END) {
		if (vals->reg_num == SC1235_REG_DELAY) {
				msleep(vals->value);
		} else {
			ret = sc1235_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		/*printk("read vals->reg_num:0x%02x, vals->value:0x%02x\n",vals->reg_num, val);*/
		vals++;
	}

	return 0;
}

static int sc1235_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC1235_REG_END) {
		if (vals->reg_num == SC1235_REG_DELAY) {
				msleep(vals->value);
		} else {
			ret = sc1235_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		/*printk("write vals->reg_num:%x, vals->value:%x\n",vals->reg_num, vals->value);*/
		vals++;
	}

	return 0;
}

static int sc1235_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sc1235_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sc1235_read(sd, 0x3107, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC1235_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc1235_read(sd, 0x3108, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC1235_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc1235_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	unsigned int expo = value;
	int ret = 0;

	ret += sc1235_write(sd, 0x3e01, (unsigned char)((expo >> 4) & 0xff));
	ret += sc1235_write(sd, 0x3e02, (unsigned char)((expo & 0x0f) << 4));
	if(expo<0x50)
		ret += sc1235_write(sd, 0x3314, 0x12);
	else if(expo>0xa0)
		ret += sc1235_write(sd, 0x3314, 0x02);

	if (ret < 0)
		return ret;

	return 0;
}

static int sc1235_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret += sc1235_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sc1235_write(sd, 0x3e08, (unsigned char)((value & 0xff00) >> 8));
	if (ret < 0)
		return ret;

	/* denoise logic */
	if (value < 0x20) {
		sc1235_write(sd, 0x3812,0x00);
		sc1235_write(sd, 0x3631,0x84);
		sc1235_write(sd, 0x3301,0x0b);
		sc1235_write(sd, 0x3622,0xc6);
		sc1235_write(sd, 0x5781,0x04);
		sc1235_write(sd, 0x5785,0x18);
		sc1235_write(sd, 0x3812,0x30);

	}else if(value>=0x20 &&value <0x40){
		sc1235_write(sd, 0x3812,0x00);
		sc1235_write(sd, 0x3631,0x88);
		sc1235_write(sd, 0x3301,0x1f);
		sc1235_write(sd, 0x3622,0x06);
		sc1235_write(sd, 0x5781,0x04);
		sc1235_write(sd, 0x5785,0x18);
		sc1235_write(sd, 0x3812,0x30);

	}else if(value>=0x40 &&value <0xa0){
		sc1235_write(sd, 0x3812,0x00);
		sc1235_write(sd, 0x3631,0x88);
		sc1235_write(sd, 0x3301,0xff);
		sc1235_write(sd, 0x3622,0x06);
		sc1235_write(sd, 0x5781,0x04);
		sc1235_write(sd, 0x5785,0x18);
		sc1235_write(sd, 0x3812,0x30);

	}else{
		sc1235_write(sd, 0x3812,0x00);
		sc1235_write(sd, 0x3631,0x88);
		sc1235_write(sd, 0x3301,0xff);
		sc1235_write(sd, 0x3622,0x06);
		sc1235_write(sd, 0x5781,0x02);
		sc1235_write(sd, 0x5785,0x18);
		sc1235_write(sd, 0x3812,0x30);
	}

	if (ret < 0)
		return ret;

	return 0;
}

static int sc1235_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	/* 0x00 bit[7] if gain > 2X set 0; if gain > 4X set 1 */
	return 0;
}

static int sc1235_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc1235_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = &sc1235_win_sizes[0];
	int ret = 0;

	if(!enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = sc1235_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc1235_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = sc1235_write_array(sd, sc1235_stream_on);
		pr_debug("sc1235 stream on\n");
	}
	else {
		ret = sc1235_write_array(sd, sc1235_stream_off);
		pr_debug("sc1235 stream off\n");
	}
	return ret;
}

static int sc1235_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int pclk = SC1235_SUPPORT_PCLK;
	unsigned short hts;
	unsigned short vts = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8))
		return -1;
	ret += sc1235_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sc1235_read(sd, 0x320d, &tmp);
	if(ret < 0)
		return -1;
	hts = (hts << 8) + tmp;
	vts = pclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = sc1235_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc1235_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if(ret < 0)
		return -1;
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sc1235_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;
	if(value == TX_ISP_SENSOR_FULL_RES_MAX_FPS){
		wsize = &sc1235_win_sizes[0];
	}else if(value == TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS){
		wsize = &sc1235_win_sizes[0];
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

static int sc1235_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	char val = 0;

	val = enable ? 0x60 : 0;
	ret += sc1235_write(sd, 0x3221, val);
	sensor->video.mbus_change = 0;
	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sc1235_g_chip_ident(struct tx_isp_subdev *sd,
		struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc1235_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}else{
			printk("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = gpio_request(pwdn_gpio, "sc1235_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(50);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		} else {
			printk("gpio requrest fail %d\n", pwdn_gpio);
		}
	}
	ret = sc1235_detect(sd, &ident);
	if (ret) {
		printk("chip found @ 0x%x (%s) is not an sc1235 chip.\n",
				client->addr, client->adapter->name);
		return ret;
	}
	printk("sc1235 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "sc1235", sizeof("sc1235"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc1235_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		printk("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			if(arg)
				ret = sc1235_set_integration_time(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			if(arg)
				ret = sc1235_set_analog_gain(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_DGAIN:
			if(arg)
				ret = sc1235_set_digital_gain(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
			if(arg)
				ret = sc1235_get_black_pedestal(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if(arg)
				ret = sc1235_set_mode(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
			ret = sc1235_write_array(sd, sc1235_stream_off);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			ret = sc1235_write_array(sd, sc1235_stream_on);
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if(arg)
				ret = sc1235_set_fps(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_VFLIP:
			if(arg)
				ret = sc1235_set_vflip(sd, *(int*)arg);
			break;
		default:
			break;;
	}
	return 0;
}

static int sc1235_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc1235_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc1235_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;
	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc1235_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc1235_core_ops = {
	.g_chip_ident = sc1235_g_chip_ident,
	.reset = sc1235_reset,
	.init = sc1235_init,
	.g_register = sc1235_g_register,
	.s_register = sc1235_s_register,
};

static struct tx_isp_subdev_video_ops sc1235_video_ops = {
	.s_stream = sc1235_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc1235_sensor_ops = {
	.ioctl	= sc1235_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc1235_ops = {
	.core = &sc1235_core_ops,
	.video = &sc1235_video_ops,
	.sensor = &sc1235_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc1235",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};


static int sc1235_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &sc1235_win_sizes[0];
	enum v4l2_mbus_pixelcode mbus;
	int i = 0;
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
	clk_set_rate(sensor->mclk, 24000000);
	clk_enable(sensor->mclk);

	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;

	sc1235_attr.dvp.gpio = sensor_gpio_func;
	switch(sensor_gpio_func){
	case DVP_PA_LOW_10BIT:
	case DVP_PA_HIGH_10BIT:
		mbus = sc1235_mbus_code[0];
		break;
	case DVP_PA_12BIT:
		mbus = sc1235_mbus_code[1];
		break;
	default:
		goto err_set_sensor_gpio;
	}

	for(i = 0; i < ARRAY_SIZE(sc1235_win_sizes); i++)
		sc1235_win_sizes[i].mbus_code = mbus;
	/*
	  convert sensor-gain into isp-gain,
	*/
	sc1235_attr.max_again = 256410;
	sc1235_attr.max_dgain = 0;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sc1235_attr;
	sensor->video.mbus_change = 0;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc1235_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("@@@@@@@probe ok ------->sc1235\n");
	return 0;
err_set_sensor_gpio:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int sc1235_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc1235_id[] = {
	{ "sc1235", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc1235_id);

static struct i2c_driver sc1235_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc1235",
	},
	.probe		= sc1235_probe,
	.remove		= sc1235_remove,
	.id_table	= sc1235_id,
};

static __init int init_sc1235(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		printk("Failed to init sc1235 dirver.\n");
		return -1;
	}

	return private_i2c_add_driver(&sc1235_driver);
}

static __exit void exit_sc1235(void)
{
	private_i2c_del_driver(&sc1235_driver);
}

module_init(init_sc1235);
module_exit(exit_sc1235);

MODULE_DESCRIPTION("A low-level driver for SmartSens sc1235 sensors");
MODULE_LICENSE("GPL");
