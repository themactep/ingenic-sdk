// SPDX-License-Identifier: GPL-2.0+
/*
 * imx347.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          2688*1520       30        mipi_2lane           linear
 *   1          2688*1520       15        mipi_2lane            dol
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

#define SENSOR_NAME "imx347"
#define SENSOR_CHIP_ID_H (0x98)
#define SENSOR_CHIP_ID_L (0x0a)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20230505"
#define AGAIN_MAX_DB 0x64
#define DGAIN_MAX_DB 0x64
#define LOG2_GAIN_SHIFT 16

static int reset_gpio = GPIO_PC(28);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
module_param(data_type, int, S_IRUGO);
MODULE_PARM_DESC(data_type, "Sensor Date Type");

static int wdr_bufsize = 2520000;//230400;
module_param(wdr_bufsize, int, S_IRUGO);
MODULE_PARM_DESC(wdr_bufsize, "Wdr Buf Size");


struct regval_list {
	uint16_t reg_num;
	unsigned char value;
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

struct tx_isp_mipi_bus mipi_2lane_dol = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 1118,
	.lans = 2,
	.settle_time_apative_en = 0,
	.image_twidth = 2688,
	.image_theight = 1522,
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


struct tx_isp_mipi_bus mipi_linear = {
	.mode = SENSOR_MIPI_SONY_MODE,
	.clk = 1118,
	.lans = 2,
	.settle_time_apative_en = 0,
	.image_twidth = 2688,
	.image_theight = 1522,
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
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x890a,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x37,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.max_again = 458752,
	.max_again_short = 131072,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_again_short = sensor_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
	.wdr_cache = 0,
};

static struct regval_list sensor_init_regs_2688_1520_15fps_mipi_2lane_dol[] = {
	{0x300C,0x5B},  // BCWAIT_TIME[7:0]
	{0x300D,0x40},  // CPWAIT_TIME[7:0]
	{0x3018,0x04},  // WINMODE[3:0]
	{0x302C,0x30},  // HTRIMMING_START[11:0]
	{0x302E,0x80},  // HNUM[11:0]
	{0x3034,0xDC},  // HMAX[15:0] =0x5dc =1500
	{0x3035,0x05},  //
	{0x3048,0x01},  // WDMODE[0]
	{0x3049,0x01},  // WDSEL[1:0]
	{0x304A,0x04},  // WD_SET1[2:0]
	{0x304B,0x04},  // WD_SET2[3:0]
	{0x304C,0x13},  // OPB_SIZE_V[5:0]
	{0x3050,0x00},  // ADBIT[0]
	{0x3056,0xF2},  // Y_OUT_SIZE[12:0]
	{0x3057,0x05},  //
	{0x3058,0xF4},  // SHR0[19:0]
	{0x3059,0x0A},  //
	{0x3068,0xC9},  //=======RHS1[19:0]0x3d = 61 -> 0xC9 = 201
	{0x3069,0x00},
	{0x3074,0x44},  // AREA3_ST_ADR_1[12:0]
	{0x3076,0xF2},  // AREA3_WIDTH_1[12:0]
	{0x3077,0x05},  //
	{0x30BE,0x5E},  // -
	{0x30C6,0x00},  // BLACK_OFSET_ADR[12:0]
	{0x30CE,0x00},  // UNRD_LINE_MAX[12:0]
	{0x30D8,0x3C},  // UNREAD_ED_ADR[12:0]
	{0x315A,0x02},  // INCKSEL2[1:0]
	{0x316A,0x7E},  // INCKSEL4[1:0]
	{0x319D,0x00},  // MDBIT
	{0x31A1,0x00},  // XVS_DRV[1:0]
	{0x31D7,0x01},  // XVSMSKCNT_INT[1:0]
	{0x3200,0x00},  //
	{0x3202,0x02},  // -
	{0x3288,0x22},  // -
	{0x328A,0x02},  // -
	{0x328C,0xA2},  // -
	{0x328E,0x22},  // -
	{0x3415,0x27},  // -
	{0x3418,0x27},  // -
	{0x3428,0xFE},  // -
	{0x349E,0x6A},  // -
	{0x34A2,0x9A},  // -
	{0x34A4,0x8A},  // -
	{0x34A6,0x8E},  // -
	{0x34AA,0xD8},  // -
	{0x3648,0x01},  // -
	{0x3678,0x01},  // -
	{0x367C,0x69},  // -
	{0x367E,0x69},  // -
	{0x3680,0x69},  // -
	{0x3682,0x69},  // -
	{0x371D,0x05},  // -
	{0x375D,0x11},  // -
	{0x375E,0x43},  // -
	{0x375F,0x76},  // -
	{0x3760,0x07},  // -
	{0x3768,0x1B},  // -
	{0x3769,0x1B},  // -
	{0x376A,0x1A},  // -
	{0x376B,0x19},  // -
	{0x376C,0x17},  // -
	{0x376D,0x0F},  // -
	{0x376E,0x0B},  // -
	{0x376F,0x0B},  // -
	{0x3770,0x0B},  // -
	{0x3776,0x89},  // -
	{0x3777,0x00},  // -
	{0x3778,0xCA},  // -
	{0x3779,0x00},  // -
	{0x377A,0x45},  // -
	{0x377B,0x01},  // -
	{0x377C,0x56},  // -
	{0x377D,0x02},  // -
	{0x377E,0xFE},  // -
	{0x377F,0x03},  // -
	{0x3780,0xFE},  // -
	{0x3781,0x05},  // -
	{0x3782,0xFE},  // -
	{0x3783,0x06},  // -
	{0x3784,0x7F},  // -
	{0x3788,0x1F},  // -
	{0x378A,0xCA},  // -
	{0x378B,0x00},  // -
	{0x378C,0x45},  // -
	{0x378D,0x01},  // -
	{0x378E,0x56},  // -
	{0x378F,0x02},  // -
	{0x3790,0xFE},  // -
	{0x3791,0x03},  // -
	{0x3792,0xFE},  // -
	{0x3793,0x05},  // -
	{0x3794,0xFE},  // -
	{0x3795,0x06},  // -
	{0x3796,0x7F},  // -
	{0x3798,0xBF},  // -
	{0x3A01,0x01},  // LANEMODE[2:0]
	{0x3000,0x00},
	{SENSOR_REG_DELAY, 0x12},
	{0x3002,0x00},
	{0x31A1,0x00},
	{SENSOR_REG_END, 0x00},
};


static struct regval_list sensor_init_regs_2688_1520_30fps_mipi[] = {
	{0x300C,0x5B},  // BCWAIT_TIME[7:0]
    {0x300D,0x40},  // CPWAIT_TIME[7:0]
    {0x3018,0x04},  // WINMODE[3:0]
    {0x302C,0x30},  // HTRIMMING_START[11:0] 0X30 = 48
    {0x302E,0x80},  // HNUM[11:0] 0Xa80 = 2688
    {0x3034,0xDC},  // HMAX[15:0] 0x5dc = 1500
    {0x3035,0x05},  //
    {0x3050,0x00},  // ADBIT[0]
    {0x3056,0xF2},  // Y_OUT_SIZE[12:0] 0X5F2 = 1522
    {0x3057,0x05},  //
    {0x3074,0x44},  // AREA3_ST_ADR_1[12:0] 0X44 = 68
    {0x3076,0xF2},  // AREA3_WIDTH_1[12:0] 0x5f2 = 1522
    {0x3077,0x05},  //
    {0x30BE,0x5E},  //
    {0x30C6,0x00},  // BLACK_OFSET_ADR[12:0] 0x0
    {0x30CE,0x00},  // UNRD_LINE_MAX[12:0] 0x0
    {0x30D8,0x3C},  // UNREAD_ED_ADR[12:0] 0x63c = 1596
    {0x315A,0x02},  // INCKSEL2[1:0]
    {0x316A,0x7E},  // INCKSEL4[1:0]
    {0x319D,0x00},  // MDBIT
    {0x31A1,0x00},  // XVS_DRV[1:0]
    {0x3202,0x02},  // -
    {0x3288,0x22},  // -
    {0x328A,0x02},  // -
    {0x328C,0xA2},  // -
    {0x328E,0x22},  // -
    {0x3415,0x27},  // -
    {0x3418,0x27},  // -
    {0x3428,0xFE},  // -
    {0x349E,0x6A},  // -
    {0x34A2,0x9A},  // -
    {0x34A4,0x8A},  // -
    {0x34A6,0x8E},  // -
    {0x34AA,0xD8},  // -
    {0x3648,0x01},  // -
    {0x3678,0x01},  // -
    {0x367C,0x69},  // -
    {0x367E,0x69},  // -
    {0x3680,0x69},  // -
    {0x3682,0x69},  // -
    {0x371D,0x05},  // -
    {0x375D,0x11},  // -
    {0x375E,0x43},  // -
    {0x375F,0x76},  // -
    {0x3760,0x07},  // -
    {0x3768,0x1B},  // -
    {0x3769,0x1B},  // -
    {0x376A,0x1A},  // -
    {0x376B,0x19},  // -
    {0x376C,0x17},  // -
    {0x376D,0x0F},  // -
    {0x376E,0x0B},  // -
    {0x376F,0x0B},  // -
    {0x3770,0x0B},  // -
    {0x3776,0x89},  // -
    {0x3777,0x00},  // -
    {0x3778,0xCA},  // -
    {0x3779,0x00},  // -
    {0x377A,0x45},  // -
    {0x377B,0x01},  // -
    {0x377C,0x56},  // -
    {0x377D,0x02},  // -
    {0x377E,0xFE},  // -
    {0x377F,0x03},  // -
    {0x3780,0xFE},  // -
    {0x3781,0x05},  // -
    {0x3782,0xFE},  // -
    {0x3783,0x06},  // -
    {0x3784,0x7F},  // -
    {0x3788,0x1F},  // -
    {0x378A,0xCA},  // -
    {0x378B,0x00},  // -
    {0x378C,0x45},  // -
    {0x378D,0x01},  // -
    {0x378E,0x56},  // -
    {0x378F,0x02},  // -
    {0x3790,0xFE},  // -
    {0x3791,0x03},  // -
    {0x3792,0xFE},  // -
    {0x3793,0x05},  // -
    {0x3794,0xFE},  // -
    {0x3795,0x06},  // -
    {0x3796,0x7F},  // -
    {0x3798,0xBF},  // -
    {0x3A01,0x01},  // LANEMODE[2:0]
	{0x3000,0x00},
	{SENSOR_REG_DELAY, 0x12},
	{0x3002,0x00},
	{0x31a1,0x00},
	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	{
		.width = 2688,
		.height = 1520,
		.fps = 30 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2688_1520_30fps_mipi,
	},
	{
		.width = 2688,
		.height = 1520,
		.fps = 15 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SRGGB10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_2688_1520_15fps_mipi_2lane_dol,
	},
};

static struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

/*
 * the part of driver was fixed.
 */

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

