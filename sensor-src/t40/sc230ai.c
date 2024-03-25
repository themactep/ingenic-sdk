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
#include <txx-funcs.h>

#define SENSOR_NAME "sc230ai"
#define SENSOR_CHIP_ID_H (0xcb)
#define SENSOR_CHIP_ID_L (0x34)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_PCLK_FPS_60 (162000000)
#define SENSOR_OUTPUT_MAX_FPS 60
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20220713a"

static int reset_gpio = GPIO_PC(28);
static int pwdn_gpio = -1;
static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static bool dpc_flag = true;

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x340, 0},
	{0x341, 1465},
	{0x342, 2818},
	{0x343, 4330},
	{0x344, 5731},
	{0x345, 7026},
	{0x346, 8471},
	{0x347, 9729},
	{0x348, 11135},
	{0x349, 12439},
	{0x34a, 13646},
	{0x34b, 14995},
	{0x34c, 16247},
	{0x34d, 17406},
	{0x34e, 18703},
	{0x34f, 19832},
	{0x350, 21097},
	{0x351, 22271},
	{0x352, 23360},
	{0x353, 24578},
	{0x354, 25710},
	{0x355, 26759},
	{0x356, 27935},
	{0x357, 28960},
	{0x358, 30108},
	{0x359, 31177},
	{0x35a, 32168},
	{0x35b, 33277},
	{0x35c, 34311},
	{0x35d, 35269},
	{0x35e, 36344},
	{0x35f, 37282},
	{0x360, 38335},
	{0x2340, 39315},
	{0x2341, 40766},
	{0x2342, 42195},
	{0x2343, 43660},
	{0x2344, 45046},
	{0x2345, 46413},
	{0x2346, 47760},
	{0x2347, 49142},
	{0x2348, 50451},
	{0x2349, 51741},
	{0x234a, 53068},
	{0x234b, 54323},
	{0x234c, 55563},
	{0x234d, 56785},
	{0x234e, 58044},
	{0x234f, 59186},
	{0x2350, 60413},
	{0x2351, 61623},
	{0x2352, 62723},
	{0x2353, 63905},
	{0x2354, 65025},
	{0x2355, 66133},
	{0x2356, 67227},
	{0x2357, 68354},
	{0x2358, 69468},
	{0x2359, 70481},
	{0x235a, 71570},
	{0x235b, 72604},
	{0x235c, 73626},
	{0x235d, 74638},
	{0x235e, 75680},
	{0x235f, 76671},
	{0x2360, 77651},
	{0x2361, 78660},
	{0x2362, 79580},
	{0x2363, 80570},
	{0x2364, 81510},
	{0x2365, 82441},
	{0x2366, 83364},
	{0x2367, 84315},
	{0x2368, 85256},
	{0x2369, 86114},
	{0x236a, 87038},
	{0x236b, 87916},
	{0x236c, 88787},
	{0x236d, 89649},
	{0x236e, 90540},
	{0x236f, 91386},
	{0x2370, 92225},
	{0x2371, 93092},
	{0x2372, 93882},
	{0x2373, 94733},
	{0x2374, 95543},
	{0x2375, 96346},
	{0x2376, 97176},
	{0x2377, 97966},
	{0x2378, 98781},
	{0x2379, 99525},
	{0x237a, 100326},
	{0x237b, 101090},
	{0x237c, 101848},
	{0x237d, 102632},
	{0x237e, 103377},
	{0x237f, 104117},
	{0x2740, 104851},
	{0x2741, 106302},
	{0x2742, 107790},
	{0x2743, 109196},
	{0x2744, 110611},
	{0x2745, 111977},
	{0x2746, 113323},
	{0x2747, 114678},
	{0x2748, 115987},
	{0x2749, 117277},
	{0x274a, 118604},
	{0x274b, 119859},
	{0x274c, 121124},
	{0x274d, 122347},
	{0x274e, 123554},
	{0x274f, 124772},
	{0x2750, 125949},
	{0x2751, 127135},
	{0x2752, 128307},
	{0x2753, 129441},
	{0x2754, 130585},
	{0x2755, 131691},
	{0x2756, 132785},
	{0x2757, 133890},
	{0x2758, 134982},
	{0x2759, 136039},
	{0x275a, 137106},
	{0x275b, 138140},
	{0x275c, 139184},
	{0x275d, 140195},
	{0x275e, 141196},
	{0x275f, 142228},
	{0x2760, 143206},
	{0x2761, 144177},
	{0x2762, 145156},
	{0x2763, 146106},
	{0x2764, 147066},
	{0x2765, 147996},
	{0x2766, 148918},
	{0x2767, 149869},
	{0x2768, 150772},
	{0x2769, 151669},
	{0x276a, 152574},
	{0x276b, 153452},
	{0x276c, 154341},
	{0x276d, 155203},
	{0x276e, 156076},
	{0x276f, 156940},
	{0x2770, 157779},
	{0x2771, 158610},
	{0x2772, 159452},
	{0x2773, 160269},
	{0x2774, 161096},
	{0x2775, 161916},
	{0x2776, 162712},
	{0x2777, 163518},
	{0x2778, 164300},
	{0x2779, 165077},
	{0x277a, 165862},
	{0x277b, 166626},
	{0x277c, 167416},
	{0x277d, 168168},
	{0x277e, 168913},
	{0x277f, 169668},
	{0x2f40, 170402},
	{0x2f41, 171867},
	{0x2f42, 173326},
	{0x2f43, 174732},
	{0x2f44, 176133},
	{0x2f45, 177513},
	{0x2f46, 178887},
	{0x2f47, 180214},
	{0x2f48, 181536},
	{0x2f49, 182853},
	{0x2f4a, 184140},
	{0x2f4b, 185395},
	{0x2f4c, 186648},
	{0x2f4d, 187896},
	{0x2f4e, 189116},
	{0x2f4f, 190308},
	{0x2f50, 191497},
	{0x2f51, 192683},
	{0x2f52, 193843},
	{0x2f53, 194977},
	{0x2f54, 196121},
	{0x2f55, 197239},
	{0x2f56, 198344},
	{0x2f57, 199426},
	{0x2f58, 200518},
	{0x2f59, 201586},
	{0x2f5a, 202642},
	{0x2f5b, 203676},
	{0x2f5c, 204720},
	{0x2f5d, 205742},
	{0x2f5e, 206752},
	{0x2f5f, 207753},
	{0x2f60, 208742},
	{0x2f61, 209722},
	{0x2f62, 210692},
	{0x2f63, 211651},
	{0x2f64, 212602},
	{0x2f65, 213542},
	{0x2f66, 214473},
	{0x2f67, 215395},
	{0x2f68, 216308},
	{0x2f69, 217214},
	{0x2f6a, 218119},
	{0x2f6b, 218997},
	{0x2f6c, 219877},
	{0x2f6d, 220748},
	{0x2f6e, 221620},
	{0x2f6f, 222467},
	{0x2f70, 223315},
	{0x2f71, 224155},
	{0x2f72, 224997},
	{0x2f73, 225814},
	{0x2f74, 226632},
	{0x2f75, 227452},
	{0x2f76, 228256},
	{0x2f77, 229046},
	{0x2f78, 229836},
	{0x2f79, 230629},
	{0x2f7a, 231407},
	{0x2f7b, 232170},
	{0x2f7c, 232936},
	{0x2f7d, 233704},
	{0x2f7e, 234455},
	{0x2f7f, 235196},
	{0x3f40, 235945},
	{0x3f41, 237410},
	{0x3f42, 238854},
	{0x3f43, 240276},
	{0x3f44, 241675},
	{0x3f45, 243055},
	{0x3f46, 244416},
	{0x3f47, 245757},
	{0x3f48, 247078},
	{0x3f49, 248389},
	{0x3f4a, 249669},
	{0x3f4b, 250944},
	{0x3f4c, 252190},
	{0x3f4d, 253432},
	{0x3f4e, 254645},
	{0x3f4f, 255856},
	{0x3f50, 257038},
	{0x3f51, 258219},
	{0x3f52, 259373},
	{0x3f53, 260525},
	{0x3f54, 261657},
	{0x3f55, 262775},
	{0x3f56, 263880},
	{0x3f57, 264973},
	{0x3f58, 266054},
	{0x3f59, 267122},
	{0x3f5a, 268178},
	{0x3f5b, 269223},
	{0x3f5c, 270256},
	{0x3f5d, 271278},
	{0x3f5e, 272288},
	{0x3f5f, 273294},
	{0x3f60, 274278},
	{0x3f61, 275264},
	{0x3f62, 276228},
	{0x3f63, 277193},
	{0x3f64, 278138},
	{0x3f65, 279082},
	{0x3f66, 280009},
	{0x3f67, 280937},
	{0x3f68, 281844},
	{0x3f69, 282755},
	{0x3f6a, 283650},
	{0x3f6b, 284537},
	{0x3f6c, 285417},
	{0x3f6d, 286288},
	{0x3f6e, 287152},
	{0x3f6f, 288007},
	{0x3f70, 288855},
	{0x3f71, 289695},
	{0x3f72, 290528},
	{0x3f73, 291358},
	{0x3f74, 292172},
	{0x3f75, 292988},
	{0x3f76, 293789},
	{0x3f77, 294590},
	{0x3f78, 295376},
	{0x3f79, 296165},
	{0x3f7a, 296938},
	{0x3f7b, 297714},
	{0x3f7c, 298477},
	{0x3f7d, 299240},
	{0x3f7e, 299991},
	{0x3f7f, 300740},

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

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = 0xcb34,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 801,
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
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 300740,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1125-4,
	.integration_time_limit = 1125-4,
	.total_width = 2400,
	.total_height = 1125,
	.max_integration_time = 1125-4,
	.one_line_expr_in_us = 20,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_1920_1080_60fps_mipi_2lane[] = {
	//2lane
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x23},
	{0x3208,0x07},
	{0x3209,0x80},
	{0x320a,0x04},
	{0x320b,0x38},
	{0x320c,0x09},
	{0x320d,0x60},
	{0x3211,0x04},
	{0x3213,0x04},
	{0x3227,0x03},
	{0x3250,0x00},
	{0x3301,0x09},
	{0x3304,0x50},
	{0x3306,0x48},
	{0x3308,0x18},
	{0x3309,0x68},
	{0x330a,0x00},
	{0x330b,0xc0},
	{0x331e,0x41},
	{0x331f,0x59},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x335d,0x60},
	{0x335e,0x06},
	{0x335f,0x08},
	{0x3364,0x5e},
	{0x337c,0x02},
	{0x337d,0x0a},
	{0x3390,0x01},
	{0x3391,0x0b},
	{0x3392,0x0f},
	{0x3393,0x0c},
	{0x3394,0x0d},
	{0x3395,0x60},
	{0x3396,0x48},
	{0x3397,0x49},
	{0x3398,0x4f},
	{0x3399,0x0a},
	{0x339a,0x0f},
	{0x339b,0x14},
	{0x339c,0x60},
	{0x33a2,0x04},
	{0x33af,0x40},
	{0x33b1,0x80},
	{0x33b3,0x40},
	{0x33b9,0x0a},
	{0x33f9,0x70},
	{0x33fb,0x90},
	{0x33fc,0x4b},
	{0x33fd,0x5f},
	{0x349f,0x03},
	{0x34a6,0x4b},
	{0x34a7,0x4f},
	{0x34a8,0x30},
	{0x34a9,0x20},
	{0x34aa,0x00},
	{0x34ab,0xe0},
	{0x34ac,0x01},
	{0x34ad,0x00},
	{0x34f8,0x5f},
	{0x34f9,0x10},
	{0x3630,0xc0},
	{0x3633,0x44},
	{0x3637,0x29},
	{0x363b,0x20},
	{0x3670,0x09},
	{0x3674,0xb0},
	{0x3675,0x80},
	{0x3676,0x88},
	{0x367c,0x40},
	{0x367d,0x49},
	{0x3690,0x54},
	{0x3691,0x44},
	{0x3692,0x55},
	{0x369c,0x49},
	{0x369d,0x4f},
	{0x36ae,0x4b},
	{0x36af,0x4f},
	{0x36b0,0x87},
	{0x36b1,0x9b},
	{0x36b2,0xb7},
	{0x36d0,0x01},
	{0x36ea,0x09},
	{0x36eb,0x04},
	{0x36ec,0x0c},
	{0x36ed,0x24},
	{0x370f,0x01},
	{0x3722,0x17},
	{0x3728,0x90},
	{0x37b0,0x17},
	{0x37b1,0x17},
	{0x37b2,0x97},
	{0x37b3,0x4b},
	{0x37b4,0x4f},
	{0x37fa,0x09},
	{0x37fb,0x04},
	{0x37fc,0x00},
	{0x37fd,0x22},
	{0x3901,0x02},
	{0x3902,0xc5},
	{0x3904,0x04},
	{0x3907,0x00},
	{0x3908,0x41},
	{0x3909,0x00},
	{0x390a,0x00},
	{0x391f,0x04},
	{0x3928,0xc1},
	{0x3933,0x84},
	{0x3934,0x02},
	{0x3940,0x62},
	{0x3941,0x00},
	{0x3942,0x04},
	{0x3943,0x03},
	{0x3e00,0x00},
	{0x3e01,0x8c},
	{0x3e02,0x10},
	{0x440e,0x02},
	{0x450d,0x11},
	{0x4819,0x0a},
	{0x481b,0x06},
	{0x481d,0x16},
	{0x481f,0x05},
	{0x4821,0x0b},
	{0x4823,0x05},
	{0x4825,0x05},
	{0x4827,0x05},
	{0x4829,0x09},
	{0x5010,0x01},
	{0x5787,0x08},
	{0x5788,0x03},
	{0x5789,0x00},
	{0x578a,0x10},
	{0x578b,0x08},
	{0x578c,0x00},
	{0x5790,0x08},
	{0x5791,0x04},
	{0x5792,0x00},
	{0x5793,0x10},
	{0x5794,0x08},
	{0x5795,0x00},
	{0x5799,0x06},
	{0x57ad,0x00},
	{0x5ae0,0xfe},
	{0x5ae1,0x40},
	{0x5ae2,0x3f},
	{0x5ae3,0x38},
	{0x5ae4,0x28},
	{0x5ae5,0x3f},
	{0x5ae6,0x38},
	{0x5ae7,0x28},
	{0x5ae8,0x3f},
	{0x5ae9,0x3c},
	{0x5aea,0x2c},
	{0x5aeb,0x3f},
	{0x5aec,0x3c},
	{0x5aed,0x2c},
	{0x5af4,0x3f},
	{0x5af5,0x38},
	{0x5af6,0x28},
	{0x5af7,0x3f},
	{0x5af8,0x38},
	{0x5af9,0x28},
	{0x5afa,0x3f},
	{0x5afb,0x3c},
	{0x5afc,0x2c},
	{0x5afd,0x3f},
	{0x5afe,0x3c},
	{0x5aff,0x2c},
	////output fsync
	//0x300a,0x24,
	//0x3032,0xa0,
	{0x36e9,0x53},
	{0x37f9,0x53},
	{0x0100,0x01},
	{SENSOR_REG_END, 0x00},	/*END MARKER */

};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 1920,
		.height = 1080,
		.fps = 60 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_60fps_mipi_2lane,
	},
};

struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

static struct regval_list sensor_stream_on[] = {
	{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
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
			ret = sensor_read(sd, vals->reg_num, &vals->value);//
			printk("---->> reg=0x%x, value=0x%x <<----\n", vals->reg_num, vals->value);//
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
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);//
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
	int it = (value & 0xffff) * 2;
	int again = (value & 0xffff0000) >> 16;

	ret = sensor_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0x0f));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
	ret += sensor_write(sd, 0x3e09, (unsigned char)(again & 0xff));
	ret += sensor_write(sd, 0x3e08, (unsigned char)((again >> 8 & 0xff)));
	if (ret != 0) {
		ISP_ERROR("err: sc230ai write err %d\n",__LINE__);
		return ret;
	}

	if ((again >= 0x3f47) && dpc_flag) {
		sensor_write(sd, 0x5799, 0x07);
		dpc_flag = false;
	} else if ((again <= 0x2f5f) && (!dpc_flag)) {
		sensor_write(sd, 0x5799, 0x00);
		dpc_flag = true;
	}

	return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

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
	unsigned int again = value;
	ret = sensor_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3e08, (unsigned char)((value >> 8 & 0xff)));
	if (ret < 0)
		return ret;

	if ((again >= 0x3f47) && dpc_flag) {
		sensor_write(sd, 0x5799, 0x07);
		dpc_flag = false;
	} else if ((again <= 0x2f5f) && (!dpc_flag)) {
		sensor_write(sd, 0x5799, 0x00);
		dpc_flag = true;
	}

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

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable) {
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		return ISP_SUCCESS;
	} else {
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = TISP_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		sensor->video.state = TX_ISP_MODULE_DEINIT;

		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
		sensor->priv = wsize;
	}

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
                        sensor->video.state = TX_ISP_MODULE_INIT;
                } else {
                        ISP_WARNING("init failed, state error!\n");
                }
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
                        ret = sensor_write_array(sd, wsize->regs);
			if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_DVP) {
				/* ret += sensor_write_array(sd, sensor_stream_on_dvp); */
			} else if (sensor_attr.dbus_type == TX_SENSOR_DATA_INTERFACE_MIPI) {
				/* ret += sensor_write_array(sd, sensor_stream_on_mipi); */
			} else {
				ISP_ERROR("[%s %d] Don't support this Sensor Data interface\n", __func__, __LINE__);
			}
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			ISP_WARNING("[%s %d] %s stream on\n", __func__, __LINE__, SENSOR_NAME);
                } else {
                        ISP_WARNING("[%s %d] Failed the open stream, state error!\n", __func__, __LINE__);
		}

	} else {
		ret = sensor_write_array(sd, sensor_stream_off);
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

	sclk = SENSOR_SUPPORT_PCLK_FPS_60;
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}
	ret = sensor_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}
	hts = (hts << 8) + tmp;
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
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned long rate;
	int ret = 0;

	/* pr_debug("default boot is %d, video interface is %d, mclk is %d, reset is %d, pwdn is %d\n", info->default_boot, info->video_interface, info->mclk, info->rst_gpio, info->pwdn_gpio); */
	switch(info->default_boot) {
		case 0:
			wsize = &sensor_win_sizes[0];
			sensor_attr.mipi.clk = 801;
			sensor_attr.mipi.lans = 2;
	                sensor_attr.again = 0;
                        sensor_attr.integration_time = 0x8c1;
			break;
		default:
			ISP_ERROR("Have no this setting!!!\n");
	}

	switch(info->video_interface) {
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
			ISP_ERROR("Have no this interface!!!\n");
	}

	switch(info->mclk) {
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
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
        ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
        if (ret) {
                ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
        }

	return 0;

err_get_mclk:
	return -1;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip)
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
			private_gpio_direction_output(pwdn_gpio, 0);//
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);//
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

