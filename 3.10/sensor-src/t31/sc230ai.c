// SPDX-License-Identifier: GPL-2.0+
/*
 * sc230ai.c
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
#include <sensor-info.h>

#define SENSOR_NAME "sc230ai"
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x30
#define SENSOR_MAX_WIDTH 1920
#define SENSOR_MAX_HEIGHT 1080
#define SENSOR_CHIP_ID 0xcb34
#define SENSOR_CHIP_ID_H (0xcb)
#define SENSOR_CHIP_ID_L (0x34)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK (81000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_1
#define SENSOR_VERSION "H20220125a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

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
    {0xa4, 23414},
    {0xa6, 24587},
    {0xa8, 25746},
    {0xaa, 26820},
    {0xac, 27953},
    {0xae, 29002},
    {0xb0, 30109},
    {0xb2, 31203},
    {0xb4, 32217},
    {0xb6, 33287},
    {0xb8, 34345},
    {0xba, 35326},
    {0xbc, 36361},
    {0xbe, 37322},
    {0xc0, 38336},
    {0xc2, 39339},
    {0xc4, 40270},
    {0xc6, 41253},
    {0xc8, 42226},
    {0xca, 43129},
    {0xcc, 44082},
    {0xce, 44968},
    {0xd0, 45904},
    {0xd2, 46830},
    {0xd4, 47690},
    {0xd6, 48599},
    {0xd8, 49500},
    {0xda, 50336},
    {0xdc, 51220},
    {0xde, 52042},
    {0xe0, 52910},
    {0xe2, 53771},
    {0xe4, 54571},
    {0xe6, 55416},
    {0xe8, 56254},
    {0xea, 57033},
    {0xec, 57857},
    {0xee, 58623},
    {0xf0, 59433},
    {0xf2, 60237},
    {0xf4, 60984},
    {0xf6, 61774},
    {0xf8, 62558},
    {0xfa, 63287},
    {0xfc, 64059},
    {0xfe, 64776},
    {0x180, 65536},
    {0x182, 67036},
    {0x184, 68422},
    {0x186, 69878},
    {0x188, 71312},
    {0x18a, 72637},
    {0x18c, 74030},
    {0x18e, 75317},
    {0x190, 76672},
    {0x192, 78007},
    {0x194, 79242},
    {0x196, 80541},
    {0x198, 81823},
    {0x19a, 83010},
    {0x19c, 84259},
    {0x19e, 85415},
    {0x1a0, 86633},
    {0x1a2, 87836},
    {0x1a4, 88950},
    {0x1a6, 90123},
    {0x1a8, 91282},
    {0x1aa, 92356},
    {0x1ac, 93489},
    {0x1ae, 94538},
    {0x1b0, 95645},
    {0x1b2, 96739},
    {0x1b4, 97753},
    {0x1b6, 98823},
    {0x1b8, 99881},
    {0x1ba, 100862},
    {0x1bc, 101897},
    {0x1be, 102858},
    {0x1c0, 103872},
    {0x1c2, 104875},
    {0x1c4, 105806},
    {0x1c6, 106789},
    {0x1c8, 107762},
    {0x1ca, 108665},
    {0x1cc, 109618},
    {0x1ce, 110504},
    {0x1d0, 111440},
    {0x1d2, 112366},
    {0x1d4, 113226},
    {0x1d6, 114135},
    {0x1d8, 115036},
    {0x4081, 115455},
    {0x4082, 116949},
    {0x4084, 118338},
    {0x4086, 119787},
    {0x4088, 121241},
    {0x408a, 122543},
    {0x408c, 123956},
    {0x408e, 125246},
    {0x4090, 126594},
    {0x4092, 127923},
    {0x4094, 129161},
    {0x4096, 130455},
    {0x4098, 131755},
    {0x409a, 132921},
    {0x409c, 134187},
    {0x409e, 135324},
    {0x40a0, 136558},
    {0x40a2, 137755},
    {0x40a4, 138872},
    {0x40a6, 140040},
    {0x40a8, 141194},
    {0x40aa, 142271},
    {0x40ac, 143419},
    {0x40ae, 144450},
    {0x40b0, 145572},
    {0x40b2, 146661},
    {0x40b4, 147677},
    {0x40b6, 148742},
    {0x40b8, 149795},
    {0x40ba, 150779},
    {0x40bc, 151809},
    {0x40be, 152773},
    {0x40c0, 153800},
    {0x40c2, 154799},
    {0x40c4, 155732},
    {0x40c6, 156710},
    {0x40c8, 157679},
    {0x40ca, 158584},
    {0x40cc, 159533},
    {0x40ce, 160421},
    {0x40d0, 161353},
    {0x40d2, 162292},
    {0x40d4, 163137},
    {0x40d6, 164059},
    {0x40d8, 164955},
    {0x40da, 165794},
    {0x40dc, 166674},
    {0x40de, 167497},
    {0x40e0, 168362},
    {0x40e2, 169234},
    {0x40e4, 170020},
    {0x40e6, 170877},
    {0x40e8, 171712},
    {0x40ea, 172493},
    {0x40ec, 173313},
    {0x40ee, 174081},
    {0x40f0, 174887},
    {0x40f2, 175687},
    {0x40f4, 176436},
    {0x40f6, 177237},
    {0x40f8, 178017},
    {0x40fa, 178748},
    {0x40fc, 179516},
    {0x40fe, 180235},
    {0x4881, 180991},
    {0x4882, 182498},
    {0x4884, 183874},
    {0x4886, 185337},
    {0x4888, 186764},
    {0x488a, 188092},
    {0x488c, 189492},
    {0x488e, 190770},
    {0x4890, 192130},
    {0x4892, 193459},
    {0x4894, 194697},
    {0x4896, 196003},
    {0x4898, 197279},
    {0x489a, 198468},
    {0x489c, 199712},
    {0x489e, 200871},
    {0x48a0, 202083},
    {0x48a2, 203291},
    {0x48a4, 204408},
    {0x48a6, 205576},
    {0x48a8, 206740},
    {0x48aa, 207807},
    {0x48ac, 208944},
    {0x48ae, 209996},
    {0x48b0, 211098},
    {0x48b2, 212197},
    {0x48b4, 213203},
    {0x48b6, 214278},
    {0x48b8, 215341},
    {0x48ba, 216315},
    {0x48bc, 217355},
    {0x48be, 218309},
    {0x48c0, 219327},
    {0x48c2, 220335},
    {0x48c4, 221259},
    {0x48c6, 222246},
    {0x48c8, 223215},
    {0x48ca, 224120},
    {0x48cc, 225078},
    {0x48ce, 225957},
    {0x48d0, 226897},
    {0x48d2, 227819},
    {0x48d4, 228682},
    {0x48d6, 229595},
    {0x48d8, 230491},
    {0x48da, 231330},
    {0x48dc, 232210},
    {0x48de, 233033},
    {0x48e0, 233898},
    {0x48e2, 234762},
    {0x48e4, 235564},
    {0x48e6, 236406},
    {0x48e8, 237248},
    {0x48ea, 238021},
    {0x48ec, 238849},
    {0x48ee, 239617},
    {0x48f0, 240423},
    {0x48f2, 241230},
    {0x48f4, 241972},
    {0x48f6, 242766},
    {0x48f8, 243553},
    {0x48fa, 244277},
    {0x48fc, 245052},
    {0x48fe, 245764},
    {0x4981, 246527},
    {0x4982, 248028},
    {0x4984, 249410},
    {0x4986, 250873},
    {0x4988, 252307},
    {0x498a, 253628},
    {0x498c, 255021},
    {0x498e, 256306},
    {0x4990, 257666},
    {0x4992, 259001},
    {0x4994, 260233},
    {0x4996, 261533},
    {0x4998, 262815},
    {0x499a, 263998},
    {0x499c, 265253},
    {0x499e, 266407},
    {0x49a0, 267625},
    {0x49a2, 268827},
    {0x49a4, 269938},
    {0x49a6, 271117},
    {0x49a8, 272276},
    {0x49aa, 273348},
    {0x49ac, 274480},
    {0x49ae, 275527},
    {0x49b0, 276634},
    {0x49b2, 277733},
    {0x49b4, 278744},
    {0x49b6, 279814},
    {0x49b8, 280872},
    {0x49ba, 281851},
    {0x49bc, 282891},
    {0x49be, 283849},
    {0x49c0, 284863},
    {0x49c2, 285866},
    {0x49c4, 286795},
    {0x49c6, 287782},
    {0x49c8, 288755},
    {0x49ca, 289656},
    {0x49cc, 290610},
    {0x49ce, 291493},
    {0x49d0, 292433},
    {0x49d2, 293359},
    {0x49d4, 294218},
    {0x49d6, 295127},
    {0x49d8, 296027},
    {0x49da, 296862},
    {0x49dc, 297750},
    {0x49de, 298569},
    {0x49e0, 299438},
    {0x49e2, 300298},
    {0x49e4, 301096},
    {0x49e6, 301946},
    {0x49e8, 302784},
    {0x49ea, 303561},
    {0x49ec, 304385},
    {0x49ee, 305149},
    {0x49f0, 305963},
    {0x49f2, 306766},
    {0x49f4, 307511},
    {0x49f6, 308302},
    {0x49f8, 309085},
    {0x49fa, 309813},
    {0x49fc, 310588},
    {0x49fe, 311304},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		}
		else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else {
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
	.chip_id = 0xcb34,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 405,
		.lans = 2,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
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
	.max_again = 311304,
	.max_dgain = 0,
	.min_integration_time = 3,
	.min_integration_time_native = 3,
	.max_integration_time_native = 1125 -8,
	.integration_time_limit = 1125 -8,
	.total_width = 2400,
	.total_height = 1125,
	.max_integration_time = 1125 -8,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};


static struct regval_list sensor_init_regs_1920_1080_30fps_mipi[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x301f, 0x07},
	{0x320c, 0x09},
	{0x320d, 0x60},
	{0x3301, 0x07},
	{0x3304, 0x50},
	{0x3306, 0x70},
	{0x3308, 0x18},
	{0x3309, 0x68},
	{0x330a, 0x01},
	{0x330b, 0x20},
	{0x3314, 0x15},
	{0x331e, 0x41},
	{0x331f, 0x59},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x335d, 0x60},
	{0x335e, 0x06},
	{0x335f, 0x08},
	{0x3364, 0x5e},
	{0x337c, 0x02},
	{0x337d, 0x0a},
	{0x3390, 0x01},
	{0x3391, 0x0b},
	{0x3392, 0x0f},
	{0x3393, 0x09},
	{0x3394, 0x0d},
	{0x3395, 0x60},
	{0x3396, 0x48},
	{0x3397, 0x49},
	{0x3398, 0x4b},
	{0x3399, 0x07},
	{0x339a, 0x0a},
	{0x339b, 0x0d},
	{0x339c, 0x60},
	{0x33a2, 0x04},
	{0x33ad, 0x2c},
	{0x33af, 0x40},
	{0x33b1, 0x80},
	{0x33b3, 0x40},
	{0x33b9, 0x0a},
	{0x33f9, 0x78},
	{0x33fb, 0xa0},
	{0x33fc, 0x4f},
	{0x33fd, 0x5f},
	{0x349f, 0x03},
	{0x34a6, 0x4b},
	{0x34a7, 0x5f},
	{0x34a8, 0x30},
	{0x34a9, 0x20},
	{0x34aa, 0x01},
	{0x34ab, 0x28},
	{0x34ac, 0x01},
	{0x34ad, 0x58},
	{0x34f8, 0x7f},
	{0x34f9, 0x10},
	{0x3630, 0xc0},
	{0x3632, 0x54},
	{0x3633, 0x44},
	{0x363b, 0x20},
	{0x363c, 0x08},
	{0x3670, 0x09},
	{0x3674, 0xb0},
	{0x3675, 0x80},
	{0x3676, 0x88},
	{0x367c, 0x40},
	{0x367d, 0x49},
	{0x3690, 0x33},
	{0x3691, 0x33},
	{0x3692, 0x43},
	{0x369c, 0x49},
	{0x369d, 0x4f},
	{0x36ae, 0x4b},
	{0x36af, 0x4f},
	{0x36b0, 0x87},
	{0x36b1, 0x9b},
	{0x36b2, 0xb7},
	{0x36d0, 0x01},
	{0x36ea, 0x09},
	{0x36eb, 0x0c},
	{0x36ec, 0x1c},
	{0x36ed, 0x24},
	{0x3722, 0x97},
	{0x3724, 0x22},
	{0x3728, 0x90},
	{0x37fa, 0x09},
	{0x37fb, 0x32},
	{0x37fc, 0x10},
	{0x37fd, 0x34},
	{0x3901, 0x02},
	{0x3902, 0xc5},
	{0x3904, 0x04},
	{0x3907, 0x00},
	{0x3908, 0x41},
	{0x3909, 0x00},
	{0x390a, 0x00},
	{0x3933, 0x84},
	{0x3934, 0x0a},
	{0x3940, 0x64},
	{0x3941, 0x00},
	{0x3942, 0x04},
	{0x3943, 0x0b},
	{0x3e00, 0x00},
	{0x3e01, 0x8c},
	{0x3e02, 0x20},
	{0x440e, 0x02},
	{0x450d, 0x11},
	{0x4819, 0x06},
	{0x481b, 0x03},
	{0x481d, 0x0b},
	{0x481f, 0x03},
	{0x4821, 0x08},
	{0x4823, 0x03},
	{0x4825, 0x03},
	{0x4827, 0x03},
	{0x4829, 0x05},
	{0x5010, 0x01},
	{0x5787, 0x08},
	{0x5788, 0x03},
	{0x5789, 0x00},
	{0x578a, 0x10},
	{0x578b, 0x08},
	{0x578c, 0x00},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x00},
	{0x5793, 0x10},
	{0x5794, 0x08},
	{0x5795, 0x00},
	{0x5799, 0x06},
	{0x57ad, 0x00},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x3f},
	{0x5ae3, 0x38},
	{0x5ae4, 0x28},
	{0x5ae5, 0x3f},
	{0x5ae6, 0x38},
	{0x5ae7, 0x28},
	{0x5ae8, 0x3f},
	{0x5ae9, 0x3c},
	{0x5aea, 0x2c},
	{0x5aeb, 0x3f},
	{0x5aec, 0x3c},
	{0x5aed, 0x2c},
	{0x5af4, 0x3f},
	{0x5af5, 0x38},
	{0x5af6, 0x28},
	{0x5af7, 0x3f},
	{0x5af8, 0x38},
	{0x5af9, 0x28},
	{0x5afa, 0x3f},
	{0x5afb, 0x3c},
	{0x5afc, 0x2c},
	{0x5afd, 0x3f},
	{0x5afe, 0x3c},
	{0x5aff, 0x2c},
	{0x36e9, 0x53},
	{0x37f9, 0x53},
	{0x0100, 0x01},

	{SENSOR_REG_END, 0x00},
};


static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1920*1080 */
	{
		.width = 1920,
		.height = 1080,
		.fps = 30 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_mipi,
	}
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

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
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

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg,
		 unsigned char value)
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
	 int ret = 0;
     int it = (value & 0xffff);
     int again = (value & 0xffff0000) >> 16;

     //integration time
