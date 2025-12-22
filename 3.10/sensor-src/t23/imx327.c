// SPDX-License-Identifier: GPL-2.0+
/*
 * imx327.c
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
#define SENSOR_NAME "imx327"
#define SENSOR_VERSION "H20190822"
#define SENSOR_CHIP_ID_H (0xb2)
#define SENSOR_CHIP_ID_L (0x01)

// ============================================================================
// REGISTER DEFINITIONS
// ============================================================================
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe

// ============================================================================
// TIMING AND PERFORMANCE
// ============================================================================
#define SENSOR_SUPPORT_SCLK (148500000)
#define SENSOR_OUTPUT_MAX_FPS 60
#define SENSOR_OUTPUT_MIN_FPS 5
#define AGAIN_MAX_DB 0x64
#define DGAIN_MAX_DB 0x64
#define LOG2_GAIN_SHIFT 16

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
module_param(data_type, int, S_IRUGO);
MODULE_PARM_DESC(data_type, "Sensor Date Type");

static int wdr_bufsize = 230400;//cache lines corrponding on VPB1
module_param(wdr_bufsize, int, S_IRUGO);
MODULE_PARM_DESC(wdr_bufsize, "Wdr Buf Size");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_25;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

static int rhs1 = 101;

struct regval_list {
    uint16_t reg_num;
    unsigned char value;
};

struct again_lut {
    unsigned int value;
    unsigned int gain;
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again) {
	uint16_t again=(isp_gain*20)>>LOG2_GAIN_SHIFT;
	// Limit Max gain
	if (again>AGAIN_MAX_DB+DGAIN_MAX_DB) again=AGAIN_MAX_DB+DGAIN_MAX_DB;

	/* p_ctx->again=again; */
	*sensor_again=again;
	isp_gain= (((int32_t)again)<<LOG2_GAIN_SHIFT)/20;

	return isp_gain;
}

unsigned int sensor_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	uint16_t again=(isp_gain*20)>>LOG2_GAIN_SHIFT;
	// Limit Max gain
	if (again>AGAIN_MAX_DB+DGAIN_MAX_DB) again=AGAIN_MAX_DB+DGAIN_MAX_DB;

	/* p_ctx->again=again; */
	*sensor_again=again;
	isp_gain= (((int32_t)again)<<LOG2_GAIN_SHIFT)/20;

	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus mipi_2dol_lcg = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 891,
	.lans = 2,
	.settle_time_apative_en = 0,
	.image_twidth = 1952,
	.image_theight = 1109,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 1,
	.mipi_sc.mipi_crop_start0x = 16,
	.mipi_sc.mipi_crop_start0y = 12,
	.mipi_sc.mipi_crop_start1x = 16,
	.mipi_sc.mipi_crop_start1y = 62,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW12,
	.mipi_sc.del_start = 1,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_NOT_VC_MODE,
};

struct tx_isp_mipi_bus mipi_linear = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 445,
	.lans = 2,
	.settle_time_apative_en = 0,
	.image_twidth = 1948,
	.image_theight = 1109,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.mipi_crop_start0x = 12,
	.mipi_sc.mipi_crop_start0y = 20,
	.mipi_sc.mipi_crop_start1x = 0,
	.mipi_sc.mipi_crop_start1y = 0,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW12,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_mipi_bus mipi_linear_60fps = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 891,
	.lans = 2,
	.settle_time_apative_en = 0,
	.image_twidth = 1920,
	.image_theight = 1080,
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

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = SENSOR_CHIP_ID,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x36,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_SONY_MODE,
		.clk = 445,
		.lans = 2,
		.settle_time_apative_en = 0,
		.image_twidth = 1948,
		.image_theight = 1109,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.mipi_crop_start0x = 12,
		.mipi_sc.mipi_crop_start0y = 20,
		.mipi_sc.mipi_crop_start1x = 0,
		.mipi_sc.mipi_crop_start1y = 0,
		.mipi_sc.mipi_crop_start2x = 0,
		.mipi_sc.mipi_crop_start2y = 0,
		.mipi_sc.mipi_crop_start3x = 0,
		.mipi_sc.mipi_crop_start3y = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.mipi_sc.data_type_en = 0,
		.mipi_sc.data_type_value = RAW12,
		.mipi_sc.del_start = 0,
		.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
		.mipi_sc.sensor_fid_mode = 0,
		.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
	},

	.max_again = 589824,
	.max_again_short = 589824,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 1176,
	.min_integration_time_short = 1,
	.max_integration_time_short = 98,
	.integration_time_limit = 1176,
	.total_width = 2136,
	.total_height = 2781,
	.max_integration_time = 1176,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_again_short = sensor_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	.wdr_cache = 0,
	// void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list sensor_init_regs_1920_1080_30fps_mipi_2dol_lcg[] = {
