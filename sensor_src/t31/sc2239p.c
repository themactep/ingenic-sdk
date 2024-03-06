/*
 * sc2239p.c
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

#define SC2239P_CHIP_ID_H	(0xcb)
#define SC2239P_CHIP_ID_L	(0x3e)
#define SC2239P_REG_END		0xffff
#define SC2239P_REG_DELAY	0xfffe
#define SC2239P_SUPPORT_30FPS_SCLK (84000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 2
#define SENSOR_VERSION	"H20210819a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

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

struct again_lut sc2239p_again_lut[] = {
	{0x80, 0},
	{0x84, 2909},
	{0x88, 5731},
	{0x8c, 8472},
	{0x90, 11136},
	{0x94, 13726},
	{0x98, 16247},
	{0x9c, 18703},
	{0xa0, 21097},
	{0xa4, 23432},
	{0xa8, 25710},
	{0xac, 27935},
	{0xb0, 30108},
	{0xb4, 32233},
	{0xb8, 34311},
	{0xbc, 36344},
	{0xc0, 38335},
	{0xc4, 40285},
	{0xc8, 42195},
	{0xcc, 44067},
	{0xd0, 45903},
	{0xd4, 47704},
	{0xd8, 49471},
	{0xdc, 51206},
	{0xe0, 52910},
	{0xe4, 54583},
	{0x4080, 56097},
	{0x4084, 59006},
	{0x4088, 61829},
	{0x408c, 64569},
	{0x4090, 67233},
	{0x4094, 69823},
	{0x4098, 72345},
	{0x409c, 74801},
	{0x40a0, 77194},
	{0x40a4, 79529},
	{0x40a8, 81807},
	{0x40ac, 84032},
	{0x40b0, 86206},
	{0x40b4, 88330},
	{0x40b8, 90408},
	{0x40bc, 92442},
	{0x40c0, 94432},
	{0x40c4, 96382},
	{0x40c8, 98292},
	{0x40cc, 100164},
	{0x40d0, 102000},
	{0x40d4, 103801},
	{0x40d8, 105568},
	{0x40dc, 107303},
	{0x40e0, 109007},
	{0x40e4, 110680},
	{0x40e8, 112325},
	{0x40ec, 113941},
	{0x40f0, 115530},
	{0x40f4, 117093},
	{0x40f8, 118630},
	{0x40fc, 120143},
	{0x4880, 121632},
	{0x4884, 124541},
	{0x4888, 127364},
	{0x488c, 130104},
	{0x4890, 132768},
	{0x4894, 135358},
	{0x4898, 137880},
	{0x489c, 140336},
	{0x48a0, 142729},
	{0x48a4, 145064},
	{0x48a8, 147342},
	{0x48ac, 149567},
	{0x48b0, 151741},
	{0x48b4, 153865},
	{0x48b8, 155943},
	{0x48bc, 157977},
	{0x48c0, 159967},
	{0x48c4, 161917},
	{0x48c8, 163827},
	{0x48cc, 165699},
	{0x48d0, 167535},
	{0x48d4, 169336},
	{0x48d8, 171103},
	{0x48dc, 172838},
	{0x48e0, 174542},
	{0x48e4, 176215},
	{0x48e8, 177860},
	{0x48ec, 179476},
	{0x48f0, 181065},
	{0x48f4, 182628},
	{0x48f8, 184165},
	{0x48fc, 185678},
	{0x4980, 187167},
	{0x4984, 190076},
	{0x4988, 192899},
	{0x498c, 195639},
	{0x4990, 198303},
	{0x4994, 200893},
	{0x4998, 203415},
	{0x499c, 205871},
	{0x49a0, 208264},
	{0x49a4, 210599},
	{0x49a8, 212877},
	{0x49ac, 215102},
	{0x49b0, 217276},
	{0x49b4, 219400},
	{0x49b8, 221478},
	{0x49bc, 223512},
	{0x49c0, 225502},
	{0x49c4, 227452},
	{0x49c8, 229362},
	{0x49cc, 231234},
	{0x49d0, 233070},
	{0x49d4, 234871},
	{0x49d8, 236638},
	{0x49dc, 238373},
	{0x49e0, 240077},
	{0x49e4, 241750},
	{0x49e8, 243395},
	{0x49ec, 245011},
	{0x49f0, 246600},
	{0x49f4, 248163},
	{0x49f8, 249700},
	{0x49fc, 251213},
	{0x4b80, 252702},
	{0x4b84, 255611},
	{0x4b88, 258434},
	{0x4b8c, 261174},
	{0x4b90, 263838},
	{0x4b94, 266428},
	{0x4b98, 268950},
	{0x4b9c, 271406},
	{0x4ba0, 273799},
	{0x4ba4, 276134},
	{0x4ba8, 278412},
	{0x4bac, 280637},
	{0x4bb0, 282811},
	{0x4bb4, 284935},
	{0x4bb8, 287013},
	{0x4bbc, 289047},
	{0x4bc0, 291037},
	{0x4bc4, 292987},
	{0x4bc8, 294897},
	{0x4bcc, 296769},
	{0x4bd0, 298605},
	{0x4bd4, 300406},
	{0x4bd8, 302173},
	{0x4bdc, 303908},
	{0x4be0, 305612},
	{0x4be4, 307285},
	{0x4be8, 308930},
	{0x4bec, 310546},
	{0x4bf0, 312135},
	{0x4bf4, 313698},
	{0x4bf8, 315235},
	{0x4bfc, 316748},
	{0x4f80, 318237},
	{0x4f84, 321146},
	{0x4f88, 323969},
	{0x4f8c, 326709},
	{0x4f90, 329373},
	{0x4f94, 331963},
	{0x4f98, 334485},
	{0x4f9c, 336941},
	{0x4fa0, 339334},
	{0x4fa4, 341669},
	{0x4fa8, 343947},
	{0x4fac, 346172},
	{0x4fb0, 348346},
	{0x4fb4, 350470},
	{0x4fb8, 352548},
	{0x4fbc, 354582},
	{0x4fc0, 356572},
	{0x4fc4, 358522},
	{0x4fc8, 360432},
	{0x4fcc, 362304},
	{0x4fd0, 364140},
	{0x4fd4, 365941},
	{0x4fd8, 367708},
	{0x4fdc, 369443},
	{0x4fe0, 371147},
	{0x4fe4, 372820},
	{0x4fe8, 374465},
	{0x4fec, 376081},
	{0x4ff0, 377670},
	{0x4ff4, 379233},
	{0x4ff8, 380770},
	{0x4ffc, 382283},
	{0x5f80, 383772},
	{0x5f84, 386681},
	{0x5f88, 389504},
	{0x5f8c, 392244},
	{0x5f90, 394908},
	{0x5f94, 397498},
	{0x5f98, 400020},
	{0x5f9c, 402476},
	{0x5fa0, 404869},
	{0x5fa4, 407204},
	{0x5fa8, 409482},
	{0x5fac, 411707},
	{0x5fb0, 413881},
	{0x5fb4, 416005},
	{0x5fb8, 418083},
	{0x5fbc, 420117},
	{0x5fc0, 422107},
	{0x5fc4, 424057},
	{0x5fc8, 425967},
	{0x5fcc, 427839},
	{0x5fd0, 429675},
	{0x5fd4, 431476},
	{0x5fd8, 433243},
	{0x5fdc, 434978},
	{0x5fe0, 436682},
	{0x5fe4, 438355},
	{0x5fe8, 440000},
	{0x5fec, 441616},
	{0x5ff0, 443205},
	{0x5ff4, 444768},
	{0x5ff8, 446305},
	{0x5ffc, 447818},
};

struct tx_isp_sensor_attribute sc2239p_attr;

unsigned int sc2239p_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc2239p_again_lut;
	while(lut->gain <= sc2239p_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == sc2239p_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc2239p_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sc2239p_attr={
	.name = "sc2239p",
	.chip_id = 0xcb3e,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 405,
		.lans = 2,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 1920,
		.image_theight = 1080,
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
	.max_again = 447818,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 1600 - 5,
	.integration_time_limit = 1600 - 5,
	.total_width = 2100,
	.total_height = 1600,
	.max_integration_time = 1600 - 5,
	.one_line_expr_in_us = 28,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc2239p_alloc_again,
	.sensor_ctrl.alloc_dgain = sc2239p_alloc_dgain,
};

static struct regval_list sc2239p_init_regs_1920_1080_30fps_mipi[] = {
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x301f, 0x0d},
	{0x30b8, 0x44},
	{0x320c, 0x08},
	{0x320d, 0x34},
	{0x320e, 0x06},
	{0x320f, 0x40},
	{0x3253, 0x0c},
	{0x3281, 0x80},
	{0x3301, 0x06},
	{0x3302, 0x12},
	{0x3306, 0x84},
	{0x3309, 0x60},
	{0x330a, 0x00},
	{0x330b, 0xe4},
	{0x330d, 0x20},
	{0x3314, 0x15},
	{0x331e, 0x41},
	{0x331f, 0x51},
	{0x3320, 0x0a},
	{0x3326, 0x0e},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x335d, 0x60},
	{0x335e, 0x06},
	{0x335f, 0x08},
	{0x3364, 0x56},
	{0x337a, 0x06},
	{0x337b, 0x0e},
	{0x337c, 0x02},
	{0x337d, 0x0a},
	{0x3390, 0x03},
	{0x3391, 0x0f},
	{0x3392, 0x1f},
	{0x3393, 0x06},
	{0x3394, 0x06},
	{0x3395, 0x06},
	{0x3396, 0x48},
	{0x3397, 0x4b},
	{0x3398, 0x5f},
	{0x3399, 0x06},
	{0x339a, 0x06},
	{0x339b, 0x8e},
	{0x339c, 0x8e},
	{0x33a2, 0x04},
	{0x33a3, 0x0a},
	{0x33ad, 0x1c},
	{0x33af, 0x40},
	{0x33b1, 0x80},
	{0x33b3, 0x20},
	{0x349f, 0x02},
	{0x34a6, 0x48},
	{0x34a7, 0x4b},
	{0x34a8, 0x20},
	{0x34a9, 0x20},
	{0x34f8, 0x5f},
	{0x34f9, 0x10},
	{0x3616, 0xac},
	{0x3630, 0xc0},
	{0x3631, 0x86},
	{0x3632, 0x26},
	{0x3633, 0x32},
	{0x3637, 0x29},
	{0x363a, 0x84},
	{0x363b, 0x04},
	{0x363c, 0x08},
	{0x3641, 0x3a},
	{0x364f, 0x39},
	{0x3670, 0xce},
	{0x3674, 0xc0},
	{0x3675, 0xc0},
	{0x3676, 0xc0},
	{0x3677, 0x86},
	{0x3678, 0x8b},
	{0x3679, 0x8c},
	{0x367c, 0x4b},
	{0x367d, 0x5f},
	{0x367e, 0x4b},
	{0x367f, 0x5f},
	{0x3690, 0x42},
	{0x3691, 0x43},
	{0x3692, 0x63},
	{0x3699, 0x86},
	{0x369a, 0x92},
	{0x369b, 0xa4},
	{0x369c, 0x48},
	{0x369d, 0x4b},
	{0x36a2, 0x4b},
	{0x36a3, 0x4f},
	{0x36ea, 0x07},
	{0x36eb, 0x0c},
	{0x36ec, 0x1c},
	{0x36ed, 0x28},
	{0x370f, 0x01},
	{0x3721, 0x6c},
	{0x3722, 0x09},
	{0x3724, 0x41},
	{0x3725, 0xc4},
	{0x37b0, 0x09},
	{0x37b1, 0x09},
	{0x37b2, 0x05},
	{0x37b3, 0x48},
	{0x37b4, 0x5f},
	{0x37fa, 0x07},
	{0x37fb, 0x32},
	{0x37fc, 0x10},
	{0x37fd, 0x37},
	{0x3900, 0x19},
	{0x3901, 0x02},
	{0x3905, 0xb8},
	{0x391b, 0x82},
	{0x391c, 0x00},
	{0x391f, 0x04},
	{0x3933, 0x81},
	{0x3934, 0x4c},
	{0x393f, 0xff},
	{0x3940, 0x73},
	{0x3942, 0x01},
	{0x3943, 0x4d},
	{0x3946, 0x20},
	{0x3957, 0x86},
	{0x3e01, 0x95},
	{0x3e02, 0x60},
	{0x3e28, 0xc4},
	{0x440e, 0x02},
	{0x4501, 0xc0},
	{0x4509, 0x14},
	{0x450d, 0x11},
	{0x4518, 0x00},
	{0x451b, 0x0a},
	{0x4819, 0x06},
	{0x481b, 0x03},
	{0x481d, 0x0b},
	{0x481f, 0x02},
	{0x4821, 0x08},
	{0x4823, 0x03},
	{0x4825, 0x02},
	{0x4827, 0x03},
	{0x4829, 0x04},
	{0x501c, 0x00},
	{0x501d, 0x60},
	{0x501e, 0x00},
	{0x501f, 0x40},
	{0x5799, 0x06},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x38},
	{0x5ae3, 0x30},
	{0x5ae4, 0x28},
	{0x5ae5, 0x38},
	{0x5ae6, 0x30},
	{0x5ae7, 0x28},
	{0x5ae8, 0x3f},
	{0x5ae9, 0x34},
	{0x5aea, 0x2c},
	{0x5aeb, 0x3f},
	{0x5aec, 0x34},
	{0x5aed, 0x2c},
	{0x5aee, 0xfe},
	{0x5aef, 0x40},
	{0x5af4, 0x38},
	{0x5af5, 0x30},
	{0x5af6, 0x28},
	{0x5af7, 0x38},
	{0x5af8, 0x30},
	{0x5af9, 0x28},
	{0x5afa, 0x3f},
	{0x5afb, 0x34},
	{0x5afc, 0x2c},
	{0x5afd, 0x3f},
	{0x5afe, 0x34},
	{0x5aff, 0x2c},
	{0x36e9, 0x24},
	{0x37f9, 0x24},
	{0x0100, 0x01},

	{SC2239P_REG_END, 0x00},	/* END MARKER */
};

