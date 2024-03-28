// SPDX-License-Identifier: GPL-2.0+
/*
 * imx415.c
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          3840*2160       30        mipi_2lane            linear
 *   1          3840*2160       20        mipi_2lane            linear
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
#include <txx-funcs.h>

#define SENSOR_WITHOUT_INIT

#define SENSOR_NAME "imx415"
#define SENSOR_CHIP_ID_H (0x28)
#define SENSOR_CHIP_ID_L (0x23)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_OUTPUT_MIN_FPS 5
#define AGAIN_MAX_DB 0x64
#define DGAIN_MAX_DB 0x64
#define LOG2_GAIN_SHIFT 16
#define SENSOR_VERSION "H20221104a"


static int reset_gpio = -1;
static int pwdn_gpio = -1;

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct tx_isp_sensor_attribute imx415_attr;

unsigned int imx415_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	uint16_t again = (isp_gain * 20) >> LOG2_GAIN_SHIFT;
	// Limit Max gain
	if (again > AGAIN_MAX_DB + DGAIN_MAX_DB) again = AGAIN_MAX_DB + DGAIN_MAX_DB;

	/* p_ctx->again=again; */
	*sensor_again = again;
	isp_gain = (((int32_t) again) << LOG2_GAIN_SHIFT) / 20;

	return isp_gain;
}

unsigned int imx415_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return 0;
}

struct tx_isp_mipi_bus imx415_mipi = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 1485,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.image_twidth = 3840,  //PIX_HWIDTH[12:0] 3042,3043
	.image_theight = 2160, //PIX_VWIDTH[12:0] {3046,3047/2}
	.mipi_sc.mipi_crop_start0x = 0,
	.mipi_sc.mipi_crop_start0y = 0,
	.mipi_sc.mipi_crop_start1x = 0,
	.mipi_sc.mipi_crop_start1y = 0,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.data_type_en = 1,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute imx415_attr = {
	.name = SENSOR_NAME,
	.chip_id = 0x2823,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x1a,
	.max_again = 404346,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = imx415_alloc_again,
	.sensor_ctrl.alloc_dgain = imx415_alloc_dgain,
};