//add
	{0x3000, 0x01},//standy
	{0x3001, 0x01},
	{0x3002, 0x01},//Master stop
//	{SENSOR_REG_DELAY, 0x18},
//	{0x3002, 0x00},
	{0x3005, 0x01},//ADBIT 01:12bit
	{0x3007, 0x40},//WINMODE[6:4]
	{0x3009, 0x01},//FRSEL[1:0]
	{0x300a, 0xf0},
	{0x300c, 0x11},//WDMODE[0] WDSEL[5:4]
	{0x3010, 0x61},//FPGC gain for each gain
	{0x30f0, 0x64},//FPGC1 gain for each gain
	{0x3011, 0x02},
	{0x3018, 0x6E},//Vmax FSC=vmax*2
	{0x3019, 0x05},//0x486
	{0x301c, 0x58},//Hmax
	{0x301d, 0x08},
#if 0
	{0x3018, 0x86},//Vmax FSC=vmax*2
	{0x3019, 0x04},//0x486  1158  60.04fps
	{0x301c, 0x05},//Hmax   2136 0x858
	{0x301d, 0x0a},
#endif
	{0x3020, 0x02},//SHS1 S
	{0x3021, 0x00},
	{0x3024, 0x73},//SHS2 L
	{0x3025, 0x04},
	{0x3028, 0x00},//SHS3
	{0x3029, 0x00},
	{0x302a, 0x00},
	{0x3030, 0x65},//RHS1
	{0x3031, 0x00},
	{0x3032, 0x00},
	{0x3034, 0x00},//RHS2
	{0x3035, 0x00},
	{0x3036, 0x00},
	{0x303c, 0x04},//WINPV[7:0]
	{0x303d, 0x00},//WINPV[2:0]
	{0x303e, 0x41},//WINWV[7:0]
	{0x303f, 0x04},//WINWV[2:0]
	{0x3045, 0x05},
	{0x3046, 0x01},//ODBIT[1:0]
	{0x304b, 0x0a},
	{0x305c, 0x18},
	{0x305d, 0x03},
	{0x305e, 0x20},
	{0x305f, 0x01},
	{0x309e, 0x4a},
	{0x309f, 0x4a},
	{0x30d2, 0x19},
	{0x30d7, 0x03},
	{0x3106, 0x11},
	{0x3129, 0x00},
	{0x313b, 0x61},
	{0x315e, 0x1a},
	{0x3164, 0x1a},
#if 0
	{0x31a0, 0xbc},
	{0x31a1, 0x00},