static struct tx_isp_sensor_win_setting sc2239p_win_sizes[] = {
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc2239p_init_regs_1920_1080_30fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sc2239p_win_sizes[0];

static struct regval_list sc2239p_stream_on_mipi[] = {
	{0x0100, 0x01},
	{SC2239P_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc2239p_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SC2239P_REG_END, 0x00},	/* END MARKER */
};

int sc2239p_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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

int sc2239p_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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

static int sc2239p_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC2239P_REG_END) {
		if (vals->reg_num == SC2239P_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2239p_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc2239p_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC2239P_REG_END) {
		if (vals->reg_num == SC2239P_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2239p_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc2239p_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sc2239p_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sc2239p_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC2239P_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc2239p_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC2239P_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc2239p_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff) * 2;
	int again = (value & 0xffff0000) >> 16;

	/*set integration time*/
	ret += sc2239p_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc2239p_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc2239p_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
	/*set analog gain*/
	ret = sc2239p_write(sd, 0x3e07, (unsigned char)(again & 0xff));
	ret += sc2239p_write(sd, 0x3e09, (unsigned char)(((again >> 8) & 0xff)));
	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int sc2239p_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
	ret += sc2239p_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc2239p_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc2239p_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));

	if (ret < 0)
		return ret;

	return 0;
}

