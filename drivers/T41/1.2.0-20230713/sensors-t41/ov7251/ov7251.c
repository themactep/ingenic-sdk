/*
 * ov7251.c
 *
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          320*240         150       mipi_2lane           linear
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

#define OV7251_CHIP_ID_H	(0x77)
#define OV7251_CHIP_ID_L	(0x50)
#define OV7251_REG_END		0xffff
#define OV7251_REG_DELAY	0xfffe
#define OV7251_SUPPORT_SCLK_FPS_150 (800 * 500 * 150)
#define SENSOR_OUTPUT_MAX_FPS 150
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20220812a"

static int reset_gpio = GPIO_PC(27);
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

struct again_lut ov7251_again_lut[] = {
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
	{0x20, 45904},
	{0x21, 49472},
	{0x22, 52910},
	{0x23, 56228},
	{0x24, 59433},
	{0x25, 62534},
	{0x26, 65536},
	{0x27, 68445},
	{0x28, 71267},
	{0x29, 74008},
	{0x30, 76672},
	{0x31, 79262},
	{0x32, 81784},
	{0x33, 84240},
	{0x34, 86633},
	{0x35, 88968},
	{0x36, 91246},
	{0x37, 93471},
	{0x38, 95645},
	{0x39, 97770},
	{0x40, 99848},
	{0x41, 101881},
	{0x42, 103872},
	{0x43, 105821},
	{0x44, 107731},
	{0x45, 109604},
	{0x46, 111440},
	{0x47, 113240},
	{0x48, 115008},
	{0x49, 116743},
	{0x50, 118446},
	{0x51, 120120},
	{0x52, 121764},
	{0x53, 123380},
	{0x54, 124969},
	{0x55, 126532},
	{0x56, 128070},
	{0x57, 129583},
	{0x58, 131072},
	{0x59, 132537},
	{0x60, 133981},
	{0x61, 135403},
	{0x62, 136803},
	{0x63, 138184},
	{0x64, 139544},
	{0x65, 140885},
	{0x66, 142208},
	{0x67, 143512},
	{0x68, 144798},
	{0x69, 146067},
	{0x70, 147320},
	{0x71, 148556},
	{0x72, 149776},
	{0x73, 150980},
	{0x74, 152169},
	{0x75, 153344},
	{0x76, 154504},
	{0x77, 155650},
	{0x78, 156782},
	{0x79, 157901},
	{0x80, 159007},
	{0x81, 160100},
	{0x82, 161181},
	{0x83, 162249},
	{0x84, 163306},
	{0x85, 164350},
	{0x86, 165384},
	{0x87, 166406},
	{0x88, 167417},
	{0x89, 168418},
	{0x90, 169408},
	{0x91, 170387},
	{0x92, 171357},
	{0x93, 172317},
	{0x94, 173267},
	{0x95, 174208},
	{0x96, 175140},
	{0x97, 176062},
	{0x98, 176976},
	{0x99, 177880},
	{0x100, 178777},
	{0x101, 179664},
	{0x102, 180544},
	{0x103, 181415},
	{0x104, 182279},
	{0x105, 183134},
	{0x106, 183982},
	{0x107, 184823},
	{0x108, 185656},
	{0x109, 186482},
	{0x110, 187300},
	{0x111, 188112},
	{0x112, 188916},
	{0x113, 189714},
	{0x114, 190505},
	{0x115, 191290},
	{0x116, 192068},
	{0x117, 192840},
	{0x118, 193606},
	{0x119, 194365},
	{0x120, 195119},
	{0x121, 195866},
	{0x122, 196608},
	{0x123, 197343},
	{0x124, 198073},
	{0x125, 198798},
	{0x126, 199517},
	{0x127, 200230},
	{0x128, 200939},
	{0x129, 201642},
	{0x130, 202339},
	{0x131, 203032},
	{0x132, 203720},
	{0x133, 204402},
	{0x134, 205080},
	{0x135, 205753},
	{0x136, 206421},
	{0x137, 207085},
	{0x138, 207744},
	{0x139, 208398},
	{0x140, 209048},
	{0x141, 209693},
	{0x142, 210334},
	{0x143, 210971},
	{0x144, 211603},
	{0x145, 212232},
	{0x146, 212856},
	{0x147, 213476},
	{0x148, 214092},
	{0x149, 214704},
	{0x150, 215312},
	{0x151, 215916},
	{0x152, 216516},
	{0x153, 217113},
	{0x154, 217705},
	{0x155, 218294},
	{0x156, 218880},
	{0x157, 219462},
	{0x158, 220040},
	{0x159, 220615},
	{0x160, 221186},
	{0x161, 221754},
	{0x162, 222318},
	{0x163, 222880},
	{0x164, 223437},
	{0x165, 223992},
	{0x166, 224543},
	{0x167, 225091},
	{0x168, 225636},
	{0x169, 226178},
	{0x170, 226717},
	{0x171, 227253},
	{0x172, 227785},
	{0x173, 228315},
	{0x174, 228842},
	{0x175, 229365},
	{0x176, 229886},
	{0x177, 230404},
	{0x178, 230920},
	{0x179, 231432},
	{0x180, 231942},
	{0x181, 232449},
	{0x182, 232953},
	{0x183, 233455},
	{0x184, 233954},
	{0x185, 234450},
	{0x186, 234944},
	{0x187, 235435},
	{0x188, 235923},
	{0x189, 236410},
	{0x190, 236893},
	{0x191, 237374},
	{0x192, 237853},
	{0x193, 238329},
	{0x194, 238803},
	{0x195, 239275},
	{0x196, 239744},
	{0x197, 240211},
	{0x198, 240676},
	{0x199, 241138},
	{0x200, 241598},
	{0x201, 242056},
	{0x202, 242512},
	{0x203, 242965},
	{0x204, 243416},
	{0x205, 243865},
	{0x206, 244313},
	{0x207, 244757},
	{0x208, 245200},
	{0x209, 245641},
	{0x210, 246080},
	{0x211, 246517},
	{0x212, 246951},
	{0x213, 247384},
	{0x214, 247815},
	{0x215, 248243},
	{0x216, 248670},
	{0x217, 249095},
	{0x218, 249518},
	{0x219, 249939},
	{0x220, 250359},
	{0x221, 250776},
	{0x222, 251192},
	{0x223, 251606},
	{0x224, 252018},
	{0x225, 252428},
	{0x226, 252836},
	{0x227, 253243},
	{0x228, 253648},
	{0x229, 254051},
	{0x230, 254452},
	{0x231, 254852},
	{0x232, 255250},
	{0x233, 255647},
	{0x234, 256041},
	{0x235, 256435},
	{0x236, 256826},
	{0x237, 257216},
	{0x238, 257604},
	{0x239, 257991},
	{0x240, 258376},
	{0x241, 258760},
	{0x242, 259142},
};

struct tx_isp_sensor_attribute ov7251_attr;

unsigned int ov7251_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = ov7251_again_lut;
	while(lut->gain <= ov7251_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == ov7251_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int ov7251_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus ov7251_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 480,
	.lans = 1,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW8,//RAW8
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.image_twidth = 320,
	.image_theight = 240,
	.mipi_sc.mipi_crop_start0x = 0,
	.mipi_sc.mipi_crop_start0y = 0,
	.mipi_sc.mipi_crop_start1x = 0,
	.mipi_sc.mipi_crop_start1y = 0,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW8,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute ov7251_attr={
	.name = "ov7251",
	.chip_id = 0x7750,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x60,
	.max_again = 259142,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = ov7251_alloc_again,
	.sensor_ctrl.alloc_dgain = ov7251_alloc_dgain,
};

static struct regval_list ov7251_init_regs_320_240_150fps_mipi[] = {
		{0x0103,0x01},
        {0x0100,0x00},
        {0x3005,0x00},
        {0x3012,0xc0},
        {0x3013,0xd2},
        {0x3014,0x04},
        {0x3016,0x10},
        {0x3017,0x00},
        {0x3018,0x10},
        {0x301a,0x00},
        {0x301b,0x00},
        {0x301c,0x20},
        {0x3023,0x07},
        {0x3037,0xf0},
        {0x3098,0x04},
        {0x3099,0x32},
        {0x309a,0x05},
        {0x309b,0x04},
        {0x30b0,0x08},
        {0x30b1,0x01},
        {0x30b3,0x50},
        {0x30b4,0x04},
        {0x30b5,0x05},
        {0x3106,0xda},
        {0x3500,0x00},
        {0x3501,0x1d},
        {0x3502,0xe0},
        {0x3503,0x07},
        {0x3509,0x10},
        {0x350b,0x70},
        {0x3600,0x1c},
        {0x3602,0x62},
        {0x3620,0xb7},
        {0x3622,0x04},
        {0x3626,0x21},
        {0x3627,0x30},
        {0x3634,0x41},
        {0x3636,0x00},
        {0x3662,0x03},
        {0x3663,0x70},
        {0x3664,0xf0},
        {0x3666,0x0a},
        {0x3669,0x1a},
        {0x366a,0x00},
        {0x366b,0x50},
        {0x3673,0x01},
        {0x3674,0xff},
        {0x3675,0x03},
        {0x3705,0x41},
        {0x3709,0x40},
        {0x373c,0xe8},
        {0x3742,0x00},
        {0x3788,0x00},
        {0x37a8,0x02},
        {0x37a9,0x14},
        {0x3800,0x00},
        {0x3801,0x00},
        {0x3802,0x00},
        {0x3803,0x00},
        {0x3804,0x02},
        {0x3805,0x8f},
        {0x3806,0x01},
        {0x3807,0xef},
        {0x3808,0x01},
        {0x3809,0x40},
        {0x380a,0x00},
        {0x380b,0xf0},
        {0x380c,0x03},//
        {0x380d,0x20},//0x320 = 800
        {0x380e,0x01},//
        {0x380f,0xf4},//0x1f4 = 500
        {0x3810,0x00},
        {0x3811,0x04},
        {0x3812,0x00},
        {0x3813,0x05},
        {0x3814,0x31},
        {0x3815,0x31},
        {0x3820,0x42},
        {0x3821,0x01},
        {0x382f,0x0e},
        {0x3832,0x00},
        {0x3833,0x05},
        {0x3834,0x00},
        {0x3835,0x0c},
        {0x3837,0x0f},
        {0x3b80,0x00},
        {0x3b81,0xa5},
        {0x3b82,0x10},
        {0x3b83,0x00},
        {0x3b84,0x08},
        {0x3b85,0x00},
        {0x3b86,0x01},
        {0x3b87,0x00},
        {0x3b88,0x00},
        {0x3b89,0x00},
        {0x3b8a,0x00},
        {0x3b8b,0x05},
        {0x3b8c,0x00},
        {0x3b8d,0x00},
        {0x3b8e,0x00},
        {0x3b8f,0x1a},
        {0x3b94,0x05},
        {0x3b95,0xf2},
        {0x3b96,0x40},
        {0x3c00,0x89},
        {0x3c01,0xab},
        {0x3c02,0x01},
        {0x3c03,0x00},
        {0x3c04,0x00},
        {0x3c05,0x03},
        {0x3c06,0x00},
        {0x3c07,0x05},
        {0x3c0c,0x00},
        {0x3c0d,0x00},
        {0x3c0e,0x00},
        {0x3c0f,0x00},
        {0x4001,0xc0},
        {0x4004,0x02},
        {0x4005,0x20},
        {0x404e,0x01},
        {0x4241,0x00},
        {0x4242,0x00},
        {0x4300,0xff},
        {0x4301,0x00},
        {0x4501,0x48},
        {0x4600,0x00},
        {0x4601,0x28},
        {0x4801,0x0f},
        {0x4806,0x0f},
        {0x4819,0xaa},
        {0x4823,0x3e},
        {0x4837,0x21},
        {0x4a0d,0x00},
        {0x4a47,0x7f},
        {0x4a49,0xf0},
        {0x4a4b,0x30},
        {0x5000,0x85},
        {0x5001,0x80},
        {0x3600,0x7c},
        {0x3757,0xb3},
        {0x3630,0x44},
        {0x3634,0x60},
        {0x3631,0x35},
        {0x3600,0x1c},
        {0x0100,0x01},
		{OV7251_REG_END, 0x00},/* END MARKER */
};