#endif
	{0x317c, 0x00},
	{0x31ec, 0x0e},
	{0x3204, 0x4a},
	{0x3209, 0xf0},
	{0x320a, 0x22},
	{0x3344, 0x38},
	{0x3405, 0x00},
	{0x3407, 0x01},
	{0x3414, 0x00},//OPB_SIZE_V[5:0]
	{0x3415, 0x00},//NULL0_SIZE_V[5:0]
	{0x3418, 0x7a},//Y_OUT_SIZE[7:0]
	{0x3419, 0x09},//Y_OUT_SIZE[4:0]
	{0x3441, 0x0c},
	{0x3442, 0x0c},
	{0x3443, 0x01},
	{0x3444, 0x20},
	{0x3445, 0x25},
	{0x3446, 0x77},
	{0x3447, 0x00},
	{0x3448, 0x67},
	{0x3449, 0x00},
	{0x344a, 0x47},
	{0x344b, 0x00},
	{0x344c, 0x37},
	{0x344d, 0x00},
	{0x344e, 0x3f},
	{0x344f, 0x00},
	{0x3450, 0xff},
	{0x3451, 0x00},
	{0x3452, 0x3f},
	{0x3453, 0x00},
	{0x3454, 0x37},
	{0x3455, 0x00},
	{0x346a, 0x9c},//EBD
	{0x346b, 0x07},
	{0x3472, 0xa0},
	{0x3473, 0x07},
	{0x347b, 0x23},
	{0x3480, 0x49},
//	{SENSOR_REG_DELAY, 0x18},
	{0x3001, 0x00},//standy cancel
	{0x3002, 0x00},//Master start
	{0x3000, 0x01},//standy cancel
	{SENSOR_REG_END, 0x00},

};

