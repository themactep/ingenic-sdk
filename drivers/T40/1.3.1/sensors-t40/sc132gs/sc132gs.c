/*
 * sc132gs.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          1080*1280       25        mipi_2lane            linear
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

#define SC132GS_CHIP_ID_H	(0xeb)
#define SC132GS_CHIP_ID_L	(0x2c)
#define SC132GS_REG_END		0xffff
#define SC132GS_REG_DELAY	0xfffe
#define SC132GS_SUPPORT_30FPS_SCLK (72000000)
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20201223a"

static int reset_gpio = GPIO_PC(28);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

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

struct again_lut sc132gs_again_lut[] = {
	{0x0320, 0},
	{0x0321, 2886},
	{0x0322, 5776},
	{0x0323, 8494},
	{0x0324, 11136},
	{0x0325, 13706},
	{0x0326, 16287},
	{0x0327, 18723},
	{0x0328, 21097},
	{0x0329, 23414},
	{0x032a, 25746},
	{0x032b, 27953},
	{0x032c, 30109},
	{0x032d, 32217},
	{0x032e, 34345},
	{0x032f, 36361},
	{0x0330, 38336},
	{0x0331, 40270},
	{0x0332, 42226},
	{0x0333, 44082},
	{0x0334, 45904},
	{0x0335, 47690},
	{0x0336, 49500},
	{0x0337, 51220},
	{0x0338, 52910},
	{0x0339, 54571},
	{0x2320, 56254},
	{0x2321, 59130},
	{0x2322, 61971},
	{0x2323, 64681},
	{0x2324, 67361},
	{0x2325, 69968},
	{0x2326, 72461},
	{0x2327, 74933},
	{0x2328, 77342},
	{0x2329, 79650},
	{0x232a, 81943},
	{0x232b, 84181},
	{0x232c, 86330},
	{0x232d, 88469},
	{0x232e, 90523},
	{0x232f, 92570},
	{0x2330, 94573},
	{0x2331, 96500},
	{0x2332, 98423},
	{0x2333, 100307},
	{0x2334, 102122},
	{0x2335, 103935},
	{0x2336, 105713},
	{0x2337, 107428},
	{0x2338, 109143},
	{0x2339, 110827},
	{0x233a, 112452},
	{0x233b, 114079},
	{0x233c, 115650},
	{0x233d, 117223},
	{0x233e, 118770},
	{0x233f, 120266},
	{0x2720, 121764},
	{0x2721, 124666},
	{0x2722, 127507},
	{0x2723, 130241},
	{0x2724, 132897},
	{0x2725, 135482},
	{0x2726, 138019},
	{0x2727, 140469},
	{0x2728, 142857},
	{0x2729, 145206},
	{0x272a, 147479},
	{0x272b, 149698},
	{0x272c, 151866},
	{0x272d, 154005},
	{0x272e, 156077},
	{0x272f, 158106},
	{0x2730, 160109},
	{0x2731, 162054},
	{0x2732, 163959},
	{0x2733, 165827},
	{0x2734, 167674},
	{0x2735, 169471},
	{0x2736, 171234},
	{0x2737, 172964},
	{0x2738, 174679},
	{0x2739, 176348},
	{0x273a, 177988},
	{0x273b, 179615},
	{0x273c, 181200},
	{0x273d, 182759},
	{0x273e, 184292},
	{0x273f, 185815},
	{0x2f20, 187300},
	{0x2f21, 190215},
	{0x2f22, 193031},
	{0x2f23, 195777},
	{0x2f24, 198433},
	{0x2f25, 201029},
	{0x2f26, 203544},
	{0x2f27, 206005},
	{0x2f28, 208403},
	{0x2f29, 210732},
	{0x2f2a, 213015},
	{0x2f2b, 215234},
	{0x2f2c, 217412},
	{0x2f2d, 219531},
	{0x2f2e, 221613},
	{0x2f2f, 223642},
	{0x2f30, 225636},
	{0x2f31, 227590},
	{0x2f32, 229495},
	{0x2f33, 231371},
	{0x2f34, 233202},
	{0x2f35, 235007},
	{0x2f36, 236770},
	{0x2f37, 238508},
	{0x2f38, 240215},
	{0x2f39, 241884},
	{0x2f3a, 243531},
	{0x2f3b, 245144},
	{0x2f3c, 246736},
	{0x2f3d, 248295},
	{0x2f3e, 249835},
	{0x2f3f, 251344},
	{0x3f20, 252836},
	{0x3f21, 255745},
	{0x3f22, 258567},
	{0x3f23, 261307},
	{0x3f24, 263975},
	{0x3f25, 266565},
	{0x3f26, 269086},
	{0x3f27, 271541},
	{0x3f28, 273934},
	{0x3f29, 276268},
	{0x3f2a, 278546},
	{0x3f2b, 280770},
	{0x3f2c, 282948},
	{0x3f2d, 285072},
	{0x3f2e, 287149},
	{0x3f2f, 289182},
	{0x3f31, 293121},
	{0x3f32, 295031},
	{0x3f33, 296903},
	{0x3f34, 298742},
	{0x3f35, 300543},
	{0x3f36, 302309},
	{0x3f37, 304044},
	{0x3f38, 305747},
	{0x3f39, 307420},
	{0x3f3a, 309064},
	{0x3f3b, 310680},
	{0x3f3c, 312272},
	{0x3f3d, 313834},
	{0x3f3e, 315371},
	{0x3f3f, 316884},
};

struct tx_isp_sensor_attribute sc132gs_attr;

unsigned int sc132gs_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc132gs_again_lut;
	while(lut->gain <= sc132gs_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == sc132gs_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc132gs_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sc132gs_attr={
	.name = "sc132gs",
	.chip_id = 0x0132,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 720,
		.lans = 1,
		.settle_time_apative_en = 1,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 1080,
		.image_theight = 1280,
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
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 316884,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1500-8,
	.integration_time_limit = 1500-8,
	.total_width = 1920,
	.total_height = 1500,
	.max_integration_time = 1500-8,
	.one_line_expr_in_us = 27,
	.integration_time_apply_delay = 2,
	.again = 0,
	.integration_time = 0x4de,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc132gs_alloc_again,
	.sensor_ctrl.alloc_dgain = sc132gs_alloc_dgain,
};

static struct regval_list sc132gs_init_regs_1920_1080_25fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x36ea,0x0f},
	{0x36eb,0x25},
	{0x36ed,0x04},
	{0x36e9,0x01},
	{0x301f,0x01},
	{0x320e,0x05},
	{0x320f,0xdc},
	{0x3248,0x02},
	{0x3253,0x0a},
	{0x3301,0xff},
	{0x3302,0xff},
	{0x3303,0x10},
	{0x3306,0x28},
	{0x3307,0x02},
	{0x330a,0x00},
	{0x330b,0xb0},
	{0x3318,0x02},
	{0x3320,0x06},
	{0x3321,0x02},
	{0x3326,0x12},
	{0x3327,0x0e},
	{0x3328,0x03},
	{0x3329,0x0f},
	{0x3364,0x0f},
	{0x33b3,0x40},
	{0x33f9,0x2c},
	{0x33fb,0x38},
	{0x33fc,0x0f},
	{0x33fd,0x1f},
	{0x349f,0x03},
	{0x34a6,0x01},
	{0x34a7,0x1f},
	{0x34a8,0x40},
	{0x34a9,0x30},
	{0x34ab,0xa6},
	{0x34ad,0xa6},
	{0x3622,0x60},
	{0x3625,0x08},
	{0x3630,0xa8},
	{0x3631,0x84},
	{0x3632,0x90},
	{0x3633,0x43},
	{0x3634,0x09},
	{0x3635,0x82},
	{0x3636,0x48},
	{0x3637,0xe4},
	{0x3641,0x22},
	{0x3670,0x0e},
	{0x3674,0xc0},
	{0x3675,0xc0},
	{0x3676,0xc0},
	{0x3677,0x86},
	{0x3678,0x88},
	{0x3679,0x8c},
	{0x367c,0x01},
	{0x367d,0x0f},
	{0x367e,0x01},
	{0x367f,0x0f},
	{0x3690,0x43},
	{0x3691,0x43},
	{0x3692,0x53},
	{0x369c,0x01},
	{0x369d,0x1f},
	{0x3900,0x0d},
	{0x3904,0x06},
	{0x3905,0x98},
	{0x391b,0x81},
	{0x391c,0x10},
	{0x391d,0x19},
	{0x3949,0xc8},
	{0x394b,0x64},
	{0x3952,0x02},
	{0x3e00,0x00},
	{0x3e01,0x4d},
	{0x3e02,0xe0},
	{0x4502,0x34},
	{0x4509,0x30},
	{0x0100,0x01},

	{SC132GS_REG_END, 0x00},	/* END MARKER */
};

