// SPDX-License-Identifier: GPL-2.0+
/* imx334.c
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

#define SENSOR_NAME "imx334"
#define SENSOR_CHIP_ID_H (0x18)
#define SENSOR_CHIP_ID_L (0x0f)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_SCLK_8M (74250000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define AGAIN_MAX_DB 0x64
#define DGAIN_MAX_DB 0x64
#define LOG2_GAIN_SHIFT 16
#define SENSOR_VERSION "H20220531a"

/* t40 sensor hvflip function only takes effect when shvflip == 1 */
static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static int reset_gpio = -1;
static int pwdn_gpio = -1;
char* __attribute__((weak)) sclk_name[4];

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
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
struct tx_isp_mipi_bus sensor_mipi={
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 891,
	.lans = 4,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.image_twidth = 3864,
	.image_theight = 2202,
	.mipi_sc.mipi_crop_start0x = 12,
	.mipi_sc.mipi_crop_start0y = 34,
	.mipi_sc.mipi_crop_start1x = 0,
	.mipi_sc.mipi_crop_start1y = 0,
	.mipi_sc.mipi_crop_start2x = 0,
	.mipi_sc.mipi_crop_start2y = 0,
	.mipi_sc.mipi_crop_start3x = 0,
	.mipi_sc.mipi_crop_start3y = 0,
	.mipi_sc.data_type_en = 1,
	.mipi_sc.data_type_value = RAW12,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x2003,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x1a,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.max_again = 404346,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 2245,
	.integration_time_limit = 2245,
	.total_width = 1100,
	.total_height = 2250,
	.max_integration_time = 2245,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_3840_2160_30fps_mipi[] = {
#if 0
	/*
	 * IMX334LQR Window cropping 3840x2164 CSI-2_4lane 37.125Mhz AD:10bit Output:10bit 891Mbps Master Mode 30fps
	 * Ver7.0
	 * */
	{0x300C, 0x5B},  // BCWAIT_TIME[7:0]
	{0x300D, 0x40},  // CPWAIT_TIME[7:0]
	{0x3018, 0x04},  // WINMODE[3:0]
	{0x302C, 0x3C},  // HTRIMMING_START[11:0]
	{0x302E, 0x00},  // HNUM[11:0]
	{0x3034, 0x4C},  // HMAX[15:0]
	{0x3035, 0x04},  //
	{0x3050, 0x00},  // ADBIT[0]
	{0x3058, 0x27},  // SHR0[19:0]
	{0x3059, 0x06},  //
	{0x3074, 0xC0},  // AREA3_ST_ADR_1[12:0]
	{0x3076, 0x74},  // AREA3_WIDTH_1[12:0]
	{0x308E, 0xC1},  // AREA3_ST_ADR_2[12:0]
	{0x3090, 0x74},  // AREA3_WIDTH_2[12:0]
	{0x30D8, 0x20},  // UNREAD_ED_ADR[12:0]
	{0x30D9, 0x12},  //
	{0x30E8, 0x14},  // GAIN[10:0]
	{0x315A, 0x06},  // INCKSEL2[1:0]
	{0x316A, 0x7E},  // INCKSEL4[1:0]
	{0x319D, 0x00},  // MDBIT
	{0x319E, 0x02},  // SYS_MODE
	{0x31A1, 0x00},  // XVS_DRV[1:0]
	{0x3288, 0x21},  // -
	{0x328A, 0x02},  // -
	{0x3308, 0x70},  // Y_OUT_SIZE[12:0]//0x74
	{0x3414, 0x05},  // -
	{0x3416, 0x18},  // -
	{0x341C, 0xFF},  // ADBIT1[8:0]
	{0x341D, 0x01},  //
	{0x35AC, 0x0E},
	{0x3648, 0x01},
	{0x364A, 0x04},
	{0x364C, 0x04},
	{0x3678, 0x01},
	{0x367C, 0x31},
	{0x367E, 0x31},
	{0x3708, 0x02},
	{0x3714, 0x01},
	{0x3715, 0x02},
	{0x3716, 0x02},
	{0x3717, 0x02},
	{0x371C, 0x3D},
	{0x371D, 0x3F},
	{0x372C, 0x00},
	{0x372D, 0x00},
	{0x372E, 0x46},
	{0x372F, 0x00},
	{0x3730, 0x89},
	{0x3731, 0x00},
	{0x3732, 0x08},
	{0x3733, 0x01},
	{0x3734, 0xFE},
	{0x3735, 0x05},
	{0x375D, 0x00},
	{0x375E, 0x00},
	{0x375F, 0x61},
	{0x3760, 0x06},
	{0x3768, 0x1B},
	{0x3769, 0x1B},
	{0x376A, 0x1A},
	{0x376B, 0x19},
	{0x376C, 0x18},
	{0x376D, 0x14},
	{0x376E, 0x0F},
	{0x3776, 0x00},
	{0x3777, 0x00},
	{0x3778, 0x46},
	{0x3779, 0x00},
	{0x377A, 0x08},
	{0x377B, 0x01},
	{0x377C, 0x45},
	{0x377D, 0x01},
	{0x377E, 0x23},
	{0x377F, 0x02},
	{0x3780, 0xD9},
	{0x3781, 0x03},
	{0x3782, 0xF5},
	{0x3783, 0x06},
	{0x3784, 0xA5},
	{0x3788, 0x0F},
	{0x378A, 0xD9},
	{0x378B, 0x03},
	{0x378C, 0xEB},
	{0x378D, 0x05},
	{0x378E, 0x87},
	{0x378F, 0x06},
	{0x3790, 0xF5},
	{0x3792, 0x43},
	{0x3794, 0x7A},
	{0x3796, 0xA1},
	{0x3A18, 0x7F},  // TCLKPOST[15:0]
	{0x3A1A, 0x37},  // TCLKPREPARE[15:0]
	{0x3A1C, 0x37},  // TCLKTRAIL[15:0]
	{0x3A1E, 0xF7},  // TCLKZERO[15:0]
	{0x3A1F, 0x00},  //
	{0x3A20, 0x3F},  // THSPREPARE[15:0]
	{0x3A22, 0x6F},  // THSZERO[15:0]
	{0x3A24, 0x3F},  // THSTRAIL[15:0]
	{0x3A26, 0x5F},  // THSEXIT[15:0]
	{0x3A28, 0x2F},  // TLPX[15:0]
	{0x3E04, 0x0E},
#endif

	/*
	 * All-pixel scan mode 3840*2160@30fps 12bit 891Mbps mipi 4lane
	 */
	{0x3000, 0x01},  // standby
	{0x3001, 0x00},
	{0x3002, 0x01},
	{0x3003, 0x00},
	{0x300C, 0x3B},
	{0x300D, 0x2A},
	{0x3018, 0x00},
	{0x302C, 0x30},
	{0x302D, 0x00},
	{0x302E, 0x18},
	{0x302F, 0x0F},
	{0x3030, 0xCA},
	{0x3031, 0x08},
	{0x3032, 0x00},
	{0x3033, 0x00},
	{0x3034, 0x4C},
	{0x3035, 0x04},
	{0x304C, 0x14},
	{0x3050, 0x01},
	{0x3076, 0x84},
	{0x3077, 0x08},
	{0x3090, 0x84},
	{0x3091, 0x08},
	{0x30C6, 0x00},
	{0x30C7, 0x00},
	{0x30CE, 0x00},
	{0x30CF, 0x00},
	{0x30D8, 0xF8},
	{0x30D9, 0x11},
	{0x3117, 0x00},
	{0x314C, 0x29},
	{0x314D, 0x01},
	{0x315A, 0x06},
	{0x3168, 0xA0},
	{0x316A, 0x7E},
	{0x3199, 0x00},
	{0x319D, 0x01},
	{0x319E, 0x02},
	{0x31A0, 0x2A},
	{0x31D4, 0x00},
	{0x31D5, 0x00},
	{0x31DD, 0x03},
	{0x3288, 0x21},
	{0x328A, 0x02},
	{0x3300, 0x00},
	{0x3302, 0x32},
	{0x3303, 0x00},
	{0x3308, 0x84},
	{0x3309, 0x08},
	{0x3414, 0x05},
	{0x3416, 0x18},
	{0x341C, 0x47},
	{0x341D, 0x00},
	{0x35AC, 0x0E},
	{0x3648, 0x01},
	{0x364A, 0x04},
	{0x364C, 0x04},
	{0x3678, 0x01},
	{0x367C, 0x31},
	{0x367E, 0x31},
	{0x3708, 0x02},
	{0x3714, 0x01},
	{0x3715, 0x02},
	{0x3716, 0x02},
	{0x3717, 0x02},
	{0x371C, 0x3D},
	{0x371D, 0x3F},
	{0x372C, 0x00},
	{0x372D, 0x00},
	{0x372E, 0x46},
	{0x372F, 0x00},
	{0x3730, 0x89},
	{0x3731, 0x00},
	{0x3732, 0x08},
	{0x3733, 0x01},
	{0x3734, 0xFE},
	{0x3735, 0x05},
	{0x375D, 0x00},
	{0x375E, 0x00},
	{0x375F, 0x61},
	{0x3760, 0x06},
	{0x3768, 0x1B},
	{0x3769, 0x1B},
	{0x376A, 0x1A},
	{0x376B, 0x19},
	{0x376C, 0x18},
	{0x376D, 0x14},
	{0x376E, 0x0F},
	{0x3776, 0x00},
	{0x3777, 0x00},
	{0x3778, 0x46},
	{0x3779, 0x00},
	{0x377A, 0x08},
	{0x377B, 0x01},
	{0x377C, 0x45},
	{0x377D, 0x01},
	{0x377E, 0x23},
	{0x377F, 0x02},
	{0x3780, 0xD9},
	{0x3781, 0x03},
	{0x3782, 0xF5},
	{0x3783, 0x06},
	{0x3784, 0xA5},
	{0x3788, 0x0F},
	{0x378A, 0xD9},
	{0x378B, 0x03},
	{0x378C, 0xEB},
	{0x378D, 0x05},
	{0x378E, 0x87},
	{0x378F, 0x06},
	{0x3790, 0xF5},
	{0x3792, 0x43},
	{0x3794, 0x7A},
	{0x3796, 0xA1},
	{0x3A01, 0x03},
	{0x3A18, 0x7F},
	{0x3A19, 0x00},
	{0x3A1A, 0x37},
	{0x3A1B, 0x00},
	{0x3A1C, 0x37},
	{0x3A1D, 0x00},
	{0x3A1E, 0xF7},
	{0x3A1F, 0x00},
	{0x3A20, 0x3F},
	{0x3A21, 0x00},
	{0x3A22, 0x6F},
	{0x3A23, 0x00},
	{0x3A24, 0x3F},
	{0x3A25, 0x00},
	{0x3A26, 0x5F},
	{0x3A27, 0x00},
	{0x3A28, 0x2F},
	{0x3A29, 0x00},
	{0x3E04, 0x0E},
	{0x3078, 0x02},
	{0x3079, 0x00},
	{0x307A, 0x00},
	{0x307B, 0x00},

	{0x3081, 0x00},
	{0x3082, 0x00},
	{0x3083, 0x00},
	{0x3088, 0x02},
	{0x3094, 0x00},
	{0x3095, 0x00},
	{0x3096, 0x00},
	{0x309C, 0x00},
	{0x309D, 0x00},
	{0x309E, 0x00},
	{0x30A4, 0x00},
	{0x30A5, 0x00},
	{0x304E, 0x00},
	{0x304F, 0x00},
	{0x3074, 0xB0},
	{0x3075, 0x00},
	{0x308E, 0xB1},
	{0x308F, 0x00},
	{0x30B6, 0x00},
	{0x30B7, 0x00},
	{0x3116, 0x00},
	{0x3080, 0x02},
	{0x309B, 0x02},

	{0x3000, 0x00},
	{SENSOR_REG_DELAY, 0x20},
	{0x3002, 0x00},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_init_regs_3840_2160_15fps_mipi[] = {
    {SENSOR_REG_END, 0x00},
};


static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 3840*2160@30fps [0]*/
	{
		.width = 3840,
		.height = 2160,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB12_1X12,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_3840_2160_30fps_mipi,
	},
	/* 3840*2160@15fps [1]*/
	{
		.width = 3840,
		.height = 2160,
		.fps = 15 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_3840_2160_15fps_mipi,

	}
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sensor_stream_on_mipi[] = {
	//{0x0100, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	//{0x0100, 0x00},
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

	ret = sensor_read(sd, 0x302e, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x302f, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
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
	unsigned short shs = 0;
	unsigned short vmax = 0;

	vmax = sensor_attr.total_height;
	shs = vmax - value;
	ret = sensor_write(sd, 0x3058, (unsigned char)(shs & 0xff));
	ret += sensor_write(sd, 0x3059, (unsigned char)((shs >> 8) & 0xff));
	ret += sensor_write(sd, 0x305a, (unsigned char)((shs >> 16) & 0x0f));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sensor_write(sd, 0x30e8, (unsigned char)(value & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

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
	sensor->video.state = TX_ISP_MODULE_INIT;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sensor_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if (sensor->video.state == TX_ISP_MODULE_RUNNING) {
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
			ISP_WARNING("%s stream on\n", SENSOR_NAME);
		}

	} else {
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int wpclk = 0;
	unsigned short vts = 0;
	unsigned short hts=0;
	unsigned int max_fps = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch(sensor->info.default_boot) {
	case 0:
		wpclk = SENSOR_SUPPORT_SCLK_8M;
		max_fps = TX_SENSOR_MAX_FPS_30;
		break;
	case 1:
		wpclk = SENSOR_SUPPORT_SCLK_8M;
		max_fps = TX_SENSOR_MAX_FPS_15;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) not in range\n", fps);
		return -1;
	}
	ret += sensor_read(sd, 0x3035, &tmp);
	hts = tmp & 0x0f;
	ret += sensor_read(sd, 0x3034, &tmp);
	if (ret < 0)
		return -1;
	hts = (hts << 8) + tmp;

	vts = wpclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += sensor_write(sd, 0x3001, 0x01);
	ret = sensor_write(sd, 0x3032, (unsigned char)((vts & 0xf0000) >> 16));
	ret += sensor_write(sd, 0x3031, (unsigned char)((vts & 0xff00) >> 8));
	ret += sensor_write(sd, 0x3030, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x3001, 0x00);
	if (ret < 0)
		return -1;

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts- 5;
	sensor->video.attr->integration_time_limit = vts - 5;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 5;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return 0;
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

static int sensor_set_hvflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val_h;
	uint8_t val_v;
	uint8_t reg_3080 = 0x02;
	uint8_t reg_309b = 0x02;

	/*
	 * 2'b01:mirror,2'b10:filp
	 * 0x3081 0x3082 must be changed as blow in all-pixel scan mode
	 */
	ret = sensor_read(sd, 0x304e, &val_h);
	ret += sensor_read(sd, 0x304f, &val_v);
	switch(enable) {
	case 0://normal
		val_h &= 0xfc;
		val_v &= 0xfc;
		reg_3080 = 0x02;
		reg_309b = 0x02;
		break;
	case 1://sensor mirror
		val_h |= 0x01;
		val_v &= 0xfc;
		reg_3080 = 0x02;
		reg_309b = 0x02;
		break;
	case 2://sensor flip
		val_h &= 0xfc;
		val_v |= 0x01;
		reg_3080 = 0xfe;
		reg_309b = 0xfe;
		break;
	case 3://sensor mirror&flip
		val_h |= 0x01;
		val_v |= 0x01;
		reg_3080 = 0xfe;
		reg_309b = 0xfe;
		break;
	}
	ret = sensor_write(sd, 0x304e, val_h);
	ret += sensor_write(sd, 0x304f, val_v);
	ret += sensor_write(sd, 0x3080, reg_3080);
	ret += sensor_write(sd, 0x309b, reg_309b);

	return ret;
}

struct clk *sclka;
static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	//struct i2c_client *client = tx_isp_get_subdevdata(sd);
	//struct clk *tclk;
	unsigned long rate;
	//uint8_t i;

	memcpy(&(sensor_attr.mipi), &sensor_mipi, sizeof(sensor_mipi));
	switch(info->default_boot) {
	case 0:
		wsize = &sensor_win_sizes[0];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.again = 0;
		sensor_attr.integration_time = 0x5;
		break;
	case 1:
		wsize = &sensor_win_sizes[1];
		/*
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sensor_attr.max_integration_time_native = 4503 - 4;
		sensor_attr.integration_time_limit = 4503 - 4;
		sensor_attr.total_width = 1066;
		sensor_attr.total_height = 4503;
		sensor_attr.max_integration_time = 4503 - 4;
		*/
		sensor_attr.again = 0;
		sensor_attr.integration_time = 0x5;
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
		break;
	}

	switch(info->video_interface) {
	case TISP_SENSOR_VI_MIPI_CSI0:
		sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sensor_attr.mipi.index = 0;
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
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
#if 0
	if (((rate / 1000) % 37125) != 0) {
		ret = clk_set_parent(sclka, clk_get(NULL, "epll"));
		if (ret != 0)
			pr_err("%s %d set parent clk to epll err!\n",__func__,__LINE__);
		sclka = private_devm_clk_get(&client->dev, "epll");
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
#endif
	private_clk_set_rate(sensor->mclk, 24000000);
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
		/*	case TX_ISP_EVENT_SENSOR_EXPO:
			if (arg)
			ret = sensor_set_expo(sd, sensor_val->value);
			break;   */
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
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sensor_write_array(sd, sensor_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if (arg)
			ret = sensor_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if (arg)
			ret = sensor_set_hvflip(sd, sensor_val->value);
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
	memset(sensor, 0 ,sizeof(*sensor));

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->dev = &client->dev;
	sensor_attr.expo_fs = 1;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sensor_attr;
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
