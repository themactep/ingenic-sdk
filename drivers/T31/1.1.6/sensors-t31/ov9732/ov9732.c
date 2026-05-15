/*
 * ov9732.c
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
#include <tx-isp-debug.h>

#define OV9732_CHIP_ID_H	(0x97)
#define OV9732_CHIP_ID_L	(0x32)

#define OV9732_REG_END		0xffff
#define OV9732_REG_DELAY	0xfffe

#define OV9732_SUPPORT_SCLK  (36*1000*1000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_1
#define SENSOR_VERSION	"H20200824a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_LOW_10BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor data interface GPIO function");

struct tx_isp_sensor_attribute ov9732_attr;

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	unsigned int value;
	unsigned int gain;
};
struct again_lut ov9732_again_lut[] = {
	{0x10, 0},
	{0x11, 5731},
	{0x12, 11136},
	{0x13, 16248},
	{0x14, 21097},
	{0x15, 25710},
	{0x16, 30109},
	{0x17, 34312},
	{0x18, 38336},
	{0x19, 42195},
	{0x1a, 45904},
	{0x1b, 49472},
	{0x1c, 52910},
	{0x1d, 56228},
	{0x1e, 59433},
	{0x1f, 62534},
	{0x20, 65536},
	{0x22, 71267},
	{0x24, 76672},
	{0x26, 81784},
	{0x28, 86633},
	{0x2a, 91246},
	{0x2c, 95645},
	{0x2e, 99848},
	{0x30, 103872},
	{0x32, 107731},
	{0x34, 111440},
	{0x36, 115008},
	{0x38, 118446},
	{0x3a, 121764},
	{0x3c, 124969},
	{0x3e, 128070},
	{0x40, 131072},
	{0x44, 136803},
	{0x48, 142208},
	{0x4c, 147320},
	{0x50, 152169},
	{0x54, 156782},
	{0x58, 161181},
	{0x5c, 165384},
	{0x60, 169408},
	{0x64, 173267},
	{0x68, 176976},
	{0x6c, 180544},
	{0x70, 183982},
	{0x74, 187300},
	{0x78, 190505},
	{0x7c, 193606},
	{0x80, 196608},
	{0x88, 202339},
	{0x90, 207744},
	{0x98, 212856},
	{0xa0, 217705},
	{0xa8, 222318},
	{0xb0, 226717},
	{0xb8, 230920},
	{0xc0, 234944},
	{0xc8, 238803},
	{0xd0, 242512},
	{0xd8, 246080},
	{0xe0, 249518},
	{0xe8, 252836},
	{0xf0, 256041},
	{0xf8, 259142},
};

struct tx_isp_sensor_attribute ov9732_attr;

unsigned int ov9732_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = ov9732_again_lut;

	while(lut->gain <= ov9732_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut->value;
			return 0;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == ov9732_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int ov9732_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus ov9732_mipi = {
	.clk = 400,
	.lans = 1,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.image_twidth = 1280,
	.image_theight = 720,
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
};

struct tx_isp_dvp_bus ov9732_dvp={
	.mode = SENSOR_DVP_HREF_MODE,
	.blanking = {
		.vblanking = 0,
		.hblanking = 0,
	},
};

struct tx_isp_sensor_attribute ov9732_attr={
	.name = "ov9732",
	.chip_id = 0x9732,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x36,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 400,
		.lans = 1,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.image_twidth = 1280,
		.image_theight = 720,
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
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 974 - 4,
	.integration_time_limit = 974 - 4,
	.total_width = 1478,
	.total_height = 974,
	.max_integration_time = 974 - 4,
	.one_line_expr_in_us = 41,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = ov9732_alloc_again,
	.sensor_ctrl.alloc_dgain = ov9732_alloc_dgain,
	//	void priv; /* point to struct tx_isp_sensor_board_info */
};

