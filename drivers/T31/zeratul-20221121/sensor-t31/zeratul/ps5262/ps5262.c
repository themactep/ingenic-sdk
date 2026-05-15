/* 
* ps5262.c
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
#include <txx-funcs.h>

#define PS5262_CHIP_ID_H	(0x52)
#define PS5262_CHIP_ID_L	(0x62)
#define ps5262_REG_END		0xffff
#define ps5262_FAST_AE		0xfffd
#define ps5262_REG_DELAY	0xfffe
#define ps5262_SUPPORT_30FPS_SCLK (79200000)
#define SENSOR_OUTPUT_MAX_FPS 30
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20210427a"

//#define SENSOR_WITHOUT_INIT
static int reset_gpio = GPIO_PA(18);
//module_param(reset_gpio, int, S_IRUGO);
//MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
//module_param(pwdn_gpio, int, S_IRUGO);
//MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
//module_param(data_interface, int, S_IRUGO);
//MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_30;
//module_param(sensor_max_fps, int, S_IRUGO);
//MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");

static int shvflip = 0;
//module_param(shvflip, int, S_IRUGO);
//MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");
static int data_type = TX_SENSOR_DATA_TYPE_LINEAR;
extern int riscv_is_pass;
int fast_wdr_mode = 0;

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

unsigned char again0 = 0;
unsigned char expt0 = 0;
unsigned char expt1 = 0;
/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

struct again_lut ps5262_again_lut[] = {
	{0	,0     },
	{1	,5732  },
	{2	,11136 },
	{3	,16248 },
	{4	,21098 },
	{5	,25711 },
	{6	,30109 },
	{7	,34312 },
	{8	,38336 },
	{9	,42196 },
	{10	,45904 },
	{11	,49472 },
	{12	,52911 },
	{13	,56229 },
	{14	,59434 },
	{15	,62534 },
	{16	,65536 },
	{17	,71268 },
	{18	,76672 },
	{19	,81784 },
	{20	,86634 },
	{21	,91247 },
	{22	,95645 },
	{23	,99848 },
	{24	,103872},
	{25	,107732},
	{26	,111440},
	{27	,115008},
	{28	,118447},
	{29	,121765},
	{30	,124970},
	{31	,128070},
	{32	,131072},
	{33	,136804},
	{34	,142208},
	{35	,147320},
	{36	,152170},
	{37	,156783},
	{38	,161181},
	{39	,165384},
	{40	,169408},
	{41	,173268},
	{42	,176976},
	{43	,180544},
	{44	,183983},
	{45	,187301},
	{46	,190506},
	{47	,193606},
	{48	,196608},
	{49	,202340},
	{50	,207744},
	{51	,212856},
	{52	,217706},
	{53	,222319},
	{54	,226717},
	{55	,230920},
	{56	,234944},
	{57	,238804},
	{58	,242512},
	{59	,246080},
	{60	,249519},
	{61	,252837},
	{62	,256042},
	{63	,259142},
	{64	,262144},
	{65	,267876},
	{66	,273280},
	{67	,278392},
	{68	,283242},
	{69	,287855},
	{70	,292253},
	{71	,296456},
	{72	,300480},
	{73	,304340},
	{74	,308048},
	{75	,311616},
	{76	,315055},
	{77	,318373},
	{78	,321578},
	{79	,324678},
	{80	,327680},
};

struct tx_isp_sensor_attribute ps5262_attr;

unsigned int ps5262_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = ps5262_again_lut;

	while(lut->gain <= ps5262_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == ps5262_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}
		lut++;
	}

	return isp_gain;
}

unsigned int ps5262_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus ps5262_mipi={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 510,
	.lans = 2,
	.settle_time_apative_en = 1,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
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
};

struct tx_isp_sensor_attribute ps5262_attr={
	.name = "ps5262",
	.chip_id = 0x5262,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x48,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.dvp = {
		.mode = SENSOR_DVP_HREF_MODE,
		.blanking = {
			.vblanking = 0,
			.hblanking = 0,
		},
		.dvp_hcomp_en = 0,
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 327680,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1200 - 3,
	.integration_time_limit = 1200 - 3,
	.total_width = 2200,
	.total_height = 1200,
	.max_integration_time = 1200 - 3,
	.one_line_expr_in_us = 25,
	.expo_fs = 1,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = ps5262_alloc_again,
	.sensor_ctrl.alloc_dgain = ps5262_alloc_dgain,
};

static struct regval_list ps5262_init_regs_1920_1080_30fps_mipi[] = {
    {0x410B, 0x83},
    {0x0114, 0x09},
    {0x0115, 0x60},
    {0x0162, 0x02},
    {0x4178, 0xB0},
    {0x4179, 0x5A},
    {0x0225, 0x11},
    {0x0242, 0x11},
    {0x0247, 0x26},
    {0x0248, 0x61},
    {0x0249, 0x11},
    {0x4212, 0xC0},
    {0x0233, 0x46},
    {0x0654, 0x01},
    {0x0655, 0xF5},
    {0x0657, 0x19},
    {0x0659, 0x90},
    {0x065B, 0x2B},
    {0x06A3, 0x40},
    {0x06AC, 0x08},
    {0x0906, 0x08},
    {0x0908, 0x1C},
    {0x090F, 0x08},
    {0x0911, 0x1C},
    {0x0919, 0x03},
    {0x091A, 0x03},
    {0x0B0C, 0x00},
    {0x0E0C, 0x04},
    {0x0E0E, 0x38},
    {0x0E10, 0x04},
    {0x0E12, 0x80},
    {0x1415, 0x05},
    {0x1417, 0x03},
    {0x1418, 0x03},
    {0x145B, 0x10},
    {0x14B0, 0x01},
    {0x140F, 0x01},
    {ps5262_FAST_AE, 0x00},
    {0x0111, 0x01},
    {0x010F, 0x01},
	{ps5262_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ps5262_init_regs_1928_1088_30fps_mipi[] = {
#if 1
    {0x410B, 0x83},
    {0x0114, 0x08},
    {0x0115, 0x98},
    {0x0116, 0x04},
    {0x0117, 0xAF},
    {0x0162, 0x02},
    {0x4178, 0xB0},
    {0x4179, 0x5A},
    {0x0226, 0x12},
    {0x0227, 0x1F},
    {0x0225, 0x11},
    {0x0242, 0x11},
    {0x0247, 0x26},
    {0x0248, 0x61},
    {0x0249, 0x11},
    {0x4212, 0xC0},
    {0x0233, 0x46},
    {0x0654, 0x01},
    {0x0655, 0xF5},
    {0x0657, 0x19},
    {0x0659, 0x90},
    {0x065B, 0x2B},
    {0x06A3, 0x40},
    {0x06AC, 0x08},
    {0x0906, 0x08},
    {0x0908, 0x1C},
    {0x090F, 0x08},
    {0x0911, 0x1C},
    {0x0919, 0x03},
    {0x091A, 0x03},
    {0x0B0C, 0x00},
    {0x1415, 0x05},
    {0x1417, 0x03},
    {0x1418, 0x03},
    {0x145B, 0x10},
    {0x14B0, 0x01},
    {0x140F, 0x01},
    {ps5262_FAST_AE, 0x00},
    {0x0111, 0x01},
    {0x010F, 0x01},
#endif
	{ps5262_REG_END, 0x00},	/* END MARKER */
};

