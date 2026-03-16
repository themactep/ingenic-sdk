// SPDX-License-Identifier: GPL-2.0+
/*
 * gc2083.c
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

// ============================================================================
// SENSOR IDENTIFICATION
// ============================================================================
#define SENSOR_NAME "gc2083"
#define SENSOR_VERSION "H20220228a"
#define SENSOR_CHIP_ID 0x2083
#define SENSOR_CHIP_ID_H (0x20)
#define SENSOR_CHIP_ID_L (0x83)

// ============================================================================
// HARDWARE INTERFACE
// ============================================================================
#define SENSOR_BUS_TYPE TX_SENSOR_CONTROL_INTERFACE_I2C
#define SENSOR_I2C_ADDRESS 0x37

// ============================================================================
// SENSOR CAPABILITIES
// ============================================================================
#define SENSOR_MAX_WIDTH 1920
#define SENSOR_MAX_HEIGHT 1080

// ============================================================================
// REGISTER DEFINITIONS
// ============================================================================
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe

// ============================================================================
// TIMING AND PERFORMANCE
// ============================================================================
#define SENSOR_SUPPORT_30FPS_SCLK (99000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5

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

static struct sensor_info sensor_info = {
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.version = SENSOR_VERSION,
	.min_fps = SENSOR_OUTPUT_MIN_FPS,
	.max_fps = SENSOR_OUTPUT_MAX_FPS,
	.actual_fps = 0,
	.chip_i2c_addr = SENSOR_I2C_ADDRESS,
	.width = SENSOR_MAX_WIDTH,
	.height = SENSOR_MAX_HEIGHT,
};

struct regval_list {
    uint16_t reg_num;
    unsigned char value;
};

struct again_lut {
    int index;
    unsigned int again_pga;
    unsigned int exp_rate_darkc;
    unsigned int darks_g1;
    unsigned int darks_r1;
    unsigned int darks_b1;
    unsigned int darks_g2;
    unsigned int darkn_g1;
    unsigned int darkn_r1;
    unsigned int darkn_b1;
    unsigned int darkn_g2;
    unsigned int col_gain0;
    unsigned int col_gain1;
    unsigned int again_shift;
    unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	// inx, 0x00d0 0x0155 0x0410 0x0411 0x0412 0x0413 0x0414 0x0415 0x0416 0x0417 0x00b8 0x00b9 0x0dc1   gain
	{0x00, 0x00, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x01, 0x00, 0x00, 0},      //   1.000000
	{0x01, 0x10, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x01, 0x0c, 0x00, 16247},  //   1.187500
	{0x02, 0x01, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x01, 0x1a, 0x00, 32233},  //   1.406250
	{0x03, 0x11, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x01, 0x2b, 0x00, 48592},  //   1.671875
	{0x04, 0x02, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x02, 0x00, 0x00, 65536},  //   2.000000
	{0x05, 0x12, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x02, 0x18, 0x00, 81783},  //   2.375000
	{0x06, 0x03, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x02, 0x33, 0x00, 97243},  //   2.796875
	{0x07, 0x13, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x03, 0x15, 0x00, 113685}, //   3.328125
	{0x08, 0x04, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x04, 0x00, 0x00, 131072}, //   4.000000
	{0x09, 0x14, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x04, 0xe0, 0x00, 147629}, //   4.765625
	{0x0a, 0x05, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x05, 0x26, 0x00, 162779}, //   5.593750
	{0x0b, 0x15, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x06, 0x2b, 0x00, 179442}, //   6.671875
	{0x0c, 0x44, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x08, 0x00, 0x00, 196608}, //   8.000000
	{0x0d, 0x54, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x09, 0x22, 0x00, 213165}, //   9.531250
	{0x0e, 0x45, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x0b, 0x0d, 0x00, 228446}, //  11.203125
	{0x0f, 0x55, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f, 0x0d, 0x16, 0x00, 244978}, //  13.343750
	{0x10, 0x04, 0x19, 0x16, 0x16, 0x16, 0x16, 0x6f, 0x6f, 0x6f, 0x6f, 0x10, 0x00, 0x01, 262144}, //  16.000000
	{0x11, 0x14, 0x19, 0x16, 0x16, 0x16, 0x16, 0x6f, 0x6f, 0x6f, 0x6f, 0x13, 0x04, 0x01, 278701}, //  19.062500
	{0x12, 0x24, 0x19, 0x16, 0x16, 0x16, 0x16, 0x6f, 0x6f, 0x6f, 0x6f, 0x16, 0x1a, 0x01, 293982}, //  22.406250
	{0x13, 0x34, 0x19, 0x16, 0x16, 0x16, 0x16, 0x6f, 0x6f, 0x6f, 0x6f, 0x1a, 0x2b, 0x01, 310458}, //  26.671875
	{0x14, 0x44, 0x36, 0x18, 0x18, 0x18, 0x18, 0x6f, 0x6f, 0x6f, 0x6f, 0x20, 0x00, 0x01, 327680}, //  32.000000
	{0x15, 0x54, 0x36, 0x18, 0x18, 0x18, 0x18, 0x6f, 0x6f, 0x6f, 0x6f, 0x26, 0x07, 0x01, 344199}, //  38.109375
	{0x16, 0x64, 0x36, 0x18, 0x18, 0x18, 0x18, 0x6f, 0x6f, 0x6f, 0x6f, 0x2c, 0x33, 0x01, 359485}, //  44.796875
	{0x17, 0x74, 0x36, 0x18, 0x18, 0x18, 0x18, 0x6f, 0x6f, 0x6f, 0x6f, 0x35, 0x17, 0x01, 376022}, //  53.359375
	{0x18, 0x84, 0x64, 0x16, 0x16, 0x16, 0x16, 0x72, 0x72, 0x72, 0x72, 0x35, 0x17, 0x01, 393216}, //  64.000000
	{0x19, 0x94, 0x64, 0x16, 0x16, 0x16, 0x16, 0x72, 0x72, 0x72, 0x72, 0x35, 0x17, 0x01, 409735}, //  76.218750
	{0x1a, 0x85, 0x64, 0x16, 0x16, 0x16, 0x16, 0x72, 0x72, 0x72, 0x72, 0x35, 0x17, 0x01, 425021}, //  89.593750
	{0x1b, 0x95, 0x64, 0x16, 0x16, 0x16, 0x16, 0x72, 0x72, 0x72, 0x72, 0x35, 0x17, 0x01, 441558}, // 106.718750
	{0x1c, 0xa5, 0x64, 0x16, 0x16, 0x16, 0x16, 0x72, 0x72, 0x72, 0x72, 0x35, 0x17, 0x01, 456839}, // 125.437500
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	struct again_lut *lut = sensor_again_lut;
	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].index;
			return lut[0].gain;
		} else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->index;
			return (lut - 1)->gain;
		} else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->index;
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
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = SENSOR_BUS_TYPE,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 500,
		.lans = 2,
		.settle_time_apative_en = 1,
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
	.max_again = 456839,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 0x546 - 4,
	.integration_time_limit = 0x546 - 4,
	.total_width = 0Xb74,
	.total_height = 0x546,
	.max_integration_time = 0x546 - 4,
	.one_line_expr_in_us = 30,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

/*
 * window size:1920x1080
 * mclk=27mhz
 * mipi_rate=495Mbps/lane
 * rowtime=29.61616us
 * max frame rate:30fps
 * hts=2932,vts=1125 for 30fps
 * bayer mode:RGGB
 * */
