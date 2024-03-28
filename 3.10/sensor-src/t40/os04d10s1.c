// SPDX-License-Identifier: GPL-2.0+
/*
 * os04d10s1.c
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

#define SENSOR_NAME "os04d10s1"
#define SENSOR_CHIP_ID_H (0x53)
#define SENSOR_CHIP_ID_M (0x04)
#define SENSOR_CHIP_ID_L (0x44)
#define SENSOR_REG_END 0xff
#define SENSOR_REG_PAGE 0xfd
#define SENSOR_REG_DELAY 0xfe
#define SENSOR_SUPPORT_SCLK_FPS_30 (35946240)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20230614a"

static int reset_gpio = GPIO_PC(28);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

struct regval_list {
	unsigned char reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x10, 0},
	{0x11, 5687},
	{0x12, 11136},
	{0x13, 16208},
	{0x14, 21097},
	{0x15, 25674},
	{0x16, 30109},
	{0x17, 34279},
	{0x18, 38336},
	{0x19, 42165},
	{0x1a, 45904},
	{0x1b, 49444},
	{0x1c, 52910},
	{0x1d, 56202},
	{0x1e, 59433},
	{0x1f, 62509},
	{0x21, 65536},
	{0x23, 71267},
	{0x25, 76672},
	{0x27, 81784},
	{0x29, 86633},
	{0x2b, 91246},
	{0x2d, 95645},
	{0x2f, 99848},
	{0x31, 103872},
	{0x33, 107731},
	{0x35, 111440},
	{0x37, 115008},
	{0x39, 118446},
	{0x3b, 121764},
	{0x3d, 124969},
	{0x3f, 128070},
	{0x43, 131072},
	{0x47, 136803},
	{0x4b, 142208},
	{0x4f, 147320},
	{0x53, 152169},
	{0x57, 156782},
	{0x5b, 161181},
	{0x5f, 165384},
	{0x63, 169408},
	{0x67, 173267},
	{0x6b, 176976},
	{0x6f, 180544},
	{0x73, 183982},
	{0x77, 187300},
	{0x7b, 190505},
	{0x7f, 193606},
	{0x87, 196608},
	{0x8f, 202339},
	{0x97, 207744},
	{0x9f, 212856},
	{0xa7, 217705},
	{0xaf, 222318},
	{0xb7, 226717},
	{0xbf, 230920},
	{0xc7, 234944},
	{0xcf, 238803},
	{0xd7, 242512},
	{0xdf, 246080},
	{0xe7, 249518},
	{0xef, 252836},
	{0xf7, 256041},
	{0xff, 259142},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	struct again_lut *lut = sensor_again_lut;

	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut->value;
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

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain) {
	return 0;
}

struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = 0x530444,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x3d,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 720,
		.lans = 2,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.image_twidth = 2560,
		.image_theight = 1440,
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
	.max_again = 259142,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 1463,
	.integration_time_limit = 1463,
	.total_width = 814,
	.total_height = 1472,
	.max_integration_time = 1463,
	.one_line_expr_in_us = 22,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_2560_1440_30fps[] = {
	{0xfd, 0x00},//P0
	{0x20, 0x00},
	{0x20, 0x01},
	{0x20, 0x01},
	{0x20, 0x01},
	{0x20, 0x01},
	{0x31, 0x20},
	{0x38, 0x15},
	{0xfd, 0x01},//P1
	{0x03, 0x00},
	{0x04, 0x04},
	{0x06, 0x01},
	{0x24, 0xff},
	{0x42, 0x59},
	{0x45, 0x02},
	{0x48, 0x0c},
	{0x4b, 0x88},
	{0xd4, 0x05},
	{0xd5, 0xd2},
	{0xd7, 0x05},
	{0xd8, 0xd2},
	{0x50, 0x01},
	{0x51, 0x11},
	{0x52, 0x18},
	{0x53, 0x01},
	{0x54, 0x01},
	{0x55, 0x01},
	{0x57, 0x08},
	{0x5c, 0x40},
	{0x7c, 0x1b},
	{0x90, 0x60},
	{0x91, 0x0f},
	{0x92, 0x30},
	{0x93, 0x3a},
	{0x94, 0x0f},
	{0x95, 0x84},
	{0x98, 0x5d},
	{0xa8, 0x50},
	{0xaa, 0x14},
	{0xab, 0x05},
	{0xac, 0x14},
	{0xad, 0x05},
	{0xae, 0x47},
	{0xaf, 0x10},
	{0xc9, 0x28},
	{0xca, 0x5e},
	{0xcb, 0x5e},
	{0xcc, 0x5e},
	{0xcd, 0x5e},
	{0xce, 0x5c},
	{0xcf, 0x5c},
	{0xd0, 0x5c},
	{0xd1, 0x5c},
	{0xd2, 0x7c},
	{0xd3, 0x7c},
	{0xdb, 0x2f},
	{0xfd, 0x01},//P1
	{0x46, 0x77},
	{0xdd, 0x00},
	{0xde, 0x3f},
	{0xfd, 0x03},//P3
	{0x2b, 0x0a},
	{0x01, 0x22},
	{0x02, 0x03},
	{0x00, 0x06},
	{0x2a, 0x22},
	{0x29, 0x0b},
	{0x1e, 0x10},
	{0x1f, 0x00},
	{0x1a, 0x24},
	{0x1b, 0x62},
	{0x1c, 0xce},
	{0x1d, 0xd3},
	{0x04, 0x0f},
	{0x36, 0x00},
	{0x37, 0x05},
	{0x38, 0x09},
	{0x39, 0x19},
	{0x3a, 0x38},
	{0x3b, 0x22},
	{0x3c, 0x22},
	{0x3d, 0x22},
	{0x3e, 0x03},
	{0xfd, 0x02},//P2
	{0xce, 0x65},
	{0xfd, 0x03},//P3
	{0x03, 0x30},
	{0x05, 0x00},
	{0x12, 0x20},
	{0x13, 0x40},
	{0x21, 0xca},
	{0x27, 0x85},
	{0x2c, 0x55},
	{0x2d, 0x08},
	{0x2e, 0xca},
	{0x3f, 0xe7},
	{0xfd, 0x00},//P0
	{0x8b, 0x01},
	{0x8d, 0x00},
	{0xfd, 0x01},//P1
	{0x01, 0x02},
	{0xfd, 0x05},//P5
	{0xc4, 0x62},
	{0xc5, 0x62},
	{0xc6, 0x62},
	{0xc7, 0x62},
	{0xf0, 0x40},
	{0xf1, 0x40},
	{0xf2, 0x40},
	{0xf3, 0x40},
	{0xf4, 0x00},
	{0xf9, 0x03},
	{0xfa, 0x5d},
	{0xfb, 0x6b},
	{0xfd, 0x02},//P2
	{0x5e, 0x32},
	{0xfd, 0x02},//P2
	{0xa0, 0x00},
	{0xa1, 0x04},
	{0xa2, 0x05},
	{0xa3, 0xa0},
	{0xa4, 0x00},
	{0xa5, 0x04},
	{0xa6, 0x0a},
	{0xa7, 0x00},
	{0x8e, 0x0a},
	{0x8f, 0x00},
	{0x90, 0x05},
	{0x91, 0xb0},
	{0xfd, 0x05},//P5
	{0xb1, 0x01},
	{0xfd, 0x00},//P0
	{0x20, 0x03},
//	{SENSOR_REG_DELAY, 0x10},
	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 5M @25fps*/
	{
		.width = 2560,
		.height = 1440,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2560_1440_30fps,
	},
};

struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sensor_stream_on[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, unsigned char reg,
		unsigned char *value) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct i2c_msg msg[2] = {
		[0] = {
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
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

int sensor_write(struct tx_isp_subdev *sd, unsigned char reg,
		 unsigned char value) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
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
			if (vals->reg_num == SENSOR_REG_PAGE) {
				val &= 0xf8;
				val |= (vals->value & 0x07);
				ret = sensor_write(sd, vals->reg_num, val);
				ret = sensor_read(sd, vals->reg_num, &val);
			}
		}
		pr_debug("vals->reg_num:0x%02x, vals->value:0x%02x\n",vals->reg_num, val);
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

	ret = sensor_read(sd, 0x02, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x03, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_M)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	ret = sensor_read(sd, 0x04, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 16) | v;
	return 0;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value) {
	int ret = 0;

	ret = sensor_write(sd, 0xfd, 0x01);
	ret += sensor_write(sd, 0x04, (unsigned char) (value & 0xff));
	ret += sensor_write(sd, 0x03, (unsigned char) ((value >> 8) & 0xff));
	ret += sensor_write(sd, 0x01, 0x01);
	if (ret < 0)
		return ret;

	return 0;
}


static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value) {
	int ret = 0;

	ret = sensor_write(sd, 0xfd, 0x01);
	ret += sensor_write(sd, 0x24, value);
	ret += sensor_write(sd, 0x01, 0x01);
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

	if (!init->enable)
		return ISP_SUCCESS;
	wsize = &sensor_win_sizes[0];
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

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
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
			pr_debug("%s stream on\n", SENSOR_NAME);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
	} else {
		ret = sensor_write_array(sd, sensor_stream_off);
		pr_debug("%s stream off\n", SENSOR_NAME);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}
	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int vb = 0;
	unsigned int vb_init = 0;
	unsigned int vts_init = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_fps = 0;

	sclk = SENSOR_SUPPORT_SCLK_FPS_30;
	max_fps = SENSOR_OUTPUT_MAX_FPS;
	vb_init = 1;
	vts_init = 1472;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_WARNING("warn: fps(%d) not in range\n", fps);
		return -1;
	}
	/* get hts */
	ret = sensor_write(sd, 0xfd, 0x01);
	ret += sensor_read(sd, 0x37, &val);
	hts = val << 8;
	ret += sensor_read(sd, 0x38, &val);
	hts |= val;

	/* get vb old */
	ret += sensor_read(sd, 0x05, &val);
	vb = val << 8;
	ret += sensor_read(sd, 0x06, &val);
	vb |= val;

	/* get vts old */
	ret += sensor_read(sd, 0x35, &val);
	vts = val << 8;
	ret += sensor_read(sd, 0x36, &val);
	vts |= val;

	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	vb = vts - vts_init + vb_init;

	ret += sensor_write(sd, 0xfd, 0x01);
	ret += sensor_write(sd, 0x05, (vb >> 8) & 0xff);
	ret += sensor_write(sd, 0x06, vb & 0xff);
	ret += sensor_write(sd, 0x01, 0x01);
	if (0 != ret) {
		ISP_ERROR("err: %s sensor_write err\n", __func__);
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 9;
	sensor->video.attr->integration_time_limit = vts - 9;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 9;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable) {
	int ret = 0;
	unsigned char val = 0;

	ret = sensor_write(sd, 0xfd, 0x01);
	ret += sensor_read(sd, 0x32, &val);
	switch (enable) {
		case 0:
			val &= 0xFC;
			break;
		case 1:
			val = ((val & 0xFD) | 0x01);
			break;
		case 2:
			val = ((val & 0xFE) | 0x02);
			break;
		case 3:
			val |= 0x03;
			break;
	}
	ret += sensor_write(sd, 0x32, val);
	ret += sensor_write(sd, 0x01, 0x01);

	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
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

static int sensor_attr_check(struct tx_isp_subdev *sd) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	unsigned long rate;

	wsize = &sensor_win_sizes[0];
	sensor_attr.max_integration_time_native = 1463;
	sensor_attr.integration_time_limit = 1463;
	sensor_attr.total_width = 814;
	sensor_attr.total_height = 1472;
	sensor_attr.max_integration_time = 1463;
	sensor_attr.one_line_expr_in_us = 19;
	sensor_attr.integration_time = 4;

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
			ISP_ERROR("Have no this interface!!!\n");
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
			       struct tx_isp_chip_ident *chip) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio, "sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(20);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", pwdn_gpio);
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

static int sensor_st(struct tx_isp_subdev *sd, int enable) {
	return 0;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg) {
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}

	switch (cmd) {
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
		case TX_ISP_EVENT_SENSOR_VFLIP:
			if (arg)
				ret = sensor_set_vflip(sd, sensor_val->value);
			break;
		case TX_ISP_EVENT_SENSOR_LOGIC:
			if (arg)
				ret = sensor_st(sd, sensor_val->value);
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
	sensor_write(sd, reg->reg & 0xff, reg->val & 0xff);

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

static int sensor_probe(struct i2c_client *client,
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

static int sensor_remove(struct i2c_client *client) {
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

static __init int init_sensor(void) {
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