static struct tx_isp_sensor_win_setting sc132gs_win_sizes[] = {
	{
		.width		= 1080,
		.height		= 1280,
		.fps		= 25 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= sc132gs_init_regs_1920_1080_25fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &sc132gs_win_sizes[0];

static struct regval_list sc132gs_stream_on_mipi[] = {
	{0x0100, 0x01},
	{SC132GS_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc132gs_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SC132GS_REG_END, 0x00},	/* END MARKER */
};

int sc132gs_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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

int sc132gs_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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

#if 0
static int sc132gs_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC132GS_REG_END) {
		if (vals->reg_num == SC132GS_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc132gs_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc132gs_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC132GS_REG_END) {
		if (vals->reg_num == SC132GS_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc132gs_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc132gs_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int sc132gs_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sc132gs_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC132GS_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc132gs_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC132GS_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc132gs_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = -1;
	int it = (value & 0xffff) ;
	int again = (value & 0xffff0000) >> 16;

	ret  = sc132gs_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc132gs_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc132gs_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
	ret += sc132gs_write(sd, 0x3e07, (unsigned char)(again & 0xff));
	ret += sc132gs_write(sd, 0x3e09, (unsigned char)(((again >> 8) & 0xff)));

	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int sc132gs_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	ret += sc132gs_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc132gs_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc132gs_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));

	if (ret < 0)
		return ret;

	return 0;
}

static int sc132gs_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sc132gs_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sc132gs_write(sd, 0x3e08, (unsigned char)(((value >> 8) & 0xff)));
	if (ret < 0)
		return ret;
	gain_val = value;

