/*
 * mis2006.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG

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

#define MIS2006_CHIP_ID_H	(0x20)
#define MIS2006_CHIP_ID_L	(0x06)
#define MIS2006_REG_END		0xffff
#define MIS2006_REG_DELAY	0xfffe
#define MIS2006_SUPPORT_PCLK (72000000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20180718a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_12BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_30;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

struct regval_list {
	unsigned int reg_num;
	unsigned int value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut mis2006_again_lut[] = {
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
      //{0x7f, 260651},
};

struct tx_isp_sensor_attribute mis2006_attr;

unsigned int mis2006_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = mis2006_again_lut;

	while(lut->gain <= mis2006_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = 0;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == mis2006_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int mis2006_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus mis2006_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 800,
	.lans = 2,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW12,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
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
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW12,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};
struct tx_isp_dvp_bus mis2006_dvp={
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking = {
		.vblanking = 0,
		.hblanking = 0,
	},
};

struct tx_isp_sensor_attribute mis2006_attr={
	.name = "mis2006",
	.chip_id = 0x2006,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.clk = 800,
		.lans = 2,
	},
	.max_again = 259138,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1125 - 1,
	.integration_time_limit = 1125 - 1,
	.total_width = 2133,
	.total_height = 1125,
	.max_integration_time = 1125 - 1,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = mis2006_alloc_again,
	.sensor_ctrl.alloc_dgain = mis2006_alloc_dgain,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};


static struct regval_list mis2006_init_regs_1920_1080_25fps_mipi[] = {
	{0x300b, 0x01},
	{0x3012, 0x2c},/*0x2c*/
	{0x3c00, 0x09},
	{0x3c01, 0x00},
	{0x3c03, 0xa0},
	{0x3c05, 0xa0},
	{0x3c40, 0x8c},
	{0x3c42, 0x03},
	{0x3c0e, 0xf7},
	{0x3c0f, 0xa4},
	{0x3c24, 0x80},
	{0x3c25, 0x07},
	{0x3c26, 0x38},
	{0x3c27, 0x04},
	{0x3c20, 0x2c},
	{0x3f05, 0x00},
	{0x3f01, 0xf0},
	{0x3f02, 0x30},
	{0x3f03, 0x10},
	{0x3006, 0x02},
	{0x300a, 0x00},
	{0x330e, 0x00},
	{0x3900, 0x00},
	{0x3904, 0x17},
	{0x3903, 0x00},
	{0x3906, 0x8a},
	{0x3905, 0x18},
	{0x3908, 0xff},
	{0x3907, 0x1f},
	{0x390a, 0x00},
	{0x3909, 0x00},
	{0x390c, 0x18},
	{0x390b, 0x06},
	{0x390e, 0x22},
	{0x390d, 0x01},
	{0x3910, 0xa0},
	{0x390f, 0x18},
	{0x3912, 0xff},
	{0x3911, 0x1f},
	{0x3919, 0x9d},
	{0x3918, 0x02},
	{0x391b, 0x7b},
	{0x391a, 0x03},
	{0x3915, 0x18},
	{0x3914, 0x06},
	{0x3917, 0x89},
	{0x3916, 0x12},
	{0x391d, 0x00},
	{0x391c, 0x00},
	{0x391f, 0xa0},
	{0x391e, 0x18},
	{0x3921, 0xff},
	{0x3920, 0x1f},
	{0x3923, 0xff},
	{0x3922, 0x1f},
	{0x3925, 0xc9},
	{0x3924, 0x00},
	{0x3927, 0x66},
	{0x3926, 0x05},
	{0x392a, 0x2d},
	{0x3929, 0x00},
	{0x392c, 0x17},
	{0x392b, 0x00},
	{0x392e, 0x17},
	{0x392d, 0x00},
	{0x3930, 0x23},
	{0x392f, 0x06},
	{0x3932, 0x17},
	{0x3931, 0x00},
	{0x3934, 0xdf},
	{0x3933, 0x00},
	{0x3937, 0xff},
	{0x3936, 0x1f},
	{0x3939, 0xff},
	{0x3938, 0x1f},
	{0x393b, 0xff},
	{0x393a, 0x1f},
	{0x393d, 0xff},
	{0x393c, 0x1f},
	{0x393f, 0x57},
	{0x393e, 0x00},
	{0x3941, 0x69},
	{0x3940, 0x00},
	{0x3943, 0x03},
	{0x3942, 0x01},
	{0x3945, 0x0f},
	{0x3944, 0x02},
	{0x3947, 0xbe},
	{0x3946, 0x01},
	{0x3949, 0xbf},
	{0x3948, 0x16},
	{0x394b, 0xe0},
	{0x394a, 0x05},
	{0x394d, 0x0c},
	{0x394c, 0x02},
	{0x394f, 0x17},
	{0x394e, 0x00},
	{0x3951, 0x70},
	{0x3950, 0x00},
	{0x3953, 0x17},
	{0x3952, 0x00},
	{0x3955, 0xb3},
	{0x3954, 0x00},
	{0x3958, 0x43},
	{0x3957, 0x00},
	{0x395a, 0x66},
	{0x3959, 0x18},
	{0x395d, 0xb7},
	{0x395c, 0x18},
	{0x3962, 0xe3},
	{0x3961, 0x18},
	{0x3945, 0x20},
	{0x3944, 0x02},
	{0x3967, 0x38},
	{0x3A04, 0x05},
	{0x3A10, 0x20},
	{0x3A01, 0x01},
	{0x3A0F, 0x01},
	{0x3A10, 0x3F},
	{0x3a16, 0xa0},/*0x00*/
	{0x3013, 0x01},
	{0x3600, 0x27},
	{0x3601, 0x02},
	{0x3800, 0x01},
	{0x3604, 0x07},
	{0x3605, 0x60},
	{0x3610, 0x02},
	{0x3967, 0x38},
	{0x3708, 0x40},
	{0x370a, 0x40},
	{0x370c, 0x40},
	{0x370e, 0x40},
	{0x3100, 0x04},
	{0x3101, 0x64},
	{0x3102, 0x00},
        {0x3300, 0x24},
        {0x3301, 0x00},
        {0x3302, 0x02},
        {0x3303, 0x03},
        {0x330b, 0x01},
        {0x330d, 0x00},
        {0x3201, 0x65},
        {0x3200, 0x04},
        {0x3203, 0x00},
        {0x3202, 0x0a},
        {0x3205, 0x08},
        {0x3204, 0x00},
        {0x3207, 0x3f},
        {0x3206, 0x04},
        {0x3209, 0x08},
        {0x3208, 0x00},
        {0x320b, 0x87},
        {0x320a, 0x07},
        {0x3006, 0x00},

	{MIS2006_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list mis2006_init_regs_1920_1080_25fps_dvp[] = {

	{MIS2006_REG_END, 0x00},	/* END MARKER */
};

/*
 * the order of the mis2006_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting mis2006_win_sizes[] = {
	/* 1280*1080 */
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SGRBG12_1X12,//GRBG
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= mis2006_init_regs_1920_1080_25fps_mipi,
	}
};

