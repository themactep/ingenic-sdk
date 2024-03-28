// SPDX-License-Identifier: GPL-2.0+
/*
 * imx515.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot	resolution      fps       interface	      mode
 *   0	  3840*2160       20	mipi_2lane	   linear
 *   1	  3840*2160       15	mipi_2lane	   dol
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

#define SENSOR_NAME "imx515"
#define SENSOR_CHIP_ID 0x2823
#define SENSOR_CHIP_ID_H (0x28)
#define SENSOR_CHIP_ID_L (0x23)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK (148500000)
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20230505a"
#define AGAIN_MAX_DB 0x64
#define DGAIN_MAX_DB 0x64
#define LOG2_GAIN_SHIFT 16

static int reset_gpio = -1;
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
module_param(data_type, int, S_IRUGO);
MODULE_PARM_DESC(data_type, "Sensor Date Type");

static int wdr_bufsize = 7080000;
module_param(wdr_bufsize, int, S_IRUGO);
MODULE_PARM_DESC(wdr_bufsize, "Wdr Buf Size");

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
	isp_gain= (((int32_t) again) << LOG2_GAIN_SHIFT) / 20;
	return isp_gain;
}

unsigned int sensor_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	uint16_t again = (isp_gain * 20) > >LOG2_GAIN_SHIFT;
	// Limit Max gain
	if (again > AGAIN_MAX_DB + DGAIN_MAX_DB) again = AGAIN_MAX_DB + DGAIN_MAX_DB;
	/* p_ctx->again=again; */
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
	.image_twidth = 3840,
	.image_theight = 2160,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
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
	.mipi_sc.data_type_value = RAW12,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus sensor_mipi_dol = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 1485,
	.lans = 2,
	.settle_time_apative_en = 0,
	.image_twidth = 3840,
	.image_theight = 2160,
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
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_sensor_attribute sensor_attr={
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

static struct regval_list sensor_init_regs_3840_2160_15fps_sensor_mipi_dol[] = {
	{0x3008, 0x7F},// BCWAIT_TIME[9:0]
	{0x300A, 0x5B},// CPWAIT_TIME[9:0]
	{0x301C, 0x04},// WINMODE[3:0]
	{0x3024, 0x76},// VMAX[19:0] 2422
	{0x3025, 0x09},
	{0x3028, 0xFE},// HMAX[15:0] 1022
	{0x3029, 0x03},
	{0x302C, 0x01},// WDMODE[1:0]
	{0x302D, 0x01},// WDSEL[1:0]
	{0x3031, 0x00},// ADBIT[1:0]
	{0x3032, 0x00},// MDBIT
	{0x3033, 0x08},// SYS_MODE[3:0]
	{0x3040, 0x0C},// PIX_HST[12:0]
	{0x3042, 0x00},// PIX_HWIDTH[12:0]
	{0x3044, 0x20},// PIX_VST[12:0]
	{0x3046, 0xE0},// PIX_VWIDTH[12:0]
	{0x3047, 0x10},
	{0x3050, 0xA4},// SHR0[19:0] 4772
	{0x3051, 0x12},
	{0x3054, 0xC8},// SHR1[19:0] 0x09 -> 0xC8 = 200
	{0x3060, 0xD9},  // RHS1[19:0] init -> 0x11 new -> 0x1D9
	{0x3061, 0x01},
	{0x30C1, 0x00},// XVS_DRV[1:0]
	{0x30CF, 0x01},// XVSMSKCNT_INT[1:0]
	{0x3116, 0x24},// INCKSEL2[7:0]
	{0x3118, 0xA0},// INCKSEL3[10:0]
	{0x311E, 0x24},// INCKSEL5[7:0]
	{0x3260, 0x00},// GAIN
	{0x32D4, 0x21},
	{0x32EC, 0xA1},
	{0x344C, 0x2B},
	{0x344D, 0x01},
	{0x344E, 0xED},
	{0x344F, 0x01},
	{0x3450, 0xF6},
	{0x3451, 0x02},
	{0x3452, 0x7F},
	{0x3453, 0x03},
	{0x358A, 0x04},
	{0x35A1, 0x02},
	{0x35EC, 0x27},
	{0x35EE, 0x8D},
	{0x35F0, 0x8D},
	{0x35F2, 0x29},
	{0x36BC, 0x0C},
	{0x36CC, 0x53},
	{0x36CD, 0x00},
	{0x36CE, 0x3C},
	{0x36D0, 0x8C},
	{0x36D1, 0x00},
	{0x36D2, 0x71},
	{0x36D4, 0x3C},
	{0x36D6, 0x53},
	{0x36D7, 0x00},
	{0x36D8, 0x71},
	{0x36DA, 0x8C},
	{0x36DB, 0x00},
	{0x3701, 0x00},// ADBIT1[7:0]
	{0x3720, 0x00},
	{0x3724, 0x02},
	{0x3726, 0x02},
	{0x3732, 0x02},
	{0x3734, 0x03},
	{0x3736, 0x03},
	{0x3742, 0x03},
	{0x3862, 0xE0},
	{0x38CC, 0x30},
	{0x38CD, 0x2F},
	{0x395C, 0x0C},
	{0x39A4, 0x07},
	{0x39A8, 0x32},
	{0x39AA, 0x32},
	{0x39AC, 0x32},
	{0x39AE, 0x32},
	{0x39B0, 0x32},
	{0x39B2, 0x2F},
	{0x39B4, 0x2D},
	{0x39B6, 0x28},
	{0x39B8, 0x30},
	{0x39BA, 0x30},
	{0x39BC, 0x30},
	{0x39BE, 0x30},
	{0x39C0, 0x30},
	{0x39C2, 0x2E},
	{0x39C4, 0x2B},
	{0x39C6, 0x25},
	{0x3A42, 0xD1},
	{0x3A4C, 0x77},
	{0x3AE0, 0x02},
	{0x3AEC, 0x0C},
	{0x3B00, 0x2E},
	{0x3B06, 0x29},
	{0x3B98, 0x25},
	{0x3B99, 0x21},
	{0x3B9B, 0x13},
	{0x3B9C, 0x13},
	{0x3B9D, 0x13},
	{0x3B9E, 0x13},
	{0x3BA1, 0x00},
	{0x3BA2, 0x06},
	{0x3BA3, 0x0B},
	{0x3BA4, 0x10},
	{0x3BA5, 0x14},
	{0x3BA6, 0x18},
	{0x3BA7, 0x1A},
	{0x3BA8, 0x1A},
	{0x3BA9, 0x1A},
	{0x3BAC, 0xED},
	{0x3BAD, 0x01},
	{0x3BAE, 0xF6},
	{0x3BAF, 0x02},
	{0x3BB0, 0xA2},
	{0x3BB1, 0x03},
	{0x3BB2, 0xE0},
	{0x3BB3, 0x03},
	{0x3BB4, 0xE0},
	{0x3BB5, 0x03},
	{0x3BB6, 0xE0},
	{0x3BB7, 0x03},
	{0x3BB8, 0xE0},
	{0x3BBA, 0xE0},
	{0x3BBC, 0xDA},
	{0x3BBE, 0x88},
	{0x3BC0, 0x44},
	{0x3BC2, 0x7B},
	{0x3BC4, 0xA2},
	{0x3BC8, 0xBD},
	{0x3BCA, 0xBD},
	{0x4001, 0x01},// LANEMODE[2:0]
	{0x4004, 0x48},// TXCLKESC_FREQ[15:0]
	{0x4005, 0x09},
	{0x4018, 0xA7},// TCLKPOST[15:0]
	{0x401A, 0x57},// TCLKPREPARE[15:0]
	{0x401C, 0x5F},// TCLKTRAIL[15:0]
	{0x401E, 0x97},// TCLKZERO[15:0]
	{0x4020, 0x5F},// THSPREPARE[15:0]
	{0x4022, 0xAF},// THSZERO[15:0]
	{0x4024, 0x5F},// THSTRAIL[15:0]
	{0x4026, 0x9F},// THSEXIT[15:0]
	{0x4028, 0x4F},// TLPX[15:0]
	{0x3000, 0x00},
	{0xfffe, 0x18},
	{0x3002, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_3840_2160_20fps_mipi[] = {
	{0x3008, 0x7F},  // BCWAIT_TIME[9:0]
	{0x300A, 0x5B},  // CPWAIT_TIME[9:0]
	{0x301C, 0x04},  // WINMODE[3:0]
	{0x3024, 0xB5},  // VMAX[19:0] 0x9B5 -> 2485
	{0x3025, 0x09},  //
	{0x3028, 0xD6},  // HMAX[15:0] 0x5D6 -> 1494
	{0x3029, 0x05},  //
	{0x3033, 0x06},  // SYS_MODE[3:0]
	{0x3040, 0x0C},  // PIX_HST[12:0]
	{0x3042, 0x00},  // PIX_HWIDTH[12:0]
	{0x3044, 0x20},  // PIX_VST[12:0]
	{0x3046, 0xE0},  // PIX_VWIDTH[12:0]
	{0x3047, 0x10},  //
	{0x3050, 0x08},  // SHR0[19:0]
	{0x30C1, 0x00},  // XVS_DRV[1:0]
	{0x3116, 0x24},  // INCKSEL2[7:0]
	{0x3118, 0x80},  // INCKSEL3[10:0]
	{0x311E, 0x24},  // INCKSEL5[7:0]
	{0x32D4, 0x21},
	{0x32EC, 0xA1},
	{0x344C, 0x2B},
	{0x344D, 0x01},
	{0x344E, 0xED},
	{0x344F, 0x01},
	{0x3450, 0xF6},
	{0x3451, 0x02},
	{0x3452, 0x7F},
	{0x3453, 0x03},
	{0x358A, 0x04},
	{0x35A1, 0x02},
	{0x35EC, 0x27},
	{0x35EE, 0x8D},
	{0x35F0, 0x8D},
	{0x35F2, 0x29},
	{0x36BC, 0x0C},
	{0x36CC, 0x53},
	{0x36CD, 0x00},
	{0x36CE, 0x3C},
	{0x36D0, 0x8C},
	{0x36D1, 0x00},
	{0x36D2, 0x71},
	{0x36D4, 0x3C},
	{0x36D6, 0x53},
	{0x36D7, 0x00},
	{0x36D8, 0x71},
	{0x36DA, 0x8C},
	{0x36DB, 0x00},
	{0x3720, 0x00},
	{0x3724, 0x02},
	{0x3726, 0x02},
	{0x3732, 0x02},
	{0x3734, 0x03},
	{0x3736, 0x03},
	{0x3742, 0x03},
	{0x3862, 0xE0},
	{0x38CC, 0x30},
	{0x38CD, 0x2F},
	{0x395C, 0x0C},
	{0x39A4, 0x07},
	{0x39A8, 0x32},
	{0x39AA, 0x32},
	{0x39AC, 0x32},
	{0x39AE, 0x32},
	{0x39B0, 0x32},
	{0x39B2, 0x2F},
	{0x39B4, 0x2D},
	{0x39B6, 0x28},
	{0x39B8, 0x30},
	{0x39BA, 0x30},
	{0x39BC, 0x30},
	{0x39BE, 0x30},
	{0x39C0, 0x30},
	{0x39C2, 0x2E},
	{0x39C4, 0x2B},
	{0x39C6, 0x25},
	{0x3A42, 0xD1},
	{0x3A4C, 0x77},
	{0x3AE0, 0x02},
	{0x3AEC, 0x0C},
	{0x3B00, 0x2E},
	{0x3B06, 0x29},
	{0x3B98, 0x25},
	{0x3B99, 0x21},
	{0x3B9B, 0x13},
	{0x3B9C, 0x13},
	{0x3B9D, 0x13},
	{0x3B9E, 0x13},
	{0x3BA1, 0x00},
	{0x3BA2, 0x06},
	{0x3BA3, 0x0B},
	{0x3BA4, 0x10},
	{0x3BA5, 0x14},
	{0x3BA6, 0x18},
	{0x3BA7, 0x1A},
	{0x3BA8, 0x1A},
	{0x3BA9, 0x1A},
	{0x3BAC, 0xED},
	{0x3BAD, 0x01},
	{0x3BAE, 0xF6},
	{0x3BAF, 0x02},
	{0x3BB0, 0xA2},
	{0x3BB1, 0x03},
	{0x3BB2, 0xE0},
	{0x3BB3, 0x03},
	{0x3BB4, 0xE0},
	{0x3BB5, 0x03},
	{0x3BB6, 0xE0},
	{0x3BB7, 0x03},
	{0x3BB8, 0xE0},
	{0x3BBA, 0xE0},
	{0x3BBC, 0xDA},
	{0x3BBE, 0x88},
	{0x3BC0, 0x44},
	{0x3BC2, 0x7B},
	{0x3BC4, 0xA2},
	{0x3BC8, 0xBD},
	{0x3BCA, 0xBD},
	{0x4001, 0x01},  // LANEMODE[2:0]
	{0x4004, 0x48},  // TXCLKESC_FREQ[15:0]
	{0x4005, 0x09},  //
	{0x400C, 0x00},  // INCKSEL6
	{0x4018, 0x8F},  // TCLKPOST[15:0]
	{0x401A, 0x4F},  // TCLKPREPARE[15:0]
	{0x401C, 0x47},  // TCLKTRAIL[15:0]
	{0x401E, 0x37},  // TCLKZERO[15:0]
	{0x4020, 0x4F},  // THSPREPARE[15:0]
	{0x4022, 0x87},  // THSZERO[15:0]
	{0x4024, 0x4F},  // THSTRAIL[15:0]
	{0x4026, 0x7F},  // THSEXIT[15:0]
	{0x4028, 0x3F},  // TLPX[15:0]
	{0x3000, 0x00},
	{SENSOR_REG_DELAY, 0x18},
	{0x3002, 0x00},
	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 3840,
		.height = 2160,
		.fps = 20 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SGBRG12_1X12,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_3840_2160_20fps_mipi,
	},
	/* 1948*1109 [1]*/
	{
		.width = 3840,
		.height = 2160,
		.fps = 15 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SGBRG10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_3840_2160_15fps_sensor_mipi_dol,
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
	ret = sensor_read(sd, 0x3b00, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;

	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;

	*ident = v;
	ret = sensor_read(sd, 0x3b06, &v);
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
	int rhs1 = 473;
	shs1 = rhs1 - (value << 1);
	ret = sensor_write(sd, 0x3054, (unsigned char) (shs1 & 0xff));
	ret += sensor_write(sd, 0x3055, (unsigned char) ((shs1 >> 8) & 0xff));
	ret += sensor_write(sd, 0x3056, (unsigned char) ((shs1 >> 16) & 0x3));
	return 0;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	unsigned short shs = 0;
	unsigned short vmax = 0;

	if (data_type == TX_SENSOR_DATA_TYPE_LINEAR) {
		vmax = sensor_attr.total_height;
		shs = vmax - value;
		ret = sensor_write(sd, 0x3050, (unsigned char) (shs & 0xff));
		ret += sensor_write(sd, 0x3051, (unsigned char) ((shs >> 8) & 0xff));
		ret += sensor_write(sd, 0x3052, (unsigned char) ((shs >> 16) & 0x0f));
	}
	if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) {
		vmax = sensor_attr.total_height;
		shs = 2 * vmax - (value << 1);
		ret = sensor_write(sd, 0x3050, (unsigned char) (shs & 0xff));
		ret += sensor_write(sd, 0x3051, (unsigned char) ((shs >> 8) & 0xff));
		ret += sensor_write(sd, 0x3052, (unsigned char) ((shs >> 16) & 0x0f));
	}
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain_short(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	ret += sensor_write(sd, 0x3092, (unsigned char) (value & 0xff));
	ret += sensor_write(sd, 0x3093, (unsigned char) ((value >> 8) & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	ret += sensor_write(sd, 0x3090, (unsigned char) (value & 0xff));
	ret += sensor_write(sd, 0x3091, (unsigned char) ((value >> 8) & 0xff));
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
		sclk = 1494 * 2485 * 20;
		max_fps = TX_SENSOR_MAX_FPS_20;
		break;
	case 1:
		sclk = 2422 * 1022 * 15;
		max_fps = TX_SENSOR_MAX_FPS_20;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps<< 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}

	ret += sensor_read(sd, 0x3029, &val);
	hts = val & 0x0f;
	ret += sensor_read(sd, 0x3028, &val);
	hts = (hts << 8) + val;
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += sensor_write(sd, 0x3001, 0x01);
	ret = sensor_write(sd, 0x3026, (unsigned char)((vts & 0xf0000) >> 16));
	ret += sensor_write(sd, 0x3025, (unsigned char)((vts & 0xff00) >> 8));
	ret += sensor_write(sd, 0x3024, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x3001, 0x00);
	if (0 != ret) {
		ISP_ERROR("err: sensor_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts -8;
	sensor->video.attr->integration_time_limit = vts -8;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts -8;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
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
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_dol),sizeof(sensor_mipi_dol));
		data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		wsize = &sensor_win_sizes[1];
		sensor_attr.wdr_cache = wdr_bufsize;
		sensor_attr.max_again = 404346;
		sensor_attr.max_again_short = 404346;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 4;
		sensor_attr.min_integration_time_native = 4;
		sensor_attr.max_integration_time_native = 2181;
		sensor_attr.min_integration_time_short = 4;
		sensor_attr.max_integration_time_short = 232;
		sensor_attr.integration_time_limit = 2181;
		sensor_attr.total_width = 1022;
		sensor_attr.total_height = 2422;
		sensor_attr.max_integration_time = 2181;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor->video.attr = &sensor_attr;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else if (wdr_en == 0) {
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_linear),sizeof(sensor_mipi_linear));
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
		sensor_attr.max_integration_time_native = 2485 - 8;
		sensor_attr.integration_time_limit = 2485 - 8;
		sensor_attr.total_width = 1494;
		sensor_attr.total_height = 2485;
		sensor_attr.max_integration_time = 2485 - 8;
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
	switch(info->default_boot) {
	case 0:
		wsize = &sensor_win_sizes[0];
		data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.max_again = 404346;
		sensor_attr.max_again_short = 404346;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 4;
		sensor_attr.min_integration_time_native = 4;
		sensor_attr.max_integration_time_native = 2485 - 8;
		sensor_attr.integration_time_limit = 2485 - 8;
		sensor_attr.total_width = 1494;
		sensor_attr.total_height = 2485;
		sensor_attr.max_integration_time = 2485 - 8;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_linear),sizeof(sensor_mipi_linear));
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
		sensor_attr.min_integration_time = 4;
		sensor_attr.min_integration_time_native = 4;
		sensor_attr.max_integration_time_native = 2181;
		sensor_attr.min_integration_time_short = 4;
		sensor_attr.max_integration_time_short = 232;
		sensor_attr.integration_time_limit = 2181;
		sensor_attr.total_width = 1022;
		sensor_attr.total_height = 2422;
		sensor_attr.max_integration_time = 2181;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&sensor_mipi_dol),sizeof(sensor_mipi_dol));
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
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(100);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(100);
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

static struct tx_isp_subdev_sensor_ops  sensor_sensor_ops = {
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
	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}

	memset(sensor, 0 ,sizeof(*sensor));
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &sensor_attr;
	sensor_attr.expo_fs = 1;
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

static __init int init_sensor(void) {
	return private_i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void) {
	private_i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for Sony imx515 sensor");
MODULE_LICENSE("GPL");
