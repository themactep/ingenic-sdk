/*
 * mis2003.c
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
#define MIS2003_CHIP_ID_H	(0x20)
#define MIS2003_CHIP_ID_L	(0x03)
#define MIS2003_REG_END		0xffff
#define MIS2003_REG_DELAY	0xfffe
#define MIS2003_SUPPORT_PCLK (72000*1000)
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define DRIVE_CAPABILITY_1

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int sensor_gpio_func = DVP_PA_HIGH_10BIT;
module_param(sensor_gpio_func, int, S_IRUGO);
MODULE_PARM_DESC(sensor_gpio_func, "Sensor GPIO function");

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

struct again_lut mis2003_again_lut[] = {
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

struct tx_isp_sensor_attribute mis2003_attr;

unsigned int mis2003_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = mis2003_again_lut;
	while(lut->gain <= mis2003_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return lut[0].gain;
		}
		else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		}
		else{
			if((lut->gain == mis2003_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int mis2003_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute mis2003_attr={
	.name = "mis2003",
	.chip_id = 0x2003,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
		.polar = {
			.vsync_polar = 2,
		},
		.frame_ecc = FRAME_ECC_OFF,
	},
	.max_again = 259138,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 4,
	.max_integration_time_native = 1124,
	.integration_time_limit = 1124,
	.total_width = 2560,
	.total_height = 1125,
	.max_integration_time = 1124,
	/*.one_line_expr_in_us = 28,*/
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = mis2003_alloc_again,
	.sensor_ctrl.alloc_dgain = mis2003_alloc_dgain,
};


static struct regval_list mis2003_init_regs_1920_1080_25fps[] = {
	{0x3006, 0x04},
	{0x3700, 0xD9}, // BLC = 仅反馈，全局
	{0x370C, 0x01}, // BLC 稳定 TH = 0x03
	{0x370E, 0x01}, // 连续 (2^0x370E)帧变化 Target值才响应
	{0x3800, 0x00}, // DRNC  ARNC全开
	{0x3B00, 0x01}, // no overflow
	{0x3400, 0x0b}, //下降沿采样
	{0x3D00, 0x01},
	{0x410c, 0x42},
	{0x400E, 0x24},
	{0x4018, 0x18},
	{0x4020, 0x14},
	{0x4026, 0x1E},
	{0x402A, 0x26},
	{0x402C, 0x3C},
	{0x4030, 0x34},
	{0x4034, 0x34},
	{0x4036, 0xE0},  // sharding消除、第一个斜波拉长500ns、保持STA_DA与EN_RAMP间隔200ns
	{0x4111, 0x0f},
	{0x4110, 0x48},
	{0x410E, 0x02}, // 太阳黑子 LM模式
	{0x4104, 0x2D}, //太阳黑子消除 20180112
	{0x4101, 0x01}, // CPN = OFF    20180118 by songbo
	{0x3100, 0x04}, //曝光时间调整寄存器
	{0x3101, 0x64},
	{0x3102, 0x00},  //PGA调整寄存器 1x,2x,4x,8x
	{0x3103, 0x0f},  //ADC 调整寄存器 1x～8(1+31/32)xx
	/*CIS_IN=24Mhz PCLK=72Mhz*/
	{0x3300, 0x48},
	{0X3301, 0X02},
	{0X3302, 0x02},
	{0X3303, 0X04},
	/*1080P @25fps frame size 2560*1125*/
	{0X3200, 0X04},
	{0X3201, 0X65},
	{0X3202, 0X0a},
	{0X3203, 0X00},
	/*1080p window size 1920*1080*/
	{0X3204, 0X00},
	{0X3205, 0X04},
	{0X3206, 0X04},
	{0X3207, 0X3B},
	{0X3208, 0X00},
	{0X3209, 0X04},
	{0X320A, 0X07},
	{0X320B, 0X83},

	{MIS2003_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list mis2003_init_regs_1920_1080_15fps[] = {
	/*not avaliable for just now*/
};
/*
 * the order of the mis2003_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting mis2003_win_sizes[] = {
	/* 1920*1080 */
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SGRBG10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= mis2003_init_regs_1920_1080_25fps,
	}
};

static enum v4l2_mbus_pixelcode mis2003_mbus_code[] = {
	V4L2_MBUS_FMT_SGRBG10_1X10,
	V4L2_MBUS_FMT_SGRBG12_1X12,
};

/*
 * the part of driver was fixed.
 */

