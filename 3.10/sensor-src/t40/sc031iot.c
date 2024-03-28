// SPDX-License-Identifier: GPL-2.0+
/*
 * sc031iot.c
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

#define SENSOR_NAME "sc031iot"
#define SENSOR_CHIP_ID_H (0x9a)
#define SENSOR_CHIP_ID_L (0x46)
#define SENSOR_REG_END 0xff
#define SENSOR_REG_PAGE 0xf0
#define SENSOR_REG_DELAY 0x04
#define SENSOR_SUPPORT_15FPS_SCLK (13500 * 1000)
#define SENSOR_OUTPUT_MAX_FPS 15
#define SENSOR_OUTPUT_MIN_FPS 5
#define MCLK 12000000
#define SENSOR_VERSION "H20220524b"

static int reset_gpio = -1;
static int pwdn_gpio = -1;
static int isp_nagain;

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut sensor_again_lut[] = {
	{0x0010, 0},
	{0x0011, 5509},
	{0x0012, 11555},
	{0x0013, 16447},
	{0x0014, 21097},
	{0x0015, 25530},
	{0x0016, 30452},
	{0x0017, 34476},
	{0x0018, 38336},
	{0x0019, 42044},
	{0x001a, 46194},
	{0x001b, 49612},
	{0x001c, 52910},
	{0x001d, 56098},
	{0x001e, 59685},
	{0x001f, 62656},
	{0x1010, 63142},/*1.95倍*/
	{0x1011, 68788},
	{0x1012, 74547},
	{0x1013, 79568},
	{0x1014, 84337},
	{0x1015, 88506},
	{0x1016, 93559},
	{0x1017, 97686},
	{0x1018, 101639},
	{0x1019, 105124},
	{0x101a, 109381},
	{0x101b, 112883},
	{0x101c, 115983},
	{0x101d, 119253},
	{0x101e, 122931},
	{0x101f, 125723},
	{0x1110, 128678},
	{0x1111, 134095},
	{0x1112, 140298},
	{0x1113, 145104},
	{0x1114, 149873},
	{0x1115, 154227},
	{0x1116, 159095},
	{0x1117, 163222},
	{0x1118, 167014},
	{0x1119, 170660},
	{0x111a, 174917},
	{0x111b, 178276},
	{0x111c, 181658},
	{0x111d, 184789},
	{0x111e, 188338},
	{0x111f, 191384},
	{0x1310, 194214},
	{0x1311, 199746},
	{0x1312, 205726},
	{0x1313, 210640},
	{0x1314, 215312},
	{0x1315, 219763},
	{0x1316, 224631},
	{0x1317, 228673},
	{0x1318, 232550},
	{0x1319, 236274},
	{0x131a, 240378},
	{0x131b, 243812},
	{0x131c, 247125},
	{0x131d, 250325},
	{0x131e, 253874},
	{0x131f, 256857},
	{0x1710, 259750},
	{0x1711, 265282},
	{0x1712, 271316},
	{0x1713, 276176},
	{0x1714, 280848},
	{0x1715, 285299},
	{0x1716, 290211},
	{0x1717, 294209},
	{0x1718, 298086},
	{0x1719, 301810},
	{0x171a, 305952},
	{0x171b, 309348},
	{0x171c, 312661},
	{0x171d, 315861},
	{0x171e, 319442},
	{0x171f, 322393},
	{0x1f10, 325286},
	{0x1f11, 330789},
	{0x1f12, 336852},
	{0x1f13, 341738},
	{0x1f14, 346384},
	{0x1f15, 350812},
	{0x1f16, 355747},
	{0x1f17, 359766},
	{0x1f18, 363622},
	{0x1f19, 367326},
	{0x1f1a, 371488},
	{0x1f1b, 374902},
	{0x1f1c, 378197},
	{0x1f1d, 381381},
	{0x1f1e, 384978},
};

struct tx_isp_sensor_attribute sensor_attr;