static struct tx_isp_sensor_win_setting ps5262_win_sizes[] = {
	/* resolution 1920*1080 @30fps max*/
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ps5262_init_regs_1920_1080_30fps_mipi,
	},
	{
		.width		= 1928,
		.height		= 1088,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ps5262_init_regs_1928_1088_30fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &ps5262_win_sizes[0];

static struct regval_list ps5262_stream_on_mipi[] = {
	//{0x0100, 0x01},
	{ps5262_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ps5262_stream_off_mipi[] = {
	//{0x0100, 0x00},
	{ps5262_REG_END, 0x00},	/* END MARKER */
};

int ps5262_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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

int ps5262_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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

static int ps5262_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != ps5262_REG_END) {
		if (vals->reg_num == ps5262_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = ps5262_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
static int ps5262_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != ps5262_REG_END) {
        if(vals->reg_num == ps5262_FAST_AE) {
	        ret = ps5262_write(sd, 0x0118, expt0);
	        ret += ps5262_write(sd, 0x0119, expt1);
	        ret += ps5262_write(sd, 0x012b, again0);
        }
		if (vals->reg_num == ps5262_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = ps5262_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int ps5262_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int ps5262_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;
    int i = 0;
	unsigned char flag,flag2;
	
    //ret = ps5262_write(sd,0x410c,0x01);
	//private_msleep(10);
    //ret = ps5262_write(sd,0x410c,0x00);
	//private_msleep(70);
	printk("ps5262_detect*****************\n");
    while(!i)
    {
	    ret = ps5262_read(sd, 0x0f37, &flag2);
        i = flag2 & ( 0x1 << 6 );
	    ret = ps5262_read(sd, 0x0f38, &again0);
	    ret = ps5262_read(sd, 0x0f3c, &expt0);
	    ret = ps5262_read(sd, 0x0f3d, &expt1);
	    ret = ps5262_read(sd, 0x010f, &flag);
        printk("--->[%d]expt=0x%02x%02x,again=0x%x,%d,flag = 0x%x,flag2 = 0x%x\n",i,expt0,expt1,again0,again0,flag,flag2);
    }

	ret = ps5262_read(sd, 0x4100, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != PS5262_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = ps5262_read(sd, 0x4101, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != PS5262_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int ps5262_set_expo(struct tx_isp_subdev *sd, int value)
{
    return 0;
	int ret = 0;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;
    printk("--->it:0x%x,%d , again:0x%x,%d\n",it,it,again,again);
	ret = ps5262_write(sd, 0x0118, (unsigned char)(it >> 8));
	ret += ps5262_write(sd, 0x0119, (unsigned char)(it & 0xff));

	ret += ps5262_write(sd, 0x012b, (unsigned char)again);
	ret += ps5262_write(sd, 0x0111,0x01);
	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int ps5262_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
	ret += ps5262_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += ps5262_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += ps5262_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));
	if (ret < 0)
		return ret;

	return 0;
}

static int ps5262_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	gain_val = value;

	ret += ps5262_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += ps5262_write(sd, 0x3e08, (unsigned char)(((value >> 8) & 0xff)));
	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int ps5262_set_logic(struct tx_isp_subdev *sd, int value)
{

    int ret = 0;
    static int i = 1;
    if(i){
        i--;
	    ret = ps5262_write(sd, 0x0118, expt0);
	    ret += ps5262_write(sd, 0x0119, expt1);

	    ret += ps5262_write(sd, 0x012b, again0);
	    ret += ps5262_write(sd, 0x0111,0x01);

        if(ret < 0)
            return ret;
    }
	return 0;
}

static int ps5262_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int ps5262_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int ps5262_init(struct tx_isp_subdev *sd, int enable)
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

#ifndef SENSOR_WITHOUT_INIT
	ret = ps5262_write_array(sd, wsize->regs);

	if (ret)
		return ret;
#endif
	if(!riscv_is_pass){
		printk("ps5262_write_array riscv_is_pass\n");
		ret = ps5262_write_array(sd, wsize->regs);
		if (ret)
			return ret;
	}
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int ps5262_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = ps5262_write_array(sd, ps5262_stream_on_mipi);
		ISP_WARNING("ps5262 stream on\n");

	} else {
		ret = ps5262_write_array(sd, ps5262_stream_off_mipi);
		ISP_WARNING("ps5262 stream off\n");
	}

	return ret;
}

static int ps5262_set_fps(struct tx_isp_subdev *sd, int fps)
{
    return 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;
	unsigned int max_fps = 0;

	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	sclk = ps5262_SUPPORT_30FPS_SCLK;
	max_fps = TX_SENSOR_MAX_FPS_30;
	
    newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(0x%x) no in range\n", fps);
		return -1;
	}

	ret = ps5262_read(sd, 0x0114, &tmp);
	hts = (tmp & 0x1f) << 8;
	ret += ps5262_read(sd, 0x115, &tmp);
	if (0 != ret) {
		ISP_ERROR("err: ps5262 read err\n");
		return ret;
	}
	hts = hts | tmp;
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += ps5262_write(sd, 0x0117, (unsigned char)((vts-1) & 0xff));
	ret += ps5262_write(sd, 0x0116, (unsigned char)((vts-1) >> 8));
	ret += ps5262_write(sd, 0x0111, 0x01);

	if (0 != ret) {
		ISP_ERROR("err: ps5262_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 3;
	sensor->video.attr->integration_time_limit = vts - 3;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 3;
	sensor->video.attr->again = -1;                                                                                                                                             
    sensor->video.attr->again_short = -1;
    sensor->video.attr->integration_time = -1;
    sensor->video.attr->integration_time_short = -1;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int ps5262_set_mode(struct tx_isp_subdev *sd, int value)
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

static int ps5262_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
#ifndef SENSOR_WITHOUT_INIT
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"ps5262_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			//private_msleep(30);
			//private_gpio_direction_output(reset_gpio, 0);
			//private_msleep(10);
			//private_gpio_direction_output(reset_gpio, 1);
			//private_msleep(60);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"ps5262_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(150);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = ps5262_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an ps5262 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
#else
	ident = 0x5262;
#endif
	ISP_WARNING("ps5262 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "ps5262", sizeof("ps5262"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

static int ps5262_set_vflip(struct tx_isp_subdev *sd, int enable)
{

	return 0;
}

static int ps5262_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
		ret = ps5262_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
//		if(arg)
//			ret = ps5262_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
//		if(arg)
//			ret = ps5262_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = ps5262_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = ps5262_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = ps5262_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		    ret = ps5262_write_array(sd, ps5262_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		    ret = ps5262_write_array(sd, ps5262_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = ps5262_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = ps5262_set_vflip(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if(arg)
			ret = ps5262_set_logic(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int ps5262_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = ps5262_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int ps5262_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ps5262_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops ps5262_core_ops = {
	.g_chip_ident = ps5262_g_chip_ident,
	.reset = ps5262_reset,
	.init = ps5262_init,
	/*.ioctl = ps5262_ops_ioctl,*/
	.g_register = ps5262_g_register,
	.s_register = ps5262_s_register,
};

static struct tx_isp_subdev_video_ops ps5262_video_ops = {
	.s_stream = ps5262_s_stream,
};

static struct tx_isp_subdev_sensor_ops	ps5262_sensor_ops = {
	.ioctl	= ps5262_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops ps5262_ops = {
	.core = &ps5262_core_ops,
	.video = &ps5262_video_ops,
	.sensor = &ps5262_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "ps5262",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int ps5262_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_video_in *video;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));

	sensor->mclk = clk_get(NULL, "cgu_cim");
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
#ifndef SENSOR_WITHOUT_INIT
	private_clk_set_rate(sensor->mclk, 24000000);
	private_clk_enable(sensor->mclk);
#endif
	switch(sensor_max_fps){
	case TX_SENSOR_MAX_FPS_30:
		wsize = &ps5262_win_sizes[0];
		//ps5262_mipi.clk = 432;
		memcpy((void*)(&(ps5262_attr.mipi)),(void*)(&ps5262_mipi),sizeof(ps5262_mipi));
		break;
	default:
		ISP_ERROR("do not support max framerate %d in mipi mode\n",sensor_max_fps);
	}

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &ps5262_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &ps5262_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->ps5262\n");

	return 0;
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int ps5262_remove(struct i2c_client *client)
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

static const struct i2c_device_id ps5262_id[] = {
	{ "ps5262", 0 },
	{ }
};
//MODULE_DEVICE_TABLE(i2c, ps5262_id);

static struct i2c_driver ps5262_driver = {
	.driver = {
		.owner	= NULL,
		.name	= "ps5262",
	},
	.probe		= ps5262_probe,
	.remove		= ps5262_remove,
	.id_table	= ps5262_id,
};

char * get_sensor_name(void)
{
	return "ps5262";
}

int get_sensor_i2c_addr(void)
{
	return 0x48;
}

int get_sensor_width()
{
	return ps5262_win_sizes->width;
}

int get_sensor_height()
{
	return ps5262_win_sizes->height;
}
#if 0
#ifdef CONFIG_BUILT_IN_SENSOR_SETTING
uint8_t *get_sensor_setting(void)
{
	return sensor_setting;
}

int get_sensor_setting_len(void)
{
	return sensor_setting_length;
}

char *get_sensor_setting_date(void)
{
	return sensor_setting_build_date;
}

char *get_sensor_setting_md5(void)
{
	return sensor_setting_md5;
}
#endif
#endif

int init_sensor(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init ps5262 dirver.\n");
		return -1;
	}
    if(data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) {
	    fast_wdr_mode = 1;
    } else {
        fast_wdr_mode = 0;
    }

	return private_i2c_add_driver(&ps5262_driver);
}

void exit_sensor(void)
{
	private_i2c_del_driver(&ps5262_driver);
}

//module_init(init_ps5262);
//module_exit(exit_ps5262);

//MODULE_DESCRIPTION("A low-level driver for SmartSens ps5262 sensors");
//MODULE_LICENSE("GPL");
