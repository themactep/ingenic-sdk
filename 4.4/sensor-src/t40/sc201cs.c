// SPDX-License-Identifier: GPL-2.0+
/*
 * sc201cs.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 */

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

#define SENSOR_NAME "sc201cs"
#define SENSOR_CHIP_ID_H (0xeb)
#define SENSOR_CHIP_ID_L (0x2c)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_30FPS_SCLK (72000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20201124a"

static int reset_gpio = GPIO_PC(28);
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

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x80, 0},
	{0x82, 1500},
	{0x84, 2886},
	{0x86, 4342},
	{0x88, 5776},
	{0x8a, 7101},
	{0x8c, 8494},
	{0x8e, 9781},
	{0x90, 11136},
	{0x92, 12471},
	{0x94, 13706},
	{0x96, 15005},
	{0x98, 16287},
	{0x9a, 17474},
	{0x9c, 18723},
	{0x9e, 19879},
	{0xa0, 21097},
	{0xa2, 22300},
	{0xa4, 23413},
	{0xa6, 24587},
	{0xa8, 25746},
	{0xaa, 26820},
	{0xac, 27952},
	{0xae, 29002},
	{0xb0, 30108},
	{0xb2, 31202},
	{0xb4, 32216},
	{0xb6, 33286},
	{0xb8, 34344},
	{0xba, 35325},
	{0xbc, 36361},
	{0xbe, 37321},
	{0xc0, 38335},
	{0xc2, 39338},
	{0xc4, 40269},
	{0xc6, 41252},
	{0xc8, 42225},
	{0xca, 43128},
	{0xcc, 44082},
	{0xce, 44967},
	{0xd0, 45903},
	{0xd2, 46829},
	{0xd4, 47689},
	{0xd6, 48599},
	{0xd8, 49499},
	{0xda, 50336},
	{0xdc, 51220},
	{0xde, 52041},
	{0xe0, 52910},
	{0xe2, 53770},
	{0xe4, 54570},
	{0xe6, 55415},
	{0xe8, 56253},
	{0xea, 57032},
	{0xec, 57856},
	{0xee, 58622},
	{0xf0, 59433},
	{0xf2, 60236},
	{0xf4, 60983},
	{0xf6, 61773},
	{0xf8, 62557},
	{0xfa, 63286},
	{0xfc, 64058},
	{0xfe, 64775},
	{0x180, 65535},
	{0x182, 67035},
	{0x184, 68421},
	{0x186, 69877},
	{0x188, 71311},
	{0x18a, 72636},
	{0x18c, 74029},
	{0x18e, 75316},
	{0x190, 76671},
	{0x192, 78006},
	{0x194, 79241},
	{0x196, 80540},
	{0x198, 81822},
	{0x19a, 83009},
	{0x19c, 84258},
	{0x19e, 85414},
	{0x1a0, 86632},
	{0x1a2, 87835},
	{0x1a4, 88948},
	{0x1a6, 90122},
	{0x1a8, 91281},
	{0x1aa, 92355},
	{0x1ac, 93487},
	{0x1ae, 94537},
	{0x1b0, 95643},
	{0x1b2, 96737},
	{0x1b4, 97751},
	{0x1b6, 98821},
	{0x1b8, 99879},
	{0x1ba, 100860},
	{0x1bc, 101896},
	{0x1be, 102856},
	{0x1c0, 103870},
	{0x1c2, 104873},
	{0x1c4, 105804},
	{0x1c6, 106787},
	{0x1c8, 107760},
	{0x1ca, 108663},
	{0x1cc, 109617},
	{0x1ce, 110502},
	{0x1d0, 111438},
	{0x1d2, 112364},
	{0x1d4, 113224},
	{0x1d6, 114134},
	{0x1d8, 115034},
	{0x1da, 115871},
	{0x1dc, 116755},
	{0x1de, 117576},
	{0x1e0, 118445},
	{0x1e2, 119305},
	{0x1e4, 120105},
	{0x1e6, 120950},
	{0x1e8, 121788},
	{0x1ea, 122567},
	{0x1ec, 123391},
	{0x1ee, 124157},
	{0x1f0, 124968},
	{0x1f2, 125771},
	{0x1f4, 126518},
	{0x1f6, 127308},
	{0x1f8, 128092},
	{0x1fa, 128821},
	{0x1fc, 129593},
	{0x1fe, 130310},
	{0x380, 131070},
	{0x382, 132570},
	{0x384, 133956},
	{0x386, 135412},
	{0x388, 136846},
	{0x38a, 138171},
	{0x38c, 139564},
	{0x38e, 140851},
	{0x390, 142206},
	{0x392, 143541},
	{0x394, 144776},
	{0x396, 146075},
	{0x398, 147357},
	{0x39a, 148544},
	{0x39c, 149793},
	{0x39e, 150949},
	{0x3a0, 152167},
	{0x3a2, 153370},
	{0x3a4, 154483},
	{0x3a6, 155657},
	{0x3a8, 156816},
	{0x3aa, 157890},
	{0x3ac, 159022},
	{0x3ae, 160072},
	{0x3b0, 161178},
	{0x3b2, 162272},
	{0x3b4, 163286},
	{0x3b6, 164356},
	{0x3b8, 165414},
	{0x3ba, 166395},
	{0x3bc, 167431},
	{0x3be, 168391},
	{0x3c0, 169405},
	{0x3c2, 170408},
	{0x3c4, 171339},
	{0x3c6, 172322},
	{0x3c8, 173295},
	{0x3ca, 174198},
	{0x3cc, 175152},
	{0x3ce, 176037},
	{0x3d0, 176973},
	{0x3d2, 177899},
	{0x3d4, 178759},
	{0x3d6, 179669},
	{0x3d8, 180569},
	{0x3da, 181406},
	{0x3dc, 182290},
	{0x3de, 183111},
	{0x3e0, 183980},
	{0x3e2, 184840},
	{0x3e4, 185640},
	{0x3e6, 186485},
	{0x3e8, 187323},
	{0x3ea, 188102},
	{0x3ec, 188926},
	{0x3ee, 189692},
	{0x3f0, 190503},
	{0x3f2, 191306},
	{0x3f4, 192053},
	{0x3f6, 192843},
	{0x3f8, 193627},
	{0x3fa, 194356},
	{0x3fc, 195128},
	{0x3fe, 195845},
	{0x780, 196605},
	{0x782, 198105},
	{0x784, 199491},
	{0x786, 200947},
	{0x788, 202381},
	{0x78a, 203706},
	{0x78c, 205099},
	{0x78e, 206386},
	{0x790, 207741},
	{0x792, 209076},
	{0x794, 210311},
	{0x796, 211610},
	{0x798, 212892},
	{0x79a, 214079},
	{0x79c, 215328},
	{0x79e, 216484},
	{0x7a0, 217702},
	{0x7a2, 218905},
	{0x7a4, 220018},
	{0x7a6, 221192},
	{0x7a8, 222351},
	{0x7aa, 223425},
	{0x7ac, 224557},
	{0x7ae, 225607},
	{0x7b0, 226713},
	{0x7b2, 227807},
	{0x7b4, 228821},
	{0x7b6, 229891},
	{0x7b8, 230949},
	{0x7ba, 231930},
	{0x7bc, 232966},
	{0x7be, 233926},
	{0x7c0, 234940},
	{0x7c2, 235943},
	{0x7c4, 236874},
	{0x7c6, 237857},
	{0x7c8, 238830},
	{0x7ca, 239733},
	{0x7cc, 240687},
	{0x7ce, 241572},
	{0x7d0, 242508},
	{0x7d2, 243434},
	{0x7d4, 244294},
	{0x7d6, 245204},
	{0x7d8, 246104},
	{0x7da, 246941},
	{0x7dc, 247825},
	{0x7de, 248646},
	{0x7e0, 249515},
	{0x7e2, 250375},
	{0x7e4, 251175},
	{0x7e6, 252020},
	{0x7e8, 252858},
	{0x7ea, 253637},
	{0x7ec, 254461},
	{0x7ee, 255227},
	{0x7f0, 256038},
	{0x7f2, 256841},
	{0x7f4, 257588},
	{0x7f6, 258378},
	{0x7f8, 259162},
	{0x7fa, 259891},
	{0x7fc, 260663},
	{0x7fe, 261380},
	{0xf80, 262140},
	{0xf82, 263640},
	{0xf84, 265026},
	{0xf86, 266482},
	{0xf88, 267916},
	{0xf8a, 269241},
	{0xf8c, 270634},
	{0xf8e, 271921},
	{0xf90, 273276},
	{0xf92, 274611},
	{0xf94, 275846},
	{0xf96, 277145},
	{0xf98, 278427},
	{0xf9a, 279614},
	{0xf9c, 280863},
	{0xf9e, 282019},
	{0xfa0, 283237},
	{0xfa2, 284440},
	{0xfa4, 285553},
	{0xfa6, 286727},
	{0xfa8, 287886},
	{0xfaa, 288960},
	{0xfac, 290092},
	{0xfae, 291142},
	{0xfb0, 292248},
	{0xfb2, 293342},
	{0xfb4, 294356},
	{0xfb6, 295426},
	{0xfb8, 296484},
	{0xfba, 297465},
	{0xfbc, 298501},
	{0xfbe, 299461},
	{0xfc0, 300475},
	{0xfc2, 301478},
	{0xfc4, 302409},
	{0xfc6, 303392},
	{0xfc8, 304365},
	{0xfca, 305268},
	{0xfcc, 306222},
	{0xfce, 307107},
	{0xfd0, 308043},
	{0xfd2, 308969},
	{0xfd4, 309829},
	{0xfd6, 310739},
	{0xfd8, 311639},
	{0xfda, 312476},
	{0xfdc, 313360},
	{0xfde, 314181},
	{0xfe0, 315050},
	{0xfe2, 315910},
	{0xfe4, 316710},
	{0xfe6, 317555},
	{0xfe8, 318393},
	{0xfea, 319172},
	{0xfec, 319996},
	{0xfee, 320762},
	{0xff0, 321573},
	{0xff2, 322376},
	{0xff4, 323123},
	{0xff6, 323913},
	{0xff8, 324697},
	{0xffa, 325426},
	{0xffc, 326198},
	{0xffe, 326915},
	{0x1f80, 327675},
	{0x1f82, 329175},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
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
	.chip_id = 0xeb2c,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x32,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 720,
		.lans = 1,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 1600,
		.image_theight = 1200,
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
	.max_again = 329175,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1500 - 4,
	.integration_time_limit = 1500 - 4,
	.total_width = 1920,
	.total_height = 1500,
	.max_integration_time = 1500 - 4,
	.one_line_expr_in_us = 27,
	.integration_time_apply_delay = 2,
	.again = 0,
	.integration_time = 0x4de,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_1920_1080_25fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x36ea,0x0f},
	{0x36eb,0x25},
	{0x36ed,0x04},
	{0x36e9,0x01},
	{0x301f,0x01},
	{0x320e,0x05},
	{0x320f,0xdc},
	{0x3248,0x02},
	{0x3253,0x0a},
	{0x3301,0xff},
	{0x3302,0xff},
	{0x3303,0x10},
	{0x3306,0x28},
	{0x3307,0x02},
	{0x330a,0x00},
	{0x330b,0xb0},
	{0x3318,0x02},
	{0x3320,0x06},
	{0x3321,0x02},
	{0x3326,0x12},
	{0x3327,0x0e},
	{0x3328,0x03},
	{0x3329,0x0f},
	{0x3364,0x0f},
	{0x33b3,0x40},
	{0x33f9,0x2c},
	{0x33fb,0x38},
	{0x33fc,0x0f},
	{0x33fd,0x1f},
	{0x349f,0x03},
	{0x34a6,0x01},
	{0x34a7,0x1f},
	{0x34a8,0x40},
	{0x34a9,0x30},
	{0x34ab,0xa6},
	{0x34ad,0xa6},
	{0x3622,0x60},
	{0x3625,0x08},
	{0x3630,0xa8},
	{0x3631,0x84},
	{0x3632,0x90},
	{0x3633,0x43},
	{0x3634,0x09},
	{0x3635,0x82},
	{0x3636,0x48},
	{0x3637,0xe4},
	{0x3641,0x22},
	{0x3670,0x0e},
	{0x3674,0xc0},
	{0x3675,0xc0},
	{0x3676,0xc0},
	{0x3677,0x86},
	{0x3678,0x88},
	{0x3679,0x8c},
	{0x367c,0x01},
	{0x367d,0x0f},
	{0x367e,0x01},
	{0x367f,0x0f},
	{0x3690,0x43},
	{0x3691,0x43},
	{0x3692,0x53},
	{0x369c,0x01},
	{0x369d,0x1f},
	{0x3900,0x0d},
	{0x3904,0x06},
	{0x3905,0x98},
	{0x391b,0x81},
	{0x391c,0x10},
	{0x391d,0x19},
	{0x3949,0xc8},
	{0x394b,0x64},
	{0x3952,0x02},
	{0x3e00,0x00},
	{0x3e01,0x4d},
	{0x3e02,0xe0},
	{0x4502,0x34},
	{0x4509,0x30},
	{0x0100,0x01},

	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 1600,
		.height = 1200,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_25fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