static struct tx_isp_sensor_win_setting ov7251_win_sizes[] = {
	{
		.width		= 320,
		.height		= 240,
		.fps		= 150 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR8_1X8,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= ov7251_init_regs_320_240_150fps_mipi,
	}
};
struct tx_isp_sensor_win_setting *wsize = &ov7251_win_sizes[0];

static struct regval_list ov7251_stream_on_mipi[] = {
	{0x0100, 0x01},
	{OV7251_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov7251_stream_off_mipi[] = {
	{0x0100, 0x00},
	{OV7251_REG_END, 0x00},	/* END MARKER */
};

int ov7251_read(struct tx_isp_subdev *sd, uint16_t reg,
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

int ov7251_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int ov7251_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != OV7251_REG_END) {
		if (vals->reg_num == OV7251_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = ov7251_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int ov7251_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	//unsigned char val = 0;
	while (vals->reg_num != OV7251_REG_END) {
		if (vals->reg_num == OV7251_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = ov7251_write(sd, vals->reg_num, vals->value);
			//ret = ov7251_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int ov7251_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int ov7251_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = ov7251_read(sd, 0x300a, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OV7251_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = ov7251_read(sd, 0x300b, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OV7251_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int ov7251_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = -1;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;

	ret = ov7251_write(sd, 0x3502, (unsigned char)(it & 0xff));
	ret += ov7251_write(sd, 0x3501, (unsigned char)((it >> 8) & 0xff));
	ret += ov7251_write(sd, 0x3500, (unsigned char)((it >> 16) & 0x0f));

	ret += ov7251_write(sd, 0x350a, (unsigned char)(again >> 8) & 0x0f);
	ret += ov7251_write(sd, 0x350b, (unsigned char)(again & 0xff));

	if (ret < 0)
		return ret;
	return 0;
}

#if 0
static int ov7251_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value<<4;

	ret = ov7251_write(sd, 0x3502, (unsigned char)(expo & 0xff));
	ret += ov7251_write(sd, 0x3501, (unsigned char)((expo >> 8) & 0xff));
	ret += ov7251_write(sd, 0x3500, (unsigned char)((expo >> 16) & 0x0f));
	if (ret < 0)
		return ret;

	return 0;
}

static int ov7251_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += ov7251_write(sd, 0x350b, (unsigned char)((value & 0xff)));
	ret += ov7251_write(sd, 0x350a, (unsigned char)((value >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int ov7251_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int ov7251_get_black_pedestal(struct tx_isp_subdev *sd, int value)
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

static int ov7251_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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

static int ov7251_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	if (init->enable) {
		if(sensor->video.state == TX_ISP_MODULE_INIT){
			ret = ov7251_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if(sensor->video.state == TX_ISP_MODULE_RUNNING){

			ret = ov7251_write_array(sd, ov7251_stream_on_mipi);
			ISP_WARNING("ov7251 stream on\n");
		}
	}
	else {
		ret = ov7251_write_array(sd, ov7251_stream_off_mipi);
		ISP_WARNING("ov7251 stream off\n");
	}

	return ret;
}

static int ov7251_set_fps(struct tx_isp_subdev *sd, int fps)
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
	case 1:
		sclk = OV7251_SUPPORT_SCLK_FPS_150;
		max_fps =SENSOR_OUTPUT_MAX_FPS;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps<< 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}

	ret += ov7251_read(sd, 0x380c, &val);
	hts = val << 8;
	ret += ov7251_read(sd, 0x380d, &val);
	hts = (hts | val);

	if (0 != ret) {
		ISP_ERROR("err: ov7251 read err\n");
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = ov7251_write(sd, 0x380f, (unsigned char)(vts & 0xff));
	ret += ov7251_write(sd, 0x380e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: ov7251_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 20;
	sensor->video.attr->integration_time_limit = vts - 20;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 20;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int ov7251_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

#if 1
static int ov7251_set_hvflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val_m;
	uint8_t val_f;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	ov7251_read(sd, 0x3821, &val_m);
	ov7251_read(sd, 0x3820, &val_f);

	switch(enable) {
	case 0:
		ov7251_write(sd, 0x3221, val_m | 0x0);
		ov7251_write(sd, 0x3220, val_f | 0x0);
		break;
	case 1:
		ov7251_write(sd, 0x3221, val_m | 0x4);
		break;
	case 2:
		ov7251_write(sd, 0x3220, val_f | 0x4);
		break;
	case 3:
		ov7251_write(sd, 0x3221, val_m | 0x4);
		ov7251_write(sd, 0x3220, val_f | 0x4);
		break;
	}

	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}
#endif

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
		wsize = &ov7251_win_sizes[0];
		memcpy(&(ov7251_attr.mipi), &ov7251_mipi, sizeof(ov7251_mipi));
		ov7251_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		ov7251_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		ov7251_attr.max_integration_time_native = 500 - 20;
		ov7251_attr.integration_time_limit = 500 - 20;
		ov7251_attr.total_width = 800;
		ov7251_attr.total_height = 500;
		ov7251_attr.max_integration_time = 500 - 20;
		ov7251_attr.again = 0x70;
		ov7251_attr.integration_time = 0x1de0;
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
	}

	switch(info->video_interface){
	case TISP_SENSOR_VI_MIPI_CSI0:
		ov7251_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		ov7251_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_DVP:
		ov7251_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
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

static int ov7251_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"ov7251_reset");
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
		ret = private_gpio_request(pwdn_gpio,"ov7251_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = ov7251_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an ov7251 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("ov7251 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "ov7251", sizeof("ov7251"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int ov7251_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
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
			ret = ov7251_set_expo(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		//if(arg)
		//	ret = ov7251_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		//if(arg)
		//	ret = ov7251_set_analog_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = ov7251_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = ov7251_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = ov7251_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = ov7251_write_array(sd, ov7251_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = ov7251_write_array(sd, ov7251_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = ov7251_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = ov7251_set_hvflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int ov7251_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = ov7251_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int ov7251_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov7251_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops ov7251_core_ops = {
	.g_chip_ident = ov7251_g_chip_ident,
	.reset = ov7251_reset,
	.init = ov7251_init,
	.g_register = ov7251_g_register,
	.s_register = ov7251_s_register,
};

static struct tx_isp_subdev_video_ops ov7251_video_ops = {
	.s_stream = ov7251_s_stream,
};

static struct tx_isp_subdev_sensor_ops	ov7251_sensor_ops = {
	.ioctl	= ov7251_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops ov7251_ops = {
	.core = &ov7251_core_ops,
	.video = &ov7251_video_ops,
	.sensor = &ov7251_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "ov7251",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int ov7251_probe(struct i2c_client *client,const struct i2c_device_id *id)
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
	ov7251_attr.expo_fs = 1;
	sensor->video.shvflip = 1;
	sensor->video.attr = &ov7251_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &ov7251_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->ov7251\n");

	return 0;
}

static int ov7251_remove(struct i2c_client *client)
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

static const struct i2c_device_id ov7251_id[] = {
	{ "ov7251", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov7251_id);

static struct i2c_driver ov7251_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov7251",
	},
	.probe		= ov7251_probe,
	.remove		= ov7251_remove,
	.id_table	= ov7251_id,
};

static __init int init_ov7251(void)
{
	return private_i2c_add_driver(&ov7251_driver);
}

static __exit void exit_ov7251(void)
{
	private_i2c_del_driver(&ov7251_driver);
}

module_init(init_ov7251);
module_exit(exit_ov7251);

MODULE_DESCRIPTION("A low-level driver for ov7251 sensors");
MODULE_LICENSE("GPL");