	return 0;
}
#endif

static int sc132gs_set_logic(struct tx_isp_subdev *sd, int value)
{
	return 0;
}
static int sc132gs_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc132gs_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc132gs_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!init->enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
    sensor->video.state = TX_ISP_MODULE_DEINIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc132gs_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	if (init->enable) {
	    if(sensor->video.state == TX_ISP_MODULE_DEINIT){
            ret = sc132gs_write_array(sd, wsize->regs);
            if (ret)
                return ret;
            sensor->video.state = TX_ISP_MODULE_INIT;
	    }
	    if(sensor->video.state == TX_ISP_MODULE_INIT){
            if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
                ret = sc132gs_write_array(sd, sc132gs_stream_on_mipi);
            } else {
                ISP_ERROR("Don't support this Sensor Data interface\n");
            }
            sensor->video.state = TX_ISP_MODULE_RUNNING;
            ISP_WARNING("sc132gs stream on\n");
	    }
	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc132gs_write_array(sd, sc132gs_stream_off_mipi);
		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
        sensor->video.state = TX_ISP_MODULE_INIT;
		ISP_WARNING("sc132gs stream off\n");
	}

	return ret;
}

static int sc132gs_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;

	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	sclk = SC132GS_SUPPORT_30FPS_SCLK;

	ret = sc132gs_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sc132gs_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("err: sc132gs read err\n");
		return ret;
	}
	hts = ((hts << 8) + tmp) ;//<< 1;
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += sc132gs_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc132gs_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: sc132gs_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 8;
	sensor->video.attr->integration_time_limit = vts - 8;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 8;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sc132gs_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
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