unsigned int sensor_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sensor_again_lut;


	while (lut->gain <= sensor_attr.max_again) {
		if (isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if (isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else {
			if ((lut->gain == sensor_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	isp_nagain = isp_gain;
	return isp_gain;
}

unsigned int sensor_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sensor_mipi={
   .mode = SENSOR_MIPI_OTHER_MODE,
   .clk = 600,
   .lans = 1,
   .settle_time_apative_en = 0,
   .mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
   .mipi_sc.hcrop_diff_en = 0,
   .mipi_sc.mipi_vcomp_en = 0,
   .mipi_sc.mipi_hcomp_en = 0,
   .mipi_sc.line_sync_mode = 0,
   .mipi_sc.work_start_flag = 0,
   .image_twidth = 640,
   .image_theight = 480,
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
};

struct tx_isp_sensor_attribute sensor_attr={
	.name = SENSOR_NAME,
	.chip_id = 0x9a46,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_8BITS,
	.cbus_device = 0x68,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.max_again = 384978,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 1049*2 - 8,
	.integration_time_limit = 1049*2 - 8,
	.total_width = 858,
	.total_height = 1049*2 - 8,
	.max_integration_time = 1049*2 - 8,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sensor_alloc_again,
	.sensor_ctrl.alloc_dgain = sensor_alloc_dgain,
};

static struct regval_list sensor_init_regs_640_480_15fps_mipi[] = {
	{0xf0, 0x31},
	{0x03, 0x01},
	{SENSOR_REG_DELAY, 0x05},
	{0xf0, 0x30},
	{0x01, 0x00},
	{0x02, 0x00},
	{0x22, 0x00},
	{0x19, 0x00},
	{0x31, 0x0a},
	{0x3f, 0x84},
	{0x37, 0x20},
	{0xf0, 0x00},
	{0x53, 0x00},
	{0x70, 0x5c},
	{0x72, 0xc0},
	{0x8b, 0x00},
	{0x8d, 0x01},
	{0x8e, 0x2a},
	{0x9e, 0x10},
	{0xb0, 0xc0},
	{0xc8, 0x10},
	{0xc9, 0x10},
	{0xc6, 0x00},
	{0xe0, 0x40},
	{0xde, 0x80},
	{0xf0, 0x01},
	{0x70, 0x02},
	{0x71, 0x82},
	{0x72, 0x20},
	{0x73, 0x08},
	{0x74, 0xe8},
	{0x75, 0x10},
	{0x76, 0x81},
	{0x77, 0x89},
	{0x78, 0x81},
	{0x79, 0xc1},
	{0xf0, 0x33},
	{0x02, 0x12},
	{0x7c, 0x02},
	{0x7d, 0x0e},
	{0xa2, 0x04},
	{0x5e, 0x06},
	{0x5f, 0x0a},
	{0x0b, 0x58},
	{0x06, 0x38},
	{0xf0, 0x32},
	{0x0e, 0x04},
	{0x0f, 0x19},
	{0x48, 0x02},
	{0xf0, 0x39},
	{0x02, 0x70},
	{0xf0, 0x45},
	{0x09, 0x1c},
	{0xf0, 0x37},
	{0x22, 0x0d},
	{0xf0, 0x33},
	{0x33, 0x10},
	{0xb1, 0x80},
	{0x34, 0x40},
	{0x0b, 0x54},
	{0xb2, 0x78},
	{0xf0, 0x36},
	{0x11, 0x80},
	{0xe9, 0x14},
	{0xea, 0x05},
	{0xeb, 0x05},
	{0xec, 0x24},
	{0xed, 0x38},
	/*mipi*/
	{0xf0, 0x48},
	{0x00, 0x44},
	{0x19, 0x04},
	{0x1B, 0x03},
	{0x1D, 0x48},/*clk zero*/
	{0x1F, 0x42},/*clk prepare*/
	{0x21, 0x07},
	{0x23, 0x02},
	{0x25, 0x02},
	{0x27, 0x02},
	{0x29, 0x03},
	{0xf4, 0x0a},
	{0xf5, 0xff},
	{0xf0, 0x30},
	{0x38, 0x44},
	{0xf0, 0x00},
	{0x8b, 0x00},
	{0x70, 0x4c},
	{0x9e, 0x10},
	{0xf0, 0x33},
	{0xb3, 0x51},
	{0x01, 0x10},
	{0x0b, 0x6c},
	{0x06, 0x24},
	{0xf0, 0x36},
	{0x31, 0x82},
	{0x3e, 0x60},
	{0x30, 0xf0},
	{0x33, 0x33},
	{0xf0, 0x34},
	{0x9f, 0x02},
	{0xa6, 0x40},
	{0xa7, 0x47},
	{0xe8, 0x5f},
	{0xa8, 0x51},
	{0xa9, 0x44},
	{0xe9, 0x36},
	{0xf0, 0x33},
	{0xb3, 0x51},
	{0x64, 0x17},
	{0x90, 0x01},
	{0x91, 0x03},
	{0x92, 0x07},
	{0x01, 0x10},
	{0x93, 0x10},
	{0x94, 0x10},
	{0x95, 0x10},
	{0x96, 0x01},
	{0x97, 0x07},
	{0x98, 0x1f},
	{0x99, 0x10},
	{0x9a, 0x20},
	{0x9b, 0x28},
	{0x9c, 0x28},
	{0xf0, 0x36},
	{0x70, 0x54},
	{0xb6, 0x40},
	{0xb7, 0x41},
	{0xb8, 0x43},
	{0xb9, 0x47},
	{0xba, 0x4f},
	{0xb0, 0x8b},
	{0xb1, 0x8b},
	{0xb2, 0x8b},
	{0xb3, 0x9b},
	{0xb4, 0xb8},
	{0xb5, 0xf0},
	{0x7e, 0x41},
	{0x7f, 0x47},
	{0x77, 0x80},
	{0x78, 0x84},
	{0x79, 0x8a},
	{0xa0, 0x47},
	{0xa1, 0x5f},
	{0x96, 0x43},
	{0x97, 0x44},
	{0x98, 0x54},
	{0xf0, 0x3f},
	{0x03, 0x9c},
	{0xf0, 0x00},
	{0xe0, 0x04},
	{0xf0, 0x01},
	{0x73, 0x00},
	{0x74, 0xe0},
	{0x70, 0x00},
	{0x71, 0x80},
	{0xf0, 0x36},
	{0x37, 0x74},
	{0xf0, 0x37},
	{0x24, 0x21},
	{0xf0, 0x36},
	{0x41, 0x60},
	{0xea, 0x09},
	{0xeb, 0x05},
	{0xec, 0x24},
	{0xed, 0x28},
	{0xe9, 0x24},

	{SENSOR_REG_DELAY, 0x50},
	{SENSOR_REG_END, 0x00},
};


static struct tx_isp_sensor_win_setting sensor_win_sizes[] = {
	/* 640*480 */
	{
		.width = 640,
		.height = 480,
		.fps = 15 << 16 | 1,
		.mbus_code = TISP_VI_FMT_SBGGR10_1X10,
		.colorspace = TISP_COLORSPACE_SRGB,
		.regs = sensor_init_regs_640_480_15fps_mipi,
	}
};
struct tx_isp_sensor_win_setting *wsize = &sensor_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sensor_stream_on_mipi[] = {
	{0xf0, 0x31},
	{0x00, 0x01},
	{SENSOR_REG_END, 0x00},
};

static struct regval_list sensor_stream_off_mipi[] = {
	{0xf0, 0x31},
	{0x00, 0x00},
	{SENSOR_REG_END, 0x00},
};

int sensor_read(struct tx_isp_subdev *sd, unsigned char reg,
		unsigned char *value)
{
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
		 unsigned char value)
{
	int ret;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg, value};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
		.buf = buf,
	};
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
			private_msleep(vals->value);
		} else {
			ret = sensor_read(sd, vals->reg_num, &val);
			printk("read:{0x%x,0x%x}\n", vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
			if (vals->reg_num == SENSOR_REG_PAGE) {
				ret = sensor_write(sd, vals->reg_num, val);
				ret = sensor_read(sd, vals->reg_num, &val);
			}
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
	int ret;
	unsigned char v;

	ret = sensor_read(sd, 0xf7, &v);
	ISP_WARNING("\n-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sensor_read(sd, 0xf8, &v);
	ISP_WARNING("\n-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SENSOR_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sensor_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	ret = sensor_write(sd, 0xf0, 0x00);
	ret += sensor_write(sd, 0x8d, (unsigned char)((it >> 8) & 0xff));
	ret += sensor_write(sd, 0x8e, (unsigned char)((it & 0xff)));


	/* sensor analog coarse gain */
	ret += sensor_write(sd, 0x70, (unsigned char)(again >> 12) | 0x10);
	ret += sensor_write(sd, 0x8b, (unsigned char)(((again >> 8) & 0x0f)));
	/* sensor dig fine gain */
	ret += sensor_write(sd, 0x9e, (unsigned char)(again & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int sensor_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

//	printk("---------[%s]:%d--------------\n",__func__,__LINE__);
	value *= 1;
	ret = sensor_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0x0f));
	ret += sensor_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sensor_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));
	if (ret < 0)
		return ret;

	return 0;
}

static int sensor_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

//	printk("---------[%s]:%d--------------\n",__func__,__LINE__);
	ret += sensor_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sensor_write(sd, 0x3e08, (unsigned char)((value & 0xff00) >> 8));
	if (ret < 0)
		return ret;

	return 0;
}
#endif

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
	sensor->video.state = TX_ISP_MODULE_DEINIT;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sensor_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
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
			ret = sensor_write_array(sd, sensor_stream_on_mipi);
			ISP_WARNING("%s stream on\n", SENSOR_NAME);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
	}
	else {
		ret = sensor_write_array(sd, sensor_stream_off_mipi);
		ISP_WARNING("%s stream off\n", SENSOR_NAME);
		sensor->video.state = TX_ISP_MODULE_DEINIT;
	}

	return ret;
}

static int sensor_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if (newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) not in range\n", fps);
		return -1;
	}
	sclk = SENSOR_SUPPORT_15FPS_SCLK;

	ret = sensor_write(sd, 0xf0, 0x32);
	ret += sensor_read(sd, 0x0c, &val);
	hts = val << 8;
	ret += sensor_read(sd, 0x0d, &val);
	hts = (hts | val);
	if (0 != ret) {
		ISP_ERROR("Error: %s read error\n", SENSOR_NAME);
		return -1;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += sensor_write(sd, 0xf0, 0x32);
	ret += sensor_write(sd, 0x0f, (unsigned char)(vts & 0xff));
	ret += sensor_write(sd, 0x0e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("Error: %s write error\n", SENSOR_NAME);
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts*2 - 8;
	sensor->video.attr->integration_time_limit = vts*2 - 8;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts*2 - 8;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
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

struct clk *sclka;
static int sensor_attr_check(struct tx_isp_subdev *sd)
{
//	uint8_t i, ret;
	unsigned long rate;
//	struct clk *tclk;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);

	memcpy(&(sensor_attr.mipi), &sensor_mipi, sizeof(sensor_mipi));
	switch(info->default_boot) {
	case 0:
		wsize = &sensor_win_sizes[0];
		sensor_attr.again = 0x032;
		sensor_attr.integration_time = 0x012a;
		break;
	default:
		ISP_ERROR("Have no this setting!!!\n");
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

	private_clk_set_rate(sensor->mclk, MCLK);
	private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	printk("\n============================> reset_gpio =%d pwdn_gpio=%d\n",reset_gpio, pwdn_gpio);
	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;

	return 0;

err_get_mclk:
	return -1;
}

static int sensor_g_chip_ident(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	sensor_attr_check(sd);
	if (reset_gpio != -1) {
		ret = private_gpio_request(reset_gpio,"sensor_reset");
		if (!ret) {
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
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
	case TX_ISP_EVENT_SENSOR_EXPO:
		if (arg)
			ret = sensor_set_expo(sd, sensor_val->value);
		break;
#if 0
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
#endif
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
	sensor->dev = &client->dev;
	sensor_attr.expo_fs = 1;
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