static struct regval_list ov9732_init_regs_1280_720_30fps_mipi[] = {
	/*
	  @@ mipi interface 1280*720 25fps
	*/
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x0100, 0x00},
	{0x3007, 0x1f},
	{0x3008, 0xff},
	{0x3014, 0x22},
	{0x3081, 0x3c},
	{0x3084, 0x00},
	{0x3600, 0xf6},
	{0x3601, 0x72},
	{0x3610, 0x0c},
	{0x3611, 0xf0},
	{0x3612, 0x35},
	{0x3658, 0x22},
	{0x3659, 0x22},
	{0x365a, 0x02},
	{0x3700, 0x1f},
	{0x3701, 0x10},
	{0x3702, 0x0c},
	{0x3703, 0x07},
	{0x3704, 0x3c},
	{0x3705, 0x81},
	{0x3710, 0x0c},
	{0x3782, 0x58},
	{0x380f, 0x03},
	{0x380f, 0xce},/*vts 25fps*/
	{0x3501, 0x31},
	{0x350a, 0x00},
	{0x350b, 0x10},
	{0x3d82, 0x38},
	{0x3d83, 0xa4},
	{0x3d86, 0x1f},
	{0x3d87, 0x03},
	{0x4001, 0xe0},
	{0x4006, 0x01},
	{0x4007, 0x40},
	{0x4800, 0x00},
	{0x4805, 0x00},
    {0x4802, 0x84},
    {0x4819, 0x30},
    {0x4825, 0x32},
    {0x4826, 0x08},
	{0x4827, 0x1d},
	{0x4821, 0x50},
	{0x4823, 0x50},
	{0x4837, 0x37},
	{0x5000, 0x0f},
	{0x5708, 0x06},
	{0x5781, 0x00},
	{0x5783, 0x0f},
	{0x0100, 0x01},
	{0x3703, 0x0b},
	{0x3705, 0x51},
	{OV9732_REG_DELAY, 50},	/* END MARKER */

	{OV9732_REG_END, 0x00},	/* END MARKER */
};
/*
 * the order of the ov9732_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting ov9732_win_sizes[] = {
	/* 1280*720 */
	{
		.width		= 1280,
		.height		= 720,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ov9732_init_regs_1280_720_30fps_mipi,
	}
};
struct tx_isp_sensor_win_setting *wsize = &ov9732_win_sizes[0];

static struct regval_list ov9732_stream_on_dvp[] = {
	{0x0100, 0x01},
	{OV9732_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list ov9732_stream_off_dvp[] = {
	{0x0100, 0x00},
	{OV9732_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov9732_stream_on_mipi[] = {
	{0x0100, 0x01},
	{OV9732_REG_END, 0x00},	/* END MARKER */
};


static struct regval_list ov9732_stream_off_mipi[] = {
	{0x0100, 0x00},
	{OV9732_REG_END, 0x00},	/* END MARKER */
};

int ov9732_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg>>8)&0xff, reg&0xff};
	int ret;
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

	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int ov9732_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg>>8)&0xff, reg&0xff, value};
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

static int ov9732_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != OV9732_REG_END) {
		if (vals->reg_num == OV9732_REG_DELAY) {
				msleep(vals->value);
		} else {
			ret = ov9732_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%02x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}
	return 0;
}
static int ov9732_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != OV9732_REG_END) {
		if (vals->reg_num == OV9732_REG_DELAY) {
				msleep(vals->value);
		} else {
			ret = ov9732_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		/* printk("vals->reg_num:%x, vals->value:%x\n",vals->reg_num, vals->value); */
		vals++;
	}
	return 0;
}

static int ov9732_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int ov9732_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;
	ret = ov9732_read(sd, 0x300a, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OV9732_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = ov9732_read(sd, 0x300b, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != OV9732_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int ov9732_set_integration_time(struct tx_isp_subdev *sd, int value)
{

	int ret = 0;
	unsigned int expo = value<<4;

	ret = ov9732_write(sd, 0x3502, (unsigned char)(expo & 0xff));
	ret += ov9732_write(sd, 0x3501, (unsigned char)((expo >> 8) & 0xff));
	ret += ov9732_write(sd, 0x3500, (unsigned char)((expo >> 16) & 0xf));
	if (ret < 0)
		return ret;
	return 0;
}

static int ov9732_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = -1;

	ret = ov9732_write(sd, 0x350b, (unsigned char)(value & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int ov9732_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int ov9732_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int ov9732_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!enable)
		return ISP_SUCCESS;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	ret = ov9732_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int ov9732_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = ov9732_write_array(sd, ov9732_stream_on_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = ov9732_write_array(sd, ov9732_stream_on_mipi);
		}else{
			printk("Don't support this Sensor Data interface\n");
		}
		printk("ov9732_mipi stream on\n");
	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
			ret = ov9732_write_array(sd, ov9732_stream_off_dvp);
		} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = ov9732_write_array(sd, ov9732_stream_off_mipi);
		}else{
			printk("Don't support this Sensor Data interface\n");
		}
		printk("ov9732_mipi stream off\n");
	}
	return ret;
}