static enum v4l2_mbus_pixelcode mis2006_mbus_code[] = {
	V4L2_MBUS_FMT_SGRBG12_1X12,
};

/*
 * the part of driver was fixed.
 */

static struct regval_list mis2006_stream_on_dvp[] = {
	{MIS2006_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list mis2006_stream_off_dvp[] = {
	{MIS2006_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list mis2006_stream_on_mipi[] = {
	{MIS2006_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list mis2006_stream_off_mipi[] = {
	{MIS2006_REG_END, 0x00},	/* END MARKER */
};

int mis2006_read(struct tx_isp_subdev *sd, uint16_t reg,
	       unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= buf,
		},
		[1] = {
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= value,
		}
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int mis2006_write(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int mis2006_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != MIS2006_REG_END) {
		if (vals->reg_num == MIS2006_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = mis2006_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}
static int mis2006_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != MIS2006_REG_END) {
		if (vals->reg_num == MIS2006_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = mis2006_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int mis2006_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int mis2006_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = mis2006_read(sd, 0x3000, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != MIS2006_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = mis2006_read(sd, 0x3001, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;

	if (v != MIS2006_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;
	return 0;
}

static int mis2006_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret = mis2006_write(sd,  0x3100, (unsigned char)((value >> 8)& 0xff));
	ret += mis2006_write(sd, 0x3101, (unsigned char)(value & 0xff));
	if (ret < 0)
		return ret;
	return 0;

}

static int mis2006_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret = mis2006_write(sd, 0x3102, (unsigned char)(value));
	if (ret < 0)
		return ret;
	return 0;
}

static int mis2006_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int mis2006_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int mis2006_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = &mis2006_win_sizes[0];
	int ret = 0;

	if(!enable)
		return ISP_SUCCESS;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = mis2006_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;
	return 0;
}

static int mis2006_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = mis2006_write_array(sd, mis2006_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = mis2006_write_array(sd, mis2006_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		pr_debug("mis2006 stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = mis2006_write_array(sd, mis2006_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = mis2006_write_array(sd, mis2006_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		pr_debug("mis2006 stream off\n");
	}
	return ret;
}

static int mis2006_set_fps(struct tx_isp_subdev *sd, int fps)
{

	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int pclk = MIS2006_SUPPORT_PCLK;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)){
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	ret = mis2006_read(sd, 0x3202, &tmp);
	hts = tmp;
	ret += mis2006_read(sd, 0x3203, &tmp);
	if(ret < 0)
		return -1;
	hts = ((hts << 8) + tmp);
	vts = pclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = mis2006_write(sd, 0x3201, (unsigned char)(vts & 0xff));
	ret += mis2006_write(sd, 0x3200, (unsigned char)(vts >> 8));
	if(ret < 0){
		ISP_ERROR("err: mis2006_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 1;
	sensor->video.attr->integration_time_limit = vts - 1;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 1;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int mis2006_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;

	if(value == TX_ISP_SENSOR_FULL_RES_MAX_FPS){
		wsize = &mis2006_win_sizes[0];
	}else if(value == TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS){
		wsize = &mis2006_win_sizes[0];
	}

	if(wsize){
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

static int mis2006_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	return 0;
}

static int mis2006_g_chip_ident(struct tx_isp_subdev *sd,
			      struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"mis2006_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(15);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"mis2006_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(150);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = mis2006_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an mis2006 chip.\n",
		       client->addr, client->adapter->name);
		return ret;
	}
	ISP_ERROR("mis2006 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "mis2006", sizeof("mis2006"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int mis2006_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = mis2006_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = mis2006_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = mis2006_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = mis2006_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = mis2006_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = mis2006_write_array(sd, mis2006_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = mis2006_write_array(sd, mis2006_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = mis2006_write_array(sd, mis2006_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = mis2006_write_array(sd, mis2006_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = mis2006_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
//			ret = mis2006_set_vflip(sd, *(int*)arg);
		break;
	default:
		break;;
	}

	return 0;
}

static int mis2006_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = mis2006_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int mis2006_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	mis2006_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}

static struct tx_isp_subdev_core_ops mis2006_core_ops = {
	.g_chip_ident = mis2006_g_chip_ident,
	.reset = mis2006_reset,
	.init = mis2006_init,
	/*.ioctl = mis2006_ops_ioctl,*/
	.g_register = mis2006_g_register,
	.s_register = mis2006_s_register,
};

static struct tx_isp_subdev_video_ops mis2006_video_ops = {
	.s_stream = mis2006_s_stream,
};

static struct tx_isp_subdev_sensor_ops	mis2006_sensor_ops = {
	.ioctl	= mis2006_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops mis2006_ops = {
	.core = &mis2006_core_ops,
	.video = &mis2006_video_ops,
	.sensor = &mis2006_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "mis2006",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int mis2006_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &mis2006_win_sizes[0];
	int ret;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
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
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;

	mis2006_attr.dbus_type = data_interface;
	if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
		wsize->regs = mis2006_init_regs_1920_1080_25fps_dvp;
		memcpy((void*)(&(mis2006_attr.dvp)),(void*)(&mis2006_dvp),sizeof(mis2006_dvp));
		mis2006_attr.dvp.gpio = sensor_gpio_func;
	} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
		wsize->regs = mis2006_init_regs_1920_1080_25fps_mipi;
		memcpy((void*)(&(mis2006_attr.mipi)),(void*)(&mis2006_mipi),sizeof(mis2006_mipi));
	} else{
		ISP_ERROR("Don't support this Sensor Data Output Interface.\n");
		goto err_set_sensor_data_interface;
	}
	/*
	  convert sensor-gain into isp-gain,
	*/
	mis2006_attr.max_again = 259138;
	mis2006_attr.max_dgain = 0;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &mis2006_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &mis2006_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->mis2006\n");
	return 0;
err_set_sensor_data_interface:
err_set_sensor_gpio:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int mis2006_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);
	return 0;
}

static const struct i2c_device_id mis2006_id[] = {
	{ "mis2006", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mis2006_id);

static struct i2c_driver mis2006_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "mis2006",
	},
	.probe		= mis2006_probe,
	.remove		= mis2006_remove,
	.id_table	= mis2006_id,
};

static __init int init_mis2006(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init mis2006 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&mis2006_driver);
}

static __exit void exit_mis2006(void)
{
	private_i2c_del_driver(&mis2006_driver);
}

module_init(init_mis2006);
module_exit(exit_mis2006);

MODULE_DESCRIPTION("A low-level driver for SOI mis2006 sensors");
MODULE_LICENSE("GPL");
