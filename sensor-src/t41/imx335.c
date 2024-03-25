// SPDX-License-Identifier: GPL-2.0+
/*
 * imx335.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps       interface             mode
 *   0          2592*1944       25        mipi_2lane           linear
 *   1          2592*1944       15        mipi_2lane           dol
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

#define SENSOR_NAME "imx335"
#define SENSOR_CHIP_ID 0x380a
#define SENSOR_CHIP_ID_H (0x38)
#define SENSOR_CHIP_ID_L (0x0a)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20231103a"
#define AGAIN_MAX_DB 0x64
#define DGAIN_MAX_DB 0x64
#define LOG2_GAIN_SHIFT 16

static int reset_gpio = -1;
static int pwdn_gpio = -1;
static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
static int wdr_bufsize = 7080000;
static int shvflip = 1;

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	uint16_t again = (isp_gain * 20) >> LOG2_GAIN_SHIFT;
	// Limit Max gain
	if (again > AGAIN_MAX_DB + DGAIN_MAX_DB) again = AGAIN_MAX_DB + DGAIN_MAX_DB;

	/* p_ctx->again=again; */
	*sensor_again = again;
	isp_gain = (((int32_t) again) << LOG2_GAIN_SHIFT) / 20;

	return isp_gain;
}

unsigned int sensor_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	uint16_t again = (isp_gain * 20) >> LOG2_GAIN_SHIFT;
	// Limit Max gain
	if (again > AGAIN_MAX_DB + DGAIN_MAX_DB) again = AGAIN_MAX_DB + DGAIN_MAX_DB;

	/* p_ctx->again = again; */
	*sensor_again = again;
	isp_gain = (((int32_t) again) << LOG2_GAIN_SHIFT) / 20;

	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi_linear = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 1188,
	.lans = 2,
	.settle_time_apative_en = 0,
	.image_twidth = 2616,
	.image_theight = 1964,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.mipi_crop_start0x = 12,
	.mipi_sc.mipi_crop_start0y = 33,
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
};

struct tx_isp_mipi_bus sensor_mipi_dol = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 1188,
	.lans = 2,
	.settle_time_apative_en = 0,
	.image_twidth = 2616,
	.image_theight = 1944,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
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
	.mipi_sc.data_type_en = 1,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x1a,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.max_again = 404346,
	.max_again_short = 404346,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.min_integration_time_short = 4,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_again_short = sensor_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	.wdr_cache = 0,
};

