// SPDX-License-Identifier: GPL-2.0+
/*
 * hi556.c
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
#include <fix-point-calc.h>

#define SENSOR_NAME "hi556"
#define SENSOR_CHIP_ID_H (0x05)
#define SENSOR_CHIP_ID_L (0x56)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe

#define SENSOR_SUPPORT_SCLK_FPS_30 (175887360)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20220526a"

static int reset_gpio = GPIO_PC(28);
static int pwdn_gpio = -1;

struct regval_list {
	uint16_t reg_num;
	uint16_t  value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};
struct again_lut sensor_again_lut[] = {
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;

	*sensor_it = expo;

	return it;
}

unsigned int sensor_alloc_integration_time_short(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;

	*sensor_it = expo;

	return it;
}

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
        *sensor_again = (tisp_math_exp2(isp_gain, 16, 16) - (1 << 16)) >> 12;
	return tisp_log2_fixed_to_fixed(fix_point_div_32(16, *sensor_again, 16) + (1 << 16), 16, 16);
}

unsigned int sensor_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x0556,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x20,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 880,
		.lans = 2,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.image_twidth = 2592,
		.image_theight = 1944,
		.mipi_sc.mipi_crop_start0x = 0,
		.mipi_sc.mipi_crop_start0y = 0,
		.mipi_sc.mipi_crop_start1x = 0,
		.mipi_sc.mipi_crop_start1y = 0,
		.mipi_sc.mipi_crop_start2x = 0,
		.mipi_sc.mipi_crop_start2y = 0,
		.mipi_sc.mipi_crop_start3x = 0,
		.mipi_sc.mipi_crop_start3y = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.mipi_sc.data_type_en = 0,
		.mipi_sc.data_type_value = RAW10,
		.mipi_sc.del_start = 0,
		.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
		.mipi_sc.sensor_fid_mode = 0,
		.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 262144,
	.max_again_short = 262144,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.min_integration_time_short = 2,
	.max_integration_time_short = 405,
	.max_integration_time_native = 0x09c2 - 2,
	.integration_time_limit = 0x09c2 - 2,
	.total_width = 0xb00,
	.total_height = 0x09c2,
	.max_integration_time = 0x09c2 - 2,
	.integration_time_apply_delay = 2,
	.again = 0,
	.integration_time = 0x8200000,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again_short = sensor_alloc_again_short,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_2592_1944_25fps[] = {
/* [SENSOR_INITIALIZATION] */
/* DISP_DATE = "2016-05-03 10:00:00" */
/* DISP_FORMAT = BAYER10_MIPI */
/* DISP_DATAORDER = GB */
/* MIPI_LANECNT = 2 */
/* I2C_SPEED = 400 */
/* MIPI_SETTLECNT = 0x15 */
/* V05_20161020 */

/* [SENSOR_RES_MOD] */
/* DISP_WIDTH = 2592 */
/* DISP_HEIGHT = 1944 */
/* DISP_NOTE = "2592x1944_30fps" */
/* MIPI_SPEED = 880.0 */
/* BEGIN */
/* I2C_BYTE = 0x22 */