#if 0
static int sensor_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
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

	ret = sensor_read(sd, 0x302e, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x302f, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
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
	unsigned short shr0 = 0;
	unsigned short vmax = 0;

	vmax = sensor_attr.total_height;
	if (data_type == TX_SENSOR_DATA_TYPE_LINEAR) shr0 = vmax - value - 1;
	else shr0 = (vmax - value - 1) << 1;
	ret = sensor_write(sd, 0x3058, (unsigned char)(shr0 & 0xff));
	ret += sensor_write(sd, 0x3059, (unsigned char)((shr0 >> 8) & 0xff));
	ret += sensor_write(sd, 0x305A, (unsigned char)((shr0 >> 16) & 0xf));
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}

	return 0;
}

static int sensor_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned short shr1 = 0;
	unsigned short rhs1 = 201;

	shr1 = rhs1 - (value << 1);
	ret = sensor_write(sd, 0x305c, (unsigned char)(shr1 & 0xff));
	ret += sensor_write(sd, 0x305d, (unsigned char)((shr1 >> 8) & 0xff));
	ret += sensor_write(sd, 0x305e, (unsigned char)((shr1 >> 16) & 0xf));

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sensor_write(sd, 0x30e8, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x30e9, (unsigned char)((value >> 8) & 0x07));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sensor_write(sd, 0x30ea, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x30eb, (unsigned char)((value >> 8) & 0x07));
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

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
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

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = 0;
	unsigned short hmax = 0;
	unsigned short vmax = 0;
	unsigned char value = 0;
	unsigned int newformat = 0; //the format is 24.8
	unsigned int max_integration = 0;
	unsigned char max_fps = 25;
	struct tx_isp_sensor_register_info *info = &sensor->info;

	switch(info->default_boot)
	{
		case 0:
			sclk = 74250000; /* 1500*1650*30 */
			max_fps = 25;
			break;
		case 1:
			sclk = 37125000; /* 1500*1650*15 */
			max_fps = 25;
			break;
	}
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		pr_debug("warn: fps(%d) not in range\n", fps);
		return -1;
	}



	/*method 2 change vts*/
	ret = sensor_read(sd, 0x3035, &value);
	hmax = value;
	value = 0;
	ret += sensor_read(sd, 0x3034, &value);
	hmax = (hmax << 8) | value;

	vmax = sclk * (fps & 0xffff) / hmax / ((fps & 0xffff0000) >> 16);

	ret += sensor_write(sd, 0x3030, vmax & 0xff);
	ret += sensor_write(sd, 0x3031, (vmax >> 8) & 0xff);
	ret += sensor_write(sd, 0x3032, (vmax >> 16) & 0x03);

	switch(info->default_boot)
	{
		case 0:
			max_integration = vmax - 3;
			break;
		case 1:
			max_integration = vmax - 105;
			break;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = max_integration;
	sensor->video.attr->integration_time_limit = max_integration;
	sensor->video.attr->total_height = vmax;
	sensor->video.attr->max_integration_time = max_integration;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	//ret = sensor_set_integration_time(sd,cur_int);
	if (ret < 0)
		return -1;

	return ret;
}

