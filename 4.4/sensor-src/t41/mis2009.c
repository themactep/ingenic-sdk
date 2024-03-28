// SPDX-License-Identifier: GPL-2.0+
/*
 * mis2009.c
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * Settings:
 * sboot        resolution      fps     interface     lane   mode
 *   0          1920*1080       30        mipi        2      linear
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

#define SENSOR_NAME "mis2009"
#define SENSOR_CHIP_ID_H (0x20)
#define SENSOR_CHIP_ID_L (0x08)
#define SENSOR_REG_END 0xffff
#define SENSOR_REG_DELAY 0xfffe
#define SENSOR_SUPPORT_RES_400_PCLK (72000000)
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION "H20230704a"

static int reset_gpio = GPIO_PA(18);
static int pwdn_gpio = GPIO_PA(19);
static int shvflip = 1;
static unsigned char sv_state = 0;
static bool trig_logic = false;
static unsigned int vic_reset = 0;

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
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

struct tx_isp_mipi_bus sensor_mipi = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 800,
	.lans = 2,
	.settle_time_apative_en = 0,
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
#if 0
struct tx_isp_dvp_bus sensor_dvp={
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking = {
		.vblanking = 0,
		.hblanking = 0,
	},
};
#endif
struct tx_isp_sensor_attribute sensor_attr = {
	.name = SENSOR_NAME,
	.chip_id = 0x2008,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_8BITS,
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
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_1920_1080_30fps[] = {
	{0x300a, 0x01},
	{0x3006, 0x02},
	{0x3637, 0x1e},
	{0x3c40, 0x8d},
	{0x3c01, 0x10},
	{0x3c0e, 0xf7},
	{0x3c0f, 0x34},
	{0x3b01, 0x3f},
	{0x3b03, 0x3f},
	{0x3902, 0x01},
	{0x3904, 0x00},
	{0x3903, 0x00},
	{0x3906, 0x1e},
	{0x3905, 0x00},
	{0x3908, 0xb0},
	{0x3907, 0x10},
	{0x390a, 0xff},
	{0x3909, 0x1f},
	{0x390c, 0x83},
	{0x390b, 0x03},
	{0x390e, 0x77},
	{0x390d, 0x00},
	{0x3910, 0xb0},
	{0x390f, 0x10},
	{0x3912, 0xff},
	{0x3911, 0x1f},
	{0x3919, 0x00},
	{0x3918, 0x00},
	{0x391b, 0xfd},
	{0x391a, 0x00},
	{0x3983, 0x5a},
	{0x3982, 0x00},
	{0x3985, 0x0f},
	{0x3984, 0x00},
	{0x391d, 0x00},
	{0x391c, 0x00},
	{0x391f, 0xa4},
	{0x391e, 0x10},
	{0x3921, 0xff},
	{0x3920, 0x1f},
	{0x3923, 0xff},
	{0x3922, 0x1f},
	{0x3932, 0x00},
	{0x3931, 0x00},
	{0x3934, 0xd0},
	{0x3933, 0x00},
	{0x393f, 0x61},
	{0x393e, 0x00},
	{0x3941, 0x89},
	{0x3940, 0x00},
	{0x3943, 0x16},
	{0x3942, 0x01},
	{0x3945, 0x10},
	{0x3944, 0x03},
	{0x3925, 0x95},
	{0x3924, 0x00},
	{0x3927, 0x3d},
	{0x3926, 0x03},
	{0x3947, 0xee},
	{0x3946, 0x00},
	{0x3949, 0x9e},
	{0x3948, 0x0f},
	{0x394b, 0x9e},
	{0x394a, 0x03},
	{0x394d, 0x9c},
	{0x394c, 0x00},
	{0x3913, 0x01},
	{0x3915, 0x0f},
	{0x3914, 0x00},
	{0x3917, 0xc3},
	{0x3916, 0x03},
	{0x392a, 0x1e},
	{0x3929, 0x00},
	{0x392c, 0x0f},
	{0x392b, 0x00},
	{0x392e, 0x0f},
	{0x392d, 0x00},
	{0x3930, 0xca},
	{0x392f, 0x03},
	{0x397f, 0x00},
	{0x397e, 0x00},
	{0x3981, 0x77},
	{0x3980, 0x00},
	{0x395d, 0xbe},
	{0x395c, 0x10},
	{0x3962, 0xdc},
	{0x3961, 0x10},
	{0x3977, 0x22},
	{0x3976, 0x00},
	{0x396d, 0x10},
	{0x396c, 0x03},
	{0x396f, 0x10},
	{0x396e, 0x03},
	{0x3971, 0x10},
	{0x3970, 0x03},
	{0x3973, 0x10},
	{0x3972, 0x03},
	{0x3978, 0x00},
	{0x3979, 0x04},

	{0x3012, 0x01},
	{0x3600, 0x13},
	{0x3601, 0x02},
	{0x360f, 0x00},
	{0x360e, 0x00},
	{0x3610, 0x02},//修改高温高增益blc跟随快慢
	{0x3707, 0x00},
	{0x3708, 0x40},
	{0x3709, 0x00},
	{0x370a, 0x40},
	{0x370b, 0x00},
	{0x370c, 0x40},
	{0x370d, 0x00},
	{0x370e, 0x40},
	{0x3800, 0x01},
	{0x3a02, 0x0b},
	{0x3a03, 0x1b},
	{0x3a08, 0x34},
	{0x3a1b, 0x54},
	{0x3a1e, 0x00},
	{0x3100, 0x04},
	{0x3101, 0x64},
	{0x3a1c, 0x1f},
	{0x3a0C, 0x04},
	{0x3a0D, 0x12},
	{0x3a0E, 0x15},
	{0x3a0F, 0x18},
	{0x3a10, 0x20},
	{0x3a11, 0x3C},
	//MCLK=24Mhz,PCLK=72Mhz
	{0x3300, 0x24},
	{0x3301, 0x00},
	{0x3302, 0x02},
	{0x3303, 0x04},//0x06
	{0x330d, 0x00},
	{0x330b, 0x01},
	{0x330f, 0x07},
	//Windows（2560*1125）
	{0x3201, 0x65},
	{0x3200, 0x04},
	{0x3203, 0x55},
	{0x3202, 0x08},
	{0x3205, 0x04},
	{0x3204, 0x00},
	{0x3207, 0x3f},
	{0x3206, 0x04},
	{0x3209, 0x09},
	{0x3208, 0x00},
	{0x320b, 0x88},
	{0x320a, 0x07},
	{0x3006, 0x00},

	{SENSOR_REG_END, 0x00},
};

/*
 * the order of the sensor_win_sizes is [full_resolution, preview_resolution]. */