static struct regval_list sensor_init_regs_2592_1944_15fps_sensor_mipi_dol[] = {
	{0x300C, 0x5B},  // BCWAIT_TIME[7:0]
	{0x300D, 0x40},  // CPWAIT_TIME[7:0]
	{0x3030, 0x88},  // VMAX[19:0] 0x1388 = 5000
	{0x3031, 0x13},  //
	{0x3034, 0xEF},  // HMAX[15:0] 0x1EF = 495
	{0x3035, 0x01},  //
	{0x3048, 0x01},  // WDMODE[0]
	{0x3049, 0x01},  // WDSEL[1:0]
	{0x304A, 0x04},  // WD_SET1[2:0]
	{0x304B, 0x03},  // WD_SET2[3:0]
	{0x304C, 0x13},  // OPB_SIZE_V[5:0]
	{0x3050, 0x00},  // ADBIT[0]
	{0x3058, 0x34},  // SHR0[19:0]
	{0x3059, 0x21},  //
	{0x3068, 0x72},  // RHS1[19:0] 0xAA
	{0x3069, 0x02},
	{0x315A, 0x02},  // INCKSEL2[1:0]
	{0x316A, 0x7E},  // INCKSEL4[1:0]
	{0x319D, 0x00},  // MDBIT
	{0x31A1, 0x00},  // XVS_DRV[1:0]
	{0x31D7, 0x01},  // XVSMSKCNT_INT[1:0]
	{0x3200, 0x00},  // FGAINEN 0 enable    1 disable
	{0x3288, 0x21},  // -
	{0x328A, 0x02},  // -
	{0x3414, 0x05},  // -
	{0x3416, 0x18},  // -
	{0x341C, 0xFF},  // ADBIT1[8:0]
	{0x341D, 0x01},  //
	{0x3648, 0x01},  // -
	{0x364A, 0x04},  // -
	{0x364C, 0x04},  // -
	{0x3678, 0x01},  // -
	{0x367C, 0x31},  // -
	{0x367E, 0x31},  // -
	{0x3706, 0x10},  // -
	{0x3708, 0x03},  // -
	{0x3714, 0x02},  // -
	{0x3715, 0x02},  // -
	{0x3716, 0x01},  // -
	{0x3717, 0x03},  // -
	{0x371C, 0x3D},  // -
	{0x371D, 0x3F},  // -
	{0x372C, 0x00},  // -
	{0x372D, 0x00},  // -
	{0x372E, 0x46},  // -
	{0x372F, 0x00},  // -
	{0x3730, 0x89},  // -
	{0x3731, 0x00},  // -
	{0x3732, 0x08},  // -
	{0x3733, 0x01},  // -
	{0x3734, 0xFE},  // -
	{0x3735, 0x05},  // -
	{0x3740, 0x02},  // -
	{0x375D, 0x00},  // -
	{0x375E, 0x00},  // -
	{0x375F, 0x11},  // -
	{0x3760, 0x01},  // -
	{0x3768, 0x1A},  // -
	{0x3769, 0x1A},  // -
	{0x376A, 0x1A},  // -
	{0x376B, 0x1A},  // -
	{0x376C, 0x1A},  // -
	{0x376D, 0x17},  // -
	{0x376E, 0x0F},  // -
	{0x3776, 0x00},  // -
	{0x3777, 0x00},  // -
	{0x3778, 0x46},  // -
	{0x3779, 0x00},  // -
	{0x377A, 0x89},  // -
	{0x377B, 0x00},  // -
	{0x377C, 0x08},  // -
	{0x377D, 0x01},  // -
	{0x377E, 0x23},  // -
	{0x377F, 0x02},  // -
	{0x3780, 0xD9},  // -
	{0x3781, 0x03},  // -
	{0x3782, 0xF5},  // -
	{0x3783, 0x06},  // -
	{0x3784, 0xA5},  // -
	{0x3788, 0x0F},  // -
	{0x378A, 0xD9},  // -
	{0x378B, 0x03},  // -
	{0x378C, 0xEB},  // -
	{0x378D, 0x05},  // -
	{0x378E, 0x87},  // -
	{0x378F, 0x06},  // -
	{0x3790, 0xF5},  // -
	{0x3792, 0x43},  // -
	{0x3794, 0x7A},  // -
	{0x3796, 0xA1},  // -
	{0x3A01, 0x01},  // LANEMODE[2:0]
	{0x3000, 0x00},
	{0xfffe, 0x1E},
	{0x3002, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_2592_1944_25fps_mipi[] = {
	{0x3000, 0x01},
	{0x3001, 0x00},
	{0x3002, 0x01},
	{0x3004, 0x00},
	{0x300c, 0x5b},
	{0x300d, 0x40},
	{0x3018, 0x00},
	{0x302c, 0x30},
	{0x302d, 0x00},
	{0x302e, 0x38},
	{0x302f, 0x0a},
	{0x3030, 0x18},//
	{0x3031, 0x15},//VMAX 5400
	{0x3032, 0x00},
	{0x3034, 0x26},//
	{0x3035, 0x02},//HMAX 550
	{0x3050, 0x00},
	{0x315a, 0x02},
	{0x316a, 0x7e},
	{0x319d, 0x00},
	{0x31a1, 0x00},
	{0x3288, 0x21},
	{0x328a, 0x02},
	{0x3414, 0x05},
	{0x3416, 0x18},
	{0x341c, 0xff},
	{0x341d, 0x01},
	{0x3648, 0x01},
	{0x364a, 0x04},
	{0x364c, 0x04},
	{0x3678, 0x01},
	{0x367c, 0x31},
	{0x367e, 0x31},
	{0x3706, 0x10},
	{0x3708, 0x03},
	{0x3714, 0x02},
	{0x3715, 0x02},
	{0x3716, 0x01},
	{0x3717, 0x03},
	{0x371c, 0x3d},
	{0x371d, 0x3f},
	{0x372c, 0x00},
	{0x372d, 0x00},
	{0x372e, 0x46},
	{0x372f, 0x00},
	{0x3730, 0x89},
	{0x3731, 0x00},
	{0x3732, 0x08},
	{0x3733, 0x01},
	{0x3734, 0xfe},
	{0x3735, 0x05},
	{0x3740, 0x02},
	{0x375d, 0x00},
	{0x375e, 0x00},
	{0x375f, 0x11},
	{0x3760, 0x01},
	{0x3768, 0x1b},
	{0x3769, 0x1b},
	{0x376a, 0x1b},
	{0x376b, 0x1b},
	{0x376c, 0x1a},
	{0x376d, 0x17},
	{0x376e, 0x0f},
	{0x3776, 0x00},
	{0x3777, 0x00},
	{0x3778, 0x46},
	{0x3779, 0x00},
	{0x377a, 0x89},
	{0x377b, 0x00},
	{0x377c, 0x08},
	{0x377d, 0x01},
	{0x377e, 0x23},
	{0x377f, 0x02},
	{0x3780, 0xd9},
	{0x3781, 0x03},
	{0x3782, 0xf5},
	{0x3783, 0x06},
	{0x3784, 0xa5},
	{0x3788, 0x0f},
	{0x378a, 0xd9},
	{0x378b, 0x03},
	{0x378c, 0xeb},
	{0x378d, 0x05},
	{0x378e, 0x87},
	{0x378f, 0x06},
	{0x3790, 0xf5},
	{0x3792, 0x43},
	{0x3794, 0x7a},
	{0x3796, 0xa1},
	{0x3a01, 0x01},
	{0x3002, 0x00},
	{0x3000, 0x00},
	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution]. */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 2592,
		.height = 1944,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2592_1944_25fps_mipi,
	},
	/* 1948*1109 [1]*/
	{
		.width = 2592,
		.height = 1944,
		.fps = 15 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2592_1944_15fps_sensor_mipi_dol,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

static struct regval_list sensor_stream_on_mipi[] = {
	{0x3000, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{0x3000, 0x01},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value) {
	int ret;
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

	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value) {
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
static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
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

static int sensor_write_array(struct tx_isp_subdev *sd, struct regval_list *vals) {
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

static int sensor_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident) {
	unsigned char v;
	int ret;

	ret = sensor_read(sd, 0x302E, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;

	*ident = v;

	ret = sensor_read(sd, 0x302F, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;

	*ident = (*ident << 8) | v;
	return 0;
}

static int sensor_set_integration_time_short(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	unsigned short shs1 = 0;
	int rhs1 = 626;

	shs1 = rhs1 - (value << 2);
	ret = sensor_write(sd, 0x305C, (unsigned char)(shs1 & 0xff));
	ret += sensor_write(sd, 0x305D, (unsigned char)((shs1 >> 8) & 0xff));
	ret += sensor_write(sd, 0x305E, (unsigned char)((shs1 >> 16) & 0x3));
	return 0;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	unsigned short shs = 0;
	unsigned short vmax = 0;

	if (data_type == TX_SENSOR_DATA_TYPE_LINEAR) {
		vmax = sensor_attr.total_height;
		shs = vmax - value;
		ret = sensor_write(sd, 0x3058, (unsigned char)(shs & 0xff));
		ret += sensor_write(sd, 0x3059, (unsigned char)((shs >> 8) & 0xff));
		ret += sensor_write(sd, 0x305A, (unsigned char)((shs >> 16) & 0x0f));
	}
	if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) {
		vmax = sensor_attr.total_height;
		shs = (vmax << 1) - (value << 2);
		ret = sensor_write(sd, 0x3058, (unsigned char)(shs & 0xff));
		ret += sensor_write(sd, 0x3059, (unsigned char)((shs >> 8) & 0xff));
		ret += sensor_write(sd, 0x305A, (unsigned char)((shs >> 16) & 0x0f));
	}
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain_short(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	ret += sensor_write(sd, 0x30EA, (unsigned char)(value & 0xff));
		ret += sensor_write(sd, 0x30EB, (unsigned char)((value >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
		ret += sensor_write(sd, 0x30E8, (unsigned char)(value & 0xff));
		ret += sensor_write(sd, 0x30E9, (unsigned char)((value >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
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

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT) {
		        ret = sensor_write_array(sd, wsize->regs);
		        if (ret)
		                return ret;

		        sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
		        ret = sensor_write_array(sd, sensor_stream_on_mipi);
		        sensor->video.state = TX_ISP_MODULE_RUNNING;
		        pr_debug("%s stream on\n", SENSOR_NAME);
		}
	} else {
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		sensor->video.state = TX_ISP_MODULE_INIT;
		pr_debug("%s stream off\n", SENSOR_NAME);
	}
	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int max_fps;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch(sensor->info.default_boot) {
		case 0:
			sclk = 550 * 5400 * 25;
			max_fps = TX_SENSOR_MAX_FPS_25;
			break;
		case 1:
			sclk = 495 * 5000 * 15;
			max_fps = TX_SENSOR_MAX_FPS_15;
			break;
		default:
			ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps<< 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}

	ret += sensor_read(sd, 0x3035, &val);
	hts = val;
	ret += sensor_read(sd, 0x3034, &val);
	hts = (hts << 8) + val;
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret = sensor_write(sd, 0x3032, (unsigned char)((vts & 0xf0000) >> 16));
	ret += sensor_write(sd, 0x3031, (unsigned char)((vts & 0xff00) >> 8));
	ret += sensor_write(sd, 0x3030, (unsigned char)(vts & 0xff));
	if (0 != ret) {
		ISP_ERROR("err: sensor_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 10;
	sensor->video.attr->integration_time_limit = vts - 10;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 10;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable) {
	int ret = 0;
	unsigned char hreverse = 0;
	unsigned char vreverse = 0;

	unsigned char reg_3081 = 0x02;
	unsigned char reg_3083 = 0x02;

	unsigned char reg_3074 = 0xC4;
	unsigned char reg_3075 = 0x00;

	ret = sensor_read(sd, 0x304E, &hreverse);
	ret += sensor_read(sd, 0x304F, &vreverse);
	switch (enable) {
		case 0:
			hreverse &= 0xFC;
			vreverse &= 0xFC;
			reg_3081 = 0x02;
			reg_3083 = 0x02;
			reg_3074 = 0xC4;
			reg_3075 = 0x00;
			break;
		case 1:
			hreverse |= 0x01;
			vreverse &= 0xFC;
			reg_3081 = 0x02;
			reg_3083 = 0x02;
			reg_3074 = 0xC4;
			reg_3075 = 0x00;
			break;
		case 2:
			hreverse &= 0xFC;
			vreverse |= 0x01;
			reg_3081 = 0xFE;
			reg_3083 = 0xFE;
			reg_3074 = 0x00;
			reg_3075 = 0x10;
			break;
		case 3:
			hreverse |= 0x01;
			vreverse |= 0x01;
			reg_3081 = 0xFE;
			reg_3083 = 0xFE;
			reg_3074 = 0x00;
			reg_3075 = 0x10;
			break;
	}
	ret += sensor_write(sd, 0x304E, hreverse);
	ret += sensor_write(sd, 0x304F, vreverse);
	ret += sensor_write(sd, 0x3081, reg_3081);
	ret += sensor_write(sd, 0x3083, reg_3083);
	ret += sensor_write(sd, 0x3074, reg_3074);
	ret += sensor_write(sd, 0x3075, reg_3075);
	return ret;
}

static int sensor_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en) {
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = 0;
	/* struct timeval tv; */

	/* do_gettimeofday(&tv); */
	/* pr_debug("%d:before:time is %d.%d\n", __LINE__,tv.tv_sec,tv.tv_usec); */
	ret = sensor_write(sd, 0x3000, 0x1);
	if (wdr_en == 1) {
		memcpy((void *) (&(sensor_attr.mipi)), (void *) (&sensor_mipi_dol), sizeof(sensor_mipi_dol));
		data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		wsize = &sensor_win_sizes[1];
		sensor_attr.wdr_cache = wdr_bufsize;
		sensor_attr.max_again = 404346;
		sensor_attr.max_again_short = 404346;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 1;
		sensor_attr.min_integration_time_native = 1;
		sensor_attr.max_integration_time_native = 2339;
		sensor_attr.min_integration_time_short = 1;
		sensor_attr.max_integration_time_short = 152;
		sensor_attr.integration_time_limit = 2339;
		sensor_attr.total_width = 495;
		sensor_attr.total_height = 5000;
		sensor_attr.max_integration_time = 2339;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor->video.attr = &sensor_attr;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else if (wdr_en == 0) {
		memcpy((void *) (&(sensor_attr.mipi)), (void *) (&sensor_mipi_linear), sizeof(sensor_mipi_linear));
		data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.data_type = data_type;
		wsize = &sensor_win_sizes[0];

		sensor_attr.data_type = data_type;
		sensor_attr.wdr_cache = wdr_bufsize;
		sensor_attr.max_again = 404346;
		sensor_attr.max_again_short = 404346;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 4;
		sensor_attr.min_integration_time_native = 4;
		sensor_attr.max_integration_time_native = 5400 - 10;
		sensor_attr.integration_time_limit = 5400 - 10;
		sensor_attr.total_width = 550;
		sensor_attr.total_height = 5400;
		sensor_attr.max_integration_time = 5400 - 10;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;

		sensor->video.attr = &sensor_attr;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else {
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	return 0;
}

static int sensor_set_wdr(struct tx_isp_subdev *sd, int wdr_en) {
	int ret = 0;

	private_gpio_direction_output(reset_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(reset_gpio, 1);
	private_msleep(1);

	ret = sensor_write_array(sd, wsize->regs);
	ret = sensor_write_array(sd, sensor_stream_on_mipi);

	return 0;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value) {
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

struct clk *sclka;

static int sensor_attr_check(struct tx_isp_subdev *sd) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = 0;

	switch (info->default_boot) {
		case 0:
			wsize = &sensor_win_sizes[0];
			data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			sensor_attr.max_again = 404346;
			sensor_attr.max_again_short = 404346;
			sensor_attr.max_dgain = 0;
			sensor_attr.min_integration_time = 4;
			sensor_attr.min_integration_time_native = 4;
			sensor_attr.max_integration_time_native = 5400 - 10;
			sensor_attr.integration_time_limit = 5400 - 10;
			sensor_attr.total_width = 550;
			sensor_attr.total_height = 5400;
			sensor_attr.max_integration_time = 5400 - 10;
			sensor_attr.integration_time_apply_delay = 2;
			sensor_attr.again_apply_delay = 2;
			sensor_attr.dgain_apply_delay = 0;
			memcpy((void *) (&(sensor_attr.mipi)), (void *)(&sensor_mipi_linear), sizeof(sensor_mipi_linear));
			break;
		case 1:
			wsize = &sensor_win_sizes[1];
			data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.wdr_cache = wdr_bufsize;
			sensor_attr.max_again = 404346;
			sensor_attr.max_again_short = 404346;
			sensor_attr.max_dgain = 0;
			sensor_attr.min_integration_time = 1;
			sensor_attr.min_integration_time_native = 1;
			sensor_attr.max_integration_time_native = 2339;
			sensor_attr.min_integration_time_short = 1;
			sensor_attr.max_integration_time_short = 152;
			sensor_attr.integration_time_limit = 2339;
			sensor_attr.total_width = 495;
			sensor_attr.total_height = 5000;
			sensor_attr.max_integration_time = 2339;
			sensor_attr.integration_time_apply_delay = 2;
			sensor_attr.again_apply_delay = 2;
			sensor_attr.dgain_apply_delay = 0;
			memcpy((void *) (&(sensor_attr.mipi)), (void *) (&sensor_mipi_dol), sizeof(sensor_mipi_dol));
			break;
		default:
			ISP_ERROR("Have no this Setting Source!!!\n");
	}

	switch (info->video_interface) {
		case TISP_SENSOR_VI_MIPI_CSI0:
		case TISP_SENSOR_VI_MIPI_CSI1:
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
			sensor_attr.mipi.index = 0;
			break;
		default:
			ISP_ERROR("Have no this interface!!!\n");
	}

	switch (info->mclk) {
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

	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	rate = private_clk_get_rate(sensor->mclk);
	if (((rate / 1000) % 37125) != 0) {
	       ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
	       sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
	       if (IS_ERR(sclka)) {
		       pr_err("get sclka failed\n");
	       } else {
		       rate = private_clk_get_rate(sclka);
		       if (((rate / 1000) % 37125) != 0) {
		               private_clk_set_rate(sclka, 891000000);
		       }
	       }
	}
	private_clk_set_rate(sensor->mclk, 37125000);
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
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;

err_get_mclk:
	return -1;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip) {
        struct i2c_client *client = tx_isp_get_subdevdata(sd);
        unsigned int ident = 0;
        int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio, "sensor_reset");
		if (!ret) {
		        private_gpio_direction_output(reset_gpio, 0);
		        private_msleep(100);
		        private_gpio_direction_output(reset_gpio, 1);
		        private_msleep(100);
		} else {
		        ISP_ERROR("gpio requrest fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
		        private_gpio_direction_output(pwdn_gpio, 1);
		        private_msleep(10);
		        private_gpio_direction_output(pwdn_gpio, 0);
		        private_msleep(10);
		} else {
		        ISP_ERROR("gpio requrest fail %d\n", pwdn_gpio);
		}
	}
	/* while (1) */
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

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg) {
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	struct tx_isp_initarg *init = arg;
	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	//return 0;
	switch (cmd) {
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			if (arg)
			       ret = sensor_set_integration_time(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
			if (arg)
				ret = sensor_set_integration_time_short(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			if (arg)
				ret = sensor_set_analog_gain(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
			if (arg)
				ret = sensor_set_analog_gain_short(sd, sensor_val->value);
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
				ret = sensor_write_array(sd, sensor_stream_off_mipi);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			if (arg)
				ret = sensor_write_array(sd, sensor_stream_on_mipi);
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if (arg)
				ret = sensor_set_fps(sd, sensor_val->value);
			break;
			case TX_ISP_EVENT_SENSOR_VFLIP:
			if (arg)
				ret = sensor_set_vflip(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_WDR:
			if (arg)
				ret = sensor_set_wdr(sd, init->enable);
			break;
		case TX_ISP_EVENT_SENSOR_WDR_STOP:
			if (arg)
				ret = sensor_set_wdr_stop(sd, init->enable);
			break;
		default:
			break;
	}

	return 0;
}

static int sensor_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg) {
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

static int sensor_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg) {
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


static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id) {
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

	sensor->video.attr = &sensor_attr;
	sensor_attr.expo_fs = 1;
	sensor->video.shvflip = shvflip;
	sensor->dev = &client->dev;
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

static int sensor_remove(struct i2c_client *client) {
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
        {SENSOR_NAME, 0},
        {}
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

MODULE_DESCRIPTION("A low-level driver for Sony imx335 sensor");
MODULE_LICENSE("GPL");
