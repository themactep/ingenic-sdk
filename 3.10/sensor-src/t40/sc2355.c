// SPDX-License-Identifier: GPL-2.0+
/*
 * sc2355.c
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

#define SENSOR_NAME "sc2355"
#define SENSOR_CHIP_ID_H (0xeb)
#define SENSOR_CHIP_ID_L (0x2c)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_60FPS_SCLK (72000000)
#define SENSOR_OUTPUT_MAX_FPS 60
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_1
#define MCLK 24000000
#define SENSOR_VERSION "H20220609a"
#define DUAL_CAMERA_MODE 1
#define MAIN_SENSOR 1
#define SECOND_SENSOR 0
#define SENSOR_WITHOUT_INIT 1

static int reset_gpio = GPIO_PC(28);
static int pwdn_gpio = -1;

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

char* __attribute__((weak)) sclk_name[4];

struct again_lut {
	unsigned int index;
	unsigned int again_val;
	unsigned int dgain_coarse_val;
	unsigned int dgain_fine_val;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x0, 0x0, 0x0, 0x80, 0},   // 1x  again
	{0x1, 0x0, 0x0, 0x84, 2886},
	{0x2, 0x0, 0x0, 0x88, 5776},
	{0x3, 0x0, 0x0, 0x8c, 8494},
	{0x4, 0x0, 0x0, 0x90, 11136},
	{0x5, 0x0, 0x0, 0x94, 13706},
	{0x6, 0x0, 0x0, 0x98, 16287},
	{0x7, 0x0, 0x0, 0x9c, 18723},
	{0x8, 0x0, 0x0, 0xa0, 21097},
	{0x9, 0x0, 0x0, 0xa4, 23413},
	{0xa, 0x0, 0x0, 0xa8, 25746},
	{0xb, 0x0, 0x0, 0xac, 27952},
	{0xc, 0x0, 0x0, 0xb0, 30108},
	{0xd, 0x0, 0x0, 0xb4, 32216},
	{0xe, 0x0, 0x0, 0xb8, 34344},
	{0xf, 0x0, 0x0, 0xbc, 36361},
	{0x10, 0x0, 0x0, 0xc0, 38335},
	{0x11, 0x0, 0x0, 0xc4, 40269},
	{0x12, 0x0, 0x0, 0xc8, 42225},
	{0x13, 0x0, 0x0, 0xcc, 44082},
	{0x14, 0x0, 0x0, 0xd0, 45903},
	{0x15, 0x0, 0x0, 0xd4, 47689},
	{0x16, 0x0, 0x0, 0xd8, 49499},
	{0x17, 0x0, 0x0, 0xdc, 51220},
	{0x18, 0x0, 0x0, 0xe0, 52910},
	{0x19, 0x0, 0x0, 0xe4, 54570},
	{0x1a, 0x0, 0x0, 0xe8, 56253},
	{0x1b, 0x0, 0x0, 0xec, 57856},
	{0x1c, 0x0, 0x0, 0xf0, 59433},
	{0x1d, 0x0, 0x0, 0xf4, 60983},
	{0x1e, 0x0, 0x0, 0xf8, 62557},
	{0x1f, 0x0, 0x0, 0xfc, 64058},
	{0x20, 0x1, 0x0, 0x80, 65535},  // 2x again
	{0x21, 0x1, 0x0, 0x86, 69877},
	{0x22, 0x1, 0x0, 0x8c, 74029},
	{0x23, 0x1, 0x0, 0x92, 77964},
	{0x24, 0x1, 0x0, 0x98, 81782},
	{0x25, 0x1, 0x0, 0x9e, 85452},
	{0x26, 0x1, 0x0, 0xa4, 88985},
	{0x27, 0x1, 0x0, 0xaa, 92355},
	{0x28, 0x1, 0x0, 0xb0, 95643},
	{0x29, 0x1, 0x0, 0xb6, 98821},
	{0x2a, 0x1, 0x0, 0xbc, 101896},
	{0x2b, 0x1, 0x0, 0xc2, 104842},
	{0x2c, 0x1, 0x0, 0xc8, 107730},
	{0x2d, 0x1, 0x0, 0xce, 110532},
	{0x2e, 0x1, 0x0, 0xd4, 113253},
	{0x2f, 0x1, 0x0, 0xda, 115871},
	{0x30, 0x1, 0x0, 0xe0, 118445},
	{0x31, 0x1, 0x0, 0xe6, 120950},
	{0x32, 0x1, 0x0, 0xec, 123391},
	{0x33, 0x1, 0x0, 0xf2, 125746},
	{0x34, 0x1, 0x0, 0xf8, 128068},
	{0x35, 0x3, 0x0, 0x80, 131070},  // 4x  again
	{0x36, 0x3, 0x0, 0x88, 136801},
	{0x37, 0x3, 0x0, 0x90, 142206},
	{0x38, 0x3, 0x0, 0x98, 147317},
	{0x39, 0x3, 0x0, 0xa0, 152167},
	{0x3a, 0x3, 0x0, 0xa8, 156780},
	{0x3b, 0x3, 0x0, 0xb0, 161178},
	{0x3c, 0x3, 0x0, 0xb8, 165381},
	{0x3d, 0x3, 0x0, 0xc0, 169405},
	{0x3e, 0x3, 0x0, 0xc8, 173265},
	{0x3f, 0x3, 0x0, 0xd0, 176973},
	{0x40, 0x3, 0x0, 0xd8, 180541},
	{0x41, 0x3, 0x0, 0xe0, 183980},
	{0x42, 0x3, 0x0, 0xe8, 187297},
	{0x43, 0x3, 0x0, 0xf0, 190503},
	{0x44, 0x3, 0x0, 0xf8, 193603},
	{0x45, 0x7, 0x0, 0x80, 196605},  // 8x again
	{0x46, 0x7, 0x0, 0x88, 202381},  // 8x again * (1~2 dgain)
	{0x47, 0x7, 0x0, 0x92, 209076},
	{0x48, 0x7, 0x0, 0x9c, 215328},
	{0x49, 0x7, 0x0, 0xa6, 221192},
	{0x4a, 0x7, 0x0, 0xb0, 226713},
	{0x4b, 0x7, 0x0, 0xba, 231930},
	{0x4c, 0x7, 0x0, 0xc4, 236874},
	{0x4d, 0x7, 0x0, 0xce, 241572},
	{0x4e, 0x7, 0x0, 0xd8, 246104},
	{0x4f, 0x7, 0x0, 0xe2, 250375},
	{0x50, 0x7, 0x0, 0xec, 254461},
	{0x51, 0x7, 0x0, 0xf6, 258378},
	{0x52, 0xf, 0x0, 0x80, 262140},  // 16x again
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again)
	{
		if (isp_gain <= 0)
		{
			*sensor_again = lut[0].index;
			return lut[0].gain;
		}
		else if (isp_gain < lut->gain)
		{
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		}
		else
		{
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain))
			{
				*sensor_again = lut->index;
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

struct tx_isp_mipi_bus sensor_mipi={
   .mode = SENSOR_MIPI_OTHER_MODE,
   .clk = 360,
   .lans = 1,
   .settle_time_apative_en = 0,
   .mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
   .mipi_sc.hcrop_diff_en = 0,
   .mipi_sc.mipi_vcomp_en = 0,
   .mipi_sc.mipi_hcomp_en = 0,
   .mipi_sc.line_sync_mode = 0,
   .mipi_sc.work_start_flag = 0,
   .image_twidth = 640,
   .image_theight = 360,
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
};

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0xeb2c,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.max_again = 262140,
	.min_integration_time = 2,
	.max_integration_time = 0x4e2 - 4,
	.min_integration_time_native = 2,
	.max_integration_time_native = 0x4e2 - 4,
	.integration_time_limit = 0x4e2 - 4,
	.total_width = 0x780,
	.total_height = 0x4e2,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_640_360_60fps_mipi[] = {
    {0x0103, 0x01},
    {0x0100, 0x00},
    {0x36e9, 0x80},
    {0x36ea, 0x0f},
    {0x36eb, 0x24},
    {0x36ed, 0x14},
    {0x36e9, 0x01},
    {0x301f, 0x07},
    {0x303f, 0x82},
    {0x3200, 0x00},
    {0x3201, 0x9c},
    {0x3202, 0x00},
    {0x3203, 0xec},
    {0x3204, 0x05},
    {0x3205, 0xab},
    {0x3206, 0x03},
    {0x3207, 0xcb},
    {0x3208, 0x02},
    {0x3209, 0x80},
    {0x320a, 0x01},
    {0x320b, 0x68},
    {0x320e, 0x04},
    {0x320f, 0xe2},
    {0x3210, 0x00},
    {0x3211, 0x08},
    {0x3212, 0x00},
    {0x3213, 0x08},
    {0x3215, 0x31},
    {0x3220, 0x01},
    {0x3248, 0x02},
    {0x3253, 0x0a},
    {0x3301, 0xff},
    {0x3302, 0xff},
    {0x3303, 0x10},
    {0x3306, 0x28},
    {0x3307, 0x02},
    {0x330a, 0x00},
    {0x330b, 0xb0},
    {0x3318, 0x02},
    {0x3320, 0x06},
    {0x3321, 0x02},
    {0x3326, 0x12},
    {0x3327, 0x0e},
    {0x3328, 0x03},
    {0x3329, 0x0f},
    {0x3364, 0x4f},
    {0x33b3, 0x40},
    {0x33f9, 0x2c},
    {0x33fb, 0x38},
    {0x33fc, 0x0f},
    {0x33fd, 0x1f},
    {0x349f, 0x03},
    {0x34a6, 0x01},
    {0x34a7, 0x1f},
    {0x34a8, 0x40},
    {0x34a9, 0x30},
    {0x34ab, 0xa6},
    {0x34ad, 0xa6},
    {0x3622, 0x60},
    {0x3623, 0x40},
    {0x3624, 0x61},
    {0x3625, 0x08},
    {0x3626, 0x03},
    {0x3630, 0xa8},
    {0x3631, 0x84},
    {0x3632, 0x90},
    {0x3633, 0x43},
    {0x3634, 0x09},
    {0x3635, 0x82},
    {0x3636, 0x48},
    {0x3637, 0xe4},
    {0x3641, 0x22},
    {0x3670, 0x0f},
    {0x3674, 0xc0},
    {0x3675, 0xc0},
    {0x3676, 0xc0},
    {0x3677, 0x86},
    {0x3678, 0x88},
    {0x3679, 0x8c},
    {0x367c, 0x01},
    {0x367d, 0x0f},
    {0x367e, 0x01},
    {0x367f, 0x0f},
    {0x3690, 0x63},
    {0x3691, 0x63},
    {0x3692, 0x73},
    {0x369c, 0x01},
    {0x369d, 0x1f},
    {0x369e, 0x8a},
    {0x369f, 0x9e},
    {0x36a0, 0xda},
    {0x36a1, 0x01},
    {0x36a2, 0x03},
    {0x3900, 0x0d},
    {0x3904, 0x04},
    {0x3905, 0x98},
    {0x391b, 0x81},
    {0x391c, 0x10},
    {0x391d, 0x19},
    {0x3933, 0x01},
    {0x3934, 0x82},
    {0x3940, 0x5d},
    {0x3942, 0x01},
    {0x3943, 0x82},
    {0x3949, 0xc8},
    {0x394b, 0x64},
    {0x3952, 0x02},
    {0x3e00, 0x00},
    {0x3e01, 0x26},
    {0x3e02, 0xb0},
    {0x4502, 0x34},
    {0x4509, 0x30},
    {0x450a, 0x71},
    {0x4819, 0x05},
    {0x481b, 0x03},
    {0x481d, 0x0a},
    {0x481f, 0x02},
    {0x4821, 0x08},
    {0x4823, 0x03},
    {0x4825, 0x02},
    {0x4827, 0x03},
    {0x4829, 0x04},
    {0x5000, 0x46},
    {0x5900, 0xf1},
    {0x5901, 0x04},

    //vbin_QC
    {0x3215,0x22},
    {0x337e,0xa0},
    {0x3905,0x9c},
    {0x3032,0x60},
    //hbin_QC
    {0x5900,0xf5},

    //{0x0100, 0x01},
    {SENSOR_REG_DELAY,10},
    {SENSOR_REG_END, 0x00},
};


static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* [0] 640*360 @max 60fps */
	{
		.width = 640,
		.height = 360,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_640_360_60fps_mipi,
	}
};
static struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