static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 1920*1080*/
	{
		.width = 1920,
		.height = 1080,
		.fps = 25 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SGRBG12_1X12,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_1920_1080_30fps,
	}
};

static struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

static struct regval_list sensor_stream_on[] = {
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off[] = {
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value) {
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

int sensor_write(struct tx_isp_subdev *sd, uint16_t reg,
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
	ret = sensor_read(sd, 0x3000, &v);
	printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0x3001, &v);
	printk("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret, v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;
	return 0;
}

#if 0
static int sensor_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned char tmp;
	int expo = (value &0xffff);
	int again = (value &0xffff0000) >> 16;

	ret = sensor_write(sd,  0x3100, (unsigned char)((expo >> 8)& 0xff));
	ret += sensor_write(sd, 0x3101, (unsigned char)(expo & 0xff));
	ret += sensor_write(sd, 0x3102, (unsigned char)(again));
    if ((expo > 200) && (expo < 600))
	    ret += sensor_write(sd, 0x4007, 0x78);
    else
	    ret += sensor_write(sd, 0x4007, 0xc4);

	ret += sensor_read(sd, 0x3100, &tmp);
    if (tmp < 1)
		sensor_write(sd, 0x401d, 0xa0);
    else
		sensor_write(sd, 0x401d, 0xa7);

	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value) {
	int ret = 0;

	ret = sensor_write(sd, 0x3100, (unsigned char) ((value >> 8) & 0xff));
	ret += sensor_write(sd, 0x3101, (unsigned char) (value & 0xff));

	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value) {
	int ret = 0;
	unsigned char tmp;
	ret = sensor_write(sd, 0x3102, (unsigned char) (value));
	if (ret < 0)
		return ret;
	ret = sensor_read(sd, 0x3100, &tmp);
	if (tmp < 1)
		sensor_write(sd, 0x3a02, 0x0b);
	else
		sensor_write(sd, 0x3a02, 0x0a);

	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_logic(struct tx_isp_subdev *sd, int value) {
	int ret = -1;
	unsigned char flip = 0x0;
	unsigned char h_start = 0;
	unsigned char h_end = 0;

	if (trig_logic == true) {
		ret = sensor_read(sd, 0x3007, &flip);
		if (1 == sv_state) {
			flip |= 0x02;
			h_start = 0xfd;
			h_end = 0x9e;
		} else {
			flip &= 0xfd;
			h_start = 0xfc;
			h_end = 0x9d;
		}
		ret += sensor_write(sd, 0x3007, flip);
		ret += sensor_write(sd, 0x3111, h_start);
		ret += sensor_write(sd, 0x3113, h_end);
		if (0 != ret)
			ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		trig_logic = false;
	}

	if ((vic_reset % 20) == 0)
		*((u32 *) 0xb3380000) = 0x5;
	vic_reset++;

	return ret;
}

static int sensor_set_digital_gain(struct tx_isp_subdev *sd, int value) {
	return 0;
}

static int sensor_get_black_pedestal(struct tx_isp_subdev *sd, int value) {
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

static int sensor_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
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

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init) {
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

			ret = sensor_write_array(sd, sensor_stream_on);
			ISP_WARNING("%s stream on\n", SENSOR_NAME);
		}
	} else {
		ret = sensor_write_array(sd, sensor_stream_off);
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
	}