static int sc2239p_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sc2239p_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sc2239p_write(sd, 0x3e08, (unsigned char)(((value >> 8) & 0xff)));
	if (ret < 0)
		return ret;
	gain_val = value;

	return 0;
}
#endif

static int sc2239p_set_logic(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc2239p_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc2239p_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc2239p_init(struct tx_isp_subdev *sd, int enable)
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

	ret = sc2239p_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc2239p_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2239p_write_array(sd, sc2239p_stream_on_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc2239p stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2239p_write_array(sd, sc2239p_stream_off_mipi);
		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc2239p stream off\n");
	}

	return ret;
}

static int sc2239p_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;

	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	sclk = SC2239P_SUPPORT_30FPS_SCLK;

	ret = sc2239p_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sc2239p_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("err: sc2239p read err\n");
		return ret;
	}
	hts = (hts << 8) + tmp;
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += sc2239p_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc2239p_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: sc2239p_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 5;
	sensor->video.attr->integration_time_limit = vts - 5;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 5;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sc2239p_set_mode(struct tx_isp_subdev *sd, int value)
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

static int sc2239p_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc2239p_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(15);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(20);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"sc2239p_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc2239p_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc2239p chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("sc2239p chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc2239p", sizeof("sc2239p"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc2239p_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = -1;
	unsigned char val = 0x0;

	ret += sc2239p_read(sd, 0x3221, &val);

	if(enable & 0x2)
		val |= 0x60;
	else
		val &= 0x9f;

	ret += sc2239p_write(sd, 0x3221, val);

	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sc2239p_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
	     	ret = sc2239p_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
	//	if(arg)
	//		ret = sc2239p_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
	//	if(arg)
	//		ret = sc2239p_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc2239p_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc2239p_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc2239p_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2239p_write_array(sd, sc2239p_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2239p_write_array(sd, sc2239p_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc2239p_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = sc2239p_set_vflip(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if(arg)
			ret = sc2239p_set_logic(sd, *(int*)arg);
	default:
		break;
	}

	return ret;
}