static int ov9732_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char val = 0;

	unsigned int newformat = 0; //the format is 24.8
	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || fps < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		/*printk("warn: fps(%d) no in range\n", fps);*/
		return -1;
	}

	sclk = OV9732_SUPPORT_SCLK;
	val = 0;
	ret += ov9732_read(sd, 0x380c, &val);
	hts = val<<8;
	val = 0;
	ret += ov9732_read(sd, 0x380d, &val);
	hts |= val;
	if (0 != ret) {
		printk("err: ov9732 read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += ov9732_write(sd, 0x380f, vts&0xff);
	ret += ov9732_write(sd, 0x380e, (vts>>8)&0xff);
	if (0 != ret) {
		printk("err: ov9732_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 4;
	sensor->video.attr->integration_time_limit = vts - 4;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 4;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int ov9732_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

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

static int ov9732_g_chip_ident(struct tx_isp_subdev *sd,
		struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"ov9732_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(20);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"ov9732_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(150);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = ov9732_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an ov9732 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("ov9732 chip found @ 0x%02x (%s) version %s \n", client->addr, client->adapter->name, SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "ov9732", sizeof("ov9732"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}
static int ov9732_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
		case TX_ISP_EVENT_SENSOR_INT_TIME:
			if(arg)
				ret = ov9732_set_integration_time(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_AGAIN:
			if(arg)
				ret = ov9732_set_analog_gain(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_DGAIN:
			if(arg)
				ret = ov9732_set_digital_gain(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
			if(arg)
				ret = ov9732_get_black_pedestal(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_RESIZE:
			if(arg)
				ret = ov9732_set_mode(sd, *(int*)arg);
			break;
		case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
			if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
				ret = ov9732_write_array(sd, ov9732_stream_off_dvp);
			} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
				ret = ov9732_write_array(sd, ov9732_stream_off_mipi);
			}else{
				printk("Don't support this Sensor Data interface\n");
			}
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
				ret = ov9732_write_array(sd, ov9732_stream_on_dvp);
			} else if(data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
				ret = ov9732_write_array(sd, ov9732_stream_on_mipi);
			}else{
				printk("Don't support this Sensor Data interface\n");
			}
			break;
		case TX_ISP_EVENT_SENSOR_FPS:
			if(arg)
				ret = ov9732_set_fps(sd, *(int*)arg);
			break;
		default:
			break;
	}

	return 0;
}

static int ov9732_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = ov9732_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int ov9732_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov9732_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops ov9732_core_ops = {
	.g_chip_ident = ov9732_g_chip_ident,
	.reset = ov9732_reset,
	.init = ov9732_init,
	.g_register = ov9732_g_register,
	.s_register = ov9732_s_register,
};

static struct tx_isp_subdev_video_ops ov9732_video_ops = {
	.s_stream = ov9732_s_stream,
};

static struct tx_isp_subdev_sensor_ops	ov9732_sensor_ops = {
	.ioctl	= ov9732_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops ov9732_ops = {
	.core = &ov9732_core_ops,
	.video = &ov9732_video_ops,
	.sensor = &ov9732_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "ov9732",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int ov9732_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
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
	clk_set_rate(sensor->mclk, 24000000);
	clk_enable(sensor->mclk);

	ov9732_attr.dbus_type = data_interface;
	if (data_interface == TX_SENSOR_DATA_INTERFACE_DVP){
		ret = set_sensor_gpio_function(sensor_gpio_func);
		if (ret < 0)
			goto err_set_sensor_gpio;
		ov9732_attr.dvp.gpio = sensor_gpio_func;
	} else if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
		wsize->regs = ov9732_init_regs_1280_720_30fps_mipi;
		memcpy((void*)(&(ov9732_attr.mipi)),(void*)(&ov9732_mipi),sizeof(ov9732_mipi));
	}else{
		printk("Don't support this Sensor Data Output Interface.\n");
		goto err_set_sensor_data_interface;
	}

	/*
	  convert sensor-gain into isp-gain,
	*/
	ov9732_attr.max_again = 259142;
	ov9732_attr.max_dgain = 0;

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &ov9732_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &ov9732_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("@@@@@@@probe ok ------->ov9732\n");

	return 0;
err_set_sensor_data_interface:
err_set_sensor_gpio:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int ov9732_remove(struct i2c_client *client)
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

static const struct i2c_device_id ov9732_id[] = {
	{ "ov9732", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov9732_id);

static struct i2c_driver ov9732_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov9732",
	},
	.probe		= ov9732_probe,
	.remove		= ov9732_remove,
	.id_table	= ov9732_id,
};

static __init int init_ov9732(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init ov9732 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&ov9732_driver);
}

static __exit void exit_ov9732(void)
{
	private_i2c_del_driver(&ov9732_driver);
}

module_init(init_ov9732);
module_exit(exit_ov9732);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov9732 sensors");
MODULE_LICENSE("GPL");