/*
 * the part of driver was fixed.
 */

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

#if 1
static int sensor_set_expo(struct tx_isp_subdev *sd, int value)
{
	struct again_lut *val_lut = sensor_again_lut;
	int it = (value & 0xffff);
	int index = (value & 0xffff0000) >> 16;
	int ret = 0;

	/* set integration time */
	ret += sensor_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
	/*set sensor gain */
    ret += sensor_write(sd, 0x3e09, val_lut[index].again_val);
    ret += sensor_write(sd, 0x3e06, val_lut[index].dgain_coarse_val);
    ret += sensor_write(sd, 0x3e07, val_lut[index].dgain_fine_val);
	if (ret < 0)
		return ret;

	return 0;
}
#endif

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
	struct again_lut *val_lut = sensor_again_lut;
	int index = value;
	int ret = 0;

    ret = sensor_write(sd, 0x3e09, val_lut[index].again_val);
    ret = sensor_write(sd, 0x3e06, val_lut[index].dgain_coarse_val);
    ret = sensor_write(sd, 0x3e07, val_lut[index].dgain_fine_val);
	if (ret < 0)
		return ret;

	return 0;
}
#endif

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

	sensor_set_attr(sd, wsize);
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
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
			ISP_WARNING("%s stream on\n", SENSOR_NAME);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
	}
	else {
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}
	sclk = SENSOR_SUPPORT_60FPS_SCLK;

	ret += sensor_read(sd, 0x320c, &val);
	hts = val << 8;
	ret += sensor_read(sd, 0x320d, &val);
	hts = (hts | val);
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = sensor_write(sd, 0x320f, (unsigned char)(vts & 0xff));
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
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static struct clk *sclka;
static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	unsigned long rate;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);

	memcpy(&(sensor_attr.mipi), &sensor_mipi, sizeof(sensor_mipi));
	switch(info->default_boot) {
	case 0:
		wsize = &sensor_win_sizes[0];
	    sensor_attr.again = 0;
            sensor_attr.integration_time = 0x0026b0;
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
	default:
		ISP_ERROR("Have no this interface!!!\n");
	}

	switch(info->mclk) {
	case TISP_SENSOR_MCLK0:
		sclka = private_devm_clk_get(&client->dev, "mux_cim0");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim0");
		set_sensor_mclk_function(0);
		break;
	case TISP_SENSOR_MCLK1:
		sclka = private_devm_clk_get(&client->dev, "mux_cim1");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim1");
		set_sensor_mclk_function(1);
		break;
	case TISP_SENSOR_MCLK2:
		sclka = private_devm_clk_get(&client->dev, "mux_cim2");
		sensor->mclk = private_devm_clk_get(sensor->dev, "div_cim2");
		set_sensor_mclk_function(2);
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
	}

	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	rate = private_clk_get_rate(sensor->mclk);