static struct regval_list imx415_init_regs_3840_2160_30fps_mipi[] = {
	{0x3008, 0x7f},
	{0x300a, 0x5b},
	{0x301c, 0x04},
	{0x3024, 0x75},//
	{0x3025, 0x09},//vts=0x975 = 2421
	{0x3028, 0xfe},//
	{0x3029, 0x03},//hts=1022
	{0x3031, 0x00},
	{0x3032, 0x00},
	{0x3033, 0x08},
	{0x3040, 0x0c},
	{0x3042, 0x00},
	{0x3044, 0x20},
	{0x3045, 0x00},
	{0x3046, 0xe0},//
	{0x3047, 0x10},//
	{0x3050, 0x9f},
	{0x3051, 0x06},
	{0x3090, 0x14},
	{0x30c1, 0x00},
	{0x3116, 0x24},
	{0x3118, 0xa0},
	{0x311e, 0x24},
	{0x32d4, 0x21},
	{0x32ec, 0xa1},
	{0x344c, 0x2b},
	{0x344d, 0x01},
	{0x344e, 0xed},
	{0x344f, 0x01},
	{0x3450, 0xf6},
	{0x3451, 0x02},
	{0x3452, 0x7f},
	{0x3453, 0x03},
	{0x358a, 0x04},
	{0x35a1, 0x02},
	{0x35ec, 0x27},
	{0x35ee, 0x8d},
	{0x35f0, 0x8d},
	{0x35f2, 0x29},
	{0x36bc, 0x0c},
	{0x36cc, 0x53},
	{0x36cd, 0x00},
	{0x36ce, 0x3c},
	{0x36d0, 0x8c},
	{0x36d1, 0x00},
	{0x36d2, 0x71},
	{0x36d4, 0x3c},
	{0x36d6, 0x53},
	{0x36d7, 0x00},
	{0x36d8, 0x71},
	{0x36da, 0x8c},
	{0x36db, 0x00},
	{0x3701, 0x00},
	{0x3720, 0x00},
	{0x3724, 0x02},
	{0x3726, 0x02},
	{0x3732, 0x02},
	{0x3734, 0x03},
	{0x3736, 0x03},
	{0x3742, 0x03},
	{0x3862, 0xe0},
	{0x38cc, 0x30},
	{0x38cd, 0x2f},
	{0x395c, 0x0c},
	{0x39a4, 0x07},
	{0x39a8, 0x32},
	{0x39aa, 0x32},
	{0x39ac, 0x32},
	{0x39ae, 0x32},
	{0x39b0, 0x32},
	{0x39b2, 0x2f},
	{0x39b4, 0x2d},
	{0x39b6, 0x28},
	{0x39b8, 0x30},
	{0x39ba, 0x30},
	{0x39bc, 0x30},
	{0x39be, 0x30},
	{0x39c0, 0x30},
	{0x39c2, 0x2e},
	{0x39c4, 0x2b},
	{0x39c6, 0x25},
	{0x3a42, 0xd1},
	{0x3a4c, 0x77},
	{0x3ae0, 0x02},
	{0x3aec, 0x0c},
	{0x3b00, 0x2e},
	{0x3b06, 0x29},
	{0x3b98, 0x25},
	{0x3b99, 0x21},
	{0x3b9b, 0x13},
	{0x3b9c, 0x13},
	{0x3b9d, 0x13},
	{0x3b9e, 0x13},
	{0x3ba1, 0x00},
	{0x3ba2, 0x06},
	{0x3ba3, 0x0b},
	{0x3ba4, 0x10},
	{0x3ba5, 0x14},
	{0x3ba6, 0x18},
	{0x3ba7, 0x1a},
	{0x3ba8, 0x1a},
	{0x3ba9, 0x1a},
	{0x3bac, 0xed},
	{0x3bad, 0x01},
	{0x3bae, 0xf6},
	{0x3baf, 0x02},
	{0x3bb0, 0xa2},
	{0x3bb1, 0x03},
	{0x3bb2, 0xe0},
	{0x3bb3, 0x03},
	{0x3bb4, 0xe0},
	{0x3bb5, 0x03},
	{0x3bb6, 0xe0},
	{0x3bb7, 0x03},
	{0x3bb8, 0xe0},
	{0x3bba, 0xe0},
	{0x3bbc, 0xda},
	{0x3bbe, 0x88},
	{0x3bc0, 0x44},
	{0x3bc2, 0x7b},
	{0x3bc4, 0xa2},
	{0x3bc8, 0xbd},
	{0x3bca, 0xbd},
	{0x4001, 0x01},
	{0x4004, 0x48},
	{0x4005, 0x09},
	{0x4018, 0xa7},
	{0x401a, 0x57},
	{0x401c, 0x5f},
	{0x401e, 0x97},
	{0x4020, 0x5f},
	{0x4022, 0xaf},
	{0x4024, 0x5f},
	{0x4026, 0x9f},
	{0x4028, 0x4f},
	{0x3000, 0x00},
	{SENSOR_REG_DELAY, 0x1e},
	{0x3002, 0x00},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct regval_list imx415_init_regs_3840_2160_20fps_mipi[] = {
	{0x3008, 0x7f},
	{0x300a, 0x5b},
	{0x301c, 0x04},
	{0x3024, 0xb2},// VMAX[19:0] 0x8b = 2226
	{0x3028, 0x84},// HMAX[15:0]
	{0x3029, 0x06},//0x684 = 1688
	{0x3031, 0x00},
	{0x3032, 0x00},
	{0x3033, 0x05},
	{0x3040, 0x0c},
	{0x3042, 0x00},//0xf00 =3840
	{0x3044, 0x20},
	{0x3045, 0x00},
	{0x3046, 0xe0},//
	{0x3047, 0x10},//0x10e0 = 4320
	{0x3050, 0x08},
	{0x30c1, 0x00},
	{0x3116, 0x24},
	{0x311e, 0x24},
	{0x32d4, 0x21},
	{0x32ec, 0xa1},
	{0x344c, 0x2b},
	{0x344d, 0x01},
	{0x344e, 0xed},
	{0x344f, 0x01},
	{0x3450, 0xf6},
	{0x3451, 0x02},
	{0x3452, 0x7f},
	{0x3453, 0x03},
	{0x358a, 0x04},
	{0x35a1, 0x02},
	{0x35ec, 0x27},
	{0x35ee, 0x8d},
	{0x35f0, 0x8d},
	{0x35f2, 0x29},
	{0x36bc, 0x0c},
	{0x36cc, 0x53},
	{0x36cd, 0x00},
	{0x36ce, 0x3c},
	{0x36d0, 0x8c},
	{0x36d1, 0x00},
	{0x36d2, 0x71},
	{0x36d4, 0x3c},
	{0x36d6, 0x53},
	{0x36d7, 0x00},
	{0x36d8, 0x71},
	{0x36da, 0x8c},
	{0x36db, 0x00},
	{0x3701, 0x00},
	{0x3720, 0x00},
	{0x3724, 0x02},
	{0x3726, 0x02},
	{0x3732, 0x02},
	{0x3734, 0x03},
	{0x3736, 0x03},
	{0x3742, 0x03},
	{0x3862, 0xe0},
	{0x38cc, 0x30},
	{0x38cd, 0x2f},
	{0x395c, 0x0c},
	{0x39a4, 0x07},
	{0x39a8, 0x32},
	{0x39aa, 0x32},
	{0x39ac, 0x32},
	{0x39ae, 0x32},
	{0x39b0, 0x32},
	{0x39b2, 0x2f},
	{0x39b4, 0x2d},
	{0x39b6, 0x28},
	{0x39b8, 0x30},
	{0x39ba, 0x30},
	{0x39bc, 0x30},
	{0x39be, 0x30},
	{0x39c0, 0x30},
	{0x39c2, 0x2e},
	{0x39c4, 0x2b},
	{0x39c6, 0x25},
	{0x3a42, 0xd1},
	{0x3a4c, 0x77},
	{0x3ae0, 0x02},
	{0x3aec, 0x0c},
	{0x3b00, 0x2e},
	{0x3b06, 0x29},
	{0x3b98, 0x25},
	{0x3b99, 0x21},
	{0x3b9b, 0x13},
	{0x3b9c, 0x13},
	{0x3b9d, 0x13},
	{0x3b9e, 0x13},
	{0x3ba1, 0x00},
	{0x3ba2, 0x06},
	{0x3ba3, 0x0b},
	{0x3ba4, 0x10},
	{0x3ba5, 0x14},
	{0x3ba6, 0x18},
	{0x3ba7, 0x1a},
	{0x3ba8, 0x1a},
	{0x3ba9, 0x1a},
	{0x3bac, 0xed},
	{0x3bad, 0x01},
	{0x3bae, 0xf6},
	{0x3baf, 0x02},
	{0x3bb0, 0xa2},
	{0x3bb1, 0x03},
	{0x3bb2, 0xe0},
	{0x3bb3, 0x03},
	{0x3bb4, 0xe0},
	{0x3bb5, 0x03},
	{0x3bb6, 0xe0},
	{0x3bb7, 0x03},
	{0x3bb8, 0xe0},
	{0x3bba, 0xe0},
	{0x3bbc, 0xda},
	{0x3bbe, 0x88},
	{0x3bc0, 0x44},
	{0x3bc2, 0x7b},
	{0x3bc4, 0xa2},
	{0x3bc8, 0xbd},
	{0x3bca, 0xbd},
	{0x4001, 0x01},
	{0x4004, 0x48},
	{0x4005, 0x09},
	{0x400c, 0x00},
	{0x4018, 0x7f},
	{0x401a, 0x37},
	{0x401c, 0x37},
	{0x401e, 0xf7},
	{0x401f, 0x00},
	{0x4020, 0x3f},
	{0x4022, 0x6f},
	{0x4024, 0x3f},
	{0x4026, 0x5f},
	{0x4028, 0x2f},
	{0x4074, 0x01},
	{0x3000, 0x00},
	{SENSOR_REG_DELAY, 0x18},
	{0x3002, 0x00},
	{SENSOR_REG_END, 0x00},/* END MARKER */
};

static struct tx_isp_sensor_win_setting imx415_win_sizes[] = {
	{
		.width = 3840,
		.height = 2160,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SGBRG10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = imx415_init_regs_3840_2160_30fps_mipi,
	},
	{
		.width = 3840,
		.height = 2160,
		.fps = 20 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SGBRG10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = imx415_init_regs_3840_2160_20fps_mipi,
	}
};
struct tx_isp_sensor_win_setting *wsize = &imx415_win_sizes[0];

static struct regval_list imx415_stream_on_mipi[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list imx415_stream_off_mipi[] = {
	{SENSOR_REG_END, 0x00},
};

int imx415_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value) {
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

int imx415_write(struct tx_isp_subdev *sd, uint16_t reg,
		 unsigned char value) {
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
static int imx415_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = imx415_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int imx415_write_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
	int ret;

	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = imx415_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int imx415_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	return 0;
}

static int imx415_detect(struct tx_isp_subdev *sd, unsigned int *ident) {
	int ret;
	unsigned char v;

	ret = imx415_read(sd, 0x3b00, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = imx415_read(sd, 0x3b06, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int imx415_set_integration_time(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	unsigned short shs = 0;
	unsigned short vmax = 0;
	vmax = imx415_attr.total_height;
	shs = vmax - value;

	ret = imx415_write(sd, 0x3050, (unsigned char) (shs & 0xff));
	ret += imx415_write(sd, 0x3051, (unsigned char) ((shs >> 8) & 0xff));
	ret += imx415_write(sd, 0x3052, (unsigned char) ((shs >> 16) & 0x0f));
	if (ret < 0)
		return ret;

	return 0;
}

static int imx415_set_analog_gain(struct tx_isp_subdev *sd, int value) {
	int ret = 0;

	ret += imx415_write(sd, 0x3090, (unsigned char) (value & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int imx415_set_digital_gain(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int imx415_get_black_pedestal(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_set_attr(struct tx_isp_subdev *sd, struct tx_isp_sensor_win_setting *wise) {
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

static int imx415_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (!init->enable)
		return ISP_SUCCESS;

	sensor_set_attr(sd, wsize);
	sensor->video.state = TX_ISP_MODULE_INIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int imx415_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
#ifndef SENSOR_WITHOUT_INIT
			ret = imx415_write_array(sd, wsize->regs);
			if (ret)
				return ret;
#endif
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if (sensor->video.state == TX_ISP_MODULE_RUNNING) {

			ret = imx415_write_array(sd, imx415_stream_on_mipi);
			ISP_WARNING("%s stream on\n", SENSOR_NAME);
		}
	} else {
		ret = imx415_write_array(sd, imx415_stream_off_mipi);
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int imx415_set_fps(struct tx_isp_subdev *sd, int fps) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int max_fps;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch (sensor->info.default_boot) {
		case 0:
			sclk = 74227860;
			max_fps = TX_SENSOR_MAX_FPS_30;
			break;
		case 1:
			sclk = 2226 * 1688 * 20;
			max_fps = TX_SENSOR_MAX_FPS_30;
			break;
		default:
			ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}

	ret += imx415_read(sd, 0x3029, &val);
	hts = val << 8;
	ret += imx415_read(sd, 0x3028, &val);
	hts = (hts | val);
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += imx415_write(sd, 0x3001, 0x01);
	ret = imx415_write(sd, 0x3026, (unsigned char) ((vts & 0xf0000) >> 16));
	ret += imx415_write(sd, 0x3025, (unsigned char) ((vts & 0xff00) >> 8));
	ret += imx415_write(sd, 0x3024, (unsigned char) (vts & 0xff));
	ret += imx415_write(sd, 0x3001, 0x00);
	if (0 != ret) {
		ISP_ERROR("err: imx415_write err\n");
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

static int imx415_set_mode(struct tx_isp_subdev *sd, int value) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

#if 0
static int imx415_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	val = imx415_read(sd, 0x3221, &val);
	switch(enable) {
	case 0:
		imx415_write(sd, 0x3221, val & 0x99);
		break;
	case 1:
		imx415_write(sd, 0x3221, val | 0x06);
		break;
	case 2:
		imx415_write(sd, 0x3221, val | 0x60);
		break;
	case 3:
		imx415_write(sd, 0x3221, val | 0x66);
		break;
	}

	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}
#endif

static int sensor_attr_check(struct tx_isp_subdev *sd) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	unsigned long rate;
	int ret;

	switch (info->default_boot) {
		case 0:
			wsize = &imx415_win_sizes[0];
			memcpy(&(imx415_attr.mipi), &imx415_mipi, sizeof(imx415_mipi));
			imx415_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			imx415_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			imx415_attr.max_integration_time_native = 2421 - 8;
			imx415_attr.integration_time_limit = 2421 - 8;
			imx415_attr.total_width = 1022;
			imx415_attr.total_height = 2421;
			imx415_attr.max_integration_time = 2421 - 8;
			imx415_attr.again = 0;
			imx415_attr.integration_time = 0x69f;
			break;
		case 1:
			wsize = &imx415_win_sizes[1];
			memcpy(&(imx415_attr.mipi), &imx415_mipi, sizeof(imx415_mipi));
			imx415_attr.mipi.clk = 891;
			imx415_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			imx415_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			imx415_attr.max_integration_time_native = 2226 - 8;
			imx415_attr.integration_time_limit = 2226 - 8;
			imx415_attr.total_width = 1668;
			imx415_attr.total_height = 2226;
			imx415_attr.max_integration_time = 2226 - 8;
			imx415_attr.again = 0x0;
			imx415_attr.integration_time = 0x8;
			break;
		default:
			ISP_ERROR("Have no this Setting Source!!!\n");
	}

	switch (info->video_interface) {
		case TISP_SENSOR_VI_MIPI_CSI0:
			imx415_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			imx415_attr.mipi.index = 0;
			break;
		case TISP_SENSOR_VI_DVP:
			imx415_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
			break;
		default:
			ISP_ERROR("Have no this Interface Source!!!\n");
	}
#ifndef SENSOR_WITHOUT_INIT
	switch(info->mclk) {
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
	switch(info->default_boot) {
	case 0:
	case 1:
		if (((rate / 1000) % 27000) != 0) {
			ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
			sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
			if (IS_ERR(sclka)) {
				pr_err("get sclka failed\n");
			} else {
				rate = private_clk_get_rate(sclka);
				if (((rate / 1000) % 27000) != 0) {
					private_clk_set_rate(sclka, 1188000000);
				}
			}
		}
		private_clk_set_rate(sensor->mclk, 37125000);
		private_clk_prepare_enable(sensor->mclk);
		break;
	}
#endif
	ISP_WARNING("\n====>[default_boot=%d] [resolution=%dx%d] [video_interface=%d] [MCLK=%d] \n", info->default_boot,
		    wsize->width, wsize->height, info->video_interface, info->mclk);
	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

	return 0;

}

static int imx415_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
#ifndef SENSOR_WITHOUT_INIT
	if (reset_gpio != -1) {
	       ret = private_gpio_request(reset_gpio,"imx415_reset");
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
	       ret = private_gpio_request(pwdn_gpio,"imx415_pwdn");
	       if (!ret) {
		       private_gpio_direction_output(pwdn_gpio, 0);
		       private_msleep(5);
		       private_gpio_direction_output(pwdn_gpio, 1);
		       private_msleep(5);
	       } else {
		       ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
	       }
       }
       ret = imx415_detect(sd, &ident);
       if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an %s chip.\n",
			  client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
       }
#else
	ident = 0x415;
#endif
	ISP_WARNING("%s chip found @ 0x%02x (%s)\n",
		    SENSOR_NAME, client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n", SENSOR_VERSION);
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int imx415_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg) {
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd) {
		case TX_ISP_EVENT_SENSOR_EXPO:
			//      if (arg)
			//              ret = imx415_set_expo(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			if (arg)
				ret = imx415_set_integration_time(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			if (arg)
				ret = imx415_set_analog_gain(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_DGAIN:
			if (arg)
				ret = imx415_set_digital_gain(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
			if (arg)
				ret = imx415_get_black_pedestal(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if (arg)
				ret = imx415_set_mode(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
			ret = imx415_write_array(sd, imx415_stream_off_mipi);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			ret = imx415_write_array(sd, imx415_stream_on_mipi);
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if (arg)
				ret = imx415_set_fps(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_VFLIP:
			//      if (arg)
			//              ret = imx415_set_vflip(sd, sensor_val->value);
			break;
		default:
			break;
	}

	return ret;
}

static int imx415_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg) {
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = imx415_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int imx415_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg) {
	int len = 0;

	len = strlen(sd->chip.name);
	if (len && strncmp(sd->chip.name, reg->name, len)) {
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	imx415_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops imx415_core_ops = {
	.g_chip_ident = imx415_g_chip_ident,
	.reset = imx415_reset,
	.init = imx415_init,
	.g_register = imx415_g_register,
	.s_register = imx415_s_register,
};

static struct tx_isp_subdev_video_ops imx415_video_ops = {
	.s_stream = imx415_s_stream,
};

static struct tx_isp_subdev_sensor_ops imx415_sensor_ops = {
	.ioctl = imx415_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops imx415_ops = {
	.core = &imx415_core_ops,
	.video = &imx415_video_ops,
	.sensor = &imx415_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64) 0;
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

static int imx415_probe(struct i2c_client *client,
			const struct i2c_device_id *id) {
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *) kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->dev = &client->dev;
	imx415_attr.expo_fs = 1;
	sensor->video.shvflip = 1;
	sensor->video.attr = &imx415_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &imx415_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

	return 0;
}

static int imx415_remove(struct i2c_client *client) {
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if (reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if (pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

#ifndef SENSOR_WITHOUT_INIT
	private_clk_disable_unprepare(sensor->mclk);
	private_devm_clk_put(&client->dev, sensor->mclk);
#endif
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id imx415_id[] = {
	{SENSOR_NAME, 0},
	{}
};


static struct i2c_driver imx415_driver = {
	.driver = {
		.owner = NULL,
		.name = SENSOR_NAME,
	},
	.probe = imx415_probe,
	.remove = imx415_remove,
	.id_table = imx415_id,
};


char *get_sensor_name(void) {
	return SENSOR_NAME;
}

int get_sensor_i2c_addr(void) {
	return 0x1a;// Caller
}

int get_sensor_width(void) {
	return imx415_win_sizes->width;
}

int get_sensor_height(void) {
	return imx415_win_sizes->height;
}

int get_sensor_wdr_mode(void) {
	return 0;
}

int init_sensor(void) {
	return private_i2c_add_driver(&imx415_driver);
}

int exit_sensor(void) {
	private_i2c_del_driver(&imx415_driver);
}