static struct regval_list sensor_init_regs_1920_1080_30fps_mipi[] = {
	{0x3000, 0x01},//standy
	{0x3001, 0x01},
	{0x3002, 0x01},//Master stop
//	{SENSOR_REG_DELAY, 0x18},
	{0x3005, 0x01},
	{0x3007, 0x00},
	{0x3009, 0x02},
	{0x300A, 0xF0},
	{0x300B, 0x00},
	{0x3011, 0x0A},
	{0x3012, 0x64},
	{0x3014, 0x00},
	{0x3018, 0x65},// 1125
	{0x3019, 0x04},
	{0x301A, 0x00},
	{0x301C, 0xa0},//0x1130:30fps 0x14a0:25fps
	{0x301D, 0x14},
	{0x3022, 0x00},
	{0x3046, 0x01},
	{0x3048, 0x00},
	{0x3049, 0x08},
	{0x304B, 0x0A},
	{0x305C, 0x18},
	{0x305D, 0x03},
	{0x305E, 0x20},
	{0x305F, 0x01},
	{0x309E, 0x4A},
	{0x309F, 0x4A},
	{0x30D2, 0x19},
	{0x30D7, 0x03},
	{0x3129, 0x00},
	{0x313B, 0x61},
	{0x315E, 0x1A},
	{0x3164, 0x1A},
	{0x317C, 0x00},
	{0x31EC, 0x0E},
	{0x3405, 0x10},
	{0x3407, 0x01},
	{0x3414, 0x0A},
	{0x3418, 0x49},
	{0x3419, 0x04},
	{0x3441, 0x0C},
	{0x3442, 0x0C},
	{0x3443, 0x01},
	{0x3444, 0x20},
	{0x3445, 0x25},
	{0x3446, 0x57},
	{0x3447, 0x00},
	{0x3448, 0x37},
	{0x3449, 0x00},
	{0x344A, 0x2F},//1f
	{0x344B, 0x00},
	{0x344C, 0x1F},
	{0x344D, 0x00},
	{0x344E, 0x1F},
	{0x344F, 0x00},
	{0x3450, 0x77},
	{0x3451, 0x00},
	{0x3452, 0x1F},
	{0x3453, 0x00},
	{0x3454, 0x17},
	{0x3455, 0x00},
	{0x346a, 0x9c},//EBD
	{0x346b, 0x07},
	{0x3472, 0x9C},
	{0x3473, 0x07},
	{0x3480, 0x49},
//	{SENSOR_REG_DELAY, 0x18},
	{0x3001, 0x00},//standy cancel
	{0x3002, 0x00},//Master start
	{0x3000, 0x01},//standy cancel
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_1920_1080_60fps_mipi[] = {
	/* imx307_cropping_1080_12bit_2lane_mipi_master_i2c_normal_60fps_mode1_37p125mhz_211013 */
	{0x3000, 0x01},
	{0x3002, 0x01},
	{0x3005, 0x01},
	{0x3007, 0x40},
	{0x3009, 0x01},
	{0x300a, 0xf0},
	{0x3011, 0x0a},
	{0x3018, 0x65},
	{0x3019, 0x04},
	{0x301a, 0x00},
	{0x301c, 0x98},
	{0x301d, 0x08},
	{0x3046, 0x01},
	{0x305c, 0x18},
	{0x305d, 0x03},
	{0x305e, 0x20},
	{0x305f, 0x01},
	{0x309e, 0x4a},
	{0x309f, 0x4a},
	{0x311c, 0x0e},
	{0x3128, 0x04},
	{0x3129, 0x00},
	{0x313b, 0x41},
	{0x315e, 0x1a},
	{0x3164, 0x1a},
	{0x3020, 0x0f},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3014, 0x0f},
	{0x317c, 0x00},
	{0x31ec, 0x0e},
	{0x3405, 0x00},
	{0x3407, 0x01},
	{0x3414, 0x06},
	{0x3418, 0x38},
	{0x3419, 0x04},
	{0x3441, 0x0c},
	{0x3442, 0x0c},
	{0x3443, 0x01},
	{0x3444, 0x20},
	{0x3445, 0x25},
	{0x3446, 0x77},
	{0x3447, 0x00},
	{0x3448, 0x67},
	{0x3449, 0x00},
	{0x344a, 0x47},
	{0x344b, 0x00},
	{0x344c, 0x37},
	{0x344d, 0x00},
	{0x344e, 0x3f},
	{0x344f, 0x00},
	{0x3450, 0xff},
	{0x3451, 0x00},
	{0x3452, 0x3f},
	{0x3453, 0x00},
	{0x3454, 0x37},
	{0x3455, 0x00},
	{0x3472, 0x80},
	{0x3473, 0x07},
	{0x3480, 0x49},
	/* win size */
	{0x303a, 0x06},
	{0x303c, 0x08},
	{0x303d, 0x00},
	{0x303e, 0x38},
	{0x303f, 0x04},
	{0x3040, 0x0c},
	{0x3041, 0x00},
	{0x3042, 0x88},
	{0x3043, 0x07},
	{0x3000, 0x00},
	{SENSOR_REG_DELAY, 0X18},
	{0x3002, 0x00},
	{0x304b, 0x0a},
	{SENSOR_REG_END, 0X00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1948*1109 */
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SRGGB12_1X12,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_mipi,
	},
	/* 1948*1109 */
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SRGGB12_1X12,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps_mipi_2dol_lcg,
	},
	{
		.width = 1920,
		.height = 1080,
		.fps = 60 << 16 | 1,
		.mbus_code = V4L2_MBUS_FMT_SRGGB12_1X12,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_60fps_mipi,
	}
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

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value)
{
	int ret;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg>>8)&0xff, reg&0xff};
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

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg,
		 unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg>>8)&0xff, reg&0xff, value};
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
	unsigned char v;
	int ret;

	ret = sensor_read(sd, 0x301e, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x301f, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned short shs1 = 0;

	//short frame use shs1
	shs1 = rhs1 - value - 1;
	ret = sensor_write(sd, 0x3020, (unsigned char)(shs1 & 0xff));
	ret += sensor_write(sd, 0x3021, (unsigned char)((shs1 >> 8) & 0xff));
	ret += sensor_write(sd, 0x3022, (unsigned char)((shs1 >> 16) & 0x3));

	return 0;
}

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned short shs = 0;
	unsigned short vmax = 0;

	if (data_type == TX_SENSOR_DATA_TYPE_LINEAR) {
		vmax = sensor_attr.total_height;
		shs = vmax - value - 1;
		ret = sensor_write(sd, 0x3020, (unsigned char)(shs & 0xff));
		ret += sensor_write(sd, 0x3021, (unsigned char)((shs >> 8) & 0xff));
		ret += sensor_write(sd, 0x3022, (unsigned char)((shs >> 16) & 0x3));
	} else {
		//long frame use shs2
		vmax = sensor_attr.total_height;
		shs = vmax - value - 1;
		ret = sensor_write(sd, 0x3024, (unsigned char)(shs & 0xff));
		ret += sensor_write(sd, 0x3025, (unsigned char)((shs >> 8) & 0xff));
		ret += sensor_write(sd, 0x3026, (unsigned char)((shs >> 16) & 0x3));
	}

	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}

	return 0;
}