#if 0
	if (((rate / 1000) % (MCLK / 1000)) != 0) {
		uint8_t sclk_name_num = sizeof(sclk_name)/sizeof(sclk_name[0]);
		for (i=0; i < sclk_name_num; i++) {
			tclk = private_devm_clk_get(&client->dev, sclk_name[i]);
			ret = clk_set_parent(sclka, clk_get(NULL, sclk_name[i]));
			if (IS_ERR(tclk)) {
				pr_err("get sclka failed\n");
			} else {
				rate = private_clk_get_rate(tclk);
				if (i == sclk_name_num - 1 && ((rate / 1000) % (MCLK / 1000)) != 0) {
					if (((MCLK / 1000) % 27000) != 0 || ((MCLK / 1000) % 37125) != 0)
						private_clk_set_rate(tclk, 891000000);
					else if (((MCLK / 1000) % 24000) != 0)
						private_clk_set_rate(tclk, 1200000000);
				} else if (((rate / 1000) % (MCLK / 1000)) == 0) {
					break;
				}
			}
		}
	}
#endif

	private_clk_set_rate(sensor->mclk, MCLK);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;

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
	case TX_ISP_EVENT_SENSOR_INT_TIME:
//		if (arg)
//			ret = sensor_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
//		if (arg)
//			ret = sensor_set_analog_gain(sd, sensor_val->value);
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
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sensor_write_array(sd, sensor_stream_on_mipi);
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
struct platform_device sensor_sensor_platform_device = {
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

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->dev = &client->dev;
	sensor_attr.expo_fs = 1;
	sensor->video.attr = &sensor_attr;
	tx_isp_subdev_init(&sensor_sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

	// gpio_request(sensor_light_ctl, "sensor_light_ctl");
	// gpio_request(GPIO_PA(20), "sensor_reset");
	// request_irq(gpio_to_irq(GPIO_PA(20)), IRQ_TYPE_EDGE_RISING, strobe_irq_handler, "sensor_strobe", (void *)&strobe_cnt);
	// gpio_direction_output(sensor_light_ctl, 0);
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