static int sc2239p_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc2239p_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc2239p_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	sc2239p_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc2239p_core_ops = {
	.g_chip_ident = sc2239p_g_chip_ident,
	.reset = sc2239p_reset,
	.init = sc2239p_init,
	/*.ioctl = sc2239p_ops_ioctl,*/
	.g_register = sc2239p_g_register,
	.s_register = sc2239p_s_register,
};

static struct tx_isp_subdev_video_ops sc2239p_video_ops = {
	.s_stream = sc2239p_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc2239p_sensor_ops = {
	.ioctl	= sc2239p_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc2239p_ops = {
	.core = &sc2239p_core_ops,
	.video = &sc2239p_video_ops,
	.sensor = &sc2239p_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc2239p",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc2239p_probe(struct i2c_client *client, const struct i2c_device_id *id)
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
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	sc2239p_attr.expo_fs = 1;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sc2239p_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc2239p_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc2239p\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int sc2239p_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc2239p_id[] = {
	{ "sc2239p", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc2239p_id);

static struct i2c_driver sc2239p_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc2239p",
	},
	.probe		= sc2239p_probe,
	.remove		= sc2239p_remove,
	.id_table	= sc2239p_id,
};

static __init int init_sc2239p(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init sc2239p dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&sc2239p_driver);
}

static __exit void exit_sc2239p(void)
{
	private_i2c_del_driver(&sc2239p_driver);
}

module_init(init_sc2239p);
module_exit(exit_sc2239p);

MODULE_DESCRIPTION("A low-level driver for SmartSens sc2239p sensors");
MODULE_LICENSE("GPL");