static int sensor_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = 0;
	/* struct timeval tv; */

	/* do_gettimeofday(&tv); */
	/* pr_debug("%d:before:time is %d.%d\n", __LINE__,tv.tv_sec,tv.tv_usec); */
	ret = sensor_write(sd, 0x3000, 0x1);
	if (wdr_en == 1) {
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&mipi_2lane_dol),sizeof(mipi_2lane_dol));
		data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		info->default_boot = 1;
		wsize = &sensor_win_sizes[1];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		sensor_attr.wdr_cache = wdr_bufsize;
		sensor_attr.max_again = 458752;
		sensor_attr.max_again_short = 131072;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 1;
		sensor_attr.min_integration_time_native = 1;
		sensor_attr.max_integration_time_native = 1545;
		sensor_attr.min_integration_time_short = 1;
		sensor_attr.max_integration_time_short = 96;
		sensor_attr.integration_time_limit = 1545;
		sensor_attr.total_width = 1500;
		sensor_attr.total_height = 1650;
		sensor_attr.max_integration_time = 1545;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;
		sensor->video.attr = &sensor_attr;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	} else if (wdr_en == 0) {
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&mipi_linear),sizeof(mipi_linear));
		data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		info->default_boot = 0;
		sensor_attr.data_type = data_type;
		wsize = &sensor_win_sizes[0];
		sensor_attr.wdr_cache = wdr_bufsize;
		sensor_attr.max_again = 458752;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 1;
		sensor_attr.min_integration_time_native = 1;
		sensor_attr.min_integration_time_short = 1;
		sensor_attr.max_integration_time_native = 1647;
		sensor_attr.integration_time_limit = 1647;
		sensor_attr.total_width = 1500;
		sensor_attr.total_height = 1650;
		sensor_attr.max_integration_time = 1647;
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
		sensor->video.mbus.field = TISP_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