static struct regval_list sensor_init_regs_1920_1080_30fps_mipi[] = {
	/* SYS */
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03f2, 0x00},
	{0x03f3, 0x00},
	{0x03f4, 0x36},
	{0x03f5, 0xc0},
	{0x03f6, 0x24},
	{0x03f7, 0x01},
	{0x03f8, 0x2c},
	{0x03f9, 0x43},
	{0x03fc, 0x8e},
	{0x0381, 0x07},
	{0x00d7, 0x29},

	/* ANALOG & CISCTL */
	{0x0d6d, 0x18},
	{0x00d5, 0x03},
	{0x0082, 0x01},
	{0x0db3, 0xd4},
	{0x0db0, 0x0d},
	{0x0db5, 0x96},
	{0x0d03, 0x02},
	{0x0d04, 0x02},
	{0x0d05, 0x05},
	{0x0d06, 0xba},
	{0x0d07, 0x00},
	{0x0d08, 0x11},
	{0x0d09, 0x00},
	{0x0d0a, 0x02},
	{0x000b, 0x00},
	{0x000c, 0x02},
	{0x0d0d, 0x04},
	{0x0d0e, 0x40},
	{0x000f, 0x07},
	{0x0010, 0x90},
	{0x0017, 0x0c},
	{0x0d73, 0x92},
	{0x0076, 0x00}, /*use frame length to change fps*/
	{0x0d76, 0x00},
	{0x0d41, 0x05},
	{0x0d42, 0x46}, /*vts for 25fps*/
	{0x0d7a, 0x10},
	{0x0d19, 0x31},
	{0x0d25, 0x0b},
	{0x0d20, 0x60},
	{0x0d27, 0x03},
	{0x0d29, 0x60},
	{0x0d43, 0x10},
	{0x0d49, 0x10},
	{0x0d55, 0x18},
	{0x0dc2, 0x44},
	{0x0058, 0x3c},
	{0x00d8, 0x68},
	{0x00d9, 0x14},
	{0x00da, 0xc1},
	{0x0050, 0x18},
	{0x0db6, 0x3d},
	{0x00d2, 0xbc},
	{0x0d66, 0x42},
	{0x008c, 0x07},
	{0x008d, 0xff},

	/* GAIN */
	{0x007a, 0x50}, //global gain
	{0x00d0, 0x00},
	{0x0dc1, 0x00},

	/* ISP */
	{0x0102, 0xa9}, //89
	{0x0158, 0x00},
	{0x0107, 0xa6},
	{0x0108, 0xa9},
	{0x0109, 0xa8},
	{0x010a, 0xa7},
	{0x010b, 0xff},
	{0x010c, 0xff},
	{0x0428, 0x86}, //84 edge_th
	{0x0429, 0x86}, //84
	{0x042a, 0x86}, //84
	{0x042b, 0x68}, //84
	{0x042c, 0x68}, //84
	{0x042d, 0x68}, //84
	{0x042e, 0x68}, //83
	{0x042f, 0x68}, //82
	{0x0430, 0x4f}, //82 noise_th
	{0x0431, 0x68}, //82
	{0x0432, 0x67}, //82
	{0x0433, 0x66}, //82
	{0x0434, 0x66}, //82
	{0x0435, 0x66}, //82
	{0x0436, 0x66}, //64
	{0x0437, 0x66}, //68
	{0x0438, 0x62}, //flat_th
	{0x0439, 0x62},
	{0x043a, 0x62},
	{0x043b, 0x62},
	{0x043c, 0x62},
	{0x043d, 0x62},
	{0x043e, 0x62},
	{0x043f, 0x62},

	/* DARK SUN */
	{0x0077, 0x01}, //settle_en
	{0x0078, 0x65}, //settle_exp_th
	{0x0079, 0x04}, //settle_exp_th
	{0x0067, 0xa0}, //0xa0 //settle_ref_th
	{0x0054, 0xff}, //settle_sig_th
	{0x0055, 0x02}, //settle_sig_th
	{0x0056, 0x00}, //settle_colgain_th
	{0x0057, 0x04}, //settle_colgain_th
	{0x005a, 0xff}, //settle_data_value
	{0x005b, 0x07}, //settle_data_value

	/* BLK */
	{0x0026, 0x01},
	{0x0152, 0x02},
	{0x0153, 0x50},
	{0x0155, 0x03},
	{0x0410, 0x11},
	{0x0411, 0x11},
	{0x0412, 0x11},
	{0x0413, 0x11},
	{0x0414, 0x6f},
	{0x0415, 0x6f},
	{0x0416, 0x6f},
	{0x0417, 0x6f},
	{0x04e0, 0x18},

	/* WINDOW */
	{0x0192, 0x04},
	{0x0194, 0x04},
	{0x0195, 0x04},
	{0x0196, 0x38},
	{0x0197, 0x07},
	{0x0198, 0x80},

	/* DVP & MIPI */
	{0x0201, 0x27},
	{0x0202, 0x53},
	{0x0203, 0xce},
	{0x0204, 0x40},
	{0x0212, 0x07},
	{0x0213, 0x80},
	{0x0215, 0x12},
	{0x0229, 0x05},
	{0x0237, 0x03},
	{0x023e, 0x99},

	{SENSOR_REG_END, 0x00},
};

