// SPDX-License-Identifier: GPL-2.0+
/*
 * mis4001.c
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
#include <tx-isp-common.h>
#include <sensor-common.h>
#include <sensor-info.h>

#define SENSOR_NAME "mis4001"
#define SENSOR_VERSION "H20230317a"
#define SENSOR_CHIP_ID 0x1311
#define SENSOR_CHIP_ID_H (0x13)
#define SENSOR_CHIP_ID_L (0x11)
#define SENSOR_I2C_ADDRESS 0x30
#define SENSOR_MAX_WIDTH 2560
#define SENSOR_MAX_HEIGHT 1440
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_RES_400_PCLK (135600000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define MCLK 24000000


static int reset_gpio = -1;
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Default reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Default power down GPIO NUM");

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
	{0x0, 0},
	{0x1, 2909},
	{0x2, 5731},
	{0x3, 8472},
	{0x4, 11136},
	{0x5, 13726},
	{0x6, 16247},
	{0x7, 18703},
	{0x8, 21097},
	{0x9, 23432},
	{0xa, 25710},
	{0xb, 27935},
	{0xc, 30108},
	{0xd, 32233},
	{0xe, 34311},
	{0xf, 36344},
	{0x10, 38335},
	{0x11, 40285},
	{0x12, 42195},
	{0x13, 44067},
	{0x14, 45903},
	{0x15, 47704},
	{0x16, 49471},
	{0x17, 51206},
	{0x18, 52910},
	{0x19, 54583},
	{0x1a, 56227},
	{0x1b, 57844},
	{0x1c, 59433},
	{0x1d, 60995},
	{0x1e, 62533},
	{0x1f, 64046},
	{0x20, 65535},
	{0x21, 68444},
	{0x22, 71266},
	{0x23, 74007},
	{0x24, 76671},
	{0x25, 79261},
	{0x26, 81782},
	{0x27, 84238},
	{0x28, 86632},
	{0x29, 88967},
	{0x2a, 91245},
	{0x2b, 93470},
	{0x2c, 95643},
	{0x2d, 97768},
	{0x2e, 99846},
	{0x2f, 101879},
	{0x30, 103870},
	{0x31, 105820},
	{0x32, 107730},
	{0x33, 109602},
	{0x34, 111438},
	{0x35, 113239},
	{0x36, 115006},
	{0x37, 116741},
	{0x38, 118445},
	{0x39, 120118},
	{0x3a, 121762},
	{0x3b, 123379},
	{0x3c, 124968},
	{0x3d, 126530},
	{0x3e, 128068},
	{0x3f, 129581},
	{0x40, 131070},
	{0x41, 133979},
	{0x42, 136801},
	{0x43, 139542},
	{0x44, 142206},
	{0x45, 144796},
	{0x46, 147317},
	{0x47, 149773},
	{0x48, 152167},
	{0x49, 154502},
	{0x4a, 156780},
	{0x4b, 159005},
	{0x4c, 161178},
	{0x4d, 163303},
	{0x4e, 165381},
	{0x4f, 167414},
	{0x50, 169405},
	{0x51, 171355},
	{0x52, 173265},
	{0x53, 175137},
	{0x54, 176973},
	{0x55, 178774},
	{0x56, 180541},
	{0x57, 182276},
	{0x58, 183980},
	{0x59, 185653},
	{0x5a, 187297},
	{0x5b, 188914},
	{0x5c, 190503},
	{0x5d, 192065},
	{0x5e, 193603},
	{0x5f, 195116},
	{0x60, 196605},
	{0x61, 199514},
	{0x62, 202336},
	{0x63, 205077},
	{0x64, 207741},
	{0x65, 210331},
	{0x66, 212852},
	{0x67, 215308},
	{0x68, 217702},
	{0x69, 220037},
	{0x6a, 222315},
	{0x6b, 224540},
	{0x6c, 226713},
	{0x6d, 228838},
	{0x6e, 230916},
	{0x6f, 232949},
	{0x70, 234940},
	{0x71, 236890},
	{0x72, 238800},
	{0x73, 240672},
	{0x74, 242508},
	{0x75, 244309},
	{0x76, 246076},
	{0x77, 247811},
	{0x78, 249515},
	{0x79, 251188},
	{0x7a, 252832},
	{0x7b, 254449},
	{0x7c, 256038},
	{0x7d, 257600},
	{0x7e, 259138},
	{0x7f, 260651},
};

struct tx_isp_sensor_attribute sensor_attr;

#if 0
unsigned int sensor_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> (shift & 0x1f);

	*sensor_it = expo;

	return it;
}
#endif

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;

	if (isp_gain == 0) {
		*sensor_again = lut->value;
		return 0;
	}

	while (lut->gain < sensor_attr.max_again) {
		if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		lut++;
	}

	*sensor_again = lut->value;
	return lut->gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 678,
	.lans = 2,
	.image_twidth = 2560,
	.image_theight = 1440,
	.settle_time_apative_en = 0,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
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
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
};

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 260651,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1500 - 4,
	.integration_time_limit = 1500 - 4,
	.integration_time = 1500 - 4, // NOTE: check this, stock driver write this
	.expo_fs = 1,
	.total_width = 3000,
	.total_height = 1500,
	.max_integration_time = 1500 - 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_2560_1440_30fps[] = {
	{0x300a, 0x01},
	{0x3006, 0x02},
	{0x3306, 0x01},
	{0x3309, 0x01},
	{0x3308, 0x03},
	{0x330a, 0x04},
	{0x330b, 0x09},
	{0x310f, 0xb8},
	{0x310e, 0x0b},
	{0x310d, 0xdc},
	{0x310c, 0x05},
	{0x3115, 0x10},
	{0x3114, 0x00},
	{0x3117, 0x0f},
	{0x3116, 0x0a},
	{0x3111, 0xfc},
	{0x3110, 0x00},
	{0x3113, 0x9d},
	{0x3112, 0x06},
	{0x3128, 0x0f},
	{0x3129, 0xff},
	{0x3012, 0x03},
	{0x3f00, 0x01},
	{0x3f02, 0x07},
	{0x3f01, 0x00},
	{0x3f04, 0x2a},
	{0x3f03, 0x00},
	{0x3f06, 0xa5},
	{0x3f05, 0x04},
	{0x3f08, 0xff},
	{0x3f07, 0x1f},
	{0x3f0a, 0xa4},
	{0x3f09, 0x01},
	{0x3f0c, 0x38},
	{0x3f0b, 0x00},
	{0x3f0e, 0xff},
	{0x3f0d, 0x1f},
	{0x3f10, 0xff},
	{0x3f0f, 0x1f},
	{0x3f13, 0x07},
	{0x3f12, 0x00},
	{0x3f15, 0x9d},
	{0x3f14, 0x01},
	{0x3f17, 0x31},
	{0x3f16, 0x00},
	{0x3f19, 0x73},
	{0x3f18, 0x01},
	{0x3f1b, 0x00},
	{0x3f1a, 0x00},
	{0x3f1d, 0xa9},
	{0x3f1c, 0x04},
	{0x3f1f, 0xff},
	{0x3f1e, 0x1f},
	{0x3f21, 0xff},
	{0x3f20, 0x1f},
	{0x3f23, 0x85},
	{0x3f22, 0x00},
	{0x3f25, 0x27},
	{0x3f24, 0x01},
	{0x3f28, 0x46},
	{0x3f27, 0x00},
	{0x3f2a, 0x07},
	{0x3f29, 0x00},
	{0x3f2c, 0x3f},
	{0x3f2b, 0x00},
	{0x3f2e, 0x70},
	{0x3f2d, 0x01},
	{0x3f30, 0x38},
	{0x3f2f, 0x00},
	{0x3f32, 0x3f},
	{0x3f31, 0x00},
	{0x3f34, 0xd1},
	{0x3f33, 0x00},
	{0x3f36, 0xc0},
	{0x3f35, 0x00},
	{0x3f38, 0x2f},
	{0x3f37, 0x02},
	{0x3f3a, 0x5d},
	{0x3f39, 0x02},
	{0x3f4f, 0x5d},
	{0x3f4e, 0x02},
	{0x3f51, 0x5d},
	{0x3f50, 0x02},
	{0x3f53, 0x5d},
	{0x3f52, 0x02},
	{0x3f55, 0x5d},
	{0x3f54, 0x02},
	{0x3f3c, 0x9a},
	{0x3f3b, 0x00},
	{0x3f3e, 0x09},
	{0x3f3d, 0x04},
	{0x3f40, 0x93},
	{0x3f3f, 0x01},
	{0x3f42, 0x8f},
	{0x3f41, 0x00},
	{0x3f44, 0xb0},
	{0x3f43, 0x04},
	{0x3129, 0x45},
	{0x3128, 0x00},
	{0x312b, 0x4a},
	{0x312a, 0x00},
	{0x312f, 0xb2},
	{0x312e, 0x00},
	{0x3124, 0x09},
	{0x4200, 0x09},
	{0x4201, 0x00},
	{0x4202, 0xa0},
	{0x4204, 0xff},
	{0x4205, 0x3f},
	{0x4214, 0x80},
	{0x420c, 0x80},
	{0x420e, 0xd0},
	{0x4103, 0x3f},
	{0x4240, 0x8d},
	{0x4242, 0x03},
	{0x4224, 0x00},
	{0x4225, 0x0a},
	{0x4226, 0xa0},
	{0x4227, 0x05},
	{0x4228, 0x00},
	{0x4229, 0x0a},
	{0x422a, 0xa0},
	{0x422b, 0x05},
	{0x422c, 0x00},
	{0x422d, 0x0a},
	{0x422e, 0xa0},
	{0x422f, 0x05},
	{0x4230, 0x00},
	{0x4231, 0x0a},
	{0x4232, 0xa0},
	{0x4233, 0x05},
	{0x4509, 0x0f},
	{0x4505, 0x00},
	{0x4501, 0xff},
	{0x4502, 0x33},
	{0x4503, 0x11},
	{0x4501, 0xf0},
	{0x4502, 0x30},
	{0x4503, 0x10},
	{0x3a01, 0xa0},
	{0x401e, 0x3c},
	{0x401d, 0xa0},
	{0x3012, 0x03},
	{0x3e00, 0x00},
	{0x3e01, 0x10},
	{0x400d, 0x30},
	{0x3500, 0x1b},
	{0x3501, 0x03},
	{0x3508, 0x0a},
	{0x3508, 0x04},
	{0x3513, 0x01},
	{0x3514, 0x09},
	{0x3515, 0x0b},
	{0x3702, 0x80},
	{0x3704, 0x80},
	{0x3706, 0x80},
	{0x3708, 0x80},
	{0x400d, 0x30},
	{0x4004, 0x40},
	{0x4005, 0x0c},
	{0x4009, 0x09},
	{0x400a, 0x48},
	{0x4006, 0x86},
	{0x4019, 0x08},
	{0x3f42, 0x58},
	{0x3f49, 0x60},
	{0x3f38, 0x38},
	{0x3307, 0x71},
	{0x3302, 0x02},
	{0x4200, 0x09},
	{0x4201, 0x00},
	{0x4204, 0xff},
	{0x4205, 0x3f},
	{0x4214, 0x60},
	{0x420c, 0x50},
	{0x420e, 0x94},
	{0x4103, 0x3f},
	{0x4220, 0x2b},
	{0x4221, 0x6b},
	{0x4222, 0xab},
	{0x4223, 0xeb},
	{0x3011, 0x2b},
	{0x4216, 0x6c},
	{0x4217, 0xdc},
	{0x4218, 0x02},
	{0x3006, 0x00},
	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 2560,
		.height = 1440,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SGRBG10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2560_1440_30fps,
	}
};

// This is the global wsize state
static struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];


static struct regval_list sensor_stream_on[] = {
	{0x3006, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	{0x3006, 0x02},
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
	unsigned char v;
	int ret;
	ret = sensor_read(sd, 0x3000, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x3001, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 16) | v;
	return 0;
}

static int sensor_set_expo(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	unsigned int integration_time = (value & 0xffff);
	unsigned int analog_gain = (value >> 16) & 0xffff;

	ret = sensor_write(sd, 0x3100, (unsigned char) ((integration_time >> 8) & 0xff));
	ret += sensor_write(sd, 0x3101, (unsigned char) (integration_time & 0xff));
	ret += sensor_write(sd, 0x3102, (unsigned char) (analog_gain & 0xff));

	if (analog_gain < 0x20) {
		ret += sensor_write(sd, 0x401d, 0xa0);
		ret += sensor_write(sd, 0x4003, 0x0b);
		ret += sensor_write(sd, 0x4007, 0x78);
	} else {
		ret += sensor_write(sd, 0x401d, 0xa7);
		ret += sensor_write(sd, 0x4003, 0x0a);
		ret += sensor_write(sd, 0x4007, 0xc4);
	}

	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = sensor_write(sd,  0x3100, (unsigned char)((value >> 8)& 0xff));
	ret += sensor_write(sd, 0x3101, (unsigned char)(value & 0xff));
	if (ret < 0)
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);

	return ret;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = sensor_write(sd, 0x3102, (unsigned char)(value));
	if (ret < 0)
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);

	return ret;
}
#endif

static int sensor_set_logic(struct tx_isp_subdev *sd, int value)
{
	return -1;
}

#if 0
static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}
#endif

static int sensor_set_attr(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wsize_set) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	sensor->video.vi_max_width = wsize_set->width;
	sensor->video.vi_max_height = wsize_set->height;
	sensor->video.mbus.width = wsize_set->width;
	sensor->video.mbus.height = wsize_set->height;
	sensor->video.mbus.code = wsize_set->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize_set->colorspace;
	sensor->video.fps = wsize_set->fps;

	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable) {
		sensor->video.state = TX_ISP_MODULE_DEINIT;
		return ISP_SUCCESS;
	}

	sensor_set_attr(sd, wsize);
	ret = sensor_write_array(sd, wsize->regs);
	sensor->video.state = TX_ISP_MODULE_ACTIVATE;
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	int ret = 0;

	if (init->enable) {
		ret = sensor_write_array(sd, sensor_stream_on);
		ISP_WARNING("%s stream on\n", SENSOR_NAME);
	} else {
		ret = sensor_write_array(sd, sensor_stream_off);
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

/*
   NOTE: the decompiled version somehow modifies VTS directly, and turns off the
   stream when applying parameters.
*/
static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int pclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int sensor_max_fps;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8

	switch (sensor->info.default_boot) {
		case 0:
			pclk = SENSOR_SUPPORT_RES_400_PCLK;
			sensor_max_fps = TX_SENSOR_MAX_FPS_30;
			break;
		default:
			ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (sensor_max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}

	ret = sensor_read(sd, 0x310c, &tmp);
	vts = tmp;
	ret += sensor_read(sd, 0x310d, &tmp);
	if (ret < 0)
		return -1;
	vts = ((vts << 8) + tmp);
	hts = pclk * (fps & 0xffff) / vts / ((fps & 0xffff0000) >> 16);

	// ret = sensor_write(sd, 0x3006, 0x02); // stream off
	ret = sensor_write(sd, 0x300b, 0x00);
	ret += sensor_write(sd, 0x310f, (unsigned char)(hts & 0xff));
	ret += sensor_write(sd, 0x310e, (unsigned char)(hts >> 8));
	ret += sensor_write(sd, 0x300b, 0x01);
	// ret += sensor_write(sd, 0x3006, 0x00); // stream on
	if (ret < 0) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}

	*((u32 *) 0xb3380000) = 0x5;
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	if (ret) {
		ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
	}

	return ret;
}

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	switch (enable) {
		case 0:
			sensor_write(sd, 0x3007, 0x00);
			sensor_write(sd, 0x3115, 0x10);
			sensor_write(sd, 0x3117, 0x0f);
			sensor_write(sd, 0x3111, 0xfc);
			sensor_write(sd, 0x3113, 0x9d);
			break;
		case 1:
			sensor_write(sd, 0x3007, 0x01);
			sensor_write(sd, 0x3115, 0x0f);
			sensor_write(sd, 0x3117, 0x0e);
			sensor_write(sd, 0x3111, 0xfc);
			sensor_write(sd, 0x3113, 0x9d);
			break;
		case 2:
			sensor_write(sd, 0x3007, 0x02);
			sensor_write(sd, 0x3115, 0x10);
			sensor_write(sd, 0x3117, 0x0f);
			sensor_write(sd, 0x3111, 0xfd);
			sensor_write(sd, 0x3113, 0x9e);
			break;
		case 3:
			sensor_write(sd, 0x3007, 0x03);
			sensor_write(sd, 0x3115, 0x0f);
			sensor_write(sd, 0x3117, 0x0e);
			sensor_write(sd, 0x3111, 0xfd);
			sensor_write(sd, 0x3113, 0x9e);
			break;
	}

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	*((u32 *) 0xb3380000) = 0x5;
	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}
	return ret;
}