struct clk *sclka;
static int sensor_attr_check(struct tx_isp_subdev *sd)
{
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
		sensor_attr.max_again = 458752;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 1;
		sensor_attr.min_integration_time_native = 1;
		sensor_attr.min_integration_time_short = 1;
		sensor_attr.max_integration_time_native = 1647;
		sensor_attr.integration_time_limit = 1647;
		sensor_attr.total_width = 1500;
		sensor_attr.total_height = 1650;
		sensor_attr.max_integration_time = 1647;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;
		sensor_attr.again = 0;
		sensor_attr.integration_time = 0x03;
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&mipi_linear),sizeof(mipi_linear));
		break;
	case 1:
		wsize = &sensor_win_sizes[1];
		sensor_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		sensor_attr.wdr_cache = wdr_bufsize;
		sensor_attr.max_again = 458752;
		sensor_attr.max_again_short = 131072;
		sensor_attr.max_dgain = 0;
		sensor_attr.min_integration_time = 1;
		sensor_attr.min_integration_time_native = 1;
		sensor_attr.max_integration_time_native = 1545;
		sensor_attr.min_integration_time_short = 1;
		sensor_attr.max_integration_time_short = 96;
		sensor_attr.integration_time_limit = 1545;
		sensor_attr.total_width = 1500;
		sensor_attr.total_height = 1650;
		sensor_attr.max_integration_time = 1545;
		sensor_attr.integration_time_apply_delay = 2;
		sensor_attr.again_apply_delay = 2;
		sensor_attr.dgain_apply_delay = 0;
		sensor_attr.again = 0;
		//sensor_attr.integration_time = 0x148;
		memcpy((void*)(&(sensor_attr.mipi)),(void*)(&mipi_2lane_dol),sizeof(mipi_2lane_dol));
		break;
	default:
		ISP_ERROR("Have no this MCLK Source!!!\n");
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
	if (((rate / 1000) % 37125) != 0) {
		ret = clk_set_parent(sclka, clk_get(NULL, "epll"));
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
	if (chip) {
		memcpy(chip->name, SENSOR_NAME, sizeof(SENSOR_NAME));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sensor_set_hvflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	unsigned char hReverse = 0;
	unsigned char vReverse = 0;
	ret = sensor_read(sd, 0x304e, &hReverse);
	ret = sensor_read(sd, 0x304f, &vReverse);
	switch(enable) {
		case 0:
			hReverse &= 0xFE;
			vReverse &= 0xFE;
			break;
		case 1:
			hReverse |= 0x01;
			vReverse &= 0xFE;
			break;
		case 2:
			hReverse &= 0xFE;
			vReverse |= 0x01;
			break;
		case 3:
			hReverse |= 0x01;
			vReverse |= 0x01;
			break;
	}
	ret += sensor_write(sd, 0x304e, hReverse);
	ret += sensor_write(sd, 0x304f, vReverse);
	return 0;
}

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	struct tx_isp_initarg *init = arg;
	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	//return 0;
	switch(cmd) {
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
			ret = sensor_set_hvflip(sd, sensor_val->value);
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

static int sensor_remove(struct i2c_client *client)
{
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

MODULE_DESCRIPTION("A low-level driver for Sony imx347 sensor");
MODULE_LICENSE("GPL");