static struct regval_list sensor_stream_on_mipi[] = {
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = buf,
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

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 3,
		.buf = buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 0
static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sensor_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sensor_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sensor_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = -1;
	int it = (value & 0xffff) ;
	int again = (value & 0xffff0000) >> 16;

	ret = sensor_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
	ret += sensor_write(sd, 0x3e07, (unsigned char)(again & 0xff));
	ret += sensor_write(sd, 0x3e09, (unsigned char)(((again >> 8) & 0xff)));

	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret += sensor_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));

	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sensor_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3e08, (unsigned char)(((value >> 8) & 0xff)));
	if (ret < 0)
		return ret;
	gain_val = value;

	return 0;
}
#endif

static int sensor_set_logic(struct tx_isp_subdev *sd, int value)
{
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

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable)
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

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	if (init->enable) {
	    if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
            ret = sensor_write_array(sd, wsize->regs);
            if (ret)
                return ret;
            sensor->video.state = TX_ISP_MODULE_INIT;
	    }
	    if (sensor->video.state == TX_ISP_MODULE_INIT) {
            if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
                ret = sensor_write_array(sd, sensor_stream_on_mipi);
            } else {
                ISP_ERROR("Don't support this Sensor Data interface\n");
            }
            sensor->video.state = TX_ISP_MODULE_RUNNING;
            ISP_WARNING("%s stream on\n", SENSOR_NAME);
	    }
	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
        sensor->video.state = TX_ISP_MODULE_INIT;
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;

	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}
	sclk = SENSOR_SUPPORT_30FPS_SCLK;

	ret = sensor_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}
	hts = ((hts << 8) + tmp) ;//<< 1;
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += sensor_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
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

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
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
    unsigned long rate;
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;

    if (info->default_boot !=0)
        ISP_ERROR("Have no this MCLK Source!!!\n");

    switch (info->video_interface) {
        case TISP_SENSOR_VI_MIPI_CSI0:
            sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
            sensor_attr.mipi.index = 0;
            break;
        case TISP_SENSOR_VI_MIPI_CSI1:
            sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
            sensor_attr.mipi.index = 1;
            break;
        case TISP_SENSOR_VI_DVP:
            sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
            break;
        default:
            ISP_ERROR("Have no this MCLK Source!!!\n");
    }

    switch (info->mclk) {
        case TISP_SENSOR_MCLK0:
            sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim0");
            set_sensor_mclk_function(0);
            break;
        case TISP_SENSOR_MCLK1:
            sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim1");
            set_sensor_mclk_function(1);
            break;
        case TISP_SENSOR_MCLK2:
            sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim2");
            set_sensor_mclk_function(2);
            break;
        default:
            ISP_ERROR("Have no this MCLK Source!!!\n");
    }

    rate = private_clk_get_rate(sensor->mclk);
    if (IS_ERR(sensor->mclk)) {
        ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
        return -1;
    }
    private_clk_set_rate(sensor->mclk, 24000000);
    private_clk_prepare_enable(sensor->mclk);

    reset_gpio = info->rst_gpio;
    pwdn_gpio = info->pwdn_gpio;

    return 0;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
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
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = -1;
	unsigned char val = 0x0;

	ret += sensor_read(sd, 0x3221, &val);

	if (enable & 0x2)
		val |= 0x60;
	else
		val &= 0x9f;

	ret += sensor_write(sd, 0x3221, val);

	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
    struct tx_isp_sensor_value *sensor_val = arg;


	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
	     	ret = sensor_set_expo(sd, sensor_val->value);
		break;
//	case TX_ISP_EVENT_SENSOR_INT_TIME:
//		if (arg)
//			ret = sensor_set_integration_time(sd, sensor_val->value);
//		break;
//	case TX_ISP_EVENT_SENSOR_AGAIN:
//		if (arg)
//			ret = sensor_set_analog_gain(sd, sensor_val->value);
//		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if (arg)
			ret = sensor_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if (arg)
			ret = sensor_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if (arg)
			ret = sensor_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);

		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);

		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if (arg)
			ret = sensor_set_vflip(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if (arg)
			ret = sensor_set_logic(sd, sensor_val->value);
	default:
		break;
	}

	return ret;
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
	/*.ioctl = sensor_ops_ioctl,*/
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

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));

	/*
	  convert sensor-gain into isp-gain,
	*/
	sd = &sensor->sd;
	video = &sensor->video;
    	sensor->dev = &client->dev;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sensor_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

    private_clk_disable_unprepare(sensor->mclk);
    private_devm_clk_put(&client->dev, sensor->mclk);
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
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