static int tgain = -1;
static int sensor_set_logic(struct tx_isp_subdev *sd, int value)
{
	if (value != tgain) {
		if (value <= 283241) {
			sensor_write(sd, 0x5799, 0x00);
		} else if (value >= 321577) {
			sensor_write(sd, 0x5799, 0x07);
		}
		tgain = value;
	}

	return 0;
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
		case TX_ISP_EVENT_SENSOR_LOGIC:
			if (arg)
				ret = sensor_set_logic(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_EXPO:
			if (arg)
				ret = sensor_set_expo(sd, sensor_val->value);
			break;
			/*
			   case TX_ISP_EVENT_SENSOR_INT_TIME:
			   if (arg)
			   ret = sensor_set_integration_time(sd, sensor_val->value);
			   break;
			   case TX_ISP_EVENT_SENSOR_AGAIN:
			   if (arg)
			   ret = sensor_set_analog_gain(sd, sensor_val->value);
			   break;
			 */
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
				ret = sensor_write_array(sd, sensor_stream_off);

			} else {
				ISP_ERROR("Don't support this Sensor Data interface\n");
			}
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
				ret = sensor_write_array(sd, sensor_stream_on);

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
	sensor->dev = &client->dev;
	sensor->video.attr = &sensor_attr;
        sensor->video.state = TX_ISP_MODULE_DEINIT;
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