static int sensor_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = sensor_write(sd, 0x30f2, (unsigned char)(value & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = sensor_write(sd, 0x3014, (unsigned char)(value & 0xff));
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
		pr_debug("%s stream on\n", SENSOR_NAME);

	}
	else {
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		pr_debug("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	return 0;
}

static int sensor_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	int ret = 0;
	/* struct timeval tv; */

	/* do_gettimeofday(&tv); */
	/* printk("%d:before:time is %d.%d\n", __LINE__,tv.tv_sec,tv.tv_usec); */
	ret = sensor_write(sd, 0x3000, 0x1);
	if (wdr_en == 1) {
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&mipi_2dol_lcg),sizeof(mipi_2dol_lcg));
		data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		wsize = &sensor_win_sizes[1];
		sensor_attr.data_type = data_type;
		sensor_attr.wdr_cache = wdr_bufsize;

		sensor_attr.max_again = 589824;
		sensor_attr.max_again_short = 589824;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 1;
		sensor_attr.min_integration_time_native = 1;
		sensor_attr.max_integration_time_native = 1176;
		sensor_attr.min_integration_time_short = 1;
		sensor_attr.max_integration_time_short = 98;
		sensor_attr.integration_time_limit = 1176;
		sensor_attr.total_width = 2136;
		sensor_attr.total_height = 2781;
		sensor_attr.max_integration_time = 1176;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;

		sensor->video.attr = &sensor_attr;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else if (wdr_en == 0) {
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&mipi_linear),sizeof(mipi_linear));
		data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.data_type = data_type;
		wsize = &sensor_win_sizes[0];

		sensor_attr.data_type = data_type;
		sensor_attr.wdr_cache = wdr_bufsize;
		sensor_attr.max_again = 589824;
		sensor_attr.max_again_short = 589824;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 1;
		sensor_attr.min_integration_time_native = 1;
		sensor_attr.max_integration_time_native = 1123;
		sensor_attr.min_integration_time_short = 1;
		sensor_attr.max_integration_time_short = 98;
		sensor_attr.integration_time_limit = 1123;
		sensor_attr.total_width = 5280;
		sensor_attr.total_height = 1125;
		sensor_attr.max_integration_time = 1123;
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