/* Sensor Information//////////////////////////// */
/* Sensor	  : Hi-556 */
/* Image size	  : 2592x1944 */
/* MCLK		  : 24MHz */
/* MIPI speed(Mbps): 880Mbps x 2Lane */
/* Frame Length	  : 2082 */
/* Line Length 	  : 2816 */
/* Max Fps 	  : 30fps */
/* Pixel order 	  : Green 1st (=GB) */
/* X/Y-flip	  : X-flip */
/* BLC offset	  : 64code */


        {0x0e00, 0x0102},
        {0x0e02, 0x0102},
        {0x0e0c, 0x0100},
        {0x2000, 0x7400},
        {0x2002, 0x001c},
        {0x2004, 0x0242},
        {0x2006, 0x0942},
        {0x2008, 0x7007},
        {0x200a, 0x0fd9},
        {0x200c, 0x0259},
        {0x200e, 0x7008},
        {0x2010, 0x160e},
        {0x2012, 0x0047},
        {0x2014, 0x2118},
        {0x2016, 0x0041},
        {0x2018, 0x00d8},
        {0x201a, 0x0145},
        {0x201c, 0x0006},
        {0x201e, 0x0181},
        {0x2020, 0x13cc},
        {0x2022, 0x2057},
        {0x2024, 0x7001},
        {0x2026, 0x0fca},
        {0x2028, 0x00cb},
        {0x202a, 0x009f},
        {0x202c, 0x7002},
        {0x202e, 0x13cc},
        {0x2030, 0x019b},
        {0x2032, 0x014d},
        {0x2034, 0x2987},
        {0x2036, 0x2766},
        {0x2038, 0x0020},
        {0x203a, 0x2060},
        {0x203c, 0x0e5d},
        {0x203e, 0x181d},
        {0x2040, 0x2066},
        {0x2042, 0x20c4},
        {0x2044, 0x5000},
        {0x2046, 0x0005},
        {0x2048, 0x0000},
        {0x204a, 0x01db},
        {0x204c, 0x025a},
        {0x204e, 0x00c0},
        {0x2050, 0x0005},
        {0x2052, 0x0006},
        {0x2054, 0x0ad9},
        {0x2056, 0x0259},
        {0x2058, 0x0618},
        {0x205a, 0x0258},
        {0x205c, 0x2266},
        {0x205e, 0x20c8},
        {0x2060, 0x2060},
        {0x2062, 0x707b},
        {0x2064, 0x0fdd},
        {0x2066, 0x81b8},
        {0x2068, 0x5040},
        {0x206a, 0x0020},
        {0x206c, 0x5060},
        {0x206e, 0x3143},
        {0x2070, 0x5081},
        {0x2072, 0x025c},
        {0x2074, 0x7800},
        {0x2076, 0x7400},
        {0x2078, 0x001c},
        {0x207a, 0x0242},
        {0x207c, 0x0942},
        {0x207e, 0x0bd9},
        {0x2080, 0x0259},
        {0x2082, 0x7008},
        {0x2084, 0x160e},
        {0x2086, 0x0047},
        {0x2088, 0x2118},
        {0x208a, 0x0041},
        {0x208c, 0x00d8},
        {0x208e, 0x0145},
        {0x2090, 0x0006},
        {0x2092, 0x0181},
        {0x2094, 0x13cc},
        {0x2096, 0x2057},
        {0x2098, 0x7001},
        {0x209a, 0x0fca},
        {0x209c, 0x00cb},
        {0x209e, 0x009f},
        {0x20a0, 0x7002},
        {0x20a2, 0x13cc},
        {0x20a4, 0x019b},
        {0x20a6, 0x014d},
        {0x20a8, 0x2987},
        {0x20aa, 0x2766},
        {0x20ac, 0x0020},
        {0x20ae, 0x2060},
        {0x20b0, 0x0e5d},
        {0x20b2, 0x181d},
        {0x20b4, 0x2066},
        {0x20b6, 0x20c4},
        {0x20b8, 0x50a0},
        {0x20ba, 0x0005},
        {0x20bc, 0x0000},
        {0x20be, 0x01db},
        {0x20c0, 0x025a},
        {0x20c2, 0x00c0},
        {0x20c4, 0x0005},
        {0x20c6, 0x0006},
        {0x20c8, 0x0ad9},
        {0x20ca, 0x0259},
        {0x20cc, 0x0618},
        {0x20ce, 0x0258},
        {0x20d0, 0x2266},
        {0x20d2, 0x20c8},
        {0x20d4, 0x2060},
        {0x20d6, 0x707b},
        {0x20d8, 0x0fdd},
        {0x20da, 0x86b8},
        {0x20dc, 0x50e0},
        {0x20de, 0x0020},
        {0x20e0, 0x5100},
        {0x20e2, 0x3143},
        {0x20e4, 0x5121},
        {0x20e6, 0x7800},
        {0x20e8, 0x3140},
        {0x20ea, 0x01c4},
        {0x20ec, 0x01c1},
        {0x20ee, 0x01c0},
        {0x20f0, 0x01c4},
        {0x20f2, 0x2700},
        {0x20f4, 0x3d40},
        {0x20f6, 0x7800},
        {0x20f8, 0xffff},
        {0x27fe, 0xe000},
        {0x3000, 0x60f8},
        {0x3002, 0x187f},
        {0x3004, 0x7060},
        {0x3006, 0x0114},
        {0x3008, 0x60b0},
        {0x300a, 0x1473},
        {0x300c, 0x0013},
        {0x300e, 0x140f},
        {0x3010, 0x0040},
        {0x3012, 0x100f},
        {0x3014, 0x60f8},
        {0x3016, 0x187f},
        {0x3018, 0x7060},
        {0x301a, 0x0114},
        {0x301c, 0x60b0},
        {0x301e, 0x1473},
        {0x3020, 0x0013},
        {0x3022, 0x140f},
        {0x3024, 0x0040},
        {0x3026, 0x000f},
        {0x0b00, 0x0000},
        {0x0b02, 0x0045},
        {0x0b04, 0xb405},
        {0x0b06, 0xc403},
        {0x0b08, 0x0081},
        {0x0b0a, 0x8252},
        {0x0b0c, 0xf814},
        {0x0b0e, 0xc618},
        {0x0b10, 0xa828},
        {0x0b12, 0x002c},
        {0x0b14, 0x4068},
        {0x0b16, 0x0000},
        {0x0f30, 0x6e25},
        {0x0f32, 0x7067},
        {0x0954, 0x0009},
        {0x0956, 0x0000},
        {0x0958, 0xbb80},
        {0x095a, 0x5140},
        {0x0c00, 0x1110},
        {0x0c02, 0x0011},
        {0x0c04, 0x0000},
        {0x0c06, 0x0200},
        {0x0c10, 0x0040},
        {0x0c12, 0x0040},
        {0x0c14, 0x0040},
        {0x0c16, 0x0040},
        {0x0a10, 0x4000},
        {0x3068, 0xf800},
        {0x306a, 0xf876},
        {0x006c, 0x0000},
        {0x005e, 0x0200},
        {0x000e, 0x0100},
        {0x0e0a, 0x0001},
        {0x004a, 0x0100},
        {0x004c, 0x0000},
        {0x004e, 0x0100},
        {0x000c, 0x0022},
        {0x0008, 0x0b00},
        {0x005a, 0x0202},
        {0x0012, 0x000e},
        {0x0018, 0x0a31},
        {0x0022, 0x0008},
        {0x0028, 0x0017},
        {0x0024, 0x0028},
        {0x002a, 0x002d},
        {0x0026, 0x0030},
        {0x002c, 0x07c7},
        {0x002e, 0x1111},
        {0x0030, 0x1111},
        {0x0032, 0x1111},
        {0x0006, 0x09c2},
        {0x0a22, 0x0000},
        {0x0a12, 0x0a20},
        {0x0a14, 0x0798},
        {0x003e, 0x0000},
        {0x0073, 0x0000},
        {0x0074, 0x080e},
        {0x0075, 0x0000},
        {0x0070, 0x0407},
        {0x0002, 0x0000},
        {0x0a02, 0x0100},
        {0x0a24, 0x0100},
        {0x0046, 0x0000},
        {0x0076, 0x0000},
        {0x0060, 0x0000},
        {0x0062, 0x0530},
        {0x0064, 0x0500},
        {0x0066, 0x0530},
        {0x0068, 0x0500},
        {0x0122, 0x0300},
        {0x015a, 0xff08},
        {0x0804, 0x0208},
        {0x005c, 0x0102},
        {0x0a1a, 0x0800},
        {0x0a00, 0x0000},
        {0x0b0a, 0x8252},
        {0x0f30, 0x6e25},
        {0x0f32, 0x7067},
        {0x004a, 0x0100},
        {0x004c, 0x0000},
        {0x000c, 0x0022},
        {0x0008, 0x0b00},
        {0x005a, 0x0202},
        {0x0012, 0x000e},
        {0x0018, 0x0a31},
        {0x0022, 0x0008},
        {0x0028, 0x0017},
        {0x0024, 0x0028},
        {0x002a, 0x002d},
        {0x0026, 0x0030},
        {0x002c, 0x07c7},
        {0x002e, 0x1111},
        {0x0030, 0x1111},
        {0x0032, 0x1111},
        {0x0006, 0x0822},
        {0x0a22, 0x0000},
        {0x0a12, 0x0a20},
        {0x0a14, 0x0798},
        {0x003e, 0x0000},
        {0x0074, 0x0820},
        {0x0070, 0x0400},
        {0x0804, 0x0200},
        {0x0a04, 0x014a},
        {0x090c, 0x0fdc},
        {0x090e, 0x002d},
        {0x0902, 0x4319},
        {0x0914, 0xc10a},
        {0x0916, 0x071f},
        {0x0918, 0x0408},
        {0x091a, 0x0c0d},
        {0x091c, 0x0f09},
        {0x091e, 0x0a00},
        {0x0958, 0xbb80},

	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* [0] 2592*1944 @30fps*/
	{
		.width = 2592,
		.height = 1944,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SGBRG10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2592_1944_25fps,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sensor_stream_on[] = {
	{0x0a00, 0x0100},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	{0x0a00, 0x0000},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,
               uint8_t *value)
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
                uint16_t value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[4] = {(char)((reg >> 8) & 0xff), (char)(reg & 0xff), (char)((value >> 8) & 0xff), (char)(value & 0xff)};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 4,
		.buf = buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write_8(struct tx_isp_subdev *sd, uint16_t reg,
		  uint8_t value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(char)((reg >> 8) & 0xff), (char)(reg & 0xff), value & 0xff};
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
			msleep(vals->value);
		} else {
			ret = sensor_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%02x, vals->value:0x%02x\n",vals->reg_num, val);
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
	uint8_t v;
	int ret;

	ret = sensor_read(sd, 0xf16, &v);
	pr_debug("-----%s: %d ret = %d,  v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0xf17, &v);
	pr_debug("-----%s: %d ret = %d,  v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int expo = value;

	ret += sensor_write_8(sd, 0x0075, (unsigned char)(expo & 0xff));
	ret += sensor_write_8(sd, 0x0074, (unsigned char)((expo >> 8) & 0xff));
	ret += sensor_write_8(sd, 0x0073, (unsigned char)((expo >> 16) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sensor_write_8(sd, 0x0077, (unsigned char)((value & 0xff)));
	if (ret < 0)
		return ret;

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
			ret = sensor_write_array(sd, sensor_stream_on);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("%s stream on\n", SENSOR_NAME);
		}
	}
	else {
		ret = sensor_write_array(sd, sensor_stream_off);
		pr_debug("%s stream off\n", SENSOR_NAME);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}
	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = SENSOR_SUPPORT_SCLK_FPS_30;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = SENSOR_OUTPUT_MAX_FPS;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}

	ret += sensor_read(sd, 0x0008, &val);
	hts = val<<8;
	val = 0;
	ret += sensor_read(sd, 0x0009, &val);
	hts |= val;
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	/* ret = sensor_write(sd, 0x3208, 0x02); */
	ret += sensor_write(sd, 0x0007, vts & 0xff);
	ret += sensor_write(sd, 0x0006, (vts >> 8) & 0xff);
	/* ret += sensor_write(sd, 0x3208, 0x12); */
	/* ret += sensor_write(sd, 0x320d, 0x00); */
	/* ret += sensor_write(sd, 0x3208, 0xe2); */
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 2;
	sensor->video.attr->integration_time_limit = vts - 2;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 2;
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

	return 0;

err_get_mclk:
	return -1;
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
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(15);
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
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sensor_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sensor_set_analog_gain(sd, sensor_val->value);
		break;
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
		if (arg)
			ret = sensor_write_array(sd, sensor_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = sensor_write_array(sd, sensor_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, sensor_val->value);
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

static int sensor_probe(struct i2c_client *client,
                       const struct i2c_device_id *id)
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
	sensor->dev = &client->dev;
	sd = &sensor->sd;
	video = &sensor->video;
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