static int sensor_attr_check(struct tx_isp_subdev *sd) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);

	switch (info->default_boot) {
		case 0:
			wsize = &sensor_win_sizes[0];
			memcpy(&(sensor_attr.mipi), &sensor_mipi, sizeof(sensor_mipi));
			break;
		default:
			ISP_ERROR("No this setting(boot: %d)!!! Default to 0!\n", info->default_boot);
			wsize = &sensor_win_sizes[0];
			memcpy(&(sensor_attr.mipi), &sensor_mipi, sizeof(sensor_mipi));
	}

	switch (info->video_interface) {
		case TISP_SENSOR_VI_MIPI_CSI0:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.mipi.index = 0;
			break;
		case TISP_SENSOR_VI_MIPI_CSI1:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.mipi.index = 1;
			break;
		/*
		case TISP_SENSOR_VI_DVP:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
			break;
		*/
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

	private_clk_get_rate(sensor->mclk);
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	private_clk_set_rate(sensor->mclk, MCLK);
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
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);

	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(30);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(15);
		} else {
			ISP_ERROR("gpio request fail %d\n",reset_gpio);
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
			ISP_ERROR("gpio request fail %d\n",pwdn_gpio);
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
	switch (cmd) {
/*
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
*/
		case TX_ISP_EVENT_SENSOR_FPS:
			if (arg)
				ret = sensor_set_fps(sd, sensor_val->value);
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
		case TX_ISP_EVENT_SENSOR_VFLIP:
			if (arg)
				ret = sensor_set_vflip(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_LOGIC:
			if (arg)
				ret = sensor_set_logic(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_EXPO:
			if (arg)
				ret = sensor_set_expo(sd, sensor_val->value);
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
	.init = sensor_init,
	.reset = sensor_reset,
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

	sensor->dev = &client->dev;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sensor_attr;

	sensor_set_attr(sd, wsize);
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
	// private_clk_put(sensor->mclk);
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