static struct regval_list mis2003_stream_on[] = {
	{MIS2003_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list mis2003_stream_off[] = {
	{MIS2003_REG_END, 0x00},	/* END MARKER */
};

int mis2003_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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

int mis2003_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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

static int mis2003_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != MIS2003_REG_END) {
		if (vals->reg_num == MIS2003_REG_DELAY) {
				private_msleep(vals->value);
		} else {
			ret = mis2003_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int mis2003_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != MIS2003_REG_END) {
		if (vals->reg_num == MIS2003_REG_DELAY) {
				private_msleep(vals->value);
		} else {
			ret = mis2003_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}
	return 0;
}

static int mis2003_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int mis2003_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;
	ret = mis2003_read(sd, 0x3000, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != MIS2003_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = mis2003_read(sd, 0x3001, &v);
	pr_debug("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != MIS2003_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;
	return 0;
}

static int mis2003_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret = mis2003_write(sd, 0x3100, (unsigned char)((value >> 8) & 0xff));
	ret += mis2003_write(sd, 0x3101, (unsigned char)(value & 0xff));
	if (ret < 0)
		return ret;
	return 0;
}
static int mis2003_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
        unsigned char temper = 0 ;
        unsigned int Ratio = 0;
        ret = mis2003_read(sd, 0x3d02, &temper);
	ret = mis2003_write(sd, 0x3103, (unsigned char)(value & 0x1f));
	ret += mis2003_write(sd, 0x3102, (unsigned char)((value >> 5) & 0x03));

	if (ret < 0)
		return ret;
#if 1
    if (value < 0x20) {
       if (temper < 0xa0)  {
               mis2003_write(sd,0x4104, 0x2d);
      }
       else {
               mis2003_write(sd,0x4104, 0x27);
      }
}
       else {
             mis2003_write(sd,0x4104, 0x27);
}
#endif
#if 1
      if  (temper < 0xa0 ) {
           mis2003_write(sd,0x370c, 0x01);
           mis2003_write(sd,0x370e, 0x01);
}
     else if (temper >= 0xa0 && temper < 0xb4)
{
           mis2003_write(sd,0x370c, 0x03);
           mis2003_write(sd,0x370e, 0x03);
}
     else if (temper >= 0xb4)  {
           mis2003_write(sd,0x370c, 0x03);
           mis2003_write(sd,0x370e, 0x07);
}
#endif

#if 1
     if(temper <= 0xa0)  {
	Ratio = 0x800;
}

    else {
	Ratio = 0x790;
}
        mis2003_write(sd,0x3701,(unsigned char) (Ratio & 0xff00) >> 8);
        mis2003_write(sd,0x3702,(unsigned char) Ratio & 0xff);
        mis2003_write(sd,0x3703,(unsigned char) (Ratio & 0xff00) >> 8);
        mis2003_write(sd,0x3704,(unsigned char) Ratio & 0xff);
        mis2003_write(sd,0x3705,(unsigned char) (Ratio & 0xff00) >> 8);
        mis2003_write(sd,0x3706,(unsigned char) Ratio & 0xff);
        mis2003_write(sd,0x3707,(unsigned char) (Ratio & 0xff00) >> 8);
        mis2003_write(sd,0x3708,(unsigned char) Ratio & 0xff);
#endif
	return 0;
}

static int mis2003_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int mis2003_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int mis2003_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = &mis2003_win_sizes[0];
	int ret = 0;

	if(!enable)
		return ISP_SUCCESS;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;

	ret = mis2003_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;
	return 0;
}

static int mis2003_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = mis2003_write_array(sd, mis2003_stream_on);
		pr_debug("mis2003 stream on\n");
	}
	else {
		ret = mis2003_write_array(sd, mis2003_stream_off);
		pr_debug("mis2003 stream off\n");
	}
	return ret;
}