//	 printk("------> it <---------\n");
     ret = sensor_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
     ret += sensor_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
     ret += sensor_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));

//	 printk("------> again <---------\n");
     //sensor analog gain
     ret += sensor_write(sd, 0x3e09, (unsigned char)(((again >> 8) & 0xff)));
     //sensor dig fine gain
     ret += sensor_write(sd, 0x3e07, (unsigned char)(again & 0xff));
     if (ret < 0)
         return ret;

     return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
	ret = sensor_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0x0f));
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
	ret += sensor_write(sd, 0x3e08, (unsigned char)((value & 0xff00) >> 8));
	if (ret < 0)
		return ret;

	return 0;
}
#endif

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
		ret = sensor_write_array(sd, sensor_stream_on_mipi);
		ISP_WARNING("%s stream on\n", SENSOR_NAME);

	}
	else {
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
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

	sclk = SENSOR_SUPPORT_SCLK;

    ret = sensor_read(sd, 0x320c, &tmp);
    hts = tmp;
    ret += sensor_read(sd, 0x320d, &tmp);
    if (0 != ret) {
        ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
        return ret;
    }
    hts = ((hts << 8) + tmp);
    vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
    ret += sensor_write(sd, 0x320f, (unsigned char)(vts & 0xff));
    ret += sensor_write(sd, 0x320e, (unsigned char)(vts >> 8));
    if (0 != ret) {
        ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
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

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

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
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		} else {
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio,"sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
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

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = sensor_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		//if (arg)
		//	ret = sensor_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		//if (arg)
		//	ret = sensor_set_analog_gain(sd, *(int*)arg);
		break;
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
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sensor_write_array(sd, sensor_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, *(int*)arg);
		break;
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

	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	/*
	  convert sensor-gain into isp-gain,
	*/
	sd = &sensor->sd;
	video = &sensor->video;
	sensor_attr.expo_fs = 1;
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

	ISP_WARNING("probe ok ------->%s\n", SENSOR_NAME);

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

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