static int sensor_attr_check(struct tx_isp_subdev *sd)
{
    unsigned long rate;
    struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
    struct tx_isp_sensor_register_info *info = &sensor->info;

    if(info->default_boot !=0)
        ISP_ERROR("Have no this MCLK Source!!!\n");

    switch (info->video_interface) {
        case TISP_SENSOR_VI_MIPI_CSI0:
            sc132gs_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
            sc132gs_attr.mipi.index = 0;
            break;
        case TISP_SENSOR_VI_MIPI_CSI1:
            sc132gs_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
            sc132gs_attr.mipi.index = 1;
            break;
        case TISP_SENSOR_VI_DVP:
            sc132gs_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
            break;
        default:
            ISP_ERROR("Have no this MCLK Source!!!\n");
    }

    switch (info->mclk) {
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
        return -1;
    }
    private_clk_set_rate(sensor->mclk, 24000000);
    private_clk_prepare_enable(sensor->mclk);

    reset_gpio = info->rst_gpio;
    pwdn_gpio = info->pwdn_gpio;

    return 0;
}

static int sc132gs_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc132gs_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"sc132gs_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc132gs_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc132gs chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("sc132gs chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc132gs", sizeof("sc132gs"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc132gs_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	val = sc132gs_read(sd, 0x3221, &val);
	switch(enable) {
	case 0:
		val &= 0x99;
		break;
	case 1:
		val = ((val & 0x9F) | 0x06);
		break;
	case 2:
		val = ((val & 0xF9) | 0x60);
		break;
	case 3:
		val |= 0x66;
		break;
	}
	sc132gs_write(sd, 0x3221, val);
	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int sc132gs_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
    struct tx_isp_sensor_value *sensor_val = arg;


	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
	     	ret = sc132gs_set_expo(sd, sensor_val->value);
		break;
//	case TX_ISP_EVENT_SENSOR_INT_TIME:
//		if(arg)
//			ret = sc132gs_set_integration_time(sd, sensor_val->value);
//		break;
//	case TX_ISP_EVENT_SENSOR_AGAIN:
//		if(arg)
//			ret = sc132gs_set_analog_gain(sd, sensor_val->value);
//		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc132gs_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc132gs_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc132gs_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc132gs_write_array(sd, sc132gs_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc132gs_write_array(sd, sc132gs_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc132gs_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = sc132gs_set_vflip(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if(arg)
			ret = sc132gs_set_logic(sd, sensor_val->value);
	default:
		break;
	}

	return ret;
}

static int sc132gs_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc132gs_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc132gs_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	sc132gs_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc132gs_core_ops = {
	.g_chip_ident = sc132gs_g_chip_ident,
	.reset = sc132gs_reset,
	.init = sc132gs_init,
	/*.ioctl = sc132gs_ops_ioctl,*/
	.g_register = sc132gs_g_register,
	.s_register = sc132gs_s_register,
};

static struct tx_isp_subdev_video_ops sc132gs_video_ops = {
	.s_stream = sc132gs_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc132gs_sensor_ops = {
	.ioctl	= sc132gs_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc132gs_ops = {
	.core = &sc132gs_core_ops,
	.video = &sc132gs_video_ops,
	.sensor = &sc132gs_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc132gs",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc132gs_probe(struct i2c_client *client, const struct i2c_device_id *id)
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

	/*
	  convert sensor-gain into isp-gain,
	*/
	sd = &sensor->sd;
	video = &sensor->video;
    	sensor->dev = &client->dev;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sc132gs_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc132gs_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc132gs\n");

	return 0;
}

static int sc132gs_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

    private_clk_disable_unprepare(sensor->mclk);
    private_devm_clk_put(&client->dev, sensor->mclk);
    tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id sc132gs_id[] = {
	{ "sc132gs", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc132gs_id);

static struct i2c_driver sc132gs_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc132gs",
	},
	.probe		= sc132gs_probe,
	.remove		= sc132gs_remove,
	.id_table	= sc132gs_id,
};

static __init int init_sc132gs(void)
{
	return private_i2c_add_driver(&sc132gs_driver);
}

static __exit void exit_sc132gs(void)
{
	private_i2c_del_driver(&sc132gs_driver);
}

module_init(init_sc132gs);
module_exit(exit_sc132gs);

MODULE_DESCRIPTION("A low-level driver for SmartSens sc132gs sensors");
MODULE_LICENSE("GPL");