static int sensor_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
	int ret = 0;

	private_gpio_direction_output(reset_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(reset_gpio, 1);
	private_msleep(1);

	ret = sensor_write_array(sd, wsize->regs);
	ret = sensor_write_array(sd, sensor_stream_on_mipi);

	return 0;
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
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(100);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(100);
		} else {
			ISP_ERROR("gpio request fail %d\n",reset_gpio);
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
	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd) {
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if (arg)
			ret = sensor_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if (arg)
			ret = sensor_set_integration_time_short(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if (arg)
			ret = sensor_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN_SHORT:
		if (arg)
			ret = sensor_set_analog_gain_short(sd, *(int*)arg);
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
		if (arg)
			ret = sensor_write_array(sd, sensor_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (arg)
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_WDR:
		if (arg)
			ret = sensor_set_wdr(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if (arg)
			ret = sensor_set_wdr_stop(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return 0;
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


static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	int ret;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}

	{
		unsigned int arate = 0,mrate = 0;
		unsigned int want_rate = 0;
		struct clk *clka = NULL;
		struct clk *clkm = NULL;

		want_rate=37125000;
		clka = clk_get(NULL, "sclka");
		clkm = clk_get(NULL, "mpll");
		arate = clk_get_rate(clka);
		mrate = clk_get_rate(clkm);
		if ((arate%want_rate) && (mrate%want_rate)) {
			if (want_rate == 37125000) {
				if (arate >= 1400000000) {
					arate = 1485000000;
				} else if ((arate >= 1100) || (arate < 1400)) {
					arate = 1188000000;
				} else if (arate <= 1100) {
				arate = 891000000;
				}
			} else {
				mrate = arate%want_rate;
				arate = arate-mrate;
			}
			clk_set_rate(clka, arate);
			clk_set_parent(sensor->mclk, clka);
		} else if (!(arate%want_rate)) {
			clk_set_parent(sensor->mclk, clka);
		} else if (!(mrate%want_rate)) {
			clk_set_parent(sensor->mclk, clkm);
		}
		private_clk_set_rate(sensor->mclk, want_rate);
		private_clk_enable(sensor->mclk);
	}

	sd = &sensor->sd;
	video = &sensor->video;

	if (data_type == TX_SENSOR_DATA_TYPE_LINEAR) {
		if (sensor_max_fps == TX_SENSOR_MAX_FPS_60) {
			wsize = &sensor_win_sizes[2];
			sensor_info.max_fps = 30;
			data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
			sensor_attr.max_again = 589824;
			sensor_attr.max_dgain = 0;
			sensor_attr.min_integration_time = 1;
			sensor_attr.min_integration_time_native = 1;
			sensor_attr.max_integration_time_native = 1123;
			sensor_attr.integration_time_limit = 1123;
			sensor_attr.total_width = 2200;
			sensor_attr.total_height = 1125;
			sensor_attr.max_integration_time = 1123;
			sensor_attr.integration_time_apply_delay = 2;
			sensor_attr.again_apply_delay = 2;
			sensor_attr.dgain_apply_delay = 0;
			sensor_attr.again = 0;
			sensor_attr.integration_time = 0xf;
			memcpy((void*)(&(sensor_attr.mipi)),(void*)(&mipi_linear_60fps),sizeof(mipi_linear_60fps));
		} else {
			sensor_attr.data_type = data_type;
			wsize = &sensor_win_sizes[0];
			sensor_info.max_fps = 25;

			sensor_attr.max_again = 589824;
			sensor_attr.max_again_short = 589824;
			sensor_attr.max_dgain = 0;
			sensor_attr.min_integration_time = 1;
			sensor_attr.min_integration_time_native = 1;
			sensor_attr.max_integration_time_native = 1123;
			sensor_attr.min_integration_time_short = 1;
			sensor_attr.max_integration_time_short = 98;
			sensor_attr.integration_time_limit = 1123;
			sensor_attr.total_width = 5280;
			sensor_attr.total_height = 1125;
			sensor_attr.max_integration_time = 1123;
			sensor_attr.integration_time_apply_delay = 2;
			sensor_attr.again_apply_delay = 2;
			sensor_attr.dgain_apply_delay = 0;
		}
	} else if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) {
		wsize = &sensor_win_sizes[1];
		sensor_attr.data_type = data_type;
		sensor_attr.wdr_cache = wdr_bufsize;

		sensor_attr.max_again = 589824;
		sensor_attr.max_again_short = 589824;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 1;
		sensor_attr.min_integration_time_native = 1;
		sensor_attr.max_integration_time_native = 1176;
		sensor_attr.min_integration_time_short = 1;
		sensor_attr.max_integration_time_short = 98;
		sensor_attr.integration_time_limit = 1176;
		sensor_attr.total_width = 2136;
		sensor_attr.total_height = 2781;
		sensor_attr.max_integration_time = 1176;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&mipi_2dol_lcg),sizeof(mipi_2dol_lcg));
	} else {
		ISP_ERROR("Can not support this data mode!!!\n");
	}

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

	pr_debug("probe ok ------->%s\n", SENSOR_NAME);

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
}

module_init(init_sensor);
module_exit(exit_sensor);

MODULE_DESCRIPTION("A low-level driver for "SENSOR_NAME" sensor");
MODULE_LICENSE("GPL");