static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_mipi,
	},
};

struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

static struct regval_list sensor_stream_on_mipi[] = {
	{0x023e, 0x99},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{0x023e, 0x00},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value) {
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

static int sensor_reset(struct tx_isp_subdev *sd, int val) {
	return 0;
}

static int sensor_detect(struct tx_isp_subdev *sd, unsigned int *ident) {
	unsigned char v;
	int ret;
	ret = sensor_read(sd, 0x03f0, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;

	*ident = v;
	ret = sensor_read(sd, 0x03f1, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;

	*ident = (*ident << 8) | v;
	return 0;
}

static int sensor_set_expo(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;
	struct again_lut *val_lut = sensor_again_lut;

	/* analog gain */
	ret += sensor_write(sd, 0xd0, val_lut[again].again_pga);
	ret += sensor_write(sd, 0x31d, 0x2e);
	ret += sensor_write(sd, 0xdc1, val_lut[again].again_shift);
	ret += sensor_write(sd, 0x31d, 0x28);
	ret += sensor_write(sd, 0xb8, val_lut[again].col_gain0);
	ret += sensor_write(sd, 0xb9, val_lut[again].col_gain1);

	/* blc */
	ret += sensor_write(sd, 0x0155, val_lut[again].exp_rate_darkc);
	ret += sensor_write(sd, 0x0410, val_lut[again].darks_g1);
	ret += sensor_write(sd, 0x0411, val_lut[again].darks_r1);
	ret += sensor_write(sd, 0x0412, val_lut[again].darks_b1);
	ret += sensor_write(sd, 0x0413, val_lut[again].darks_g2);
	ret += sensor_write(sd, 0x0414, val_lut[again].darkn_g1);
	ret += sensor_write(sd, 0x0415, val_lut[again].darkn_r1);
	ret += sensor_write(sd, 0x0416, val_lut[again].darkn_b1);
	ret += sensor_write(sd, 0x0417, val_lut[again].darkn_g2);

	/* integration time */
	ret += sensor_write(sd, 0x0d03, (unsigned char) ((it >> 8) & 0xff));
	ret += sensor_write(sd, 0x0d04, (unsigned char) (it & 0xff));
	if (0 != ret)
		ISP_ERROR("%s reg write err!!\n");

	return ret;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value) {
	return 0;
}
#endif

static int sensor_set_logic(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_init(struct tx_isp_subdev *sd, int enable) {
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

	sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);
	ret = sensor_write_array(sd, wsize->regs);
	if (ret)
		return ret;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;
	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, int enable) {
	int ret = 0;
	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("%s stream on\n", SENSOR_NAME);
	} else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI) {
			ret = sensor_write_array(sd, sensor_stream_off_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
	}
	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int clk = SENSOR_SUPPORT_30FPS_SCLK;
	unsigned short vts = 0;
	unsigned short hts = 0;
	unsigned char val;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}

	ret += sensor_read(sd, 0xd05, &val);
	hts = val;
	ret += sensor_read(sd, 0xd06, &val);
	if (ret < 0)
		return -1;

	hts = ((hts << 8) + val) << 1;
	vts = clk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	//ret += sensor_write(sd, 0x31d, 0x2e);
	ret += sensor_write(sd, 0x0d41, (unsigned char) ((vts & 0xff00) >> 8));
	ret += sensor_write(sd, 0x0d42, (unsigned char) (vts & 0xff));
	//ret += sensor_write(sd, 0x31d, 0x28);
	if (ret < 0)
		return -1;

	sensor->video.fps = fps;


	sensor_update_actual_fps((fps >> 16) & 0xffff);
	sensor->video.attr->max_integration_time_native = vts - 8;
	sensor->video.attr->integration_time_limit = vts - 8;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 8;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;
	if (wsize) {
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;

		sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}
	return ret;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip) {
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio, "sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
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
	ret = sensor_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an %s chip.\n", client->addr, client->adapter->name, SENSOR_NAME);
		return ret;
	}

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

static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable) {
	int ret = -1;
	unsigned char val = 0x0;
	unsigned char col_start = 0x0;
	ret = sensor_read(sd, 0x0d15, &val);
	if (enable & 0x2) {
		val |= 0x02;
		col_start = 0x01;
	} else {
		val &= 0xfd;
		col_start = 0x00;
	}
	ret += sensor_write(sd, 0x0d15, val);
	ret += sensor_write(sd, 0x0192, col_start);
	return ret;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg) {
	long ret = 0;
	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd) {
		case TX_ISP_EVENT_SENSOR_EXPO:
			if (arg)
				ret = sensor_set_expo(sd, *(int *) arg);
			break;
		case TX_ISP_EVENT_SENSOR_INT_TIME:
//			if (arg)
//				ret = sensor_set_integration_time(sd, *(int *) arg);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
//			if (arg)
//				ret = sensor_set_analog_gain(sd, *(int *) arg);
			break;
		case TX_ISP_EVENT_SENSOR_DGAIN:
			if (arg)
				ret = sensor_set_digital_gain(sd, *(int *) arg);
			break;
		case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
			if (arg)
				ret = sensor_get_black_pedestal(sd, *(int *) arg);
			break;
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if (arg)
				ret = sensor_set_mode(sd, *(int *) arg);
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
				ret = sensor_set_fps(sd, *(int *) arg);
			break;
		case TX_ISP_EVENT_SENSOR_VFLIP:
			if (arg)
				ret = sensor_set_vflip(sd, *(int *) arg);
			break;
		case TX_ISP_EVENT_SENSOR_LOGIC:
			if (arg)
				ret = sensor_set_logic(sd, *(int *) arg);
		default:
			break;
	}
	return ret;
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

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id) {
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	unsigned int rate = 0;
	int ret = -1;

	sensor = (struct tx_isp_sensor *) kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}

	memset(sensor, 0, sizeof(*sensor));
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	rate = clk_get_rate(clk_get_parent(sensor->mclk));
	if (((rate / 1000) % 27000) != 0) {
		struct clk *vpll;
		vpll = clk_get(NULL, "vpll");
		if (IS_ERR(vpll)) {
			pr_err("get vpll failed\n");
		} else {
			rate = clk_get_rate(vpll);
			if (((rate / 1000) % 27000) != 0) {
				clk_set_rate(vpll, 1080000000);
			}
			ret = clk_set_parent(sensor->mclk, vpll);
			if (ret < 0)
				pr_err("set mclk parent as epll err\n");
		}
	}

	private_clk_set_rate(sensor->mclk, 27000000);
	private_clk_enable(sensor->mclk);

	/*
	  convert sensor-gain into isp-gain,
	*/
	sensor_attr.max_dgain = 0;
	sensor_attr.expo_fs = 1;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sensor_attr;
	sensor_info.max_fps = 30;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;

	sensor_update_actual_fps((wsize->fps >> 16) & 0xffff);
	tx_isp_subdev_init(&sensor_platform_device, sd, &sensor_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);
	pr_debug("probe ok ------->%s\n", SENSOR_NAME);
	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);
	return -1;
}

static int sensor_remove(struct i2c_client *client) {
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
	int ret = 0;
	sensor_common_init(&sensor_info);

	ret = private_driver_get_interface();
	if (ret) {
		ISP_ERROR("Failed to init %s driver.\n", SENSOR_NAME);
		return -1;
	}
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	private_i2c_del_driver(&sensor_driver);
	sensor_common_exit();
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