static int mis2003_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int pclk = MIS2003_SUPPORT_PCLK;
	unsigned short hts;
	unsigned short vts = 0;
	unsigned char tmp;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	/* the format of fps is 16/16. for example 25 << 16 | 2, the value is 25/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)){
		printk("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	ret = mis2003_read(sd, 0x3200, &tmp);
	hts = tmp;
	ret += mis2003_read(sd, 0x3201, &tmp);
	if(ret < 0)
		return -1;
	hts = ((hts << 8) + tmp);

	vts = pclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret = mis2003_write(sd, 0x3203, (unsigned char)(vts & 0xff));
	ret += mis2003_write(sd, 0x3202, (unsigned char)(vts >> 8));
	if(ret < 0){
		printk("err: mis2003_write err\n");
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

static int mis2003_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_win_setting *wsize = NULL;
	int ret = ISP_SUCCESS;

	if(value == TX_ISP_SENSOR_FULL_RES_MAX_FPS){
		wsize = &mis2003_win_sizes[0];
	}else if(value == TX_ISP_SENSOR_PREVIEW_RES_MAX_FPS){
		wsize = &mis2003_win_sizes[0];
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

static int mis2003_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"mis2003_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}else{
			printk("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"mis2003_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			printk("gpio requrest fail %d\n",pwdn_gpio);
		}
	}

	ret = mis2003_detect(sd, &ident);
	if (ret) {
		printk("chip found @ 0x%x (%s) is not an mis2003 chip.\n",
		       client->addr, client->adapter->name);
		return ret;
	}
	printk("mis2003 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	if(chip){
		memcpy(chip->name, "mis2003", sizeof("mis2003"));
		chip->ident = ident;
	}

	return 0;
}

static int mis2003_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;

	if(IS_ERR_OR_NULL(sd)){
		printk("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = mis2003_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = mis2003_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = mis2003_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = mis2003_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = mis2003_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = mis2003_write_array(sd, mis2003_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = mis2003_write_array(sd, mis2003_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = mis2003_set_fps(sd, *(int*)arg);
		break;
	default:
		break;;
	}

	return 0;
}

static int mis2003_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = mis2003_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int mis2003_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;
	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	mis2003_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}

static struct tx_isp_subdev_core_ops mis2003_core_ops = {
	.g_chip_ident = mis2003_g_chip_ident,
	.reset = mis2003_reset,
	.init = mis2003_init,
	.g_register = mis2003_g_register,
	.s_register = mis2003_s_register,
};

static struct tx_isp_subdev_video_ops mis2003_video_ops = {
	.s_stream = mis2003_s_stream,
};

static struct tx_isp_subdev_sensor_ops	mis2003_sensor_ops = {
	.ioctl	= mis2003_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops mis2003_ops = {
	.core = &mis2003_core_ops,
	.video = &mis2003_video_ops,
	.sensor = &mis2003_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "mis2003",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int mis2003_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;
	struct tx_isp_sensor_win_setting *wsize = &mis2003_win_sizes[0];
	enum v4l2_mbus_pixelcode mbus;
	int i = 0;
	int ret;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		printk("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));
	/* request mclk of sensor */
	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		printk("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);

	ret = set_sensor_gpio_function(sensor_gpio_func);
	if (ret < 0)
		goto err_set_sensor_gpio;

	mis2003_attr.dvp.gpio = sensor_gpio_func;

	switch(sensor_gpio_func){
	case DVP_PA_LOW_10BIT:
	case DVP_PA_HIGH_10BIT:
		mbus = mis2003_mbus_code[0];
		break;
	case DVP_PA_12BIT:
		mbus = mis2003_mbus_code[1];
		break;
	default:
		goto err_set_sensor_gpio;
	}

	for(i = 0; i < ARRAY_SIZE(mis2003_win_sizes); i++)
		mis2003_win_sizes[i].mbus_code = mbus;

	/*
	  convert sensor-gain into isp-gain,
	*/
	mis2003_attr.max_again = 259138;
	mis2003_attr.max_dgain = 0; //mis2003_attr.max_dgain;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.attr = &mis2003_attr;
	sensor->video.mbus_change = 1;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &mis2003_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("@@@@@@@probe ok ------->mis2003\n");

	return 0;
err_set_sensor_gpio:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int mis2003_remove(struct i2c_client *client)
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

static const struct i2c_device_id mis2003_id[] = {
	{ "mis2003", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mis2003_id);

static struct i2c_driver mis2003_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "mis2003",
	},
	.probe		= mis2003_probe,
	.remove		= mis2003_remove,
	.id_table	= mis2003_id,
};

static __init int init_mis2003(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		printk("Failed to init mis2003 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&mis2003_driver);
}

static __exit void exit_mis2003(void)
{
	i2c_del_driver(&mis2003_driver);
}

module_init(init_mis2003);
module_exit(exit_mis2003);

MODULE_DESCRIPTION("A low-level driver for Image Design mis2003 sensors");
MODULE_LICENSE("GPL");