	return ret;
}

/*
mis2009修改帧率：
    原厂提供直接修改VTS会出现断流情况，需要通过修改HTS实现实时修改帧率
*/
static int sensor_set_fps(struct tx_isp_subdev *sd, int fps) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int pclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int sensor_max_fps;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8

	switch (sensor->info.default_boot) {
		case 0:
			pclk = SENSOR_SUPPORT_RES_400_PCLK;
			sensor_max_fps = TX_SENSOR_MAX_FPS_30;
			break;
		default:
			ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (sensor_max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}

	ret = sensor_read(sd, 0x3202, &tmp);
	hts = tmp;
	ret += sensor_read(sd, 0x3203, &tmp);
	if (ret < 0)
		return -1;
	hts = ((hts << 8) + tmp);
	vts = pclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = sensor_write(sd, 0x3201, (unsigned char) (vts & 0xff));
	ret += sensor_write(sd, 0x3200, (unsigned char) (vts >> 8));
	if (ret < 0) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 1;
	sensor->video.attr->integration_time_limit = vts - 1;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 1;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	if (ret) {
		ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
	}

	return ret;
}


static int sensor_set_vflip(struct tx_isp_subdev *sd, int enable) {
	int ret = 0;
	//uint8_t val;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	//val = sensor_read(sd, 0x3007, &val);
	switch (enable) {
		case 0:
			sensor_write(sd, 0x3007, 0x00);
			sensor_write(sd, 0x3115, 0x10);
			sensor_write(sd, 0x3117, 0x0f);
			sensor_write(sd, 0x3111, 0xfc);
			sensor_write(sd, 0x3113, 0x9d);
			break;
		case 1:
			sensor_write(sd, 0x3007, 0x01);
			sensor_write(sd, 0x3115, 0x0f);
			sensor_write(sd, 0x3117, 0x0e);
			sensor_write(sd, 0x3111, 0xfc);
			sensor_write(sd, 0x3113, 0x9d);
			break;
		case 2:
			sensor_write(sd, 0x3007, 0x02);
			sensor_write(sd, 0x3115, 0x10);
			sensor_write(sd, 0x3117, 0x0f);
			sensor_write(sd, 0x3111, 0xfd);
			sensor_write(sd, 0x3113, 0x9e);
			break;
		case 3:
			sensor_write(sd, 0x3007, 0x03);
			sensor_write(sd, 0x3115, 0x0f);
			sensor_write(sd, 0x3117, 0x0e);
			sensor_write(sd, 0x3111, 0xfd);
			sensor_write(sd, 0x3113, 0x9e);
			break;
	}

	if (!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	*((u32 *) 0xb3380000) = 0x5;
	return ret;
}

static int sensor_set_mode(struct tx_isp_subdev *sd, int value) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if (wsize) {
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int sensor_attr_check(struct tx_isp_subdev *sd) {
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	unsigned long rate;
	int ret = 0;

	switch (info->default_boot) {
		case 0:
			wsize = &sensor_win_sizes[0];
			memcpy(&sensor_attr.mipi, &sensor_mipi, sizeof(sensor_mipi));
			sensor_attr.mipi.clk = 800;
			sensor_attr.min_integration_time = 2;
			sensor_attr.total_width = 2133,
			sensor_attr.total_height = 1125,
			sensor_attr.max_integration_time_native = 1125 - 1,
			sensor_attr.max_integration_time = 1125 - 1,
			sensor_attr.integration_time_limit = 1125 - 1,
			sensor_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
			sensor_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
				printk("==========> linear is ok");
			break;
		default:
			ISP_ERROR("Have no this setting!!!\n");
	}

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

	switch (info->default_boot) {
		case 0:
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
			private_clk_set_rate(sensor->mclk, 24000000);
			private_clk_prepare_enable(sensor->mclk);
			break;
	}

	ISP_WARNING("\n====>[default_boot=%d] [resolution=%dx%d] [video_interface=%d] [MCLK=%d] \n", info->default_boot,
		    wsize->width, wsize->height, info->video_interface, info->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;
	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;

	return 0;
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
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(15);
		} else {
			ISP_ERROR("gpio requrest fail %d\n", reset_gpio);
		}
	}
	if (pwdn_gpio != -1) {
		ret = private_gpio_request(pwdn_gpio, "sensor_pwdn");
		if (!ret) {
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(150);
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

static int sensor_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg) {
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if (IS_ERR_OR_NULL(sd)) {
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch (cmd) {
		case TX_ISP_EVENT_SENSOR_EXPO:
			//if (arg)
			//	ret = sensor_set_expo(sd, sensor_val->value);
			break;
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
				ret = sensor_set_logic(sd, sensor_val->value);
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

	sensor = (struct tx_isp_sensor *) kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0, sizeof(*sensor));

	sensor->dev = &client->dev;
	sd = &sensor->sd;
	video = &sensor->video;
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
	//private_clk_put(sensor->mclk);
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
